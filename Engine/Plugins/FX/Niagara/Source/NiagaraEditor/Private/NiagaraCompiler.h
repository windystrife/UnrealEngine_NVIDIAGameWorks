// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraCommon.h"
#include "NiagaraHlslTranslator.h"
#include "INiagaraCompiler.h"
#include "Kismet2/CompilerResultsLog.h"

class Error;
class UNiagaraGraph;
class UNiagaraNode;
class UNiagaraNodeFunctionCall;
class UNiagaraScript;
class UNiagaraScriptSource;
struct FNiagaraTranslatorOutput;

class NIAGARAEDITOR_API FHlslNiagaraCompiler : public INiagaraCompiler
{
protected:
	/** The script we are compiling. */
	UNiagaraScript* Script;

	/** Message log. Automatically handles marking the NodeGraph with errors. */
	FCompilerResultsLog MessageLog;

	/** Captures information about a script compile. */
	FNiagaraCompileResults CompileResults;

public:

	FHlslNiagaraCompiler();
	virtual ~FHlslNiagaraCompiler() {}

	//Begin INiagaraCompiler Interface
	virtual const FNiagaraCompileResults& CompileScript(UNiagaraScript* InScript, FNiagaraTranslatorOutput *TranslatorOutput, FString& TranslatedHLSL) override;

	virtual void Error(FText ErrorText) override;
	virtual void Warning(FText WarningText) override;
};
