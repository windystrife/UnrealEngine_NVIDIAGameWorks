// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "NiagaraScript.h"
#include "Modules/ModuleManager.h"
#include "NiagaraScriptSourceBase.h"
#include "NiagaraComponent.h"
#include "NiagaraEmitter.h"
#include "Package.h"
#include "Linker.h"
#include "NiagaraModule.h"
#include "NiagaraCustomVersion.h"
#include "NiagaraShaderCompilationManager.h"


#include "Stats/Stats.h"
#include "Linker.h"

#if WITH_EDITOR
	#include "Interfaces/ITargetPlatform.h"
#endif

DECLARE_STATS_GROUP(TEXT("Niagara Detailed"), STATGROUP_NiagaraDetailed, STATCAT_Advanced);

FNiagaraScriptDebuggerInfo::FNiagaraScriptDebuggerInfo() : bRequestDebugFrame(false), DebugFrameLastWriteId(-1)
{
}

UNiagaraScript::UNiagaraScript(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Usage(ENiagaraScriptUsage::Function)
	, ModuleUsageBitmask( (1 << (int32)ENiagaraScriptUsage::ParticleSpawnScript) | (1 << (int32)ENiagaraScriptUsage::ParticleSpawnScriptInterpolated) | (1 << (int32)ENiagaraScriptUsage::ParticleUpdateScript) | (1 << (int32)ENiagaraScriptUsage::ParticleEventScript) )
	, NumUserPtrs(0)
	, NumericOutputTypeSelectionMode(ENiagaraNumericOutputTypeSelectionMode::Largest)
#if WITH_EDITORONLY_DATA
	, LastCompileStatus(ENiagaraScriptCompileStatus::NCS_Unknown)
	, UniqueID(FGuid::NewGuid())
#endif
{
	
}

UNiagaraScript::~UNiagaraScript()
{
}

void UNiagaraScript::PostInitProperties()
{
	Super::PostInitProperties();
	UsageIndex = 0;
}

UNiagaraScriptSourceBase::UNiagaraScriptSourceBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}


void UNiagaraScript::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FNiagaraCustomVersion::GUID);		// only changes version if not loading
	const int32 NiagaraVer = Ar.CustomVer(FNiagaraCustomVersion::GUID);

	bool IsValidShaderScript = Usage != ENiagaraScriptUsage::Module && Usage != ENiagaraScriptUsage::Function && Usage != ENiagaraScriptUsage::DynamicInput 
		&& (NiagaraVer<FNiagaraCustomVersion::NiagaraShaderMapCooking2 || (Usage != ENiagaraScriptUsage::SystemSpawnScript && Usage != ENiagaraScriptUsage::SystemUpdateScript));
	if ( (!Ar.IsLoading() && IsValidShaderScript)		// saving shader maps only for particle sim and spawn scripts
		|| (Ar.IsLoading() && NiagaraVer >= FNiagaraCustomVersion::NiagaraShaderMaps && (NiagaraVer < FNiagaraCustomVersion::NiagaraShaderMapCooking || IsValidShaderScript))  // load only if we know shader map is presen
		)
	{
#if WITH_EDITOR
		SerializeNiagaraShaderMaps(&CachedScriptResourcesForCooking, Ar, ScriptResource);
#else
		SerializeNiagaraShaderMaps(NULL, Ar, ScriptResource);
#endif
	}
}

void UNiagaraScript::PostLoad()
{
	Super::PostLoad();
	
	bool bNeedsRecompile = false;
	const int32 NiagaraVer = GetLinkerCustomVersion(FNiagaraCustomVersion::GUID);
	
	//Recompile is before the latest version that requires a script recompile. Some may not.
	if (NiagaraVer < FNiagaraCustomVersion::RemovalOfNiagaraVariableIDs)
	{
		if (Usage != ENiagaraScriptUsage::Module && Usage != ENiagaraScriptUsage::Function && Usage != ENiagaraScriptUsage::DynamicInput)
		{
			bNeedsRecompile = true;
			ByteCode.Empty();
#if WITH_EDITORONLY_DATA
			LastCompileStatus = ENiagaraScriptCompileStatus::NCS_Unknown;
#endif
			UE_LOG(LogNiagara, Warning, TEXT("Niagara script is out of date and requires recompile to be used! %s"), *GetFullName());
		}
	}


	// Resources can be processed / registered now that we're back on the main thread
	ProcessSerializedShaderMaps(this, ScriptResource, ScriptResourcesByFeatureLevel);


	if (GIsEditor)
	{
		
#if WITH_EDITORONLY_DATA
		// Since we're about to check the synchronized state, we need to make sure that it has been post-loaded (which 
		// can affect the results of that call).
		if (Source != nullptr)
		{	
			Source->ConditionalPostLoad();
		}

		// If we've never computed a change id or the source graph is different than the compiled script change id,
		// we're out of sync and should recompile.
		if (Usage != ENiagaraScriptUsage::Module && Usage != ENiagaraScriptUsage::Function && Usage != ENiagaraScriptUsage::DynamicInput)
		{
			if (ChangeId.IsValid() == false || (Source != nullptr && !Source->IsSynchronized(ChangeId)))
			{
				bNeedsRecompile = true;
				ByteCode.Empty();
				LastCompileStatus = ENiagaraScriptCompileStatus::NCS_Unknown;
				UE_LOG(LogNiagara, Warning, TEXT("Niagara script is out of date with source graph requires recompile to be used! %s"), *GetFullName());
			}
		}
#endif
	}

	// for now, force recompile until we can be sure everything is working
//	bNeedsRecompile = true;
#if WITH_EDITORONLY_DATA
	CacheResourceShadersForRendering(false, bNeedsRecompile);
	GenerateStatScopeIDs();
#endif

}


#if WITH_EDITOR

void UNiagaraScript::GenerateStatScopeIDs()
{
	StatScopesIDs.Empty();
#if STATS
	for (FNiagaraStatScope& StatScope : StatScopes)
	{
		StatScopesIDs.Add(FDynamicStats::CreateStatId<FStatGroup_STATGROUP_NiagaraDetailed>(StatScope.FriendlyName.ToString()));
	}
#endif
}

void UNiagaraScript::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	CacheResourceShadersForRendering(true);
}

#endif

#if WITH_EDITORONLY_DATA
bool UNiagaraScript::AreScriptAndSourceSynchronized() const
{
	if (Source)
	{
		return Source->IsSynchronized(ChangeId);
	}
	else
	{
		return false;
	}
}

void UNiagaraScript::MarkScriptAndSourceDesynchronized()
{
	if (Source)
	{
		Source->MarkNotSynchronized();
	}
}


UNiagaraScript* UNiagaraScript::MakeRecursiveDeepCopy(UObject* DestOuter, TMap<const UObject*, UObject*>& ExistingConversions) const
{
	check(GetOuter() != DestOuter);

	bool bSourceConvertedAlready = ExistingConversions.Contains(Source);

	ResetLoaders(GetTransientPackage()); // Make sure that we're not going to get invalid version number linkers into the transient package. 
	GetTransientPackage()->LinkerCustomVersion.Empty();

	// For some reason, the default parameters of FObjectDuplicationParameters aren't the same as
	// StaticDuplicateObject uses internally. These are copied from Static Duplicate Object...
	EObjectFlags FlagMask = RF_AllFlags & ~RF_Standalone & ~RF_Public; // Remove Standalone and Public flags.
	EDuplicateMode::Type DuplicateMode = EDuplicateMode::Normal;
	EInternalObjectFlags InternalFlagsMask = EInternalObjectFlags::AllFlags;

	FObjectDuplicationParameters ObjParameters((UObject*)this, GetTransientPackage());
	ObjParameters.DestName = NAME_None;
	if (this->GetOuter() != DestOuter)
	{
		// try to keep the object name consistent if possible
		if (FindObjectFast<UObject>(DestOuter, GetFName()) == nullptr)
		{
			ObjParameters.DestName = GetFName();
		}
	}

	ObjParameters.DestClass = GetClass();
	ObjParameters.FlagMask = FlagMask;
	ObjParameters.InternalFlagMask = InternalFlagsMask;
	ObjParameters.DuplicateMode = DuplicateMode;
	
	// Make sure that we don't duplicate objects that we've alredy converted...
	TMap<const UObject*, UObject*>::TConstIterator It = ExistingConversions.CreateConstIterator();
	while (It)
	{
		ObjParameters.DuplicationSeed.Add(const_cast<UObject*>(It.Key()), It.Value());
		++It;
	}

	UNiagaraScript*	Script = CastChecked<UNiagaraScript>(StaticDuplicateObjectEx(ObjParameters));

	check(Script->HasAnyFlags(RF_Standalone) == false);
	check(Script->HasAnyFlags(RF_Public) == false);

	if (bSourceConvertedAlready)
	{
		// Confirm that we've converted these properly..
		check(Script->Source == ExistingConversions[Source]);
	}

	if (DestOuter != nullptr)
	{
		Script->Rename(nullptr, DestOuter, REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
	}
	UE_LOG(LogNiagara, Warning, TEXT("MakeRecursiveDeepCopy %s"), *Script->GetFullName());
	ExistingConversions.Add(const_cast<UNiagaraScript*>(this), Script);

	// Since the Source is the only thing we subsume from UNiagaraScripts, only do the subsume if 
	// we haven't already converted it.
	if (bSourceConvertedAlready == false)
	{
		Script->SubsumeExternalDependencies(ExistingConversions);
	}
	return Script;
}

void UNiagaraScript::SubsumeExternalDependencies(TMap<const UObject*, UObject*>& ExistingConversions)
{
	Source->SubsumeExternalDependencies(ExistingConversions);
}

ENiagaraScriptCompileStatus UNiagaraScript::Compile(FString& OutGraphLevelErrorMessages)
{
	if (Source == nullptr)
		return ENiagaraScriptCompileStatus::NCS_Error;

	ENiagaraScriptCompileStatus CPUCompileStatus = Source->Compile(this, OutGraphLevelErrorMessages);
	CacheResourceShadersForRendering(false, true);

	return CPUCompileStatus;
}
#endif



#if WITH_EDITOR
void UNiagaraScript::BeginCacheForCookedPlatformData(const ITargetPlatform *TargetPlatform)
{
	TArray<FName> DesiredShaderFormats;
	TargetPlatform->GetAllTargetedShaderFormats(DesiredShaderFormats);

	FNiagaraScript** CachedScriptResourceForPlatformPtr = CachedScriptResourcesForCooking.Find(TargetPlatform);

	if (CachedScriptResourceForPlatformPtr == nullptr)
	{
		CachedScriptResourcesForCooking.Add(TargetPlatform);
		FNiagaraScript* &CachedScriptResourceForPlatform = *CachedScriptResourcesForCooking.Find(TargetPlatform);

		if (DesiredShaderFormats.Num())
		{
			// Cache for all the shader formats that the cooking target requires
			for (int32 FormatIndex = 0; FormatIndex < DesiredShaderFormats.Num(); FormatIndex++)
			{
				if (DesiredShaderFormats[FormatIndex] != TEXT("PCD3D_SM4"))	// TODO: remove once we get rid of SM4 globally
				{
					const EShaderPlatform LegacyShaderPlatform = ShaderFormatToLegacyShaderPlatform(DesiredShaderFormats[FormatIndex]);

					// Begin caching shaders for the target platform and store the FNiagaraScript being compiled into CachedScriptResourcesForCooking
					CacheResourceShadersForCooking(LegacyShaderPlatform, CachedScriptResourceForPlatform);
				}
			}
		}
	}
}


void UNiagaraScript::CacheResourceShadersForCooking(EShaderPlatform ShaderPlatform, FNiagaraScript* &OutCachedResource)
{
	if (Usage != ENiagaraScriptUsage::Function && Usage != ENiagaraScriptUsage::Module && Usage != ENiagaraScriptUsage::DynamicInput)
	{
		FNiagaraScript *ResourceToCache = nullptr;
		ERHIFeatureLevel::Type TargetFeatureLevel = GetMaxSupportedFeatureLevel(ShaderPlatform);

		FNiagaraScript* NewResource = AllocateResource();
		NewResource->SetScript(this, (ERHIFeatureLevel::Type)TargetFeatureLevel, UniqueID, GetName());
		ResourceToCache = NewResource;

		check(ResourceToCache);

		CacheShadersForResources(ShaderPlatform, ResourceToCache, false, false, true);

		INiagaraModule NiagaraModule = FModuleManager::GetModuleChecked<INiagaraModule>(TEXT("Niagara"));
		NiagaraModule.ProcessShaderCompilationQueue();

		OutCachedResource = ResourceToCache;
	}
}



void UNiagaraScript::CacheShadersForResources(EShaderPlatform ShaderPlatform, FNiagaraScript *ResourceToCache, bool bApplyCompletedShaderMapForRendering, bool bForceRecompile, bool bCooking)
{
	const bool bSuccess = ResourceToCache->CacheShaders(ShaderPlatform, bApplyCompletedShaderMapForRendering, bForceRecompile, bCooking);

#if defined(NIAGARA_SCRIPT_COMPILE_LOGGING_MEDIUM)
	if (!bSuccess)
	{
		UE_LOG(LogNiagara, Warning, TEXT("Failed to compile Niagara shader %s for platform %s."),
			*GetPathName(),
			*LegacyShaderPlatformToShaderFormat(ShaderPlatform).ToString());

		const TArray<FString>& CompileErrors = ResourceToCache->GetCompileErrors();
		for (int32 ErrorIndex = 0; ErrorIndex < CompileErrors.Num(); ErrorIndex++)
		{
			UE_LOG(LogNiagara, Warning, TEXT("	%s"), *CompileErrors[ErrorIndex]);
		}
	}
#endif
}



void UNiagaraScript::CacheResourceShadersForRendering(bool bRegenerateId, bool bForceRecompile)
{
	if (bRegenerateId)
	{
		// Regenerate this script's Id if requested
		for (int32 Idx = 0; Idx < ERHIFeatureLevel::Num; Idx++)
		{
			if (ScriptResourcesByFeatureLevel[Idx])
			{
				ScriptResourcesByFeatureLevel[Idx]->ReleaseShaderMap();
				ScriptResourcesByFeatureLevel[Idx] = nullptr;
			}
		}
	}

	//UpdateResourceAllocations();

	if (FApp::CanEverRender() && CanBeRunOnGpu())
	{
		if (Source)
		{
			FNiagaraScript* ResourceToCache;
			ERHIFeatureLevel::Type CacheFeatureLevel = ERHIFeatureLevel::SM5;
			ScriptResource.SetScript(this, FeatureLevel, UniqueID, GetName());

			//if (ScriptResourcesByFeatureLevel[FeatureLevel])
			{
				EShaderPlatform ShaderPlatform = GShaderPlatformForFeatureLevel[CacheFeatureLevel];
				ResourceToCache = ScriptResourcesByFeatureLevel[CacheFeatureLevel];
				CacheShadersForResources(ShaderPlatform, &ScriptResource, true, bForceRecompile);
				ScriptResourcesByFeatureLevel[CacheFeatureLevel] = &ScriptResource;
			}
		}
	}
}

#endif

void SerializeNiagaraShaderMaps(const TMap<const ITargetPlatform*, FNiagaraScript*>* PlatformScriptResourcesToSavePtr, FArchive& Ar, FNiagaraScript &OutLoadedResource)
{
//	SCOPED_LOADTIMER(SerializeInlineShaderMaps);
	if (Ar.IsSaving())
	{
		int32 NumResourcesToSave = 0;
		FNiagaraScript *ScriptResourceToSave = nullptr;
		FNiagaraScript* const * ScriptResourceToSavePtr = nullptr;
		if (Ar.IsCooking())
		{
			check(PlatformScriptResourcesToSavePtr);
			auto& PlatformScriptResourcesToSave = *PlatformScriptResourcesToSavePtr;

			ScriptResourceToSavePtr = PlatformScriptResourcesToSave.Find(Ar.CookingTarget());
			check(ScriptResourceToSavePtr != NULL || (Ar.GetLinker() == NULL));
			if (ScriptResourceToSavePtr != NULL)
			{
				NumResourcesToSave++;
			}
		}

		Ar << NumResourcesToSave;

		if (ScriptResourceToSavePtr)
		{
			ScriptResourceToSave = *ScriptResourceToSavePtr;
			ScriptResourceToSave->SerializeShaderMap(Ar);
		}

	}
	else if (Ar.IsLoading())
	{
		int32 NumLoadedResources = 0;
		Ar << NumLoadedResources;
		if (NumLoadedResources)
		{
			OutLoadedResource.SerializeShaderMap(Ar);
		}
	}
}



void ProcessSerializedShaderMaps(UNiagaraScript* Owner, FNiagaraScript LoadedResource, FNiagaraScript* (&OutScriptResourcesLoaded)[ERHIFeatureLevel::Num])
{
	check(IsInGameThread());

	LoadedResource.RegisterShaderMap();

	FNiagaraShaderMap* LoadedShaderMap = LoadedResource.GetGameThreadShaderMap();
	if (LoadedShaderMap && LoadedShaderMap->GetShaderPlatform() == GMaxRHIShaderPlatform)
	{
		ERHIFeatureLevel::Type LoadedFeatureLevel = LoadedShaderMap->GetShaderMapId().FeatureLevel;
		if (!OutScriptResourcesLoaded[LoadedFeatureLevel])
		{
			OutScriptResourcesLoaded[LoadedFeatureLevel] = Owner->AllocateResource();
		}

		OutScriptResourcesLoaded[LoadedFeatureLevel]->SetShaderMap(LoadedShaderMap);
	}
}



FNiagaraScript* UNiagaraScript::AllocateResource()
{
	return new FNiagaraScript();
}

TArray<ENiagaraScriptUsage> UNiagaraScript::GetSupportedUsageContexts() const
{
	TArray<ENiagaraScriptUsage> Supported;
	for (int32 i = 0; i <= (int32)ENiagaraScriptUsage::SystemUpdateScript; i++)
	{
		int32 TargetBit = (ModuleUsageBitmask >> (int32)i) & 1;
		if (TargetBit == 1)
		{
			Supported.Add((ENiagaraScriptUsage)i);
		}
	}
	return Supported;
}

bool UNiagaraScript::UsesCollection(const UNiagaraParameterCollection* Collection)const
{
	return ParameterCollections.FindByPredicate([&](const UNiagaraParameterCollection* CheckCollection)
	{
		return CheckCollection == Collection;
	}) != NULL;
}