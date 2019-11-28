//===--- PILNodes.def - Swift PIL Metaprogramming ---------------*- C++ -*-===//
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
// This source file is part of the polarphp.org open source project
//
// Copyright (c) 2017 - 2019 polarphp software foundation
// Copyright (c) 2017 - 2019 zzu_softboy <zzu_softboy@163.com>
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://polarphp.org/LICENSE.txt for license information
// See https://polarphp.org/CONTRIBUTORS.txt for the list of polarphp project authors
//
// Created by polarboy on 2019/11/27.
///
/// \file
///
/// This file defines macros used for macro-metaprogramming with PIL nodes. It
/// supports changing how macros expand by #defining an auxillary variable
/// before including PILNodes.def as summarized in the chart below:
///
/// | #define            | Operation                                               |
/// |--------------------+---------------------------------------------------------|
/// | N/A                | Visit single value insts as insts                       |
/// | VALUE              | Visit single value insts as values                      |
/// | ABSTRACT_VALUE     | Visit abstract single value insts as values             |
/// | APPLYSITE_INST     | Visit full and partial apply site insts as apply sites. |
/// | FULLAPPLYSITE_INST | Visit full apply site insts as apply sites.             |
/// | DYNAMICCAST_INST   | Visit dynamic casts as dynamic casts.                   |
///
/// We describe the triggering variables below:
///
/// 1. VALUE(ID, PARENT).
///
///     If defined will cause SingleValueInsts to passed to VALUE(ID, PARENT)
///     instead of to FULL_INST.
///
/// 2. ABSTRACT_VALUE(ID, PARENT).
///
///     If defined this will cause ABSTRACT_SINGLE_VALUE_INST to expand to
///     ABSTRACT_VALUE INSTEAD OF ABSTRACT_INST.
///
/// 3. FULLAPPLYSITE_INST(ID, PARENT).
///
///   If defined this will cause:
///
///       * FULLAPPLYSITE_SINGLE_VALUE_INST,
///       * FULLAPPLYSITE_MULTIPLE_VALUE_INST,
///       * FULLAPPLYSITE_TERMINATOR_INST,
///
///   To expand to FULLAPPLYSITE_INST(ID, PARENT) instead of SINGLE_VALUE_INST,
///   MULTIPLE_VALUE_INST, or TERMINATOR_INST.
///
/// 4. APPLYSITE_INST(ID, PARENT)
///
///   If defined this will cause:
///
///         * APPLYSITE_SINGLE_VALUE_INST
///         * APPLYSITE_MULTIPLE_VALUE_INST
///         * APPLYSITE_TERMINATOR_INST
///
///   to expand to APPLYSITE_INST(ID, PARENT) instead of delegating to
///   SINGLE_VALUE_INST.
///
/// 5. DYNAMICCAST_INST(ID, PARENT)
///
///   If defined this will cause:
///
///         * DYNAMICCAST_SINGLE_VALUE_INST
///         * DYNAMICCAST_TERMINATOR
///         * DYNAMICCAST_NON_VALUE_INST
///
///   To expand to DYNAMICCAST_INST(ID, PARENT) instead of delegating to
///   SINGLE_VALUE_INST, TERMINATOR, or NON_VALUE_INST
///
//===----------------------------------------------------------------------===//

#ifdef APPLYSITE_INST
#ifdef FULLAPPLYSITE_INST
#error "Can not query for apply site and full apply site in one include"
#endif
#endif

/// NODE(ID, PARENT)
///
///   A concrete subclass of PILNode.  ID is the name of the class as well
///   as a member of PILNodeKind.  PARENT is the name of its abstract
///   superclass.
#ifndef NODE
#define NODE(ID, PARENT)
#endif

/// SINGLE_VALUE_INST(ID, TEXTUALNAME, PARENT, MEMBEHAVIOR, MAYRELEASE)
///
///   A concrete subclass of SingleValueInstruction, which inherits from
///   both ValueBase and PILInstruction.  ID is a member of both ValueKind
///   and PILInstructionKind.
#ifndef SINGLE_VALUE_INST
#ifdef VALUE
#define SINGLE_VALUE_INST(ID, NAME, PARENT, MEMBEHAVIOR, MAYRELEASE) \
   VALUE(ID, PARENT)
#else
#define SINGLE_VALUE_INST(ID, NAME, PARENT, MEMBEHAVIOR, MAYRELEASE) \
   FULL_INST(ID, NAME, PARENT, MEMBEHAVIOR, MAYRELEASE)
#endif
#endif

/// DYNAMICCAST_SINGLE_VALUE_INST(ID, TEXTUALNAME, PARENT, MEMBEHAVIOR, MAYRELEASE)
///
/// A SINGLE_VALUE_INST that is a cast instruction. ID is a member of
/// PILDynamicCastKind.
#ifndef DYNAMICCAST_SINGLE_VALUE_INST
#ifdef DYNAMICCAST_INST
#define DYNAMICCAST_SINGLE_VALUE_INST(ID, TEXTUALNAME, PARENT, MEMBEHAVIOR, MAYRELEASE) \
   DYNAMICCAST_INST(ID, TEXTUALNAME)
#else
#define DYNAMICCAST_SINGLE_VALUE_INST(ID, TEXTUALNAME, PARENT, MEMBEHAVIOR, MAYRELEASE) \
   SINGLE_VALUE_INST(ID, TEXTUALNAME, PARENT, MEMBEHAVIOR, MAYRELEASE)
#endif
#endif

/// APPLYSITE_SINGLE_VALUE_INST(ID, TEXTUALNAME, PARENT, MEMBEHAVIOR,
///                             MAYRELEASE)
///
///   A SINGLE_VALUE_INST that is a partial or full apply site. ID is a member
///   of ApplySiteKind.
#ifndef APPLYSITE_SINGLE_VALUE_INST
#ifdef APPLYSITE_INST
#define APPLYSITE_SINGLE_VALUE_INST(ID, NAME, PARENT, MEMBEHAVIOR, MAYRELEASE) \
   APPLYSITE_INST(ID, PARENT)
#else
#define APPLYSITE_SINGLE_VALUE_INST(ID, NAME, PARENT, MEMBEHAVIOR, MAYRELEASE) \
   SINGLE_VALUE_INST(ID, NAME, PARENT, MEMBEHAVIOR, MAYRELEASE)
#endif
#endif

/// FULLAPPLYSITE_SINGLE_VALUE_INST(ID, TEXTUALNAME, PARENT, MEMBEHAVIOR,
///                                 MAYRELEASE)
///
///   A SINGLE_VALUE_INST that is a full apply site. ID is a member of
///   FullApplySiteKind and ApplySiteKind.
#ifndef FULLAPPLYSITE_SINGLE_VALUE_INST
#ifdef FULLAPPLYSITE_INST
#define FULLAPPLYSITE_SINGLE_VALUE_INST(ID, NAME, PARENT, MEMBEHAVIOR, MAYRELEASE) \
   FULLAPPLYSITE_INST(ID, PARENT)
#else
#define FULLAPPLYSITE_SINGLE_VALUE_INST(ID, NAME, PARENT, MEMBEHAVIOR, MAYRELEASE) \
   APPLYSITE_SINGLE_VALUE_INST(ID, NAME, PARENT, MEMBEHAVIOR, MAYRELEASE)
#endif
#endif

/// MULTIPLE_VALUE_INST(Id, TextualName, Parent, MemBehavior, MayRelease)
///
///   A concrete subclass of MultipleValueInstruction. ID is a member of
///   PILInstructionKind. The Node's class name is ID and the name of the base
///   class in the heirarchy is PARENT.
#ifndef MULTIPLE_VALUE_INST
#define MULTIPLE_VALUE_INST(ID, NAME, PARENT, MEMBEHAVIOR, MAYRELEASE) \
   FULL_INST(ID, NAME, PARENT, MEMBEHAVIOR, MAYRELEASE)
#endif

/// APPLYSITE_MULTIPLE_VALUE_INST(ID, TEXTUALNAME, PARENT, MEMBEHAVIOR,
///                               MAYRELEASE)
///
///   A MULTIPLE_VALUE_INST that is additionally either a partial or full apply
///   site. ID is a member of ApplySiteKind.
#ifndef APPLYSITE_MULTIPLE_VALUE_INST
#ifdef APPLYSITE_INST
#define APPLYSITE_MULTIPLE_VALUE_INST(ID, NAME, PARENT, MEMBEHAVIOR, MAYRELEASE) \
   APPLYSITE_INST(ID, PARENT)
#else
#define APPLYSITE_MULTIPLE_VALUE_INST(ID, NAME, PARENT, MEMBEHAVIOR, MAYRELEASE) \
   MULTIPLE_VALUE_INST(ID, NAME, PARENT, MEMBEHAVIOR, MAYRELEASE)
#endif
#endif

/// FULLAPPLYSITE_MULTIPLE_VALUE_INST(ID, TEXTUALNAME, PARENT, MEMBEHAVIOR,
///                                   MAYRELEASE)
///
///   A MULTIPLE_VALUE_INST that is additionally a full apply site. ID is a
///   member of FullApplySiteKind and ApplySiteKind.
#ifndef FULLAPPLYSITE_MULTIPLE_VALUE_INST
#ifdef FULLAPPLYSITE_INST
#define FULLAPPLYSITE_MULTIPLE_VALUE_INST(ID, NAME, PARENT, MEMBEHAVIOR, MAYRELEASE) \
   FULLAPPLYSITE_INST(ID, PARENT)
#else
#define FULLAPPLYSITE_MULTIPLE_VALUE_INST(ID, NAME, PARENT, MEMBEHAVIOR, MAYRELEASE) \
   APPLYSITE_MULTIPLE_VALUE_INST(ID, NAME, PARENT, MEMBEHAVIOR, MAYRELEASE)
#endif
#endif

/// MULTIPLE_VALUE_INST_RESULT(ID, PARENT)
///
///   A concrete subclass of MultipleValueInstructionResult. ID is a member of
///   ValueKind. The Node's class name is ID and the name of the base class in
///   the heirarchy is PARENT.
#ifndef MULTIPLE_VALUE_INST_RESULT
#define MULTIPLE_VALUE_INST_RESULT(ID, PARENT) VALUE(ID, PARENT)
#endif

/// VALUE(ID, PARENT)
///
///   A concrete subclass of ValueBase.  ID is a member of ValueKind.
///   ID is a member of ValueKind.  The node's class name is
///   Id, and the name of its base class (in the PILValue hierarchy) is Parent.
#ifndef VALUE
#define VALUE(ID, PARENT) NODE(ID, PARENT)
#endif

/// ARGUMENT(ID, PARENT)
///
///   A concrete subclass of PILArgument, which is a subclass of ValueBase.
#ifndef ARGUMENT
#define ARGUMENT(ID, PARENT) VALUE(ID, PARENT)
#endif

/// INST(ID, PARENT)
///
///   A concrete subclass of PILInstruction.  ID is a member of
///   PILInstructionKind.
#ifndef INST
#define INST(ID, PARENT) NODE(ID, PARENT)
#endif

/// FULL_INST(ID, NAME, PARENT, MEMBEHAVIOR, MAYRELEASE)
///
///   A macro which includes a bunch of secondary information about
///   an instruction.  In addition to the information from INST:
///
///   NAME is the name of the instruction in PIL assembly.
///   The argument will be a bare identifier, not a string literal.
///
///   MEMBEHAVIOR is an enum value that reflects the memory behavior of
///   the instruction.
///
///   MAYRELEASE indicates whether the execution of the
///   instruction may result in memory being released.
#ifndef FULL_INST
#define FULL_INST(ID, NAME, PARENT, MEMBEHAVIOR, MAYRELEASE) INST(ID, PARENT)
#endif

/// NON_VALUE_INST(ID, NAME, PARENT, MEMBEHAVIOR, MAYRELEASE)
///
///   ID is a PILInstructionKind and the name of a subclass of PILInstruction
///   that does not inherit from ValueBase.
#ifndef NON_VALUE_INST
#define NON_VALUE_INST(ID, NAME, PARENT, MEMBEHAVIOR, MAYRELEASE) \
   FULL_INST(ID, NAME, PARENT, MEMBEHAVIOR, MAYRELEASE)
#endif

#ifndef DYNAMICCAST_NON_VALUE_INST
#ifdef DYNAMICCAST_INST
#define DYNAMICCAST_NON_VALUE_INST(ID, NAME, PARENT, MEMBEHAVIOR, MAYRELEASE) \
   DYNAMICCAST_INST(ID, NAME)
#else
#define DYNAMICCAST_NON_VALUE_INST(ID, NAME, PARENT, MEMBEHAVIOR, MAYRELEASE) \
   NON_VALUE_INST(ID, NAME, PARENT, MEMBEHAVIOR, MAYRELEASE)
#endif
#endif

/// TERMINATOR(ID, NAME, PARENT, MEMBEHAVIOR, MAYRELEASE)
///
///   ID is a member of TerminatorKind and the name of a subclass of TermInst.
#ifndef TERMINATOR
#define TERMINATOR(ID, NAME, PARENT, MEMBEHAVIOR, MAYRELEASE) \
   NON_VALUE_INST(ID, NAME, PARENT, MEMBEHAVIOR, MAYRELEASE)
#endif

/// DYNAMICCAST_TERMINATOR(ID, TEXTUAL_NAME, PARENT, MEMBEHAVIOR, MAYRELEASE)
///
///   A terminator that casts its inputs. ID is a member of PILDynamicCastKind.
#ifndef DYNAMICCAST_TERMINATOR
#ifdef DYNAMICCAST_INST
#define DYNAMICCAST_TERMINATOR(ID, TEXTUAL_NAME, PARENT, MEMBEHAVIOR, MAYRELEASE) \
   DYNAMICCAST_INST(ID, TEXTUAL_NAME)
#else
#define DYNAMICCAST_TERMINATOR(ID, TEXTUAL_NAME, PARENT, MEMBEHAVIOR, MAYRELEASE) \
   TERMINATOR(ID, TEXTUAL_NAME, PARENT, MEMBEHAVIOR, MAYRELEASE)
#endif
#endif

/// APPLYSITE_TERMINATOR_INST(ID, NAME, PARENT, MEMBEHAVIOR, MAYRELEASE)
///
/// ID is a member of ApplySiteKind, TerminatorKind, and ApplySiteKind and name
/// of a subclass of TermInst.
#ifndef APPLYSITE_TERMINATOR_INST
#ifdef APPLYSITE_INST
#define APPLYSITE_TERMINATOR_INST(ID, NAME, PARENT, MEMBEHAVIOR, MAYRELEASE) \
   APPLYSITE_INST(ID, NAME)
#else
#define APPLYSITE_TERMINATOR_INST(ID, NAME, PARENT, MEMBEHAVIOR, MAYRELEASE) \
   TERMINATOR(ID, NAME, PARENT, MEMBEHAVIOR, MAYRELEASE)
#endif
#endif

/// FULLAPPLYSITE_TERMINATOR(ID, NAME, PARENT, MEMBEHAVIOR, MAYRELEASE)
///
/// ID is a member of FullApplySiteKind, TerminatorKind, and ApplySiteKind and
/// name of a subclass of TermInst.
#ifndef FULLAPPLYSITE_TERMINATOR_INST
#ifdef FULLAPPLYSITE_INST
#define FULLAPPLYSITE_TERMINATOR_INST(ID, NAME, PARENT, MEMBEHAVIOR, MAYRELEASE) \
   FULLAPPLYSITE_INST(ID, PARENT)
#else
#define FULLAPPLYSITE_TERMINATOR_INST(ID, NAME, PARENT, MEMBEHAVIOR, MAYRELEASE) \
   APPLYSITE_TERMINATOR_INST(ID, NAME, PARENT, MEMBEHAVIOR, MAYRELEASE)
#endif
#endif

/// ABSTRACT_NODE(ID, PARENT)
///
///   An abstract class in the PILNode hierarchy.   It does not have an
///   enumerator in PILNodeKind and is never the most-derived type of a
///   node.  ID is the name of the class.
///
///   PARENT is the name of its abstract superclass in the node
///   hierarchy, which will be either a subject of an ABSTRACT_NODE
///   entry or PILNode.  SingleValueInstruction considers its superclass
///   to be PILInstruction for the purposes of the node hierarchy.
#ifndef ABSTRACT_NODE
#define ABSTRACT_NODE(ID, PARENT)
#endif

// Handle SingleValueInstruction.
#ifdef ABSTRACT_VALUE
#define ABSTRACT_VALUE_AND_INST(ID, VALUE_PARENT, INST_PARENT) \
   ABSTRACT_VALUE(ID, VALUE_PARENT)
#else
#define ABSTRACT_VALUE_AND_INST(ID, VALUE_PARENT, INST_PARENT) \
   ABSTRACT_INST(ID, INST_PARENT)
#endif

/// ABSTRACT_SINGLE_VALUE_INST(ID, PARENT)
///
///   An abstract subclass of SingleValueInstruction, which is therefore
///   in both the ValueBase and PILInstruction hierarchies.
#ifndef ABSTRACT_SINGLE_VALUE_INST
#ifdef ABSTRACT_VALUE
#define ABSTRACT_SINGLE_VALUE_INST(ID, PARENT) ABSTRACT_VALUE(ID, PARENT)
#else
#define ABSTRACT_SINGLE_VALUE_INST(ID, PARENT) ABSTRACT_INST(ID, PARENT)
#endif
#endif

/// ABSTRACT_VALUE(ID, PARENT)
///
///   An abstract class in the ValueBase hierarchy.   It does not have an
///   enumerator in ValueKind and is never the most-derived type of a
///   node.  ID is the name of the class.
///
///   PARENT is the name of its abstract superclass in the ValueBase
///   hierarchy, which be either a subject of an ABSTRACT_VALUE
///   entry or ValueBase.
#ifndef ABSTRACT_VALUE
#define ABSTRACT_VALUE(ID, PARENT) ABSTRACT_NODE(ID, PARENT)
#endif

/// ABSTRACT_INST(ID, PARENT)
///
///   An abstract class in the PILInstruction hierarchy.   It does not
///   enumerator in PILInstructionKind and is never the most-derived type
///   of a node.  ID is the name of the class.
///
///   PARENT is the name of its abstract superclass in the PILInstruction
///   hierarchy, which be either a subject of an ABSTRACT_INST
///   entry or PILInstruction.
#ifndef ABSTRACT_INST
#define ABSTRACT_INST(ID, PARENT) ABSTRACT_NODE(ID, PARENT)
#endif

/// NODE_RANGE(ID, PARENT)
///
///   The enumerator range of an abstract class in the PILNode hierarchy.
///   This will always appear right after the last member of the class.
#ifndef NODE_RANGE
#define NODE_RANGE(ID, FIRST, LAST)
#endif

#ifndef SINGLE_VALUE_INST_RANGE
#ifdef VALUE_RANGE
#define SINGLE_VALUE_INST_RANGE(ID, FIRST, LAST) VALUE_RANGE(ID, FIRST, LAST)
#else
#define SINGLE_VALUE_INST_RANGE(ID, FIRST, LAST) INST_RANGE(ID, FIRST, LAST)
#endif
#endif

/// VALUE_RANGE(ID, PARENT)
///
///   The enumerator range of an abstract class in the ValueBase hierarchy.
#ifndef VALUE_RANGE
#define VALUE_RANGE(ID, FIRST, LAST) NODE_RANGE(ID, FIRST, LAST)
#endif

/// ARGUMENT_RANGE(ID, FIRST, LAST)
///
///   The enumerator range of an abstract class in the PILArgument
///   hierarchy.
#ifndef ARGUMENT_RANGE
#define ARGUMENT_RANGE(ID, FIRST, LAST) VALUE_RANGE(ID, FIRST, LAST)
#endif

/// INST_RANGE(ID, PARENT)
///
///   The enumerator range of an abstract class in the PILInstruction
///   hierarchy.
#ifndef INST_RANGE
#define INST_RANGE(ID, FIRST, LAST) NODE_RANGE(ID, FIRST, LAST)
#endif

ABSTRACT_NODE(ValueBase, PILNode)

ABSTRACT_VALUE(PILArgument, ValueBase)
ARGUMENT(PILPhiArgument, PILArgument)
ARGUMENT(PILFunctionArgument, PILArgument)
ARGUMENT_RANGE(PILArgument, PILPhiArgument, PILFunctionArgument)

ABSTRACT_VALUE(MultipleValueInstructionResult, ValueBase)
MULTIPLE_VALUE_INST_RESULT(BeginApplyResult, MultipleValueInstructionResult)
MULTIPLE_VALUE_INST_RESULT(DestructureStructResult, MultipleValueInstructionResult)
MULTIPLE_VALUE_INST_RESULT(DestructureTupleResult, MultipleValueInstructionResult)
VALUE_RANGE(MultipleValueInstructionResult, BeginApplyResult, DestructureTupleResult)

VALUE(PILUndef, ValueBase)

ABSTRACT_NODE(PILInstruction, PILNode)

ABSTRACT_VALUE_AND_INST(SingleValueInstruction, ValueBase, PILInstruction)
// Allocation instructions.
ABSTRACT_SINGLE_VALUE_INST(AllocationInst, SingleValueInstruction)
SINGLE_VALUE_INST(AllocStackInst, alloc_stack,
                  AllocationInst, None, DoesNotRelease)
SINGLE_VALUE_INST(AllocRefInst, alloc_ref,
                  AllocationInst, None, DoesNotRelease)
SINGLE_VALUE_INST(AllocRefDynamicInst, alloc_ref_dynamic,
                  AllocationInst, None, DoesNotRelease)
SINGLE_VALUE_INST(AllocValueBufferInst, alloc_value_buffer,
                  AllocationInst, None, DoesNotRelease)
SINGLE_VALUE_INST(AllocBoxInst, alloc_box,
                  AllocationInst, None, DoesNotRelease)
SINGLE_VALUE_INST(AllocExistentialBoxInst, alloc_existential_box,
                  AllocationInst, MayWrite, DoesNotRelease)
SINGLE_VALUE_INST_RANGE(AllocationInst, AllocStackInst, AllocExistentialBoxInst)

ABSTRACT_SINGLE_VALUE_INST(IndexingInst, SingleValueInstruction)
SINGLE_VALUE_INST(IndexAddrInst, index_addr,
                  IndexingInst, None, DoesNotRelease)
SINGLE_VALUE_INST(TailAddrInst, tail_addr,
                  IndexingInst, None, DoesNotRelease)
SINGLE_VALUE_INST(IndexRawPointerInst, index_raw_pointer,
                  IndexingInst, None, DoesNotRelease)
SINGLE_VALUE_INST_RANGE(IndexingInst, IndexAddrInst, IndexRawPointerInst)

// Literals
ABSTRACT_SINGLE_VALUE_INST(LiteralInst, SingleValueInstruction)
SINGLE_VALUE_INST(FunctionRefInst, function_ref,
                  LiteralInst, None, DoesNotRelease)
SINGLE_VALUE_INST(DynamicFunctionRefInst, dynamic_function_ref,
                  LiteralInst, None, DoesNotRelease)
SINGLE_VALUE_INST(PreviousDynamicFunctionRefInst, prev_dynamic_function_ref,
                  LiteralInst, None, DoesNotRelease)
SINGLE_VALUE_INST(GlobalAddrInst, global_addr,
                  LiteralInst, None, DoesNotRelease)
SINGLE_VALUE_INST(GlobalValueInst, global_value,
                  LiteralInst, None, DoesNotRelease)
SINGLE_VALUE_INST(IntegerLiteralInst, integer_literal,
                  LiteralInst, None, DoesNotRelease)
SINGLE_VALUE_INST(FloatLiteralInst, float_literal,
                  LiteralInst, None, DoesNotRelease)
SINGLE_VALUE_INST(StringLiteralInst, string_literal,
                  LiteralInst, None, DoesNotRelease)
SINGLE_VALUE_INST_RANGE(LiteralInst, FunctionRefInst, StringLiteralInst)

// Dynamic Dispatch
ABSTRACT_SINGLE_VALUE_INST(MethodInst, SingleValueInstruction)
SINGLE_VALUE_INST(ClassMethodInst, class_method,
                  MethodInst, None, DoesNotRelease)
SINGLE_VALUE_INST(SuperMethodInst, super_method,
                  MethodInst, None, DoesNotRelease)
SINGLE_VALUE_INST(WitnessMethodInst, witness_method,
                  MethodInst, None, DoesNotRelease)
SINGLE_VALUE_INST_RANGE(MethodInst, ClassMethodInst, WitnessMethodInst)

// Conversions
ABSTRACT_SINGLE_VALUE_INST(ConversionInst, SingleValueInstruction)
SINGLE_VALUE_INST(UpcastInst, upcast,
                  ConversionInst, None, DoesNotRelease)
SINGLE_VALUE_INST(AddressToPointerInst, address_to_pointer,
                  ConversionInst, None, DoesNotRelease)
SINGLE_VALUE_INST(PointerToAddressInst, pointer_to_address,
                  ConversionInst, None, DoesNotRelease)
SINGLE_VALUE_INST(UncheckedRefCastInst, unchecked_ref_cast,
                  ConversionInst, None, DoesNotRelease)
SINGLE_VALUE_INST(UncheckedAddrCastInst, unchecked_addr_cast,
                  ConversionInst, None, DoesNotRelease)
SINGLE_VALUE_INST(UncheckedTrivialBitCastInst, unchecked_trivial_bit_cast,
                  ConversionInst, None, DoesNotRelease)
SINGLE_VALUE_INST(UncheckedBitwiseCastInst, unchecked_bitwise_cast,
                  ConversionInst, None, DoesNotRelease)
SINGLE_VALUE_INST(RefToRawPointerInst, ref_to_raw_pointer,
                  ConversionInst, None, DoesNotRelease)
SINGLE_VALUE_INST(RawPointerToRefInst, raw_pointer_to_ref,
                  ConversionInst, None, DoesNotRelease)
#define LOADABLE_REF_STORAGE(Name, name, ...) \
   SINGLE_VALUE_INST(RefTo##Name##Inst, ref_to_##name, \
   ConversionInst, None, DoesNotRelease) \
   SINGLE_VALUE_INST(Name##ToRefInst, name##_to_ref, \
   ConversionInst, None, DoesNotRelease)
#include "polarphp/ast/ReferenceStorageDef.h"
SINGLE_VALUE_INST(ConvertFunctionInst, convert_function,
                  ConversionInst, None, DoesNotRelease)
SINGLE_VALUE_INST(ConvertEscapeToNoEscapeInst, convert_escape_to_noescape,
                  ConversionInst, None, DoesNotRelease)
SINGLE_VALUE_INST(ThinFunctionToPointerInst, thin_function_to_pointer,
                  ConversionInst, None, DoesNotRelease)
SINGLE_VALUE_INST(PointerToThinFunctionInst, pointer_to_thin_function,
                  ConversionInst, None, DoesNotRelease)
SINGLE_VALUE_INST(RefToBridgeObjectInst, ref_to_bridge_object,
                  ConversionInst, None, DoesNotRelease)
SINGLE_VALUE_INST(BridgeObjectToRefInst, bridge_object_to_ref,
                  ConversionInst, None, DoesNotRelease)
SINGLE_VALUE_INST(BridgeObjectToWordInst, bridge_object_to_word,
                  ConversionInst, None, DoesNotRelease)
SINGLE_VALUE_INST(ThinToThickFunctionInst, thin_to_thick_function,
                  ConversionInst, None, DoesNotRelease)
// unconditional_checked_cast_value reads the source value and produces
// a new value with a potentially different representation.
DYNAMICCAST_SINGLE_VALUE_INST(UnconditionalCheckedCastValueInst, unconditional_checked_cast_value,
                              ConversionInst, MayRead, MayRelease)
// unconditional_checked_cast_inst is only MayRead to prevent a subsequent
// release of the cast's source from being hoisted above the cast:
// retain X
// Y = unconditional_checked_cast_inst X
// _ = Y
// release X // This release cannot be reordered with the cast.
//
// With Semantic PIL, this pattern of unbalanced retain/release
// should never happen.  Since unconditional_checked_cast is a
// scalar cast that doesn't affect the value's representation, its
// side effect can then be modeled as None.
DYNAMICCAST_SINGLE_VALUE_INST(UnconditionalCheckedCastInst, unconditional_checked_cast,
                              ConversionInst, MayRead, DoesNotRelease)
SINGLE_VALUE_INST_RANGE(ConversionInst, UpcastInst, UnconditionalCheckedCastInst)

SINGLE_VALUE_INST(ClassifyBridgeObjectInst, classify_bridge_object,
                  SingleValueInstruction, None, DoesNotRelease)
SINGLE_VALUE_INST(ValueToBridgeObjectInst, value_to_bridge_object,
                  SingleValueInstruction, None, DoesNotRelease)
SINGLE_VALUE_INST(MarkDependenceInst, mark_dependence,
                  SingleValueInstruction, None, DoesNotRelease)
SINGLE_VALUE_INST(CopyBlockInst, copy_block,
                  SingleValueInstruction, MayHaveSideEffects, DoesNotRelease)
SINGLE_VALUE_INST(CopyBlockWithoutEscapingInst, copy_block_without_escaping,
                  SingleValueInstruction, MayHaveSideEffects, DoesNotRelease)
SINGLE_VALUE_INST(CopyValueInst, copy_value,
                  SingleValueInstruction, MayHaveSideEffects, DoesNotRelease)
#define ALWAYS_OR_SOMETIMES_LOADABLE_CHECKED_REF_STORAGE(Name, name, ...) \
   SINGLE_VALUE_INST(Copy##Name##ValueInst, copy_##name##_value, \
   SingleValueInstruction, MayHaveSideEffects, DoesNotRelease)
#include "polarphp/ast/ReferenceStorageDef.h"
SINGLE_VALUE_INST(UncheckedOwnershipConversionInst, unchecked_ownership_conversion,
                  SingleValueInstruction, MayHaveSideEffects, MayRelease)

// IsUnique does not actually write to memory but should be modeled
// as such. Its operand is a pointer to an object reference. The
// optimizer should not assume that the same object is pointed to after
// the isUnique instruction. It appears to write a new object reference.
SINGLE_VALUE_INST(IsUniqueInst, is_unique,
                  SingleValueInstruction, MayHaveSideEffects, DoesNotRelease)

SINGLE_VALUE_INST(IsEscapingClosureInst, is_escaping_closure,
                  SingleValueInstruction, MayRead, DoesNotRelease)

// Accessing memory
SINGLE_VALUE_INST(LoadInst, load,
                  SingleValueInstruction, MayRead, DoesNotRelease)
SINGLE_VALUE_INST(LoadBorrowInst, load_borrow,
                  SingleValueInstruction, MayRead, DoesNotRelease)
SINGLE_VALUE_INST(BeginBorrowInst, begin_borrow,
                  SingleValueInstruction, MayHaveSideEffects, DoesNotRelease)
SINGLE_VALUE_INST(StoreBorrowInst, store_borrow,
                  PILInstruction, MayWrite, DoesNotRelease)
SINGLE_VALUE_INST(BeginAccessInst, begin_access,
                  SingleValueInstruction, MayHaveSideEffects, DoesNotRelease)
#define NEVER_OR_SOMETIMES_LOADABLE_CHECKED_REF_STORAGE(Name, name, ...) \
   SINGLE_VALUE_INST(Load##Name##Inst, load_##name, \
   SingleValueInstruction, MayRead, DoesNotRelease)
#include "polarphp/ast/ReferenceStorageDef.h"
SINGLE_VALUE_INST(MarkUninitializedInst, mark_uninitialized,
                  SingleValueInstruction, None, DoesNotRelease)
SINGLE_VALUE_INST(ProjectValueBufferInst, project_value_buffer,
                  SingleValueInstruction, MayRead, DoesNotRelease)
SINGLE_VALUE_INST(ProjectBoxInst, project_box,
                  SingleValueInstruction, None, DoesNotRelease)
SINGLE_VALUE_INST(ProjectExistentialBoxInst, project_existential_box,
                  SingleValueInstruction, None, DoesNotRelease)

// Function Application
FULLAPPLYSITE_SINGLE_VALUE_INST(ApplyInst, apply,
                                SingleValueInstruction, MayHaveSideEffects, MayRelease)
SINGLE_VALUE_INST(BuiltinInst, builtin,
                  SingleValueInstruction, MayHaveSideEffects, MayRelease)
APPLYSITE_SINGLE_VALUE_INST(PartialApplyInst, partial_apply,
                            SingleValueInstruction, MayHaveSideEffects, DoesNotRelease)

// Metatypes
SINGLE_VALUE_INST(MetatypeInst, metatype,
                  SingleValueInstruction, None, DoesNotRelease)
SINGLE_VALUE_INST(ValueMetatypeInst, value_metatype,
                  SingleValueInstruction, MayRead, DoesNotRelease)
SINGLE_VALUE_INST(ExistentialMetatypeInst, existential_metatype,
                  SingleValueInstruction, MayRead, DoesNotRelease)

// Aggregate Types
SINGLE_VALUE_INST(ObjectInst, object,
                  SingleValueInstruction, None, DoesNotRelease)
SINGLE_VALUE_INST(TupleInst, tuple,
                  SingleValueInstruction, None, DoesNotRelease)
SINGLE_VALUE_INST(TupleExtractInst, tuple_extract,
                  SingleValueInstruction, None, DoesNotRelease)
SINGLE_VALUE_INST(TupleElementAddrInst, tuple_element_addr,
                  SingleValueInstruction, None, DoesNotRelease)
SINGLE_VALUE_INST(StructInst, struct,
                  SingleValueInstruction, None, DoesNotRelease)
SINGLE_VALUE_INST(StructExtractInst, struct_extract,
                  SingleValueInstruction, None, DoesNotRelease)
SINGLE_VALUE_INST(StructElementAddrInst, struct_element_addr,
                  SingleValueInstruction, None, DoesNotRelease)
SINGLE_VALUE_INST(RefElementAddrInst, ref_element_addr,
                  SingleValueInstruction, None, DoesNotRelease)
SINGLE_VALUE_INST(RefTailAddrInst, ref_tail_addr,
                  SingleValueInstruction, None, DoesNotRelease)

// Enums
SINGLE_VALUE_INST(EnumInst, enum,
                  SingleValueInstruction, None, DoesNotRelease)
SINGLE_VALUE_INST(UncheckedEnumDataInst, unchecked_enum_data,
                  SingleValueInstruction, None, DoesNotRelease)
SINGLE_VALUE_INST(InitEnumDataAddrInst, init_enum_data_addr,
                  SingleValueInstruction, None, DoesNotRelease)
SINGLE_VALUE_INST(UncheckedTakeEnumDataAddrInst, unchecked_take_enum_data_addr,
                  SingleValueInstruction, MayWrite, DoesNotRelease)
SINGLE_VALUE_INST(SelectEnumInst, select_enum,
                  SingleValueInstruction, None, DoesNotRelease)
SINGLE_VALUE_INST(SelectEnumAddrInst, select_enum_addr,
                  SingleValueInstruction, MayRead, DoesNotRelease)
SINGLE_VALUE_INST(SelectValueInst, select_value,
                  SingleValueInstruction, None, DoesNotRelease)

// Protocol and Protocol Composition Types
SINGLE_VALUE_INST(InitExistentialAddrInst, init_existential_addr,
                  SingleValueInstruction, MayWrite, DoesNotRelease)
SINGLE_VALUE_INST(InitExistentialValueInst, init_existential_value,
                  SingleValueInstruction, MayWrite, DoesNotRelease)
SINGLE_VALUE_INST(OpenExistentialAddrInst, open_existential_addr,
                  SingleValueInstruction, MayRead, DoesNotRelease)
SINGLE_VALUE_INST(InitExistentialRefInst, init_existential_ref,
                  SingleValueInstruction, None, DoesNotRelease)
SINGLE_VALUE_INST(OpenExistentialRefInst, open_existential_ref,
                  SingleValueInstruction, None, DoesNotRelease)
SINGLE_VALUE_INST(InitExistentialMetatypeInst, init_existential_metatype,
                  SingleValueInstruction, None, DoesNotRelease)
SINGLE_VALUE_INST(OpenExistentialMetatypeInst, open_existential_metatype,
                  SingleValueInstruction, None, DoesNotRelease)
SINGLE_VALUE_INST(OpenExistentialBoxInst, open_existential_box,
                  SingleValueInstruction, MayRead, DoesNotRelease)
SINGLE_VALUE_INST(OpenExistentialValueInst, open_existential_value,
                  SingleValueInstruction, MayRead, DoesNotRelease)
SINGLE_VALUE_INST(OpenExistentialBoxValueInst, open_existential_box_value,
                  SingleValueInstruction, MayRead, DoesNotRelease)

// Blocks
SINGLE_VALUE_INST(ProjectBlockStorageInst, project_block_storage,
                  SingleValueInstruction, None, DoesNotRelease)
SINGLE_VALUE_INST(InitBlockStorageHeaderInst, init_block_storage_header,
                  SingleValueInstruction, None, DoesNotRelease)

// Key paths
// TODO: The only "side effect" is potentially retaining the returned key path
// object; is there a more specific effect?
SINGLE_VALUE_INST(KeyPathInst, keypath,
                  SingleValueInstruction, MayHaveSideEffects, DoesNotRelease)

SINGLE_VALUE_INST_RANGE(SingleValueInstruction, AllocStackInst, KeyPathInst)

NODE_RANGE(ValueBase, PILPhiArgument, KeyPathInst)

// Terminators
ABSTRACT_INST(TermInst, PILInstruction)
TERMINATOR(UnreachableInst, unreachable,
           TermInst, None, DoesNotRelease)
TERMINATOR(ReturnInst, return,
           TermInst, None, DoesNotRelease)
TERMINATOR(ThrowInst, throw,
           TermInst, None, DoesNotRelease)
TERMINATOR(YieldInst, yield,
           TermInst, MayHaveSideEffects, MayRelease)
TERMINATOR(UnwindInst, unwind,
           TermInst, None, DoesNotRelease)
FULLAPPLYSITE_TERMINATOR_INST(TryApplyInst, try_apply,
                              TermInst, MayHaveSideEffects, MayRelease)
TERMINATOR(BranchInst, br,
           TermInst, None, DoesNotRelease)
TERMINATOR(CondBranchInst, cond_br,
           TermInst, None, DoesNotRelease)
TERMINATOR(SwitchValueInst, switch_value,
           TermInst, None, DoesNotRelease)
TERMINATOR(SwitchEnumInst, switch_enum,
           TermInst, None, DoesNotRelease)
TERMINATOR(SwitchEnumAddrInst, switch_enum_addr,
           TermInst, MayRead, DoesNotRelease)
TERMINATOR(DynamicMethodBranchInst, dynamic_method_br,
           TermInst, None, DoesNotRelease)
DYNAMICCAST_TERMINATOR(CheckedCastBranchInst, checked_cast_br,
                       TermInst, None, DoesNotRelease)
DYNAMICCAST_TERMINATOR(CheckedCastAddrBranchInst, checked_cast_addr_br,
                       TermInst, MayHaveSideEffects, MayRelease)
DYNAMICCAST_TERMINATOR(CheckedCastValueBranchInst, checked_cast_value_br,
                       TermInst, None, DoesNotRelease)
INST_RANGE(TermInst, UnreachableInst, CheckedCastValueBranchInst)

// Deallocation instructions.
ABSTRACT_INST(DeallocationInst, PILInstruction)
NON_VALUE_INST(DeallocStackInst, dealloc_stack,
               DeallocationInst, MayHaveSideEffects, DoesNotRelease)
NON_VALUE_INST(DeallocRefInst, dealloc_ref,
               DeallocationInst, MayHaveSideEffects, DoesNotRelease)
NON_VALUE_INST(DeallocPartialRefInst, dealloc_partial_ref,
               DeallocationInst, MayHaveSideEffects, DoesNotRelease)
NON_VALUE_INST(DeallocValueBufferInst, dealloc_value_buffer,
               DeallocationInst, MayHaveSideEffects, DoesNotRelease)
NON_VALUE_INST(DeallocBoxInst, dealloc_box,
               DeallocationInst, MayHaveSideEffects, DoesNotRelease)
NON_VALUE_INST(DeallocExistentialBoxInst, dealloc_existential_box,
               DeallocationInst, MayHaveSideEffects, DoesNotRelease)
INST_RANGE(DeallocationInst, DeallocStackInst, DeallocExistentialBoxInst)

// Reference Counting
ABSTRACT_INST(RefCountingInst, PILInstruction)
NON_VALUE_INST(StrongRetainInst, strong_retain,
               RefCountingInst, MayHaveSideEffects, DoesNotRelease)
NON_VALUE_INST(StrongReleaseInst, strong_release,
               RefCountingInst, MayHaveSideEffects, MayRelease)
NON_VALUE_INST(UnmanagedRetainValueInst, unmanaged_retain_value,
               RefCountingInst, MayHaveSideEffects, DoesNotRelease)
NON_VALUE_INST(UnmanagedReleaseValueInst, unmanaged_release_value,
               RefCountingInst, MayHaveSideEffects, MayRelease)
NON_VALUE_INST(UnmanagedAutoreleaseValueInst, unmanaged_autorelease_value,
               RefCountingInst, MayHaveSideEffects, DoesNotRelease)
#define ALWAYS_OR_SOMETIMES_LOADABLE_CHECKED_REF_STORAGE(Name, name, ...) \
   NON_VALUE_INST(StrongRetain##Name##Inst, strong_retain_##name, \
   RefCountingInst, MayHaveSideEffects, DoesNotRelease) \
   NON_VALUE_INST(Name##RetainInst, name##_retain, \
   RefCountingInst, MayHaveSideEffects, DoesNotRelease) \
   NON_VALUE_INST(Name##ReleaseInst, name##_release, \
   RefCountingInst, MayHaveSideEffects, MayRelease)
#include "polarphp/ast/ReferenceStorageDef.h"
NON_VALUE_INST(RetainValueInst, retain_value,
               RefCountingInst, MayHaveSideEffects, DoesNotRelease)
NON_VALUE_INST(RetainValueAddrInst, retain_value_addr,
               RefCountingInst, MayHaveSideEffects, DoesNotRelease)
NON_VALUE_INST(ReleaseValueInst, release_value,
               RefCountingInst, MayHaveSideEffects, MayRelease)
NON_VALUE_INST(ReleaseValueAddrInst, release_value_addr,
               RefCountingInst, MayHaveSideEffects, MayRelease)
NON_VALUE_INST(SetDeallocatingInst, set_deallocating,
               RefCountingInst, MayHaveSideEffects,
               DoesNotRelease)
NON_VALUE_INST(AutoreleaseValueInst, autorelease_value,
               RefCountingInst, MayHaveSideEffects,
               DoesNotRelease)
INST_RANGE(RefCountingInst, StrongRetainInst, AutoreleaseValueInst)

// BindMemory has no physical side effect. Semantically it writes to
// its affected memory region because any reads or writes accessing
// that memory must be dependent on the bind operation.
NON_VALUE_INST(BindMemoryInst, bind_memory,
               PILInstruction, MayWrite, DoesNotRelease)

// FIXME: Is MayHaveSideEffects appropriate?
NON_VALUE_INST(FixLifetimeInst, fix_lifetime,
               PILInstruction, MayHaveSideEffects, DoesNotRelease)

NON_VALUE_INST(DestroyValueInst, destroy_value,
               PILInstruction, MayHaveSideEffects, MayRelease)
NON_VALUE_INST(EndBorrowInst, end_borrow,
               PILInstruction, MayHaveSideEffects, DoesNotRelease)
NON_VALUE_INST(EndAccessInst, end_access,
               PILInstruction, MayHaveSideEffects, DoesNotRelease)
NON_VALUE_INST(BeginUnpairedAccessInst, begin_unpaired_access,
               PILInstruction, MayHaveSideEffects, DoesNotRelease)
NON_VALUE_INST(EndUnpairedAccessInst, end_unpaired_access,
               PILInstruction, MayHaveSideEffects, DoesNotRelease)
NON_VALUE_INST(StoreInst, store,
               PILInstruction, MayWrite, DoesNotRelease)
NON_VALUE_INST(AssignInst, assign,
               PILInstruction, MayWrite, DoesNotRelease)
NON_VALUE_INST(AssignByWrapperInst, assign_by_wrapper,
               PILInstruction, MayWrite, DoesNotRelease)
NON_VALUE_INST(MarkFunctionEscapeInst, mark_function_escape,
               PILInstruction, None, DoesNotRelease)
NON_VALUE_INST(DebugValueInst, debug_value,
               PILInstruction, None, DoesNotRelease)
NON_VALUE_INST(DebugValueAddrInst, debug_value_addr,
               PILInstruction, None, DoesNotRelease)
#define NEVER_OR_SOMETIMES_LOADABLE_CHECKED_REF_STORAGE(Name, name, ...) \
   NON_VALUE_INST(Store##Name##Inst, store_##name, \
   PILInstruction, MayWrite, DoesNotRelease)
#include "polarphp/ast/ReferenceStorageDef.h"
NON_VALUE_INST(CopyAddrInst, copy_addr,
               PILInstruction, MayHaveSideEffects, MayRelease)
NON_VALUE_INST(DestroyAddrInst, destroy_addr,
               PILInstruction, MayHaveSideEffects, MayRelease)
NON_VALUE_INST(EndLifetimeInst, end_lifetime,
               PILInstruction, MayHaveSideEffects, MayRelease)
NON_VALUE_INST(InjectEnumAddrInst, inject_enum_addr,
               PILInstruction, MayWrite, DoesNotRelease)
NON_VALUE_INST(DeinitExistentialAddrInst, deinit_existential_addr,
               PILInstruction, MayHaveSideEffects, DoesNotRelease)
NON_VALUE_INST(DeinitExistentialValueInst, deinit_existential_value,
               PILInstruction, MayHaveSideEffects, DoesNotRelease)
DYNAMICCAST_NON_VALUE_INST(
      UnconditionalCheckedCastAddrInst, unconditional_checked_cast_addr,
      PILInstruction, MayHaveSideEffects, MayRelease)
NON_VALUE_INST(UncheckedRefCastAddrInst, unchecked_ref_cast_addr,
               PILInstruction, MayHaveSideEffects, DoesNotRelease)
NON_VALUE_INST(AllocGlobalInst, alloc_global,
               PILInstruction, MayHaveSideEffects, DoesNotRelease)
NON_VALUE_INST(EndApplyInst, end_apply,
               PILInstruction, MayHaveSideEffects, MayRelease)
NON_VALUE_INST(AbortApplyInst, abort_apply,
               PILInstruction, MayHaveSideEffects, MayRelease)

// Runtime failure
// FIXME: Special MemBehavior for runtime failure?
NON_VALUE_INST(CondFailInst, cond_fail,
               PILInstruction, MayHaveSideEffects, DoesNotRelease)

NODE_RANGE(NonValueInstruction, UnreachableInst, CondFailInst)

ABSTRACT_INST(MultipleValueInstruction, PILInstruction)
FULLAPPLYSITE_MULTIPLE_VALUE_INST(BeginApplyInst, begin_apply,
                                  MultipleValueInstruction, MayHaveSideEffects, MayRelease)
MULTIPLE_VALUE_INST(DestructureStructInst, destructure_struct,
                    MultipleValueInstruction, None, DoesNotRelease)
MULTIPLE_VALUE_INST(DestructureTupleInst, destructure_tuple,
                    MultipleValueInstruction, None, DoesNotRelease)
INST_RANGE(MultipleValueInstruction, BeginApplyInst, DestructureTupleInst)

NODE_RANGE(PILInstruction, AllocStackInst, DestructureTupleInst)
NODE_RANGE(PILNode, PILPhiArgument, DestructureTupleInst)

#undef SINGLE_VALUE_INST_RANGE
#undef INST_RANGE
#undef ARGUMENT_RANGE
#undef VALUE_RANGE
#undef NODE_RANGE
#undef ABSTRACT_SINGLE_VALUE_INST
#undef ABSTRACT_INST
#undef ABSTRACT_VALUE
#undef ABSTRACT_NODE
#undef ABSTRACT_VALUE_AND_INST
#undef FULLAPPLYSITE_TERMINATOR_INST
#undef APPLYSITE_TERMINATOR_INST
#undef DYNAMICCAST_TERMINATOR
#undef TERMINATOR
#undef NON_VALUE_INST
#undef DYNAMICCAST_NON_VALUE_INST
#undef MULTIPLE_VALUE_INST_RESULT
#undef FULLAPPLYSITE_MULTIPLE_VALUE_INST
#undef APPLYSITE_MULTIPLE_VALUE_INST
#undef MULTIPLE_VALUE_INST
#undef FULLAPPLYSITE_SINGLE_VALUE_INST
#undef APPLYSITE_SINGLE_VALUE_INST
#undef DYNAMICCAST_SINGLE_VALUE_INST
#undef DYNAMICCAST_INST
#undef SINGLE_VALUE_INST
#undef FULL_INST
#undef INST
#undef ARGUMENT
#undef VALUE
#undef NODE
#undef APPLYSITE_INST
#undef FULLAPPLYSITE_INST

