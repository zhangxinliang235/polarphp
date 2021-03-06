//===--- EpilogueARCAnalysis.cpp ------------------------------------------===//
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

#include "polarphp/pil/optimizer/analysis/EpilogueARCAnalysis.h"
#include "polarphp/pil/optimizer/analysis/DominanceAnalysis.h"
#include "polarphp/pil/lang/PILInstruction.h"
#include "llvm/Support/CommandLine.h"

using namespace polar;

//===----------------------------------------------------------------------===//
//                          Epilogue ARC Utilities
//===----------------------------------------------------------------------===//

void EpilogueARCContext::initializeDataflow() {
   for (auto *BB : PO->getPostOrder()) {
      // Find the exit blocks.
      if (isInterestedFunctionExitingBlock(BB)) {
         ExitBlocks.insert(BB);
      }
      // Allocate state for this block.
      IndexToStateMap.emplace_back();
   }
   // Split the PILArgument into local arguments to each specific basic block.
   llvm::SmallVector<PILValue, 4> ToProcess;
   llvm::DenseSet<PILValue> Processed;
   ToProcess.push_back(Arg);
   while (!ToProcess.empty()) {
      PILValue CArg = ToProcess.pop_back_val();
      if (!CArg)
         continue;
      if (Processed.find(CArg) != Processed.end())
         continue;
      Processed.insert(CArg);
      if (auto *A = dyn_cast<PILPhiArgument>(CArg)) {
         // Find predecessor and break the PILArgument to predecessors.
         for (auto *X : A->getParent()->getPredecessorBlocks()) {
            // Try to find the predecessor edge-value.
            PILValue IA = A->getIncomingPhiValue(X);
            getState(X).LocalArg = IA;
            // Maybe the edge value is another PILArgument.
            ToProcess.push_back(IA);
         }
      }
   }
}

bool EpilogueARCContext::convergeDataflow() {
   // Keep iterating until Changed is false.
   bool Changed = false;
   do {
      Changed = false;
      // Iterate until the data flow converges.
      for (PILBasicBlock *B : PO->getPostOrder()) {
         auto &BS = getState(B);
         // Merge in all the successors.
         bool BBSetOut = false;
         if (!B->succ_empty()) {
            auto Iter = B->succ_begin();
            BBSetOut = getState(*Iter).BBSetIn;
            Iter = std::next(Iter);
            for (auto E = B->succ_end(); Iter != E; ++Iter) {
               BBSetOut &= getState(*Iter).BBSetIn;
            }
         } else if (isExitBlock(B)) {
            // We set the BBSetOut for exit blocks.
            BBSetOut = true;
         }

         // If an epilogue ARC instruction or blocking operating has been identified
         // then there is no point visiting every instruction in this block.
         if (BBSetOut) {
            // Iterate over all instructions in the basic block and find the
            // interested ARC instruction in the block.
            for (auto I = B->rbegin(), E = B->rend(); I != E; ++I) {
               // This is a transition from 1 to 0 due to an interested instruction.
               if (isInterestedInstruction(&*I)) {
                  BBSetOut = false;
                  break;
               }
               // This is a transition from 1 to 0 due to a blocking instruction.
               // at this point, its OK to abort the data flow as we have one path
               // which we did not find an epilogue retain before getting blocked.
               if (mayBlockEpilogueARC(&*I, RCFI->getRCIdentityRoot(Arg))) {
                  return false;
               }
            }
         }

         // Update BBSetIn.
         Changed |= (BS.BBSetIn != BBSetOut);
         BS.BBSetIn = BBSetOut;
      }
   } while (Changed);
   return true;
}

bool EpilogueARCContext::computeEpilogueARC() {
   // At this point the data flow should have converged. Find the epilogue
   // releases.
   for (PILBasicBlock *B : PO->getPostOrder()) {
      bool BBSetOut = false;
      // Merge in all the successors.
      if (!B->succ_empty()) {
         // Make sure we've either found no ARC instructions in all the successors
         // or we've found ARC instructions in all successors.
         //
         // In case we've found ARC instructions in some and not all successors,
         // that means from this point to the end of the function, some paths will
         // not have an epilogue ARC instruction, which means the data flow has
         // failed.
         auto Iter = B->succ_begin();
         auto Base = getState(*Iter).BBSetIn;
         Iter = std::next(Iter);
         for (auto E = B->succ_end(); Iter != E; ++Iter) {
            if (getState(*Iter).BBSetIn != Base)
               return false;
         }
         BBSetOut = Base;
      } else if (isExitBlock(B)) {
         // We set the BBSetOut for exit blocks.
         BBSetOut = true;
      }

      // If an epilogue ARC instruction or blocking operating has been identified
      // then there is no point visiting every instruction in this block.
      if (!BBSetOut) {
         continue;
      }

      // An epilogue ARC instruction has not been identified, maybe its in this block.
      //
      // Iterate over all instructions in the basic block and find the interested ARC
      // instruction in the block.
      for (auto I = B->rbegin(), E = B->rend(); I != E; ++I) {
         // This is a transition from 1 to 0 due to an interested instruction.
         if (isInterestedInstruction(&*I)) {
            EpilogueARCInsts.insert(&*I);
            break;
         }
         // This is a transition from 1 to 0 due to a blocking instruction.
         if (mayBlockEpilogueARC(&*I, RCFI->getRCIdentityRoot(Arg))) {
            break;
         }
      }
   }
   return true;
}

//===----------------------------------------------------------------------===//
//                              Main Entry Point
//===----------------------------------------------------------------------===//

void EpilogueARCAnalysis::initialize(PILPassManager *PM) {
   AA = PM->getAnalysis<AliasAnalysis>();
   PO = PM->getAnalysis<PostOrderAnalysis>();
   RC = PM->getAnalysis<RCIdentityAnalysis>();
}

PILAnalysis *polar::createEpilogueARCAnalysis(PILModule *M) {
   return new EpilogueARCAnalysis(M);
}
