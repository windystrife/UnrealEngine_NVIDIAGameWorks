// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraEditorCommon.h"
#include "NiagaraParameters.h"

class Error;
class UEdGraphPin;
class UNiagaraNode;
class UNiagaraGraph;
class UEdGraphPin;
class FCompilerResultsLog;
class UNiagaraDataInterface;
struct FNiagaraTranslatorOutput;


/** Defines information about the results of a Niagara script compile. */
struct FNiagaraCompileResults
{
	/** Whether or not the script compiled successfully for VectorVM */
	bool bVMSucceeded;

	/** Whether or not the script compiled successfully for GPU compute */
	bool bComputeSucceeded;

	/** A results log with messages, warnings, and errors which occurred during the compile. */
	FCompilerResultsLog* MessageLog;

	/** A string representation of the compilation output. */
	FString OutputHLSL;

	FNiagaraCompileResults(FCompilerResultsLog* InMessageLog = nullptr)
		: MessageLog(InMessageLog)
	{
	}

	static ENiagaraScriptCompileStatus CompileResultsToSummary(const FNiagaraCompileResults* CompileResults);

	FNiagaraParameters Parameters;
	TArray<FNiagaraVariable> Attributes;
};

//Interface for Niagara compilers.
// NOTE: the graph->hlsl translation step is now in FNiagaraHlslTranslator
//
class INiagaraCompiler
{
public:
	/** Compiles a script. */
	virtual const FNiagaraCompileResults& CompileScript(class UNiagaraScript* InScript, FNiagaraTranslatorOutput *TranslatorOutput, FString& TranslatedHLSL) = 0;

	/** Adds an error to be reported to the user. Any error will lead to compilation failure. */
	virtual void Error(FText ErrorText) = 0 ;

	/** Adds a warning to be reported to the user. Warnings will not cause a compilation failure. */
	virtual void Warning(FText WarningText) = 0;
};
