<?php
// This source file is part of the polarphp.org open source project
//
// Copyright (c) 2017 - 2019 polarphp software foundation
// Copyright (c) 2017 - 2019 zzu_softboy <zzu_softboy@163.com>
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://polarphp.org/LICENSE.txt for license information
// See https://polarphp.org/CONTRIBUTORS.txt for the list of polarphp project authors
//
// Created by polarboy on 2019/11/03.

return array(
   // 0 is 'Token'. Needs to be defined manually
   // 1 is 'Unknown'. Needs to be defined manually
   'Decl' => 2,
   'Expr' => 3,
   'Stmt' => 4,
   'UnknownDecl' => 5,
   'ReservedNonModifier' => 6,
   'SemiReserved' => 7,
   'Identifier' => 8,
   'NamespaceName' => 9,
   'Name' => 10,
   'NameListItem' => 11,
   'InitializerClause' => 12,
   'TypeClause' => 13,
   'TypeExprClause' => 14,
   'ReturnTypeClause' => 15,
   'Parameter' => 16,
   'ParameterListItem' => 17,
   'ParameterClause' => 18,
   'FunctionDefinition' => 19,
   'ClassModifier' => 20,
   'ExtendsFromClause' => 21,
   'ImplementsClause' => 22,
   'InterfaceExtendsClause' => 23,
   'ClassPropertyClause' => 24,
   'ClassPropertyListItem' => 25,
   'ClassConstClause' => 26,
   'ClassConstListItem' => 27,
   'MemberModifier' => 28,
   'ClassPropertyDecl' => 29,
   'ClassConstDecl' => 30,
   'ClassMethodDecl' => 31,
   'ClassTraitMethodReference' => 32,
   'ClassAbsoluteTraitMethodReference' => 33,
   'ClassTraitPrecedence' => 34,
   'ClassTraitAlias' => 35,
   'ClassTraitAdaptation' => 36,
   'ClassTraitAdaptationBlock' => 37,
   'ClassTraitDecl' => 38,
   'MemberDeclListItem' => 39,
   'MemberDeclBlock' => 40,
   'ClassDefinition' => 41,
   'InterfaceDefinition' => 42,
   'TraitDefinition' => 43,
   'SourceFile' => 44,
   'NameList' => 45,
   'ParameterList' => 46,
   'ClassModifierList' => 47,
   'MemberDeclList' => 48,
   'MemberModifierList' => 49,
   'ClassPropertyList' => 50,
   'ClassConstList' => 51,
   'ClassTraitAdaptationList' => 52,
   'UnknownExpr' => 1001,
   'ParenDecoratedExpr' => 1002,
   'NullExpr' => 1003,
   'OptionalExpr' => 1004,
   'ExprListItem' => 1005,
   'VariableExpr' => 1006,
   'ReferencedVariableExpr' => 1007,
   'ClassConstIdentifierExpr' => 1008,
   'ConstExpr' => 1009,
   'NewVariableClause' => 1010,
   'CallableVariableExpr' => 1011,
   'CallableFuncNameClause' => 1012,
   'MemberNameClause' => 1013,
   'PropertyNameClause' => 1014,
   'InstancePropertyExpr' => 1015,
   'StaticPropertyExpr' => 1016,
   'Argument' => 1017,
   'ArgumentListItem' => 1018,
   'ArgumentListClause' => 1019,
   'DereferencableClause' => 1020,
   'VariableClassNameClause' => 1021,
   'ClassNameClause' => 1022,
   'ClassNameRefClause' => 1023,
   'BraceDecoratedExprClause' => 1024,
   'BraceDecoratedVariableExpr' => 1025,
   'ArrayKeyValuePairItem' => 1026,
   'ArrayUnpackPairItem' => 1027,
   'ArrayPair' => 1028,
   'ArrayPairListItem' => 1029,
   'ListRecursivePairItem' => 1030,
   'SimpleVariableExpr' => 1031,
   'ArrayCreateExpr' => 1032,
   'SimplifiedArrayCreateExpr' => 1033,
   'ArrayAccessExpr' => 1034,
   'BraceDecoratedArrayAccessExpr' => 1035,
   'SimpleFunctionCallExpr' => 1036,
   'FunctionCallExpr' => 1037,
   'InstanceMethodCallExpr' => 1038,
   'StaticMethodCallExpr' => 1039,
   'DereferencableScalarExpr' => 1040,
   'AnonymousClassDefinitionClause' => 1041,
   'SimpleInstanceCreateExpr' => 1042,
   'AnonymousInstanceCreateExpr' => 1043,
   'ClassicLambdaExpr' => 1044,
   'SimplifiedLambdaExpr' => 1045,
   'LambdaExpr' => 1046,
   'InstanceCreateExpr' => 1047,
   'ScalarExpr' => 1048,
   'ClassRefParentExpr' => 1049,
   'ClassRefSelfExpr' => 1050,
   'ClassRefStaticExpr' => 1051,
   'IntegerLiteralExpr' => 1052,
   'FloatLiteralExpr' => 1053,
   'StringLiteralExpr' => 1054,
   'BooleanLiteralExpr' => 1055,
   'IssetVariable' => 1056,
   'IssetVariableListItem' => 1057,
   'IssetVariablesClause' => 1058,
   'IssetFuncExpr' => 1059,
   'EmptyFuncExpr' => 1060,
   'IncludeExpr' => 1061,
   'RequireExpr' => 1062,
   'EvalFuncExpr' => 1063,
   'PrintFuncExpr' => 1064,
   'FuncLikeExpr' => 1065,
   'ArrayStructureAssignmentExpr' => 1066,
   'ListStructureClause' => 1067,
   'ListStructureAssignmentExpr' => 1068,
   'AssignmentExpr' => 1069,
   'CompoundAssignmentExpr' => 1070,
   'LogicalExpr' => 1071,
   'BitLogicalExpr' => 1072,
   'RelationExpr' => 1073,
   'CastExpr' => 1074,
   'ExitExprArgClause' => 1075,
   'ExitExpr' => 1076,
   'YieldExpr' => 1077,
   'YieldFromExpr' => 1078,
   'CloneExpr' => 1079,
   'EncapsVariableOffset' => 1080,
   'EncapsArrayVar' => 1081,
   'EncapsObjProp' => 1082,
   'EncapsDollarCurlyExpr' => 1083,
   'EncapsDollarCurlyVar' => 1084,
   'EncapsDollarCurlyArray' => 1085,
   'EncapsCurlyVariable' => 1086,
   'EncapsVariable' => 1087,
   'EncapsListItem' => 1088,
   'BackticksClause' => 1089,
   'HeredocExpr' => 1090,
   'EncapsListStringExpr' => 1091,
   'TernaryExpr' => 1092,
   'SequenceExpr' => 1093,
   'PrefixOperatorExpr' => 1094,
   'PostfixOperatorExpr' => 1095,
   'BinaryOperatorExpr' => 1096,
   'InstanceofExpr' => 1097,
   'ShellCmdExpr' => 1098,
   'UseLexicalVariableClause' => 1099,
   'LexicalVariable' => 1100,
   'LexicalVariableListItem' => 1101,
   'ExprList' => 1102,
   'LexicalVariableList' => 1103,
   'ArrayPairList' => 1104,
   'EncapsItemList' => 1105,
   'ArgumentList' => 1106,
   'IssetVariablesList' => 1107,
   'UnknownStmt' => 2001,
   'EmptyStmt' => 2002,
   'NestStmt' => 2003,
   'ExprStmt' => 2004,
   'InnerStmt' => 2005,
   'InnerCodeBlockStmt' => 2006,
   'TopStmt' => 2007,
   'TopCodeBlockStmt' => 2008,
   'DeclareStmt' => 2009,
   'GotoStmt' => 2010,
   'UnsetVariable' => 2011,
   'UnsetVariableListItem' => 2012,
   'UnsetStmt' => 2013,
   'LabelStmt' => 2014,
   'ConditionElement' => 2015,
   'ContinueStmt' => 2016,
   'BreakStmt' => 2017,
   'FallthroughStmt' => 2018,
   'ElseIfClause' => 2019,
   'IfStmt' => 2020,
   'WhileStmt' => 2021,
   'ForStmt' => 2022,
   'ForeachVariable' => 2023,
   'ForeachStmt' => 2024,
   'SwitchDefaultLabel' => 2025,
   'SwitchCaseLabel' => 2026,
   'SwitchCaseListClause' => 2027,
   'SwitchCase' => 2028,
   'SwitchStmt' => 2029,
   'DeferStmt' => 2030,
   'ThrowStmt' => 2031,
   'TryStmt' => 2032,
   'FinallyClause' => 2033,
   'CatchArgTypeHintItem' => 2034,
   'CatchListItemClause' => 2035,
   'ReturnStmt' => 2036,
   'EchoStmt' => 2037,
   'HaltCompilerStmt' => 2038,
   'GlobalVariable' => 2039,
   'GlobalVariableListItem' => 2040,
   'GlobalVariableDeclarationsStmt' => 2041,
   'StaticVariableDeclare' => 2042,
   'StaticVariableListItem' => 2043,
   'StaticVariableDeclarationsStmt' => 2044,
   'NamespaceUseType' => 2045,
   'NamespaceUnprefixedUseDeclaration' => 2046,
   'NamespaceUnprefixedUseDeclarationListItem' => 2047,
   'NamespaceUseDeclaration' => 2048,
   'NamespaceUseDeclarationListItem' => 2049,
   'NamespaceInlineUseDeclaration' => 2050,
   'NamespaceInlineUseDeclarationListItem' => 2051,
   'NamespaceGroupUseDeclaration' => 2052,
   'NamespaceMixedGroupUseDeclaration' => 2053,
   'NamespaceUseStmt' => 2054,
   'NamespaceDefinitionStmt' => 2055,
   'NamespaceBlockStmt' => 2056,
   'ConstDeclare' => 2057,
   'ConstListItem' => 2058,
   'ConstDefinitionStmt' => 2059,
   'ClassDefinitionStmt' => 2060,
   'InterfaceDefinitionStmt' => 2061,
   'TraitDefinitionStmt' => 2062,
   'FunctionDefinitionStmt' => 2063,
   'ConditionElementList' => 2064,
   'SwitchCaseList' => 2065,
   'ElseIfList' => 2066,
   'InnerStmtList' => 2067,
   'TopStmtList' => 2068,
   'CatchList' => 2069,
   'CatchArgTypeHintList' => 2070,
   'UnsetVariableList' => 2071,
   'GlobalVariableList' => 2072,
   'StaticVariableList' => 2073,
   'NamespaceUseDeclarationList' => 2074,
   'NamespaceInlineUseDeclarationList' => 2075,
   'NamespaceUnprefixedUseDeclarationList' => 2076,
   'ConstDeclareList' => 2077
);