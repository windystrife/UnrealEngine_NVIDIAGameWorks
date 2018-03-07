// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Misc/ExpressionParser.h"
#include "Misc/AutomationTest.h"

#define LOCTEXT_NAMESPACE "ExpressionParser"



FTokenStream::FTokenStream(const TCHAR* In)
	: Start(In)
	, End(Start + FCString::Strlen(Start))
	, ReadPos(In)
{
}

bool FTokenStream::IsReadPosValid(const TCHAR* InPos, int32 MinNumChars) const
{
	return InPos >= Start && InPos <= End - MinNumChars;
}

TCHAR FTokenStream::PeekChar(int32 Offset) const
{
	if (ReadPos + Offset < End)
	{
		return *(ReadPos + Offset);
	}

	return '\0';
}

int32 FTokenStream::CharsRemaining() const
{
	return End - ReadPos;
}

bool FTokenStream::IsEmpty() const
{
	return ReadPos == End;
}

int32 FTokenStream::GetPosition() const
{
	return ReadPos - Start;
}

FString FTokenStream::GetErrorContext() const
{
	const TCHAR* StartPos = ReadPos;
	const TCHAR* EndPos = StartPos;

	// Skip over any leading whitespace
	while (FChar::IsWhitespace(*EndPos))
	{
		EndPos++;
	}

	// Read until next whitespace or end of string
	while (!FChar::IsWhitespace(*EndPos) && *EndPos != '\0')
	{
		EndPos++;
	}

	static const int32 MaxChars = 32;
	FString Context(FMath::Min(int32(EndPos - StartPos), MaxChars), StartPos);
	if (EndPos - StartPos > MaxChars)
	{
		Context += TEXT("...");
	}
	return Context;
}

/** Parse out a token */
TOptional<FStringToken> FTokenStream::ParseToken(TFunctionRef<EParseState(TCHAR)> Pred, FStringToken* Accumulate) const
{
	const TCHAR* OptReadPos = Accumulate ? Accumulate->GetTokenEndPos() : ReadPos;

	if (!IsReadPosValid(OptReadPos))
	{
		return TOptional<FStringToken>();
	}

	FStringToken Token(OptReadPos, 0, OptReadPos - Start);

	while (Token.GetTokenEndPos() != End)
	{
		const EParseState State = Pred(*Token.GetTokenEndPos());
		
		if (State == EParseState::Cancel)
		{
			return TOptional<FStringToken>();
		}
		
		if (State == EParseState::Continue || State == EParseState::StopAfter)
		{
			// Need to include this character in this token
			++Token.TokenEnd;
		}

		if (State == EParseState::StopAfter || State == EParseState::StopBefore)
		{
			// Finished parsing the token
			break;
		}
	}

	if (Token.IsValid())
	{
		if (Accumulate)
		{
			Accumulate->Accumulate(Token);
		}
		return Token;
	}
	return TOptional<FStringToken>();
}

TOptional<FStringToken> FTokenStream::ParseSymbol(FStringToken* Accumulate) const
{
	const TCHAR* OptReadPos = Accumulate ? Accumulate->GetTokenEndPos() : ReadPos;

	if (!IsReadPosValid(OptReadPos))
	{
		return TOptional<FStringToken>();
	}
	
	FStringToken Token(OptReadPos, 0, OptReadPos - Start);
	++Token.TokenEnd;

	if (Accumulate)
	{
		Accumulate->Accumulate(Token);
	}

	return Token;
}

TOptional<FStringToken> FTokenStream::ParseSymbol(TCHAR Symbol, FStringToken* Accumulate) const
{
	const TCHAR* OptReadPos = Accumulate ? Accumulate->GetTokenEndPos() : ReadPos;

	if (!IsReadPosValid(OptReadPos))
	{
		return TOptional<FStringToken>();
	}
	
	FStringToken Token(OptReadPos, 0, OptReadPos - Start);

	if (*Token.TokenEnd == Symbol)
	{
		++Token.TokenEnd;

		if (Accumulate)
		{
			Accumulate->Accumulate(Token);
		}

		return Token;
	}

	return TOptional<FStringToken>();
}

TOptional<FStringToken> FTokenStream::ParseToken(const TCHAR* Symbol, FStringToken* Accumulate) const
{
	const TCHAR* OptReadPos = Accumulate ? Accumulate->GetTokenEndPos() : ReadPos;

	const int32 Len = FCString::Strlen(Symbol);
	if (!IsReadPosValid(OptReadPos, Len))
	{
		return TOptional<FStringToken>();
	}

	if (*OptReadPos != *Symbol)
	{
		return TOptional<FStringToken>();		
	}

	FStringToken Token(OptReadPos, 0, OptReadPos - Start);
	
	if (FCString::Strncmp(Token.GetTokenEndPos(), Symbol, Len) == 0)
	{
		Token.TokenEnd += Len;

		if (Accumulate)
		{
			Accumulate->Accumulate(Token);
		}

		return Token;
	}

	return TOptional<FStringToken>();
}

TOptional<FStringToken> FTokenStream::ParseTokenIgnoreCase(const TCHAR* Symbol, FStringToken* Accumulate) const
{
	const TCHAR* OptReadPos = Accumulate ? Accumulate->GetTokenEndPos() : ReadPos;

	const int32 Len = FCString::Strlen(Symbol);
	if (!IsReadPosValid(OptReadPos, Len))
	{
		return TOptional<FStringToken>();
	}

	FStringToken Token(OptReadPos, 0, OptReadPos - Start);
	
	if (FCString::Strnicmp(OptReadPos, Symbol, Len) == 0)
	{
		Token.TokenEnd += Len;

		if (Accumulate)
		{
			Accumulate->Accumulate(Token);
		}

		return Token;
	}

	return TOptional<FStringToken>();
}

TOptional<FStringToken> FTokenStream::ParseWhitespace(FStringToken* Accumulate) const
{
	const TCHAR* OptReadPos = Accumulate ? Accumulate->GetTokenEndPos() : ReadPos;

	if (IsReadPosValid(OptReadPos))
	{
		return ParseToken([](TCHAR InC){ return FChar::IsWhitespace(InC) ? EParseState::Continue : EParseState::StopBefore; });
	}

	return TOptional<FStringToken>();
}

TOptional<FStringToken> FTokenStream::GenerateToken(int32 NumChars, FStringToken* Accumulate) const
{
	const TCHAR* OptReadPos = Accumulate ? Accumulate->GetTokenEndPos() : ReadPos;

	if (IsReadPosValid(OptReadPos, NumChars))
	{
		FStringToken Token(OptReadPos, 0, OptReadPos - Start);
		Token.TokenEnd += NumChars;
		if (Accumulate)
		{
			Accumulate->Accumulate(Token);
		}
		return Token;
	}

	return TOptional<FStringToken>();	
}

void FTokenStream::SetReadPos(const FStringToken& Token)
{
	if (ensure(IsReadPosValid(Token.TokenEnd, 0)))
	{
		ReadPos = Token.TokenEnd;
	}
}

FExpressionTokenConsumer::FExpressionTokenConsumer(const TCHAR* InExpression)
	: Stream(InExpression)
{}

TArray<FExpressionToken> FExpressionTokenConsumer::Extract()
{
	TArray<FExpressionToken> Swapped;
	Swap(Swapped, Tokens);
	return Swapped;
}

void FExpressionTokenConsumer::Add(const FStringToken& SourceToken, FExpressionNode&& Node)
{
	Stream.SetReadPos(SourceToken);
	Tokens.Emplace(SourceToken, MoveTemp(Node));
}

void FTokenDefinitions::DefineToken(TFunction<FExpressionDefinition>&& Definition)
{
	Definitions.Emplace(MoveTemp(Definition));
}

TOptional<FExpressionError> FTokenDefinitions::ConsumeToken(FExpressionTokenConsumer& Consumer) const
{
	auto& Stream = Consumer.GetStream();
	
	// Skip over whitespace
	if (bIgnoreWhitespace)
	{
		TOptional<FStringToken> Whitespace = Stream.ParseWhitespace();
		if (Whitespace.IsSet())
		{
			Stream.SetReadPos(Whitespace.GetValue());
		}
	}

	if (Stream.IsEmpty())
	{
		// Trailing whitespace in the expression.
		return TOptional<FExpressionError>();
	}

	const auto* Pos = Stream.GetRead();

	// Try each token in turn. First come first served.
	for (const auto& Def : Definitions)
	{
		// Call the token definition
		auto Error = Def(Consumer);
		if (Error.IsSet())
		{
			return Error;
		}
		// If the stream has moved on, the definition added one or more tokens, so 
		else if (Stream.GetRead() != Pos)
		{
			return TOptional<FExpressionError>();
		}
	}

	// No token definition matched the stream at its current position - fatal error
	FFormatOrderedArguments Args;
	Args.Add(FText::FromString(Consumer.GetStream().GetErrorContext()));
	Args.Add(Consumer.GetStream().GetPosition());
	return FExpressionError(FText::Format(LOCTEXT("LexicalError", "Unrecognized token '{0}' at character {1}"), Args));
}

TOptional<FExpressionError> FTokenDefinitions::ConsumeTokens(FExpressionTokenConsumer& Consumer) const
{
	auto& Stream = Consumer.GetStream();
	while(!Stream.IsEmpty())
	{
		auto Error = ConsumeToken(Consumer);
		if (Error.IsSet())
		{
			return Error;
		}
	}

	return TOptional<FExpressionError>();
}

FExpressionNode::~FExpressionNode()
{
	if (auto* Data = GetData())
	{
		Data->~IExpressionNodeStorage();
	}
}

FExpressionNode::FExpressionNode(FExpressionNode&& In)
{
	*this = MoveTemp(In);
}

FExpressionNode& FExpressionNode::operator=(FExpressionNode&& In)
{
	if (TypeId == In.TypeId && TypeId.IsValid())
	{
		// If we have the same types, we can move-assign properly
		In.GetData()->MoveAssign(InlineBytes);
	}
	else
	{
		// Otherwise we have to destroy what we have, and reseat the RHS
		if (auto* ThisData = GetData())
		{
			ThisData->~IExpressionNodeStorage();
		}

		TypeId = In.TypeId;
		if (auto* SrcData = In.GetData())
		{
			SrcData->Reseat(InlineBytes);

			// Empty the RHS
			In.TypeId = FGuid();
			SrcData->~IExpressionNodeStorage();
		}
	}

	return *this;
}

const FGuid& FExpressionNode::GetTypeId() const
{
	return TypeId;
}

Impl::IExpressionNodeStorage* FExpressionNode::GetData()
{
	return TypeId.IsValid() ? reinterpret_cast<Impl::IExpressionNodeStorage*>(InlineBytes) : nullptr;
}

const Impl::IExpressionNodeStorage* FExpressionNode::GetData() const
{
	return TypeId.IsValid() ? reinterpret_cast<const Impl::IExpressionNodeStorage*>(InlineBytes) : nullptr;
}

FExpressionNode FExpressionNode::Copy() const
{
	if (const auto* Data = GetData())
	{
		return Data->Copy();
	}
	return FExpressionNode();
}

const FGuid* FExpressionGrammar::GetGrouping(const FGuid& TypeId) const
{
	return Groupings.Find(TypeId);
}

bool FExpressionGrammar::HasPreUnaryOperator(const FGuid& InTypeId) const
{
	return PreUnaryOperators.Contains(InTypeId);
}

bool FExpressionGrammar::HasPostUnaryOperator(const FGuid& InTypeId) const
{
	return PostUnaryOperators.Contains(InTypeId);
}

const FOpParameters* FExpressionGrammar::GetBinaryOperatorDefParameters(const FGuid& InTypeId) const
{
	return BinaryOperators.Find(InTypeId);
}

struct FExpressionCompiler
{
	FExpressionCompiler(const FExpressionGrammar& InGrammar, TArray<FExpressionToken>& InTokens)
		: Grammar(InGrammar), Tokens(InTokens)
	{
		CurrentTokenIndex = 0;
		Commands.Reserve(Tokens.Num());
	}

	TValueOrError<TArray<FCompiledToken>, FExpressionError> Compile()
	{
		auto Error = CompileGroup(nullptr, nullptr);
		if (Error.IsSet())
		{
			return MakeError(Error.GetValue());
		}
		return MakeValue(MoveTemp(Commands));
	}

	struct FWrappedOperator : FNoncopyable
	{
		FWrappedOperator(FCompiledToken InToken, int32 InPrecedence = 0)
			: Token(MoveTemp(InToken)), Precedence(InPrecedence)
		{}

		FWrappedOperator(FWrappedOperator&& In) : Token(MoveTemp(In.Token)), Precedence(In.Precedence) {}
		FWrappedOperator& operator=(FWrappedOperator&& In) { Token = MoveTemp(In.Token); Precedence = In.Precedence; return *this; }

		FCompiledToken Steal() { return MoveTemp(Token); }

		FCompiledToken Token;
		int32 Precedence;
	};

	TOptional<FExpressionError> CompileGroup(const FExpressionToken* GroupStart, const FGuid* StopAt)
	{
		enum class EState { PreUnary, PostUnary, Binary };

		TArray<FWrappedOperator> OperatorStack;
		OperatorStack.Reserve(Tokens.Num() - CurrentTokenIndex);

		bool bFoundEndOfGroup = StopAt == nullptr;

		// Start off looking for a unary operator
		EState State = EState::PreUnary;
		for (; CurrentTokenIndex < Tokens.Num(); ++CurrentTokenIndex)
		{
			auto& Token = Tokens[CurrentTokenIndex];
			const auto& TypeId = Token.Node.GetTypeId();

			if (const FGuid* GroupingEnd = Grammar.GetGrouping(TypeId))
			{
				// Ignore this token
				CurrentTokenIndex++;

				// Start of group - recurse
				auto Error = CompileGroup(&Token, GroupingEnd);

				if (Error.IsSet())
				{
					return Error;
				}

				State = EState::PostUnary;
			}
			else if (StopAt && TypeId == *StopAt)
			{
				// End of group
				bFoundEndOfGroup = true;
				break;
			}
			else if (State == EState::PreUnary)
			{
				if (Grammar.HasPreUnaryOperator(TypeId))
				{
					// Make this a unary op
					OperatorStack.Emplace(FCompiledToken(FCompiledToken::PreUnaryOperator, MoveTemp(Token)));
				}
				else if (Grammar.GetBinaryOperatorDefParameters(TypeId))
				{
					return FExpressionError(FText::Format(LOCTEXT("SyntaxError_NoBinaryOperand", "Syntax error: No operand specified for operator '{0}'"), FText::FromString(Token.Context.GetString())));
				}
				else if (Grammar.HasPostUnaryOperator(TypeId))
				{
					// Found a post-unary operator for the preceeding token
					State = EState::PostUnary;

					// Pop off any pending unary operators
					while (OperatorStack.Num() > 0 && OperatorStack.Last().Precedence <= 0)
					{
						Commands.Add(OperatorStack.Pop(false).Steal());
					}

					// Make this a post-unary op
					OperatorStack.Emplace(FCompiledToken(FCompiledToken::PostUnaryOperator, MoveTemp(Token)));
				}
				else
				{
					// Not an operator, so treat it as an ordinary token
					Commands.Add(FCompiledToken(FCompiledToken::Operand, MoveTemp(Token)));
					State = EState::PostUnary;
				}
			}
			else if (State == EState::PostUnary)
			{
				if (Grammar.HasPostUnaryOperator(TypeId))
				{
					// Pop off any pending unary operators
					while (OperatorStack.Num() > 0 && OperatorStack.Last().Precedence <= 0)
					{
						Commands.Add(OperatorStack.Pop(false).Steal());
					}

					// Make this a post-unary op
					OperatorStack.Emplace(FCompiledToken(FCompiledToken::PostUnaryOperator, MoveTemp(Token)));
				}
				else
				{
					// Checking for binary operators
					if (const FOpParameters* OpParms = Grammar.GetBinaryOperatorDefParameters(TypeId))
					{
						auto CheckPrecedence = [OpParms](int32 LastPrec, int32 Prec)
							{
								return (OpParms->Associativity == EAssociativity::LeftToRight ? (LastPrec <= Prec) : (LastPrec < Prec));
							};

						// Pop off anything of higher (or equal, if LTR associative) precedence than this one onto the command stack
						while (OperatorStack.Num() > 0 && CheckPrecedence(OperatorStack.Last().Precedence, OpParms->Precedence))
						{
							Commands.Add(OperatorStack.Pop(false).Steal());
						}

						// Add the operator itself to the op stack
						OperatorStack.Emplace(FCompiledToken(FCompiledToken::BinaryOperator, MoveTemp(Token)), OpParms->Precedence);

						// Check for a unary op again
						State = EState::PreUnary;
					}
					else
					{
						// Just add the token. It's possible that this is a syntax error (there's no binary operator specified between two tokens),
						// But we don't have enough information at this point to say whether or not it is an error
						Commands.Add(FCompiledToken(FCompiledToken::Operand, MoveTemp(Token)));
						State = EState::PreUnary;
					}
				}
			}
		}

		if (!bFoundEndOfGroup)
		{
			return FExpressionError(FText::Format(LOCTEXT("SyntaxError_UnmatchedGroup", "Syntax error: Reached end of expression before matching end of group '{0}' at line {1}:{2}"),
				FText::FromString(GroupStart->Context.GetString()),
				FText::AsNumber(GroupStart->Context.GetLineNumber()),
				FText::AsNumber(GroupStart->Context.GetCharacterIndex())
			));
		}

		// Pop everything off the operator stack, onto the command stack
		while (OperatorStack.Num() > 0)
		{
			Commands.Add(OperatorStack.Pop(false).Token);
		}

		return TOptional<FExpressionError>();
	}


private:

	int32 CurrentTokenIndex;

	/** Working structures */
	TArray<FCompiledToken> Commands;

private:
	/** Const data provided by the parser */
	const FExpressionGrammar& Grammar;
	TArray<FExpressionToken>& Tokens;
};

namespace ExpressionParser
{
	LexResultType Lex(const TCHAR* InExpression, const FTokenDefinitions& TokenDefinitions)
	{
		FExpressionTokenConsumer TokenConsumer(InExpression);
		
		TOptional<FExpressionError> Error = TokenDefinitions.ConsumeTokens(TokenConsumer);
		if (Error.IsSet())
		{
			return MakeError(Error.GetValue());
		}

		return MakeValue(TokenConsumer.Extract());
	}

	CompileResultType Compile(const TCHAR* InExpression, const FTokenDefinitions& InTokenDefinitions, const FExpressionGrammar& InGrammar)
	{
		TValueOrError<TArray<FExpressionToken>, FExpressionError> Result = Lex(InExpression, InTokenDefinitions);

		if (!Result.IsValid())
		{
			return MakeError(Result.GetError());
		}

		return Compile(MoveTemp(Result.GetValue()), InGrammar);
	}

	CompileResultType Compile(TArray<FExpressionToken> InTokens, const FExpressionGrammar& InGrammar)
	{
		return FExpressionCompiler(InGrammar, InTokens).Compile();
	}

	FExpressionResult Evaluate(const TCHAR* InExpression, const FTokenDefinitions& InTokenDefinitions, const FExpressionGrammar& InGrammar, const IOperatorEvaluationEnvironment& InEnvironment)
	{
		TValueOrError<TArray<FCompiledToken>, FExpressionError> CompilationResult = Compile(InExpression, InTokenDefinitions, InGrammar);

		if (!CompilationResult.IsValid())
		{
			return MakeError(CompilationResult.GetError());
		}

		return Evaluate(CompilationResult.GetValue(), InEnvironment);
	}

	FExpressionResult Evaluate(const TArray<FCompiledToken>& CompiledTokens, const IOperatorEvaluationEnvironment& InEnvironment)
	{
		// Evaluation strategy: the supplied compiled tokens are const. To avoid copying the whole array, we store a separate array of
		// any tokens that are generated at runtime by the evaluator. The operand stack will consist of indices into either the CompiledTokens
		// array, or the RuntimeGeneratedTokens (where Index >= CompiledTokens.Num())
		TArray<FExpressionToken> RuntimeGeneratedTokens;
		TArray<int32> OperandStack;

		/** Get the token pertaining to the specified operand index */
		auto GetToken = [&](int32 Index) -> const FExpressionToken& {
			if (Index < CompiledTokens.Num())
			{
				return CompiledTokens[Index];
			}

			return RuntimeGeneratedTokens[Index - CompiledTokens.Num()];
		};

		/** Add a new token to the runtime generated array */
		auto AddToken = [&](FExpressionToken&& In) -> int32 {
			auto Index = CompiledTokens.Num() + RuntimeGeneratedTokens.Num();
			RuntimeGeneratedTokens.Emplace(MoveTemp(In));
			return Index;
		};


		for (int32 Index = 0; Index < CompiledTokens.Num(); ++Index)
		{
			const auto& Token = CompiledTokens[Index];

			switch(Token.Type)
			{
			case FCompiledToken::Benign:
				continue;

			case FCompiledToken::Operand:
				OperandStack.Push(Index);
				continue;

			case FCompiledToken::BinaryOperator:
				if (OperandStack.Num() >= 2)
				{
					// Binary
					const auto& R = GetToken(OperandStack.Pop());
					const auto& L = GetToken(OperandStack.Pop());

					auto OpResult = InEnvironment.ExecBinary(Token, L, R);
					if (OpResult.IsValid())
					{
						// Inherit the LHS context
						OperandStack.Push(AddToken(FExpressionToken(L.Context, MoveTemp(OpResult.GetValue()))));
					}
					else
					{
						return MakeError(OpResult.GetError());
					}
				}
				else
				{
					FFormatOrderedArguments Args;
					Args.Add(FText::FromString(Token.Context.GetString()));
					return MakeError(FText::Format(LOCTEXT("SyntaxError_NotEnoughOperandsBinary", "Not enough operands for binary operator {0}"), Args));
				}
				break;
			
			case FCompiledToken::PostUnaryOperator:
			case FCompiledToken::PreUnaryOperator:

				if (OperandStack.Num() >= 1)
				{
					const auto& Operand = GetToken(OperandStack.Pop());

					FExpressionResult OpResult = (Token.Type == FCompiledToken::PreUnaryOperator) ?
						InEnvironment.ExecPreUnary(Token, Operand) :
						InEnvironment.ExecPostUnary(Token, Operand);

					if (OpResult.IsValid())
					{
						// Inherit the LHS context
						OperandStack.Push(AddToken(FExpressionToken(Operand.Context, MoveTemp(OpResult.GetValue()))));
					}
					else
					{
						return MakeError(OpResult.GetError());
					}			
				}
				else
				{
					FFormatOrderedArguments Args;
					Args.Add(FText::FromString(Token.Context.GetString()));
					return MakeError(FText::Format(LOCTEXT("SyntaxError_NoUnaryOperand", "No operand for unary operator {0}"), Args));
				}
				break;
			}
		}

		if (OperandStack.Num() == 1)
		{
			return MakeValue(GetToken(OperandStack[0]).Node.Copy());
		}

		return MakeError(LOCTEXT("SyntaxError_InvalidExpression", "Could not evaluate expression"));
	}
}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

namespace Tests
{
PRAGMA_DISABLE_OPTIMIZATION
	struct FOperator {};

	struct FMoveableType
	{
		static int32* LeakCount;

		FMoveableType(int32 InId)
			: Id(InId), bOwnsLeak(true)
		{
			++*LeakCount;
		}

		FMoveableType(FMoveableType&& In) : Id(-1), bOwnsLeak(false) { *this = MoveTemp(In); }
		FMoveableType& operator=(FMoveableType&& In)
		{
			if (bOwnsLeak)
			{
				bOwnsLeak = false;
				--*LeakCount;
			}

			Id = In.Id;
			In.Id = -1;
			
			bOwnsLeak = In.bOwnsLeak;

			In.bOwnsLeak = false;
			return *this;
		}

		FMoveableType(const FMoveableType& In) : Id(-1), bOwnsLeak(false) { *this = In; }
		const FMoveableType& operator=(const FMoveableType& In)
		{
			const bool bDidOwnLeak = bOwnsLeak;
			bOwnsLeak = In.bOwnsLeak;

			if (bOwnsLeak && !bDidOwnLeak)
			{
				++*LeakCount;
			}
			else if (!bOwnsLeak && bDidOwnLeak)
			{
				--*LeakCount;
			}
			return *this;
		}

		virtual ~FMoveableType()
		{
			if (bOwnsLeak)
			{
				--*LeakCount;
			}
		}

		int32 Id;
		bool bOwnsLeak;
	};
	
	int32* FMoveableType::LeakCount = nullptr;

	template<typename T>
	bool TestWithType(FAutomationTestBase* Test)
	{
		int32 NumLeaks = 0;
		
		// Test that move-assigning the expression node correctly assigns the data, and calls the destructors successfully
		{
			TGuardValue<int32*> LeakCounter(T::LeakCount, &NumLeaks);
			
			FExpressionNode Original(T(1));
			FExpressionNode New = MoveTemp(Original);
			
			int32 ResultingId = New.Cast<T>()->Id;
			if (ResultingId != 1)
			{
				Test->AddError(FString::Printf(TEXT("Expression node move operator did not operate correctly. Expected moved-to state to be 1, it's actually %d."), ResultingId));
				return false;
			}

			// Try assigning it over the top again
			Original = FExpressionNode(T(1));
			New = MoveTemp(Original);

			ResultingId = New.Cast<T>()->Id;
			if (ResultingId != 1)
			{
				Test->AddError(FString::Printf(TEXT("Expression node move operator did not operate correctly. Expected moved-to state to be 1, it's actually %d."), ResultingId));
				return false;
			}

			// Now try running it all through a parser
			FTokenDefinitions TokenDefs;
			FExpressionGrammar Grammar;
			FOperatorJumpTable JumpTable;

			// Only valid tokens are a, b, and +
			TokenDefs.DefineToken([](FExpressionTokenConsumer& Consumer){
				auto Token = Consumer.GetStream().GenerateToken(1);
				if (Token.IsSet())
				{
					switch(Consumer.GetStream().PeekChar())
					{
					case 'a': Consumer.Add(Token.GetValue(), T(1)); break;
					case '+': Consumer.Add(Token.GetValue(), FOperator()); break;
					}
				}
				return TOptional<FExpressionError>();
			});

			Grammar.DefinePreUnaryOperator<FOperator>();
			Grammar.DefineBinaryOperator<FOperator>(1);

			JumpTable.MapPreUnary<FOperator>([](const T& A)					{ return T(A.Id); });
			JumpTable.MapBinary<FOperator>([](const T& A, const T& B)		{ return T(A.Id); });

			ExpressionParser::Evaluate(TEXT("+a"), TokenDefs, Grammar, JumpTable);
			ExpressionParser::Evaluate(TEXT("a+a"), TokenDefs, Grammar, JumpTable);
			ExpressionParser::Evaluate(TEXT("+a++a"), TokenDefs, Grammar, JumpTable);
		}

		if (NumLeaks != 0)
		{
			Test->AddError(FString::Printf(TEXT("Expression node did not call wrapped type's destructors correctly. Potentially resulted in %d leaks."), NumLeaks));
			return false;
		}

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExpressionParserMoveableTypes, "System.Core.Expression Parser.Moveable Types", EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)
	bool FExpressionParserMoveableTypes::RunTest( const FString& Parameters )
	{
		return TestWithType<FMoveableType>(this);
	}

	struct FHugeType : FMoveableType
	{
		FHugeType(int32 InId) : FMoveableType(InId) {}
		FHugeType(FHugeType&& In) : FMoveableType(MoveTemp(In)) {}
		FHugeType(const FHugeType& In) : FMoveableType(In) {}

		FHugeType& operator=(FHugeType&& In)
		{
			MoveTemp(In);
			return *this;
		}
		
		uint8 Padding[1024];
	};

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExpressionParserAllocatedTypes, "System.Core.Expression Parser.Allocated Types", EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)
	bool FExpressionParserAllocatedTypes::RunTest( const FString& Parameters )
	{
		return TestWithType<FHugeType>(this);
	}

}

DEFINE_EXPRESSION_NODE_TYPE(Tests::FMoveableType, 0xB7F3F127, 0xD5E74833, 0x9EAB754E, 0x6CF3AAC1)
DEFINE_EXPRESSION_NODE_TYPE(Tests::FHugeType, 0x4A329D81, 0x102343A8, 0xAB95BF45, 0x6578EE54)
DEFINE_EXPRESSION_NODE_TYPE(Tests::FOperator, 0xC777A5D7, 0x6895456C, 0x9854BFA0, 0xB71B5A8D)

PRAGMA_ENABLE_OPTIMIZATION

#endif

#undef LOCTEXT_NAMESPACE
