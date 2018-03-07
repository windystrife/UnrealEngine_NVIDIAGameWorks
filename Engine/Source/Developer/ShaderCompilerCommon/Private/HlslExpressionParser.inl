// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HlslExpressionParser.inl - Implementation for parsing hlsl expressions.
=============================================================================*/

#pragma once
#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Containers/UnrealString.h"
#include "Containers/Set.h"
#include "Developer/ShaderCompilerCommon/Private/HlslParser.h"
#include "Developer/ShaderCompilerCommon/Private/HlslAST.h"

class Error;

namespace CrossCompiler
{
	EParseResult ParseResultError();

	struct FSymbolScope;
	//struct FInfo;

	EParseResult ComputeExpr(FHlslScanner& Scanner, int32 MinPrec, /*FInfo& Info,*/ FSymbolScope* SymbolScope, bool bAllowAssignment, FLinearAllocator* Allocator, AST::FExpression** OutExpression, AST::FExpression** OutTernaryExpression);
	EParseResult ParseExpressionList(EHlslToken EndListToken, FHlslScanner& Scanner, FSymbolScope* SymbolScope, EHlslToken NewStartListToken, FLinearAllocator* Allocator, AST::FExpression* OutExpression);

	struct FSymbolScope
	{
		FSymbolScope* Parent;
		const TCHAR* Name;

		TSet/*TLinearSet*/<FString> Symbols;
		TLinearArray<FSymbolScope> Children;

		FSymbolScope(FLinearAllocator* InAllocator, FSymbolScope* InParent) :
			Parent(InParent),
			Name(nullptr),
			Children(InAllocator)
		{
		}
		~FSymbolScope() {}

		void Add(const FString& Type)
		{
			Symbols.Add(Type);
		}

		static bool FindType(const FSymbolScope* Scope, const FString& Type, bool bSearchUpwards = true)
		{
			while (Scope)
			{
				if (Scope->Symbols.Contains(Type))
				{
					return true;
				}

				if (!bSearchUpwards)
				{
					return false;
				}

				Scope = Scope->Parent;
			}

			return false;
		}

		const FSymbolScope* FindNamespace(const TCHAR* Namespace) const
		{
			for (auto& Child : Children)
			{
				if (!FCString::Strcmp(Child.Name, Namespace))
				{
					return &Child;
				}
			}

			return nullptr;
		}

		static const FSymbolScope* FindGlobalNamespace(const TCHAR* Namespace, const FSymbolScope* Scope)
		{
			return Scope->GetGlobalScope()->FindNamespace(Namespace);
		}

		const FSymbolScope* GetGlobalScope() const
		{
			const FSymbolScope* Scope = this;
			while (Scope->Parent)
			{
				Scope = Scope->Parent;
			}

			return Scope;
		}
	};

	struct FCreateSymbolScope
	{
		FSymbolScope* Original;
		FSymbolScope** Current;

		FCreateSymbolScope(FLinearAllocator* InAllocator, FSymbolScope** InCurrent) :
			Current(InCurrent)
		{
			Original = *InCurrent;
			auto* NewScope = new(Original->Children) FSymbolScope(InAllocator, Original);
			*Current = NewScope;
		}

		~FCreateSymbolScope()
		{
			*Current = Original;
		}
	};

/*
	struct FInfo
	{
		int32 Indent;
		bool bEnabled;

		FInfo(bool bInEnabled = false) : Indent(0), bEnabled(bInEnabled) {}

		void Print(const FString& Message)
		{
			if (bEnabled)
			{
				FPlatformMisc::LocalPrint(*Message);
			}
		}

		void PrintTabs()
		{
			if (bEnabled)
			{
				for (int32 Index = 0; Index < Indent; ++Index)
				{
					FPlatformMisc::LocalPrint(TEXT("\t"));
				}
			}
		}

		void PrintWithTabs(const FString& Message)
		{
			PrintTabs();
			Print(Message);
		}
	};

	struct FInfoIndentScope
	{
		FInfoIndentScope(FInfo& InInfo) : Info(InInfo)
		{
			++Info.Indent;
		}

		~FInfoIndentScope()
		{
			--Info.Indent;
		}

		FInfo& Info;
	};*/

	enum ETypeFlags
	{
		ETF_VOID					= 1 << 0,
		ETF_BUILTIN_NUMERIC			= 1 << 1,
		ETF_SAMPLER_TEXTURE_BUFFER	= 1 << 2,
		ETF_USER_TYPES				= 1 << 3,
		ETF_ERROR_IF_NOT_USER_TYPE	= 1 << 4,
	};

	EParseResult ParseGeneralType(const FHlslToken* Token, int32 TypeFlags, FLinearAllocator* Allocator, AST::FTypeSpecifier** OutSpecifier)
	{
		if (!Token)
		{
			return ParseResultError();
		}

		bool bMatched = false;
		const TCHAR* InnerType = nullptr;
		switch (Token->Token)
		{
		case EHlslToken::Void:
			if (TypeFlags & ETF_VOID)
			{
				bMatched = true;
			}
			break;

		case EHlslToken::Bool:
		case EHlslToken::Bool1:
		case EHlslToken::Bool2:
		case EHlslToken::Bool3:
		case EHlslToken::Bool4:
		case EHlslToken::Bool1x1:
		case EHlslToken::Bool1x2:
		case EHlslToken::Bool1x3:
		case EHlslToken::Bool1x4:
		case EHlslToken::Bool2x1:
		case EHlslToken::Bool2x2:
		case EHlslToken::Bool2x3:
		case EHlslToken::Bool2x4:
		case EHlslToken::Bool3x1:
		case EHlslToken::Bool3x2:
		case EHlslToken::Bool3x3:
		case EHlslToken::Bool3x4:
		case EHlslToken::Bool4x1:
		case EHlslToken::Bool4x2:
		case EHlslToken::Bool4x3:
		case EHlslToken::Bool4x4:

		case EHlslToken::Int:
		case EHlslToken::Int1:
		case EHlslToken::Int2:
		case EHlslToken::Int3:
		case EHlslToken::Int4:
		case EHlslToken::Int1x1:
		case EHlslToken::Int1x2:
		case EHlslToken::Int1x3:
		case EHlslToken::Int1x4:
		case EHlslToken::Int2x1:
		case EHlslToken::Int2x2:
		case EHlslToken::Int2x3:
		case EHlslToken::Int2x4:
		case EHlslToken::Int3x1:
		case EHlslToken::Int3x2:
		case EHlslToken::Int3x3:
		case EHlslToken::Int3x4:
		case EHlslToken::Int4x1:
		case EHlslToken::Int4x2:
		case EHlslToken::Int4x3:
		case EHlslToken::Int4x4:

		case EHlslToken::Uint:
		case EHlslToken::Uint1:
		case EHlslToken::Uint2:
		case EHlslToken::Uint3:
		case EHlslToken::Uint4:
		case EHlslToken::Uint1x1:
		case EHlslToken::Uint1x2:
		case EHlslToken::Uint1x3:
		case EHlslToken::Uint1x4:
		case EHlslToken::Uint2x1:
		case EHlslToken::Uint2x2:
		case EHlslToken::Uint2x3:
		case EHlslToken::Uint2x4:
		case EHlslToken::Uint3x1:
		case EHlslToken::Uint3x2:
		case EHlslToken::Uint3x3:
		case EHlslToken::Uint3x4:
		case EHlslToken::Uint4x1:
		case EHlslToken::Uint4x2:
		case EHlslToken::Uint4x3:
		case EHlslToken::Uint4x4:

		case EHlslToken::Half:
		case EHlslToken::Half1:
		case EHlslToken::Half2:
		case EHlslToken::Half3:
		case EHlslToken::Half4:
		case EHlslToken::Half1x1:
		case EHlslToken::Half1x2:
		case EHlslToken::Half1x3:
		case EHlslToken::Half1x4:
		case EHlslToken::Half2x1:
		case EHlslToken::Half2x2:
		case EHlslToken::Half2x3:
		case EHlslToken::Half2x4:
		case EHlslToken::Half3x1:
		case EHlslToken::Half3x2:
		case EHlslToken::Half3x3:
		case EHlslToken::Half3x4:
		case EHlslToken::Half4x1:
		case EHlslToken::Half4x2:
		case EHlslToken::Half4x3:
		case EHlslToken::Half4x4:

		case EHlslToken::Float:
		case EHlslToken::Float1:
		case EHlslToken::Float2:
		case EHlslToken::Float3:
		case EHlslToken::Float4:
		case EHlslToken::Float1x1:
		case EHlslToken::Float1x2:
		case EHlslToken::Float1x3:
		case EHlslToken::Float1x4:
		case EHlslToken::Float2x1:
		case EHlslToken::Float2x2:
		case EHlslToken::Float2x3:
		case EHlslToken::Float2x4:
		case EHlslToken::Float3x1:
		case EHlslToken::Float3x2:
		case EHlslToken::Float3x3:
		case EHlslToken::Float3x4:
		case EHlslToken::Float4x1:
		case EHlslToken::Float4x2:
		case EHlslToken::Float4x3:
		case EHlslToken::Float4x4:
			if (TypeFlags & ETF_BUILTIN_NUMERIC)
			{
				bMatched = true;
			}
			break;

		case EHlslToken::Texture:
		case EHlslToken::Texture1D:
		case EHlslToken::Texture1DArray:
		case EHlslToken::Texture2D:
		case EHlslToken::Texture2DArray:
		case EHlslToken::Texture2DMS:
		case EHlslToken::Texture2DMSArray:
		case EHlslToken::Texture3D:
		case EHlslToken::TextureCube:
		case EHlslToken::TextureCubeArray:

		case EHlslToken::Buffer:
		case EHlslToken::AppendStructuredBuffer:
		case EHlslToken::ConsumeStructuredBuffer:
		case EHlslToken::RWBuffer:
		case EHlslToken::RWStructuredBuffer:
		case EHlslToken::RWTexture1D:
		case EHlslToken::RWTexture1DArray:
		case EHlslToken::RWTexture2D:
		case EHlslToken::RWTexture2DArray:
		case EHlslToken::RWTexture3D:
		case EHlslToken::StructuredBuffer:
			if (TypeFlags & ETF_SAMPLER_TEXTURE_BUFFER)
			{
				bMatched = true;
				InnerType = TEXT("float4");
			}
			break;

		case EHlslToken::Sampler:
		case EHlslToken::Sampler1D:
		case EHlslToken::Sampler2D:
		case EHlslToken::Sampler3D:
		case EHlslToken::SamplerCube:
		case EHlslToken::SamplerState:
		case EHlslToken::SamplerComparisonState:
		case EHlslToken::ByteAddressBuffer:
		case EHlslToken::RWByteAddressBuffer:
			if (TypeFlags & ETF_SAMPLER_TEXTURE_BUFFER)
			{
				bMatched = true;
			}
			break;
		}

		if (bMatched)
		{
			auto* Type = new(Allocator) AST::FTypeSpecifier(Allocator, Token->SourceInfo);
			Type->TypeName = Allocator->Strdup(Token->String);
			Type->InnerType = InnerType;
			*OutSpecifier = Type;
			return EParseResult::Matched;
		}

		return EParseResult::NotMatched;
	}

	EParseResult ParseGeneralTypeFromToken(const FHlslToken* Token, int32 TypeFlags, FSymbolScope* SymbolScope, FLinearAllocator* Allocator, AST::FTypeSpecifier** OutSpecifier)
	{
		if (Token)
		{
			if (ParseGeneralType(Token, TypeFlags, Allocator, OutSpecifier) == EParseResult::Matched)
			{
				return EParseResult::Matched;
			}

			if (TypeFlags & ETF_USER_TYPES)
			{
				check(SymbolScope);
				if (Token->Token == EHlslToken::Identifier)
				{
					if (FSymbolScope::FindType(SymbolScope, Token->String))
					{
						auto* Type = new(Allocator) AST::FTypeSpecifier(Allocator, Token->SourceInfo);
						Type->TypeName = Allocator->Strdup(Token->String);
						*OutSpecifier = Type;
						return EParseResult::Matched;
					}
					else if (TypeFlags & ETF_ERROR_IF_NOT_USER_TYPE)
					{
						return ParseResultError();
					}
				}
			}

			return EParseResult::NotMatched;
		}

		return ParseResultError();
	}

	EParseResult ParseGeneralType(FHlslScanner& Scanner, int32 TypeFlags, FSymbolScope* SymbolScope, FLinearAllocator* Allocator, AST::FTypeSpecifier** OutSpecifier)
	{
		auto* Token = Scanner.PeekToken();

		// Handle types with namespaces
		{
			auto* Token1 = Scanner.PeekToken(1);
			auto* Token2 = Scanner.PeekToken(2);
			FString TypeString;
			if (Token && Token->Token == EHlslToken::Identifier && Token1 && Token1->Token == EHlslToken::ColonColon && Token2 && Token2->Token == EHlslToken::Identifier)
			{
				auto* Namespace = SymbolScope->GetGlobalScope();
				do
				{
					Token = Scanner.PeekToken();
					check(Scanner.MatchToken(EHlslToken::Identifier));
					auto* OuterNamespace = Token;
					check(Scanner.MatchToken(EHlslToken::ColonColon));
					auto* InnerOrType = Scanner.PeekToken();
					if (!InnerOrType)
					{
						Scanner.SourceError(*FString::Printf(TEXT("Expecting identifier for type '%s'!"), InnerOrType ? *InnerOrType->String : TEXT("null")));
						return ParseResultError();
					}

					Namespace = Namespace->FindNamespace(*OuterNamespace->String);
					if (!Namespace)
					{
						Scanner.SourceError(*FString::Printf(TEXT("Unknown namespace '%s'!"), *TypeString));
						return ParseResultError();
					}
					TypeString += OuterNamespace->String;
					TypeString += TEXT("::");
					auto* Token3 = Scanner.PeekToken(1);
					if (Token3 && Token3->Token == EHlslToken::ColonColon)
					{
						continue;
					}
					else
					{
						if (InnerOrType && FChar::IsAlpha(InnerOrType->String[0]))
						{
							Scanner.Advance();
							if (FSymbolScope::FindType(Namespace, InnerOrType->String, false))
							{
								auto* Type = new(Allocator) AST::FTypeSpecifier(Allocator, Token->SourceInfo);
								Type->TypeName = Allocator->Strdup(*TypeString);
								*OutSpecifier = Type;
								return EParseResult::Matched;
							}
							else
							{
								Scanner.SourceError(*FString::Printf(TEXT("Unknown type '%s'!"), *TypeString));
								return ParseResultError();
							}
						}
						break;
					}
				}
				while (Token);
			}
		}

		auto Result = ParseGeneralTypeFromToken(Token, TypeFlags, SymbolScope, Allocator, OutSpecifier);
		if (Result == EParseResult::Matched)
		{
			Scanner.Advance();
			return EParseResult::Matched;
		}

		if (Result == EParseResult::Error && (TypeFlags & ETF_ERROR_IF_NOT_USER_TYPE))
		{
			Scanner.SourceError(*FString::Printf(TEXT("Unknown type '%s'!"), Token ? *Token->String : TEXT("<null>")));
			return Result;
		}

		return EParseResult::NotMatched;
	}

	// Unary!(Unary-(Unary+())) would have ! as Top, and + as Inner
	EParseResult MatchUnaryOperator(FHlslScanner& Scanner, /*FInfo& Info,*/ FSymbolScope* SymbolScope, FLinearAllocator* Allocator, AST::FExpression** OuterExpression, AST::FExpression** InnerExpression)
	{
		bool bFoundAny = false;
		bool bTryAgain = true;
		AST::FExpression*& PrevExpression = *InnerExpression;
		while (Scanner.HasMoreTokens() && bTryAgain)
		{
			auto* Token = Scanner.GetCurrentToken();
			AST::EOperators Operator = AST::EOperators::Plus;

			switch (Token->Token)
			{
			case EHlslToken::PlusPlus:
				bFoundAny = true;
				Scanner.Advance();
				Operator = AST::EOperators::PreInc;
				break;

			case EHlslToken::MinusMinus:
				bFoundAny = true;
				Scanner.Advance();
				Operator = AST::EOperators::PreDec;
				break;

			case EHlslToken::Plus:
				Scanner.Advance();
				bFoundAny = true;
				Operator = AST::EOperators::Plus;
				break;

			case EHlslToken::Minus:
				Scanner.Advance();
				bFoundAny = true;
				Operator = AST::EOperators::Minus;
				break;

			case EHlslToken::Not:
				Scanner.Advance();
				bFoundAny = true;
				Operator = AST::EOperators::LogicNot;
				break;

			case EHlslToken::Neg:
				Scanner.Advance();
				bFoundAny = true;
				Operator = AST::EOperators::BitNeg;
				break;

			case EHlslToken::LeftParenthesis:
			// Only cast expressions are Unary
			{
				const auto* Peek1 = Scanner.PeekToken(1);
				const auto* Peek2 = Scanner.PeekToken(2);
				AST::FTypeSpecifier* TypeSpecifier = nullptr;
				if (Peek1 && ParseGeneralTypeFromToken(Peek1, ETF_BUILTIN_NUMERIC | ETF_USER_TYPES, SymbolScope, Allocator, &TypeSpecifier) == EParseResult::Matched && Peek2 && Peek2->Token == EHlslToken::RightParenthesis)
				{
					// Cast
					Scanner.Advance();
					Scanner.Advance();
					Scanner.Advance();
					bFoundAny = true;

					auto* Expression = new(Allocator) AST::FUnaryExpression(Allocator, AST::EOperators::TypeCast, nullptr, Token->SourceInfo);
					if (PrevExpression)
					{
						PrevExpression->SubExpressions[0] = Expression;
					}

					Expression->TypeSpecifier = TypeSpecifier;

					if (!*OuterExpression)
					{
						*OuterExpression = Expression;
					}

					PrevExpression = Expression;
					continue;
				}
				else
				{
					// Non-unary
					return bFoundAny ? EParseResult::Matched : EParseResult::NotMatched;
				}
			}
				break;

			default:
				return bFoundAny ? EParseResult::Matched : EParseResult::NotMatched;
			}

			auto* Expression = new(Allocator) AST::FUnaryExpression(Allocator, Operator, nullptr, Token->SourceInfo);
			if (PrevExpression)
			{
				PrevExpression->SubExpressions[0] = Expression;
			}

			if (!*OuterExpression)
			{
				*OuterExpression = Expression;
			}

			PrevExpression = Expression;
		}

		// Ran out of tokens!
		return ParseResultError();
	}

	EParseResult MatchSuffixOperator(FHlslScanner& Scanner, /*FInfo& Info,*/ FSymbolScope* SymbolScope, bool bAllowAssignment, FLinearAllocator* Allocator, AST::FExpression** InOutExpression, AST::FExpression** OutTernaryExpression)
	{
		bool bFoundAny = false;
		bool bTryAgain = true;
		AST::FExpression*& PrevExpression = *InOutExpression;
		while (Scanner.HasMoreTokens() && bTryAgain)
		{
			auto* Token = Scanner.GetCurrentToken();
			AST::EOperators Operator = AST::EOperators::Plus;

			switch (Token->Token)
			{
			case EHlslToken::LeftSquareBracket:
			{
				Scanner.Advance();
				AST::FExpression* ArrayIndex = nullptr;
				auto Result = ComputeExpr(Scanner, 1, /*Info,*/ SymbolScope, bAllowAssignment, Allocator, &ArrayIndex, nullptr);
				if (Result != EParseResult::Matched)
				{
					Scanner.SourceError(TEXT("Expected expression!"));
					return ParseResultError();
				}

				if (!Scanner.MatchToken(EHlslToken::RightSquareBracket))
				{
					Scanner.SourceError(TEXT("Expected ']'!"));
					return ParseResultError();
				}

				auto* ArrayIndexExpression = new(Allocator) AST::FBinaryExpression(Allocator, AST::EOperators::ArrayIndex, PrevExpression, ArrayIndex, Token->SourceInfo);
				PrevExpression = ArrayIndexExpression;
				bFoundAny = true;
			}
				break;
			case EHlslToken::Dot:
			{
				Scanner.Advance();
				const auto* Identifier = Scanner.GetCurrentToken();
				if (!Scanner.MatchToken(EHlslToken::Identifier))
				{
					Scanner.SourceError(TEXT("Expected identifier for member or swizzle!"));
					return ParseResultError();
				}
				auto* FieldExpression = new(Allocator) AST::FUnaryExpression(Allocator, AST::EOperators::FieldSelection, PrevExpression, Token->SourceInfo);
				FieldExpression->Identifier = Allocator->Strdup(Identifier->String);
				PrevExpression = FieldExpression;
				bFoundAny = true;
			}
				break;
			case EHlslToken::LeftParenthesis:
			{
				Scanner.Advance();

				// Function Call
				auto* FunctionCall = new(Allocator) AST::FFunctionExpression(Allocator, Token->SourceInfo, PrevExpression);
				auto Result = ParseExpressionList(EHlslToken::RightParenthesis, Scanner, SymbolScope, EHlslToken::Invalid, Allocator, FunctionCall);
				if (Result != EParseResult::Matched)
				{
					Scanner.SourceError(TEXT("Expected ')'!"));
					return ParseResultError();
				}

				PrevExpression = FunctionCall;

				bFoundAny = true;
			}
				break;
			case EHlslToken::PlusPlus:
			{
				Scanner.Advance();
				auto* IncExpression = new(Allocator) AST::FUnaryExpression(Allocator, AST::EOperators::PostInc, PrevExpression, Token->SourceInfo);
				PrevExpression = IncExpression;
				bFoundAny = true;
			}
				break;
			case EHlslToken::MinusMinus:
			{
				Scanner.Advance();
				auto* DecExpression = new(Allocator) AST::FUnaryExpression(Allocator, AST::EOperators::PostDec, PrevExpression, Token->SourceInfo);
				PrevExpression = DecExpression;
				bFoundAny = true;
			}
				break;
			case EHlslToken::Question:
			{
				Scanner.Advance();
				AST::FExpression* Left = nullptr;
				if (ComputeExpr(Scanner, 0, /*Info,*/ SymbolScope, true, Allocator, &Left, nullptr) != EParseResult::Matched)
				{
					Scanner.SourceError(TEXT("Expected expression!"));
					return ParseResultError();
				}
				if (!Scanner.MatchToken(EHlslToken::Colon))
				{
					Scanner.SourceError(TEXT("Expected ':'!"));
					return ParseResultError();
				}
				AST::FExpression* Right = nullptr;
				if (ComputeExpr(Scanner, 0, /*Info,*/ SymbolScope, true, Allocator, &Right, nullptr) != EParseResult::Matched)
				{
					Scanner.SourceError(TEXT("Expected expression!"));
					return ParseResultError();
				}

				auto* Ternary = new(Allocator) AST::FExpression(Allocator, AST::EOperators::Conditional, nullptr, Left, Right, Token->SourceInfo);
				*OutTernaryExpression = Ternary;
				bFoundAny = true;
				bTryAgain = false;
			}
				break;
			default:
				bTryAgain = false;
				break;
			}
		}

		*InOutExpression = PrevExpression;
		return bFoundAny ? EParseResult::Matched : EParseResult::NotMatched;
	}

	EParseResult ComputeAtom(FHlslScanner& Scanner, /*FInfo& Info,*/ FSymbolScope* SymbolScope, bool bAllowAssignment, FLinearAllocator* Allocator, AST::FExpression** OutExpression, AST::FExpression** OutTernaryExpression)
	{
		AST::FExpression* InnerUnaryExpression = nullptr;
		auto UnaryResult = MatchUnaryOperator(Scanner, /*Info,*/ SymbolScope, Allocator, OutExpression, &InnerUnaryExpression);
		auto* Token = Scanner.GetCurrentToken();
		if (!Token || UnaryResult == EParseResult::Error)
		{
			return ParseResultError();
		}

		AST::FExpression* AtomExpression = nullptr;
		switch (Token->Token)
		{
		case EHlslToken::BoolConstant:
			Scanner.Advance();
			AtomExpression = new(Allocator) AST::FUnaryExpression(Allocator, AST::EOperators::BoolConstant, nullptr, Token->SourceInfo);
			AtomExpression->BoolConstant = Token->UnsignedInteger != 0;
			break;

		case EHlslToken::UnsignedIntegerConstant:
			Scanner.Advance();
			AtomExpression = new(Allocator) AST::FUnaryExpression(Allocator, AST::EOperators::UintConstant, nullptr, Token->SourceInfo);
			AtomExpression->UintConstant = Token->UnsignedInteger;
			break;

		case EHlslToken::FloatConstant:
			Scanner.Advance();
			AtomExpression = new(Allocator) AST::FUnaryExpression(Allocator, AST::EOperators::FloatConstant, nullptr, Token->SourceInfo);
			AtomExpression->FloatConstant = Token->Float;
			break;

		case EHlslToken::Identifier:
		{
			auto* Token1 = Scanner.PeekToken(1);
			auto* Token2 = Scanner.PeekToken(2);
			if (Token1 && Token1->Token == EHlslToken::ColonColon && Token2 && Token2->Token == EHlslToken::Identifier)
			{
				FString Name = Token->String;
				Scanner.Advance();
				while (Token1 && Token1->Token == EHlslToken::ColonColon && Token2 && Token2->Token == EHlslToken::Identifier)
				{
					Name += Token1->String;
					Name += Token2->String;
					Scanner.Advance();
					Scanner.Advance();
					Token1 = Scanner.PeekToken();
					Token2 = Scanner.PeekToken(1);
				}

				AtomExpression = new(Allocator) AST::FUnaryExpression(Allocator, AST::EOperators::Identifier, nullptr, Token->SourceInfo);
				AtomExpression->Identifier = Allocator->Strdup(*Name);
			}
			else
			{
				Scanner.Advance();
				AtomExpression = new(Allocator) AST::FUnaryExpression(Allocator, AST::EOperators::Identifier, nullptr, Token->SourceInfo);
				AtomExpression->Identifier = Allocator->Strdup(Token->String);
			}
		}
			break;

		case EHlslToken::LeftParenthesis:
		{
			Scanner.Advance();

			// Check if it's a cast expression first
			const auto* Peek1 = Scanner.PeekToken(0);
			const auto* Peek2 = Scanner.PeekToken(1);
			// Parenthesis expression
			if (ComputeExpr(Scanner, 1, /*Info,*/ SymbolScope, bAllowAssignment, Allocator, &AtomExpression, nullptr) != EParseResult::Matched)
			{
				Scanner.SourceError(TEXT("Expected expression!"));
				return ParseResultError();
			}

			if (!Scanner.MatchToken(EHlslToken::RightParenthesis))
			{
				Scanner.SourceError(TEXT("Expected ')'!"));
				return ParseResultError();
			}
		}
			break;

		default:
		{
			AST::FTypeSpecifier* TypeSpecifier = nullptr;

			// Grrr handle Sampler as a variable name... This is safe here since Declarations are always handled first
			if (ParseGeneralType(Scanner, ETF_SAMPLER_TEXTURE_BUFFER, nullptr, Allocator, &TypeSpecifier) == EParseResult::Matched)
			{
				//@todo-rco: Check this var exists on the symnbol table
				AtomExpression = new(Allocator) AST::FUnaryExpression(Allocator, AST::EOperators::Identifier, nullptr, TypeSpecifier->SourceInfo);
				AtomExpression->Identifier = TypeSpecifier->TypeName;
				break;
			}
			// Handle float3(x,y,z)
			else if (ParseGeneralType(Scanner, ETF_BUILTIN_NUMERIC, nullptr, Allocator, &TypeSpecifier) == EParseResult::Matched)
			{
				if (Scanner.MatchToken(EHlslToken::LeftParenthesis))
				{
					auto* TypeExpression = new(Allocator) AST::FUnaryExpression(Allocator, AST::EOperators::Identifier, nullptr, TypeSpecifier->SourceInfo);
					TypeExpression->Identifier = TypeSpecifier->TypeName;
					auto* FunctionCall = new(Allocator) AST::FFunctionExpression(Allocator, Token->SourceInfo, TypeExpression);
					auto Result = ParseExpressionList(EHlslToken::RightParenthesis, Scanner, SymbolScope, EHlslToken::Invalid, Allocator, FunctionCall);
					if (Result != EParseResult::Matched)
					{
						Scanner.SourceError(TEXT("Unexpected type in numeric constructor!"));
						return ParseResultError();
					}

					AtomExpression = FunctionCall;
				}
				else
				{
					Scanner.SourceError(TEXT("Unexpected type in declaration!"));
					return ParseResultError();
				}
				break;
			}
			else
			{
				if (UnaryResult == EParseResult::Matched)
				{
					Scanner.SourceError(TEXT("Expected expression!"));
					return ParseResultError();
				}

				return EParseResult::NotMatched;
			}
		}
			break;
		}

		check(AtomExpression);
		auto SuffixResult = MatchSuffixOperator(Scanner, /*Info,*/ SymbolScope, bAllowAssignment, Allocator, &AtomExpression, OutTernaryExpression);
		//auto* Token = Scanner.GetCurrentToken();
		if (/*!Token || */SuffixResult == EParseResult::Error)
		{
			return ParseResultError();
		}

		// Patch unary if necessary
		if (InnerUnaryExpression)
		{
			check(!InnerUnaryExpression->SubExpressions[0]);
			InnerUnaryExpression->SubExpressions[0] = AtomExpression;
		}

		if (!*OutExpression)
		{
			*OutExpression = AtomExpression;
		}

		return EParseResult::Matched;
	}

	int32 GetPrecedence(const FHlslToken* Token)
	{
		if (Token)
		{
			switch (Token->Token)
			{
			case EHlslToken::Equal:
			case EHlslToken::PlusEqual:
			case EHlslToken::MinusEqual:
			case EHlslToken::TimesEqual:
			case EHlslToken::DivEqual:
			case EHlslToken::ModEqual:
			case EHlslToken::GreaterGreaterEqual:
			case EHlslToken::LowerLowerEqual:
			case EHlslToken::AndEqual:
			case EHlslToken::OrEqual:
			case EHlslToken::XorEqual:
				return 1;

			case EHlslToken::Question:
				check(0);
				return 2;

			case EHlslToken::OrOr:
				return 3;

			case EHlslToken::AndAnd:
				return 4;

			case EHlslToken::Or:
				return 5;

			case EHlslToken::Xor:
				return 6;

			case EHlslToken::And:
				return 7;

			case EHlslToken::EqualEqual:
			case EHlslToken::NotEqual:
				return 8;

			case EHlslToken::Lower:
			case EHlslToken::Greater:
			case EHlslToken::LowerEqual:
			case EHlslToken::GreaterEqual:
				return 9;

			case EHlslToken::LowerLower:
			case EHlslToken::GreaterGreater:
				return 10;

			case EHlslToken::Plus:
			case EHlslToken::Minus:
				return 11;

			case EHlslToken::Times:
			case EHlslToken::Div:
			case EHlslToken::Mod:
				return 12;

			default:
				break;
			}
		}

		return -1;
	}

	bool IsTernaryOperator(const FHlslToken* Token)
	{
		return (Token && Token->Token == EHlslToken::Question);
	}

	bool IsBinaryOperator(const FHlslToken* Token)
	{
		return GetPrecedence(Token) > 0;
	}

	bool IsAssignmentOperator(const FHlslToken* Token)
	{
		if (Token)
		{
			switch (Token->Token)
			{
			case EHlslToken::Equal:
			case EHlslToken::PlusEqual:
			case EHlslToken::MinusEqual:
			case EHlslToken::TimesEqual:
			case EHlslToken::DivEqual:
			case EHlslToken::ModEqual:
			case EHlslToken::GreaterGreaterEqual:
			case EHlslToken::LowerLowerEqual:
			case EHlslToken::AndEqual:
			case EHlslToken::OrEqual:
			case EHlslToken::XorEqual:
				return true;

			default:
				break;
			}
		}

		return false;
	}

	bool IsRightAssociative(const FHlslToken* Token)
	{
		return IsTernaryOperator(Token);
	}

	// Ternary is handled by popping out so it can right associate
	//#todo-rco: Fix the case for right-associative assignment operators
	EParseResult ComputeExpr(FHlslScanner& Scanner, int32 MinPrec, /*FInfo& Info,*/ FSymbolScope* SymbolScope, bool bAllowAssignment, FLinearAllocator* Allocator, AST::FExpression** OutExpression, AST::FExpression** OutTernaryExpression)
	{
		auto OriginalToken = Scanner.GetCurrentTokenIndex();
		//FInfoIndentScope Scope(Info);
		/*
		// Precedence Climbing
		// http://eli.thegreenplace.net/2012/08/02/parsing-expressions-by-precedence-climbing
			compute_expr(min_prec):
			  result = compute_atom()

			  while cur token is a binary operator with precedence >= min_prec:
				prec, assoc = precedence and associativity of current token
				if assoc is left:
				  next_min_prec = prec + 1
				else:
				  next_min_prec = prec
				rhs = compute_expr(next_min_prec)
				result = compute operator(result, rhs)

			  return result
		*/
		//Info.PrintWithTabs(FString::Printf(TEXT("Compute Expr %d\n"), MinPrec));
		AST::FExpression* TernaryExpression = nullptr;
		auto Result = ComputeAtom(Scanner, /*Info,*/ SymbolScope, bAllowAssignment, Allocator, OutExpression, &TernaryExpression);
		if (Result != EParseResult::Matched)
		{
			return Result;
		}
		check(*OutExpression);
		do
		{
			auto* Token = Scanner.GetCurrentToken();
			int32 Precedence = GetPrecedence(Token);
			if (!Token || !IsBinaryOperator(Token) || Precedence < MinPrec || (!bAllowAssignment && IsAssignmentOperator(Token)) || (OutTernaryExpression && *OutTernaryExpression))
			{
				break;
			}

			Scanner.Advance();
			auto NextMinPrec = IsRightAssociative(Token) ? Precedence : Precedence + 1;
			AST::FExpression* RHSExpression = nullptr;
			AST::FExpression* RHSTernaryExpression = nullptr;
			Result = ComputeExpr(Scanner, NextMinPrec, /*Info,*/ SymbolScope, bAllowAssignment, Allocator, &RHSExpression, &RHSTernaryExpression);
			if (Result == EParseResult::Error)
			{
				return ParseResultError();
			}
			else if (Result == EParseResult::NotMatched)
			{
				break;
			}
			check(RHSExpression);
			auto BinaryOperator = AST::TokenToASTOperator(Token->Token);
			*OutExpression = new(Allocator) AST::FBinaryExpression(Allocator, BinaryOperator, *OutExpression, RHSExpression, Token->SourceInfo);
			if  (RHSTernaryExpression)
			{
				check(!TernaryExpression);
				TernaryExpression = RHSTernaryExpression;
				break;
			}
		}
		while (Scanner.HasMoreTokens());

		if (OriginalToken == Scanner.GetCurrentTokenIndex())
		{
			return EParseResult::NotMatched;
		}

		if (TernaryExpression)
		{
			if (!OutTernaryExpression)
			{
				if (!TernaryExpression->SubExpressions[0])
				{
					TernaryExpression->SubExpressions[0] = *OutExpression;
					*OutExpression = TernaryExpression;
				}
				else
				{
					check(0);
				}
			}
			else
			{
				*OutTernaryExpression = TernaryExpression;
			}
		}

		return EParseResult::Matched;
	}

	EParseResult ParseExpression(FHlslScanner& Scanner, FSymbolScope* SymbolScope, bool bAllowAssignment, FLinearAllocator* Allocator, AST::FExpression** OutExpression)
	{
		/*FInfo Info(!true);*/
		return ComputeExpr(Scanner, 0, /*Info,*/ SymbolScope, bAllowAssignment, Allocator, OutExpression, nullptr);
	}

	EParseResult ParseExpressionList(EHlslToken EndListToken, FHlslScanner& Scanner, FSymbolScope* SymbolScope, EHlslToken NewStartListToken, FLinearAllocator* Allocator, AST::FExpression* OutExpression)
	{
		check(OutExpression);
		while (Scanner.HasMoreTokens())
		{
			const auto* Token = Scanner.PeekToken();
			if (Token->Token == EndListToken)
			{
				Scanner.Advance();
				return EParseResult::Matched;
			}
			else if (NewStartListToken != EHlslToken::Invalid && Token->Token == NewStartListToken)
			{
				Scanner.Advance();
				auto* SubExpression = new(Allocator) AST::FInitializerListExpression(Allocator, Token->SourceInfo);
				auto Result = ParseExpressionList(EndListToken, Scanner, SymbolScope, NewStartListToken, Allocator, SubExpression);
				if (Result != EParseResult::Matched)
				{
					return Result;
				}

				OutExpression->Expressions.Add(SubExpression);
			}
			else
			{
				AST::FExpression* Expression = nullptr;
				auto Result = ParseExpression(Scanner, SymbolScope, true, Allocator, &Expression);
				if (Result == EParseResult::Error)
				{
					Scanner.SourceError(TEXT("Invalid expression list\n"));
					return ParseResultError();
				}
				else if (Result == EParseResult::NotMatched)
				{
					Scanner.SourceError(TEXT("Expected expression\n"));
					return ParseResultError();
				}

				OutExpression->Expressions.Add(Expression);
			}

			if (Scanner.MatchToken(EHlslToken::Comma))
			{
				continue;
			}
			else if (Scanner.MatchToken(EndListToken))
			{
				return EParseResult::Matched;
			}

			Scanner.SourceError(TEXT("Expected ','\n"));
			break;
		}

		return ParseResultError();
	}

	EParseResult ParseResultError()
	{
		// Extracted into a function so callstacks can be seen/debugged in the case of an error
		return EParseResult::Error;
	}
}
