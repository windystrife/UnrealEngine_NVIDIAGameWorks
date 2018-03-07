// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraEmitterHandle.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraScriptSourceBase.h"
#include "NiagaraCommon.h"
#include "NiagaraDataInterface.h"

const FNiagaraEmitterHandle FNiagaraEmitterHandle::InvalidHandle;

FNiagaraEmitterHandle::FNiagaraEmitterHandle() 
	:
#if WITH_EDITORONLY_DATA
	Source(nullptr)
	,
#endif
	Instance(nullptr)
{
}

FNiagaraEmitterHandle::FNiagaraEmitterHandle(UNiagaraEmitter& Emitter)
	: Id(FGuid::NewGuid())
	, IdName(*Id.ToString())
	, bIsEnabled(true)
	, Name(TEXT("Emitter"))
#if WITH_EDITORONLY_DATA
	, Source(&Emitter)
#endif
	, Instance(&Emitter)
{
}

#if WITH_EDITORONLY_DATA
FNiagaraEmitterHandle::FNiagaraEmitterHandle(const UNiagaraEmitter& InSourceEmitter, FName InName, UNiagaraSystem& InOuterSystem)
	: Id(FGuid::NewGuid())
	, IdName(*Id.ToString())
	, bIsEnabled(true)
	, Name(InName)
	, Source(&InSourceEmitter)
	, Instance(InSourceEmitter.MakeRecursiveDeepCopy(&InOuterSystem))
{
}

FNiagaraEmitterHandle::FNiagaraEmitterHandle(const FNiagaraEmitterHandle& InHandleToDuplicate, FName InDuplicateName, UNiagaraSystem& InDuplicateOwnerSystem)
	: Id(FGuid::NewGuid())
	, IdName(*Id.ToString())
	, bIsEnabled(InHandleToDuplicate.bIsEnabled)
	, Name(InDuplicateName)
	, Source(InHandleToDuplicate.Source)
	, Instance(InHandleToDuplicate.Instance->MakeRecursiveDeepCopy(&InDuplicateOwnerSystem))
{
}
#endif

bool FNiagaraEmitterHandle::IsValid() const
{
	return Id.IsValid();
}

FGuid FNiagaraEmitterHandle::GetId() const
{
	return Id;
}

FName FNiagaraEmitterHandle::GetIdName() const
{
	return IdName;
}

FName FNiagaraEmitterHandle::GetName() const
{
	return Name;
}

void FNiagaraEmitterHandle::SetName(FName InName)
{
	Name = InName;
}

bool FNiagaraEmitterHandle::GetIsEnabled() const
{
	return bIsEnabled;
}

void FNiagaraEmitterHandle::SetIsEnabled(bool bInIsEnabled)
{
	bIsEnabled = bInIsEnabled;
}

#if WITH_EDITORONLY_DATA
const UNiagaraEmitter* FNiagaraEmitterHandle::GetSource() const
{
	return Source;
}

#endif

UNiagaraEmitter* FNiagaraEmitterHandle::GetInstance() const
{
	return Instance;
}

FString FNiagaraEmitterHandle::GetUniqueInstanceName()const
{
	check(Instance);
	return Instance->GetUniqueEmitterName();
}

void FNiagaraEmitterHandle::SetInstance(UNiagaraEmitter* InInstance)
{
	Instance = InInstance;
}
#if WITH_EDITORONLY_DATA

void FNiagaraEmitterHandle::ResetToSource()
{
	UObject* Outer = GetInstance()->GetOuter();
	Instance = nullptr; // Clear out our reference to this object first..
	Instance = GetSource()->MakeRecursiveDeepCopy(Outer);
}

void CopyParameterValues(UNiagaraScript* Script, UNiagaraScript* PreviousScript)
{
	for (FNiagaraVariable& InputParameter : Script->Parameters.Parameters)
	{
		for (FNiagaraVariable& PreviousInputParameter : PreviousScript->Parameters.Parameters)
		{
			if (PreviousInputParameter.IsDataAllocated())
			{
				if (InputParameter.GetName() == PreviousInputParameter.GetName() &&
					InputParameter.GetType() == PreviousInputParameter.GetType())
				{
					InputParameter.AllocateData();
					PreviousInputParameter.CopyTo(InputParameter.GetData());
				}
			}
		}
	}
	
	for (FNiagaraScriptDataInterfaceInfo& InputInfo : Script->DataInterfaceInfo)
	{
		for (FNiagaraScriptDataInterfaceInfo&  PreviousInputInfo : PreviousScript->DataInterfaceInfo)
		{
			if (InputInfo.Name == PreviousInputInfo.Name &&
				InputInfo.DataInterface->GetClass() == PreviousInputInfo.DataInterface->GetClass())
			{
				PreviousInputInfo.CopyTo(&InputInfo, Script->GetOuter());
			}
		}
	}
}

bool FNiagaraEmitterHandle::IsSynchronizedWithSource() const
{
	if (Instance && Source && Source->ChangeId.IsValid() && Instance->ChangeId.IsValid())
	{
		return Instance->ChangeId == Source->ChangeId;		
	}
	return false;
}

bool FNiagaraEmitterHandle::NeedsRecompile() const
{
	TArray<UNiagaraScript*> Scripts;
	Instance->GetScripts(Scripts);

	for (UNiagaraScript* Script : Scripts)
	{
		if (!Script->AreScriptAndSourceSynchronized())
		{
			return true;
		}
	}
	return false;
}

bool FNiagaraEmitterHandle::RefreshFromSource()
{
	// TODO: Update this to support events.
	UNiagaraScript* PreviousInstanceSpawnScript = Instance->SpawnScriptProps.Script;
	UNiagaraScript* PreviousInstanceUpdateScript = Instance->UpdateScriptProps.Script;
	TMap<const UObject*, UObject*> ExistingConversions;

	if (Source == nullptr)
	{
		return false;
	}

	// First we need to deep copy the graph, which is shared amongst children
	UNiagaraScriptSourceBase* GraphSource = Source->GraphSource->MakeRecursiveDeepCopy(Instance, ExistingConversions);
	ExistingConversions.Add(Source->GraphSource, GraphSource);

	// The script graphs will look up from ExistingConversions
	UNiagaraScript* NewSpawnScript = Source->SpawnScriptProps.Script->MakeRecursiveDeepCopy(Instance, ExistingConversions);
	UNiagaraScript* NewUpdateScript = Source->UpdateScriptProps.Script->MakeRecursiveDeepCopy(Instance, ExistingConversions);

	if (NewSpawnScript->GetLastCompileStatus() == ENiagaraScriptCompileStatus::NCS_UpToDate && NewUpdateScript->GetLastCompileStatus() == ENiagaraScriptCompileStatus::NCS_UpToDate)
	{
		Instance->GraphSource = GraphSource;
		Instance->ChangeId = Source->ChangeId;
		Instance->SpawnScriptProps.Script = NewSpawnScript;
		Instance->UpdateScriptProps.Script = NewUpdateScript;
		
		// We may have overridden the interpolated spawn script value from the default. If so, this requires
		// a recompile.
		bool bNeedRecompile = false;
		if ((int32)Instance->SpawnScriptProps.Script->IsInterpolatedParticleSpawnScript() != Instance->bInterpolatedSpawning)
		{
			Instance->SpawnScriptProps.Script->SetUsage(ENiagaraScriptUsage::ParticleSpawnScriptInterpolated);
			bNeedRecompile = true;
		}

		int32 NumDifference = Source->EventHandlerScriptProps.Num() - Instance->EventHandlerScriptProps.Num();
		if (NumDifference < 0)
		{
			Instance->EventHandlerScriptProps.SetNum(Source->EventHandlerScriptProps.Num());
		}
		else if (NumDifference > 0)
		{
			Instance->EventHandlerScriptProps.AddDefaulted(NumDifference);
		}
		check(Source->EventHandlerScriptProps.Num() == Instance->EventHandlerScriptProps.Num());

		for (int32 i = 0; i < Source->EventHandlerScriptProps.Num(); i++)
		{
			if (Instance->EventHandlerScriptProps[i].Script == nullptr)
			{
				Instance->EventHandlerScriptProps[i] = Source->EventHandlerScriptProps[i];
				Instance->EventHandlerScriptProps[i].Script = Source->EventHandlerScriptProps[i].Script->MakeRecursiveDeepCopy(Instance, ExistingConversions);
				check(Instance->EventHandlerScriptProps[i].Script->GetSource() == Instance->GraphSource);

			}
			else if (Source->EventHandlerScriptProps[i].Script != nullptr && Source->GraphSource == Source->EventHandlerScriptProps[i].Script->GetSource())
			{
				// Make sure that we copy all required variables and update our bytecode too.
				Instance->EventHandlerScriptProps[i].Script = Source->EventHandlerScriptProps[i].Script->MakeRecursiveDeepCopy(Instance, ExistingConversions);
				check(Instance->EventHandlerScriptProps[i].Script->GetSource() == Instance->GraphSource);
			}
		}

		if (bNeedRecompile)
		{
			TArray<ENiagaraScriptCompileStatus> OutScriptStatuses;
			TArray<FString> OutGraphLevelErrorMessages;
			TArray<FString> PathNames;
			TArray<UNiagaraScript*> Scripts;
			Instance->CompileScripts(OutScriptStatuses, OutGraphLevelErrorMessages, PathNames, Scripts);

			//ensure that we are now sync'ed with source
			Instance->ChangeId = Source->ChangeId;
		}

		// We wait until after the potential recompile above to copy the old values over because
		// the script parameters array is also written by the CompileScripts call.
		CopyParameterValues(Instance->SpawnScriptProps.Script, PreviousInstanceSpawnScript);
		CopyParameterValues(Instance->UpdateScriptProps.Script, PreviousInstanceUpdateScript);
		check(Instance->SpawnScriptProps.Script->GetSource() == Instance->GraphSource);
		check(Instance->UpdateScriptProps.Script->GetSource() == Instance->GraphSource);

		UE_LOG(LogNiagara, Log, TEXT("Successful refresh from source %s"), *Instance->GetFullName());
		check((int32)Instance->SpawnScriptProps.Script->IsInterpolatedParticleSpawnScript() == Instance->bInterpolatedSpawning);

		Instance->SpawnScriptProps.InitDataSetAccess();
		Instance->UpdateScriptProps.InitDataSetAccess();
		for (int32 j = 0; j < Instance->EventHandlerScriptProps.Num(); j++)
		{
			Instance->EventHandlerScriptProps[j].InitDataSetAccess();
		}
		
		return true;
	}
	else
	{
		UE_LOG(LogNiagara, Warning, TEXT("Failed to refresh from source %s"), *Instance->GetFullName());
	}
	return false;
}
#endif