//===--- ManagedValue.h - Exploded RValue Representation --------*- C++ -*-===//
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
// A storage structure for holding a destructured rvalue with an optional
// cleanup(s).
// Ownership of the rvalue can be "forwarded" to disable the associated
// cleanup(s).
//
//===----------------------------------------------------------------------===//

#ifndef POLARPHP_PIL_GEN_MANAGEDVALUE_H
#define POLARPHP_PIL_GEN_MANAGEDVALUE_H

#include "polarphp/pil/gen/Cleanup.h"
#include "llvm/ADT/PointerIntPair.h"
#include "polarphp/pil/lang/Consumption.h"
#include "polarphp/pil/lang/PILValue.h"

namespace polar::pil {

enum class CastConsumptionKind : unsigned char;

namespace lowering {

class Initialization;
class PILGenFunction;

/// ManagedValue - represents a singular PIL value and an optional cleanup.
/// Ownership of the ManagedValue can be "forwarded" to disable its cleanup when
/// the rvalue is consumed. A ManagedValue can also represent an LValue used as
/// a value, such as an inout function argument, and can be null.
///
/// Interesting relevant cases include:
///   LValue: the PILValue will always have an isAddress() PILType. LValues
///           never have an associated cleanup.
///   RValue, isAddress() type: an address-only RValue.
///   RValue, !isAddress() type: a loadable RValue.
///   "InContext": Represented with the lvalue flag set but with no PILValue,
///                this represents a value that was emitted directly into an
///                initialization stored by an SGFContext.
///
/// The RValue cases may or may not have a cleanup associated with the value.  A
/// cleanup is associated with +1 values of non-trivial type and +0 values of
/// non-trivial type.
///
class ManagedValue {
   /// The value (or address of an address-only value) being managed, and
   /// whether it represents an lvalue.  InContext is represented with the lvalue
   /// flag set but with a null PILValue.
   llvm::PointerIntPair<PILValue, 1, bool> valueAndFlag;

   /// A handle to the cleanup that destroys this value, or
   /// CleanupHandle::invalid() if the value has no cleanup.
   CleanupHandle cleanup;

   explicit ManagedValue(PILValue value, bool isLValue, CleanupHandle cleanup)
      : valueAndFlag(value, isLValue), cleanup(cleanup) {
   }

public:

   ManagedValue() = default;

   /// Create a managed value for a +1 rvalue.
   ///
   /// Please do not introduce new uses of this method! Instead use one of the
   /// static constructors below.
   ManagedValue(PILValue value, CleanupHandle cleanup)
      : valueAndFlag(value, false), cleanup(cleanup) {
      assert(value && "No value specified?!");
      assert((!getType().isObject() ||
              value.getOwnershipKind() != ValueOwnershipKind::None ||
              !hasCleanup()) &&
             "Objects with trivial ownership should never have a cleanup");
   }

   /// Create a managed value for a +0 rvalue.
   ///
   /// Please do not introduce new uses of this method! Instead use one of the
   /// static constructors below!
   static ManagedValue forUnmanaged(PILValue value) {
      assert(value && "No value specified");
      return ManagedValue(value, false, CleanupHandle::invalid());
   }

   /// Create a managed value for a +1 rvalue object.
   static ManagedValue forOwnedObjectRValue(PILValue value,
                                            CleanupHandle cleanup) {
      assert(value && "No value specified");
      assert(value->getType().isObject() &&
             "Expected borrowed rvalues to be objects");
      assert(value.getOwnershipKind() != ValueOwnershipKind::None);
      return ManagedValue(value, false, cleanup);
   }

   /// Create a managed value for a +1 rvalue address.
   ///
   /// From a high level perspective, this consists of a temporary buffer.
   static ManagedValue forOwnedAddressRValue(PILValue value,
                                             CleanupHandle cleanup) {
      assert(value && "No value specified");
      assert(value->getType().isAddress() && "Expected value to be an address");
      assert(value.getOwnershipKind() == ValueOwnershipKind::None &&
             "Addresses always have any ownership");
      return ManagedValue(value, false, cleanup);
   }

   /// Create a managed value for a +1 non-trivial rvalue.
   static ManagedValue forOwnedRValue(PILValue value, CleanupHandle cleanup) {
      if (value->getType().isAddress())
         return ManagedValue::forOwnedAddressRValue(value, cleanup);
      return ManagedValue::forOwnedObjectRValue(value, cleanup);
   }

   /// Create a managed value for a +0 borrowed non-trivial rvalue object.
   static ManagedValue
   forBorrowedObjectRValue(PILValue value) {
      assert(value && "No value specified");
      assert(value->getType().isObject() &&
             "Expected borrowed rvalues to be objects");
      assert(value.getOwnershipKind() != ValueOwnershipKind::None);
      return ManagedValue(value, false, CleanupHandle::invalid());
   }

   /// Create a managed value for a +0 borrowed non-trivial rvalue address.
   static ManagedValue
   forBorrowedAddressRValue(PILValue value) {
      assert(value && "No value specified");
      assert(value->getType().isAddress() && "Expected value to be an address");
      assert(value.getOwnershipKind() == ValueOwnershipKind::None &&
             "Addresses always have trivial ownership");
      return ManagedValue(value, false, CleanupHandle::invalid());
   }

   /// Create a managed value for a +0 guaranteed rvalue.
   static ManagedValue
   forBorrowedRValue(PILValue value) {
      if (value->getType().isAddress())
         return ManagedValue::forBorrowedAddressRValue(value);
      return ManagedValue::forBorrowedObjectRValue(value);
   }

   /// Create a managed value for a +0 trivial object rvalue.
   static ManagedValue forTrivialObjectRValue(PILValue value) {
      assert(value->getType().isObject() && "Expected an object");
      assert(value.getOwnershipKind() == ValueOwnershipKind::None);
      return ManagedValue(value, false, CleanupHandle::invalid());
   }

   /// Create a managed value for a +0 trivial address rvalue.
   static ManagedValue forTrivialAddressRValue(PILValue value) {
      assert(value->getType().isAddress() && "Expected an address");
      assert(value.getOwnershipKind() == ValueOwnershipKind::None);
      return ManagedValue(value, false, CleanupHandle::invalid());
   }

   /// Create a managed value for a +0 trivial rvalue.
   static ManagedValue forTrivialRValue(PILValue value) {
      if (value->getType().isObject())
         return ManagedValue::forTrivialObjectRValue(value);
      return ManagedValue::forTrivialAddressRValue(value);
   }

   /// Create a managed value for an l-value.
   static ManagedValue forLValue(PILValue value) {
      assert(value && "No value specified");
      assert(value->getType().isAddress() &&
             "lvalues always have isAddress() type");
      return ManagedValue(value, true, CleanupHandle::invalid());
   }

   /// Create a managed value that indicates that the value you're looking for
   /// got stored into an initialization specified by an SGFContext, instead of
   /// being represented by this ManagedValue.
   static ManagedValue forInContext() {
      return ManagedValue(PILValue(), true, CleanupHandle::invalid());
   }

   bool isLValue() const {
      return valueAndFlag.getInt() && valueAndFlag.getPointer();
   }
   bool isInContext() const {
      return valueAndFlag.getInt() && !valueAndFlag.getPointer();
   }

   /// Return true if this is an +0 rvalue, or has trivial type.
   bool isPlusZeroRValueOrTrivial() const {
      // If this is an lvalue or isInContext() then it is not an RValue.
      if (isLValue() || isInContext()) return false;

      // If this has a cleanup attached, then it is +1 rvalue.  If not, it is
      // either +0 or trivial (in which case +0 vs +1 doesn't matter).
      return !hasCleanup();
   }

   /// Returns true if this is an managed value that can be used safely as a +1
   /// managed value.
   ///
   /// This returns true iff:
   ///
   /// 1. All sub-values are trivially typed.
   /// 2. There exists at least one non-trivial typed sub-value and all such
   /// sub-values all have cleanups.
   ///
   /// *NOTE* Due to 1. isPlusOne and isPlusZero both return true for managed
   /// values consisting of only trivial values.
   bool isPlusOne(PILGenFunction &SGF) const;

   /// Returns true if this is an ManagedValue that can be used safely as a +0
   /// ManagedValue.
   ///
   /// Specifically, we return true if:
   ///
   /// 1. All sub-values are trivially typed.
   /// 2. At least 1 subvalue is non-trivial and all such non-trivial values do
   /// not have a cleanup.
   ///
   /// *NOTE* Due to 1. isPlusOne and isPlusZero both return true for
   /// ManagedValues consisting of only trivial values.
   bool isPlusZero() const;

   PILValue getLValueAddress() const {
      assert(isLValue() && "This isn't an lvalue");
      return getValue();
   }

   PILValue getUnmanagedValue() const {
      assert(!hasCleanup());
      return getValue();
   }
   PILValue getValue() const { return valueAndFlag.getPointer(); }

   PILType getType() const { return getValue()->getType(); }

   ValueOwnershipKind getOwnershipKind() const {
      return getValue().getOwnershipKind();
   }

   /// Transform the given ManagedValue, replacing the underlying value, but
   /// keeping the same cleanup.
   ///
   /// For owned values, this is equivalent to forwarding the cleanup and
   /// creating a new cleanup of the same type on the new value. This is useful
   /// for forwarding sequences.
   ///
   /// For all other values, it is a move.
   ManagedValue transform(PILValue newValue) && {
      assert(getValue().getOwnershipKind() == newValue.getOwnershipKind() &&
             "New value and old value must have the same ownership kind");
      ManagedValue M(newValue, isLValue(), getCleanup());
      *this = ManagedValue();
      return M;
   }

   /// Emit a copy of this value with independent ownership.
   ManagedValue copy(PILGenFunction &SGF, PILLocation loc) const;

   /// Emit a copy of this value with independent ownership into the current
   /// formal evaluation scope.
   ManagedValue formalAccessCopy(PILGenFunction &SGF, PILLocation loc);

   /// This is the same operation as 'copy', but works on +0 values that don't
   /// have cleanups.  It returns a +1 value with one.
   ManagedValue copyUnmanaged(PILGenFunction &SGF, PILLocation loc);

   /// This is the same operation as 'formalAccessCopy', but works on +0 values
   /// that don't have cleanups.  It returns a +1 value with one.
   ManagedValue formalAccessCopyUnmanaged(PILGenFunction &SGF, PILLocation loc);

   bool hasCleanup() const { return cleanup.isValid(); }
   CleanupHandle getCleanup() const { return cleanup; }

   /// Return a "borrowed" version of this value.
   ///
   /// An l-value is borrowed as itself.  A +1 r-value is borrowed as a
   /// +0 r-value, with the assumption that the original ManagedValue
   /// will not be forwarded until the borrowed value is fully used.
   ManagedValue borrow(PILGenFunction &SGF, PILLocation loc) const;

   /// Return a formally evaluated "borrowed" version of this value.
   ManagedValue formalAccessBorrow(PILGenFunction &SGF, PILLocation loc) const;

   ManagedValue unmanagedBorrow() const {
      return isLValue() ? *this : ManagedValue::forUnmanaged(getValue());
   }

   /// If this managed value is a plus one value, return *this. If this is a plus
   /// zero value, return a copy instead.
   ManagedValue ensurePlusOne(PILGenFunction &SGF, PILLocation loc) const;

   /// Given a scalar value, materialize it into memory with the
   /// exact same level of cleanup it had before.
   ManagedValue materialize(PILGenFunction &SGF, PILLocation loc) const;

   /// Disable the cleanup for this value.
   void forwardCleanup(PILGenFunction &SGF) const;

   /// Forward this value, deactivating the cleanup and returning the
   /// underlying value.
   PILValue forward(PILGenFunction &SGF) const;

   /// Forward this value into memory by storing it to the given address.
   ///
   /// \param SGF - The PILGenFunction.
   /// \param loc - the AST location to associate with emitted instructions.
   /// \param address - the address to assign to.
   void forwardInto(PILGenFunction &SGF, PILLocation loc, PILValue address);

   /// Forward this value into the given initialization.
   ///
   /// \param SGF - The PILGenFunction.
   /// \param loc - the AST location to associate with emitted instructions.
   /// \param dest - the destination to forward into
   void forwardInto(PILGenFunction &SGF, PILLocation loc, Initialization *dest);

   /// Assign this value into memory, destroying the existing
   /// value at the destination address.
   ///
   /// \param SGF - The PILGenFunction.
   /// \param loc - the AST location to associate with emitted instructions.
   /// \param address - the address to assign to.
   void assignInto(PILGenFunction &SGF, PILLocation loc, PILValue address);

   /// Store a copy of this value with independent ownership into the given
   /// uninitialized address.
   void copyInto(PILGenFunction &SGF, PILLocation loc, PILValue dest);

   /// Store a copy of this value with independent ownership into the given
   /// initialization \p dest.
   void copyInto(PILGenFunction &SGF, PILLocation loc, Initialization *dest);

   explicit operator bool() const {
      // "InContext" is not considered false.
      return bool(getValue()) || valueAndFlag.getInt();
   }

   void dump() const;
   void dump(raw_ostream &os, unsigned indent = 0) const;
   void print(raw_ostream &os) const;
};

/// A ManagedValue which may not be intended to be consumed.
///
/// The invariant is that the cleanup on a ManagedValue that's not
/// meant to be consumed should be free to clear.
///
/// Code which gets a ManagedValue from a ConsumableManagedValue
/// must be careful before handing the MV off to an API.  Many
/// PILGen APIs expect that an MV is +1, but ConsumableManagedValue
/// often traffics in borrowed values.  A value is only +1 if
/// the associated consumption is TakeAlways, but conditional
/// operation should turn TakeOnSuccess consumptions into TakeAlways
/// consumptions on their success path.
class ConsumableManagedValue {
   ManagedValue Value;
   CastConsumptionKind FinalConsumption;

public:
   /// Create an invalid CMV.
   ConsumableManagedValue() = default;

   /// Create a CMV with a specific value and consumption rule.
   /*implicit*/ ConsumableManagedValue(ManagedValue value,
                                       CastConsumptionKind finalConsumption)
      : Value(value), FinalConsumption(finalConsumption) {
      assert((value.getType().isObject() ||
              finalConsumption != CastConsumptionKind::BorrowAlways) &&
             "Can not borrow always a value");
      assert((value.getType().isAddress() ||
              finalConsumption != CastConsumptionKind::CopyOnSuccess) &&
             "Can not copy on success a value.");
   }

   /// Create a CMV for a value of trivial type.
   static ConsumableManagedValue forUnmanaged(PILValue value) {
      return { ManagedValue::forUnmanaged(value),
               CastConsumptionKind::TakeAlways };
   }

   /// Create a CMV for an owned value.
   static ConsumableManagedValue forOwned(ManagedValue value) {
      return { value, CastConsumptionKind::TakeAlways };
   }

   /// Has this been filled in with meaningful data?
   bool isValid() const { return (bool) Value; }

   bool isOwned() const {
      assert(isValid());
      return FinalConsumption == CastConsumptionKind::TakeAlways;
   }

   /// Return true if there's a cleanup associated with this value.
   bool hasCleanup() const { return Value.hasCleanup(); }
   CleanupHandle getCleanup() const { return Value.getCleanup(); }

   PILType getType() const { return Value.getType(); }
   PILValue getValue() const { return Value.getValue(); }
   ValueOwnershipKind getOwnershipKind() const {
      return Value.getOwnershipKind();
   }

   /// Return a managed value appropriate for the final use of this CMV.
   ManagedValue getFinalManagedValue() const { return Value; }

   /// Get the value as an unmanaged ManagedValue.
   ///
   /// You probably should not be using this; it's here to make it easy
   /// to find code that is probably wrong.
   ManagedValue asUnmanagedValue() const {
      return ManagedValue::forUnmanaged(Value.getValue());
   }

   /// Return the consumption rules appropriate for the final use of
   /// this CMV.
   CastConsumptionKind getFinalConsumption() const { return FinalConsumption; }

   /// Return a managed value that's appropriate for borrowing this
   /// value and promising not to consume it.
   ConsumableManagedValue asBorrowedOperand(PILGenFunction &SGF,
                                            PILLocation loc) const {
      if (getType().isAddress())
         return {asUnmanagedValue(), CastConsumptionKind::CopyOnSuccess};
      return {asUnmanagedValue().borrow(SGF, loc),
              CastConsumptionKind::BorrowAlways};
   }

   /// Return a managed value that's appropriate for copying this value and
   /// always consuming it.
   ConsumableManagedValue copy(PILGenFunction &SGF, PILLocation loc) const {
      return ConsumableManagedValue::forOwned(asUnmanagedValue().copy(SGF, loc));
   }
};

} // namespace lowering
} // namespace polar::pil

namespace polar {

template <typename To> inline bool isa(const polar::pil::lowering::ManagedValue &M) {
   return isa<To>(M.getValue());
}

} // end namespace polar

#endif // POLARPHP_PIL_GEN_MANAGEDVALUE_H
