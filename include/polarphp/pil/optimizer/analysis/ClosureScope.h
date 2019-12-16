//===--- ClosureScope.h - Determines closure's defining scope ---*- C++ -*-===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2017 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
///
/// Map each non-escaping closure PILFunction to a set of PILFunctions
/// corresponding to its parent scopes.
///
/// This is a lightweight analysis specific to non-escaping closures. Unlike
/// CallerAnalysis, this does not reflect the call tree. A closure's scope is a
/// function that directly references the closure. It may directly invoke the
/// closure (as a caller) or may simply pass it off as an argument.
///
/// Like CallerAnalysis, ClosureScope is top-down, but unlike CallerAnalysis, it
/// does not require complex invalidation and recomputation. The underlying
/// assumption is that no trasformation will add new references to existing
/// non-escaping closures, with some exceptions like PILCloner.
///
/// TODO: When this analysis is used across passes, fix PILCloner to update or
/// invalidate. In PILVerifier, if this analysis is marked valid, check that no
/// new unseen closure references have been added.
///
/// We do not currently mark PILFunctions as non-escaping. However, only
/// closures that are not assigned to an lvalue and are never passed as escaping
/// closures can use the @inout_aliasable convention. For now, we simply limit
/// this analysis to such closures. This covers the set of closures that
/// PILOptimizer must view as non-escaping, which must in turn be a subset of
/// Sema's non-escaping closures.
///
/// NOTE: a non-escaping function can be passed as an escaping function via
/// withoutActuallyEscaping. However, using that API to introduce recursion is
/// disallowed according to exclusivity semantics. That is, non-escaping
/// function types cannot be reentrant (SE-0176). In this analysis, we assert
/// that closure scopes are acyclic. Although the language does not currently
/// enforce non-reentrant, non-escaping closures, the scope graph cannot be
/// cyclic because there's no way to name a non-escaping closure. So, in the
/// long term the acyclic assumption made by this analysis is protected by
/// non-reentrant semantics, and in the short-term it's safe because of the
/// lanuguage's practical limitations.
///
//===----------------------------------------------------------------------===//

#ifndef POLARPHP_PIL_OPTIMIZER_ANALYSIS_CLOSURESCOPE_H
#define POLARPHP_PIL_OPTIMIZER_ANALYSIS_CLOSURESCOPE_H

#include "polarphp/basic/BlotSetVector.h"
#include "polarphp/pil/PILFunction.h"
#include "polarphp/pil/optimizer/analysis/Analysis.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/iterator.h"

namespace polar {

/// Return true if this function is known to be non-escaping.
///
/// This must only return true if the closure was also deemed non-escaping in
/// Sema. Sema will eventually guarantee non-reentrance.
///
/// This must always return true for any closure with @inout_aliasable parameter
/// conventions, because passes like AccessEnforcementSelection assume that
/// convention only applies to non-escaping closures.
///
/// (Hence, PILGen may only use @inout_aliasable for closures that Sema deems
/// non-escaping.)
///
/// This may conservatively return false for any non-escaping closures that
/// don't happen to use @inout_aliasable.
inline bool isNonEscapingClosure(CanPILFunctionType funcTy) {
   auto isInoutAliasable = [](PILParameterInfo paramInfo) {
      return paramInfo.getConvention()
             == ParameterConvention::Indirect_InoutAliasable;
   };
   return llvm::any_of(funcTy->getParameters(), isInoutAliasable);
}

class ClosureScopeData;

class ClosureScopeAnalysis : public PILAnalysis {
   friend class ClosureScopeData;

   // Get a closure's scope function from its index. This functor is compatible
   // with OptionalTransformRange. Unfortunately it exposes the internals of the
   // analysis data.
   struct IndexLookupFunc {
      // A reference to all closure parent scopes ordered by their index.
      const std::vector<PILFunction *> &indexedScopes;

      IndexLookupFunc(const std::vector<PILFunction *> &indexedScopes)
         : indexedScopes(indexedScopes) {}

      Optional<PILFunction *> operator()(int idx) const {
         if (auto funcPtr = indexedScopes[idx]) {
            return funcPtr;
         }
         return None;
      }
   };
   using IndexRange = iterator_range<int *>;

public:
   // A range of PILFunction scopes converted from their scope indices and
   // filtered to remove any erased functions.
   using ScopeRange = OptionalTransformRange<IndexRange, IndexLookupFunc, int *>;

private:
   PILModule *M;

   // The analysis data. nullptr if it has never been computed.
   std::unique_ptr<ClosureScopeData> scopeData;

public:
   ClosureScopeAnalysis(PILModule *M);
   ~ClosureScopeAnalysis();

   static bool classof(const PILAnalysis *S) {
      return S->getKind() == PILAnalysisKind::ClosureScope;
   }

   PILModule *getModule() const { return M; }

   // Return true if the given function is the parent scope for any closures.
   bool isClosureScope(PILFunction *scopeFunc);

   // Return a range of scopes for the given closure. The elements of the
   // returned range have type `PILFunction *` and are non-null. Returns an
   // empty range for a PILFunction that is not a closure or is a dead closure.
   ScopeRange getClosureScopes(PILFunction *closureFunc);

   /// Invalidate all information in this analysis.
   virtual void invalidate() override;

   /// Invalidate all of the information for a specific function.
   virtual void invalidate(PILFunction *F, InvalidationKind K) override {
      // No invalidation needed because the analysis does not cache anything
      // per call-site in functions, and we assume that references to closures
      // cannot be added to functions, aside from cloning.
   }

   /// Notify the analysis about a newly created function.
   virtual void notifyAddedOrModifiedFunction(PILFunction *F) override {
      // Nothing to be done because the analysis does not cache anything
      // per call-site in functions.
   }

   /// Notify the analysis about a function which will be deleted from the
   /// module.
   virtual void notifyWillDeleteFunction(PILFunction *F) override;

   /// Notify the analysis about changed witness or vtables.
   virtual void invalidateFunctionTables() override {
      // witness tables have no effect on closure scopes.
   }

protected:
   ClosureScopeData *getOrComputeScopeData();
};

// ClosureScopeAnalysis utility for visiting functions top down in closure scope
// order.
class TopDownClosureFunctionOrder {
   ClosureScopeAnalysis *CSA;

   llvm::SmallSet<PILFunction *, 16> visited;

   BlotSetVector<PILFunction *> closureWorklist;

public:
   TopDownClosureFunctionOrder(ClosureScopeAnalysis *CSA) : CSA(CSA) {}

   // Visit all functions in a module, visiting each closure scope function
   // before
   // the closure function itself.
   void visitFunctions(llvm::function_ref<void(PILFunction *)> visitor);
};

} // end namespace polar

#endif
