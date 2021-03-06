//===------- ExistentialTransform.cpp - Transform Existential Args -------===//
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
// Transform existential parameters to generic ones.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "pil-existential-transform"

#include "polarphp/pil/optimizer/internal/funcsignaturetransforms/ExistentialTransform.h"
#include "polarphp/ast/ExistentialLayout.h"
#include "polarphp/ast/GenericEnvironment.h"
#include "polarphp/ast/TypeCheckRequests.h"
#include "polarphp/pil/lang/OptimizationRemark.h"
#include "polarphp/pil/lang/PILFunction.h"
#include "polarphp/pil/lang/PILInstruction.h"
#include "polarphp/pil/lang/TypeSubstCloner.h"
#include "polarphp/pil/optimizer/utils/BasicBlockOptUtils.h"
#include "polarphp/pil/optimizer/utils/Existential.h"
#include "polarphp/pil/optimizer/utils/Generics.h"
#include "polarphp/pil/optimizer/utils/PILOptFunctionBuilder.h"
#include "llvm/ADT/SmallVector.h"

using namespace polar;

using llvm::SmallDenseMap;
using llvm::SmallPtrSet;
using llvm::SmallVector;
using llvm::SmallVectorImpl;

/// Create a PILCloner for Existential Specilizer.
namespace {
class ExistentialSpecializerCloner
   : public TypeSubstCloner<ExistentialSpecializerCloner,
      PILOptFunctionBuilder> {
   using SuperTy =
   TypeSubstCloner<ExistentialSpecializerCloner, PILOptFunctionBuilder>;
   friend class PILInstructionVisitor<ExistentialSpecializerCloner>;
   friend class PILCloner<ExistentialSpecializerCloner>;

   PILFunction *OrigF;
   SmallVector<ArgumentDescriptor, 4> &ArgumentDescList;
   SmallDenseMap<int, GenericTypeParamType *> &ArgToGenericTypeMap;
   SmallDenseMap<int, ExistentialTransformArgumentDescriptor>
      &ExistentialArgDescriptor;

   // Use one OpenedArchetypesTracker while cloning.
   PILOpenedArchetypesTracker OpenedArchetypesTracker;

   // AllocStack instructions introduced in the new prolog that require cleanup.
   SmallVector<AllocStackInst *, 4> AllocStackInsts;
   // Temporary values introduced in the new prolog that require cleanup.
   SmallVector<PILValue, 4> CleanupValues;

protected:
   void postProcess(PILInstruction *Orig, PILInstruction *Cloned) {
      PILClonerWithScopes<ExistentialSpecializerCloner>::postProcess(Orig,
                                                                     Cloned);
   }

   void cloneArguments(SmallVectorImpl<PILValue> &entryArgs);

public:
   ExistentialSpecializerCloner(
      PILFunction *OrigF, PILFunction *NewF, SubstitutionMap Subs,
      SmallVector<ArgumentDescriptor, 4> &ArgumentDescList,
      SmallDenseMap<int, GenericTypeParamType *> &ArgToGenericTypeMap,
      SmallDenseMap<int, ExistentialTransformArgumentDescriptor>
      &ExistentialArgDescriptor)
      : SuperTy(*NewF, *OrigF, Subs), OrigF(OrigF),
        ArgumentDescList(ArgumentDescList),
        ArgToGenericTypeMap(ArgToGenericTypeMap),
        ExistentialArgDescriptor(ExistentialArgDescriptor),
        OpenedArchetypesTracker(NewF) {
      getBuilder().setOpenedArchetypesTracker(&OpenedArchetypesTracker);
   }

   void cloneAndPopulateFunction();
};
} // end anonymous namespace

/// This function will create the generic version.
void ExistentialSpecializerCloner::cloneAndPopulateFunction() {
   SmallVector<PILValue, 4> entryArgs;
   entryArgs.reserve(OrigF->getArguments().size());
   cloneArguments(entryArgs);

   // Visit original BBs in depth-first preorder, starting with the
   // entry block, cloning all instructions and terminators.
   auto *NewEntryBB = getBuilder().getFunction().getEntryBlock();
   cloneFunctionBody(&Original, NewEntryBB, entryArgs);

   // Cleanup allocations created in the new prolog.
   SmallVector<PILBasicBlock *, 4> exitingBlocks;
   getBuilder().getFunction().findExitingBlocks(exitingBlocks);
   for (auto *exitBB : exitingBlocks) {
      PILBuilderWithScope Builder(exitBB->getTerminator());
      // A return location can't be used for a non-return instruction.
      auto loc = RegularLocation::getAutoGeneratedLocation();
      for (PILValue cleanupVal : CleanupValues)
         Builder.createDestroyAddr(loc, cleanupVal);

      for (auto *ASI : llvm::reverse(AllocStackInsts))
         Builder.createDeallocStack(loc, ASI);
   }
}

// Gather the conformances needed for an existential value based on an opened
// archetype. This adds any conformances inherited from superclass constraints.
static ArrayRef<InterfaceConformanceRef>
collectExistentialConformances(ModuleDecl *M, CanType openedType,
                               CanType existentialType) {
   assert(!openedType.isAnyExistentialType());

   auto layout = existentialType.getExistentialLayout();
   auto protocols = layout.getInterfaces();

   SmallVector<InterfaceConformanceRef, 4> conformances;
   for (auto proto : protocols) {
      auto conformance = M->lookupConformance(openedType, proto->getDecl());
      assert(conformance);
      conformances.push_back(conformance);
   }
   return M->getAstContext().AllocateCopy(conformances);
}

// Create the entry basic block with the function arguments.
void ExistentialSpecializerCloner::cloneArguments(
   SmallVectorImpl<PILValue> &entryArgs) {
   auto &M = OrigF->getModule();

   // Create the new entry block.
   PILFunction &NewF = getBuilder().getFunction();
   PILBasicBlock *ClonedEntryBB = NewF.createBasicBlock();

   /// Builder will have a ScopeClone with a debugscope that is inherited from
   /// the F.
   ScopeCloner SC(NewF);
   auto DebugScope = SC.getOrCreateClonedScope(OrigF->getDebugScope());

   // Setup a NewFBuilder for the new entry block, reusing the cloner's
   // PILBuilderContext.
   PILBuilder NewFBuilder(ClonedEntryBB, DebugScope,
                          getBuilder().getBuilderContext());
   auto InsertLoc = RegularLocation::getAutoGeneratedLocation();

   auto NewFTy = NewF.getLoweredFunctionType();
   SmallVector<PILParameterInfo, 4> params;
   params.append(NewFTy->getParameters().begin(), NewFTy->getParameters().end());

   for (auto &ArgDesc : ArgumentDescList) {
      auto iter = ArgToGenericTypeMap.find(ArgDesc.Index);
      if (iter == ArgToGenericTypeMap.end()) {
         // Clone arguments that are not rewritten.
         auto Ty = params[ArgDesc.Index].getArgumentType(M, NewFTy);
         auto LoweredTy = NewF.getLoweredType(NewF.mapTypeIntoContext(Ty));
         auto MappedTy =
            LoweredTy.getCategoryType(ArgDesc.Arg->getType().getCategory());
         auto *NewArg =
            ClonedEntryBB->createFunctionArgument(MappedTy, ArgDesc.Decl);
         NewArg->setOwnershipKind(ValueOwnershipKind(
            NewF, MappedTy, ArgDesc.Arg->getArgumentConvention()));
         entryArgs.push_back(NewArg);
         continue;
      }
      // Create the generic argument.
      GenericTypeParamType *GenericParam = iter->second;
      PILType GenericPILType =
         NewF.getLoweredType(NewF.mapTypeIntoContext(GenericParam));
      GenericPILType = GenericPILType.getCategoryType(
         ArgDesc.Arg->getType().getCategory());
      auto *NewArg = ClonedEntryBB->createFunctionArgument(GenericPILType);
      NewArg->setOwnershipKind(ValueOwnershipKind(
         NewF, GenericPILType, ArgDesc.Arg->getArgumentConvention()));
      // Determine the Conformances.
      PILType ExistentialType = ArgDesc.Arg->getType().getObjectType();
      CanType OpenedType = NewArg->getType().getAstType();
      auto Conformances = collectExistentialConformances(
         M.getPolarphpModule(), OpenedType, ExistentialType.getAstType());
      auto ExistentialRepr =
         ArgDesc.Arg->getType().getPreferredExistentialRepresentation();
      auto &EAD = ExistentialArgDescriptor[ArgDesc.Index];
      switch (ExistentialRepr) {
         case ExistentialRepresentation::Opaque: {
            /// Create this sequence for init_existential_addr.:
            /// bb0(%0 : $*T):
            /// %3 = alloc_stack $P
            /// %4 = init_existential_addr %3 : $*P, $T
            /// copy_addr [take] %0 to [initialization] %4 : $*T
            /// %7 = open_existential_addr immutable_access %3 : $*P to
            /// $*@opened P
            auto *ASI =
               NewFBuilder.createAllocStack(InsertLoc, ArgDesc.Arg->getType());
            AllocStackInsts.push_back(ASI);

            auto *EAI = NewFBuilder.createInitExistentialAddr(
               InsertLoc, ASI, NewArg->getType().getAstType(), NewArg->getType(),
               Conformances);

            bool origConsumed = EAD.isConsumed;
            // If the existential is not consumed in the function body, then the one
            // we introduce here needs cleanup.
            if (!origConsumed)
               CleanupValues.push_back(ASI);

            NewFBuilder.createCopyAddr(InsertLoc, NewArg, EAI,
                                       origConsumed ? IsTake_t::IsTake
                                                    : IsTake_t::IsNotTake,
                                       IsInitialization_t::IsInitialization);
            entryArgs.push_back(ASI);
            break;
         }
         case ExistentialRepresentation::Class: {
            PILValue NewArgValue = NewArg;
            if (!NewArg->getType().isObject()) {
               NewArgValue = NewFBuilder.createLoad(InsertLoc, NewArg,
                                                    LoadOwnershipQualifier::Unqualified);
            }

            // FIXME_ownership: init_existential_ref always takes ownership of the
            // incoming reference. If the argument convention is borrowed
            // (!isConsumed), then we should create a copy_value here and add this new
            // existential to the CleanupValues vector.

            ///  Simple case: Create an init_existential.
            /// %5 = init_existential_ref %0 : $T : $T, $P
            PILValue InitRef = NewFBuilder.createInitExistentialRef(
               InsertLoc, ArgDesc.Arg->getType().getObjectType(),
               NewArg->getType().getAstType(),
               NewArgValue, Conformances);

            if (!NewArg->getType().isObject()) {
               auto alloc = NewFBuilder.createAllocStack(InsertLoc,
                                                         InitRef->getType());
               NewFBuilder.createStore(InsertLoc, InitRef, alloc,
                                       StoreOwnershipQualifier::Unqualified);
               InitRef = alloc;
               AllocStackInsts.push_back(alloc);
            }

            entryArgs.push_back(InitRef);
            break;
         }
         default: {
            llvm_unreachable("Unhandled existential type in ExistentialTransform!");
            break;
         }
      };
   }
}

/// Create a new function name for the newly generated protocol constrained
/// generic function.
std::string ExistentialTransform::createExistentialSpecializedFunctionName() {
   for (auto const &IdxIt : ExistentialArgDescriptor) {
      int Idx = IdxIt.first;
      Mangler.setArgumentExistentialToGeneric(Idx);
   }
   auto MangledName = Mangler.mangle();
   assert(!F->getModule().hasFunction(MangledName));
   return MangledName;
}

/// Convert all existential argument types to generic argument type.
void ExistentialTransform::convertExistentialArgTypesToGenericArgTypes(
   SmallVectorImpl<GenericTypeParamType *> &genericParams,
   SmallVectorImpl<Requirement> &requirements) {

   PILModule &M = F->getModule();
   auto &Ctx = M.getAstContext();
   auto FTy = F->getLoweredFunctionType();

   /// If the original function is generic, then maintain the same.
   auto OrigGenericSig = FTy->getInvocationGenericSignature();

   /// Original list of parameters
   SmallVector<PILParameterInfo, 4> params;
   params.append(FTy->getParameters().begin(), FTy->getParameters().end());

   /// Determine the existing generic parameter depth.
   int Depth = 0;
   if (OrigGenericSig != nullptr) {
      Depth = OrigGenericSig->getGenericParams().back()->getDepth() + 1;
   }

   /// Index of the Generic Parameter.
   int GPIdx = 0;

   /// Convert the protocol arguments of F to generic ones.
   for (auto const &IdxIt : ExistentialArgDescriptor) {
      int Idx = IdxIt.first;
      auto &param = params[Idx];
      auto PType = param.getArgumentType(M, FTy);
      assert(PType.isExistentialType());
      /// Generate new generic parameter.
      auto *NewGenericParam = GenericTypeParamType::get(Depth, GPIdx++, Ctx);
      genericParams.push_back(NewGenericParam);
      Requirement NewRequirement(RequirementKind::Conformance, NewGenericParam,
                                 PType);
      requirements.push_back(NewRequirement);
      ArgToGenericTypeMap.insert(
         std::pair<int, GenericTypeParamType *>(Idx, NewGenericParam));
      assert(ArgToGenericTypeMap.find(Idx) != ArgToGenericTypeMap.end());
   }
}

/// Create the signature for the newly generated protocol constrained generic
/// function.
CanPILFunctionType
ExistentialTransform::createExistentialSpecializedFunctionType() {
   auto FTy = F->getLoweredFunctionType();
   PILModule &M = F->getModule();
   auto &Ctx = M.getAstContext();
   GenericSignature NewGenericSig;
   GenericEnvironment *NewGenericEnv;

   /// If the original function is generic, then maintain the same.
   auto OrigGenericSig = FTy->getInvocationGenericSignature();

   SmallVector<GenericTypeParamType *, 2> GenericParams;
   SmallVector<Requirement, 2> Requirements;

   /// Convert existential argument types to generic argument types.
   convertExistentialArgTypesToGenericArgTypes(GenericParams, Requirements);

   /// Compute the updated generic signature.
   NewGenericSig = evaluateOrDefault(
      Ctx.evaluator,
      AbstractGenericSignatureRequest{
         OrigGenericSig.getPointer(), std::move(GenericParams),
         std::move(Requirements)},
      GenericSignature());

   NewGenericEnv = NewGenericSig->getGenericEnvironment();

   /// Create a lambda for GenericParams.
   auto getCanonicalType = [&](Type t) -> CanType {
      return t->getCanonicalType(NewGenericSig);
   };

   /// Original list of parameters
   SmallVector<PILParameterInfo, 4> params;
   params.append(FTy->getParameters().begin(), FTy->getParameters().end());

   /// Create the complete list of parameters.
   int Idx = 0;
   SmallVector<PILParameterInfo, 8> InterfaceParams;
   InterfaceParams.reserve(params.size());
   for (auto &param : params) {
      auto iter = ArgToGenericTypeMap.find(Idx);
      if (iter != ArgToGenericTypeMap.end()) {
         auto GenericParam = iter->second;
         InterfaceParams.push_back(PILParameterInfo(getCanonicalType(GenericParam),
                                                    param.getConvention()));
      } else {
         InterfaceParams.push_back(param);
      }
      Idx++;
   }

   // Add error results.
   Optional<PILResultInfo> InterfaceErrorResult;
   if (FTy->hasErrorResult()) {
      InterfaceErrorResult = FTy->getErrorResult();
   }

   /// Finally the ExtInfo.
   auto ExtInfo = FTy->getExtInfo();
   ExtInfo = ExtInfo.withRepresentation(PILFunctionTypeRepresentation::Thin);
   auto witnessMethodConformance = FTy->getWitnessMethodConformanceOrInvalid();

   /// Return the new signature.
   return PILFunctionType::get(
      NewGenericSig, ExtInfo, FTy->getCoroutineKind(),
      FTy->getCalleeConvention(), InterfaceParams, FTy->getYields(),
      FTy->getResults(), InterfaceErrorResult,
      SubstitutionMap(), false,
      Ctx, witnessMethodConformance);
}

/// Create the Thunk Body with always_inline attribute.
void ExistentialTransform::populateThunkBody() {

   PILModule &M = F->getModule();

   F->setThunk(IsSignatureOptimizedThunk);
   F->setInlineStrategy(AlwaysInline);

   /// Remove original body of F.
   for (auto It = F->begin(), End = F->end(); It != End;) {
      auto *BB = &*It++;
      removeDeadBlock(BB);
   }

   /// Create a basic block and the function arguments.
   auto *ThunkBody = F->createBasicBlock();
   for (auto &ArgDesc : ArgumentDescList) {
      auto argumentType = ArgDesc.Arg->getType();
      ThunkBody->createFunctionArgument(argumentType, ArgDesc.Decl);
   }

   /// Builder to add new instructions in the Thunk.
   PILBuilder Builder(ThunkBody);
   PILOpenedArchetypesTracker OpenedArchetypesTracker(F);
   Builder.setOpenedArchetypesTracker(&OpenedArchetypesTracker);
   Builder.setCurrentDebugScope(ThunkBody->getParent()->getDebugScope());

   /// Location to insert new instructions.
   auto Loc = ThunkBody->getParent()->getLocation();

   /// Create the function_ref instruction to the NewF.
   auto *FRI = Builder.createFunctionRefFor(Loc, NewF);

   auto GenCalleeType = NewF->getLoweredFunctionType();
   auto CalleeGenericSig = GenCalleeType->getInvocationGenericSignature();
   auto OrigGenCalleeType = F->getLoweredFunctionType();
   auto OrigCalleeGenericSig =
      OrigGenCalleeType->getInvocationGenericSignature();

   /// Determine arguments to Apply.
   /// Generate opened existentials for generics.
   SmallVector<PILValue, 8> ApplyArgs;
   // Maintain a list of arg values to be destroyed. These are consumed by the
   // convention and require a copy.
   struct Temp {
      PILValue DeallocStackEntry;
      PILValue DestroyValue;
   };
   SmallVector<Temp, 8> Temps;
   SmallDenseMap<GenericTypeParamType *, Type> GenericToOpenedTypeMap;
   for (auto &ArgDesc : ArgumentDescList) {
      auto iter = ArgToGenericTypeMap.find(ArgDesc.Index);
      auto it = ExistentialArgDescriptor.find(ArgDesc.Index);
      if (iter != ArgToGenericTypeMap.end() &&
          it != ExistentialArgDescriptor.end()) {
         ExistentialTransformArgumentDescriptor &ETAD = it->second;
         OpenedArchetypeType *Opened;
         auto OrigOperand = ThunkBody->getArgument(ArgDesc.Index);
         auto SwiftType = ArgDesc.Arg->getType().getAstType();
         auto OpenedType =
            SwiftType->openAnyExistentialType(Opened)->getCanonicalType();
         auto OpenedPILType = NewF->getLoweredType(OpenedType);
         PILValue archetypeValue;
         auto ExistentialRepr =
            ArgDesc.Arg->getType().getPreferredExistentialRepresentation();
         switch (ExistentialRepr) {
            case ExistentialRepresentation::Opaque: {
               archetypeValue = Builder.createOpenExistentialAddr(
                  Loc, OrigOperand, OpenedPILType, it->second.AccessType);
               PILValue calleeArg = archetypeValue;
               if (ETAD.isConsumed) {
                  // open_existential_addr projects a borrowed address into the
                  // existential box. Since the callee consumes the generic value, we
                  // must pass in a copy.
                  auto *ASI =
                     Builder.createAllocStack(Loc, OpenedPILType);
                  Builder.createCopyAddr(Loc, archetypeValue, ASI, IsNotTake,
                                         IsInitialization_t::IsInitialization);
                  Temps.push_back({ASI, OrigOperand});
                  calleeArg = ASI;
               }
               ApplyArgs.push_back(calleeArg);
               break;
            }
            case ExistentialRepresentation::Class: {
               // If the operand is not object type, we need an explicit load.
               PILValue OrigValue = OrigOperand;
               if (!OrigOperand->getType().isObject()) {
                  OrigValue = Builder.createLoad(Loc, OrigValue,
                                                 LoadOwnershipQualifier::Unqualified);
               }
               // OpenExistentialRef forwards ownership, so it does the right thing
               // regardless of whether the argument is borrowed or consumed.
               archetypeValue =
                  Builder.createOpenExistentialRef(Loc, OrigValue, OpenedPILType);
               if (!OrigOperand->getType().isObject()) {
                  PILValue ASI = Builder.createAllocStack(Loc, OpenedPILType);
                  Builder.createStore(Loc, archetypeValue, ASI,
                                      StoreOwnershipQualifier::Unqualified);
                  Temps.push_back({ASI, PILValue()});
                  archetypeValue = ASI;
               }
               ApplyArgs.push_back(archetypeValue);
               break;
            }
            default: {
               llvm_unreachable("Unhandled existential type in ExistentialTransform!");
               break;
            }
         };
         GenericToOpenedTypeMap.insert(
            std::pair<GenericTypeParamType *, Type>(iter->second, OpenedType));
         assert(GenericToOpenedTypeMap.find(iter->second) !=
                GenericToOpenedTypeMap.end());
      } else {
         ApplyArgs.push_back(ThunkBody->getArgument(ArgDesc.Index));
      }
   }

   unsigned int OrigDepth = 0;
   if (F->getLoweredFunctionType()->isPolymorphic()) {
      OrigDepth = OrigCalleeGenericSig->getGenericParams().back()->getDepth() + 1;
   }
   SubstitutionMap OrigSubMap = F->getForwardingSubstitutionMap();

   /// Create substitutions for Apply instructions.
   auto SubMap = SubstitutionMap::get(
      CalleeGenericSig,
      [&](SubstitutableType *type) -> Type {
         if (auto *GP = dyn_cast<GenericTypeParamType>(type)) {
            if (GP->getDepth() < OrigDepth) {
               return Type(GP).subst(OrigSubMap);
            } else {
               auto iter = GenericToOpenedTypeMap.find(GP);
               assert(iter != GenericToOpenedTypeMap.end());
               return iter->second;
            }
         } else {
            return type;
         }
      },
      MakeAbstractConformanceForGenericType());

   /// Perform the substitutions.
   auto SubstCalleeType = GenCalleeType->substGenericArgs(
      M, SubMap, Builder.getTypeExpansionContext());

   /// Obtain the Result Type.
   PILValue ReturnValue;
   auto FunctionTy = NewF->getLoweredFunctionType();
   PILFunctionConventions Conv(SubstCalleeType, M);
   PILType ResultType = Conv.getPILResultType();

   /// If the original function has error results,  we need to generate a
   /// try_apply to call a function with an error result.
   if (FunctionTy->hasErrorResult()) {
      PILFunction *Thunk = ThunkBody->getParent();
      PILBasicBlock *NormalBlock = Thunk->createBasicBlock();
      ReturnValue =
         NormalBlock->createPhiArgument(ResultType, ValueOwnershipKind::Owned);
      PILBasicBlock *ErrorBlock = Thunk->createBasicBlock();

      PILType Error = Conv.getPILType(FunctionTy->getErrorResult());
      auto *ErrorArg =
         ErrorBlock->createPhiArgument(Error, ValueOwnershipKind::Owned);
      Builder.createTryApply(Loc, FRI, SubMap, ApplyArgs, NormalBlock,
                             ErrorBlock);

      Builder.setInsertionPoint(ErrorBlock);
      Builder.createThrow(Loc, ErrorArg);
      Builder.setInsertionPoint(NormalBlock);
   } else {
      /// Create the Apply with substitutions
      ReturnValue = Builder.createApply(Loc, FRI, SubMap, ApplyArgs);
   }
   auto cleanupLoc = RegularLocation::getAutoGeneratedLocation();
   for (auto &Temp : llvm::reverse(Temps)) {
      // The original argument was copied into a temporary and consumed by the
      // callee as such:
      //   bb (%consumedExistential : $*Interface)
      //     %valAdr = open_existential_addr %consumedExistential
      //     %temp = alloc_stack $T
      //     copy_addr %valAdr to %temp // <== Temp CopyAddr
      //     apply(%temp)               // <== Temp is consumed by the apply
      //
      // Destroy the original arument and deallocation the temporary:
      //     destroy_addr %consumedExistential : $*Interface
      //     dealloc_stack %temp : $*T
      if (Temp.DestroyValue)
         Builder.createDestroyAddr(cleanupLoc, Temp.DestroyValue);
      if (Temp.DeallocStackEntry)
         Builder.createDeallocStack(cleanupLoc, Temp.DeallocStackEntry);
   }
   /// Set up the return results.
   if (NewF->isNoReturnFunction()) {
      Builder.createUnreachable(Loc);
   } else {
      Builder.createReturn(Loc, ReturnValue);
   }
}

/// Strategy to specialize existential arguments:
/// (1) Create a protocol constrained generic function from the old function;
/// (2) Create a thunk for the original function that invokes (1) including
/// setting
///     its inline strategy as always inline.
void ExistentialTransform::createExistentialSpecializedFunction() {
   std::string Name = createExistentialSpecializedFunctionName();
   PILLinkage linkage = getSpecializedLinkage(F, F->getLinkage());

   /// Create devirtualized function type.
   auto NewFTy = createExistentialSpecializedFunctionType();

   auto NewFGenericSig = NewFTy->getInvocationGenericSignature();
   auto NewFGenericEnv = NewFGenericSig->getGenericEnvironment();

   /// Step 1: Create the new protocol constrained generic function.
   NewF = FunctionBuilder.createFunction(
      linkage, Name, NewFTy, NewFGenericEnv, F->getLocation(), F->isBare(),
      F->isTransparent(), F->isSerialized(), IsNotDynamic, F->getEntryCount(),
      F->isThunk(), F->getClassSubclassScope(), F->getInlineStrategy(),
      F->getEffectsKind(), nullptr, F->getDebugScope());
   /// Set the semantics attributes for the new function.
   for (auto &Attr : F->getSemanticsAttrs())
      NewF->addSemanticsAttr(Attr);

   /// Set Unqualified ownership, if any.
   if (!F->hasOwnership()) {
      NewF->setOwnershipEliminated();
   }

   /// Step 1a: Populate the body of NewF.
   SubstitutionMap Subs = SubstitutionMap::get(
      NewFGenericSig,
      [&](SubstitutableType *type) -> Type {
         return NewFGenericEnv->mapTypeIntoContext(type);
      },
      LookUpConformanceInModule(F->getModule().getPolarphpModule()));
   ExistentialSpecializerCloner cloner(F, NewF, Subs, ArgumentDescList,
                                       ArgToGenericTypeMap,
                                       ExistentialArgDescriptor);
   cloner.cloneAndPopulateFunction();

   /// Step 2: Create the thunk with always_inline and populate its body.
   populateThunkBody();

   assert(F->getDebugScope()->Parent != NewF->getDebugScope()->Parent);

   LLVM_DEBUG(llvm::dbgs() << "After ExistentialSpecializer Pass\n"; F->dump();
                 NewF->dump(););
}
