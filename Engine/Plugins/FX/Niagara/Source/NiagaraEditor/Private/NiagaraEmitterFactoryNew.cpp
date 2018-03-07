// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraEmitterFactoryNew.h"
#include "NiagaraEmitter.h"
#include "NiagaraEditorModule.h"
#include "NiagaraScriptFactoryNew.h"
#include "NiagaraGraph.h"
#include "NiagaraScriptSource.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraEditorSettings.h"

#include "ConfigCacheIni.h"

#define LOCTEXT_NAMESPACE "NiagaraEmitterFactory"

UNiagaraEmitterFactoryNew::UNiagaraEmitterFactoryNew(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	SupportedClass = UNiagaraEmitter::StaticClass();
	bCreateNew = false;
	bEditAfterNew = true;
	bCreateNew = true;
}

UObject* UNiagaraEmitterFactoryNew::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	check(Class->IsChildOf(UNiagaraEmitter::StaticClass()));

	const UNiagaraEditorSettings* Settings = GetDefault<UNiagaraEditorSettings>();
	check(Settings);

	UNiagaraEmitter* NewEmitter;

	if (UNiagaraEmitter* Default = Cast<UNiagaraEmitter>(Settings->DefaultEmitter.TryLoad()))
	{
		NewEmitter = Cast<UNiagaraEmitter>(StaticDuplicateObject(Default, InParent, Name, Flags, Class));
	}
	else
	{
		NewEmitter = NewObject<UNiagaraEmitter>(InParent, Class, Name, Flags | RF_Transactional);

		UNiagaraScriptSource* Source = NewObject<UNiagaraScriptSource>(NewEmitter, NAME_None, RF_Transactional);
		if (Source)
		{
			UNiagaraGraph* CreatedGraph = NewObject<UNiagaraGraph>(Source, NAME_None, RF_Transactional);
			Source->NodeGraph = CreatedGraph;

			// Set pointer in script to source
			NewEmitter->GraphSource = Source;
			NewEmitter->SpawnScriptProps.Script->SetSource(Source);
			NewEmitter->UpdateScriptProps.Script->SetSource(Source);
		}
	}
	
	return NewEmitter;
}

#undef LOCTEXT_NAMESPACE