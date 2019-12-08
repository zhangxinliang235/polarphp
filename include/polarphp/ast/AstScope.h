//===--- AstScope.h - Swift AST Scope ---------------------------*- C++ -*-===//
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
//
// This file defines the AstScope class and related functionality, which
// describes the scopes that exist within a Polarphp AST.
//
//===----------------------------------------------------------------------===//

#ifndef POLARPHP_AST_AST_SCOPE_H
#define POLARPHP_AST_AST_SCOPE_H

#include "polarphp/ast/AstNode.h"
#include "polarphp/ast/NameLookup.h" // for DeclVisibilityKind
#include "polarphp/ast/SimpleRequest.h"
#include "polarphp/basic/Debug.h"
#include "polarphp/basic/LLVM.h"
#include "polarphp/basic/NullablePtr.h"
#include "polarphp/basic/SourceMgr.h"
#include "llvm/ADT/PointerIntPair.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"


/// In case there's a bug in the ASTScope lookup system, suggest that the user
/// try disabling it.
/// \p message must be a string literal
#define AST_SCOPE_ASSERT(predicate, message)                                     \
  assert((predicate) && message                                                \
         " Try compiling with '-disable-astscope-lookup'.")

#define AST_SCOPE_UNREACHABLE(message)                                          \
  llvm_unreachable(message " Try compiling with '-disable-astscope-lookup'.")

namespace polar::basic {
class Syntax;
}

namespace polar::ast {

using polar::basic::SourceRange;
using polar::basic::SourceLoc;
using polar::basic::Syntax;
using polar::basic::NullablePtr;

class AbstractStorageDecl;
class AstContext;
class BraceStmt;
class CaseStmt;
class CatchStmt;
class ClosureExpr;
class Decl;
class DoCatchStmt;
class Expr;
class ForEachStmt;
class GenericParamList;
class GuardStmt;
class IfStmt;
class IterableDeclContext;
class LabeledConditionalStmt;
class ParamDecl;
class PatternBindingDecl;
class RepeatWhileStmt;
class SourceFile;
class Stmt;
class StmtConditionElement;
class SwitchStmt;
class TopLevelCodeDecl;
class TypeDecl;
class WhileStmt;

#pragma mark Forward-references

#define DECL(Id, Parent) class Id##Decl;
#define ABSTRACT_DECL(Id, Parent) class Id##Decl;
#include "polarphp/ast/DeclNodesDef.h"
#undef DECL
#undef ABSTRACT_DECL

#define EXPR(Id, Parent) class Id##Expr;
#include "polarphp/ast/ExprNodesDef.h"
#undef EXPR

#define STMT(Id, Parent) class Id##Stmt;
#define ABSTRACT_STMT(Id, Parent) class Id##Stmt;
#include "polarphp/ast/StmtNodesDef.h"
#undef STMT
#undef ABSTRACT_STMT

class GenericParamList;
class TrailingWhereClause;
class ParameterList;
class PatternBindingEntry;
class SpecializeAttr;
class GenericContext;
class DeclName;
class StmtConditionElement;

class AstScopeImpl;
class GenericTypeOrExtensionScope;
class IterableTypeScope;
class TypeAliasScope;
class ScopeCreator;

struct AnnotatedInsertionPoint {
  AstScopeImpl *insertionPoint;
  const char *explanation;
};

void simple_display(llvm::raw_ostream &out, const AstScopeImpl *);
void simple_display(llvm::raw_ostream &out, const ScopeCreator *);

SourceLoc extractNearestSourceLoc(std::tuple<AstScopeImpl *, ScopeCreator *>);

#pragma mark the root AstScopeImpl class

/// Describes a lexical scope within a source file.
///
/// Each \c AstScopeImpl is a node within a tree that describes all of the
/// lexical scopes within a particular source range. The root of this scope tree
/// is always a \c SourceFile node, and the tree covers the entire source file.
/// The children of a particular node are the lexical scopes immediately
/// nested within that node, and have source ranges that are enclosed within
/// the source range of their parent node. At the leaves are lexical scopes
/// that cannot be subdivided further.
///
/// The tree provides source-location-based query operations, allowing one to
/// find the innermost scope that contains a given source location. Navigation
/// to parent nodes from that scope allows one to walk the lexically enclosing
/// scopes outward to the source file. Given a scope, one can also query the
/// associated \c DeclContext for additional contextual information.
///
/// \code
/// -dump-scope-maps expanded
/// \endcode
class AstScopeImpl {
  friend class NodeAdder;
  friend class Portion;
  friend class GenericTypeOrExtensionWholePortion;
  friend class NomExtDeclPortion;
  friend class GenericTypeOrExtensionWherePortion;
  friend class GenericTypeOrExtensionWherePortion;
  friend class IterableTypeBodyPortion;
  friend class ScopeCreator;

#pragma mark - tree state
protected:
  using Children = SmallVector<AstScopeImpl *, 4>;
  /// Whether the given parent is the accessor node for an abstract
  /// storage declaration or is directly descended from it.

private:
  /// Always set by the constructor, so that when creating a child
  /// the parent chain is available.
  AstScopeImpl *parent = nullptr; // null at the root

  /// Child scopes, sorted by source range.
  /// Must clear source range change whenever this changes
  Children storedChildren;

  bool wasExpanded = false;

  /// For use-before-def, ASTAncestor scopes may be added to a BraceStmt.
  unsigned astAncestorScopeCount = 0;

  /// Can clear storedChildren, so must remember this
  bool haveAddedCleanup = false;

  // Must be updated after last child is added and after last child's source
  // position is known
  mutable Optional<SourceRange> cachedSourceRange;

  // When ignoring ASTNodes in a scope, they still must count towards a scope's
  // source range. So include their ranges here
  SourceRange sourceRangeOfIgnoredASTNodes;

#pragma mark - constructor / destructor
public:
  AstScopeImpl(){}
  // TOD: clean up all destructors and deleters
  virtual ~AstScopeImpl() {}

  AstScopeImpl(AstScopeImpl &&) = delete;
  AstScopeImpl &operator=(AstScopeImpl &&) = delete;
  AstScopeImpl(const AstScopeImpl &) = delete;
  AstScopeImpl &operator=(const AstScopeImpl &) = delete;

  // Make vanilla new illegal for ASTScopes.
  void *operator new(size_t bytes) = delete;
  // Need this because have virtual destructors
  void operator delete(void *data) {}

  // Only allow allocation of scopes using the allocator of a particular source
  // file.
  void *operator new(size_t bytes, const AstContext &ctx,
                     unsigned alignment = alignof(AstScopeImpl));
  void *operator new(size_t Bytes, void *Mem) {
    AST_SCOPE_ASSERT(Mem, "Allocation failed");
    return Mem;
  }

#pragma mark - tree declarations
protected:
  NullablePtr<AstScopeImpl> getParent() { return parent; }
  NullablePtr<const AstScopeImpl> getParent() const { return parent; }

  const Children &getChildren() const { return storedChildren; }

  /// Get ride of descendants and remove them from scopedNodes so the scopes
  /// can be recreated. Needed because typechecking inserts a return statment
  /// into intiailizers.
  void disownDescendants(ScopeCreator &);

public: // for addReusedBodyScopes
  void addChild(AstScopeImpl *child, AstContext &);
  std::vector<AstScopeImpl *> rescueASTAncestorScopesForReuseFromMe();

  /// When reexpanding, do we always create a new body?
  virtual NullablePtr<AstScopeImpl> getParentOfASTAncestorScopesToBeRescued();
  std::vector<AstScopeImpl *>
  rescueASTAncestorScopesForReuseFromMeOrDescendants();
  void replaceASTAncestorScopes(ArrayRef<AstScopeImpl *>);

private:
  void removeChildren();

private:
  void emancipate() { parent = nullptr; }
  NullablePtr<AstScopeImpl> getPriorSibling() const;

public:
  void preOrderDo(function_ref<void(AstScopeImpl *)>);
  /// Like preorderDo but without myself.
  void preOrderChildrenDo(function_ref<void(AstScopeImpl *)>);
  void postOrderDo(function_ref<void(AstScopeImpl *)>);

#pragma mark - source ranges

#pragma mark - source range queries

public:
  /// Return signum of ranges. Centralize the invariant that ASTScopes use ends.
  static int compare(SourceRange, SourceRange, const SourceManager &,
                     bool ensureDisjoint);

  SourceRange getSourceRangeOfScope(bool omitAssertions = false) const;

  /// InterpolatedStringLiteralExprs and EditorPlaceHolders respond to
  /// getSourceRange with the starting point. But we might be asked to lookup an
  /// identifer within one of them. So, find the real source range of them here.
  SourceRange getEffectiveSourceRange(AstNode) const;

  void computeAndCacheSourceRangeOfScope(bool omitAssertions = false) const;
  bool isSourceRangeCached(bool omitAssertions = false) const;

  bool checkSourceRangeOfThisASTNode() const;

  /// For debugging
  bool doesRangeMatch(unsigned start, unsigned end, StringRef file = "",
                      StringRef className = "");

  unsigned countDescendants() const;

  /// Make sure that when the argument is executed, there are as many
  /// descendants after as before.
  void assertThatTreeDoesNotShrink(function_ref<void()>);

private:
  SourceRange computeSourceRangeOfScope(bool omitAssertions = false) const;
  SourceRange
  computeSourceRangeOfScopeWithChildASTNodes(bool omitAssertions = false) const;
  bool ensureNoAncestorsSourceRangeIsCached() const;

#pragma mark - source range adjustments
private:
  SourceRange widenSourceRangeForIgnoredAstNodes(SourceRange range) const;

  /// If the scope refers to a Decl whose source range tells the whole story,
  /// for example a NominalTypeScope, it is not necessary to widen the source
  /// range by examining the children. In that case we could just return
  /// the childlessRange here.
  /// But, we have not marked such scopes yet. Doing so would be an
  /// optimization.
  SourceRange widenSourceRangeForChildren(SourceRange range,
                                          bool omitAssertions) const;

  /// Even ASTNodes that do not form scopes must be included in a Scope's source
  /// range. Widen the source range of the receiver to include the (ignored)
  /// node.
  void widenSourceRangeForIgnoredAstNode(AstNode);

private:
  void clearCachedSourceRangesOfMeAndAncestors();

public:
  /// Since source ranges are cached but depend on child ranges,
  /// when descendants are added, my and my ancestor ranges must be
  /// recalculated.
  void ensureSourceRangesAreCorrectWhenAddingDescendants(function_ref<void()>);

public: // public for debugging
  /// Returns source range of this node alone, without factoring in any
  /// children.
  virtual SourceRange
  getSourceRangeOfThisAstNode(bool omitAssertions = false) const = 0;

protected:
  SourceManager &getSourceManager() const;
  bool hasValidSourceRange() const;
  bool hasValidSourceRangeOfIgnoredASTNodes() const;
  bool precedesInSource(const AstScopeImpl *) const;
  bool verifyThatChildrenAreContainedWithin(SourceRange) const;
  bool verifyThatThisNodeComeAfterItsPriorSibling() const;

  virtual SourceRange
  getSourceRangeOfEnclosedParamsOfASTNode(bool omitAssertions) const;

private:
  bool checkSourceRangeAfterExpansion(const AstContext &) const;

#pragma mark common queries
public:
  virtual NullablePtr<ClosureExpr> getClosureIfClosureScope() const;
  virtual AstContext &getAstContext() const;
  virtual NullablePtr<DeclContext> getDeclContext() const;
  virtual NullablePtr<Decl> getDeclIfAny() const { return nullptr; }
  virtual NullablePtr<Stmt> getStmtIfAny() const { return nullptr; }
  virtual NullablePtr<Expr> getExprIfAny() const { return nullptr; }
  virtual NullablePtr<DeclAttribute> getDeclAttributeIfAny() const {
    return nullptr;
  }
  virtual NullablePtr<const void> getReferrent() const { return nullptr; }

#pragma mark - debugging and printing

public:
  virtual const SourceFile *getSourceFile() const;
  virtual std::string getClassName() const = 0;

  /// Print out this scope for debugging/reporting purposes.
  void print(llvm::raw_ostream &out, unsigned level = 0, bool lastChild = false,
             bool printChildren = true) const;

  void printRange(llvm::raw_ostream &out) const;

protected:
  virtual void printSpecifics(llvm::raw_ostream &out) const {}
  virtual NullablePtr<const void> addressForPrinting() const;

public:
  POLAR_DEBUG_DUMP;

  void dumpOneScopeMapLocation(std::pair<unsigned, unsigned> lineColumn);

private:
  llvm::raw_ostream &verificationError() const;

#pragma mark - Scope tree creation
public:
  /// expandScope me, sending deferred nodes to my descendants.
  /// Return the scope into which to place subsequent decls
  AstScopeImpl *expandAndBeCurrentDetectingRecursion(ScopeCreator &);

  /// Expand or reexpand the scope if unexpanded or if not current.
  /// There are several places in the compiler that mutate the AST after the
  /// fact, above and beyond adding Decls to the SourceFile.
  AstScopeImpl *expandAndBeCurrent(ScopeCreator &);

  unsigned getAstAncestorScopeCount() const { return astAncestorScopeCount; }
  bool getWasExpanded() const { return wasExpanded; }

protected:
  void resetASTAncestorScopeCount() { astAncestorScopeCount = 0; }
  void increaseASTAncestorScopeCount(unsigned c) { astAncestorScopeCount += c; }
  void setWasExpanded() { wasExpanded = true; }
  virtual AstScopeImpl *expandSpecifically(ScopeCreator &) = 0;
  virtual void beCurrent();
  virtual bool doesExpansionOnlyAddNewDeclsAtEnd() const;

public:
  bool isExpansionNeeded(const ScopeCreator &) const;

protected:
  bool isCurrent() const;
  virtual bool isCurrentIfWasExpanded() const;

private:
  /// Compare the pre-expasion range with the post-expansion range and return
  /// false if lazyiness couild miss lookups.
  bool checkLazySourceRange(const AstContext &) const;

public:
  /// Some scopes can be expanded lazily.
  /// Such scopes must: not change their source ranges after expansion, and
  /// their expansion must return an insertion point outside themselves.
  /// After a node is expanded, its source range (getSourceRangeofThisASTNode
  /// union children's ranges) must be same as this.
  virtual NullablePtr<AstScopeImpl> insertionPointForDeferredExpansion();
  virtual SourceRange sourceRangeForDeferredExpansion() const;

public:
  // Some nodes (VarDecls and Accessors) are created directly from
  // pattern scope code and should neither be deferred nor should
  // contribute to widenSourceRangeForIgnoredAstNode.
  // Closures and captures are also created directly but are
  // screened out because they are expressions.
  static bool isHandledSpeciallyByPatterns(const AstNode n);

  virtual NullablePtr<AbstractStorageDecl>
  getEnclosingAbstractStorageDecl() const;

  bool isATypeDeclScope() const;

private:
  virtual ScopeCreator &getScopeCreator();

#pragma mark - - creation queries
public:
  virtual bool isThisAnAbstractStorageDecl() const { return false; }

#pragma mark - lookup

public:
  using DeclConsumer = namelookup::AbstractAstScopeDeclConsumer &;

  /// Entry point into AstScopeImpl-land for lookups
  static llvm::SmallVector<const AstScopeImpl *, 0>
  unqualifiedLookup(SourceFile *, DeclName, SourceLoc,
                    const DeclContext *startingContext, DeclConsumer);

  static Optional<bool>
  computeIsCascadingUse(ArrayRef<const AstScopeImpl *> history,
                        Optional<bool> initialIsCascadingUse);

#pragma mark - - lookup- starting point
private:
  static const AstScopeImpl *findStartingScopeForLookup(SourceFile *,
                                                        const DeclName name,
                                                        const SourceLoc where,
                                                        const DeclContext *ctx);

protected:
  virtual bool doesContextMatchStartingContext(const DeclContext *) const;

protected:
  /// Not const because may reexpand some scopes.
  AstScopeImpl *findInnermostEnclosingScope(SourceLoc,
                                            NullablePtr<raw_ostream>);
  AstScopeImpl *findInnermostEnclosingScopeImpl(SourceLoc,
                                                NullablePtr<raw_ostream>,
                                                SourceManager &,
                                                ScopeCreator &);

private:
  NullablePtr<AstScopeImpl> findChildContaining(SourceLoc loc,
                                                SourceManager &sourceMgr) const;

#pragma mark - - lookup- per scope
protected:
  /// The main (recursive) lookup function:
  /// Tell DeclConsumer about all names found in this scope and if not done,
  /// recurse for enclosing scopes. Stop lookup if about to look in limit.
  /// Return final value for isCascadingUse
  ///
  /// If the lookup depends on implicit self, selfDC is its context.
  /// (Names in extensions never depend on self.)
  ///
  /// In a Nominal, Extension, or TypeAliasScope, the lookup can start at either
  /// the body portion (for the first two), the where portion, or a
  /// GenericParamScope. In every case, the generics on the type decl must be
  /// searched, but only once. And they must be searched *before* the generic
  /// parameters. For instance, the following is correct: \code class
  /// ShadowingGenericParameter<T> { \code   typealias T = Int;  func foo (t :
  /// T) {} \code } \code ShadowingGenericParameter<String>().foo(t: "hi")
  ///
  /// So keep track of the last generic param list searched to avoid
  /// duplicating work.
  ///
  /// Look in this scope.
  /// \param history are the scopes traversed for this lookup (including this
  /// one) \param limit A scope into which lookup should not go. See \c
  /// getLookupLimit. \param lastListSearched Last list searched.
  /// \param consumer is the object to which found decls are reported.
  void lookup(llvm::SmallVectorImpl<const AstScopeImpl *> &history,
              NullablePtr<const AstScopeImpl> limit,
              NullablePtr<const GenericParamList> lastListSearched,
              DeclConsumer consumer) const;

public:
  /// Returns the SelfDC for parent (and possibly ancestor) scopes.
  /// A return of None indicates that the previous child (in history) should be
  /// asked.
  virtual Optional<NullablePtr<DeclContext>> computeSelfDCForParent() const;

protected:
  /// Find either locals or members (no scope has both)
  /// \param history The scopes visited since the start of lookup (including
  /// this one)
  /// \return True if lookup is done
  virtual bool lookupLocalsOrMembers(ArrayRef<const AstScopeImpl *> history,
                                     DeclConsumer consumer) const;

  /// Returns isDone and the list searched, if any
  std::pair<bool, NullablePtr<const GenericParamList>>
  lookInMyGenericParameters(
      NullablePtr<const GenericParamList> priorListSearched,
      DeclConsumer consumer) const;

  virtual NullablePtr<const GenericParamList> genericParams() const;

  // Consume the generic parameters in the context and its outer contexts
  static bool lookInGenericParametersOf(NullablePtr<const GenericParamList>,
                                        DeclConsumer);

  NullablePtr<const AstScopeImpl> parentIfNotChildOfTopScope() const {
    const auto *p = getParent().get();
    return p->getParent().isNonNull() ? p : nullptr;
  }

  /// The tree is organized by source location and for most nodes this is also
  /// what obtaines for scoping. However, guards are different. The scope after
  /// the guard else must hop into the innermoset scope of the guard condition.
  virtual NullablePtr<const AstScopeImpl> getLookupParent() const {
    return parent;
  }

#pragma mark - - lookup- local bindings
protected:
  virtual Optional<bool>
  resolveIsCascadingUseForThisScope(Optional<bool>) const;

  // A local binding is a basically a local variable defined in that very scope
  // It is not an instance variable or inherited type.

  static bool lookupLocalBindingsInPattern(Pattern *p,
                                           DeclVisibilityKind vis,
                                           DeclConsumer consumer);

  /// When lookup must stop before the outermost scope, return the scope to stop
  /// at. Example, if a protocol is nested in a struct, we must stop before
  /// looking into the struct.
  ///
  /// Ultimately, the task of rejecting results found in inapplicable outer
  /// scopes is best moved to the clients of the ASTScope lookup subsystem. It
  /// seems out of place here.
  virtual NullablePtr<const AstScopeImpl> getLookupLimit() const;

  NullablePtr<const AstScopeImpl>
  ancestorWithDeclSatisfying(function_ref<bool(const Decl *)> predicate) const;
}; // end of AstScopeImpl

#pragma mark - specific scope classes

/// The root of the scope tree.
class AstSourceFileScope final : public AstScopeImpl {
public:
  SourceFile *const SF;
  ScopeCreator *const scopeCreator;
  AstScopeImpl *insertionPoint;

  /// The number of \c Decls in the \c SourceFile that were already seen.
  /// Since parsing can be interleaved with type-checking, on every
  /// lookup, look at creating scopes for any \c Decls beyond this number.
  /// TODO: Unify with numberOfChildrenWhenLastExpanded
  size_t numberOfDeclsAlreadySeen = 0;

  AstSourceFileScope(SourceFile *SF, ScopeCreator *scopeCreator);

  std::string getClassName() const override;
  SourceRange
  getSourceRangeOfThisAstNode(bool omitAssertions = false) const override;

protected:
  void printSpecifics(llvm::raw_ostream &out) const override;

public:
  NullablePtr<DeclContext> getDeclContext() const override;

  void buildFullyExpandedTree();
  void
  buildEnoughOfTreeForTopLevelExpressionsButDontRequestGenericsOrExtendedNominals();

  void expandFunctionBody(AbstractFunctionDecl *AFD);

  const SourceFile *getSourceFile() const override;
  NullablePtr<const void> addressForPrinting() const override { return SF; }
  bool crossCheckWithAST();

protected:
  AstScopeImpl *expandSpecifically(ScopeCreator &scopeCreator) override;
  bool isCurrentIfWasExpanded() const override;
  void beCurrent() override;
  bool doesExpansionOnlyAddNewDeclsAtEnd() const override;

  ScopeCreator &getScopeCreator() override;

private:
  AnnotatedInsertionPoint
  expandAScopeThatCreatesANewInsertionPoint(ScopeCreator &);
};

class Portion {
public:
  const char *portionName;
  Portion(const char *n) : portionName(n) {}
  virtual ~Portion() {}

  // Make vanilla new illegal for ASTScopes.
  void *operator new(size_t bytes) = delete;
  // Need this because have virtual destructors
  void operator delete(void *data) {}

  // Only allow allocation of scopes using the allocator of a particular source
  // file.
  void *operator new(size_t bytes, const AstContext &ctx,
                     unsigned alignment = alignof(AstScopeImpl));
  void *operator new(size_t Bytes, void *Mem) {
    AST_SCOPE_ASSERT(Mem, "Allocation failed");
    return Mem;
  }

  /// Return the new insertion point
  virtual AstScopeImpl *expandScope(GenericTypeOrExtensionScope *,
                                    ScopeCreator &) const = 0;

  virtual SourceRange
  getChildlessSourceRangeOf(const GenericTypeOrExtensionScope *scope,
                            bool omitAssertions) const = 0;

  /// Returns isDone and isCascadingUse
  virtual bool lookupMembersOf(const GenericTypeOrExtensionScope *scope,
                               ArrayRef<const AstScopeImpl *>,
                               AstScopeImpl::DeclConsumer consumer) const;

  virtual NullablePtr<const AstScopeImpl>
  getLookupLimitFor(const GenericTypeOrExtensionScope *) const;

  virtual const Decl *
  getReferrentOfScope(const GenericTypeOrExtensionScope *s) const;

  virtual void beCurrent(IterableTypeScope *) const = 0;
  virtual bool isCurrentIfWasExpanded(const IterableTypeScope *) const = 0;
  virtual NullablePtr<AstScopeImpl>
  insertionPointForDeferredExpansion(IterableTypeScope *) const = 0;
  virtual SourceRange
  sourceRangeForDeferredExpansion(const IterableTypeScope *) const = 0;
  };

  // For the whole Decl scope of a GenericType or an Extension
  class GenericTypeOrExtensionWholePortion final : public Portion {
  public:
    GenericTypeOrExtensionWholePortion() : Portion("Decl") {}
    virtual ~GenericTypeOrExtensionWholePortion() {}

    // Just for TypeAlias
    AstScopeImpl *expandScope(GenericTypeOrExtensionScope *,
                              ScopeCreator &) const override;

    SourceRange getChildlessSourceRangeOf(const GenericTypeOrExtensionScope *,
                                          bool omitAssertions) const override;

    NullablePtr<const AstScopeImpl>
    getLookupLimitFor(const GenericTypeOrExtensionScope *) const override;

    const Decl *
    getReferrentOfScope(const GenericTypeOrExtensionScope *s) const override;

    /// Make whole portion lazy to avoid circularity in lookup of generic
    /// parameters of extensions. When \c bindExtension is called, it needs to
    /// unqualifed-lookup the type being extended. That causes an \c
    /// ExtensionScope
    /// (\c GenericTypeOrExtensionWholePortion) to be built.
    /// The building process needs the generic parameters, but that results in a
    /// request for the extended nominal type of the \c ExtensionDecl, which is
    /// an endless recursion. Although we only need to make \c ExtensionScope
    /// lazy, might as well do it for all \c IterableTypeScopes.

    void beCurrent(IterableTypeScope *) const override;
    bool isCurrentIfWasExpanded(const IterableTypeScope *) const override;

    NullablePtr<AstScopeImpl>
    insertionPointForDeferredExpansion(IterableTypeScope *) const override;
    SourceRange
    sourceRangeForDeferredExpansion(const IterableTypeScope *) const override;
  };

  /// GenericTypeOrExtension = GenericType or Extension
  class GenericTypeOrExtensionWhereOrBodyPortion : public Portion {
  public:
    GenericTypeOrExtensionWhereOrBodyPortion(const char *n) : Portion(n) {}
    virtual ~GenericTypeOrExtensionWhereOrBodyPortion() {}

    bool lookupMembersOf(const GenericTypeOrExtensionScope *scope,
                         ArrayRef<const AstScopeImpl *>,
                         AstScopeImpl::DeclConsumer consumer) const override;

  private:
    /// A client needs to know if a lookup result required the dynamic implicit
    /// self value. It is required if the lookup originates from a method body
    /// or a lazy pattern initializer. So, one approach would be to call the
    /// consumer to find members right from those scopes. However, because
    /// members aren't the first things searched, generics are, that approache
    /// ends up duplicating code from the \c GenericTypeOrExtensionScope. So we
    /// take the approach of doing those lookups there, and using this function
    /// to compute the selfDC from the history.
    static NullablePtr<DeclContext>
    computeSelfDC(ArrayRef<const AstScopeImpl *> history);
};

/// Behavior specific to representing the trailing where clause of a
/// GenericTypeDecl or ExtensionDecl scope.
class GenericTypeOrExtensionWherePortion final
    : public GenericTypeOrExtensionWhereOrBodyPortion {
public:
  GenericTypeOrExtensionWherePortion()
      : GenericTypeOrExtensionWhereOrBodyPortion("Where") {}

  AstScopeImpl *expandScope(GenericTypeOrExtensionScope *,
                            ScopeCreator &) const override;

  SourceRange getChildlessSourceRangeOf(const GenericTypeOrExtensionScope *,
                                        bool omitAssertions) const override;

  void beCurrent(IterableTypeScope *) const override;
  bool isCurrentIfWasExpanded(const IterableTypeScope *) const override;

  NullablePtr<AstScopeImpl>
  insertionPointForDeferredExpansion(IterableTypeScope *) const override;
  SourceRange
  sourceRangeForDeferredExpansion(const IterableTypeScope *) const override;
};

/// Behavior specific to representing the Body of a NominalTypeDecl or
/// ExtensionDecl scope
class IterableTypeBodyPortion final
    : public GenericTypeOrExtensionWhereOrBodyPortion {
public:
  IterableTypeBodyPortion()
      : GenericTypeOrExtensionWhereOrBodyPortion("Body") {}

  AstScopeImpl *expandScope(GenericTypeOrExtensionScope *,
                            ScopeCreator &) const override;
  SourceRange getChildlessSourceRangeOf(const GenericTypeOrExtensionScope *,
                                        bool omitAssertions) const override;

  void beCurrent(IterableTypeScope *) const override;
  bool isCurrentIfWasExpanded(const IterableTypeScope *) const override;
  NullablePtr<AstScopeImpl>
  insertionPointForDeferredExpansion(IterableTypeScope *) const override;
  SourceRange
  sourceRangeForDeferredExpansion(const IterableTypeScope *) const override;
};

/// GenericType or Extension scope
/// : Whole type decl, trailing where clause, or body
class GenericTypeOrExtensionScope : public AstScopeImpl {
public:
  const Portion *const portion;

  GenericTypeOrExtensionScope(const Portion *p) : portion(p) {}
  virtual ~GenericTypeOrExtensionScope() {}

  virtual NullablePtr<IterableDeclContext> getIterableDeclContext() const {
    return nullptr;
  }
  virtual bool shouldHaveABody() const { return false; }

protected:
  AstScopeImpl *expandSpecifically(ScopeCreator &scopeCreator) override;

public:
  virtual void expandBody(ScopeCreator &);

  virtual Decl *getDecl() const = 0;
  NullablePtr<Decl> getDeclIfAny() const override { return getDecl(); }
  NullablePtr<const void> getReferrent() const override;

private:
  AnnotatedInsertionPoint
  expandAScopeThatCreatesANewInsertionPoint(ScopeCreator &);

public:
  SourceRange
  getSourceRangeOfThisAstNode(bool omitAssertions = false) const override;

  /// \c tryBindExtension needs to get the extended nominal, and the DeclContext
  /// is the parent of the \c ExtensionDecl. If the \c SourceRange of an \c
  /// ExtensionScope were to start where the \c ExtensionDecl says, the lookup
  /// source locaiton would fall within the \c ExtensionScope. This inclusion
  /// would cause the lazy \c ExtensionScope to be expanded which would ask for
  /// its generic parameters in order to create those sub-scopes. That request
  /// would cause a cycle because it would ask for the extended nominal. So,
  /// move the source range of an \c ExtensionScope *past* the extended nominal
  /// type, which is not in-scope there anyway.
  virtual SourceRange moveStartPastExtendedNominal(SourceRange) const = 0;

  virtual GenericContext *getGenericContext() const = 0;
  std::string getClassName() const override;
  virtual std::string declKindName() const = 0;
  virtual bool doesDeclHaveABody() const;
  const char *portionName() const { return portion->portionName; }
  Optional<NullablePtr<DeclContext>> computeSelfDCForParent() const override;

protected:
  Optional<bool> resolveIsCascadingUseForThisScope(
      Optional<bool> isCascadingUse) const override;

public:
  // Only for DeclScope, not BodyScope
  // Returns the where clause scope, or the parent if none
  virtual AstScopeImpl *createTrailingWhereClauseScope(AstScopeImpl *parent,
                                                       ScopeCreator &);
  NullablePtr<DeclContext> getDeclContext() const override;
  virtual NullablePtr<NominalTypeDecl> getCorrespondingNominalTypeDecl() const {
    return nullptr;
  }

  virtual void createBodyScope(AstScopeImpl *leaf, ScopeCreator &) {}

protected:
  bool
  lookupLocalsOrMembers(ArrayRef<const AstScopeImpl *> history,
                        AstScopeImpl::DeclConsumer consumer) const override;
  void printSpecifics(llvm::raw_ostream &out) const override;

public:
  NullablePtr<const AstScopeImpl> getLookupLimit() const override;
  virtual NullablePtr<const AstScopeImpl> getLookupLimitForDecl() const;
};

class GenericTypeScope : public GenericTypeOrExtensionScope {
public:
  GenericTypeScope(const Portion *p) : GenericTypeOrExtensionScope(p) {}
  virtual ~GenericTypeScope() {}
  SourceRange moveStartPastExtendedNominal(SourceRange) const override;

protected:
  NullablePtr<const GenericParamList> genericParams() const override;
};

class IterableTypeScope : public GenericTypeScope {
  /// Because of \c parseDelayedDecl members can get added after the tree is
  /// constructed, and they can be out of order. Detect this happening by
  /// remembering the member count.
  unsigned memberCount = 0;

public:
  IterableTypeScope(const Portion *p) : GenericTypeScope(p) {}
  virtual ~IterableTypeScope() {}

  virtual SourceRange getBraces() const = 0;
  bool shouldHaveABody() const override { return true; }
  bool doesDeclHaveABody() const override;
  void expandBody(ScopeCreator &) override;

protected:
  void beCurrent() override;
  bool isCurrentIfWasExpanded() const override;

public:
  void makeWholeCurrent();
  bool isWholeCurrent() const;
  void makeBodyCurrent();
  bool isBodyCurrent() const;
  NullablePtr<AstScopeImpl> insertionPointForDeferredExpansion() override;
  SourceRange sourceRangeForDeferredExpansion() const override;

  void countBodies(ScopeCreator &) const;
};

class NominalTypeScope final : public IterableTypeScope {
public:
  NominalTypeDecl *decl;
  NominalTypeScope(const Portion *p, NominalTypeDecl *e)
      : IterableTypeScope(p), decl(e) {}
  virtual ~NominalTypeScope() {}

  std::string declKindName() const override { return "NominalType"; }
  NullablePtr<IterableDeclContext> getIterableDeclContext() const override {
    return decl;
  }
  NullablePtr<NominalTypeDecl>
  getCorrespondingNominalTypeDecl() const override {
    return decl;
  }
  GenericContext *getGenericContext() const override { return decl; }
  Decl *getDecl() const override { return decl; }

  SourceRange getBraces() const override;
  NullablePtr<const AstScopeImpl> getLookupLimitForDecl() const override;

  void createBodyScope(AstScopeImpl *leaf, ScopeCreator &) override;
  AstScopeImpl *createTrailingWhereClauseScope(AstScopeImpl *parent,
                                               ScopeCreator &) override;
};

class ExtensionScope final : public IterableTypeScope {
public:
  ExtensionDecl *const decl;
  ExtensionScope(const Portion *p, ExtensionDecl *e)
      : IterableTypeScope(p), decl(e) {}
  virtual ~ExtensionScope() {}

  GenericContext *getGenericContext() const override { return decl; }
  NullablePtr<IterableDeclContext> getIterableDeclContext() const override {
    return decl;
  }
  NullablePtr<NominalTypeDecl> getCorrespondingNominalTypeDecl() const override;
  std::string declKindName() const override { return "Extension"; }
  SourceRange getBraces() const override;
  SourceRange moveStartPastExtendedNominal(SourceRange) const override;
  AstScopeImpl *createTrailingWhereClauseScope(AstScopeImpl *parent,
                                               ScopeCreator &) override;
  void createBodyScope(AstScopeImpl *leaf, ScopeCreator &) override;
  Decl *getDecl() const override { return decl; }
  NullablePtr<const AstScopeImpl> getLookupLimitForDecl() const override;
protected:
  NullablePtr<const GenericParamList> genericParams() const override;
};

class TypeAliasScope final : public GenericTypeScope {
public:
  TypeAliasDecl *const decl;
  TypeAliasScope(const Portion *p, TypeAliasDecl *e)
      : GenericTypeScope(p), decl(e) {}
  virtual ~TypeAliasScope() {}

  std::string declKindName() const override { return "TypeAlias"; }
  AstScopeImpl *createTrailingWhereClauseScope(AstScopeImpl *parent,
                                               ScopeCreator &) override;
  GenericContext *getGenericContext() const override { return decl; }
  Decl *getDecl() const override { return decl; }
};

class OpaqueTypeScope final : public GenericTypeScope {
public:
  OpaqueTypeDecl *const decl;
  OpaqueTypeScope(const Portion *p, OpaqueTypeDecl *e)
      : GenericTypeScope(p), decl(e) {}
  virtual ~OpaqueTypeScope() {}

  std::string declKindName() const override { return "OpaqueType"; }
  GenericContext *getGenericContext() const override { return decl; }
  Decl *getDecl() const override { return decl; }
};

/// Since each generic parameter can "see" the preceeding ones,
/// (e.g. <A, B: A>) -- it's not legal but that's how lookup behaves --
/// Each GenericParamScope scopes just ONE parameter, and we next
/// each one within the previous one.
///
/// Here's a wrinkle: for a Subscript, the caller expects this scope (based on
/// source loc) to match requested DeclContexts for starting lookup in EITHER
/// the getter or setter AbstractFunctionDecl (context)
class GenericParamScope final : public AstScopeImpl {
public:
  /// The declaration that has generic parameters.
  Decl *const holder;
  /// The generic parameters themselves.
  GenericParamList *const paramList;
  /// The index of the current parameter.
  const unsigned index;

  GenericParamScope(Decl *holder, GenericParamList *paramList, unsigned index)
      : holder(holder), paramList(paramList), index(index) {}
  virtual ~GenericParamScope() {}

  /// Actually holder is always a GenericContext, need to test if
  /// InterfaceDecl or SubscriptDecl but will refactor later.
  NullablePtr<DeclContext> getDeclContext() const override;
  NullablePtr<const void> getReferrent() const override;
  std::string getClassName() const override;
  SourceRange
  getSourceRangeOfThisAstNode(bool omitAssertions = false) const override;

protected:
  AstScopeImpl *expandSpecifically(ScopeCreator &) override;
  void printSpecifics(llvm::raw_ostream &out) const override;

public:
  NullablePtr<AbstractStorageDecl>
  getEnclosingAbstractStorageDecl() const override;

  NullablePtr<const void> addressForPrinting() const override {
    return paramList;
  }

protected:
  bool lookupLocalsOrMembers(ArrayRef<const AstScopeImpl *>,
                             DeclConsumer) const override;
  bool doesContextMatchStartingContext(const DeclContext *) const override;
  Optional<bool>
  resolveIsCascadingUseForThisScope(Optional<bool>) const override;
};

/// Concrete class for a function/initializer/deinitializer
class AbstractFunctionDeclScope final : public AstScopeImpl {
public:
  AbstractFunctionDecl *const decl;
  AbstractFunctionDeclScope(AbstractFunctionDecl *e) : decl(e) {}
  virtual ~AbstractFunctionDeclScope() {}

protected:
  AstScopeImpl *expandSpecifically(ScopeCreator &scopeCreator) override;

private:
  void expandAScopeThatDoesNotCreateANewInsertionPoint(ScopeCreator &);

public:
  std::string getClassName() const override;
  SourceRange
  getSourceRangeOfThisAstNode(bool omitAssertions = false) const override;

protected:
  void printSpecifics(llvm::raw_ostream &out) const override;

public:
  virtual NullablePtr<DeclContext> getDeclContext() const override;

  virtual NullablePtr<Decl> getDeclIfAny() const override { return decl; }
  Decl *getDecl() const { return decl; }

  NullablePtr<AbstractStorageDecl>
  getEnclosingAbstractStorageDecl() const override;

  NullablePtr<const void> getReferrent() const override;

  static bool shouldCreateAccessorScope(const AccessorDecl *);

protected:
  SourceRange
  getSourceRangeOfEnclosedParamsOfASTNode(bool omitAssertions) const override;

private:
  static SourceLoc getParmsSourceLocOfAFD(AbstractFunctionDecl *);

protected:
  NullablePtr<const GenericParamList> genericParams() const override;

  Optional<bool>
  resolveIsCascadingUseForThisScope(Optional<bool>) const override;
};

/// The parameters for an abstract function (init/func/deinit)., subscript, and
/// enum element
class ParameterListScope final : public AstScopeImpl {
public:
  ParameterList *const params;
  /// For get functions in subscript declarations,
  /// a lookup into the subscript parameters must count as the get func context.
  const NullablePtr<DeclContext> matchingContext;

  ParameterListScope(ParameterList *params,
                     NullablePtr<DeclContext> matchingContext)
      : params(params), matchingContext(matchingContext) {}
  virtual ~ParameterListScope() {}

protected:
  AstScopeImpl *expandSpecifically(ScopeCreator &scopeCreator) override;

private:
  AnnotatedInsertionPoint
  expandAScopeThatCreatesANewInsertionPoint(ScopeCreator &);
  SourceLoc fixupEndForBadInput(SourceRange) const;

public:
  std::string getClassName() const override;
  SourceRange
  getSourceRangeOfThisAstNode(bool omitAssertions = false) const override;
  virtual NullablePtr<DeclContext> getDeclContext() const override;

  NullablePtr<AbstractStorageDecl>
  getEnclosingAbstractStorageDecl() const override;
  NullablePtr<const void> addressForPrinting() const override { return params; }
};

class AbstractFunctionBodyScope : public AstScopeImpl {
public:
  AbstractFunctionDecl *const decl;

  /// \c Parser::parseAbstractFunctionBodyDelayed can call \c
  /// AbstractFunctionDecl::setBody after the tree has been constructed. So if
  /// this changes, have to rebuild body.
  NullablePtr<BraceStmt> bodyWhenLastExpanded;

  AbstractFunctionBodyScope(AbstractFunctionDecl *e) : decl(e) {}
  virtual ~AbstractFunctionBodyScope() {}

protected:
  AstScopeImpl *expandSpecifically(ScopeCreator &scopeCreator) override;
  void beCurrent() override;
  bool isCurrentIfWasExpanded() const override;

private:
  void expandAScopeThatDoesNotCreateANewInsertionPoint(ScopeCreator &);
  void expandBody(ScopeCreator &);

public:
  SourceRange
  getSourceRangeOfThisAstNode(bool omitAssertions = false) const override;
  virtual NullablePtr<DeclContext> getDeclContext() const override {
    return decl;
  }
  virtual NullablePtr<Decl> getDeclIfAny() const override { return decl; }
  Decl *getDecl() const { return decl; }
  static bool isAMethod(const AbstractFunctionDecl *);

  NullablePtr<AstScopeImpl> getParentOfASTAncestorScopesToBeRescued() override;

protected:
  bool lookupLocalsOrMembers(ArrayRef<const AstScopeImpl *>,
                             DeclConsumer) const override;
  Optional<bool>
  resolveIsCascadingUseForThisScope(Optional<bool>) const override;

public:
  NullablePtr<AstScopeImpl> insertionPointForDeferredExpansion() override;
  SourceRange sourceRangeForDeferredExpansion() const override;
};

/// Body of methods, functions in types.
class MethodBodyScope final : public AbstractFunctionBodyScope {
public:
  MethodBodyScope(AbstractFunctionDecl *e) : AbstractFunctionBodyScope(e) {}
  std::string getClassName() const override;
  bool lookupLocalsOrMembers(ArrayRef<const AstScopeImpl *>,
                             DeclConsumer consumer) const override;

  Optional<NullablePtr<DeclContext>> computeSelfDCForParent() const override;
};

/// Body of "pure" functions, functions without an implicit "self".
class PureFunctionBodyScope final : public AbstractFunctionBodyScope {
public:
  PureFunctionBodyScope(AbstractFunctionDecl *e)
      : AbstractFunctionBodyScope(e) {}
  std::string getClassName() const override;
  bool lookupLocalsOrMembers(ArrayRef<const AstScopeImpl *>,
                             DeclConsumer consumer) const override;
};

class DefaultArgumentInitializerScope final : public AstScopeImpl {
public:
  ParamDecl *const decl;

  DefaultArgumentInitializerScope(ParamDecl *e) : decl(e) {}
  ~DefaultArgumentInitializerScope() {}

protected:
  AstScopeImpl *expandSpecifically(ScopeCreator &scopeCreator) override;

public:
  void expandAScopeThatDoesNotCreateANewInsertionPoint(ScopeCreator &);
  std::string getClassName() const override;
  SourceRange
  getSourceRangeOfThisAstNode(bool omitAssertions = false) const override;
  virtual NullablePtr<DeclContext> getDeclContext() const override;
  virtual NullablePtr<Decl> getDeclIfAny() const override { return decl; }
  Decl *getDecl() const { return decl; }

protected:
  Optional<bool>
  resolveIsCascadingUseForThisScope(Optional<bool>) const override;
};

/// Consider:
///  @_propertyWrapper
///  struct WrapperWithInitialValue {
///  }
///  struct HasWrapper {
///    @WrapperWithInitialValue var y = 17
///  }
/// Lookup has to be able to find the use of WrapperWithInitialValue, that's
/// what this scope is for. Because the source positions are screwy.

class AttachedPropertyWrapperScope final : public AstScopeImpl {
public:
  VarDecl *const decl;
  /// Because we have to avoid request cycles, we approximate the test for an
  /// AttachedPropertyWrapper with one based on source location. We might get
  /// false positives, that that doesn't hurt anything. However, the result of
  /// the conservative source range computation doesn't seem to be stable. So
  /// keep the original here, and use it for source range queries.

  const SourceRange sourceRangeWhenCreated;

  AttachedPropertyWrapperScope(VarDecl *e)
      : decl(e), sourceRangeWhenCreated(getSourceRangeOfVarDecl(e)) {
    AST_SCOPE_ASSERT(sourceRangeWhenCreated.isValid(),
                   "VarDecls must have ranges to be looked-up");
  }
  virtual ~AttachedPropertyWrapperScope() {}

protected:
  AstScopeImpl *expandSpecifically(ScopeCreator &) override;

public:
  std::string getClassName() const override;
  SourceRange
  getSourceRangeOfThisAstNode(bool omitAssertions = false) const override;
  NullablePtr<const void> addressForPrinting() const override { return decl; }
  virtual NullablePtr<DeclContext> getDeclContext() const override;

  static SourceRange getSourceRangeOfVarDecl(const VarDecl *);

private:
  void expandAScopeThatDoesNotCreateANewInsertionPoint(ScopeCreator &);
};

/// PatternBindingDecl's (PBDs) are tricky (See the comment for \c
/// PatternBindingDecl):
///
/// A PBD contains a list of "patterns", e.g.
///   var (a, b) = foo(), (c,d) = bar() which has two patterns.
///
/// For each pattern, there will be potentially three scopes:
/// always one for the declarations, maybe one for the initializers, and maybe
/// one for users of that pattern.
///
/// If a PBD occurs in code, its initializer can access all prior declarations.
/// Thus, a new scope must be created, nested in the scope of the PBD.
/// In contrast, if a PBD occurs in a type declaration body, its initializer
/// cannot access prior declarations in that body.
///
/// As a further complication, we get VarDecls and their accessors in deferred
/// which really must go into one of the PBD scopes. So we discard them in
/// createIfNeeded, and special-case their creation in
/// addVarDeclScopesAndTheirAccessors.

class AbstractPatternEntryScope : public AstScopeImpl {
public:
  PatternBindingDecl *const decl;
  const unsigned patternEntryIndex;
  const DeclVisibilityKind vis;

  AbstractPatternEntryScope(PatternBindingDecl *, unsigned entryIndex,
                            DeclVisibilityKind);
  virtual ~AbstractPatternEntryScope() {}

  const PatternBindingEntry &getPatternEntry() const;
  Pattern *getPattern() const;

protected:
  void printSpecifics(llvm::raw_ostream &out) const override;
  void forEachVarDeclWithLocalizableAccessors(
      ScopeCreator &scopeCreator, function_ref<void(VarDecl *)> foundOne) const;

public:
  bool isLastEntry() const;
  NullablePtr<Decl> getDeclIfAny() const override { return decl; }
  Decl *getDecl() const { return decl; }
};

class PatternEntryDeclScope final : public AbstractPatternEntryScope {
  const Expr *initWhenLastExpanded;
  unsigned varCountWhenLastExpanded = 0;

public:
  PatternEntryDeclScope(PatternBindingDecl *pbDecl, unsigned entryIndex,
                        DeclVisibilityKind vis)
      : AbstractPatternEntryScope(pbDecl, entryIndex, vis) {}
  virtual ~PatternEntryDeclScope() {}

protected:
  AstScopeImpl *expandSpecifically(ScopeCreator &scopeCreator) override;
  void beCurrent() override;
  bool isCurrentIfWasExpanded() const override;

private:
  AnnotatedInsertionPoint
  expandAScopeThatCreatesANewInsertionPoint(ScopeCreator &);

public:
  std::string getClassName() const override;
  SourceRange
  getSourceRangeOfThisAstNode(bool omitAssertions = false) const override;

  NullablePtr<const void> getReferrent() const override;

protected:
  bool lookupLocalsOrMembers(ArrayRef<const AstScopeImpl *>,
                             DeclConsumer) const override;
};

class PatternEntryInitializerScope final : public AbstractPatternEntryScope {
  Expr *initAsWrittenWhenCreated;

public:
  PatternEntryInitializerScope(PatternBindingDecl *pbDecl, unsigned entryIndex,
                               DeclVisibilityKind vis)
      : AbstractPatternEntryScope(pbDecl, entryIndex, vis),
        initAsWrittenWhenCreated(pbDecl->getOriginalInit(entryIndex)) {}
  virtual ~PatternEntryInitializerScope() {}

protected:
  AstScopeImpl *expandSpecifically(ScopeCreator &scopeCreator) override;

private:
  AnnotatedInsertionPoint
  expandAScopeThatCreatesANewInsertionPoint(ScopeCreator &);

public:
  std::string getClassName() const override;
  SourceRange
  getSourceRangeOfThisAstNode(bool omitAssertions = false) const override;
  virtual NullablePtr<DeclContext> getDeclContext() const override;

  Optional<NullablePtr<DeclContext>> computeSelfDCForParent() const override;

protected:
  bool lookupLocalsOrMembers(ArrayRef<const AstScopeImpl *>,
                             DeclConsumer) const override;

  Optional<bool>
  resolveIsCascadingUseForThisScope(Optional<bool>) const override;
};

/// The scope introduced by a conditional clause in an if/guard/while
/// statement.
/// Since there may be more than one "let foo = ..." in (e.g.) an "if",
/// we allocate a matrushka of these.
class ConditionalClauseScope final : public AstScopeImpl {
public:
  LabeledConditionalStmt *const stmt;
  const unsigned index;
  const SourceLoc endLoc; // cannot get it from the stmt

  ConditionalClauseScope(LabeledConditionalStmt *stmt, unsigned index,
                         SourceLoc endLoc)
      : stmt(stmt), index(index), endLoc(endLoc) {}

  virtual ~ConditionalClauseScope() {}

protected:
  AstScopeImpl *expandSpecifically(ScopeCreator &scopeCreator) override;

private:
  AnnotatedInsertionPoint
  expandAScopeThatCreatesANewInsertionPoint(ScopeCreator &);

public:
  std::string getClassName() const override;

protected:
  void printSpecifics(llvm::raw_ostream &out) const override;

public:
  SourceRange
  getSourceRangeOfThisAstNode(bool omitAssertions = false) const override;

private:
  ArrayRef<StmtConditionElement> getCond() const;
  const StmtConditionElement &getStmtConditionElement() const;
};

/// If, while, & guard statements all start with a conditional clause, then some
/// later part of the statement, (then, body, or after the guard) circumvents
/// the normal lookup rule to pass the lookup scope into the deepest conditional
/// clause.
class ConditionalClausePatternUseScope final : public AstScopeImpl {
  Pattern *const pattern;
  const SourceLoc startLoc;

public:
  ConditionalClausePatternUseScope(Pattern *pattern, SourceLoc startLoc)
      : pattern(pattern), startLoc(startLoc) {}

  SourceRange
  getSourceRangeOfThisAstNode(bool omitAssertions = false) const override;
  std::string getClassName() const override;

protected:
  AstScopeImpl *expandSpecifically(ScopeCreator &) override;
  bool lookupLocalsOrMembers(ArrayRef<const AstScopeImpl *>,
                             DeclConsumer) const override;
  void printSpecifics(llvm::raw_ostream &out) const override;
};


/// Capture lists may contain initializer expressions
/// No local bindings here (other than closures in initializers);
/// rather include these in the params or body local bindings
class CaptureListScope final : public AstScopeImpl {
public:
  CaptureListExpr *const expr;
  CaptureListScope(CaptureListExpr *e) : expr(e) {}
  virtual ~CaptureListScope() {}

protected:
  AstScopeImpl *expandSpecifically(ScopeCreator &scopeCreator) override;

private:
  void expandAScopeThatDoesNotCreateANewInsertionPoint(ScopeCreator &);

public:
  std::string getClassName() const override;
  SourceRange
  getSourceRangeOfThisAstNode(bool omitAssertions = false) const override;
  NullablePtr<DeclContext> getDeclContext() const override;
  NullablePtr<Expr> getExprIfAny() const override { return expr; }
  Expr *getExpr() const { return expr; }
  NullablePtr<const void> getReferrent() const override;
};

// In order for compatibility with existing lookup, closures are represented
// by multiple scopes: An overall scope (including the part before the "in"
// and a body scope, including the part after the "in"
class AbstractClosureScope : public AstScopeImpl {
public:
  NullablePtr<CaptureListExpr> captureList;
  ClosureExpr *const closureExpr;

  AbstractClosureScope(ClosureExpr *closureExpr,
                       NullablePtr<CaptureListExpr> captureList)
      : captureList(captureList), closureExpr(closureExpr) {}
  virtual ~AbstractClosureScope() {}

  NullablePtr<ClosureExpr> getClosureIfClosureScope() const override;
  NullablePtr<DeclContext> getDeclContext() const override {
    return closureExpr;
  }
  NullablePtr<const void> addressForPrinting() const override {
    return closureExpr;
  }
};

class WholeClosureScope final : public AbstractClosureScope {
  const BraceStmt *bodyWhenLastExpanded;

public:
  WholeClosureScope(ClosureExpr *closureExpr,
                    NullablePtr<CaptureListExpr> captureList)
      : AbstractClosureScope(closureExpr, captureList) {}
  virtual ~WholeClosureScope() {}

protected:
  AstScopeImpl *expandSpecifically(ScopeCreator &scopeCreator) override;
  void beCurrent() override;
  bool isCurrentIfWasExpanded() const override;

private:
  void expandAScopeThatDoesNotCreateANewInsertionPoint(ScopeCreator &);

public:
  std::string getClassName() const override;
  SourceRange
  getSourceRangeOfThisAstNode(bool omitAssertions = false) const override;
  NullablePtr<Expr> getExprIfAny() const override { return closureExpr; }
  Expr *getExpr() const { return closureExpr; }
  NullablePtr<const void> getReferrent() const override;
};

/// For a closure with named parameters, this scope does the local bindings.
/// Absent if no "in".
class ClosureParametersScope final : public AbstractClosureScope {
public:
  ClosureParametersScope(ClosureExpr *closureExpr,
                         NullablePtr<CaptureListExpr> captureList)
      : AbstractClosureScope(closureExpr, captureList) {}
  virtual ~ClosureParametersScope() {}

  std::string getClassName() const override;
  SourceRange
  getSourceRangeOfThisAstNode(bool omitAssertions = false) const override;

protected:
  AstScopeImpl *expandSpecifically(ScopeCreator &) override;
  bool lookupLocalsOrMembers(ArrayRef<const AstScopeImpl *>,
                             DeclConsumer) const override;
  Optional<bool> resolveIsCascadingUseForThisScope(
      Optional<bool> isCascadingUse) const override;
};

// The body encompasses the code in the closure; the part after the "in" if
// there is an "in"
class ClosureBodyScope final : public AbstractClosureScope {
public:
  ClosureBodyScope(ClosureExpr *closureExpr,
                   NullablePtr<CaptureListExpr> captureList)
      : AbstractClosureScope(closureExpr, captureList) {}
  virtual ~ClosureBodyScope() {}

protected:
  AstScopeImpl *expandSpecifically(ScopeCreator &scopeCreator) override;

private:
  void expandAScopeThatDoesNotCreateANewInsertionPoint(ScopeCreator &);

public:
  std::string getClassName() const override;
  SourceRange
  getSourceRangeOfThisAstNode(bool omitAssertions = false) const override;

protected:
  Optional<bool> resolveIsCascadingUseForThisScope(
      Optional<bool> isCascadingUse) const override;
};

class TopLevelCodeScope final : public AstScopeImpl {
public:
  TopLevelCodeDecl *const decl;
  BraceStmt *bodyWhenLastExpanded;

  TopLevelCodeScope(TopLevelCodeDecl *e) : decl(e) {}
  virtual ~TopLevelCodeScope() {}

protected:
  AstScopeImpl *expandSpecifically(ScopeCreator &scopeCreator) override;
  void beCurrent() override;
  bool isCurrentIfWasExpanded() const override;

private:
  AnnotatedInsertionPoint
  expandAScopeThatCreatesANewInsertionPoint(ScopeCreator &);
  std::vector<AstScopeImpl *> rescueBodyScopesToReuse();

public:
  std::string getClassName() const override;
  SourceRange
  getSourceRangeOfThisAstNode(bool omitAssertions = false) const override;
  virtual NullablePtr<DeclContext> getDeclContext() const override {
    return decl;
  }
  virtual NullablePtr<Decl> getDeclIfAny() const override { return decl; }
  Decl *getDecl() const { return decl; }
  NullablePtr<const void> getReferrent() const override;

  NullablePtr<AstScopeImpl> getParentOfASTAncestorScopesToBeRescued() override;
};

/// The \c _@specialize attribute.
class SpecializeAttributeScope final : public AstScopeImpl {
public:
  SpecializeAttr *const specializeAttr;
  AbstractFunctionDecl *const whatWasSpecialized;

  SpecializeAttributeScope(SpecializeAttr *specializeAttr,
                           AbstractFunctionDecl *whatWasSpecialized)
      : specializeAttr(specializeAttr), whatWasSpecialized(whatWasSpecialized) {
  }
  virtual ~SpecializeAttributeScope() {}

  std::string getClassName() const override;
  SourceRange
  getSourceRangeOfThisAstNode(bool omitAssertions = false) const override;
  NullablePtr<const void> addressForPrinting() const override {
    return specializeAttr;
  }

  NullablePtr<AbstractStorageDecl>
  getEnclosingAbstractStorageDecl() const override;

  NullablePtr<DeclAttribute> getDeclAttributeIfAny() const override {
    return specializeAttr;
  }
  NullablePtr<const void> getReferrent() const override;

protected:
  AstScopeImpl *expandSpecifically(ScopeCreator &) override;
  bool lookupLocalsOrMembers(ArrayRef<const AstScopeImpl *>,
                             DeclConsumer) const override;
};

class SubscriptDeclScope final : public AstScopeImpl {
public:
  SubscriptDecl *const decl;

  SubscriptDeclScope(SubscriptDecl *e) : decl(e) {}
  virtual ~SubscriptDeclScope() {}

protected:
  AstScopeImpl *expandSpecifically(ScopeCreator &scopeCreator) override;

private:
  void expandAScopeThatDoesNotCreateANewInsertionPoint(ScopeCreator &);

public:
  std::string getClassName() const override;
  SourceRange
  getSourceRangeOfThisAstNode(bool omitAssertions = false) const override;

protected:
  void printSpecifics(llvm::raw_ostream &out) const override;

public:
  virtual NullablePtr<DeclContext> getDeclContext() const override {
    return decl;
  }
  virtual NullablePtr<Decl> getDeclIfAny() const override { return decl; }
  Decl *getDecl() const { return decl; }
  NullablePtr<const void> getReferrent() const override;

protected:
  SourceRange
  getSourceRangeOfEnclosedParamsOfASTNode(bool omitAssertions) const override;

  NullablePtr<const GenericParamList> genericParams() const override;
  NullablePtr<AbstractStorageDecl>
  getEnclosingAbstractStorageDecl() const override {
    return decl;
  }
public:
  bool isThisAnAbstractStorageDecl() const override { return true; }
};

class VarDeclScope final : public AstScopeImpl {

public:
  VarDecl *const decl;
  VarDeclScope(VarDecl *e) : decl(e) {}
  virtual ~VarDeclScope() {}

protected:
  AstScopeImpl *expandSpecifically(ScopeCreator &scopeCreator) override;

private:
  void expandAScopeThatDoesNotCreateANewInsertionPoint(ScopeCreator &);

public:
  std::string getClassName() const override;
  SourceRange
  getSourceRangeOfThisAstNode(bool omitAssertions = false) const override;

protected:
  void printSpecifics(llvm::raw_ostream &out) const override;

public:
  virtual NullablePtr<Decl> getDeclIfAny() const override { return decl; }
  Decl *getDecl() const { return decl; }
  NullablePtr<const void> getReferrent() const override;
  NullablePtr<AbstractStorageDecl>
  getEnclosingAbstractStorageDecl() const override {
    return decl;
  }
  bool isThisAnAbstractStorageDecl() const override { return true; }
};

class EnumElementScope : public AstScopeImpl {
  EnumElementDecl *const decl;

public:
  EnumElementScope(EnumElementDecl *e) : decl(e) {}

  SourceRange
  getSourceRangeOfThisAstNode(bool omitAssertions = false) const override;

  std::string getClassName() const override;
  AstScopeImpl *expandSpecifically(ScopeCreator &) override;
  NullablePtr<DeclContext> getDeclContext() const override { return decl; }
  NullablePtr<Decl> getDeclIfAny() const override { return decl; }
  Decl *getDecl() const { return decl; }

protected:
  SourceRange
  getSourceRangeOfEnclosedParamsOfASTNode(bool omitAssertions) const override;

private:
  void expandAScopeThatDoesNotCreateANewInsertionPoint(ScopeCreator &);
};

class AbstractStmtScope : public AstScopeImpl {
public:
  SourceRange
  getSourceRangeOfThisAstNode(bool omitAssertions = false) const override;
  virtual Stmt *getStmt() const = 0;
  NullablePtr<Stmt> getStmtIfAny() const override { return getStmt(); }
  NullablePtr<const void> getReferrent() const override;
};

class LabeledConditionalStmtScope : public AbstractStmtScope {
public:
  Stmt *getStmt() const override;
  virtual LabeledConditionalStmt *getLabeledConditionalStmt() const = 0;

  /// If a condition is present, create the martuska.
  /// Return the lookupParent for the use scope.
  AstScopeImpl *createCondScopes();

protected:
  /// Return the lookupParent required to search these.
  AstScopeImpl *createNestedConditionalClauseScopes(ScopeCreator &,
                                                    const Stmt *afterConds);
};

class IfStmtScope final : public LabeledConditionalStmtScope {
public:
  IfStmt *const stmt;
  IfStmtScope(IfStmt *e) : stmt(e) {}
  virtual ~IfStmtScope() {}

protected:
  AstScopeImpl *expandSpecifically(ScopeCreator &scopeCreator) override;

private:
  void expandAScopeThatDoesNotCreateANewInsertionPoint(ScopeCreator &);

public:
  std::string getClassName() const override;
  LabeledConditionalStmt *getLabeledConditionalStmt() const override;
};

class WhileStmtScope final : public LabeledConditionalStmtScope {
public:
  WhileStmt *const stmt;
  WhileStmtScope(WhileStmt *e) : stmt(e) {}
  virtual ~WhileStmtScope() {}

protected:
  AstScopeImpl *expandSpecifically(ScopeCreator &scopeCreator) override;

private:
  void expandAScopeThatDoesNotCreateANewInsertionPoint(ScopeCreator &);

public:
  std::string getClassName() const override;
  LabeledConditionalStmt *getLabeledConditionalStmt() const override;
};

class GuardStmtScope final : public LabeledConditionalStmtScope {
public:
  GuardStmt *const stmt;
  GuardStmtScope(GuardStmt *e) : stmt(e) {}
  virtual ~GuardStmtScope() {}

protected:
  AstScopeImpl *expandSpecifically(ScopeCreator &scopeCreator) override;

private:
  AnnotatedInsertionPoint
  expandAScopeThatCreatesANewInsertionPoint(ScopeCreator &);

public:
  std::string getClassName() const override;
  LabeledConditionalStmt *getLabeledConditionalStmt() const override;
};

/// A scope after a guard statement that follows lookups into the conditions
/// Also for:
///  The insertion point of the last statement of an active clause in an #if
///  must be the lookup parent
/// of any following scopes. But the active clause may not be the last clause.
/// In sort, this is another case where the lookup parent cannot follow the same
/// nesting as the source order. IfConfigUseScope implements this twist. It
/// follows the IfConfig, wraps all subsequent scopes, and redirects the lookup.
class LookupParentDiversionScope final : public AstScopeImpl {
public:
  AstScopeImpl *const lookupParent;
  const SourceLoc startLoc;

  LookupParentDiversionScope(AstScopeImpl *lookupParent, SourceLoc startLoc)
      : lookupParent(lookupParent), startLoc(startLoc) {}

  SourceRange
  getSourceRangeOfThisAstNode(bool omitAssertions = false) const override;
  std::string getClassName() const override;

protected:
  AstScopeImpl *expandSpecifically(ScopeCreator &) override;
  NullablePtr<const AstScopeImpl> getLookupParent() const override {
    return lookupParent;
  }
};

class RepeatWhileScope final : public AbstractStmtScope {
public:
  RepeatWhileStmt *const stmt;
  RepeatWhileScope(RepeatWhileStmt *e) : stmt(e) {}
  virtual ~RepeatWhileScope() {}

protected:
  AstScopeImpl *expandSpecifically(ScopeCreator &scopeCreator) override;

private:
  void expandAScopeThatDoesNotCreateANewInsertionPoint(ScopeCreator &);

public:
  std::string getClassName() const override;
  Stmt *getStmt() const override { return stmt; }
};

class DoCatchStmtScope final : public AbstractStmtScope {
public:
  DoCatchStmt *const stmt;
  DoCatchStmtScope(DoCatchStmt *e) : stmt(e) {}
  virtual ~DoCatchStmtScope() {}

protected:
  AstScopeImpl *expandSpecifically(ScopeCreator &scopeCreator) override;

private:
  void expandAScopeThatDoesNotCreateANewInsertionPoint(ScopeCreator &);

public:
  std::string getClassName() const override;
  Stmt *getStmt() const override { return stmt; }
};

class SwitchStmtScope final : public AbstractStmtScope {
public:
  SwitchStmt *const stmt;
  SwitchStmtScope(SwitchStmt *e) : stmt(e) {}
  virtual ~SwitchStmtScope() override {}

protected:
  AstScopeImpl *expandSpecifically(ScopeCreator &scopeCreator) override;

private:
  void expandAScopeThatDoesNotCreateANewInsertionPoint(ScopeCreator &);

public:
  std::string getClassName() const override;
  Stmt *getStmt() const override { return stmt; }
};

class ForEachStmtScope final : public AbstractStmtScope {
public:
  ForEachStmt *const stmt;
  ForEachStmtScope(ForEachStmt *e) : stmt(e) {}
  virtual ~ForEachStmtScope() override {}

protected:
  AstScopeImpl *expandSpecifically(ScopeCreator &scopeCreator) override;

private:
  void expandAScopeThatDoesNotCreateANewInsertionPoint(ScopeCreator &);

public:
  std::string getClassName() const override;
  Stmt *getStmt() const override { return stmt; }
};

class ForEachPatternScope final : public AstScopeImpl {
public:
  ForEachStmt *const stmt;
  ForEachPatternScope(ForEachStmt *e) : stmt(e) {}
  virtual ~ForEachPatternScope() override {}

protected:
  AstScopeImpl *expandSpecifically(ScopeCreator &scopeCreator) override;

private:
  void expandAScopeThatDoesNotCreateANewInsertionPoint(ScopeCreator &);

public:
  std::string getClassName() const override;
  SourceRange
  getSourceRangeOfThisAstNode(bool omitAssertions = false) const override;

protected:
  bool lookupLocalsOrMembers(ArrayRef<const AstScopeImpl *>,
                             DeclConsumer) const override;
};

class CatchStmtScope final : public AbstractStmtScope {
public:
  CatchStmt *const stmt;
  CatchStmtScope(CatchStmt *e) : stmt(e) {}
  virtual ~CatchStmtScope() override {}

protected:
  AstScopeImpl *expandSpecifically(ScopeCreator &scopeCreator) override;

private:
  void expandAScopeThatDoesNotCreateANewInsertionPoint(ScopeCreator &);

public:
  std::string getClassName() const override;
  SourceRange
  getSourceRangeOfThisAstNode(bool omitAssertions = false) const override;
  Stmt *getStmt() const override { return stmt; }

protected:
  bool lookupLocalsOrMembers(ArrayRef<const AstScopeImpl *>,
                             AstScopeImpl::DeclConsumer) const override;
};

class CaseStmtScope final : public AbstractStmtScope {
public:
  CaseStmt *const stmt;
  CaseStmtScope(CaseStmt *e) : stmt(e) {}
  virtual ~CaseStmtScope() override {}

protected:
  AstScopeImpl *expandSpecifically(ScopeCreator &scopeCreator) override;

private:
  void expandAScopeThatDoesNotCreateANewInsertionPoint(ScopeCreator &);

public:
  std::string getClassName() const override;
  SourceRange
  getSourceRangeOfThisAstNode(bool omitAssertions = false) const override;
  Stmt *getStmt() const override { return stmt; }

protected:
  bool lookupLocalsOrMembers(ArrayRef<const AstScopeImpl *>,
                             AstScopeImpl::DeclConsumer) const override;
};

class BraceStmtScope final : public AbstractStmtScope {

public:
  BraceStmt *const stmt;
  BraceStmtScope(BraceStmt *e) : stmt(e) {}
  virtual ~BraceStmtScope() override {}

protected:
  AstScopeImpl *expandSpecifically(ScopeCreator &scopeCreator) override;

private:
  AnnotatedInsertionPoint
  expandAScopeThatCreatesANewInsertionPoint(ScopeCreator &);

public:
  std::string getClassName() const override;
  SourceRange
  getSourceRangeOfThisAstNode(bool omitAssertions = false) const override;
  virtual NullablePtr<DeclContext> getDeclContext() const override;

  NullablePtr<ClosureExpr> parentClosureIfAny() const; // public??
  Stmt *getStmt() const override { return stmt; }

protected:
  bool lookupLocalsOrMembers(ArrayRef<const AstScopeImpl *>,
                             DeclConsumer) const override;
};

} // polar::ast

#endif // POLARPHP_AST_AST_SCOPE_H