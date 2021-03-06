//===--- PrettyStackTrace.h - Crash trace information -----------*- C++ -*-===//
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
// This file defines RAII classes that give better diagnostic output
// about when, exactly, a crash is occurring.
//
//===----------------------------------------------------------------------===//

#ifndef POLARPHP_AST_PRETTYSTACKTRACE
#define POLARPHP_AST_PRETTYSTACKTRACE

#include "llvm/Support/PrettyStackTrace.h"
#include "polarphp/basic/SourceLoc.h"
#include "polarphp/ast/AnyFunctionRef.h"
#include "polarphp/ast/Identifier.h"
#include "polarphp/ast/Type.h"

namespace polar {
class AstContext;
class Decl;
class Expr;
class GenericSignature;
class Pattern;
class Stmt;
class TypeRepr;

void printSourceLocDescription(llvm::raw_ostream &out, SourceLoc loc,
                               AstContext &Context, bool addNewline = true);

/// PrettyStackTraceLocation - Observe that we are doing some
/// processing starting at a fixed location.
class PrettyStackTraceLocation : public llvm::PrettyStackTraceEntry {
   AstContext &Context;
   SourceLoc Loc;
   const char *Action;
public:
   PrettyStackTraceLocation(AstContext &C, const char *action, SourceLoc loc)
      : Context(C), Loc(loc), Action(action) {}
   virtual void print(llvm::raw_ostream &OS) const;
};

void printDeclDescription(llvm::raw_ostream &out, const Decl *D,
                          AstContext &Context, bool addNewline = true);

/// PrettyStackTraceDecl - Observe that we are processing a specific
/// declaration.
class PrettyStackTraceDecl : public llvm::PrettyStackTraceEntry {
   const Decl *TheDecl;
   const char *Action;
public:
   PrettyStackTraceDecl(const char *action, const Decl *D)
      : TheDecl(D), Action(action) {}
   virtual void print(llvm::raw_ostream &OS) const;
};

/// PrettyStackTraceAnyFunctionRef - Observe that we are processing a specific
/// function or closure literal.
class PrettyStackTraceAnyFunctionRef : public llvm::PrettyStackTraceEntry {
   AnyFunctionRef TheRef;
   const char *Action;
public:
   PrettyStackTraceAnyFunctionRef(const char *action, AnyFunctionRef ref)
      : TheRef(ref), Action(action) {}
   virtual void print(llvm::raw_ostream &OS) const;
};

void printExprDescription(llvm::raw_ostream &out, Expr *E,
                          AstContext &Context, bool addNewline = true);

/// PrettyStackTraceExpr - Observe that we are processing a specific
/// expression.
class PrettyStackTraceExpr : public llvm::PrettyStackTraceEntry {
   AstContext &Context;
   Expr *TheExpr;
   const char *Action;
public:
   PrettyStackTraceExpr(AstContext &C, const char *action, Expr *E)
      : Context(C), TheExpr(E), Action(action) {}
   virtual void print(llvm::raw_ostream &OS) const;
};

void printStmtDescription(llvm::raw_ostream &out, Stmt *S,
                          AstContext &Context, bool addNewline = true);

/// PrettyStackTraceStmt - Observe that we are processing a specific
/// statement.
class PrettyStackTraceStmt : public llvm::PrettyStackTraceEntry {
   AstContext &Context;
   Stmt *TheStmt;
   const char *Action;
public:
   PrettyStackTraceStmt(AstContext &C, const char *action, Stmt *S)
      : Context(C), TheStmt(S), Action(action) {}
   virtual void print(llvm::raw_ostream &OS) const;
};

void printPatternDescription(llvm::raw_ostream &out, Pattern *P,
                             AstContext &Context, bool addNewline = true);

/// PrettyStackTracePattern - Observe that we are processing a
/// specific pattern.
class PrettyStackTracePattern : public llvm::PrettyStackTraceEntry {
   AstContext &Context;
   Pattern *ThePattern;
   const char *Action;
public:
   PrettyStackTracePattern(AstContext &C, const char *action, Pattern *P)
      : Context(C), ThePattern(P), Action(action) {}
   virtual void print(llvm::raw_ostream &OS) const;
};

void printTypeDescription(llvm::raw_ostream &out, Type T,
                          AstContext &Context, bool addNewline = true);

/// PrettyStackTraceType - Observe that we are processing a specific type.
class PrettyStackTraceType : public llvm::PrettyStackTraceEntry {
   AstContext &Context;
   Type TheType;
   const char *Action;
public:
   PrettyStackTraceType(AstContext &C, const char *action, Type type)
      : Context(C), TheType(type), Action(action) {}
   virtual void print(llvm::raw_ostream &OS) const;
};

/// Observe that we are processing a specific type representation.
class PrettyStackTraceTypeRepr : public llvm::PrettyStackTraceEntry {
   AstContext &Context;
   TypeRepr *TheType;
   const char *Action;
public:
   PrettyStackTraceTypeRepr(AstContext &C, const char *action, TypeRepr *type)
      : Context(C), TheType(type), Action(action) {}
   virtual void print(llvm::raw_ostream &OS) const;
};

/// PrettyStackTraceConformance - Observe that we are processing a
/// specific protocol conformance.
class PrettyStackTraceConformance : public llvm::PrettyStackTraceEntry {
   AstContext &Context;
   const InterfaceConformance *Conformance;
   const char *Action;
public:
   PrettyStackTraceConformance(AstContext &C, const char *action,
                               const InterfaceConformance *conformance)
      : Context(C), Conformance(conformance), Action(action) {}
   virtual void print(llvm::raw_ostream &OS) const;
};

void printConformanceDescription(llvm::raw_ostream &out,
                                 const InterfaceConformance *conformance,
                                 AstContext &Context, bool addNewline = true);

class PrettyStackTraceGenericSignature : public llvm::PrettyStackTraceEntry {
   const char *Action;
   GenericSignature GenericSig;
   Optional<unsigned> Requirement;

public:
   PrettyStackTraceGenericSignature(const char *action,
                                    GenericSignature genericSig,
                                    Optional<unsigned> requirement = None)
      : Action(action), GenericSig(genericSig), Requirement(requirement) { }

   void setRequirement(Optional<unsigned> requirement) {
      Requirement = requirement;
   }

   void print(llvm::raw_ostream &out) const override;
};

} // end namespace polar

#endif // POLARPHP_AST_PRETTYSTACKTRACE
