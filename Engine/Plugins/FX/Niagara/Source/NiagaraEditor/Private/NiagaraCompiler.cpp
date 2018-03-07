// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "NiagaraCompiler.h"
#include "NiagaraHlslTranslator.h"
#include "NiagaraScriptSource.h"
#include "NiagaraGraph.h"
#include "NiagaraEditorModule.h"
#include "NiagaraComponent.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Interfaces/IShaderFormat.h"
#include "ShaderFormatVectorVM.h"
#include "NiagaraConstants.h"
#include "NiagaraSystem.h"
#include "NiagaraNodeEmitter.h"
#include "NiagaraNodeInput.h"

#include "NiagaraDataInterface.h"
#include "NiagaraDataInterfaceStaticMesh.h"
#include "NiagaraDataInterfaceCurlNoise.h"

#include "ShaderCore.h"

#define LOCTEXT_NAMESPACE "NiagaraCompiler"

DEFINE_LOG_CATEGORY_STATIC(LogNiagaraCompiler, All, All);


ENiagaraScriptCompileStatus FNiagaraEditorModule::CompileScript(UNiagaraScript* ScriptToCompile, FString& OutGraphLevelErrorMessages)
{
	check(ScriptToCompile != NULL);

	UNiagaraGraph* Graph = Cast<UNiagaraScriptSource>(ScriptToCompile->GetSource())->NodeGraph;

	OutGraphLevelErrorMessages.Empty();
	FNiagaraCompileResults Results;
	FHlslNiagaraCompiler Compiler;
	FNiagaraTranslateResults TranslateResults;
	FHlslNiagaraTranslator Translator;

	/*
	if (ScriptToCompile && ScriptToCompile->IsNonParticleScript())
	{
		ENiagaraScriptCompileStatus Status = ENiagaraScriptCompileStatus::NCS_UpToDate;
		if (ScriptToCompile->IsSystemSpawnScript())
		{
			Status = CompileSystemScript(ScriptToCompile, OutGraphLevelErrorMessages);
		}
		ScriptToCompile->SetChangeID(Graph->GetChangeID());
		ScriptToCompile->SetLastCompileStatus(Status);
		return Status;
	}
	else
	*/
	{
		FHlslNiagaraTranslatorOptions Options; 
		Options.SimTarget = ENiagaraSimTarget::CPUSim;
		TranslateResults = Translator.Translate(ScriptToCompile, Options);
		Results = Compiler.CompileScript(ScriptToCompile, &Translator.GetTranslateOutput(), Translator.GetTranslatedHLSL());
		ScriptToCompile->SetChangeID(Graph->GetChangeID());
		ScriptToCompile->GenerateStatScopeIDs();
	}

#if defined(NIAGARA_SCRIPT_COMPILE_LOGGING_MEDIUM)
	if (TranslateResults.bHLSLGenSucceeded)
	{
		UE_LOG(LogNiagaraCompiler, Log, TEXT("HLSL generation succeeded: %s"), *ScriptToCompile->GetPathName());
	}
	else
	{
		UE_LOG(LogNiagaraCompiler, Error, TEXT("HLSL generation failed: %s"), *ScriptToCompile->GetPathName());
	}

	if (Results.bVMSucceeded)
	{
		UE_LOG(LogNiagaraCompiler, Log, TEXT("CPU Compile succeeded: %s"), *ScriptToCompile->GetPathName());
	}
	else
	{
		UE_LOG(LogNiagaraCompiler, Error, TEXT("CPU Compile failed: %s"), *ScriptToCompile->GetPathName());
	}
#endif

	TArray<TSharedRef<FTokenizedMessage>> Messages;
	if (TranslateResults.MessageLog)
	{
		Messages.Append(TranslateResults.MessageLog->Messages);
	}
	if (Results.MessageLog)
	{
		Messages.Append(Results.MessageLog->Messages);
	}

	for (TSharedRef<FTokenizedMessage> Message : Messages)
	{
		if (Message->GetSeverity() == EMessageSeverity::Info)
		{
		#if defined(NIAGARA_SCRIPT_COMPILE_LOGGING_MEDIUM)
			UE_LOG(LogNiagaraCompiler, Log, TEXT("%s"), *Message->ToText().ToString());
		#endif
		}
		else if (Message->GetSeverity() == EMessageSeverity::Warning || Message->GetSeverity() == EMessageSeverity::PerformanceWarning)
		{
		#if defined(NIAGARA_SCRIPT_COMPILE_LOGGING_MEDIUM)
			UE_LOG(LogNiagaraCompiler, Warning, TEXT("%s"), *Message->ToText().ToString());
		#endif
		}
		else if (Message->GetSeverity() == EMessageSeverity::Error || Message->GetSeverity() == EMessageSeverity::CriticalError)
		{
		#if defined(NIAGARA_SCRIPT_COMPILE_LOGGING_MEDIUM)
			UE_LOG(LogNiagaraCompiler, Error, TEXT("%s"), *Message->ToText().ToString());
		#endif
			// Write the error messages to the string as well so that they can be echoed up the chain.
			if (OutGraphLevelErrorMessages.Len() > 0)
			{
				OutGraphLevelErrorMessages += "\n";
			}
			OutGraphLevelErrorMessages += Message->ToText().ToString();
		}
	}

#if defined(NIAGARA_SCRIPT_COMPILE_LOGGING_MEDIUM)
	UE_LOG(LogNiagaraCompiler, Log, TEXT("Compile output as text:"));
	UE_LOG(LogNiagaraCompiler, Log, TEXT("==================================================================================="));
	TArray<FString> OutputByLines;
	Results.OutputHLSL.ParseIntoArrayLines(OutputByLines, false);
	for (int32 i = 0; i < OutputByLines.Num(); i++)
	{
		UE_LOG(LogNiagaraCompiler, Log, TEXT("/*%04d*/\t\t%s"), i, *OutputByLines[i]);
	}
	UE_LOG(LogNiagaraCompiler, Log, TEXT("==================================================================================="));
#endif

	ScriptToCompile->SetLastCompileStatus(FNiagaraCompileResults::CompileResultsToSummary(&Results));
	return ScriptToCompile->GetLastCompileStatus();
}


ENiagaraScriptCompileStatus FNiagaraCompileResults::CompileResultsToSummary(const FNiagaraCompileResults* CompileResults)
{
	ENiagaraScriptCompileStatus SummaryStatus = ENiagaraScriptCompileStatus::NCS_Unknown;
	if (CompileResults != nullptr)
	{
		if (CompileResults->MessageLog->NumErrors > 0)
		{
			SummaryStatus = ENiagaraScriptCompileStatus::NCS_Error;
		}
		else
		{
			if (CompileResults->bVMSucceeded)
			{
				if (CompileResults->MessageLog->NumWarnings)
				{
					SummaryStatus = ENiagaraScriptCompileStatus::NCS_UpToDateWithWarnings;
				}
				else
				{
					SummaryStatus = ENiagaraScriptCompileStatus::NCS_UpToDate;
				}
			}

			if (CompileResults->bComputeSucceeded)
			{
				if (CompileResults->MessageLog->NumWarnings)
				{
					SummaryStatus = ENiagaraScriptCompileStatus::NCS_ComputeUpToDateWithWarnings;
				}
				else
				{
					SummaryStatus = ENiagaraScriptCompileStatus::NCS_UpToDate;
				}
			}
		}
	}
	return SummaryStatus;
}

const FNiagaraCompileResults &FHlslNiagaraCompiler::CompileScript(UNiagaraScript* InScript, FNiagaraTranslatorOutput *TranslatorOutput, FString &TranslatedHLSL)
{
	//TODO: This should probably be done via the same route that other shaders take through the shader compiler etc.
	//But that adds the complexity of a new shader type, new shader class and a new shader map to contain them etc.
	//Can do things simply for now.
	Script = InScript;
	Script->LastHlslTranslation = TEXT("");
	FShaderCompilerInput Input;
	Input.VirtualSourceFilePath = TEXT("/Engine/Private/NiagaraEmitterInstanceShader.usf");
	Input.EntryPointName = TEXT("SimulateMain");
	Input.Environment.SetDefine(TEXT("VM_SIMULATION"), 1);
	Input.Environment.IncludeVirtualPathToContentsMap.Add(TEXT("/Engine/Generated/NiagaraEmitterInstance.usf"), StringToArray<ANSICHAR>(*TranslatedHLSL, TranslatedHLSL.Len() + 1));
	FShaderCompilerOutput Output;

	FVectorVMCompilationOutput CompilationOutput;

	if (TranslatorOutput != nullptr && TranslatorOutput->Errors.Len() > 0)
	{
		//TODO: Map Lines of HLSL to their source Nodes and flag those nodes with errors associated with their lines.
		Error(FText::Format(LOCTEXT("HlslTranslateErrorMessageFormat", "The HLSL Translator failed.  Errors:\n{0}"), FText::FromString(TranslatorOutput->Errors)));
		CompileResults.bVMSucceeded = false;
	}
	else if (TranslatedHLSL.Len() == 0)
	{
		Error(LOCTEXT("HlslTranslateErrorMessageFormat", "The HLSL Translator failed to generate HLSL!"));
		CompileResults.bVMSucceeded = false;
	}
	else
	{
		CompileResults.bVMSucceeded = CompileShader_VectorVM(Input, Output, FString(FPlatformProcess::ShaderDir()), 0, CompilationOutput);
	}

	if (CompilationOutput.Errors.Len() > 0)
	{
		//TODO: Map Lines of HLSL to their source Nodes and flag those nodes with errors associated with their lines.
		Error(FText::Format(LOCTEXT("VectorVMCompileErrorMessageFormat", "The Vector VM compile failed.  Errors:\n{0}"), FText::FromString(CompilationOutput.Errors)));
		CompileResults.bVMSucceeded = false;
	}

	//For now we just copy the shader code over into the script. 
	//Eventually Niagara will have all the shader plumbing and do things like materials.
	if (CompileResults.bVMSucceeded)
	{
		check(TranslatorOutput);
		Script->ByteCode = CompilationOutput.ByteCode;
		Script->LastHlslTranslation = TranslatedHLSL;
		Script->Attributes = TranslatorOutput->Attributes;
		Script->Parameters = TranslatorOutput->Parameters;
		Script->DataUsage.bReadsAttriubteData = TranslatorOutput->bReadsAttributeData;
		//Build internal parameters
		Script->InternalParameters.Empty();
		for (int32 i = 0; i < CompilationOutput.InternalConstantOffsets.Num(); ++i)
		{
			EVectorVMBaseTypes Type = CompilationOutput.InternalConstantTypes[i];
			int32 Offset = CompilationOutput.InternalConstantOffsets[i];
			switch (Type)
			{
			case EVectorVMBaseTypes::Float:
			{
				float Val = *(float*)(CompilationOutput.InternalConstantData.GetData() + Offset);
				Script->InternalParameters.SetOrAdd(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), *LexicalConversion::ToString(Val)))->SetValue(Val);
			}
			break;
			case EVectorVMBaseTypes::Int:
			{
				int32 Val = *(int32*)(CompilationOutput.InternalConstantData.GetData() + Offset);
				Script->InternalParameters.SetOrAdd(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), *LexicalConversion::ToString(Val)))->SetValue(Val);
			}
			break;
			case EVectorVMBaseTypes::Bool:
			{
				int32 Val = *(int32*)(CompilationOutput.InternalConstantData.GetData() + Offset);
				Script->InternalParameters.SetOrAdd(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), Val == 0 ? TEXT("FALSE") : TEXT("TRUE")))->SetValue(Val);
			}
			break;
			}
		}

		Script->DataInterfaceInfo.Empty();
		for (FNiagaraScriptDataInterfaceInfo& DataInterfaceInfo : TranslatorOutput->DataInterfaceInfo)
		{
			int32 Idx = Script->DataInterfaceInfo.AddDefaulted();
			Script->DataInterfaceInfo[Idx].DataInterface = Cast<UNiagaraDataInterface>(StaticDuplicateObject(DataInterfaceInfo.DataInterface, Script, NAME_None, ~RF_Transient));
			Script->DataInterfaceInfo[Idx].Name = DataInterfaceInfo.Name;
			Script->DataInterfaceInfo[Idx].UserPtrIdx = DataInterfaceInfo.UserPtrIdx;
		}

		Script->NumUserPtrs = TranslatorOutput->NumUserPtrs;

		//Extract the external function call table binding info.
		Script->CalledVMExternalFunctions.Empty(CompilationOutput.CalledVMFunctionTable.Num());
		for (FVectorVMCompilationOutput::FCalledVMFunction& FuncInfo : CompilationOutput.CalledVMFunctionTable)
		{
			//Find the interface corresponding to this call.
			const FNiagaraFunctionSignature* Sig = nullptr;
			for (FNiagaraScriptDataInterfaceInfo& NDIInfo : TranslatorOutput->DataInterfaceInfo)
			{			
				Sig = NDIInfo.RegisteredFunctions.FindByPredicate([&](const FNiagaraFunctionSignature& CheckSig)
				{
					FString SigSymbol = FHlslNiagaraTranslator::GetFunctionSignatureSymbol(CheckSig);
					return SigSymbol == FuncInfo.Name;
				});
				if (Sig)
				{
					break;
				}
			}

			if (Sig)
			{
				int32 NewBindingIdx = Script->CalledVMExternalFunctions.AddDefaulted();
				Script->CalledVMExternalFunctions[NewBindingIdx].Name = *Sig->GetName();
				Script->CalledVMExternalFunctions[NewBindingIdx].OwnerName = Sig->OwnerName;

				Script->CalledVMExternalFunctions[NewBindingIdx].InputParamLocations = FuncInfo.InputParamLocations;
				Script->CalledVMExternalFunctions[NewBindingIdx].NumOutputs = FuncInfo.NumOutputs;
			}
			else
			{
				Error(FText::Format(LOCTEXT("VectorVMExternalFunctionBindingError", "Failed to bind the exernal function call:  {0}"), FText::FromString(FuncInfo.Name)));
				CompileResults.bVMSucceeded = false;
			}
		}
	}

	CompileResults.OutputHLSL = TranslatedHLSL;

	if (CompileResults.bVMSucceeded == false)
	{
		//Some error. Clear script and exit.
		Script->ByteCode.Empty();
		Script->Attributes.Empty();
		Script->Parameters.Empty();
		Script->InternalParameters.Empty();
		Script->DataInterfaceInfo.Empty();
//		Script->NumUserPtrs = 0;
	}

	return CompileResults;
}


FHlslNiagaraCompiler::FHlslNiagaraCompiler()
	: Script(nullptr)
	, CompileResults(&MessageLog)
{
	// Make the message log silent so we're not spamming the blueprint log.
	MessageLog.bSilentMode = true;
}



void FHlslNiagaraCompiler::Error(FText ErrorText)
{
	FString ErrorString = FString::Printf(TEXT("%s"), *ErrorText.ToString());
	MessageLog.Error(*ErrorString);
}

void FHlslNiagaraCompiler::Warning(FText WarningText)
{
	FString WarnString = FString::Printf(TEXT("%s"), *WarningText.ToString());
	MessageLog.Warning(*WarnString);
}

//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
