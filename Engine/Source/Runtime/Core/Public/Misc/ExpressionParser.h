// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Templates/ValueOrError.h"
#include "Misc/ExpressionParserTypes.h"

/** An expression parser, responsible for lexing, compiling, and evaluating expressions.
 *	The parser supports 3 functions:
 *		1. Lexing the expression into a set of user defined tokens,
 *		2. Compiling the tokenized expression to an efficient reverse-polish execution order,
 *		3. Evaluating the compiled tokens
 *
 *  See ExpressionParserExamples.cpp for example usage.
 */

namespace ExpressionParser
{
	typedef TValueOrError< TArray<FExpressionToken>, FExpressionError > LexResultType;
	typedef TValueOrError< TArray<FCompiledToken>, FExpressionError > CompileResultType;

	/** Lex the specified string, using the specified grammar */
	CORE_API LexResultType Lex(const TCHAR* InExpression, const FTokenDefinitions& TokenDefinitions);
	
	/** Compile the specified expression into an array of Reverse-Polish order nodes for evaluation, according to our grammar definition */
	CORE_API CompileResultType Compile(const TCHAR* InExpression, const FTokenDefinitions& TokenDefinitions, const FExpressionGrammar& InGrammar);

	/** Compile the specified tokens into an array of Reverse-Polish order nodes for evaluation, according to our grammar definition */
	CORE_API CompileResultType Compile(TArray<FExpressionToken> InTokens, const FExpressionGrammar& InGrammar);

	/** Evaluate the specified expression using the specified token definitions, grammar definition, and evaluation environment */
	CORE_API FExpressionResult Evaluate(const TCHAR* InExpression, const FTokenDefinitions& InTokenDefinitions, const FExpressionGrammar& InGrammar, const IOperatorEvaluationEnvironment& InEnvironment);

	/** Evaluate the specified pre-compiled tokens using an evaluation environment */
	CORE_API FExpressionResult Evaluate(const TArray<FCompiledToken>& CompiledTokens, const IOperatorEvaluationEnvironment& InEnvironment);

	/** Templated versions of evaluation functions used when passing a specific jump table and context */
	template<typename ContextType>
	FExpressionResult Evaluate(const TCHAR* InExpression, const FTokenDefinitions& InTokenDefinitions, const FExpressionGrammar& InGrammar,	const TOperatorJumpTable<ContextType>& InJumpTable, const ContextType* InContext = nullptr)
	{
		TOperatorEvaluationEnvironment<ContextType> Env(InJumpTable, InContext);
		return Evaluate(InExpression, InTokenDefinitions, InGrammar, Env);
	}

	template<typename ContextType>
	FExpressionResult Evaluate(const TArray<FCompiledToken>& CompiledTokens, const TOperatorJumpTable<ContextType>& InJumpTable, const ContextType* InContext = nullptr)
	{
		TOperatorEvaluationEnvironment<ContextType> Env(InJumpTable, InContext);
		return Evaluate(CompiledTokens, Env);
	}
}
