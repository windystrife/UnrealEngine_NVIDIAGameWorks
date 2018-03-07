// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "NiagaraCommon.h"
#include "NiagaraShared.h"
#include "NiagaraShader.h"
#include "NiagaraParameters.h"
#include "NiagaraDataSet.h"

#include "NiagaraScript.generated.h"

class UNiagaraDataInterface;

void SerializeNiagaraShaderMaps(const TMap<const ITargetPlatform*, FNiagaraScript*>* PlatformScriptResourcesToSavePtr, FArchive& Ar, FNiagaraScript& OutLoadedResources);
void ProcessSerializedShaderMaps(UNiagaraScript* Owner, FNiagaraScript LoadedResource, FNiagaraScript* (&OutScriptResourcesLoaded)[ERHIFeatureLevel::Num]);

#define NIAGARA_INVALID_MEMORY (0xBA)

//#define NIAGARA_SCRIPT_COMPILE_LOGGING_MEDIUM

/** Defines what will happen to unused attributes when a script is run. */
UENUM()
enum class EUnusedAttributeBehaviour : uint8
{
	/** The previous value of the attribute is copied across. */
	Copy,
	/** The attribute is set to zero. */
	Zero,
	/** The attribute is untouched. */
	None,
	/** The memory for the attribute is set to NIAGARA_INVALID_MEMORY. */
	MarkInvalid, 
	/** The attribute is passed through without double buffering */
	PassThrough,
};

struct FNiagaraScriptDebuggerInfo
{
	FNiagaraScriptDebuggerInfo();

	bool bRequestDebugFrame;
	
	int32 DebugFrameLastWriteId;

	FNiagaraDataSet DebugFrame;
};


/** Runtime script for a Niagara system */
UCLASS(MinimalAPI)
class UNiagaraScript : public UObject
{
	GENERATED_UCLASS_BODY()

	// how this script is to be used. cannot be private due to use of GET_MEMBER_NAME_CHECKED
	UPROPERTY(AssetRegistrySearchable, EditAnywhere, Category = Script)
	ENiagaraScriptUsage Usage;

	/** Which instance of the usage in the graph to use.*/
	UPROPERTY()
	int32 UsageIndex;

	/** When used as a module, what are the appropriate script types for referencing this module?*/
	UPROPERTY(AssetRegistrySearchable, EditAnywhere, Category = Script, meta = (Bitmask, BitmaskEnum = ENiagaraScriptUsage))
	int32 ModuleUsageBitmask;

	/** Used to break up scripts of the same Usage type in UI display.*/
	UPROPERTY(AssetRegistrySearchable, EditAnywhere, Category = Script)
	FText Category;

	/** Number of user pointers we must pass to the VM. */
	UPROPERTY()
	int32 NumUserPtrs;

	/** Byte code to execute for this system */
	UPROPERTY()
	TArray<uint8> ByteCode;

	/** All the data for using constants in the script. */
	UPROPERTY()
	FNiagaraParameters Parameters;

	UPROPERTY()
	FNiagaraParameters InternalParameters;

	UPROPERTY()
	TMap<FName, FNiagaraParameters> DataSetToParameters;

	/** Attributes used by this script. */
	UPROPERTY()
 	TArray<FNiagaraVariable> Attributes;

	/** Contains various usage information for this script. */
	UPROPERTY()
	FNiagaraScriptDataUsageInfo DataUsage;

	/** Information about all data interfaces used by this script. */
	UPROPERTY()
	TArray<FNiagaraScriptDataInterfaceInfo> DataInterfaceInfo;

	/** Array of ordered vm external functions to place in the function table. */
	UPROPERTY()
	TArray<FVMExternalFunctionBindingInfo> CalledVMExternalFunctions;

	/** The mode to use when deducing the type of numeric output pins from the types of the input pins. */
	UPROPERTY(EditAnywhere, Category=Script)
	ENiagaraNumericOutputTypeSelectionMode NumericOutputTypeSelectionMode;

	TArray<FNiagaraDataSetID> ReadDataSets;
	TArray<FNiagaraDataSetProperties> WriteDataSets;

	/** Scopes we'll track with stats.*/
	UPROPERTY()
	TArray<FNiagaraStatScope> StatScopes;
	
	/** The parameter collections used by this script. */
	UPROPERTY()
	TArray<class UNiagaraParameterCollection*> ParameterCollections;

	UPROPERTY(AssetRegistrySearchable, EditAnywhere, Category = Script)
	FText Description;

	UPROPERTY()
	FString LastHlslTranslation;
	
	/** Returns true if this script is valid and can be executed. */
	bool IsValid()const { return ByteCode.Num() > 0; }//More? Differentiate by CPU/GPU?

	void SetUsage(ENiagaraScriptUsage InUsage) { Usage = InUsage; }
	ENiagaraScriptUsage GetUsage() const { return Usage; }
	void SetUsageIndex(int32 InUsageIndex) { UsageIndex = InUsageIndex; }
	int32 GetUsageIndex() const { return UsageIndex; }

	bool IsEquivalentUsage(ENiagaraScriptUsage InUsage) {return (InUsage == Usage) || (Usage == ENiagaraScriptUsage::ParticleSpawnScript && InUsage == ENiagaraScriptUsage::ParticleSpawnScriptInterpolated) || (Usage == ENiagaraScriptUsage::ParticleSpawnScriptInterpolated && InUsage == ENiagaraScriptUsage::ParticleSpawnScript);}
	static bool IsEquivalentUsage(ENiagaraScriptUsage InUsageA, ENiagaraScriptUsage InUsageB) { return (InUsageA == InUsageB) || (InUsageB == ENiagaraScriptUsage::ParticleSpawnScript && InUsageA == ENiagaraScriptUsage::ParticleSpawnScriptInterpolated) || (InUsageB == ENiagaraScriptUsage::ParticleSpawnScriptInterpolated && InUsageA == ENiagaraScriptUsage::ParticleSpawnScript); }

	bool IsParticleSpawnScript() const { return Usage == ENiagaraScriptUsage::ParticleSpawnScript || Usage == ENiagaraScriptUsage::ParticleSpawnScriptInterpolated; }
	bool IsInterpolatedParticleSpawnScript()const { return Usage == ENiagaraScriptUsage::ParticleSpawnScriptInterpolated; }
	bool IsParticleUpdateScript()const { return Usage == ENiagaraScriptUsage::ParticleUpdateScript; }
	bool IsModuleScript()const { return Usage == ENiagaraScriptUsage::Module; }
	bool IsFunctionScript()	const { return Usage == ENiagaraScriptUsage::Function; }
	bool IsDynamicInputScript()	const { return Usage == ENiagaraScriptUsage::DynamicInput; }
	bool IsParticleEventScript()const { return Usage == ENiagaraScriptUsage::ParticleEventScript; }

	bool IsNonParticleScript()const { return Usage >= ENiagaraScriptUsage::EmitterSpawnScript; }
	
	bool IsSystemSpawnScript()const { return Usage == ENiagaraScriptUsage::SystemSpawnScript; }
	bool IsSystemUpdateScript()const { return Usage == ENiagaraScriptUsage::SystemUpdateScript; }
	bool IsEmitterSpawnScript()const { return Usage == ENiagaraScriptUsage::EmitterSpawnScript; }
	bool IsEmitterUpdateScript()const { return Usage == ENiagaraScriptUsage::EmitterUpdateScript; }
	bool IsStandaloneScript() const { return IsDynamicInputScript() || IsFunctionScript() || IsModuleScript(); }

	bool IsSpawnScript()const { return IsParticleSpawnScript() || IsEmitterSpawnScript() || IsSystemSpawnScript(); }

	NIAGARA_API TArray<ENiagaraScriptUsage> GetSupportedUsageContexts() const;

	bool CanBeRunOnGpu() const { return IsParticleSpawnScript() || IsParticleUpdateScript() || IsParticleEventScript(); }

#if WITH_EDITORONLY_DATA
	class UNiagaraScriptSourceBase *GetSource() { return Source; }
	void SetSource(class UNiagaraScriptSourceBase *InSource) { Source = InSource; }
#endif

	//~ Begin UObject interface
	void Serialize(FArchive& Ar)override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject interface

	// Infrastructure for GPU compute Shaders
	NIAGARA_API void CacheResourceShadersForCooking(EShaderPlatform ShaderPlatform, FNiagaraScript* &OutCachedResource);
	NIAGARA_API void CacheResourceShadersForRendering(bool bRegenerateId, bool bForceRecompile=false);
	void BeginCacheForCookedPlatformData(const ITargetPlatform *TargetPlatform);
	void CacheShadersForResources(EShaderPlatform ShaderPlatform, FNiagaraScript *ResourceToCache, bool bApplyCompletedShaderMapForRendering, bool bForceRecompile = false, bool bCooking=false);
	FNiagaraScript* AllocateResource();
	FNiagaraScript *GetRenderThreadScript()
	{
		return &ScriptResource;
	}

	FComputeShaderRHIRef GetScriptShader() 
	{
		if (!ScriptShader)
		{
			ScriptShader = ScriptResource.GetShader()->GetComputeShader();	// NIAGARATODO: need to put this caching somewhere else, as it wont' know when we update the resource
		}
		return ScriptShader; 
	}

	FComputeShaderRHIRef GetScriptShaderGameThread()
	{
		if (!ScriptShader)
		{
			ScriptShader = ScriptResource.GetShaderGameThread()->GetComputeShader();	// NIAGARATODO: need to put this caching somewhere else, as it wont' know when we update the resource
		}
		return ScriptShader;
	}

	void SetFeatureLevel(ERHIFeatureLevel::Type InFeatureLevel)		{ FeatureLevel = InFeatureLevel; }


	// SourceChangeID is updated with the source ID everytime the source graph changes
	// UniqueID is regenerated in that case, so we have separate Ids for scripts coming from the 
	// same source (e.g. spawn and update of the same emitter)
	void InvalidateChangeID()	{ ChangeId.Invalidate(); }

	const FGuid &GetChangeID()	{ return ChangeId; }

	void SetChangeID(FGuid InGuid)
	{
		ChangeId = InGuid;
		
		// whenever source changes, regenerate so we know to recompile; need this because we may have 
		//	multiple scripts with the same source, for example spawn and update scripts from one graph
		UniqueID = FGuid::NewGuid();
	}

	NIAGARA_API void GenerateStatScopeIDs();

#if WITH_EDITORONLY_DATA
	FText GetDescription() { return Description.IsEmpty() ? FText::FromString(GetName()) : Description; }
	ENiagaraScriptCompileStatus GetLastCompileStatus() { return LastCompileStatus; }
	void SetLastCompileStatus(ENiagaraScriptCompileStatus InStatus) { LastCompileStatus = InStatus; }

	/** Makes a deep copy of any script dependencies, including itself.*/
	NIAGARA_API virtual UNiagaraScript* MakeRecursiveDeepCopy(UObject* DestOuter, TMap<const UObject*, UObject*>& ExistingConversions) const;

	/** Determine if there are any external dependencies with respect to scripts and ensure that those dependencies are sucked into the existing package.*/
	NIAGARA_API virtual void SubsumeExternalDependencies(TMap<const UObject*, UObject*>& ExistingConversions);

	/** Determine if the Script and its source graph are in sync.*/
	NIAGARA_API bool AreScriptAndSourceSynchronized() const;

	/** Ensure that the Script and its source graph are marked out of sync.*/
	NIAGARA_API void MarkScriptAndSourceDesynchronized();

	NIAGARA_API ENiagaraScriptCompileStatus Compile(FString& OutGraphLevelErrorMessages);

	NIAGARA_API FNiagaraScriptDebuggerInfo& GetDebuggerInfo() { return DebuggerInfo; }

	const TArray<TStatId>& GetStatScopeIDs()const { return StatScopesIDs; }
#endif

	bool UsesCollection(const class UNiagaraParameterCollection* Collection)const;
	
	virtual ~UNiagaraScript();

	virtual void PostInitProperties() override;
private:
#if WITH_EDITORONLY_DATA
	/** 'Source' data/graphs for this script */
	UPROPERTY()
	class UNiagaraScriptSourceBase*	Source;
	
	/** Last known compile status. Lets us determine the latest state of the script byte buffer.*/
	UPROPERTY()
	ENiagaraScriptCompileStatus LastCompileStatus;

	FNiagaraScriptDebuggerInfo DebuggerInfo;
#endif

	/** Adjusted every time that we compile this script. Lets us know that we might differ from any cached versions.*/
	UPROPERTY()
	FGuid ChangeId;

	/** unique ID for this script */
	UPROPERTY()
	FGuid UniqueID;

	FNiagaraScript ScriptResource;
	FNiagaraScript* ScriptResourcesByFeatureLevel[ERHIFeatureLevel::Num];

	/** Feature level that the shader map is going to be compiled for. */
	ERHIFeatureLevel::Type FeatureLevel;

	/** Compute shader compiled for this script */
	FComputeShaderRHIRef ScriptShader;

	/** Runtime stat IDs generated from StatScopes. */
#if STATS
	TArray<TStatId> StatScopesIDs;
#endif

#if WITH_EDITOR
	/* script resources being cached for cooking. */
	TMap<const class ITargetPlatform*, FNiagaraScript*> CachedScriptResourcesForCooking;
#endif

};
