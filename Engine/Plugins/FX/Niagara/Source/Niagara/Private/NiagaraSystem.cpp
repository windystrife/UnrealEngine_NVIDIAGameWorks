// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "NiagaraSystem.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraRenderer.h"
#include "NiagaraConstants.h"
#include "NiagaraScriptSourceBase.h"
#include "NiagaraCustomVersion.h"
#include "NiagaraModule.h"
#include "ModuleManager.h"

// UNiagaraSystemCategory::UNiagaraSystemCategory(const FObjectInitializer& ObjectInitializer)
// 	: Super(ObjectInitializer)
// {
// }

//////////////////////////////////////////////////////////////////////////

UNiagaraSystem::UNiagaraSystem(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

void UNiagaraSystem::PostInitProperties()
{
	Super::PostInitProperties();
#if WITH_EDITORONLY_DATA
	bAutoImportChangedEmitters = true;
#endif
	if (HasAnyFlags(RF_ClassDefaultObject | RF_NeedLoad) == false)
	{
		SystemSpawnScript = NewObject<UNiagaraScript>(this, "SystemSpawnScript", RF_Transactional);
		SystemSpawnScript->SetUsage(ENiagaraScriptUsage::SystemSpawnScript);

		SystemUpdateScript = NewObject<UNiagaraScript>(this, "SystemUpdateScript", RF_Transactional);
		SystemUpdateScript->SetUsage(ENiagaraScriptUsage::SystemUpdateScript);

		SystemSpawnScriptSolo = NewObject<UNiagaraScript>(this, "SystemSpawnScriptSolo", RF_Transactional);
		SystemSpawnScriptSolo->SetUsage(ENiagaraScriptUsage::SystemSpawnScript);

		SystemUpdateScriptSolo = NewObject<UNiagaraScript>(this, "SystemUpdateScriptSolo", RF_Transactional);
		SystemUpdateScriptSolo->SetUsage(ENiagaraScriptUsage::SystemUpdateScript);
	}
}

void UNiagaraSystem::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FNiagaraCustomVersion::GUID);
}

void UNiagaraSystem::PostLoad()
{
	Super::PostLoad();

	if (GIsEditor)
	{
		SetFlags(RF_Transactional);
	}

	// Check to see if our version is out of date. If so, we'll finally need to recompile.
	bool bNeedsRecompile = false;
	const int32 NiagaraVer = GetLinkerCustomVersion(FNiagaraCustomVersion::GUID);
	if (NiagaraVer < FNiagaraCustomVersion::LatestVersion)
	{
		bNeedsRecompile = true;
	}		
	
	// We will be using this later potentially, so make sure that it's postload is already up-to-date.
	if (SystemSpawnScript)
	{
		SystemSpawnScript->ConditionalPostLoad();
	}
	if (SystemUpdateScript)
	{
		SystemUpdateScript->ConditionalPostLoad();
	}

 	if (NiagaraVer < FNiagaraCustomVersion::SystemEmitterScriptSimulations)
 	{
 		bNeedsRecompile = true;
		SystemSpawnScriptSolo = NewObject<UNiagaraScript>(this, "SystemSpawnScriptSolo", RF_Transactional);
		SystemSpawnScriptSolo->SetUsage(ENiagaraScriptUsage::SystemSpawnScript);

		SystemUpdateScriptSolo = NewObject<UNiagaraScript>(this, "SystemUpdateScriptSolo", RF_Transactional);
		SystemUpdateScriptSolo->SetUsage(ENiagaraScriptUsage::SystemUpdateScript);
#if WITH_EDITORONLY_DATA
		if (SystemSpawnScript)
		{
			SystemSpawnScriptSolo->SetSource(SystemSpawnScript->GetSource());
		}

		if (SystemUpdateScript)
		{
			SystemUpdateScriptSolo->SetSource(SystemUpdateScript->GetSource());
		}
#endif
 	}

	if (bNeedsRecompile)
	{
		for (FNiagaraEmitterHandle& Handle : EmitterHandles)
		{
			UNiagaraEmitter* Props = Handle.GetInstance();
			if (Props)
			{
				// We will be refreshing later potentially, so make sure that it's postload is already up-to-date.
				Props->ConditionalPostLoad();
			}
		}
	}

#if WITH_EDITORONLY_DATA
	if (SystemSpawnScript == nullptr)
	{
		SystemSpawnScript = NewObject<UNiagaraScript>(this, "SystemSpawnScript", RF_Transactional);
		SystemSpawnScript->SetUsage(ENiagaraScriptUsage::SystemSpawnScript);
	}

	if (SystemUpdateScript == nullptr)
	{
		SystemUpdateScript = NewObject<UNiagaraScript>(this, "SystemUpdateScript", RF_Transactional);
		SystemUpdateScript->SetUsage(ENiagaraScriptUsage::SystemUpdateScript);
	}

	if (bNeedsRecompile)
	{
		Compile();
	}

	// Note that we do not call ResynchronizeAllHandles() or CheckForUpdates here.
	// There are multiple reasons:
	// 1) You can run into issues with the Linker table associated with this package when doing the deep copy during load. 
	// 2) We'd have to make sure that all of the referenced nodes that were in the old version of the graph have already been loaded and cleaned out.
	// Better to just CheckForUpdates anytime this System is used for the first time.
#endif
}

#if WITH_EDITORONLY_DATA
bool UNiagaraSystem::CheckForUpdates()
{
	// An emitter may have changed since last load, let's refresh if possible.
	if (bAutoImportChangedEmitters)
	{
		return ResynchronizeAllHandles();
	}
	return false;
}

UObject* UNiagaraSystem::GetEditorData()
{
	return EditorData;
}

const UObject* UNiagaraSystem::GetEditorData() const
{
	return EditorData;
}

void UNiagaraSystem::SetEditorData(UObject* InEditorData)
{
	EditorData = InEditorData;
}

bool UNiagaraSystem::ResynchronizeAllHandles()
{
	uint32 HandlexIdx = 0;
	bool bAnyRefreshed = false;
	for (FNiagaraEmitterHandle& Handle : EmitterHandles)
	{
		const UNiagaraEmitter* SrcPrps = Handle.GetSource();
		const UNiagaraEmitter* InstancePrps = Handle.GetInstance();

		// Check to see if the source and handle are synched. If they aren't go ahead and resynch.
		if (!Handle.IsSynchronizedWithSource())
		{
			// Todo.. eliminate synchronization until we have rewritten it...
#if 0
			if (Handle.RefreshFromSource())
			{
				check(SrcPrps->ChangeId == Handle.GetInstance()->ChangeId);
				UE_LOG(LogNiagara, Log, TEXT("Refreshed System %s emitter %d from %s"), *GetPathName(), HandlexIdx, *SrcPrps->GetPathName());
				bAnyRefreshed = true;
			}
#endif
		}
		HandlexIdx++;
	}

	if (bAnyRefreshed)
	{
		Compile();
	}

	return bAnyRefreshed;
}

bool UNiagaraSystem::ReferencesSourceEmitter(UNiagaraEmitter* Emitter)
{
	for (FNiagaraEmitterHandle& Handle : EmitterHandles)
	{
		if (Emitter == Handle.GetSource())
		{
			return true;
		}
	}
	return false;
}
#endif


const TArray<FNiagaraEmitterHandle>& UNiagaraSystem::GetEmitterHandles()
{
	return EmitterHandles;
}


bool UNiagaraSystem::IsValid()const
{
	if (!SystemSpawnScript->IsValid() || !SystemSpawnScriptSolo->IsValid() || !SystemUpdateScript->IsValid() || !SystemUpdateScriptSolo->IsValid())
	{
		return false;
	}

	for (const FNiagaraEmitterHandle& Handle : EmitterHandles)
	{
		if (!Handle.GetInstance()->IsValid())
		{
			return false;
		}
	}

	return true;
}
#if WITH_EDITORONLY_DATA

FNiagaraEmitterHandle UNiagaraSystem::AddEmitterHandle(const UNiagaraEmitter& SourceEmitter, FName EmitterName)
{
	FNiagaraEmitterHandle EmitterHandle(SourceEmitter, EmitterName, *this);
	EmitterHandles.Add(EmitterHandle);
	return EmitterHandle;
}

FNiagaraEmitterHandle UNiagaraSystem::AddEmitterHandleWithoutCopying(UNiagaraEmitter& Emitter)
{
	FNiagaraEmitterHandle EmitterHandle(Emitter);
	EmitterHandles.Add(EmitterHandle);
	return EmitterHandle;
}

FNiagaraEmitterHandle UNiagaraSystem::DuplicateEmitterHandle(const FNiagaraEmitterHandle& EmitterHandleToDuplicate, FName EmitterName)
{
	FNiagaraEmitterHandle EmitterHandle(EmitterHandleToDuplicate, EmitterName, *this);
	EmitterHandles.Add(EmitterHandle);
	return EmitterHandle;
}
#endif

void UNiagaraSystem::RemoveEmitterHandle(const FNiagaraEmitterHandle& EmitterHandleToDelete)
{
	auto RemovePredicate = [&](const FNiagaraEmitterHandle& EmitterHandle) { return EmitterHandle.GetId() == EmitterHandleToDelete.GetId(); };
	EmitterHandles.RemoveAll(RemovePredicate);
}

void UNiagaraSystem::RemoveEmitterHandlesById(const TSet<FGuid>& HandlesToRemove)
{
	auto RemovePredicate = [&](const FNiagaraEmitterHandle& EmitterHandle)
	{
		return HandlesToRemove.Contains(EmitterHandle.GetId());
	};
	EmitterHandles.RemoveAll(RemovePredicate);
}

UNiagaraScript* UNiagaraSystem::GetSystemSpawnScript(bool bSolo)
{
	return bSolo ? SystemSpawnScriptSolo : SystemSpawnScript;
}

UNiagaraScript* UNiagaraSystem::GetSystemUpdateScript(bool bSolo)
{
	return bSolo ? SystemUpdateScriptSolo : SystemUpdateScript;
}

#if WITH_EDITORONLY_DATA
bool UNiagaraSystem::GetAutoImportChangedEmitters() const
{
	return bAutoImportChangedEmitters;
}

void UNiagaraSystem::SetAutoImportChangedEmitters(bool bShouldImport)
{
	bAutoImportChangedEmitters = bShouldImport;
}

void UNiagaraSystem::CompileScripts(TArray<ENiagaraScriptCompileStatus>& OutScriptStatuses, TArray<FString>& OutGraphLevelErrorMessages, TArray<FString>& PathNames, TArray<UNiagaraScript*>& Scripts)
{
	PathNames.Empty();
	OutScriptStatuses.Empty();
	OutGraphLevelErrorMessages.Empty();

	TArray<FNiagaraVariable> ExposedVars;

	//Compile all emitters
	for (FNiagaraEmitterHandle Handle : EmitterHandles)
	{
		TArray<FString> ScriptErrors;
		UNiagaraScriptSourceBase* GraphSource = Handle.GetInstance()->GraphSource;
		check(!GraphSource->IsPreCompiled());
		GraphSource->PreCompile(Handle.GetInstance());

		Handle.GetInstance()->CompileScripts(OutScriptStatuses, ScriptErrors, PathNames, Scripts);

		GraphSource->GatherPreCompiledVariables(TEXT("User"), ExposedVars);
		GraphSource->PostCompile();
		OutGraphLevelErrorMessages.Append(ScriptErrors);
	}

	check(SystemSpawnScript->GetSource() == SystemUpdateScript->GetSource());
	check(SystemSpawnScript->GetSource()->IsPreCompiled() == false);

	SystemSpawnScript->GetSource()->PreCompile(nullptr);
	SystemSpawnScript->GetSource()->GatherPreCompiledVariables(TEXT("User"), ExposedVars);

	FString ErrorMsg;
	check((int32)EScriptCompileIndices::SpawnScript == 0);
	OutScriptStatuses.Add(SystemSpawnScript->Compile(ErrorMsg));
	OutGraphLevelErrorMessages.Add(ErrorMsg); // EScriptCompileIndices::SpawnScript
	Scripts.Add(SystemSpawnScript);
	PathNames.Add(SystemSpawnScript->GetPathName());

	ErrorMsg.Empty();
	check((int32)EScriptCompileIndices::UpdateScript == 1);
	OutScriptStatuses.Add(SystemUpdateScript->Compile(ErrorMsg));
	OutGraphLevelErrorMessages.Add(ErrorMsg); // EScriptCompileIndices::SpawnScript
	Scripts.Add(SystemUpdateScript);
	PathNames.Add(SystemUpdateScript->GetPathName());

	ErrorMsg.Empty();
	check((int32)EScriptCompileIndices::SpawnScript == 0);
	OutScriptStatuses.Add(SystemSpawnScriptSolo->Compile(ErrorMsg));
	OutGraphLevelErrorMessages.Add(ErrorMsg); // EScriptCompileIndices::SpawnScript
	Scripts.Add(SystemSpawnScriptSolo);
	PathNames.Add(SystemSpawnScriptSolo->GetPathName());

	ErrorMsg.Empty();
	check((int32)EScriptCompileIndices::UpdateScript == 1);
	OutScriptStatuses.Add(SystemUpdateScriptSolo->Compile(ErrorMsg));
	OutGraphLevelErrorMessages.Add(ErrorMsg); // EScriptCompileIndices::SpawnScript
	Scripts.Add(SystemUpdateScriptSolo);
	PathNames.Add(SystemUpdateScriptSolo->GetPathName());

	SystemSpawnScript->GetSource()->PostCompile();

	InitEmitterSpawnAttributes();
	
	// Now let's synchronize the variables that we expose to the end user.
	TArray<FNiagaraVariable> OriginalVars;
	ExposedParameters.GetParameters(OriginalVars);
	for (int32 i = 0; i < ExposedVars.Num(); i++)
	{
		if (OriginalVars.Contains(ExposedVars[i]) == false)
		{
			ExposedParameters.AddParameter(ExposedVars[i]);
		}
	}
	for (int32 i = 0; i < OriginalVars.Num(); i++)
	{
		if (ExposedVars.Contains(OriginalVars[i]) == false)
		{
			ExposedParameters.RemoveParameter(OriginalVars[i]);
		}
	}

	FNiagaraSystemUpdateContext UpdateCtx(this, true);

	INiagaraModule& NiagaraModule = FModuleManager::LoadModuleChecked<INiagaraModule>("Niagara");
	NiagaraModule.DestroyAllSystemSimulations(this);

	//I assume this was added to recompile emitters too but I'm doing that above. We need to rework this compilation flow.
	//Compile();
}

void UNiagaraSystem::Compile()
{
	TArray<FNiagaraVariable> ExposedVars;
	for (FNiagaraEmitterHandle Handle: EmitterHandles)
	{
		TArray<ENiagaraScriptCompileStatus> ScriptStatuses;
		TArray<FString> ScriptErrors;
		TArray<FString> PathNames;
		TArray<UNiagaraScript*> Scripts;
		
		UNiagaraScriptSourceBase* GraphSource = Handle.GetInstance()->GraphSource;
		check(!GraphSource->IsPreCompiled());
		GraphSource->PreCompile(Handle.GetInstance());

		Handle.GetInstance()->CompileScripts(ScriptStatuses, ScriptErrors, PathNames, Scripts);

		GraphSource->GatherPreCompiledVariables(TEXT("User"), ExposedVars);

		GraphSource->PostCompile();

		for (int32 i = 0; i < ScriptStatuses.Num(); i++)
		{
			if (ScriptErrors[i].Len() != 0 || ScriptStatuses[i] != ENiagaraScriptCompileStatus::NCS_UpToDate)
			{
				UE_LOG(LogNiagara, Warning, TEXT("Script '%s', compile status: %d  errors: %s"), *PathNames[i], (int32)ScriptStatuses[i], *ScriptErrors[i]);
			}
			else
			{
				UE_LOG(LogNiagara, Log, TEXT("Script '%s', compile status: Success!"), *PathNames[i]);
			}
		}
	}

	check(SystemSpawnScript->GetSource() == SystemUpdateScript->GetSource());
	check(SystemSpawnScript->GetSource()->IsPreCompiled() == false);

	SystemSpawnScript->GetSource()->PreCompile(nullptr);
	SystemSpawnScript->GetSource()->GatherPreCompiledVariables(TEXT("User"), ExposedVars);

	FString CompileErrors;
	ENiagaraScriptCompileStatus CompileStatus = SystemSpawnScript->Compile(CompileErrors);
	if (CompileErrors.Len() != 0 || CompileStatus != ENiagaraScriptCompileStatus::NCS_UpToDate)
	{
		UE_LOG(LogNiagara, Warning, TEXT("System Spawn Script '%s', compile status: %d  errors: %s"), *SystemSpawnScript->GetPathName(), (int32)CompileStatus, *CompileErrors);
	}
	else
	{
		UE_LOG(LogNiagara, Log, TEXT("Script Spawn '%s', compile status: Success!"), *SystemSpawnScript->GetPathName());
	}

	CompileStatus = SystemUpdateScript->Compile(CompileErrors);
	if (CompileErrors.Len() != 0 || CompileStatus != ENiagaraScriptCompileStatus::NCS_UpToDate)
	{
		UE_LOG(LogNiagara, Warning, TEXT("System Update Script '%s', compile status: %d  errors: %s"), *SystemUpdateScript->GetPathName(), (int32)CompileStatus, *CompileErrors);
	}
	else
	{
		UE_LOG(LogNiagara, Log, TEXT("Script Update '%s', compile status: Success!"), *SystemUpdateScript->GetPathName());
	}

	if(SystemSpawnScriptSolo)
	{
		CompileStatus = SystemSpawnScriptSolo->Compile(CompileErrors);
		if (CompileErrors.Len() != 0 || CompileStatus != ENiagaraScriptCompileStatus::NCS_UpToDate)
		{
			UE_LOG(LogNiagara, Warning, TEXT("System Spawn Solo Script '%s', compile status: %d  errors: %s"), *SystemSpawnScriptSolo->GetPathName(), (int32)CompileStatus, *CompileErrors);
		}
		else
		{
			UE_LOG(LogNiagara, Log, TEXT("Script Spawn Solo '%s', compile status: Success!"), *SystemSpawnScriptSolo->GetPathName());
		}
	}

	if (SystemUpdateScriptSolo)
	{
		CompileStatus = SystemUpdateScriptSolo->Compile(CompileErrors);
		if (CompileErrors.Len() != 0 || CompileStatus != ENiagaraScriptCompileStatus::NCS_UpToDate)
		{
			UE_LOG(LogNiagara, Warning, TEXT("System Update Solo Script '%s', compile status: %d  errors: %s"), *SystemUpdateScriptSolo->GetPathName(), (int32)CompileStatus, *CompileErrors);
		}
		else
		{
			UE_LOG(LogNiagara, Log, TEXT("Script Update Solo '%s', compile status: Success!"), *SystemUpdateScriptSolo->GetPathName());
		}
	}
	SystemSpawnScript->GetSource()->PostCompile();

	InitEmitterSpawnAttributes();
	TArray<FNiagaraVariable> OriginalVars;
	ExposedParameters.GetParameters(OriginalVars);
	for (int32 i = 0; i < ExposedVars.Num(); i++)
	{
		ExposedParameters.AddParameter(ExposedVars[i]);
	}
	for (int32 i = 0; i < OriginalVars.Num(); i++)
	{
		if (ExposedVars.Contains(OriginalVars[i]) == false)
		{
			ExposedParameters.RemoveParameter(OriginalVars[i]);
		}
	}

	FNiagaraSystemUpdateContext UpdateCtx(this, true);

	INiagaraModule& NiagaraModule = FModuleManager::LoadModuleChecked<INiagaraModule>("Niagara");
	NiagaraModule.DestroyAllSystemSimulations(this);
}
#endif

void UNiagaraSystem::InitEmitterSpawnAttributes()
{
	EmitterSpawnAttributes.Empty();
	EmitterSpawnAttributes.SetNum(EmitterHandles.Num());
	FNiagaraTypeDefinition SpawnInfoDef = FNiagaraTypeDefinition(FNiagaraSpawnInfo::StaticStruct());
	for (FNiagaraVariable& Var : SystemSpawnScript->Attributes)
	{
		for (int32 EmitterIdx = 0; EmitterIdx < EmitterHandles.Num(); ++EmitterIdx)
		{
			UNiagaraEmitter* Emitter = EmitterHandles[EmitterIdx].GetInstance();
			FString EmitterName = Emitter->GetUniqueEmitterName();
			if (Var.GetType() == SpawnInfoDef && Var.GetName().ToString().StartsWith(EmitterName))
			{
				EmitterSpawnAttributes[EmitterIdx].SpawnAttributes.AddUnique(Var.GetName());
			}
		}
	}
	for (FNiagaraVariable& Var : SystemUpdateScript->Attributes)
	{
		for (int32 EmitterIdx = 0; EmitterIdx < EmitterHandles.Num(); ++EmitterIdx)
		{
			UNiagaraEmitter* Emitter = EmitterHandles[EmitterIdx].GetInstance();
			FString EmitterName = Emitter->GetUniqueEmitterName();
			if (Var.GetType() == SpawnInfoDef && Var.GetName().ToString().StartsWith(EmitterName))
			{
				EmitterSpawnAttributes[EmitterIdx].SpawnAttributes.AddUnique(Var.GetName());
			}
		}
	}
}