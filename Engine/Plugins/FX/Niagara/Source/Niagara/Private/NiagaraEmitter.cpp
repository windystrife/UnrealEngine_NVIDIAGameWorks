// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "NiagaraEmitter.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSourceBase.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraCustomVersion.h"
#include "Package.h"
#include "Linker.h"
#include "NiagaraModule.h"

static int32 GbForceNiagaraCompileOnLoad = 0;
static FAutoConsoleVariableRef CVarForceNiagaraCompileOnLoad(
	TEXT("fx.ForceCompileOnLoad"),
	GbForceNiagaraCompileOnLoad,
	TEXT("If > 0 emitters will be forced to compile on load. \n"),
	ECVF_Default
	);


void FNiagaraEmitterScriptProperties::InitDataSetAccess()
{
	EventReceivers.Empty();
	EventGenerators.Empty();

	if (Script)
	{
		// TODO: add event receiver and generator lists to the script properties here
		//
		for (FNiagaraDataSetID &ReadID : Script->ReadDataSets)
		{
			EventReceivers.Add( FNiagaraEventReceiverProperties(ReadID.Name, "", "") );
		}

		for (FNiagaraDataSetProperties &WriteID : Script->WriteDataSets)
		{
			FNiagaraEventGeneratorProperties Props(WriteID, "", "");
			EventGenerators.Add(Props);
		}
	}
}

//////////////////////////////////////////////////////////////////////////

UNiagaraEmitter::UNiagaraEmitter(const FObjectInitializer& Initializer)
: Super(Initializer)
, CollisionMode(ENiagaraCollisionMode::None)
, bInterpolatedSpawning(false)
{
}

void UNiagaraEmitter::PostInitProperties()
{
	Super::PostInitProperties();
	if (HasAnyFlags(RF_ClassDefaultObject | RF_NeedLoad) == false)
	{
		RendererProperties.Add(NewObject<UNiagaraSpriteRendererProperties>(this, "Renderer"));

		SpawnScriptProps.Script = NewObject<UNiagaraScript>(this, "SpawnScript", EObjectFlags::RF_Transactional);
		SpawnScriptProps.Script->SetUsage(ENiagaraScriptUsage::ParticleSpawnScript);

		UpdateScriptProps.Script = NewObject<UNiagaraScript>(this, "UpdateScript", EObjectFlags::RF_Transactional);
		UpdateScriptProps.Script->SetUsage(ENiagaraScriptUsage::ParticleUpdateScript);
	}
}

void UNiagaraEmitter::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FNiagaraCustomVersion::GUID);
}

void UNiagaraEmitter::PostLoad()
{
	Super::PostLoad();

	if (GIsEditor)
	{
		SetFlags(RF_Transactional);
	}

// 	//Have to force the usage to be correct.
// 	if (SpawnScriptProps.Script)
// 	{
// 		SpawnScriptProps.Script->ConditionalPostLoad();
// 		ENiagaraScriptUsage ActualUsage = bInterpolatedSpawning ? ENiagaraScriptUsage::ParticleSpawnScriptInterpolated : ENiagaraScriptUsage::ParticleSpawnScript;
// 		if (SpawnScriptProps.Script->Usage != ActualUsage)
// 		{
// 			SpawnScriptProps.Script->Usage = ActualUsage;
// 			SpawnScriptProps.Script->ByteCode.Empty();//Force them to recompile.
// 			UE_LOG(LogNiagara, Warning, TEXT("Niagara Emitter had mis matched bInterpolatedSpawning values. You must recompile this emitter. %s"), *GetFullName());
// 		}
// 	}
		
	//Temporarily disabling interpolated spawn.
	if (SpawnScriptProps.Script)
	{
		SpawnScriptProps.Script->ConditionalPostLoad();
		bool bActualInterpolatedSpawning = SpawnScriptProps.Script->IsInterpolatedParticleSpawnScript();
		if (bInterpolatedSpawning != bActualInterpolatedSpawning)
		{
			bInterpolatedSpawning = false;
			if (bActualInterpolatedSpawning)
			{
				SpawnScriptProps.Script->ByteCode.Empty();//clear out the script as it was compiled with interpolated spawn.
#if WITH_EDITORONLY_DATA
				SpawnScriptProps.Script->InvalidateChangeID(); // Identify that we have no idea if we're synchronized or not.
#endif
				SpawnScriptProps.Script->SetUsage(ENiagaraScriptUsage::ParticleSpawnScript);
			}
			UE_LOG(LogNiagara, Warning, TEXT("Temporarily disabling interpolated spawn. Emitter may need recompile.. %s"), *GetFullName());
		}
	}

	// Referenced scripts may need to be invalidated due to version mismatch or 
	// other issues. This is determined in PostLoad for UNiagaraScript, so we need
	// to make sure that PostLoad has happened for each of them first.
	bool bNeedsRecompile = false;
	if (SpawnScriptProps.Script)
	{
		SpawnScriptProps.Script->ConditionalPostLoad();
		if (SpawnScriptProps.Script->ByteCode.Num() == 0 || GbForceNiagaraCompileOnLoad > 0)
		{
			bNeedsRecompile = true;
		}
	}
	if (UpdateScriptProps.Script)
	{
		UpdateScriptProps.Script->ConditionalPostLoad();
		if (UpdateScriptProps.Script->ByteCode.Num() == 0 || GbForceNiagaraCompileOnLoad > 0)
		{
			bNeedsRecompile = true;
		}
	}

	for (int32 i = 0; i < EventHandlerScriptProps.Num(); i++)
	{
		if (EventHandlerScriptProps[i].Script)
		{
			EventHandlerScriptProps[i].Script->ConditionalPostLoad();
			if (EventHandlerScriptProps[i].Script->ByteCode.Num() == 0 || GbForceNiagaraCompileOnLoad > 0)
			{
				bNeedsRecompile = true;
			}
		}
	}

	// Check ourselves against the current script rebuild version. Force our children
	// to need to update.
	const int32 NiagaraVer = GetLinkerCustomVersion(FNiagaraCustomVersion::GUID);
	if (NiagaraVer < FNiagaraCustomVersion::LatestVersion)
	{
		bNeedsRecompile = true;
	}
	
	if (bNeedsRecompile)
	{
		if (SpawnScriptProps.Script)
		{
			SpawnScriptProps.Script->ByteCode.Empty();//clear out the script.
#if WITH_EDITORONLY_DATA
			SpawnScriptProps.Script->InvalidateChangeID(); // Identify that we have no idea if we're synchronized or not.
#endif
		}
		if (UpdateScriptProps.Script)
		{
			UpdateScriptProps.Script->ByteCode.Empty();//clear out the script.
#if WITH_EDITORONLY_DATA
			UpdateScriptProps.Script->InvalidateChangeID(); // Identify that we have no idea if we're synchronized or not.
#endif
		}

		for (int32 i = 0; i < EventHandlerScriptProps.Num(); i++)
		{
			if (EventHandlerScriptProps[i].Script)
			{
				EventHandlerScriptProps[i].Script->ByteCode.Empty();//clear out the script.
#if WITH_EDITORONLY_DATA
				EventHandlerScriptProps[i].Script->InvalidateChangeID(); // Identify that we have no idea if we're synchronized or not.
#endif
			}
		}
	}

#if WITH_EDITORONLY_DATA
	// Go ahead and do a recompile for all referenced scripts. Note that the compile will 
	// cause the emitter's change id to be regenerated.
	if (bNeedsRecompile)
	{
		UObject* Outer = GetOuter();
		if (Outer != nullptr && Outer->IsA<UPackage>())
		{
			TArray<ENiagaraScriptCompileStatus> ScriptStatuses;
			TArray<FString> ScriptErrors;
			TArray<FString> PathNames;
			TArray<UNiagaraScript*> Scripts;
			CompileScripts(ScriptStatuses, ScriptErrors, PathNames, Scripts);

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
	}
#endif
}

#if WITH_EDITOR
void UNiagaraEmitter::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FName PropertyName;
	if (PropertyChangedEvent.Property)
	{
		PropertyName = PropertyChangedEvent.Property->GetFName();
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UNiagaraEmitter, bInterpolatedSpawning))
	{
		bool bActualInterpolatedSpawning = SpawnScriptProps.Script->IsInterpolatedParticleSpawnScript();
		if (bInterpolatedSpawning != bActualInterpolatedSpawning)
		{
			//Recompile spawn script if we've altered the interpolated spawn property.
			SpawnScriptProps.Script->SetUsage(bInterpolatedSpawning ? ENiagaraScriptUsage::ParticleSpawnScriptInterpolated : ENiagaraScriptUsage::ParticleSpawnScript);
			UE_LOG(LogNiagara, Log, TEXT("Updating script usage: Script->IsInterpolatdSpawn %d Emitter->bInterpolatedSpawning %d"), (int32)SpawnScriptProps.Script->IsInterpolatedParticleSpawnScript(), bInterpolatedSpawning);
			if (GraphSource != nullptr)
			{
				GraphSource->MarkNotSynchronized();
			}
		}
	}
}
#endif


bool UNiagaraEmitter::IsValid()const
{
	return SpawnScriptProps.Script && SpawnScriptProps.Script->IsValid() && UpdateScriptProps.Script && UpdateScriptProps.Script->IsValid();
}

#if WITH_EDITORONLY_DATA

void UNiagaraEmitter::GetScripts(TArray<UNiagaraScript*>& OutScripts)
{
	OutScripts.Add(SpawnScriptProps.Script);
	OutScripts.Add(UpdateScriptProps.Script);
	for (int32 i = 0; i < EventHandlerScriptProps.Num(); i++)
	{
		if (EventHandlerScriptProps[i].Script)
		{
			OutScripts.Add(EventHandlerScriptProps[i].Script);
		}
	}
}

void UNiagaraEmitter::CompileScripts(TArray<ENiagaraScriptCompileStatus>& OutScriptStatuses, TArray<FString>& OutGraphLevelErrorMessages, TArray<FString>& PathNames,
	TArray<UNiagaraScript*>& Scripts)
{
	PathNames.AddDefaulted(2);
	OutScriptStatuses.Empty();
	OutGraphLevelErrorMessages.Empty();
	bool bPostCompile = false;

	// Force the end-user to reopen the file to resolve the problems.
	if (GraphSource == nullptr)
	{
		OutScriptStatuses.AddDefaulted(2);
		OutGraphLevelErrorMessages.AddDefaulted(2);

		OutGraphLevelErrorMessages[0] = OutGraphLevelErrorMessages[1] = TEXT("Please reopen asset in editor.");
		OutScriptStatuses[0] = OutScriptStatuses[1] = ENiagaraScriptCompileStatus::NCS_Error;
		return;
	}
	else if (!GraphSource->IsPreCompiled()) // If we haven't already precompiled this source, do so..
	{
		GraphSource->PreCompile(this);
		bPostCompile = true;
	}

	FString ErrorMsg;
	check((int32)EScriptCompileIndices::SpawnScript == 0);
	OutScriptStatuses.Add(SpawnScriptProps.Script->Compile(ErrorMsg));
	OutGraphLevelErrorMessages.Add(ErrorMsg); // EScriptCompileIndices::SpawnScript
	Scripts.Add(SpawnScriptProps.Script);
	SpawnScriptProps.InitDataSetAccess();
	PathNames[(int32)EScriptCompileIndices::SpawnScript] = SpawnScriptProps.Script->GetPathName();

	ErrorMsg.Empty();
	check((int32)EScriptCompileIndices::UpdateScript == 1);
	OutScriptStatuses.Add(UpdateScriptProps.Script->Compile(ErrorMsg));
	Scripts.Add(UpdateScriptProps.Script);
	OutGraphLevelErrorMessages.Add(ErrorMsg); // EScriptCompileIndices::UpdateScript
	UpdateScriptProps.InitDataSetAccess();
	PathNames[(int32)EScriptCompileIndices::UpdateScript] = UpdateScriptProps.Script->GetPathName();

	for (int32 i = 0; i < EventHandlerScriptProps.Num(); i++)
	{
		if (EventHandlerScriptProps[i].Script)
		{
			check((int32)EScriptCompileIndices::EventScript == 2);
			ErrorMsg.Empty();
			OutScriptStatuses.Add(EventHandlerScriptProps[i].Script->Compile(ErrorMsg));
			Scripts.Add(EventHandlerScriptProps[i].Script);
			OutGraphLevelErrorMessages.Add(ErrorMsg); // EScriptCompileIndices::EventScript
			EventHandlerScriptProps[i].InitDataSetAccess();
			PathNames.Add(EventHandlerScriptProps[i].Script->GetPathName());
		}
	}

	// Go through all attached renderers and make sure that the scripts work properly for those renderers.
	for (int32 ScriptIdx = 0; ScriptIdx < Scripts.Num(); ScriptIdx++)
	{
		// Only care about spawn and update
		if (ScriptIdx >= 2)
		{
			continue;
		}

		// Only care about scripts that successfully compiled so far.
		if (OutScriptStatuses[ScriptIdx] != ENiagaraScriptCompileStatus::NCS_UpToDate)
		{
			continue;
		}

		// Ignore missing scripts.
		if (Scripts[ScriptIdx] == nullptr)
		{
			continue;
		}

		for (int32 i = 0; i < RendererProperties.Num(); i++)
		{
			if (RendererProperties[i] != nullptr)
			{
				const TArray<FNiagaraVariable>& RequiredAttrs = RendererProperties[i]->GetRequiredAttributes();

				for (FNiagaraVariable Attr : RequiredAttrs)
				{
					// TODO .. should we always be namespaced?
					FString AttrName = Attr.GetName().ToString();
					if (AttrName.RemoveFromStart(TEXT("Particles.")))
					{
						Attr.SetName(*AttrName);
					}

					if (false == Scripts[ScriptIdx]->Attributes.ContainsByPredicate([&Attr](const FNiagaraVariable& Var) { return Var.GetName() == Attr.GetName(); }))
					{
						OutGraphLevelErrorMessages[ScriptIdx] += FString::Printf(TEXT("\nCannot bind to renderer %s because it does not define attribute %s %s."), *RendererProperties[i]->GetName(), *Attr.GetType().GetNameText().ToString(), *Attr.GetName().ToString());
						OutScriptStatuses[ScriptIdx] = ENiagaraScriptCompileStatus::NCS_Error;
					}
				}
			}
		}
	}

	ChangeId = FGuid::NewGuid();

	if (GraphSource && bPostCompile) // Cleanup if we precompiled inside this function
	{
		GraphSource->PostCompile();
	}
}

ENiagaraScriptCompileStatus UNiagaraEmitter::CompileScript(EScriptCompileIndices InScriptToCompile, FString& OutGraphLevelErrorMessages, int32 InSubScriptIdx)
{
	ENiagaraScriptCompileStatus CompileStatus = ENiagaraScriptCompileStatus::NCS_Unknown;
	bool bPostCompile = false;
	if (GraphSource && !GraphSource->IsPreCompiled()) // If we haven't already precompiled this source, do so..
	{
		bPostCompile = true;
		GraphSource->PreCompile(this);
	}

	switch (InScriptToCompile)
	{
	case EScriptCompileIndices::SpawnScript:
		CompileStatus = SpawnScriptProps.Script->Compile(OutGraphLevelErrorMessages);
		SpawnScriptProps.InitDataSetAccess();
		break;
	case EScriptCompileIndices::UpdateScript:
		CompileStatus = UpdateScriptProps.Script->Compile(OutGraphLevelErrorMessages);
		UpdateScriptProps.InitDataSetAccess();
		break;
	case EScriptCompileIndices::EventScript:
		if (EventHandlerScriptProps[InSubScriptIdx].Script)
		{
			CompileStatus = EventHandlerScriptProps[InSubScriptIdx].Script->Compile(OutGraphLevelErrorMessages);
			EventHandlerScriptProps[InSubScriptIdx].InitDataSetAccess();
		}
		else
		{
			return CompileStatus;
		}
		break;
	default:
		return CompileStatus;
	}
	ChangeId = FGuid::NewGuid();

	if (GraphSource && bPostCompile) // Cleanup if we precompiled inside this function
	{
		GraphSource->PostCompile();
	}
	return CompileStatus;
}

UNiagaraEmitter* UNiagaraEmitter::MakeRecursiveDeepCopy(UObject* DestOuter) const
{
	TMap<const UObject*, UObject*> ExistingConversions;
	return MakeRecursiveDeepCopy(DestOuter, ExistingConversions);
}

UNiagaraEmitter* UNiagaraEmitter::MakeRecursiveDeepCopy(UObject* DestOuter, TMap<const UObject*, UObject*>& ExistingConversions) const
{
	ResetLoaders(GetTransientPackage());
	GetTransientPackage()->LinkerCustomVersion.Empty();

	EObjectFlags Flags = RF_AllFlags & ~RF_Standalone & ~RF_Public; // Remove Standalone and Public flags..
	UNiagaraEmitter* Props = CastChecked<UNiagaraEmitter>(StaticDuplicateObject(this, GetTransientPackage(), *GetName(), Flags));
	check(Props->HasAnyFlags(RF_Standalone) == false);
	check(Props->HasAnyFlags(RF_Public) == false);
	Props->Rename(nullptr, DestOuter, REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
	UE_LOG(LogNiagara, Warning, TEXT("MakeRecursiveDeepCopy %s"), *Props->GetFullName());
	ExistingConversions.Add(this, Props);

	check(GraphSource != Props->GraphSource);

	Props->GraphSource->SubsumeExternalDependencies(ExistingConversions);
	ExistingConversions.Add(GraphSource, Props->GraphSource);

	// Suck in the referenced scripts into this package.
	if (Props->SpawnScriptProps.Script)
	{
		Props->SpawnScriptProps.Script->SubsumeExternalDependencies(ExistingConversions);
		check(Props->GraphSource == Props->SpawnScriptProps.Script->GetSource());
	}

	if (Props->UpdateScriptProps.Script)
	{
		Props->UpdateScriptProps.Script->SubsumeExternalDependencies(ExistingConversions);
		check(Props->GraphSource == Props->UpdateScriptProps.Script->GetSource());
	}

	for (int32 i = 0; i < Props->EventHandlerScriptProps.Num(); i++)
	{
		if (Props->EventHandlerScriptProps[i].Script)
		{
			Props->EventHandlerScriptProps[i].Script->SubsumeExternalDependencies(ExistingConversions);
			check(Props->GraphSource == Props->EventHandlerScriptProps[i].Script->GetSource());
		}
	}
	return Props;
}
#endif

bool UNiagaraEmitter::UsesScript(const UNiagaraScript* Script)const
{
	if (SpawnScriptProps.Script == Script || UpdateScriptProps.Script == Script)
	{
		return true;
	}
	for (int32 i = 0; i < EventHandlerScriptProps.Num(); i++)
	{
		if (EventHandlerScriptProps[i].Script == Script)
		{
			return true;
		}
	}
	return false;
}

//TODO
// bool UNiagaraEmitter::UsesDataInterface(UNiagaraDataInterface* Interface)
//{
//}

bool UNiagaraEmitter::UsesCollection(const class UNiagaraParameterCollection* Collection)const
{
	if (SpawnScriptProps.Script && SpawnScriptProps.Script->UsesCollection(Collection))
	{
		return true;
	}
	if (UpdateScriptProps.Script && UpdateScriptProps.Script->UsesCollection(Collection))
	{
		return true;
	}
	for (int32 i = 0; i < EventHandlerScriptProps.Num(); i++)
	{
		if (EventHandlerScriptProps[i].Script && EventHandlerScriptProps[i].Script->UsesCollection(Collection))
		{
			return true;
		}
	}
	return false;
}

FString UNiagaraEmitter::GetUniqueEmitterName()const
{
	return GetName() + LexicalConversion::ToString(FCrc::StrCrc32(*GetPathName()));
}

FNiagaraVariable UNiagaraEmitter::GetEmitterParameter(const FNiagaraVariable& EmitterVar)const
{
	FNiagaraVariable Var = EmitterVar;
	Var.SetName(*Var.GetName().ToString().Replace(TEXT("Emitter."), *(GetUniqueEmitterName() + TEXT("."))));
	return Var;
}