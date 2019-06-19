// This source file is part of the polarphp.org open source project
//
// Copyright (c) 2017 - 2019 polarphp software foundation
// Copyright (c) 2017 - 2019 zzu_softboy <zzu_softboy@163.com>
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://polarphp.org/LICENSE.txt for license information
// See https://polarphp.org/CONTRIBUTORS.txt for the list of polarphp project authors
//
// Created by polarboy on 2019/06/17.

#ifndef POLARPHP_PARSER_PARSED_STMT_SYNTAX_NODE_BUILDERS_H
#define POLARPHP_PARSER_PARSED_STMT_SYNTAX_NODE_BUILDERS_H

#include "polarphp/parser/ParsedRawSyntaxNode.h"
#include "polarphp/parser/ParsedSyntaxNodes.h"
#include "polarphp/parser/SyntaxParsingContext.h"
#include "polarphp/syntax/syntaxnode/StmtSyntaxNodes.h"

namespace polar::parser {

using namespace polar::syntax;
using polar::basic::SmallVector;

class ParsedRawSyntaxRecorder;
class SyntaxParsingContext;

class ParsedConditionElementSyntaxBuilder
{
public:
   using Cursor = ConditionElementSyntax::Cursor;
public:
   explicit ParsedConditionElementSyntaxBuilder(SyntaxParsingContext &context)
      : m_context(context)
   {}

   ParsedConditionElementSyntaxBuilder &useCondition(ParsedSyntax condition);
   ParsedConditionElementSyntaxBuilder &useTrailingComma(ParsedTokenSyntax trailingComma);

   ParsedConditionElementSyntax build();
   ParsedConditionElementSyntax makeDeferred();
private:
   ParsedConditionElementSyntax record();
   void finishLayout(bool deferred);

   SyntaxParsingContext &m_context;
   ParsedRawSyntaxNode m_layout[ConditionElementSyntax::CHILDREN_COUNT];
};

class ParsedContinueStmtSyntaxBuilder
{
public:
   using Cursor = ContinueStmtSyntax::Cursor;
public:
   explicit ParsedContinueStmtSyntaxBuilder(SyntaxParsingContext &context)
      : m_context(context)
   {}

   ParsedContinueStmtSyntaxBuilder &useContinueKeyword(ParsedTokenSyntax continueKeyword);
   ParsedContinueStmtSyntaxBuilder &useLNumberToken(ParsedTokenSyntax numberToken);

   ParsedContinueStmtSyntax build();
   ParsedContinueStmtSyntax makeDeferred();
private:
   ParsedContinueStmtSyntax record();
   void finishLayout(bool deferred);

   SyntaxParsingContext &m_context;
   ParsedRawSyntaxNode m_layout[ContinueStmtSyntax::CHILDREN_COUNT];
};

class ParsedBreakStmtSyntaxBuilder
{
public:
   using Cursor = BreakStmtSyntax::Cursor;
public:
   explicit ParsedBreakStmtSyntaxBuilder(SyntaxParsingContext &context)
      : m_context(context)
   {}

   ParsedBreakStmtSyntaxBuilder &useBreakKeyword(ParsedTokenSyntax breakKeyword);
   ParsedBreakStmtSyntaxBuilder &useLNumberToken(ParsedTokenSyntax numberToken);

   ParsedBreakStmtSyntax build();
   ParsedBreakStmtSyntax makeDeferred();
private:
   ParsedBreakStmtSyntax record();
   void finishLayout(bool deferred);

   SyntaxParsingContext &m_context;
   ParsedRawSyntaxNode m_layout[BreakStmtSyntax::CHILDREN_COUNT];
};

class ParsedFallthroughStmtSyntaxBuilder
{
public:
   using Cursor = FallthroughStmtSyntax::Cursor;
public:
   explicit ParsedFallthroughStmtSyntaxBuilder(SyntaxParsingContext &context)
      : m_context(context)
   {}

   ParsedFallthroughStmtSyntaxBuilder &useFallthroughKeyword(ParsedTokenSyntax fallthroughKeyword);

   ParsedFallthroughStmtSyntax build();
   ParsedFallthroughStmtSyntax makeDeferred();
private:
   ParsedFallthroughStmtSyntax record();
   void finishLayout(bool deferred);

   SyntaxParsingContext &m_context;
   ParsedRawSyntaxNode m_layout[FallthroughStmtSyntax::CHILDREN_COUNT];
};

class ParsedElseIfClauseSyntaxBuilder
{
public:
   using Cursor = ElseIfClauseSyntax::Cursor;
public:
   explicit ParsedElseIfClauseSyntaxBuilder(SyntaxParsingContext &context)
      : m_context(context)
   {}

   ParsedElseIfClauseSyntaxBuilder &useElseIfKeyword(ParsedTokenSyntax elseIfKeyword);
   ParsedElseIfClauseSyntaxBuilder &useLeftParen(ParsedTokenSyntax leftParen);
   ParsedElseIfClauseSyntaxBuilder &useCondition(ParsedSyntax condition);
   ParsedElseIfClauseSyntaxBuilder &useRightParen(ParsedTokenSyntax rightParen);
   ParsedElseIfClauseSyntaxBuilder &useBody(ParsedCodeBlockSyntax body);

   ParsedElseIfClauseSyntax build();
   ParsedElseIfClauseSyntax makeDeferred();
private:
   ParsedElseIfClauseSyntax record();
   void finishLayout(bool deferred);

   SyntaxParsingContext &m_context;
   ParsedRawSyntaxNode m_layout[ElseIfClauseSyntax::CHILDREN_COUNT];
};

class ParsedIfStmtSyntaxBuilder
{
public:
   using Cursor = IfStmtSyntax::Cursor;
public:
   explicit ParsedIfStmtSyntaxBuilder(SyntaxParsingContext &context)
      : m_context(context)
   {}

   ParsedIfStmtSyntaxBuilder &useLabelName(ParsedTokenSyntax labelName);
   ParsedIfStmtSyntaxBuilder &useLabelColon(ParsedTokenSyntax labelColon);
   ParsedIfStmtSyntaxBuilder &useIfKeyword(ParsedTokenSyntax ifKeyword);
   ParsedIfStmtSyntaxBuilder &useLeftParen(ParsedTokenSyntax leftParen);
   ParsedIfStmtSyntaxBuilder &useCondition(ParsedExprSyntax condition);
   ParsedIfStmtSyntaxBuilder &useRightParen(ParsedTokenSyntax rightParen);
   ParsedIfStmtSyntaxBuilder &useBody(ParsedCodeBlockSyntax body);
   ParsedIfStmtSyntaxBuilder &useElseIfClauses(ParsedElseIfListSyntax elseIfClauses);
   ParsedIfStmtSyntaxBuilder &addElseIfClausesMember(ParsedElseIfClauseSyntax elseIfClause);
   ParsedIfStmtSyntaxBuilder &useElseKeyword(ParsedTokenSyntax elseKeyword);
   ParsedIfStmtSyntaxBuilder &useElseBody(ParsedSyntax elseBody);

   ParsedIfStmtSyntax build();
   ParsedIfStmtSyntax makeDeferred();
private:
   ParsedIfStmtSyntax record();
   void finishLayout(bool deferred);

   SyntaxParsingContext &m_context;
   ParsedRawSyntaxNode m_layout[IfStmtSyntax::CHILDREN_COUNT];
   SmallVector<ParsedRawSyntaxNode, 8> m_elseIfClausesMembers;
};

class ParsedWhileStmtSyntaxBuilder
{
public:
   using Cursor = WhileStmtSyntax::Cursor;
public:
   explicit ParsedWhileStmtSyntaxBuilder(SyntaxParsingContext &context)
      : m_context(context)
   {}

   ParsedWhileStmtSyntaxBuilder &useLabelName(ParsedTokenSyntax labelName);
   ParsedWhileStmtSyntaxBuilder &useLabelColon(ParsedTokenSyntax labelColon);
   ParsedWhileStmtSyntaxBuilder &useWhileKeyword(ParsedTokenSyntax whileKeyword);
   ParsedWhileStmtSyntaxBuilder &useLeftParen(ParsedTokenSyntax leftParen);
   ParsedWhileStmtSyntaxBuilder &useConditions(ParsedConditionElementListSyntax conditions);
   ParsedWhileStmtSyntaxBuilder &useRightParen(ParsedTokenSyntax rightParen);
   ParsedWhileStmtSyntaxBuilder &addConditionsMember(ParsedConditionElementSyntax condition);
   ParsedWhileStmtSyntaxBuilder &useBody(ParsedCodeBlockSyntax body);

   ParsedWhileStmtSyntax build();
   ParsedWhileStmtSyntax makeDeferred();
private:
   ParsedWhileStmtSyntax record();
   void finishLayout(bool deferred);

   SyntaxParsingContext &m_context;
   ParsedRawSyntaxNode m_layout[WhileStmtSyntax::CHILDREN_COUNT];
   SmallVector<ParsedRawSyntaxNode, 8> m_conditionsMembers;
};

class ParsedDoWhileStmtSyntaxBuilder
{
public:
   using Cursor = DoWhileStmtSyntax::Cursor;
public:
   explicit ParsedDoWhileStmtSyntaxBuilder(SyntaxParsingContext &context)
      : m_context(context)
   {}

   ParsedDoWhileStmtSyntaxBuilder &useLabelName(ParsedTokenSyntax labelName);
   ParsedDoWhileStmtSyntaxBuilder &useLabelColon(ParsedTokenSyntax labelColon);
   ParsedDoWhileStmtSyntaxBuilder &useDoKeyword(ParsedTokenSyntax doKeyword);
   ParsedDoWhileStmtSyntaxBuilder &useBody(ParsedCodeBlockSyntax body);
   ParsedDoWhileStmtSyntaxBuilder &useWhileKeyword(ParsedTokenSyntax whileKeyword);
   ParsedDoWhileStmtSyntaxBuilder &useLeftParen(ParsedTokenSyntax leftParen);
   ParsedDoWhileStmtSyntaxBuilder &useCondition(ParsedExprSyntax condition);
   ParsedDoWhileStmtSyntaxBuilder &useRightParen(ParsedTokenSyntax rightParen);
   ParsedDoWhileStmtSyntaxBuilder &addConditionsMember(ParsedConditionElementSyntax condition);

   ParsedDoWhileStmtSyntax build();
   ParsedDoWhileStmtSyntax makeDeferred();
private:
   ParsedDoWhileStmtSyntax record();
   void finishLayout(bool deferred);

   SyntaxParsingContext &m_context;
   ParsedRawSyntaxNode m_layout[DoWhileStmtSyntax::CHILDREN_COUNT];
};

class ParsedSwitchDefaultLabelSyntaxBuilder
{
public:
   using Cursor = SwitchDefaultLabelSyntax::Cursor;
public:
   explicit ParsedSwitchDefaultLabelSyntaxBuilder(SyntaxParsingContext &context)
      : m_context(context)
   {}

   ParsedSwitchDefaultLabelSyntaxBuilder &useDefaultKeyword(ParsedTokenSyntax defaultKeyword);
   ParsedSwitchDefaultLabelSyntaxBuilder &useColon(ParsedTokenSyntax colon);

   ParsedSwitchDefaultLabelSyntax build();
   ParsedSwitchDefaultLabelSyntax makeDeferred();
private:
   ParsedSwitchDefaultLabelSyntax record();
   void finishLayout(bool deferred);

   SyntaxParsingContext &m_context;
   ParsedRawSyntaxNode m_layout[SwitchDefaultLabelSyntax::CHILDREN_COUNT];
};

class ParsedSwitchCaseLabelSyntaxBuilder
{
public:
   using Cursor = SwitchCaseLabelSyntax::Cursor;
public:
   explicit ParsedSwitchCaseLabelSyntaxBuilder(SyntaxParsingContext &context)
      : m_context(context)
   {}

   ParsedSwitchCaseLabelSyntaxBuilder &useCaseKeyword(ParsedTokenSyntax caseKeyword);
   ParsedSwitchCaseLabelSyntaxBuilder &useExpr(ParsedExprSyntax expr);
   ParsedSwitchCaseLabelSyntaxBuilder &useColon(ParsedTokenSyntax colon);

   ParsedSwitchCaseLabelSyntax build();
   ParsedSwitchCaseLabelSyntax makeDeferred();
private:
   ParsedSwitchCaseLabelSyntax record();
   void finishLayout(bool deferred);

   SyntaxParsingContext &m_context;
   ParsedRawSyntaxNode m_layout[SwitchCaseLabelSyntax::CHILDREN_COUNT];
};


class ParsedSwitchCaseSyntaxBuilder
{
public:
   using Cursor = SwitchCaseSyntax::Cursor;
public:
   explicit ParsedSwitchCaseSyntaxBuilder(SyntaxParsingContext &context)
      : m_context(context)
   {}

   ParsedSwitchCaseSyntaxBuilder &useLabel(ParsedSyntax label);
   ParsedSwitchCaseSyntaxBuilder &useStatements(ParsedCodeBlockItemListSyntax statements);
   ParsedSwitchCaseSyntaxBuilder &addStatement(ParsedCodeBlockItemSyntax statement);

   ParsedSwitchCaseSyntax build();
   ParsedSwitchCaseSyntax makeDeferred();
private:
   ParsedSwitchCaseSyntax record();
   void finishLayout(bool deferred);

   SyntaxParsingContext &m_context;
   ParsedRawSyntaxNode m_layout[SwitchCaseSyntax::CHILDREN_COUNT];
   SmallVector<ParsedRawSyntaxNode, 8> m_statementsMembers;
};

class ParsedSwitchStmtSyntaxBuilder
{
public:
   using Cursor = SwitchStmtSyntax::Cursor;
public:
   explicit ParsedSwitchStmtSyntaxBuilder(SyntaxParsingContext &context)
      : m_context(context)
   {}

   ParsedSwitchStmtSyntaxBuilder &useLabelName(ParsedTokenSyntax labelName);
   ParsedSwitchStmtSyntaxBuilder &useLabelColon(ParsedTokenSyntax labelColon);
   ParsedSwitchStmtSyntaxBuilder &useSwitchKeyword(ParsedTokenSyntax switchKeyword);
   ParsedSwitchStmtSyntaxBuilder &useLeftParen(ParsedTokenSyntax leftParen);
   ParsedSwitchStmtSyntaxBuilder &useConditionExpr(ParsedExprSyntax conditionExpr);
   ParsedSwitchStmtSyntaxBuilder &useRightParen(ParsedTokenSyntax rightParen);
   ParsedSwitchStmtSyntaxBuilder &useLeftBrace(ParsedTokenSyntax leftBrace);
   ParsedSwitchStmtSyntaxBuilder &useCases(ParsedSwitchCaseListSyntax cases);
   ParsedSwitchStmtSyntaxBuilder &addCase(ParsedSwitchCaseSyntax caseItem);
   ParsedSwitchStmtSyntaxBuilder &useRightBrace(ParsedTokenSyntax rightBrace);

   ParsedSwitchStmtSyntax build();
   ParsedSwitchStmtSyntax makeDeferred();
private:
   ParsedSwitchStmtSyntax record();
   void finishLayout(bool deferred);

   SyntaxParsingContext &m_context;
   ParsedRawSyntaxNode m_layout[SwitchStmtSyntax::CHILDREN_COUNT];
   SmallVector<ParsedRawSyntaxNode, 8> m_casesMembers;
};


} // polar::parser

#endif // POLARPHP_PARSER_PARSED_STMT_SYNTAX_NODE_BUILDERS_H
