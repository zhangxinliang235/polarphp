// This source file is part of the polarphp.org open source project
//
// Copyright (c) 2017 - 2019 polarphp software foundation
// Copyright (c) 2017 - 2019 zzu_softboy <zzu_softboy@163.com>
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://polarphp.org/LICENSE.txt for license information
// See https://polarphp.org/CONTRIBUTORS.txt for the list of polarphp project authors
//
// Created by polarboy on 2019/07/24.

#include "../AbstractParserTestCase.h"

using polar::unittest::AbstractParserTestCase;

class ExprSyntaxNodeTest : public AbstractParserTestCase
{
};

TEST_F(ExprSyntaxNodeTest, testNumberExpr)
{
   {
      // test empty stmt
      std::string source =
            R"(
            123;
            )";
      RefCountPtr<RawSyntax> ast = parseSource(source);
   }
}

TEST_F(ExprSyntaxNodeTest, testMagicConstExpr)
{
   {
      // __LINE__
      std::string source =
            R"(
            __LINE__;
            )";
      RefCountPtr<RawSyntax> ast = parseSource(source);
   }
}
