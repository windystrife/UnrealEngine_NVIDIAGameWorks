// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "KismetCompiledFunctionContext.h"
#include "KismetCompiler.h"

//////////////////////////////////////////////////////////////////////////
// IKismetCompilerBackend

class IKismetCompilerBackend
{
};

//////////////////////////////////////////////////////////////////////////
// FKismetVMBackend

class FKismetCompilerVMBackend : public IKismetCompilerBackend
{
public:
	typedef TMap<FBlueprintCompiledStatement*, CodeSkipSizeType> TStatementToSkipSizeMap;

protected:
	UBlueprint* Blueprint;
	UEdGraphSchema_K2* Schema;
	FCompilerResultsLog& MessageLog;
	FKismetCompilerContext& CompilerContext;

	TStatementToSkipSizeMap UbergraphStatementLabelMap;
public:
	bool bAnyNonReducibleFunctionGenerated;

	FKismetCompilerVMBackend(UBlueprint* InBlueprint, UEdGraphSchema_K2* InSchema, FKismetCompilerContext& InContext)
		: Blueprint(InBlueprint)
		, Schema(InSchema)
		, MessageLog(InContext.MessageLog)
		, CompilerContext(InContext)
		, bAnyNonReducibleFunctionGenerated(false)
	{
	}

	void GenerateCodeFromClass(UClass* SourceClass, TIndirectArray<FKismetFunctionContext>& Functions, bool bGenerateStubsOnly=false);

protected:
	/** Builds both the header declaration and body implementation of a function */
	void ConstructFunction(FKismetFunctionContext& FunctionContext, bool bIsUbergraph, bool bGenerateStubOnly);
};

//////////////////////////////////////////////////////////////////////////
