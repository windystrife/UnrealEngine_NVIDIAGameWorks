// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HlslUtils.cpp - Utils for HLSL.
=============================================================================*/

#include "HlslUtils.h"
#include "Misc/ScopeLock.h"
#include "HlslAST.h"
#include "HlslParser.h"
#include "ShaderCompilerCommon.h"

static bool bLeaveAllUsed = false;

namespace CrossCompiler
{
	namespace Memory
	{
#if USE_PAGE_POOLING
		static struct FPagePoolInstance
		{
			~FPagePoolInstance()
			{
				check(UsedPages.Num() == 0);
				for (int32 Index = 0; Index < FreePages.Num(); ++Index)
				{
					delete FreePages[Index];
				}
			}

			FPage* AllocatePage(SIZE_T PageSize)
			{
				FScopeLock ScopeLock(&CriticalSection);

				if (FreePages.Num() == 0)
				{
					FreePages.Add(new FPage(PageSize));
				}

				auto* Page = FreePages.Last();
				FreePages.RemoveAt(FreePages.Num() - 1, 1, false);
				UsedPages.Add(Page);
				return Page;
			}

			void FreePage(FPage* Page)
			{
				FScopeLock ScopeLock(&CriticalSection);

				int32 Index = UsedPages.Find(Page);
				check(Index >= 0);
				UsedPages.RemoveAt(Index, 1, false);
				FreePages.Add(Page);
			}

			TArray<FPage*, TInlineAllocator<8> > FreePages;
			TArray<FPage*, TInlineAllocator<8> > UsedPages;

			FCriticalSection CriticalSection;

		} GMemoryPagePool;
#endif

		FPage* FPage::AllocatePage(SIZE_T PageSize)
		{
#if USE_PAGE_POOLING
			return GMemoryPagePool.AllocatePage();
#else
			return new FPage(PageSize);
#endif
		}

		void FPage::FreePage(FPage* Page)
		{
#if USE_PAGE_POOLING
			GMemoryPagePool.FreePage(Page);
#else
			delete Page;
#endif
		}
	}
}

// Returns "TEXCOORD" if Semantic is "TEXCOORD4" and OutStartIndex=4; return nullptr if the semantic didn't have a number
static inline TCHAR* GetNonDigitSemanticPrefix(CrossCompiler::FLinearAllocator* Allocator, const TCHAR* Semantic, uint32& OutStartIndex)
{
	const TCHAR* StartOfDigit = Semantic;
	do 
	{
		if (*StartOfDigit >= '0' && *StartOfDigit <= '9')
		{
			break;
		}
		++StartOfDigit;
	}
	while (*StartOfDigit);

	if (!*StartOfDigit)
	{
		return nullptr;
	}

	OutStartIndex = FCString::Atoi(StartOfDigit);
	TCHAR* Prefix = Allocator->Strdup(Semantic);
	Prefix[StartOfDigit - Semantic] = 0;
	return Prefix;
}


static inline TCHAR* MakeIndexedSemantic(CrossCompiler::FLinearAllocator* Allocator, const TCHAR* Semantic, uint32 Index)
{
	FString Out = FString::Printf(TEXT("%s%d"), Semantic, Index);
	return Allocator->Strdup(Out);
}

struct FRemoveAlgorithm
{
	FString EntryPoint;
	bool bSuccess;
	FString GeneratedCode;
	TArray<FString> Errors;
	CrossCompiler::FLinearAllocator* Allocator;
	CrossCompiler::FSourceInfo SourceInfo;

	TArray<FString> RemovedSemantics;

	struct FBodyContext
	{
		TArray<CrossCompiler::AST::FStructSpecifier*> NewStructs;

		// Instructions before calling the original function
		TArray<CrossCompiler::AST::FNode*> PreInstructions;

		// Call to the original function
		CrossCompiler::AST::FFunctionExpression* CallToOriginalFunction;

		// Instructions after calling the original function
		TArray<CrossCompiler::AST::FNode*> PostInstructions;

		// Final instruction
		CrossCompiler::AST::FNode* FinalInstruction;

		// Parameter of the new entry point
		TArray<CrossCompiler::AST::FParameterDeclarator*> NewFunctionParameters;

		FBodyContext() :
			CallToOriginalFunction(nullptr),
			FinalInstruction(nullptr)
		{
		}
	};

	FRemoveAlgorithm() :
		bSuccess(false),
		Allocator(nullptr)
	{
	}

	static CrossCompiler::AST::FUnaryExpression* MakeIdentifierExpression(CrossCompiler::FLinearAllocator* Allocator, const TCHAR* Name, const CrossCompiler::FSourceInfo& SourceInfo)
	{
		using namespace CrossCompiler::AST;
		FUnaryExpression* Expression = new(Allocator)FUnaryExpression(Allocator, EOperators::Identifier, nullptr, SourceInfo);
		Expression->Identifier = Allocator->Strdup(Name);
		return Expression;
	}

	CrossCompiler::AST::FFunctionDefinition* FindEntryPointAndPopulateSymbolTable(CrossCompiler::TLinearArray<CrossCompiler::AST::FNode*>& ASTNodes, TArray<CrossCompiler::AST::FStructSpecifier*>& OutMiniSymbolTable, FString* OutOptionalWriteNodes)
	{
		using namespace CrossCompiler::AST;
		FFunctionDefinition* EntryFunction = nullptr;
		for (int32 Index = 0; Index < ASTNodes.Num(); ++Index)
		{
			auto* Node = ASTNodes[Index];
			if (FDeclaratorList* DeclaratorList = Node->AsDeclaratorList())
			{
				// Skip unnamed structures
				if (DeclaratorList->Type->Specifier->Structure && DeclaratorList->Type->Specifier->Structure->Name)
				{
					OutMiniSymbolTable.Add(DeclaratorList->Type->Specifier->Structure);
				}
			}
			else if (FFunctionDefinition* FunctionDefinition = Node->AsFunctionDefinition())
			{
				if (FCString::Strcmp(*EntryPoint, FunctionDefinition->Prototype->Identifier) == 0)
				{
					EntryFunction = FunctionDefinition;
				}
			}

			if (OutOptionalWriteNodes)
			{
				FASTWriter Writer(*OutOptionalWriteNodes);
				Node->Write(Writer);
			}
		}

		return EntryFunction;
	}

	CrossCompiler::AST::FFullySpecifiedType* CloneType(CrossCompiler::AST::FFullySpecifiedType* InType, bool bStripInOut = true)
	{
		auto* New = new(Allocator)CrossCompiler::AST::FFullySpecifiedType(Allocator, SourceInfo);
		New->Qualifier = InType->Qualifier;
		if (bStripInOut)
		{
			New->Qualifier.bIn = false;
			New->Qualifier.bOut = false;
		}
		New->Specifier = InType->Specifier;
		return New;
	}

	CrossCompiler::AST::FStructSpecifier* CreateNewStructSpecifier(const TCHAR* TypeName, TArray<CrossCompiler::AST::FStructSpecifier*>& NewStructs)
	{
		auto* NewReturnType = new(Allocator) CrossCompiler::AST::FStructSpecifier(Allocator, SourceInfo);
		NewReturnType->Name = Allocator->Strdup(TypeName);
		NewStructs.Add(NewReturnType);
		return NewReturnType;
	}

	CrossCompiler::AST::FFunctionDefinition* CreateNewEntryFunction(CrossCompiler::AST::FCompoundStatement* Body, CrossCompiler::AST::FFullySpecifiedType* ReturnType, TArray<CrossCompiler::AST::FParameterDeclarator*>& Parameters)
	{
		using namespace CrossCompiler::AST;
		// New Entry definition/prototype
		FFunctionDefinition* NewEntryFunction = new(Allocator) FFunctionDefinition(Allocator, SourceInfo);
		NewEntryFunction->Prototype = new(Allocator) FFunction(Allocator, SourceInfo);
		NewEntryFunction->Prototype->Identifier = Allocator->Strdup(*(EntryPoint + TEXT("__OPTIMIZED")));
		NewEntryFunction->Prototype->ReturnType = ReturnType;
		NewEntryFunction->Body = Body;
		for (auto* Parameter : Parameters)
		{
			NewEntryFunction->Prototype->Parameters.Add(Parameter);
		}

		return NewEntryFunction;
	}

	CrossCompiler::AST::FFullySpecifiedType* MakeSimpleType(const TCHAR* Name)
	{
		auto* ReturnType = new(Allocator) CrossCompiler::AST::FFullySpecifiedType(Allocator, SourceInfo);
		ReturnType->Specifier = new(Allocator) CrossCompiler::AST::FTypeSpecifier(Allocator, SourceInfo);
		ReturnType->Specifier->TypeName = Allocator->Strdup(Name);
		return ReturnType;
	};

	CrossCompiler::AST::FStructSpecifier* FindStructSpecifier(TArray<CrossCompiler::AST::FStructSpecifier*>& MiniSymbolTable, const TCHAR* StructName)
	{
		for (auto* StructSpecifier : MiniSymbolTable)
		{
			if (!FCString::Strcmp(StructSpecifier->Name, StructName))
			{
				return StructSpecifier;
			}
		}

		return nullptr;
	}

	// Case-insensitive when working with Semantics
	static bool IsStringInArray(const TArray<FString>& Array, const TCHAR* Semantic)
	{
		for (const FString& String : Array)
		{
			if (FCString::Stricmp(*String, Semantic) == 0)
			{
				return true;
			}
		}

		return false;
	};

	static bool IsSubstringInArray(const TArray<FString>& Array, const TCHAR* Semantic)
	{
		for (const FString& String : Array)
		{
			if (FCString::Stristr(*String, Semantic) != NULL)
			{
				return true;
			}
		}

		return false;
	};

	bool CopyMember(CrossCompiler::AST::FDeclaration* Declaration, const TCHAR* DestPrefix, const TCHAR* SourcePrefix, TArray<CrossCompiler::AST::FNode*>& InstructionList)
	{
		using namespace CrossCompiler::AST;

		// Add copy statement(s)
		FString LHSName = DestPrefix;
		LHSName += '.';
		LHSName += Declaration->Identifier;
		FString RHSName = SourcePrefix;
		RHSName += '.';
		RHSName += Declaration->Identifier;

		if (Declaration->bIsArray)
		{
			uint32 ArrayLength = 0;
			if (!GetArrayLength(Declaration, ArrayLength))
			{
				return false;
			}

			for (uint32 Index = 0; Index < ArrayLength; ++Index)
			{
				FString LHSElement = FString::Printf(TEXT("%s[%d]"), *LHSName, Index);
				FString RHSElement = FString::Printf(TEXT("%s[%d]"), *RHSName, Index);
				auto* LHS = MakeIdentifierExpression(Allocator, *LHSElement, SourceInfo);
				auto* RHS = MakeIdentifierExpression(Allocator, *RHSElement, SourceInfo);
				auto* Assignment = new(Allocator) FBinaryExpression(Allocator, EOperators::Assign, LHS, RHS, SourceInfo);
				InstructionList.Add(new(Allocator) FExpressionStatement(Allocator, Assignment, SourceInfo));
			}
		}
		else
		{
			auto* LHS = MakeIdentifierExpression(Allocator, *LHSName, SourceInfo);
			auto* RHS = MakeIdentifierExpression(Allocator, *RHSName, SourceInfo);
			auto* Assignment = new(Allocator) FBinaryExpression(Allocator, EOperators::Assign, LHS, RHS, SourceInfo);
			InstructionList.Add(new(Allocator) FExpressionStatement(Allocator, Assignment, SourceInfo));
		}

		return true;
	}

	bool CheckSimpleVectorType(const TCHAR* SimpleType)
	{
		if (!FCString::Strncmp(SimpleType, TEXT("float"), 5))
		{
			SimpleType += 5;
		}
		else if (!FCString::Strncmp(SimpleType, TEXT("int"), 3))
		{
			SimpleType += 3;
		}
		else if (!FCString::Strncmp(SimpleType, TEXT("half"), 4))
		{
			SimpleType += 4;
		}
		else
		{
			return false;
		}

		return FChar::IsDigit(SimpleType[0]) && SimpleType[1] == 0;
	}

	CrossCompiler::AST::FDeclaratorList* CreateLocalVariable(const TCHAR* Type, const TCHAR* Name, CrossCompiler::AST::FExpression* Initializer = nullptr)
	{
		using namespace CrossCompiler::AST;
		auto* LocalVarDeclaratorList = new(Allocator) FDeclaratorList(Allocator, SourceInfo);
		LocalVarDeclaratorList->Type = MakeSimpleType(Type);
		auto* LocalVarDeclaration = new(Allocator) FDeclaration(Allocator, SourceInfo);
		LocalVarDeclaration->Identifier = Allocator->Strdup(Name);
		LocalVarDeclaration->Initializer = Initializer;
		LocalVarDeclaratorList->Declarations.Add(LocalVarDeclaration);
		return LocalVarDeclaratorList;
	}

	CrossCompiler::AST::FCompoundStatement* AddStatementsToBody(FBodyContext& Return, CrossCompiler::AST::FNode* CallInstruction)
	{
		CrossCompiler::AST::FCompoundStatement* Body = new(Allocator)CrossCompiler::AST::FCompoundStatement(Allocator, SourceInfo);
		for (auto* Instruction : Return.PreInstructions)
		{
			Body->Statements.Add(Instruction);
		}

		if (CallInstruction)
		{
			Body->Statements.Add(CallInstruction);
		}

		for (auto* Instruction : Return.PostInstructions)
		{
			Body->Statements.Add(Instruction);
		}

		if (Return.FinalInstruction)
		{
			Body->Statements.Add(Return.FinalInstruction);
		}

		return Body;
	}


	bool GetArrayLength(CrossCompiler::AST::FDeclaration* A, uint32& OutLength)
	{
		using namespace CrossCompiler::AST;
		if (!A->bIsArray)
		{
			Errors.Add(FString::Printf(TEXT("RemoveUnusedOutputs: %s is expected to be an array!"), A->Identifier));
			return false;
		}
		else
		{
			if (A->ArraySize.Num() > 1)
			{
				Errors.Add(FString::Printf(TEXT("RemoveUnusedOutputs: No support for multidimensional arrays on %s!"), A->Identifier));
				return false;
			}

			for (int32 Index = 0; Index < A->ArraySize.Num(); ++Index)
			{
				int32 DimA = 0;
				if (!A->ArraySize[Index]->GetConstantIntValue(DimA))
				{
					Errors.Add(FString::Printf(TEXT("RemoveUnusedOutputs: Array %s is not a compile-time constant expression!"), A->Identifier));
					return false;
				}

				OutLength = DimA;
			}
		}

		return true;
	}
};

struct FRemoveUnusedOutputs : FRemoveAlgorithm
{
	const TArray<FString>& UsedOutputs;
	const TArray<FString>& Exceptions;

	struct FOutputsBodyContext : FBodyContext
	{
		CrossCompiler::AST::FStructSpecifier* NewReturnStruct;

		// Expression (might be assignment) calling CallToOriginalFunction
		CrossCompiler::AST::FExpression* CallExpression;

		const TCHAR* ReturnVariableName;
		const TCHAR* ReturnTypeName;

		// Parameter of the new entry point
		TArray<CrossCompiler::AST::FParameterDeclarator*> NewFunctionParameters;

		FOutputsBodyContext() :
			NewReturnStruct(nullptr),
			CallExpression(nullptr),
			ReturnVariableName(TEXT("OptimizedReturn")),
			ReturnTypeName(TEXT("FOptimizedReturn"))
		{
		}
	};

	FRemoveUnusedOutputs(const TArray<FString>& InUsedOutputs, const TArray<FString>& InExceptions) :
		UsedOutputs(InUsedOutputs),
		Exceptions(InExceptions)
	{
	}

	bool SetupReturnType(CrossCompiler::AST::FFunctionDefinition* EntryFunction, TArray<CrossCompiler::AST::FStructSpecifier*>& MiniSymbolTable, FOutputsBodyContext& OutReturn)
	{
		using namespace CrossCompiler::AST;

		// Create the new return type, local variable and the final return statement
		{
			// New return type
			OutReturn.NewReturnStruct = CreateNewStructSpecifier(OutReturn.ReturnTypeName, OutReturn.NewStructs);

			// Local Variable
			OutReturn.PreInstructions.Add(CreateLocalVariable(OutReturn.NewReturnStruct->Name, OutReturn.ReturnVariableName));

			// Return Statement
			auto* ReturnStatement = new(Allocator) FJumpStatement(Allocator, EJumpType::Return, SourceInfo);
			ReturnStatement->OptionalExpression = MakeIdentifierExpression(Allocator, OutReturn.ReturnVariableName, SourceInfo);
			OutReturn.FinalInstruction = ReturnStatement;
		}

		auto* ReturnType = EntryFunction->Prototype->ReturnType;
		if (ReturnType && ReturnType->Specifier && ReturnType->Specifier->TypeName)
		{
			const TCHAR* ReturnTypeName = ReturnType->Specifier->TypeName;
			if (!EntryFunction->Prototype->ReturnSemantic && !FCString::Strcmp(ReturnTypeName, TEXT("void")))
			{
				return true;
			}
			else
			{
				// Confirm this is a struct living in the symbol table
				FStructSpecifier* OriginalStructSpecifier = FindStructSpecifier(MiniSymbolTable, ReturnTypeName);
				if (OriginalStructSpecifier)
				{
					return ProcessStructReturnType(OriginalStructSpecifier, MiniSymbolTable, OutReturn);
				}
				else if (CheckSimpleVectorType(ReturnTypeName))
				{
					if (EntryFunction->Prototype->ReturnSemantic)
					{
						ProcessSimpleReturnType(ReturnTypeName, EntryFunction->Prototype->ReturnSemantic, OutReturn);
						return true;
					}
					else
					{
						Errors.Add(FString::Printf(TEXT("RemoveUnusedOutputs: Function %s with return type %s doesn't have a return semantic"), *EntryPoint, ReturnTypeName));
					}
				}
				else
				{
					Errors.Add(FString::Printf(TEXT("RemoveUnusedOutputs: Invalid return type %s for function %s"), ReturnTypeName, *EntryPoint));
				}
			}
		}
		else
		{
			Errors.Add(FString::Printf(TEXT("RemoveUnusedOutputs: Internal error trying to determine return type")));
		}

		return false;
	};

	void RemoveUnusedOutputs(CrossCompiler::TLinearArray<CrossCompiler::AST::FNode*>& ASTNodes)
	{
		using namespace CrossCompiler::AST;

		// Find Entry point from original AST nodes
		TArray<FStructSpecifier*> MiniSymbolTable;
		FString Test;
		FFunctionDefinition* EntryFunction = FindEntryPointAndPopulateSymbolTable(ASTNodes, MiniSymbolTable, &Test);
		if (!EntryFunction)
		{
			Errors.Add(FString::Printf(TEXT("RemoveUnusedOutputs: Unable to find entry point %s"), *EntryPoint));
			bSuccess = false;
			return;
		}
		//FPlatformMisc::LowLevelOutputDebugString(*Test);

		SourceInfo = EntryFunction->SourceInfo;

		FOutputsBodyContext BodyContext;

		// Setup the call to the original entry point
		BodyContext.CallToOriginalFunction = new(Allocator) FFunctionExpression(Allocator, SourceInfo, MakeIdentifierExpression(Allocator, *EntryPoint, SourceInfo));

		if (!SetupReturnType(EntryFunction, MiniSymbolTable, BodyContext))
		{
			bSuccess = false;
			return;
		}

		if (!ProcessOriginalParameters(EntryFunction, MiniSymbolTable, BodyContext))
		{
			bSuccess = false;
			return;
		}

		// Real call statement
		if (BodyContext.CallToOriginalFunction && !BodyContext.CallExpression)
		{
			BodyContext.CallExpression = BodyContext.CallToOriginalFunction;
		}
		auto* CallInstruction = new(Allocator) CrossCompiler::AST::FExpressionStatement(Allocator, BodyContext.CallExpression, SourceInfo);

		FCompoundStatement* Body = AddStatementsToBody(BodyContext, CallInstruction);
		FFunctionDefinition* NewEntryFunction = CreateNewEntryFunction(Body, MakeSimpleType(BodyContext.NewReturnStruct->Name), BodyContext.NewFunctionParameters);
		EntryPoint = NewEntryFunction->Prototype->Identifier;
		WriteGeneratedOutCode(NewEntryFunction, BodyContext.NewStructs, GeneratedCode);
		bSuccess = true;
	}

	void WriteGeneratedOutCode(CrossCompiler::AST::FFunctionDefinition* NewEntryFunction, TArray<CrossCompiler::AST::FStructSpecifier*>& NewStructs, FString& OutGeneratedCode)
	{
		CrossCompiler::AST::FASTWriter Writer(OutGeneratedCode);
		GeneratedCode = TEXT("#line 1 \"RemoveUnusedOutputs.usf\"\n// Generated Entry Point: ");
		GeneratedCode += NewEntryFunction->Prototype->Identifier;
		GeneratedCode += TEXT("\n");
		if (UsedOutputs.Num() > 0)
		{
			GeneratedCode += TEXT("// Requested UsedOutputs:");
			for (int32 Index = 0; Index < UsedOutputs.Num(); ++Index)
			{
				GeneratedCode += (Index == 0) ? TEXT(" ") : TEXT(", ");
				GeneratedCode += UsedOutputs[Index];
			}
			GeneratedCode += TEXT("\n");
		}
		if (RemovedSemantics.Num() > 0)
		{
			GeneratedCode += TEXT("// Removed Outputs:");
			for (int32 Index = 0; Index < RemovedSemantics.Num(); ++Index)
			{
				GeneratedCode += (Index == 0) ? TEXT(" ") : TEXT(", ");
				GeneratedCode += RemovedSemantics[Index];
			}
			GeneratedCode += TEXT("\n");
		}
		for (auto* Struct : NewStructs)
		{
			auto* Declarator = new(Allocator) CrossCompiler::AST::FDeclaratorList(Allocator, SourceInfo);
			Declarator->Declarations.Add(Struct);
			Declarator->Write(Writer);
		}
		NewEntryFunction->Write(Writer);
		//FPlatformMisc::LowLevelOutputDebugStringf(TEXT("*********************************\n%s\n"), *GeneratedCode);
	}

	void ProcessSimpleOutParameter(CrossCompiler::AST::FParameterDeclarator* ParameterDeclarator, FOutputsBodyContext& BodyContext)
	{
		using namespace CrossCompiler::AST;

		// Only add the parameter if it needs to also be returned
		bool bRequiresToBeInReturnStruct = IsSemanticUsed(ParameterDeclarator->Semantic);

		if (bRequiresToBeInReturnStruct)
		{
			// Add the member to the return struct
			auto* MemberDeclaratorList = new(Allocator) FDeclaratorList(Allocator, SourceInfo);
			MemberDeclaratorList->Type = CloneType(ParameterDeclarator->Type);
			auto* MemberDeclaration = new(Allocator) FDeclaration(Allocator, SourceInfo);
			MemberDeclaration->Identifier = ParameterDeclarator->Identifier;
			MemberDeclaration->Semantic = ParameterDeclarator->Semantic;
			MemberDeclaratorList->Declarations.Add(MemberDeclaration);

			// Add it to the return struct type
			check(BodyContext.NewReturnStruct);
			BodyContext.NewReturnStruct->Declarations.Add(MemberDeclaratorList);

			// Add the parameter to the actual function call
			FString ParameterName = BodyContext.ReturnVariableName;
			ParameterName += TEXT(".");
			ParameterName += ParameterDeclarator->Identifier;
			auto* Parameter = MakeIdentifierExpression(Allocator, *ParameterName, SourceInfo);
			BodyContext.CallToOriginalFunction->Expressions.Add(Parameter);
		}
		else
		{
			// Make a local to receive the out parameter
			auto* LocalVar = CreateLocalVariable(ParameterDeclarator->Type->Specifier->TypeName, ParameterDeclarator->Identifier);
			BodyContext.PreInstructions.Add(LocalVar);

			// Add the parameter to the actual function call
			auto* Parameter = MakeIdentifierExpression(Allocator, ParameterDeclarator->Identifier, SourceInfo);
			BodyContext.CallToOriginalFunction->Expressions.Add(Parameter);
		}
	}

	void ProcessSimpleReturnType(const TCHAR* TypeName, const TCHAR* Semantic, FOutputsBodyContext& BodyContext)
	{
		using namespace CrossCompiler::AST;

		// Create a member to return this simple type out
		auto* MemberDeclaratorList = new(Allocator) FDeclaratorList(Allocator, SourceInfo);
		MemberDeclaratorList->Type = MakeSimpleType(TypeName);
		auto* MemberDeclaration = new(Allocator) FDeclaration(Allocator, SourceInfo);
		MemberDeclaration->Identifier = TEXT("SimpleReturn");
		MemberDeclaration->Semantic = Semantic;
		MemberDeclaratorList->Declarations.Add(MemberDeclaration);

		// Add it to the return struct type
		check(BodyContext.NewReturnStruct);
		BodyContext.NewReturnStruct->Declarations.Add(MemberDeclaratorList);

		// Create the LHS of the member assignment
		FString MemberName = BodyContext.ReturnVariableName;
		MemberName += TEXT(".");
		MemberName += MemberDeclaration->Identifier;
		auto* SimpleTypeMember = MakeIdentifierExpression(Allocator, *MemberName, SourceInfo);

		// Create an assignment from the call the original function
		check(BodyContext.CallToOriginalFunction);
		BodyContext.CallExpression = new(Allocator) FBinaryExpression(Allocator, EOperators::Assign, SimpleTypeMember, BodyContext.CallToOriginalFunction, SourceInfo);
	}

	bool ProcessStructReturnType(CrossCompiler::AST::FStructSpecifier* StructSpecifier, TArray<CrossCompiler::AST::FStructSpecifier*>& MiniSymbolTable, FOutputsBodyContext& BodyContext)
	{
		using namespace CrossCompiler::AST;

		// Add a local variable to receive the output from the function
		FString LocalStructVarName = TEXT("Local_");
		LocalStructVarName += StructSpecifier->Name;
		auto* LocalStructVariable = CreateLocalVariable(StructSpecifier->Name, *LocalStructVarName);
		BodyContext.PreInstructions.Add(LocalStructVariable);

		// Create the LHS of the member assignment
		auto* SimpleTypeMember = MakeIdentifierExpression(Allocator, *LocalStructVarName, SourceInfo);

		// Create an assignment from the call the original function
		check(BodyContext.CallToOriginalFunction);
		BodyContext.CallExpression = new(Allocator) FBinaryExpression(Allocator, EOperators::Assign, SimpleTypeMember, BodyContext.CallToOriginalFunction, SourceInfo);

		// Add all the members and the copies to the return struct
		return AddUsedOutputMembers(BodyContext.NewReturnStruct, BodyContext.ReturnVariableName, StructSpecifier, *LocalStructVarName, MiniSymbolTable, BodyContext);
	}

	bool ProcessStructOutParameter(CrossCompiler::AST::FParameterDeclarator* ParameterDeclarator, CrossCompiler::AST::FStructSpecifier* OriginalStructSpecifier, TArray<CrossCompiler::AST::FStructSpecifier*>& MiniSymbolTable, FOutputsBodyContext& BodyContext)
	{
		// Add a local variable to receive the output from the function
		FString LocalStructVarName = TEXT("Local_");
		LocalStructVarName += OriginalStructSpecifier->Name;
		LocalStructVarName += TEXT("_OUT");
		auto* LocalStructVariable = CreateLocalVariable(OriginalStructSpecifier->Name, *LocalStructVarName);
		BodyContext.PreInstructions.Add(LocalStructVariable);

		// Add the parameter to the actual function call
		auto* Parameter = MakeIdentifierExpression(Allocator, *LocalStructVarName, SourceInfo);
		BodyContext.CallToOriginalFunction->Expressions.Add(Parameter);

		return AddUsedOutputMembers(BodyContext.NewReturnStruct, BodyContext.ReturnVariableName, OriginalStructSpecifier, *LocalStructVarName, MiniSymbolTable, BodyContext);
	}

	bool ProcessOriginalParameters(CrossCompiler::AST::FFunctionDefinition* EntryFunction, TArray<CrossCompiler::AST::FStructSpecifier*>& MiniSymbolTable, FOutputsBodyContext& BodyContext)
	{
		using namespace CrossCompiler::AST;
		for (FNode* ParamNode : EntryFunction->Prototype->Parameters)
		{
			FParameterDeclarator* ParameterDeclarator = ParamNode->AsParameterDeclarator();
			check(ParameterDeclarator);

			if (ParameterDeclarator->Type->Qualifier.bOut)
			{
				if (ParameterDeclarator->Semantic)
				{
					ProcessSimpleOutParameter(ParameterDeclarator, BodyContext);
				}
				else
				{
					// Confirm this is a struct living in the symbol table
					FStructSpecifier* OriginalStructSpecifier = FindStructSpecifier(MiniSymbolTable, ParameterDeclarator->Type->Specifier->TypeName);
					if (OriginalStructSpecifier)
					{
						if (!ProcessStructOutParameter(ParameterDeclarator, OriginalStructSpecifier, MiniSymbolTable, BodyContext))
						{
							return false;
						}
					}
					else if (CheckSimpleVectorType(ParameterDeclarator->Type->Specifier->TypeName))
					{
						Errors.Add(FString::Printf(TEXT("RemoveUnusedOutputs: Function %s with out parameter %s doesn't have a return semantic"), *EntryPoint, ParameterDeclarator->Identifier));
						return false;
					}
					else
					{
						Errors.Add(FString::Printf(TEXT("RemoveUnusedOutputs: Invalid return type %s for out parameter %s for function %s"), ParameterDeclarator->Type->Specifier->TypeName, ParameterDeclarator->Identifier, *EntryPoint));
						return false;
					}
				}
			}
			else
			{
				// Add this parameter as an input to the new function
				BodyContext.NewFunctionParameters.Add(ParameterDeclarator);

				// Add the parameter to the actual function call
				auto* Parameter = MakeIdentifierExpression(Allocator, ParameterDeclarator->Identifier, SourceInfo);
				BodyContext.CallToOriginalFunction->Expressions.Add(Parameter);
			}
		}

		return true;
	}

	bool IsSemanticUsed(const TCHAR* SemanticName)
	{
		if (bLeaveAllUsed || IsStringInArray(UsedOutputs, SemanticName) || IsSubstringInArray(Exceptions, SemanticName))
		{
			return true;
		}

		// Try the centroid modifier for safety
		if (!FCString::Stristr(SemanticName, TEXT("_centroid")))
		{
			FString Centroid = SemanticName;
			Centroid += "_centroid";
			return IsStringInArray(UsedOutputs, SemanticName);
		}

		return false;
	}

	bool AddUsedOutputMembers(CrossCompiler::AST::FStructSpecifier* DestStruct, const TCHAR* DestPrefix, CrossCompiler::AST::FStructSpecifier* SourceStruct, const TCHAR* SourcePrefix, TArray<CrossCompiler::AST::FStructSpecifier*>& MiniSymbolTable, FBodyContext& BodyContext)
	{
		using namespace CrossCompiler::AST;

		for (auto* MemberNodes : SourceStruct->Declarations)
		{
			FDeclaratorList* MemberDeclarator = MemberNodes->AsDeclaratorList();
			check(MemberDeclarator);
			for (auto* DeclarationNode : MemberDeclarator->Declarations)
			{
				FDeclaration* MemberDeclaration = DeclarationNode->AsDeclaration();
				check(MemberDeclaration);
				if (MemberDeclaration->Semantic)
				{
					if (MemberDeclaration->bIsArray)
					{
						uint32 ArrayLength = 0;
						if (!GetArrayLength(MemberDeclaration, ArrayLength))
						{
							return false;
						}

						uint32 StartIndex = 0;
						TCHAR* ElementSemanticPrefix = GetNonDigitSemanticPrefix(Allocator, MemberDeclaration->Semantic, StartIndex);
						if (!ElementSemanticPrefix)
						{
							Errors.Add(FString::Printf(TEXT("RemoveUnusedOutputs: Member (%s) %s : %s is expected to have an indexed semantic!"), MemberDeclarator->Type->Specifier->TypeName, MemberDeclaration->Identifier, MemberDeclaration->Semantic));

							// Fatal: Array of non-indexed semantic (eg float4 Colors[4] : MYSEMANTIC; )
							// Assume semantic is used and just fallback
							auto* NewDeclaratorList = new(Allocator) FDeclaratorList(Allocator, MemberDeclarator->SourceInfo);
							NewDeclaratorList->Type = CloneType(MemberDeclarator->Type);
							NewDeclaratorList->Declarations.Add(MemberDeclaration);
							DestStruct->Declarations.Add(NewDeclaratorList);

							CopyMember(MemberDeclaration, SourcePrefix, DestPrefix, BodyContext.PostInstructions);
						}

						for (uint32 Index = 0; Index < ArrayLength; ++Index)
						{
							TCHAR* ElementSemantic = MakeIndexedSemantic(Allocator, ElementSemanticPrefix, StartIndex + Index);
							if (IsSemanticUsed(ElementSemantic))
							{
								auto* NewMemberDeclaration = new(Allocator) FDeclaration(Allocator, MemberDeclaration->SourceInfo);
								NewMemberDeclaration->Semantic = ElementSemantic;
								NewMemberDeclaration->Identifier = Allocator->Strdup(FString::Printf(TEXT("%s_%d"), MemberDeclaration->Identifier, Index));

								// Add member to struct
								auto* NewDeclaratorList = new(Allocator) FDeclaratorList(Allocator, MemberDeclarator->SourceInfo);
								NewDeclaratorList->Type = CloneType(MemberDeclarator->Type);
								NewDeclaratorList->Declarations.Add(NewMemberDeclaration);
								DestStruct->Declarations.Add(NewDeclaratorList);

								FString LHSElement = FString::Printf(TEXT("%s.%s"), DestPrefix, NewMemberDeclaration->Identifier);
								FString RHSElement = FString::Printf(TEXT("%s.%s[%d]"), SourcePrefix, MemberDeclaration->Identifier, Index);

								auto* LHS = MakeIdentifierExpression(Allocator, *LHSElement, SourceInfo);
								auto* RHS = MakeIdentifierExpression(Allocator, *RHSElement, SourceInfo);
								auto* Assignment = new(Allocator) FBinaryExpression(Allocator, EOperators::Assign, LHS, RHS, SourceInfo);
								BodyContext.PostInstructions.Add(new(Allocator) FExpressionStatement(Allocator, Assignment, SourceInfo));
							}
							else
							{
								RemovedSemantics.Add(ElementSemantic);
							}
						}
					}
					else if (IsSemanticUsed(MemberDeclaration->Semantic))
					{
						// Add member to struct
						auto* NewDeclaratorList = new(Allocator) FDeclaratorList(Allocator, MemberDeclarator->SourceInfo);
						NewDeclaratorList->Type = CloneType(MemberDeclarator->Type);
						NewDeclaratorList->Declarations.Add(MemberDeclaration);
						DestStruct->Declarations.Add(NewDeclaratorList);

						CopyMember(MemberDeclaration, DestPrefix, SourcePrefix, BodyContext.PostInstructions);
					}
					else
					{
						RemovedSemantics.Add(MemberDeclaration->Semantic);
					}
				}
				else
				{
					if (!MemberDeclarator->Type || !MemberDeclarator->Type->Specifier || !MemberDeclarator->Type->Specifier->TypeName)
					{
						Errors.Add(FString::Printf(TEXT("RemoveUnusedOutputs: Internal error tracking down nested type %s"), MemberDeclaration->Identifier));
						return false;
					}

					// No semantic, so make sure this is a nested struct, or error that it's missing a semantic
					FStructSpecifier* NestedStructSpecifier = FindStructSpecifier(MiniSymbolTable, MemberDeclarator->Type->Specifier->TypeName);
					if (!NestedStructSpecifier)
					{
						Errors.Add(FString::Printf(TEXT("RemoveUnusedOutputs: Member (%s) %s is expected to have a semantic!"), MemberDeclarator->Type->Specifier->TypeName, MemberDeclaration->Identifier));
						return false;
					}

					// Add all the elements of this new struct into the return type
					FString NewSourcePrefix = SourcePrefix;
					NewSourcePrefix += TEXT(".");
					NewSourcePrefix += MemberDeclaration->Identifier;
					AddUsedOutputMembers(DestStruct, DestPrefix, NestedStructSpecifier, *NewSourcePrefix, MiniSymbolTable, BodyContext);
				}
			}
		}

		return true;
	}
};

bool RemoveUnusedOutputs(FString& InOutSourceCode, const TArray<FString>& InUsedOutputs, const TArray<FString>& InExceptions, FString& EntryPoint, TArray<FString>& OutErrors)
{
	FString DummyFilename(TEXT("/Engine/Private/RemoveUnusedOutputs.usf"));
	FRemoveUnusedOutputs Data(InUsedOutputs, InExceptions);
	Data.EntryPoint = EntryPoint;
	auto Lambda = [&Data](CrossCompiler::FLinearAllocator* Allocator, CrossCompiler::TLinearArray<CrossCompiler::AST::FNode*>& ASTNodes)
	{
		Data.Allocator = Allocator;
		Data.RemoveUnusedOutputs(ASTNodes);
		if (!Data.bSuccess)
		{
			int i = 0;
			++i;
		}
	};
	CrossCompiler::FCompilerMessages Messages;
	if (!CrossCompiler::Parser::Parse(InOutSourceCode, DummyFilename, Messages, Lambda))
	{
		Data.Errors.Add(FString(TEXT("RemoveUnusedOutputs: Failed to compile!")));
		OutErrors = Data.Errors;
		for (auto& Message : Messages.MessageList)
		{
			OutErrors.Add(Message.Message);
		}
		return false;
	}

	for (auto& Message : Messages.MessageList)
	{
		OutErrors.Add(Message.Message);
	}

	if (Data.bSuccess)
	{
		InOutSourceCode += (TCHAR)'\n';
		InOutSourceCode += Data.GeneratedCode;
		EntryPoint = Data.EntryPoint;

		return true;
	}

	OutErrors = Data.Errors;
	return false;
}

struct FRemoveUnusedInputs : FRemoveAlgorithm
{
	const TArray<FString>& UsedInputs;

	struct FInputsBodyContext : FBodyContext
	{
		CrossCompiler::AST::FStructSpecifier* NewInputStruct;

		const TCHAR* InputVariableName;
		const TCHAR* InputTypeName;

		FInputsBodyContext() :
			NewInputStruct(nullptr),
			InputVariableName(TEXT("OptimizedInput")),
			InputTypeName(TEXT("FOptimizedInput"))
		{
		}
	};

	FRemoveUnusedInputs(const TArray<FString>& InUsedInputs) :
		UsedInputs(InUsedInputs)
	{
	}

	void RemoveUnusedInputs(CrossCompiler::TLinearArray<CrossCompiler::AST::FNode*>& ASTNodes)
	{
		using namespace CrossCompiler::AST;

		// Find Entry point from original AST nodes
		TArray<FStructSpecifier*> MiniSymbolTable;
		FString Test;
		FFunctionDefinition* EntryFunction = FindEntryPointAndPopulateSymbolTable(ASTNodes, MiniSymbolTable, &Test);
		if (!EntryFunction)
		{
			Errors.Add(FString::Printf(TEXT("RemoveUnused: Unable to find entry point %s"), *EntryPoint));
			bSuccess = false;
			return;
		}

		SourceInfo = EntryFunction->SourceInfo;

		FInputsBodyContext BodyContext;

		// Setup the call to the original entry point
		BodyContext.CallToOriginalFunction = new(Allocator) FFunctionExpression(Allocator, SourceInfo, MakeIdentifierExpression(Allocator, *EntryPoint, SourceInfo));

		if (!SetupInputAndReturnType(EntryFunction, MiniSymbolTable, BodyContext))
		{
			bSuccess = false;
			return;
		}

		if (!ProcessOriginalParameters(EntryFunction, MiniSymbolTable, BodyContext))
		{
			bSuccess = false;
			return;
		}

		// Real call statement
		if (BodyContext.FinalInstruction)
		{
			auto* JumpStatement = BodyContext.FinalInstruction->AsJumpStatement();
			if (JumpStatement)
			{
				JumpStatement->OptionalExpression = BodyContext.CallToOriginalFunction;
			}
		}
		else
		{
			BodyContext.FinalInstruction = new(Allocator) CrossCompiler::AST::FExpressionStatement(Allocator, BodyContext.CallToOriginalFunction, SourceInfo);
		}

		auto* Body = AddStatementsToBody(BodyContext, nullptr);

		if (BodyContext.NewInputStruct->Declarations.Num() > 0)
		{
			// If the input struct is not empty, add this as an argument to the new entry function
			FParameterDeclarator* Declarator = new(Allocator) FParameterDeclarator(Allocator, SourceInfo);
			Declarator->Type = MakeSimpleType(BodyContext.InputTypeName);
			Declarator->Identifier = BodyContext.InputVariableName;
			BodyContext.NewFunctionParameters.Add(Declarator);
		}

		FFunctionDefinition* NewEntryFunction = CreateNewEntryFunction(Body, EntryFunction->Prototype->ReturnType, BodyContext.NewFunctionParameters);
		NewEntryFunction->Prototype->ReturnSemantic = EntryFunction->Prototype->ReturnSemantic;

		WriteGeneratedInCode(NewEntryFunction, BodyContext.NewStructs, GeneratedCode);

		EntryPoint = NewEntryFunction->Prototype->Identifier;
		bSuccess = true;
	}

	bool ProcessOriginalParameters(CrossCompiler::AST::FFunctionDefinition* EntryFunction, TArray<CrossCompiler::AST::FStructSpecifier*>& MiniSymbolTable, FInputsBodyContext& BodyContext)
	{
		using namespace CrossCompiler::AST;
		for (FNode* ParamNode : EntryFunction->Prototype->Parameters)
		{
			FParameterDeclarator* ParameterDeclarator = ParamNode->AsParameterDeclarator();
			check(ParameterDeclarator);
			if (!ParameterDeclarator->Type->Qualifier.bOut)
			{
				if (ParameterDeclarator->Semantic)
				{
					ProcessSimpleInParameter(ParameterDeclarator, BodyContext);
				}
				else
				{
					// Confirm this is a struct living in the symbol table
					FStructSpecifier* OriginalStructSpecifier = FindStructSpecifier(MiniSymbolTable, ParameterDeclarator->Type->Specifier->TypeName);
					if (OriginalStructSpecifier)
					{
						if (!ProcessStructInParameter(ParameterDeclarator, OriginalStructSpecifier, MiniSymbolTable, BodyContext))
						{
							return false;
						}
					}
					else if (CheckSimpleVectorType(ParameterDeclarator->Type->Specifier->TypeName))
					{
						Errors.Add(FString::Printf(TEXT("RemoveUnusedInputs: Function %s with in parameter %s doesn't have a return semantic"), *EntryPoint, ParameterDeclarator->Identifier));
						return false;
					}
					else
					{
						Errors.Add(FString::Printf(TEXT("RemoveUnusedInputs: Invalid return type %s for in parameter %s for function %s"), ParameterDeclarator->Type->Specifier->TypeName, ParameterDeclarator->Identifier, *EntryPoint));
						return false;
					}
				}
			}
			else
			{
				// Add this parameter as an input to the new function
				BodyContext.NewFunctionParameters.Add(ParameterDeclarator);

				// Add the parameter to the actual function call
				auto* Parameter = MakeIdentifierExpression(Allocator, ParameterDeclarator->Identifier, SourceInfo);
				BodyContext.CallToOriginalFunction->Expressions.Add(Parameter);
			}
		}
		return true;
	}

	bool ProcessStructInParameter(CrossCompiler::AST::FParameterDeclarator* ParameterDeclarator, CrossCompiler::AST::FStructSpecifier* OriginalStructSpecifier, TArray<CrossCompiler::AST::FStructSpecifier*>& MiniSymbolTable, FInputsBodyContext& BodyContext)
	{
		using namespace CrossCompiler::AST;

		auto* Zero = new(Allocator) FUnaryExpression(Allocator, EOperators::FloatConstant, nullptr, SourceInfo);
		Zero->FloatConstant = 0;
		auto* Initializer = new(Allocator) FUnaryExpression(Allocator, EOperators::TypeCast, Zero, SourceInfo);
		Initializer->TypeSpecifier = MakeSimpleType(OriginalStructSpecifier->Name)->Specifier;

		// Add a local variable to receive the output from the function
		FString LocalStructVarName = TEXT("Local_");
		LocalStructVarName += OriginalStructSpecifier->Name;
		LocalStructVarName += TEXT("_IN");
		auto* LocalStructVariable = CreateLocalVariable(OriginalStructSpecifier->Name, *LocalStructVarName, Initializer);
		BodyContext.PreInstructions.Add(LocalStructVariable);

		// Add the parameter to the actual function call
		auto* Parameter = MakeIdentifierExpression(Allocator, *LocalStructVarName, SourceInfo);
		BodyContext.CallToOriginalFunction->Expressions.Add(Parameter);
		return AddUsedInputMembers(BodyContext.NewInputStruct, BodyContext.InputVariableName, OriginalStructSpecifier, *LocalStructVarName, MiniSymbolTable, BodyContext);
	}

	bool IsSemanticUsed(const TCHAR* SemanticName)
	{
		if (bLeaveAllUsed || IsStringInArray(UsedInputs, SemanticName))
		{
			return true;
		}

		// Try the centroid modifier for safety
		if (!FCString::Stristr(SemanticName, TEXT("_centroid")))
		{
			FString Centroid = SemanticName;
			Centroid += "_centroid";
			return IsStringInArray(UsedInputs, SemanticName);
		}

		return false;
	}

	void ProcessSimpleInParameter(CrossCompiler::AST::FParameterDeclarator* ParameterDeclarator, FInputsBodyContext& BodyContext)
	{
		using namespace CrossCompiler::AST;

		FExpression* Initializer = nullptr;

		bool bRequiresToBeOnInputStruct = IsSemanticUsed(ParameterDeclarator->Semantic);
		if (bRequiresToBeOnInputStruct)
		{
			// Add the member to the input struct
			auto* MemberDeclaratorList = new(Allocator) FDeclaratorList(Allocator, SourceInfo);
			MemberDeclaratorList->Type = CloneType(ParameterDeclarator->Type);
			auto* MemberDeclaration = new(Allocator) FDeclaration(Allocator, SourceInfo);
			MemberDeclaration->Identifier = ParameterDeclarator->Identifier;
			MemberDeclaration->Semantic = Allocator->Strdup(ParameterDeclarator->Semantic);
			MemberDeclaratorList->Declarations.Add(MemberDeclaration);

			// Add it to the input struct type
			check(BodyContext.NewInputStruct);
			BodyContext.NewInputStruct->Declarations.Add(MemberDeclaratorList);

			// Make this parameter the initializer of the new local variable
			FString ParameterName = BodyContext.InputVariableName;
			ParameterName += TEXT(".");
			ParameterName += ParameterDeclarator->Identifier;
			Initializer = MakeIdentifierExpression(Allocator, *ParameterName, SourceInfo);
		}
		else
		{
			// Make a local to generate the in parameter: Type Local = (Type)0;
			auto* Zero = new(Allocator) FUnaryExpression(Allocator, EOperators::FloatConstant, nullptr, SourceInfo);
			Zero->FloatConstant = 0;
			Initializer = new(Allocator) FUnaryExpression(Allocator, EOperators::TypeCast, Zero, SourceInfo);
			Initializer->TypeSpecifier = ParameterDeclarator->Type->Specifier;

			RemovedSemantics.Add(ParameterDeclarator->Semantic);
		}

		auto* LocalVar = CreateLocalVariable(ParameterDeclarator->Type->Specifier->TypeName, ParameterDeclarator->Identifier, Initializer);
		BodyContext.PreInstructions.Add(LocalVar);

		// Add the parameter to the actual function call
		auto* Parameter = MakeIdentifierExpression(Allocator, ParameterDeclarator->Identifier, SourceInfo);
		BodyContext.CallToOriginalFunction->Expressions.Add(Parameter);
	}

	bool SetupInputAndReturnType(CrossCompiler::AST::FFunctionDefinition* EntryFunction, TArray<CrossCompiler::AST::FStructSpecifier*>& MiniSymbolTable, FInputsBodyContext& BodyContext)
	{
		using namespace CrossCompiler::AST;

		// New return type
		BodyContext.NewInputStruct = CreateNewStructSpecifier(BodyContext.InputTypeName, BodyContext.NewStructs);

		auto* ReturnType = EntryFunction->Prototype->ReturnType;
		if (ReturnType && ReturnType->Specifier && ReturnType->Specifier->TypeName)
		{
			const TCHAR* ReturnTypeName = ReturnType->Specifier->TypeName;
			if (!EntryFunction->Prototype->ReturnSemantic && !FCString::Strcmp(ReturnTypeName, TEXT("void")))
			{
				// No return instruction required
			}
			else
			{
				auto* ReturnStatement = new(Allocator) FJumpStatement(Allocator, EJumpType::Return, SourceInfo);
				BodyContext.FinalInstruction = ReturnStatement;
			}
			return true;
		}
		else
		{
			Errors.Add(FString::Printf(TEXT("RemoveUnusedInputs: Internal error trying to determine return type")));
		}
		return false;
	}

	void WriteGeneratedInCode(CrossCompiler::AST::FFunctionDefinition* NewEntryFunction, TArray<CrossCompiler::AST::FStructSpecifier*>& NewStructs, FString& OutGeneratedCode)
	{
		CrossCompiler::AST::FASTWriter Writer(OutGeneratedCode);
		GeneratedCode = TEXT("#line 1 \"RemoveUnusedInputs.usf\"\n// Generated Entry Point: ");
		GeneratedCode += NewEntryFunction->Prototype->Identifier;
		GeneratedCode += TEXT("\n");
		if (UsedInputs.Num() > 0)
		{
			GeneratedCode += TEXT("// Requested UsedInputs:");
			for (int32 Index = 0; Index < UsedInputs.Num(); ++Index)
			{
				GeneratedCode += (Index == 0) ? TEXT(" ") : TEXT(", ");
				GeneratedCode += UsedInputs[Index];
			}
			GeneratedCode += TEXT("\n");
		}
		if (RemovedSemantics.Num() > 0)
		{
			GeneratedCode += TEXT("// Removed Inputs:");
			for (int32 Index = 0; Index < RemovedSemantics.Num(); ++Index)
			{
				GeneratedCode += (Index == 0) ? TEXT(" ") : TEXT(", ");
				GeneratedCode += RemovedSemantics[Index];
			}
			GeneratedCode += TEXT("\n");
		}
		for (auto* Struct : NewStructs)
		{
			auto* Declarator = new(Allocator)CrossCompiler::AST::FDeclaratorList(Allocator, SourceInfo);
			Declarator->Declarations.Add(Struct);
			Declarator->Write(Writer);
		}
		NewEntryFunction->Write(Writer);
		//FPlatformMisc::LowLevelOutputDebugStringf(TEXT("*********************************\n%s\n"), *GeneratedCode);
	}

	bool AddUsedInputMembers(CrossCompiler::AST::FStructSpecifier* DestStruct, const TCHAR* DestPrefix, CrossCompiler::AST::FStructSpecifier* SourceStruct, const TCHAR* SourcePrefix, TArray<CrossCompiler::AST::FStructSpecifier*>& MiniSymbolTable, FBodyContext& BodyContext)
	{
		using namespace CrossCompiler::AST;

		for (auto* MemberNodes : SourceStruct->Declarations)
		{
			FDeclaratorList* MemberDeclarator = MemberNodes->AsDeclaratorList();
			check(MemberDeclarator);
			for (auto* DeclarationNode : MemberDeclarator->Declarations)
			{
				FDeclaration* MemberDeclaration = DeclarationNode->AsDeclaration();
				check(MemberDeclaration);
				if (MemberDeclaration->Semantic)
				{
					if (MemberDeclaration->bIsArray)
					{
						uint32 ArrayLength = 0;
						if (!GetArrayLength(MemberDeclaration, ArrayLength))
						{
							return false;
						}

						uint32 StartIndex = 0;
						TCHAR* ElementSemanticPrefix = GetNonDigitSemanticPrefix(Allocator, MemberDeclaration->Semantic, StartIndex);
						if (!ElementSemanticPrefix)
						{
							Errors.Add(FString::Printf(TEXT("RemoveUnusedInputs: Member (%s) %s : %s is expected to have an indexed semantic!"), MemberDeclarator->Type->Specifier->TypeName, MemberDeclaration->Identifier, MemberDeclaration->Semantic));

							// Fatal: Array of non-indexed semantic (eg float4 Colors[4] : MYSEMANTIC; )
							// Assume semantic is used and just fallback
							auto* NewDeclaratorList = new(Allocator) FDeclaratorList(Allocator, MemberDeclarator->SourceInfo);
							NewDeclaratorList->Type = CloneType(MemberDeclarator->Type);
							NewDeclaratorList->Declarations.Add(MemberDeclaration);
							DestStruct->Declarations.Add(NewDeclaratorList);

							CopyMember(MemberDeclaration, SourcePrefix, DestPrefix, BodyContext.PreInstructions);
						}

						for (uint32 Index = 0; Index < ArrayLength; ++Index)
						{
							TCHAR* ElementSemantic = MakeIndexedSemantic(Allocator, ElementSemanticPrefix, StartIndex + Index);
							if (IsSemanticUsed(ElementSemantic))
							{
								auto* NewMemberDeclaration = new(Allocator) FDeclaration(Allocator, MemberDeclaration->SourceInfo);
								NewMemberDeclaration->Semantic = ElementSemantic;
								NewMemberDeclaration->Identifier = Allocator->Strdup(FString::Printf(TEXT("%s_%d"), MemberDeclaration->Identifier, Index));

								// Add member to struct
								auto* NewDeclaratorList = new(Allocator) FDeclaratorList(Allocator, MemberDeclarator->SourceInfo);
								NewDeclaratorList->Type = CloneType(MemberDeclarator->Type);
								NewDeclaratorList->Declarations.Add(NewMemberDeclaration);
								DestStruct->Declarations.Add(NewDeclaratorList);

								FString LHSElement = FString::Printf(TEXT("%s.%s[%d]"), SourcePrefix, MemberDeclaration->Identifier, Index);
								FString RHSElement = FString::Printf(TEXT("%s.%s"), DestPrefix, NewMemberDeclaration->Identifier);

								auto* LHS = MakeIdentifierExpression(Allocator, *LHSElement, SourceInfo);
								auto* RHS = MakeIdentifierExpression(Allocator, *RHSElement, SourceInfo);
								auto* Assignment = new(Allocator) FBinaryExpression(Allocator, EOperators::Assign, LHS, RHS, SourceInfo);
								BodyContext.PreInstructions.Add(new(Allocator) FExpressionStatement(Allocator, Assignment, SourceInfo));
							}
							else
					{
								RemovedSemantics.Add(ElementSemantic);
							}
						}
					}
					else if (IsSemanticUsed(MemberDeclaration->Semantic))
					{
						// Add member to struct
						auto* NewDeclaratorList = new(Allocator) FDeclaratorList(Allocator, MemberDeclarator->SourceInfo);
						NewDeclaratorList->Type = CloneType(MemberDeclarator->Type);
						NewDeclaratorList->Declarations.Add(MemberDeclaration);
						DestStruct->Declarations.Add(NewDeclaratorList);

						CopyMember(MemberDeclaration, SourcePrefix, DestPrefix, BodyContext.PreInstructions);
					}
					else
					{
						// Ignore as the base struct is zero'd out
/*
						auto* Zero = new(Allocator) FUnaryExpression(Allocator, EOperators::FloatConstant, nullptr, SourceInfo);
						Zero->FloatConstant = 0;
						auto* Cast = new(Allocator) FUnaryExpression(Allocator, EOperators::TypeCast, Zero, SourceInfo);
						Cast->TypeSpecifier = MemberDeclarator->Type->Specifier;
						auto* Assignment = new(Allocator) FBinaryExpression(Allocator, EOperators::Assign, MakeIdentifierExpression(Allocator, SourcePrefix, SourceInfo), Cast, SourceInfo);
						auto* Statement = new(Allocator) FExpressionStatement(Allocator, Assignment, SourceInfo);
						BodyContext.PostInstructions.Add(Statement);
*/
						RemovedSemantics.Add(MemberDeclaration->Semantic);
					}
				}
				else
				{
					if (!MemberDeclarator->Type || !MemberDeclarator->Type->Specifier || !MemberDeclarator->Type->Specifier->TypeName)
					{
						Errors.Add(FString::Printf(TEXT("RemoveUnusedInputs: Internal error tracking down nested type %s"), MemberDeclaration->Identifier));
						return false;
					}

					// No semantic, so make sure this is a nested struct, or error that it's missing a semantic
					FStructSpecifier* NestedStructSpecifier = FindStructSpecifier(MiniSymbolTable, MemberDeclarator->Type->Specifier->TypeName);
					if (!NestedStructSpecifier)
					{
						Errors.Add(FString::Printf(TEXT("RemoveUnusedInputs: Member (%s) %s is expected to have a semantic!"), MemberDeclarator->Type->Specifier->TypeName, MemberDeclaration->Identifier));
						return false;
					}

					// Add all the elements of this new struct into the return type
					FString NewSourcePrefix = SourcePrefix;
					NewSourcePrefix += TEXT(".");
					NewSourcePrefix += MemberDeclaration->Identifier;
					AddUsedInputMembers(DestStruct, DestPrefix, NestedStructSpecifier, *NewSourcePrefix, MiniSymbolTable, BodyContext);
				}
			}
		}
		return true;
	}
};

bool RemoveUnusedInputs(FString& InOutSourceCode, const TArray<FString>& InInputs, FString& EntryPoint, TArray<FString>& OutErrors)
{
	FString DummyFilename(TEXT("/Engine/Private/RemoveUnusedInputs.usf"));
	FRemoveUnusedInputs Data(InInputs);
	Data.EntryPoint = EntryPoint;
	CrossCompiler::FCompilerMessages Messages;
	auto Lambda = [&Data](CrossCompiler::FLinearAllocator* Allocator, CrossCompiler::TLinearArray<CrossCompiler::AST::FNode*>& ASTNodes)
	{
		Data.Allocator = Allocator;
		Data.RemoveUnusedInputs(ASTNodes);
		if (!Data.bSuccess)
		{
			int i = 0;
			++i;
		}
	};
	if (!CrossCompiler::Parser::Parse(InOutSourceCode, DummyFilename, Messages, Lambda))
	{
		Data.Errors.Add(FString(TEXT("RemoveUnusedInputs: Failed to compile!")));
		OutErrors = Data.Errors;
		for (auto& Message : Messages.MessageList)
		{
			OutErrors.Add(Message.Message);
		}
		return false;
	}

	for (auto& Message : Messages.MessageList)
	{
		OutErrors.Add(Message.Message);
	}

	if (Data.bSuccess)
	{
		InOutSourceCode += (TCHAR)'\n';
		InOutSourceCode += Data.GeneratedCode;
		EntryPoint = Data.EntryPoint;

		return true;
	}

	OutErrors = Data.Errors;
	return false;
}

void StripInstancedStereo(FString& ShaderSource)
{
	ShaderSource.ReplaceInline(TEXT("ResolvedView = ResolveView();"), TEXT(""));
	ShaderSource.ReplaceInline(TEXT("ResolvedView"), TEXT("View"));
}

struct FConvertFP32ToFP16 {
	FString Filename;
	FString GeneratedCode;
	bool bSuccess;
};

static void ConvertFromFP32ToFP16(const TCHAR*& TypeName, CrossCompiler::FLinearAllocator* Allocator)
{
	static FString FloatTypes[9] = { "float", "float2", "float3", "float4", "float2x2", "float3x3", "float4x4", "float3x4", "float4x3" };
	static FString HalfTypes[9] = { "half", "half2", "half3", "half4", "half2x2", "half3x3", "half4x4", "half3x4", "half4x3" };
	FString NewType;
	for (int32 i = 0; i < 9; ++i) 
	{
		if (FString(TypeName).Equals(FloatTypes[i]))
		{
			NewType = HalfTypes[i];
			break;
		}
	}
	if (NewType.IsEmpty()) return;
	check(Allocator);
	*const_cast<TCHAR**>(&TypeName) = Allocator->Strdup(*NewType);
}

static void ConvertFromFP32ToFP16(CrossCompiler::AST::FTypeSpecifier* Type, CrossCompiler::FLinearAllocator* Allocator)
{
    ConvertFromFP32ToFP16(Type->TypeName, Allocator);
}

static void ConvertFromFP32ToFP16Base(CrossCompiler::AST::FNode* Node, CrossCompiler::FLinearAllocator* Allocator);
static void ConvertFromFP32ToFP16(CrossCompiler::AST::FFunctionDefinition* Node, CrossCompiler::FLinearAllocator* Allocator)
{
	if (FString(Node->Prototype->Identifier).Equals("CalcSceneDepth"))
	{
		return;
	}
	ConvertFromFP32ToFP16(Node->Prototype->ReturnType->Specifier, Allocator);
	for (auto Elem : Node->Prototype->Parameters)
	{
		ConvertFromFP32ToFP16Base(Elem, Allocator);
	}
	for (auto Elem : Node->Body->Statements)
	{
		ConvertFromFP32ToFP16Base(Elem, Allocator);
	}
}

static void ConvertFromFP32ToFP16(CrossCompiler::AST::FParameterDeclarator* Node, CrossCompiler::FLinearAllocator* Allocator)
{
	if (Node->bIsArray)
	{
		return;
	}
	ConvertFromFP32ToFP16(Node->Type->Specifier, Allocator);
}

static void ConvertFromFP32ToFP16(CrossCompiler::AST::FDeclaratorList* Node, CrossCompiler::FLinearAllocator* Allocator)
{
	for (auto Elem : Node->Declarations)
	{
		if (Elem->AsDeclaration() && Elem->AsDeclaration()->bIsArray)
		{
			return;
		}
	}
	ConvertFromFP32ToFP16(Node->Type->Specifier, Allocator);
}

static void ConvertFromFP32ToFP16(CrossCompiler::AST::FSelectionStatement* Node, CrossCompiler::FLinearAllocator* Allocator)
{
	if (Node->ThenStatement)
	{
		ConvertFromFP32ToFP16Base(Node->ThenStatement, Allocator);
	}
	if (Node->ElseStatement)
	{
		ConvertFromFP32ToFP16Base(Node->ElseStatement, Allocator);
	}
}

static void ConvertFromFP32ToFP16(CrossCompiler::AST::FIterationStatement* Node, CrossCompiler::FLinearAllocator* Allocator)
{
	if (Node->InitStatement)
	{
		ConvertFromFP32ToFP16Base(Node->InitStatement, Allocator);
	}
	if (Node->Condition)
	{
		ConvertFromFP32ToFP16Base(Node->Condition, Allocator);
	}
	if (Node->Body)
	{
		ConvertFromFP32ToFP16Base(Node->Body, Allocator);
	}
}

static void ConvertFromFP32ToFP16(CrossCompiler::AST::FCompoundStatement* Node, CrossCompiler::FLinearAllocator* Allocator)
{
	for (auto Statement : Node->Statements)
	{
		ConvertFromFP32ToFP16Base(Statement, Allocator);
	}
}

static void ConvertFromFP32ToFP16(CrossCompiler::AST::FSwitchStatement* Node, CrossCompiler::FLinearAllocator* Allocator)
{
	if (Node->Body == nullptr || Node->Body->CaseList == nullptr)
	{
		return;
	}
	for (auto Elem : Node->Body->CaseList->Cases)
	{
		if (Elem == nullptr)
		{
			continue;
		}
		for (auto Statement : Elem->Statements)
		{
			if (Statement)
			{
				ConvertFromFP32ToFP16Base(Statement, Allocator);
			}
		}
	}
}

static void ConvertFromFP32ToFP16(CrossCompiler::AST::FExpression* Expression, CrossCompiler::FLinearAllocator* Allocator)
{
    if (Expression->Operator == CrossCompiler::AST::EOperators::Identifier)
    {
        ConvertFromFP32ToFP16(Expression->Identifier, Allocator);
    }
    if (Expression->Operator == CrossCompiler::AST::EOperators::TypeCast)
    {
        ConvertFromFP32ToFP16(Expression->TypeSpecifier, Allocator);
    }
    if (Expression->Operator == CrossCompiler::AST::EOperators::FieldSelection)
    {
        ConvertFromFP32ToFP16(Expression->SubExpressions[0], Allocator);
    }
    if (Expression->Operator == CrossCompiler::AST::EOperators::Assign)
    {
        ConvertFromFP32ToFP16(Expression->SubExpressions[0], Allocator);
        ConvertFromFP32ToFP16(Expression->SubExpressions[1], Allocator);
    }
    if (Expression->Operator == CrossCompiler::AST::EOperators::FunctionCall)
    {
        if (Expression->SubExpressions[0])
        {
            ConvertFromFP32ToFP16(Expression->SubExpressions[0], Allocator);
        }
        for (auto SubExpression : Expression->Expressions)
        {
            ConvertFromFP32ToFP16(SubExpression, Allocator);
        }
    }
}

static void ConvertFromFP32ToFP16(CrossCompiler::AST::FExpressionStatement* Node, CrossCompiler::FLinearAllocator* Allocator)
{
    if (Node->Expression == nullptr)
    {
        return;
    }
    ConvertFromFP32ToFP16(Node->Expression, Allocator);
}

static void ConvertFromFP32ToFP16(CrossCompiler::AST::FJumpStatement* Node, CrossCompiler::FLinearAllocator* Allocator)
{
    if (Node->OptionalExpression == nullptr)
    {
        return;
    }
    ConvertFromFP32ToFP16(Node->OptionalExpression, Allocator);
}

static void ConvertFromFP32ToFP16Base(CrossCompiler::AST::FNode* Node, CrossCompiler::FLinearAllocator* Allocator)
{
	if (Node->AsFunctionDefinition())
	{
		ConvertFromFP32ToFP16(Node->AsFunctionDefinition(), Allocator);
	}
	if (Node->AsParameterDeclarator())
	{
		ConvertFromFP32ToFP16(Node->AsParameterDeclarator(), Allocator);
	}
	if (Node->AsDeclaratorList())
	{
		ConvertFromFP32ToFP16(Node->AsDeclaratorList(), Allocator);
	}
	if (Node->AsSelectionStatement())
	{
		ConvertFromFP32ToFP16(Node->AsSelectionStatement(), Allocator);
	}
	if (Node->AsSwitchStatement())
	{
		ConvertFromFP32ToFP16(Node->AsSwitchStatement(), Allocator);
	}
	if (Node->AsIterationStatement())
	{
		ConvertFromFP32ToFP16(Node->AsIterationStatement(), Allocator);
	}
	if (Node->AsCompoundStatement())
	{
		ConvertFromFP32ToFP16(Node->AsCompoundStatement(), Allocator);
	}
    if (Node->AsExpressionStatement())
    {
		ConvertFromFP32ToFP16(Node->AsExpressionStatement(), Allocator);
    }
    if (Node->AsJumpStatement())
    {
		ConvertFromFP32ToFP16(Node->AsJumpStatement(), Allocator);
    }
}

static void ConvertFromFP32ToFP16(CrossCompiler::AST::FStructSpecifier* Node, CrossCompiler::FLinearAllocator* Allocator)
{
    for (auto Declaration : Node->Declarations)
    {
        ConvertFromFP32ToFP16Base(Declaration, Allocator);
    }
}

static void HlslParserCallbackWrapperFP32ToFP16(void* CallbackData, CrossCompiler::FLinearAllocator* Allocator, CrossCompiler::TLinearArray<CrossCompiler::AST::FNode*>& ASTNodes)
{
	auto* ConvertData = (FConvertFP32ToFP16*)CallbackData;
	CrossCompiler::AST::FASTWriter writer(ConvertData->GeneratedCode);
	for (auto Elem : ASTNodes)
	{
		if (Elem->AsFunctionDefinition())
		{
			ConvertFromFP32ToFP16(Elem->AsFunctionDefinition(), Allocator);
		}
		/*if (Elem->AsDeclaratorList() && Elem->AsDeclaratorList()->Type && Elem->AsDeclaratorList()->Type->Specifier->Structure)
		{
			ConvertFromFP32ToFP16(Elem->AsDeclaratorList()->Type->Specifier->Structure, Allocator);
		}*/
		Elem->Write(writer);
	}
	ConvertData->bSuccess = true;
}

bool ConvertFromFP32ToFP16(FString& InOutSourceCode, TArray<FString>& OutErrors)
{
	FString DummyFilename(TEXT("/Engine/Private/ConvertFP32ToFP16.usf"));
	CrossCompiler::FCompilerMessages Messages;
	FConvertFP32ToFP16 Data;
	Data.Filename = DummyFilename;
	Data.GeneratedCode = "";
	if (!CrossCompiler::Parser::Parse(InOutSourceCode, DummyFilename, Messages, HlslParserCallbackWrapperFP32ToFP16, &Data))
	{
		OutErrors.Add(FString(TEXT("ConvertFP32ToFP16: Failed to compile!")));
		for (auto& Message : Messages.MessageList)
		{
			OutErrors.Add(Message.Message);
		}
		return false;
	}

	for (auto& Message : Messages.MessageList)
	{
		OutErrors.Add(Message.Message);
	}

	if (Data.bSuccess)
	{
		InOutSourceCode = Data.GeneratedCode;
		return true;
	}

	return false;
}
