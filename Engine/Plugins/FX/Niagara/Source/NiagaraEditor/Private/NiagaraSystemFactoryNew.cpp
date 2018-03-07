// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "NiagaraSystemFactoryNew.h"
#include "CoreMinimal.h"
#include "Misc/ConfigCacheIni.h"
#include "NiagaraSystem.h"
#include "NiagaraScriptSource.h"
#include "NiagaraGraph.h"
#include "NiagaraEditorSettings.h"

#define LOCTEXT_NAMESPACE "NiagaraSystemFactory"

UNiagaraSystemFactoryNew::UNiagaraSystemFactoryNew(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{

	SupportedClass = UNiagaraSystem::StaticClass();
	bCreateNew = false;
	bEditAfterNew = true;
	bCreateNew = true;
}

UObject* UNiagaraSystemFactoryNew::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	check(Class->IsChildOf(UNiagaraSystem::StaticClass()));

	const UNiagaraEditorSettings* Settings = GetDefault<UNiagaraEditorSettings>();
	check(Settings);

	UNiagaraSystem* NewSystem;
	
	if (UNiagaraSystem* Default = Cast<UNiagaraSystem>(Settings->DefaultSystem.TryLoad()))
	{
		NewSystem = Cast<UNiagaraSystem>(StaticDuplicateObject(Default, InParent, Name, Flags, Class));
	}
	else
	{
		NewSystem = NewObject<UNiagaraSystem>(InParent, Class, Name, Flags | RF_Transactional);
	}

	InitializeSystem(NewSystem);

	return NewSystem;
}

void UNiagaraSystemFactoryNew::InitializeSystem(UNiagaraSystem* System)
{
	UNiagaraScript* SystemSpawnScript = System->GetSystemSpawnScript();
	UNiagaraScript* SystemUpdateScript = System->GetSystemUpdateScript();
	UNiagaraScript* SystemSpawnScriptSolo = System->GetSystemSpawnScript(true);
	UNiagaraScript* SystemUpdateScriptSolo = System->GetSystemUpdateScript(true);

	UNiagaraScriptSource* SystemScriptSource = NewObject<UNiagaraScriptSource>(SystemSpawnScript, "SystemScriptSource", RF_Transactional);
	SystemScriptSource->NodeGraph = NewObject<UNiagaraGraph>(SystemScriptSource, "SystemScriptGraph", RF_Transactional);

	SystemSpawnScript->SetSource(SystemScriptSource);
	SystemUpdateScript->SetSource(SystemScriptSource);
	SystemSpawnScriptSolo->SetSource(SystemScriptSource);
	SystemUpdateScriptSolo->SetSource(SystemScriptSource);
}

#undef LOCTEXT_NAMESPACE