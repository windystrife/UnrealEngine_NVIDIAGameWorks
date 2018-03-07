// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/ConfigCacheIni.h"
#include "Modules/ModuleManager.h"
#include "NiagaraCommon.h"
#include "NiagaraEditorModule.h"
#include "NiagaraScript.h"
#include "EdGraph/EdGraph.h"
#include "NiagaraGraph.h"
#include "NiagaraScriptSource.h"
#include "NiagaraScriptFactoryNew.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraEditorSettings.h"

#define LOCTEXT_NAMESPACE "NiagaraScriptFactory"

UNiagaraScriptFactoryNew::UNiagaraScriptFactoryNew(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

	SupportedClass = UNiagaraScript::StaticClass();
	bCreateNew = false;
	bEditAfterNew = true;
	bCreateNew = true;
}

UObject* UNiagaraScriptFactoryNew::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	check(Class->IsChildOf(UNiagaraScript::StaticClass()));
	
	const UNiagaraEditorSettings* Settings = GetDefault<UNiagaraEditorSettings>();
	check(Settings);

	UNiagaraScript* NewScript;

	if (UNiagaraScript* Default = Cast<UNiagaraScript>(Settings->DefaultScript.TryLoad()))
	{
		NewScript = Cast<UNiagaraScript>(StaticDuplicateObject(Default, InParent, Name, Flags, Class));
	}
	else
	{
		NewScript = NewObject<UNiagaraScript>(InParent, Class, Name, Flags | RF_Transactional);
		InitializeScript(NewScript);
	}

	return NewScript;
}

void UNiagaraScriptFactoryNew::InitializeScript(UNiagaraScript* NewScript)
{
	if(NewScript != NULL)
	{
		UNiagaraScriptSource* Source = NewObject<UNiagaraScriptSource>(NewScript, NAME_None, RF_Transactional);
		if(Source)
		{
			UNiagaraGraph* CreatedGraph = NewObject<UNiagaraGraph>(Source, NAME_None, RF_Transactional);
			Source->NodeGraph = CreatedGraph;

			FGraphNodeCreator<UNiagaraNodeOutput> OutputNodeCreator(*CreatedGraph);
			UNiagaraNodeOutput* OutputNode = OutputNodeCreator.CreateNode();

			FNiagaraVariable PosAttrib(FNiagaraTypeDefinition::GetVec3Def(), "Position");
			FNiagaraVariable VelAttrib(FNiagaraTypeDefinition::GetVec3Def(), "Velocity");
			FNiagaraVariable RotAttrib(FNiagaraTypeDefinition::GetFloatDef(), "Rotation");
			FNiagaraVariable AgeAttrib(FNiagaraTypeDefinition::GetFloatDef(), "NormalizedAge");
			FNiagaraVariable ColAttrib(FNiagaraTypeDefinition::GetColorDef(), "Color");
			FNiagaraVariable SizeAttrib(FNiagaraTypeDefinition::GetVec2Def(), "Size");

			OutputNode->Outputs.Add(PosAttrib);
			OutputNode->Outputs.Add(VelAttrib);
			OutputNode->Outputs.Add(RotAttrib);
			OutputNode->Outputs.Add(ColAttrib);
			OutputNode->Outputs.Add(SizeAttrib);
			OutputNode->Outputs.Add(AgeAttrib);

			OutputNodeCreator.Finalize();


			// Set pointer in script to source
			NewScript->SetSource(Source);

			FString ErrorMessages;
			FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::Get().LoadModuleChecked<FNiagaraEditorModule>(TEXT("NiagaraEditor"));
			NiagaraEditorModule.CompileScript(NewScript, ErrorMessages);
		}
	}
}

#undef LOCTEXT_NAMESPACE
