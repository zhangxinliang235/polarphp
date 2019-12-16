//===--- AbstractionPattern.h - PIL type abstraction patterns ---*- C++ -*-===//
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
// This file defines the AbstractionPattern class, which is used to
// lower formal AST types into their PIL lowerings.
//
//===----------------------------------------------------------------------===//

#ifndef POLARPHP_PIL_ABSTRACTIONPATTERN_H
#define POLARPHP_PIL_ABSTRACTIONPATTERN_H

#include "polarphp/ast/Decl.h"
#include "polarphp/ast/Types.h"

namespace llvm {
  template <class T> class function_ref;
}

namespace clang {
  class CXXMethodDecl;
  class ObjCMethodDecl;
  class Type;
  class ValueDecl;
}

namespace polar::lowering {

using polar::ast::CanGenericSignature;
using polar::ast::CanType;
using polar::ast::Type;
using polar::ast::AnyFunctionType;
using polar::ast::ImportAsMemberStatus;
using polar::ast::ForeignErrorConvention;
using polar::ast::GenericFunctionType;
using polar::ast::AbstractFunctionDecl;
using polar::ast::ValueDecl;
using polar::ast::DependentMemberType;
using polar::ast::GenericTypeParamType;
using polar::ast::ArchetypeType;
using polar::ast::OpaqueTypeArchetypeType;
using polar::ast::LayoutConstraint;
using polar::ast::CanTypeWrapperTraits;
using polar::ast::TupleType;
using polar::ast::CanTupleType;

/// A pattern for the abstraction of a value.
///
/// The representation of values in Swift can vary according to how
/// their type is abstracted: which is to say, according to the pattern
/// of opaque type variables within their type.  The main motivation
/// here is performance: it would be far easier for types to adopt a
/// single representation regardless of their abstraction, but this
/// would force Swift to adopt a very inefficient representation for
/// abstractable values.
///
/// For example, consider the comparison function on Int:
///   func <(lhs : Int, rhs : Int) -> Bool
///
/// This function can be used as an opaque value of type
/// (Int,Int)->Bool.  An optimal representation of values of that type
/// (ignoring context parameters for the moment) would be a pointer to
/// a function that takes these two arguments directly in registers and
/// returns the result directly in a register.
///
/// (It's important to remember throughout this discussion that we're
/// talking about abstract values.  There's absolutely nothing that
/// requires direct uses of the function to follow the same conventions
/// as abstract uses!  A direct use of a declaration --- even one that
/// implies an indirect call, like a class's instance method ---
/// provides a concrete specification for exactly how to interact with
/// value.)
///
/// However, that representation is problematic in the presence of
/// generics.  This function could be passed off to any of the following
/// generic functions:
///   func foo<T>(f : (T, Int) -> Bool)
///   func bar<U,V>(f : (U, V) -> Bool)
///   func baz<W>(f : (Int, Int) -> W)
///
/// These generic functions all need to be able to call 'f'.  But in
/// Swift's implementation model, these functions don't have to be
/// instantiated for different parameter types, which means that (e.g.)
/// the same 'baz' implementation needs to also be able to work when
/// W=String.  But the optimal way to pass an Int to a function might
/// well be different from the optimal way to pass a String.
///
/// And this runs in both directions: a generic function might return
/// a function that the caller would like to use as an (Int,Int)->Bool:
///   func getFalseFunction<T>() -> (T,T)->Bool
///
/// There are three ways we can deal with this:
///
/// 1. Give all types in Swift a common representation.  The generic
/// implementation can work with both W=String and W=Int because
/// both of those types have the same (direct) storage representation.
/// That's pretty clearly not an acceptable sacrifice.
///
/// 2. Adopt a most-general representation of function types that is
/// used for opaque values; for example, all parameters and results
/// could be passed indirectly.  Concrete values must be coerced to
/// this representation when made abstract.  Unfortunately, there
/// are a lot of obvious situations where this is sub-optimal:
/// for example, in totally non-generic code that just passes around
/// a value of type (Int,Int)->Bool.
///
/// 3. Permit the representation of values to vary by abstraction.
/// Values require coercion when changing abstraction patterns.
/// For example, the argument to 'bar' would be expected to return
/// its Bool result directly but take the T and U parameters indirectly.
/// When '<' is passed to this, what must actually be passed is a
/// thunk that loads both indirect parameters before calling '<'.
///
/// There is one major risk with (3): naively implemented, a single
/// function value which undergoes many coercions could build up a
/// linear number of re-abstraction thunks.  However, this can be
/// solved dynamically by applying thunks with a runtime function that
/// can recognize and bypass its own previous handiwork.
///
/// In general, abstraction patterns are derived from some explicit
/// type expression, such as the written type of a variable or
/// parameter.  This works whenever the expression directly provides
/// structure for the type in question; for example, when the original
/// type is (T,Int)->Bool and we are working with an (Int,Int)->Bool
/// substitution.  However, it is inadequate when the expression does
/// not provide structure at the appropriate level, i.e. when that
/// level is substituted in: when the original type is merely T.  In
/// these cases, we must devolve to a representation which all legal
/// substitutors will agree upon.
///
/// The most general type of a function type replaces all parameters and the
/// result with fresh, unrestricted generic parameters.
///
/// That is, if we have a substituted function type:
///
///   (UnicodeScalar, (Int, Float), Double) -> (Bool, String)
///
/// then its most general form is
///
///   (A, B, C) -> D
///
/// because there is a valid substitution
///   A := UnicodeScalar
///   B := (Int, Float)
///   C := Double
///   D := (Bool, String)
///
class AbstractionPattern {
  enum class Kind {
    /// A type reference.  OrigType is valid.
    Type,
    /// An invalid pattern.
    Invalid,
    /// A completely opaque abstraction pattern.
    Opaque,
    /// An open-coded tuple pattern.  OrigTupleElements is valid.
    /// OtherData is the number of tuple elements.
    Tuple,
    /// A discarded value. OrigType is valid.
    Discard,
    /// A type reference with a Clang type.  OrigType and ClangType are valid.
    ClangType,
    /// The curried imported type of an Objective-C method (that is,
    /// 'Self -> Input -> Result').  OrigType is valid and is a function
    /// type.  ObjCMethod is valid.  OtherData is an encoded foreign
    /// error index.
    CurriedObjCMethodType,
    /// The partially-applied curried imported type of an Objective-C
    /// method (that is, 'Input -> Result').  OrigType is valid and is a
    /// function type.  ObjCMethod is valid.  OtherData is an encoded
    /// foreign error index.
    PartialCurriedObjCMethodType,
    /// The uncurried imported type of a C function imported as a method.
    /// OrigType is valid and is a function type. ClangType is valid and is
    /// a function type. OtherData is an encoded ImportAsMemberStatus.
    CFunctionAsMethodType,
    /// The curried imported type of a C function imported as a method.
    /// OrigType is valid and is a function type. ClangType is valid and is
    /// a function type. OtherData is an encoded ImportAsMemberStatus.
    CurriedCFunctionAsMethodType,
    /// The partially-applied curried imported type of a C function imported as
    /// a method.
    /// OrigType is valid and is a function type. ClangType is valid and is
    /// a function type. OtherData is an encoded ImportAsMemberStatus.
    PartialCurriedCFunctionAsMethodType,
    /// The uncurried imported type of an Objective-C method (that is,
    /// '(Input, Self) -> Result').  OrigType is valid and is a function
    /// type.  ObjCMethod is valid.  OtherData is an encoded foreign
    /// error index.
    ObjCMethodType,
    /// The uncurried imported type of a C++ method. OrigType is valid and is a
    /// function type. CXXMethod is valid.
    CXXMethodType,
    /// The curried imported type of a C++ method. OrigType is valid and is a
    /// function type. CXXMethod is valid.
    CurriedCXXMethodType,
    /// The partially-applied curried imported type of a C++ method. OrigType is
    /// valid and is a function type. CXXMethod is valid.
    PartialCurriedCXXMethodType,
  };

  class EncodedForeignErrorInfo {
    unsigned Value;

  public:
    EncodedForeignErrorInfo() : Value(0) {}
    EncodedForeignErrorInfo(unsigned errorParameterIndex,
                            bool replaceParamWithVoid,
                            bool stripsResultOptionality)
      : Value(1 +
              (unsigned(stripsResultOptionality)) +
              (unsigned(replaceParamWithVoid) << 1) +
              (errorParameterIndex << 2)) {}

    static EncodedForeignErrorInfo
    encode(const Optional<ForeignErrorConvention> &foreignError);

    bool hasValue() const { return Value != 0; }
    bool hasErrorParameter() const { return hasValue(); }
    bool hasUnreplacedErrorParameter() const {
      return hasValue() && !isErrorParameterReplacedWithVoid();
    }

    bool stripsResultOptionality() const {
      assert(hasValue());
      return (Value - 1) & 1;
    }

    bool isErrorParameterReplacedWithVoid() const {
      assert(hasValue());
      return (Value - 1) & 2;
    }

    unsigned getErrorParameterIndex() const {
      assert(hasValue());
      return (Value - 1) >> 2;
    }

    unsigned getOpaqueValue() const { return Value; }
    static EncodedForeignErrorInfo fromOpaqueValue(unsigned value) {
      EncodedForeignErrorInfo result;
      result.Value = value;
      return result;
    }
  };

  static constexpr const unsigned NumOtherDataBits = 28;
  static constexpr const unsigned MaxOtherData = (1 << NumOtherDataBits) - 1;

  unsigned TheKind : 32 - NumOtherDataBits;
  unsigned OtherData : NumOtherDataBits;
  CanType OrigType;
  union {
    const clang::Type *ClangType;
    const clang::ObjCMethodDecl *ObjCMethod;
    const clang::CXXMethodDecl *CXXMethod;
    const AbstractionPattern *OrigTupleElements;
  };
  CanGenericSignature GenericSig;

  Kind getKind() const { return Kind(TheKind); }

  CanGenericSignature getGenericSignatureForFunctionComponent() const {
    if (auto genericFn = dyn_cast<GenericFunctionType>(getType())) {
      return genericFn.getGenericSignature();
    } else {
      return getGenericSignature();
    }
  }

  unsigned getNumTupleElements_Stored() const {
    assert(getKind() == Kind::Tuple);
    return OtherData;
  }

  bool hasStoredClangType() const {
    switch (getKind()) {
    case Kind::ClangType:
    case Kind::CFunctionAsMethodType:
    case Kind::CurriedCFunctionAsMethodType:
    case Kind::PartialCurriedCFunctionAsMethodType:
      return true;

    default:
      return false;
    }
  }

  bool hasStoredCXXMethod() const {
    switch (getKind()) {
    case Kind::CXXMethodType:
    case Kind::CurriedCXXMethodType:
    case Kind::PartialCurriedCXXMethodType:
      return true;

    default:
      return false;
    }
  }

  bool hasStoredObjCMethod() const {
    switch (getKind()) {
    case Kind::CurriedObjCMethodType:
    case Kind::PartialCurriedObjCMethodType:
    case Kind::ObjCMethodType:
      return true;

    default:
      return false;
    }
  }

  bool hasStoredForeignErrorInfo() const {
    return hasStoredObjCMethod();
  }

  bool hasImportAsMemberStatus() const {
    switch (getKind()) {
    case Kind::CFunctionAsMethodType:
    case Kind::CurriedCFunctionAsMethodType:
    case Kind::PartialCurriedCFunctionAsMethodType:
      return true;

    default:
      return false;
    }
  }

  void initSwiftType(CanGenericSignature signature, CanType origType,
                     Kind kind = Kind::Type) {
    assert(signature || !origType->hasTypeParameter());
    TheKind = unsigned(kind);
    OrigType = origType;
    GenericSig = CanGenericSignature();
    if (OrigType->hasTypeParameter())
      GenericSig = signature;
  }

  void initClangType(CanGenericSignature signature,
                     CanType origType, const clang::Type *clangType,
                     Kind kind = Kind::ClangType) {
    initSwiftType(signature, origType, kind);
    ClangType = clangType;
  }

  void initObjCMethod(CanGenericSignature signature,
                      CanType origType, const clang::ObjCMethodDecl *method,
                      Kind kind, EncodedForeignErrorInfo errorInfo) {
    initSwiftType(signature, origType, kind);
    ObjCMethod = method;
    OtherData = errorInfo.getOpaqueValue();
  }

  void initCFunctionAsMethod(CanGenericSignature signature,
                             CanType origType, const clang::Type *clangType,
                             Kind kind,
                             ImportAsMemberStatus memberStatus) {
    initClangType(signature, origType, clangType, kind);
    OtherData = memberStatus.getRawValue();
  }

  void initCXXMethod(CanGenericSignature signature, CanType origType,
                     const clang::CXXMethodDecl *method, Kind kind) {
    initSwiftType(signature, origType, kind);
    CXXMethod = method;
  }

  AbstractionPattern() {}
  explicit AbstractionPattern(Kind kind) : TheKind(unsigned(kind)) {}

public:
  explicit AbstractionPattern(Type origType)
    : AbstractionPattern(origType->getCanonicalType()) {}
  explicit AbstractionPattern(CanType origType)
    : AbstractionPattern(nullptr, origType) {}
  explicit AbstractionPattern(CanGenericSignature signature, CanType origType) {
    initSwiftType(signature, origType);
  }
  explicit AbstractionPattern(CanType origType, const clang::Type *clangType)
    : AbstractionPattern(nullptr, origType, clangType) {}
  explicit AbstractionPattern(CanGenericSignature signature, CanType origType,
                              const clang::Type *clangType) {
    initClangType(signature, origType, clangType);
  }

  static AbstractionPattern getOpaque() {
    return AbstractionPattern(Kind::Opaque);
  }

  static AbstractionPattern getInvalid() {
    return AbstractionPattern(Kind::Invalid);
  }

  bool hasGenericSignature() const {
    switch (getKind()) {
    case Kind::Type:
    case Kind::Discard:
    case Kind::ClangType:
    case Kind::CFunctionAsMethodType:
    case Kind::CurriedCFunctionAsMethodType:
    case Kind::PartialCurriedCFunctionAsMethodType:
    case Kind::CurriedObjCMethodType:
    case Kind::PartialCurriedObjCMethodType:
    case Kind::ObjCMethodType:
    case Kind::CXXMethodType:
    case Kind::CurriedCXXMethodType:
    case Kind::PartialCurriedCXXMethodType:
      return true;
    case Kind::Invalid:
    case Kind::Opaque:
    case Kind::Tuple:
      return false;
    }
    llvm_unreachable("Unhandled AbstractionPatternKind in switch");
  }

  CanGenericSignature getGenericSignature() const {
    assert(hasGenericSignature());
    return CanGenericSignature(GenericSig);
  }

  CanGenericSignature getGenericSignatureOrNull() const {
    if (!hasGenericSignature())
      return CanGenericSignature();
    return CanGenericSignature(GenericSig);
  }

  /// Return an open-coded abstraction pattern for a tuple.  The
  /// caller is responsible for ensuring that the storage for the
  /// tuple elements is valid for as long as the abstraction pattern is.
  static AbstractionPattern getTuple(ArrayRef<AbstractionPattern> tuple) {
    AbstractionPattern pattern(Kind::Tuple);
    pattern.OtherData = tuple.size();
    pattern.OrigTupleElements = tuple.data();
    return pattern;
  }

public:
  /// Return an abstraction pattern for the curried type of an
  /// Objective-C method.
  static AbstractionPattern
  getCurriedObjCMethod(CanType origType, const clang::ObjCMethodDecl *method,
                       const Optional<ForeignErrorConvention> &foreignError);

  /// Return an abstraction pattern for the uncurried type of a C function
  /// imported as a method.
  ///
  /// For example, if the original function is:
  ///   void CCRefrigatorSetTemperature(CCRefrigeratorRef fridge,
  ///                                   CCRefrigeratorCompartment compartment,
  ///                                   CCTemperature temperature);
  /// then the uncurried type is:
  ///   ((CCRefrigeratorComponent, CCTemperature), CCRefrigerator) -> ()
  static AbstractionPattern
  getCFunctionAsMethod(CanType origType, const clang::Type *clangType,
                       ImportAsMemberStatus memberStatus) {
    assert(isa<AnyFunctionType>(origType));
    AbstractionPattern pattern;
    pattern.initCFunctionAsMethod(nullptr, origType, clangType,
                                  Kind::CFunctionAsMethodType,
                                  memberStatus);
    return pattern;
  }

  /// Return an abstraction pattern for the curried type of a
  /// C function imported as a method.
  ///
  /// For example, if the original function is:
  ///   void CCRefrigatorSetTemperature(CCRefrigeratorRef fridge,
  ///                                   CCRefrigeratorCompartment compartment,
  ///                                   CCTemperature temperature);
  /// then the curried type is:
  ///   (CCRefrigerator) -> (CCRefrigeratorCompartment, CCTemperature) -> ()
  static AbstractionPattern
  getCurriedCFunctionAsMethod(CanType origType,
                              const AbstractFunctionDecl *function);

  /// Return an abstraction pattern for the curried type of a C++ method.
  static AbstractionPattern
  getCurriedCXXMethod(CanType origType, const AbstractFunctionDecl *function);

  /// Return an abstraction pattern for the uncurried type of a C++ method.
  ///
  /// For example, if the original function is:
  ///   void Refrigerator::SetTemperature(RefrigeratorCompartment compartment,
  ///                                     Temperature temperature);
  /// then the uncurried type is:
  ///   ((RefrigeratorCompartment, Temperature), Refrigerator) -> ()
  static AbstractionPattern getCXXMethod(CanType origType,
                                         const clang::CXXMethodDecl *method) {
    assert(isa<AnyFunctionType>(origType));
    AbstractionPattern pattern;
    pattern.initCXXMethod(nullptr, origType, method, Kind::CXXMethodType);
    return pattern;
  }

  /// Return an abstraction pattern for the curried type of a C++ method.
  ///
  /// For example, if the original function is:
  ///   void Refrigerator::SetTemperature(RefrigeratorCompartment compartment,
  ///                                     Temperature temperature);
  /// then the curried type:
  ///   (Refrigerator) -> (Compartment, Temperature) -> ()
  static AbstractionPattern
  getCurriedCXXMethod(CanType origType, const clang::CXXMethodDecl *method) {
    assert(isa<AnyFunctionType>(origType));
    AbstractionPattern pattern;
    pattern.initCXXMethod(nullptr, origType, method,
                          Kind::CurriedCXXMethodType);
    return pattern;
  }

  /// For a C-function-as-method pattern,
  /// get the index of the C function parameter that was imported as the
  /// `self` parameter of the imported method, or None if this is a static
  /// method with no `self` parameter.
  ImportAsMemberStatus getImportAsMemberStatus() const {
    assert(hasImportAsMemberStatus());
    return ImportAsMemberStatus(OtherData);
  }

  /// Return an abstraction pattern for a value that is discarded after being
  /// evaluated.
  static AbstractionPattern
  getDiscard(CanGenericSignature signature, CanType origType) {
    AbstractionPattern pattern;
    pattern.initSwiftType(signature, origType, Kind::Discard);
    return pattern;
  }

  /// Return an abstraction pattern for the type of the given struct field or enum case
  /// substituted in `this` type.
  ///
  /// Note that, for most purposes, you should lower a field's type against its
  /// *unsubstituted* interface type.
  AbstractionPattern
  unsafeGetSubstFieldType(ValueDecl *member,
                          CanType origMemberType = CanType()) const;

private:
  /// Return an abstraction pattern for the curried type of an
  /// Objective-C method.
  static AbstractionPattern
  getCurriedObjCMethod(CanType origType, const clang::ObjCMethodDecl *method,
                       EncodedForeignErrorInfo errorInfo) {
    assert(isa<AnyFunctionType>(origType));
    AbstractionPattern pattern;
    pattern.initObjCMethod(nullptr, origType, method,
                           Kind::CurriedObjCMethodType, errorInfo);
    return pattern;
  }

  static AbstractionPattern
  getCurriedCFunctionAsMethod(CanType origType,
                              const clang::Type *clangType,
                              ImportAsMemberStatus memberStatus) {
    assert(isa<AnyFunctionType>(origType));
    AbstractionPattern pattern;
    pattern.initCFunctionAsMethod(nullptr, origType, clangType,
                                  Kind::CurriedCFunctionAsMethodType,
                                  memberStatus);
    return pattern;
  }

  /// Return an abstraction pattern for the partially-applied curried
  /// type of an Objective-C method.
  static AbstractionPattern
  getPartialCurriedObjCMethod(CanGenericSignature signature,
                              CanType origType,
                              const clang::ObjCMethodDecl *method,
                              EncodedForeignErrorInfo errorInfo) {
    assert(isa<AnyFunctionType>(origType));
    AbstractionPattern pattern;
    pattern.initObjCMethod(signature, origType, method,
                           Kind::PartialCurriedObjCMethodType, errorInfo);
    return pattern;
  }

  /// Return an abstraction pattern for the partially-applied curried
  /// type of a C function imported as a method.
  ///
  /// For example, if the original function is:
  ///   CCRefrigatorSetTemperature(CCRefrigeratorRef, CCTemperature)
  /// then the curried type is:
  ///   (CCRefrigerator) -> (CCTemperature) -> ()
  /// and the partially-applied curried type is:
  ///   (CCTemperature) -> ()
  static AbstractionPattern
  getPartialCurriedCFunctionAsMethod(CanGenericSignature signature,
                                     CanType origType,
                                     const clang::Type *clangType,
                                     ImportAsMemberStatus memberStatus) {
    assert(isa<AnyFunctionType>(origType));
    AbstractionPattern pattern;
    pattern.initCFunctionAsMethod(signature, origType, clangType,
                                  Kind::PartialCurriedCFunctionAsMethodType,
                                  memberStatus);
    return pattern;
  }

  /// Return an abstraction pattern for the partially-applied curried
  /// type of an C++ method.
  ///
  /// For example, if the original function is:
  ///   void Refrigerator::SetTemperature(RefrigeratorCompartment compartment,
  ///                                     Temperature temperature);
  /// then the partially-applied curried type is:
  ///   (Compartment, Temperature) -> ()
  static AbstractionPattern
  getPartialCurriedCXXMethod(CanGenericSignature signature, CanType origType,
                             const clang::CXXMethodDecl *method) {
    assert(isa<AnyFunctionType>(origType));
    AbstractionPattern pattern;
    pattern.initCXXMethod(signature, origType, method,
                          Kind::PartialCurriedCXXMethodType);
    return pattern;
  }

public:
  /// Return an abstraction pattern for the type of an Objective-C method.
  static AbstractionPattern
  getObjCMethod(CanType origType, const clang::ObjCMethodDecl *method,
                const Optional<ForeignErrorConvention> &foreignError);

private:
  /// Return an abstraction pattern for the uncurried type of an
  /// Objective-C method.
  static AbstractionPattern
  getObjCMethod(CanType origType, const clang::ObjCMethodDecl *method,
                EncodedForeignErrorInfo errorInfo) {
    assert(isa<AnyFunctionType>(origType));
    AbstractionPattern pattern;
    pattern.initObjCMethod(nullptr, origType, method, Kind::ObjCMethodType,
                           errorInfo);
    return pattern;
  }

  /// Return a pattern corresponding to the 'self' parameter of the
  /// current Objective-C method.
  AbstractionPattern getObjCMethodSelfPattern(CanType paramType) const;

  /// Return a pattern corresponding to the 'self' parameter of the
  /// current C function imported as a method.
  AbstractionPattern getCFunctionAsMethodSelfPattern(CanType paramType) const;

  /// Return a pattern corresponding to the 'self' parameter of the
  /// current C++ method.
  AbstractionPattern getCXXMethodSelfPattern(CanType paramType) const;

public:
  /// Return an abstraction pattern with an added level of optionality.
  ///
  /// The based abstraction pattern must be either opaque or based on
  /// a Clang or Swift type.  That is, it cannot be a tuple or an ObjC
  /// method type.
  static AbstractionPattern getOptional(AbstractionPattern objectPattern);

  /// Does this abstraction pattern have something that can be used as a key?
  bool hasCachingKey() const {
    // Only the simplest Kind::Type pattern has a caching key; we
    // don't want to try to unique by Clang node.
    return getKind() == Kind::Type || getKind() == Kind::Opaque
        || getKind() == Kind::Discard;
  }
  using CachingKey = CanType;
  CachingKey getCachingKey() const {
    assert(hasCachingKey());
    return OrigType;
  }

  bool isValid() const {
    return getKind() != Kind::Invalid;
  }

  bool isTypeParameterOrOpaqueArchetype() const {
    switch (getKind()) {
    case Kind::Opaque:
      return true;
    case Kind::Type:
    case Kind::ClangType:
    case Kind::Discard: {
      auto type = getType();
      if (isa<DependentMemberType>(type) ||
          isa<GenericTypeParamType>(type)) {
        return true;
      }
      if (auto archetype = dyn_cast<ArchetypeType>(type)) {
        return true;
      }
      return false;
    }
    default:
      return false;
    }
  }

  bool isTypeParameter() const {
    switch (getKind()) {
    case Kind::Opaque:
      return true;
    case Kind::Type:
    case Kind::ClangType:
    case Kind::Discard: {
      auto type = getType();
      if (isa<DependentMemberType>(type) ||
          isa<GenericTypeParamType>(type)) {
        return true;
      }
      if (auto archetype = dyn_cast<ArchetypeType>(type)) {
        return !isa<OpaqueTypeArchetypeType>(archetype->getRoot());
      }
      return false;
    }
    default:
      return false;
    }
  }

  /// Is this an interface type that is subject to a concrete
  /// same-type constraint?
  bool isConcreteType() const;

  bool requiresClass() const;
  LayoutConstraint getLayoutConstraint() const;

  /// Return the Swift type which provides structure for this
  /// abstraction pattern.
  ///
  /// This is always valid unless the pattern is opaque or an
  /// open-coded tuple.  However, it does not always fully describe
  /// the abstraction pattern.
  CanType getType() const {
    switch (getKind()) {
    case Kind::Invalid:
      llvm_unreachable("querying invalid abstraction pattern!");
    case Kind::Opaque:
      llvm_unreachable("opaque pattern has no type");
    case Kind::Tuple:
      llvm_unreachable("open-coded tuple pattern has no type");
    case Kind::ClangType:
    case Kind::CurriedObjCMethodType:
    case Kind::PartialCurriedObjCMethodType:
    case Kind::ObjCMethodType:
    case Kind::CFunctionAsMethodType:
    case Kind::CurriedCFunctionAsMethodType:
    case Kind::PartialCurriedCFunctionAsMethodType:
    case Kind::CXXMethodType:
    case Kind::CurriedCXXMethodType:
    case Kind::PartialCurriedCXXMethodType:
    case Kind::Type:
    case Kind::Discard:
      return OrigType;
    }
    llvm_unreachable("bad kind");
  }

  /// Do the two given types have the same basic type structure as
  /// far as abstraction patterns are concerned?
  ///
  /// Type structure means tuples, functions, and optionals should
  /// appear in the same positions.
  static bool hasSameBasicTypeStructure(CanType l, CanType r);

  /// Rewrite the type of this abstraction pattern without otherwise
  /// changing its structure.  It is only valid to do this on a pattern
  /// that already stores a type, and the new type must have the same
  /// basic type structure as the old one.
  void rewriteType(CanGenericSignature signature, CanType type) {
    switch (getKind()) {
    case Kind::Invalid:
    case Kind::Opaque:
    case Kind::Tuple:
      llvm_unreachable("type cannot be replaced on pattern without type");
    case Kind::ClangType:
    case Kind::CurriedObjCMethodType:
    case Kind::PartialCurriedObjCMethodType:
    case Kind::ObjCMethodType:
    case Kind::CFunctionAsMethodType:
    case Kind::CurriedCFunctionAsMethodType:
    case Kind::PartialCurriedCFunctionAsMethodType:
    case Kind::CXXMethodType:
    case Kind::CurriedCXXMethodType:
    case Kind::PartialCurriedCXXMethodType:
    case Kind::Type:
    case Kind::Discard:
      assert(signature || !type->hasTypeParameter());
      assert(hasSameBasicTypeStructure(OrigType, type));
      GenericSig = (type->hasTypeParameter() ? signature : nullptr);
      OrigType = type;
      return;
    }
    llvm_unreachable("bad kind");
  }

  /// Return whether this abstraction pattern contains foreign type
  /// information.
  ///
  /// In general, after eliminating tuples, a foreign abstraction
  /// pattern will satisfy either isClangType() or isObjCMethod().
  bool isForeign() const {
    switch (getKind()) {
    case Kind::Invalid:
      llvm_unreachable("querying invalid abstraction pattern!");
    case Kind::Opaque:
    case Kind::Tuple:
    case Kind::Type:
    case Kind::Discard:
      return false;
    case Kind::ClangType:
    case Kind::PartialCurriedObjCMethodType:
    case Kind::CurriedObjCMethodType:
    case Kind::ObjCMethodType:
    case Kind::CFunctionAsMethodType:
    case Kind::CurriedCFunctionAsMethodType:
    case Kind::PartialCurriedCFunctionAsMethodType:
    case Kind::CXXMethodType:
    case Kind::CurriedCXXMethodType:
    case Kind::PartialCurriedCXXMethodType:
      return true;
    }
    llvm_unreachable("bad kind");
  }

  /// True if the value is discarded.
  bool isDiscarded() const {
    return getKind() == Kind::Discard;
  }

  /// Return whether this abstraction pattern represents a Clang type.
  /// If so, it is legal to return getClangType().
  bool isClangType() const {
    return (getKind() == Kind::ClangType);
  }

  const clang::Type *getClangType() const {
    assert(hasStoredClangType());
    return ClangType;
  }

  /// Return whether this abstraction pattern represents an
  /// Objective-C method.  If so, it is legal to call getObjCMethod().
  bool isObjCMethod() const {
    return (getKind() == Kind::ObjCMethodType ||
            getKind() == Kind::CurriedObjCMethodType);
  }

  const clang::ObjCMethodDecl *getObjCMethod() const {
    assert(hasStoredObjCMethod());
    return ObjCMethod;
  }

  /// Return whether this abstraction pattern represents a C++ method.
  /// If so, it is legal to call getCXXMethod().
  bool isCXXMethod() const {
    return (getKind() == Kind::CXXMethodType ||
            getKind() == Kind::CurriedCXXMethodType);
  }

  const clang::CXXMethodDecl *getCXXMethod() const {
    assert(hasStoredCXXMethod());
    return CXXMethod;
  }

  EncodedForeignErrorInfo getEncodedForeignErrorInfo() const {
    assert(hasStoredForeignErrorInfo());
    return EncodedForeignErrorInfo::fromOpaqueValue(OtherData);
  }

  bool hasForeignErrorStrippingResultOptionality() const {
    switch (getKind()) {
    case Kind::Invalid:
      llvm_unreachable("querying invalid abstraction pattern!");
    case Kind::Tuple:
      llvm_unreachable("querying foreign-error bits on non-function pattern");

    case Kind::Opaque:
    case Kind::ClangType:
    case Kind::Type:
    case Kind::Discard:
    case Kind::CFunctionAsMethodType:
    case Kind::CurriedCFunctionAsMethodType:
    case Kind::PartialCurriedCFunctionAsMethodType:
    case Kind::CXXMethodType:
    case Kind::CurriedCXXMethodType:
    case Kind::PartialCurriedCXXMethodType:
      return false;
    case Kind::PartialCurriedObjCMethodType:
    case Kind::CurriedObjCMethodType:
    case Kind::ObjCMethodType: {
      auto errorInfo = getEncodedForeignErrorInfo();
      return (errorInfo.hasValue() && errorInfo.stripsResultOptionality());
    }
    }
    llvm_unreachable("bad kind");
  }

  template<typename TYPE>
  typename CanTypeWrapperTraits<TYPE>::type
  getAs() const {
    switch (getKind()) {
    case Kind::Invalid:
      llvm_unreachable("querying invalid abstraction pattern!");
    case Kind::Opaque:
      return typename CanTypeWrapperTraits<TYPE>::type();
    case Kind::Tuple:
      return typename CanTypeWrapperTraits<TYPE>::type();
    case Kind::ClangType:
    case Kind::PartialCurriedObjCMethodType:
    case Kind::CurriedObjCMethodType:
    case Kind::ObjCMethodType:
    case Kind::CFunctionAsMethodType:
    case Kind::CurriedCFunctionAsMethodType:
    case Kind::PartialCurriedCFunctionAsMethodType:
    case Kind::CXXMethodType:
    case Kind::CurriedCXXMethodType:
    case Kind::PartialCurriedCXXMethodType:
    case Kind::Type:
    case Kind::Discard:
      return dyn_cast<TYPE>(getType());
    }
    llvm_unreachable("bad kind");
  }

  /// Is this pattern the exact given type?
  ///
  /// This is only useful for avoiding redundant work at compile time;
  /// code should be prepared to do the right thing in the face of a slight
  /// mismatch.  This may happen for any number of reasons.
  bool isExactType(CanType type) const {
    switch (getKind()) {
    case Kind::Invalid:
      llvm_unreachable("querying invalid abstraction pattern!");
    case Kind::Opaque:
    case Kind::Tuple:
    case Kind::ClangType:
    case Kind::PartialCurriedObjCMethodType:
    case Kind::CurriedObjCMethodType:
    case Kind::ObjCMethodType:
    case Kind::CFunctionAsMethodType:
    case Kind::CurriedCFunctionAsMethodType:
    case Kind::PartialCurriedCFunctionAsMethodType:
    case Kind::CXXMethodType:
    case Kind::CurriedCXXMethodType:
    case Kind::PartialCurriedCXXMethodType:
      // We assume that the Clang type might provide additional structure.
      return false;
    case Kind::Type:
    case Kind::Discard:
      return getType() == type;
    }
    llvm_unreachable("bad kind");
  }

  /// Is the given tuple type a valid substitution of this abstraction
  /// pattern?
  bool matchesTuple(CanTupleType substType);

  bool isTuple() {
    switch (getKind()) {
    case Kind::Invalid:
      llvm_unreachable("querying invalid abstraction pattern!");
    case Kind::Opaque:
    case Kind::PartialCurriedObjCMethodType:
    case Kind::CurriedObjCMethodType:
    case Kind::CFunctionAsMethodType:
    case Kind::CurriedCFunctionAsMethodType:
    case Kind::PartialCurriedCFunctionAsMethodType:
    case Kind::ObjCMethodType:
    case Kind::CXXMethodType:
    case Kind::CurriedCXXMethodType:
    case Kind::PartialCurriedCXXMethodType:
      return false;
    case Kind::Tuple:
      return true;
    case Kind::Type:
    case Kind::Discard:
    case Kind::ClangType:
      return isa<TupleType>(getType());
    }
    llvm_unreachable("bad kind");
  }

  size_t getNumTupleElements() const {
    switch (getKind()) {
    case Kind::Invalid:
      llvm_unreachable("querying invalid abstraction pattern!");
    case Kind::Opaque:
    case Kind::PartialCurriedObjCMethodType:
    case Kind::CurriedObjCMethodType:
    case Kind::CFunctionAsMethodType:
    case Kind::CurriedCFunctionAsMethodType:
    case Kind::PartialCurriedCFunctionAsMethodType:
    case Kind::ObjCMethodType:
    case Kind::CXXMethodType:
    case Kind::CurriedCXXMethodType:
    case Kind::PartialCurriedCXXMethodType:
      llvm_unreachable("pattern is not a tuple");
    case Kind::Tuple:
      return getNumTupleElements_Stored();
    case Kind::Type:
    case Kind::Discard:
    case Kind::ClangType:
      return cast<TupleType>(getType())->getNumElements();
    }
    llvm_unreachable("bad kind");
  }

  /// Given that the value being abstracted is a tuple type, return
  /// the abstraction pattern for its object type.
  AbstractionPattern getTupleElementType(unsigned index) const;

  /// Given that the value being abstracted is a function, return the
  /// abstraction pattern for its result type.
  AbstractionPattern getFunctionResultType() const;

  /// Given that the value being abstracted is a function type, return
  /// the abstraction pattern for one of its parameter types.
  AbstractionPattern getFunctionParamType(unsigned index) const;

  /// Given that the value being abstracted is a function type, return
  /// the number of parameters.
  unsigned getNumFunctionParams() const;

  /// Given that the value being abstracted is optional, return the
  /// abstraction pattern for its object type.
  AbstractionPattern getOptionalObjectType() const;

  /// If this pattern refers to a reference storage type, look through
  /// it.
  AbstractionPattern getReferenceStorageReferentType() const;

  void dump() const LLVM_ATTRIBUTE_USED;
  void print(raw_ostream &OS) const;
};

inline llvm::raw_ostream &operator<<(llvm::raw_ostream &out,
                                     const AbstractionPattern &pattern) {
  pattern.print(out);
  return out;
}

} // polar::lowering

#endif
