// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/PointerIsConvertibleFromTo.h"
#include "Templates/UnrealTemplate.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Templates/Function.h"
#include "Containers/Set.h"
#include "Containers/Map.h"
#include "Misc/Optional.h"
#include "Internationalization/Text.h"
#include "Internationalization/Internationalization.h"
#include "Misc/Guid.h"
#include "Templates/ValueOrError.h"

class FExpressionNode;
struct FExpressionError;

namespace Impl
{
	struct IExpressionNodeStorage;
}

typedef TValueOrError<FExpressionNode, FExpressionError> FExpressionResult;

/** Simple error structure used for reporting parse errors */
struct FExpressionError
{
	FExpressionError(const FText& InText) : Text(InText) {}

	FText Text;
};

/** Simple struct that defines a specific token contained in an FTokenStream  */
class FStringToken
{
public:
	FStringToken() : TokenStart(nullptr), TokenEnd(nullptr), LineNumber(0), CharacterIndex(0) {}

	/** Get the string representation of this token */
	FString GetString() const { return FString(TokenEnd - TokenStart, TokenStart); }

	/** Check if this token is valid */
	bool IsValid() const { return TokenEnd != TokenStart; }

	/** Get the position of the start and end of this token in the stream */
	const TCHAR* GetTokenStartPos() const { return TokenStart; }
	const TCHAR* GetTokenEndPos() const { return TokenEnd; }

	/** Contextual information about this token */
	int32 GetCharacterIndex() const { return CharacterIndex; }
	int32 GetLineNumber() const { return LineNumber; }
	
	/** Accumulate another token into this one */
	void Accumulate(const FStringToken& InToken) { if (InToken.TokenEnd > TokenEnd) { TokenEnd = InToken.TokenEnd; } }

protected:
	friend class FTokenStream;

	FStringToken(const TCHAR* InStart, int32 Line = 0, int32 Character = 0)
		: TokenStart(InStart), TokenEnd(InStart), LineNumber(Line), CharacterIndex(Character)
	{}

	/** The start of the token */
	const TCHAR* TokenStart;
	/** The end of the token */
	const TCHAR* TokenEnd;
	/** Line number and Character index */
	int32 LineNumber, CharacterIndex;
};

/** Enum specifying how to treat the currently parsing character. */
enum class EParseState
{
	/** Include this character in the token and continue consuming */
	Continue,
	/** Include this character in the token and stop consuming */
	StopAfter,
	/** Exclude this character from the token and stop consuming */
	StopBefore, 
	/** Cancel parsing this token, and return nothing. */
	Cancel,
};

/** A token stream wraps up a raw string, providing accessors into it for consuming tokens */
class CORE_API FTokenStream
{
public:

	/**
	 * Parse out a token using the supplied predicate.
	 * Will keep consuming characters into the resulting token provided the predicate returns EParseState::Continue or EParseState::StopAfter.
	 * Optionally supply a token to accumulate into
	 * Returns a string token for the stream, or empty on error
	 */
	TOptional<FStringToken> ParseToken(TFunctionRef<EParseState(TCHAR)> Pred, FStringToken* Accumulate = nullptr) const;

	/** Attempt parse out the specified pre-defined string from the current read position (or accumulating into the specified existing token) */
	TOptional<FStringToken> ParseToken(const TCHAR* Symbol, FStringToken* Accumulate = nullptr) const;
	TOptional<FStringToken> ParseTokenIgnoreCase(const TCHAR* Symbol, FStringToken* Accumulate = nullptr) const;

	/** Return a string token for the next character in the stream (or accumulating into the specified existing token) */
	TOptional<FStringToken> ParseSymbol(FStringToken* Accumulate = nullptr) const;

	/** Attempt parse out the specified pre-defined string from the current read position (or accumulating into the specified existing token) */
	TOptional<FStringToken> ParseSymbol(TCHAR Symbol, FStringToken* Accumulate = nullptr) const;

	/** Parse a whitespace token */
	TOptional<FStringToken> ParseWhitespace(FStringToken* Accumulate = nullptr) const;

	/** Generate a token for the specified number of chars, at the current read position (or end of Accumulate) */
	TOptional<FStringToken> GenerateToken(int32 NumChars, FStringToken* Accumulate = nullptr) const;

public:

	/** Constructor. The stream is only valid for the lifetime of the string provided */
	FTokenStream(const TCHAR* In);
	
	/** Peek at the character at the specified offset from the current read position */
	TCHAR PeekChar(int32 Offset = 0) const;

	/** Get the number of characters remaining in the stream after the current read position */
	int32 CharsRemaining() const;

	/** Check if it is valid to read (the optional number of characters) from the specified position */
	bool IsReadPosValid(const TCHAR* InPos, int32 MinNumChars = 1) const;

	/** Check if the stream is empty */
	bool IsEmpty() const;

	/** Get the current read position from the start of the stream */
	int32 GetPosition() const;

	const TCHAR* GetStart() const { return Start; }
	const TCHAR* GetRead() const { return ReadPos; }
	const TCHAR* GetEnd() const { return End; }

	/** Get the error context from the current read position */
	FString GetErrorContext() const;

	/** Set the current read position to the character proceeding the specified token */
	void SetReadPos(const FStringToken& Token);

private:

	/** The start of the expression */
	const TCHAR* Start;
	/** The end of the expression */
	const TCHAR* End;
	/** The current read position in the expression */
	const TCHAR* ReadPos;
};

/** Helper macro to define the necessary template specialization for a particular expression node type */
/** Variable length arguments are passed the FGuid constructor. Must be unique per type */
#define DEFINE_EXPRESSION_NODE_TYPE(TYPE, ...) \
template<> struct TGetExpressionNodeTypeId<TYPE>\
{\
	static const FGuid& GetTypeId()\
	{\
		static FGuid Global(__VA_ARGS__);\
		return Global;\
	}\
};
template<typename T> struct TGetExpressionNodeTypeId;

/**
 * A node in an expression.
 * 	Can be constructed from any C++ type that has a corresponding DEFINE_EXPRESSION_NODE_TYPE.
 * 	Evaluation behaviour (unary/binary operator etc) is defined in the expression grammar, rather than the type itself.
 */
class CORE_API FExpressionNode : FNoncopyable
{
public:

	/** Default constructor */
	FExpressionNode() : TypeId() {}

	/** Construction from client expression data type */
	template<typename T>
	FExpressionNode(T In,
		/** @todo: make this a default function template parameter when VS2012 support goes */
		typename TEnableIf<!TPointerIsConvertibleFromTo<T, FExpressionNode>::Value>::Type* = nullptr
		);

	~FExpressionNode();

	/** Move construction/assignment */
	FExpressionNode(FExpressionNode&& In);
	FExpressionNode& operator=(FExpressionNode&& In);

	/** Get the type identifier of this node */
	const FGuid& GetTypeId() const;

	/** Cast this node to the specified type. Will return nullptr if the types do not match. */
	template<typename T>
	const T* Cast() const;

	/** Copy this node and its wrapped data */
	FExpressionNode Copy() const;

private:

	/** The maximum size of type we will allow allocation on the stack (for efficiency). Anything larger will be allocated on the heap. */
	static const uint32 MaxStackAllocationSize = 64 - sizeof(FGuid);

	/** Helper accessor to the data interface. Returns null for empty containers. */
	Impl::IExpressionNodeStorage* GetData();
	const Impl::IExpressionNodeStorage* GetData() const;

	/** TypeID - 16 bytes */
	FGuid TypeId;
	uint8 InlineBytes[MaxStackAllocationSize];
};

/** A specific token in a stream. Comprises an expression node, and the stream token it was created from */
class FExpressionToken : FNoncopyable
{
public:
	FExpressionToken(const FStringToken& InContext, FExpressionNode InNode)
		: Node(MoveTemp(InNode))
		, Context(InContext)
	{
	}

	FExpressionToken(FExpressionToken&& In) : Node(MoveTemp(In.Node)), Context(In.Context) {}
	FExpressionToken& operator=(FExpressionToken&& In) { Node = MoveTemp(In.Node); Context = In.Context; return *this; }

	FExpressionNode Node;
	FStringToken Context;
};

/** A compiled token, holding the token itself, and any compiler information required to evaluate it */
struct FCompiledToken : FExpressionToken
{
	// Todo: add callable types here?
	enum EType { Operand, PreUnaryOperator, PostUnaryOperator, BinaryOperator, Benign };

	FCompiledToken(EType InType, FExpressionToken InToken)
		: FExpressionToken(MoveTemp(InToken)), Type(InType)
	{}

	FCompiledToken(FCompiledToken&& In) : FExpressionToken(MoveTemp(In)), Type(In.Type) {}
	FCompiledToken& operator=(FCompiledToken&& In) { FExpressionToken::operator=(MoveTemp(In)); Type = In.Type; return *this; }

	EType Type;
};

/** Struct used to identify a function for a specific operator overload */
struct FOperatorFunctionID
{
	FGuid OperatorType;
	FGuid LeftOperandType;
	FGuid RightOperandType;

	friend bool operator==(const FOperatorFunctionID& A, const FOperatorFunctionID& B)
	{
		return A.OperatorType == B.OperatorType &&
			A.LeftOperandType == B.LeftOperandType &&
			A.RightOperandType == B.RightOperandType;
	}

	friend uint32 GetTypeHash(const FOperatorFunctionID& In)
	{
		const uint32 Hash = HashCombine(GetTypeHash(In.OperatorType), GetTypeHash(In.LeftOperandType));
		return HashCombine(GetTypeHash(In.RightOperandType), Hash);
	}
};

/** Jump table specifying how to execute an operator with different types */
template<typename ContextType=void>
struct TOperatorJumpTable
{
	/** Execute the specified token as a unary operator, if such an overload exists */
	FExpressionResult ExecPreUnary(const FExpressionToken& Operator, const FExpressionToken& R, const ContextType* Context) const;
	/** Execute the specified token as a unary operator, if such an overload exists */
	FExpressionResult ExecPostUnary(const FExpressionToken& Operator, const FExpressionToken& L, const ContextType* Context) const;
	/** Execute the specified token as a binary operator, if such an overload exists */
	FExpressionResult ExecBinary(const FExpressionToken& Operator, const FExpressionToken& L, const FExpressionToken& R, const ContextType* Context) const;

	/**
	 * Map an expression node to a pre-unary operator with the specified implementation.
	 *
	 * The callable type must match the declaration Ret(Operand[, Context]), where:
	 *	 	Ret 	= Any DEFINE_EXPRESSION_NODE_TYPE type, OR FExpressionResult
	 *	 	Operand = Any DEFINE_EXPRESSION_NODE_TYPE type
	 *	 	Context = (optional) const ptr to user-supplied arbitrary context
	 *
	 * Examples that binds a '!' token to a function that attempts to do a boolean 'not':
	 *		JumpTable.MapPreUnary<FExclamation>([](bool A){ return !A; });
	 *		JumpTable.MapPreUnary<FExclamation>([](bool A, FMyContext* Ctxt){ if (Ctxt->IsBooleanNotOpEnabled()) { return !A; } else { return A; } });
	 *		JumpTable.MapPreUnary<FExclamation>([](bool A, const FMyContext* Ctxt) -> FExpressionResult {

	 *			if (Ctxt->IsBooleanNotOpEnabled())
	 *			{
	 *				return MakeValue(!A);
	 *			}
	 *			return MakeError(FExpressionError(LOCTEXT("NotNotEnabled", "Boolean not is not enabled.")));
	 *		});
	 */
	template<typename OperatorType, typename FuncType>
	void MapPreUnary(FuncType InFunc);

	/**
	 * Map an expression node to a post-unary operator with the specified implementation.
	 * The same function signature rules apply here as with MapPreUnary.
	 */
	template<typename OperatorType, typename FuncType>
	void MapPostUnary(FuncType InFunc);

	/**
	 * Map an expression node to a binary operator with the specified implementation.
	 *
	 * The callable type must match the declaration Ret(OperandL, OperandR, [, Context]), where:
	 *	 	Ret 		= Any DEFINE_EXPRESSION_NODE_TYPE type, OR FExpressionResult
	 *	 	OperandL	= Any DEFINE_EXPRESSION_NODE_TYPE type
	 *	 	OperandR	= Any DEFINE_EXPRESSION_NODE_TYPE type
	 *	 	Context 	= (optional) const ptr to user-supplied arbitrary context
	 *
	 * Examples that binds a '/' token to a function that attempts to do a division:
	 *		JumpTable.MapUnary<FForwardSlash>([](double A, double B){ return A / B; }); // Runtime exception on div/0 
	 *		JumpTable.MapUnary<FForwardSlash>([](double A, double B, FMyContext* Ctxt){
	 *			if (!Ctxt->IsMathEnabled())
	 *			{
	 *				return A;
	 *			}
	 *			return A / B; // Runtime exception on div/0 
	 *		});
	 *		JumpTable.MapUnary<FForwardSlash>([](double A, double B, const FMyContext* Ctxt) -> FExpressionResult {
	 *			if (!Ctxt->IsMathEnabled())
	 *			{
	 *				return MakeError(FExpressionError(LOCTEXT("MathNotEnabled", "Math is not enabled.")));
	 *			}
	 *			else if (B == 0)
	 *			{
	 *				return MakeError(FExpressionError(LOCTEXT("DivisionByZero", "Division by zero.")));	
	 *			}
	 *
	 *			return MakeValue(!A);
	 *		});
	 */
	template<typename OperatorType, typename FuncType>
	void MapBinary(FuncType InFunc);

public:

	typedef TFunction<FExpressionResult(const FExpressionNode&, const ContextType* Context)> FUnaryFunction;
	typedef TFunction<FExpressionResult(const FExpressionNode&, const FExpressionNode&, const ContextType* Context)> FBinaryFunction;

private:

	/** Maps of unary/binary operators */
	TMap<FOperatorFunctionID, FUnaryFunction> PreUnaryOps;
	TMap<FOperatorFunctionID, FUnaryFunction> PostUnaryOps;
	TMap<FOperatorFunctionID, FBinaryFunction> BinaryOps;
};

typedef TOperatorJumpTable<> FOperatorJumpTable;

/** Structures used for managing the evaluation environment for operators in an expression. This class manages the evaluation context
 * to avoid templating the whole evaluation code on a context type
 */
struct IOperatorEvaluationEnvironment
{
	/** Execute the specified token as a unary operator, if such an overload exists */
	virtual FExpressionResult ExecPreUnary(const FExpressionToken& Operator, const FExpressionToken& R) const = 0;
	/** Execute the specified token as a unary operator, if such an overload exists */
	virtual FExpressionResult ExecPostUnary(const FExpressionToken& Operator, const FExpressionToken& L) const = 0;
	/** Execute the specified token as a binary operator, if such an overload exists */
	virtual FExpressionResult ExecBinary(const FExpressionToken& Operator, const FExpressionToken& L, const FExpressionToken& R) const = 0;
};
template<typename ContextType = void>
struct TOperatorEvaluationEnvironment : IOperatorEvaluationEnvironment
{
	TOperatorEvaluationEnvironment(const TOperatorJumpTable<ContextType>& InOperators, const ContextType* InContext)
		: Operators(InOperators), Context(InContext)
	{}

	virtual FExpressionResult ExecPreUnary(const FExpressionToken& Operator, const FExpressionToken& R) const override
	{
		return Operators.ExecPreUnary(Operator, R, Context);
	}
	virtual FExpressionResult ExecPostUnary(const FExpressionToken& Operator, const FExpressionToken& L) const override
	{
		return Operators.ExecPostUnary(Operator, L, Context);
	}
	virtual FExpressionResult ExecBinary(const FExpressionToken& Operator, const FExpressionToken& L, const FExpressionToken& R) const override
	{
		return Operators.ExecBinary(Operator, L, R, Context);
	}

private:
	const TOperatorJumpTable<ContextType>& Operators;
	const ContextType* Context;
};

typedef TOperatorEvaluationEnvironment<> FOperatorEvaluationEnvironment;

/** Class used to consume tokens from a string */
class CORE_API FExpressionTokenConsumer
{
public:
	/** Construction from a raw string. The consumer is only valid as long as the string is valid */
	FExpressionTokenConsumer(const TCHAR* InExpression);

	/** Extract the list of tokens from this consumer */
	TArray<FExpressionToken> Extract();

	/** Add an expression node to the consumer, specifying the FStringToken this node relates to.
	 *	Adding a node to the consumer will move its stream read position to the end of the added token.
	 */
	void Add(const FStringToken& SourceToken, FExpressionNode&& Node);

	/** Get the expression stream */
	FTokenStream& GetStream() { return Stream; }

private:
	FExpressionTokenConsumer(const FExpressionTokenConsumer&);
	FExpressionTokenConsumer& operator=(const FExpressionTokenConsumer&);

	/** Array of added tokens */
	TArray<FExpressionToken> Tokens;

	/** Stream that looks at the constructed expression */
	FTokenStream Stream;
};

/** 
 * Typedef that defines a function used to consume tokens
 * 	Definitions may add FExpressionNodes parsed from the provided consumer's stream, or return an optional error.
 *	Where a definition performs no mutable operations, subsequent token definitions will be invoked.
 */
typedef TOptional<FExpressionError> (FExpressionDefinition)(FExpressionTokenConsumer&);


/** A lexeme dictionary defining how to lex an expression. */
class CORE_API FTokenDefinitions
{
public:
	FTokenDefinitions() : bIgnoreWhitespace(false) {}

	/** Define the grammar to ignore whitespace between tokens, unless explicitly included in a token */
	void IgnoreWhitespace() { bIgnoreWhitespace = true; }

	/** Define a token by way of a function to be invoked to attempt to parse a token from a stream */
	void DefineToken(TFunction<FExpressionDefinition>&& Definition);

public:

	/** Check if the grammar ignores whitespace */
	bool DoesIgnoreWhitespace() { return bIgnoreWhitespace; }

	/** Consume a token for the specified consumer */
	TOptional<FExpressionError> ConsumeTokens(FExpressionTokenConsumer& Consumer) const;

private:

	TOptional<FExpressionError> ConsumeToken(FExpressionTokenConsumer& Consumer) const;

private:

	bool bIgnoreWhitespace;
	TArray<TFunction<FExpressionDefinition>> Definitions;
};


/**
 * Enum specifying the associativity (order of execution) for binary operators
 */
enum class EAssociativity
{
	RightToLeft,
	LeftToRight
};

/**
 * Struct for storing binary operator definition parameters
 */
struct FOpParameters
{
	/** The precedence of the operator */
	int32			Precedence;

	/** The associativity of the operator */
	EAssociativity	Associativity;

	FOpParameters(int32 InPrecedence, EAssociativity InAssociativity)
		: Precedence(InPrecedence)
		, Associativity(InAssociativity)
	{
	}
};

/** A lexical gammer defining how to parse an expression. Clients must define the tokens and operators to be interpreted by the parser. */
class CORE_API FExpressionGrammar
{
public:
	/** Define a grouping operator from two expression node types */
	template<typename TStartGroup, typename TEndGroup>
	void DefineGrouping() { Groupings.Add(TGetExpressionNodeTypeId<TStartGroup>::GetTypeId(), TGetExpressionNodeTypeId<TEndGroup>::GetTypeId()); }

	/** Define a pre-unary operator for the specified symbol */
	template<typename TExpressionNode>
	void DefinePreUnaryOperator() { PreUnaryOperators.Add(TGetExpressionNodeTypeId<TExpressionNode>::GetTypeId()); }

	/** Define a post-unary operator for the specified symbol */
	template<typename TExpressionNode>
	void DefinePostUnaryOperator() { PostUnaryOperators.Add(TGetExpressionNodeTypeId<TExpressionNode>::GetTypeId()); }

	/**
	 * Define a binary operator for the specified symbol, with the specified precedence and associativity
	 * NOTE: Associativity defaults to RightToLeft for legacy reasons.
	 *
	 * @param InPrecedence		The precedence (priority of execution) this operator should have
	 * @param InAssociativity	With operators of the same precedence, determines whether they execute left to right, or right to left
	 */
	template<typename TExpressionNode>
	void DefineBinaryOperator(int32 InPrecedence, EAssociativity InAssociativity=EAssociativity::RightToLeft)
	{
#if DO_CHECK
		for (TMap<FGuid, FOpParameters>::TConstIterator It(BinaryOperators); It; ++It)
		{
			const FOpParameters& CurValue = It.Value();

			if (CurValue.Precedence == InPrecedence)
			{
				// Operators of the same precedence, must all have the same associativity
				check(CurValue.Associativity == InAssociativity);
			}
		}
#endif

		BinaryOperators.Add(TGetExpressionNodeTypeId<TExpressionNode>::GetTypeId(), FOpParameters(InPrecedence, InAssociativity));
	}

public:

	/** Retrieve the corresponding grouping token for the specified open group type, or nullptr if it's not a group token */
	const FGuid* GetGrouping(const FGuid& TypeId) const;

	/** Check if this grammar defines a pre-unary operator for the specified symbol */
	bool HasPreUnaryOperator(const FGuid& TypeId) const;
	
	/** Check if this grammar defines a post-unary operator for the specified symbol */
	bool HasPostUnaryOperator(const FGuid& TypeId) const;

	/** Get the binary operator precedence and associativity parameters, for the specified symbol, if any */
	const FOpParameters* GetBinaryOperatorDefParameters(const FGuid& TypeId) const;

private:

	TMap<FGuid, FGuid>			Groupings;
	TSet<FGuid>					PreUnaryOperators;
	TSet<FGuid>					PostUnaryOperators;
	TMap<FGuid, FOpParameters>	BinaryOperators;
};

#include "Misc/ExpressionParserTypes.inl"
