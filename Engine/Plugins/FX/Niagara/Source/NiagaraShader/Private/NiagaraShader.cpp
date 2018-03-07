// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "NiagaraShader.h"
#include "NiagaraShared.h"
#include "NiagaraShaderMap.h"
#include "Stats/StatsMisc.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "ProfilingDebugging/DiagnosticTable.h"
#include "ShaderCompiler.h"
#include "NiagaraShaderDerivedDataVersion.h"
#include "NiagaraShaderCompilationManager.h"
#if WITH_EDITOR
	#include "Interfaces/ITargetPlatformManagerModule.h"
	#include "TickableEditorObject.h"
	#include "DerivedDataCacheInterface.h"
#endif
#include "ProfilingDebugging/CookStats.h"

IMPLEMENT_SHADER_TYPE(,FNiagaraShader, TEXT("/Engine/Private/NiagaraEmitterInstanceShader.usf"),TEXT("SimulateMain"), SF_Compute)


int32 GCreateNiagaraShadersOnLoad = 0;
static FAutoConsoleVariableRef CVarCreateNiagaraShadersOnLoad(
	TEXT("niagara.CreateShadersOnLoad"),
	GCreateNiagaraShadersOnLoad,
	TEXT("Whether to create Niagara's simulation shaders on load, which can reduce hitching, but use more memory.  Otherwise they will be created as needed.")
	);


#if ENABLE_COOK_STATS
namespace NiagaraShaderCookStats
{
	static FCookStats::FDDCResourceUsageStats UsageStats;
	static int32 ShadersCompiled = 0;
	static FCookStatsManager::FAutoRegisterCallback RegisterCookStats([](FCookStatsManager::AddStatFuncRef AddStat)
	{
		UsageStats.LogStats(AddStat, TEXT("NiagaraShader.Usage"), TEXT(""));
		AddStat(TEXT("NiagaraShader.Misc"), FCookStatsManager::CreateKeyValueArray(
			TEXT("ShadersCompiled"), ShadersCompiled
			));
	});
}
#endif


//
// Globals
//
TMap<FNiagaraShaderMapId, FNiagaraShaderMap*> FNiagaraShaderMap::GIdToNiagaraShaderMap[SP_NumPlatforms];
TArray<FNiagaraShaderMap*> FNiagaraShaderMap::AllNiagaraShaderMaps;

// The Id of 0 is reserved for global shaders
uint32 FNiagaraShaderMap::NextCompilingId = 2;


/** 
 * Tracks FNiagaraScripts and their shader maps that are being compiled.
 * Uses a TRefCountPtr as this will be the only reference to a shader map while it is being compiled.
 */
TMap<TRefCountPtr<FNiagaraShaderMap>, TArray<FNiagaraScript*> > FNiagaraShaderMap::NiagaraShaderMapsBeingCompiled;



static inline bool ShouldCacheNiagaraShader(const FNiagaraShaderType* ShaderType, EShaderPlatform Platform, const FNiagaraScript* Script)
{
	return ShaderType->ShouldCache(Platform, Script) && Script->ShouldCache(Platform, ShaderType);
}



/** Called for every script shader to update the appropriate stats. */
void UpdateNiagaraShaderCompilingStats(const FNiagaraScript* Script)
{
	INC_DWORD_STAT_BY(STAT_ShaderCompiling_NumTotalNiagaraShaders,1);
}


void FNiagaraShaderMapId::Serialize(FArchive& Ar)
{
	// You must bump NIAGARASHADERMAP_DERIVEDDATA_VER if changing the serialization of FNiagaraShaderMapId.

	Ar << BaseScriptID;

	Ar << (int32&)FeatureLevel;

	//ParameterSet.Serialize(Ar);		// NIAGARATODO: at some point we'll need stuff for static switches here
	//Ar << ReferencedFunctions;		// may want this too, although we've got other mechanisms to determine need to recompile, unsure whether the shader map needs to know
}

/** Hashes the script-specific part of this shader map Id. */
void FNiagaraShaderMapId::GetScriptHash(FSHAHash& OutHash) const
{
	FSHA1 HashState;

	HashState.Update((const uint8*)&BaseScriptID, sizeof(BaseScriptID));
	HashState.Update((const uint8*)&FeatureLevel, sizeof(FeatureLevel));

	//ParameterSet.UpdateHash(HashState);		// will need for static switches

	/*
	for (int32 FunctionIndex = 0; FunctionIndex < ReferencedFunctions.Num(); FunctionIndex++)
	{
		HashState.Update((const uint8*)&ReferencedFunctions[FunctionIndex], sizeof(ReferencedFunctions[FunctionIndex]));
	}
	*/
	HashState.Final();
	HashState.GetHash(&OutHash.Hash[0]);
}

/** 
* Tests this set against another for equality, disregarding override settings.
* 
* @param ReferenceSet	The set to compare against
* @return				true if the sets are equal
*/
bool FNiagaraShaderMapId::operator==(const FNiagaraShaderMapId& ReferenceSet) const
{
	if (BaseScriptID != ReferenceSet.BaseScriptID 
		//|| QualityLevel != ReferenceSet.QualityLevel
		|| FeatureLevel != ReferenceSet.FeatureLevel)
	{
		return false;
	}
	/*
	if (ParameterSet != ReferenceSet.ParameterSet
		|| ReferencedFunctions.Num() != ReferenceSet.ReferencedFunctions.Num()
	{
		return false;
	}

	for (int32 RefFunctionIndex = 0; RefFunctionIndex < ReferenceSet.ReferencedFunctions.Num(); RefFunctionIndex++)
	{
		const FGuid& ReferenceGuid = ReferenceSet.ReferencedFunctions[RefFunctionIndex];

		if (ReferencedFunctions[RefFunctionIndex] != ReferenceGuid)
		{
			return false;
		}
	}
	*/
	return true;
}

void FNiagaraShaderMapId::AppendKeyString(FString& KeyString) const
{
	KeyString += BaseScriptID.ToString();
	KeyString += TEXT("_");
	FString FeatureLevelString;
	GetFeatureLevelName(FeatureLevel, FeatureLevelString);
	KeyString += FeatureLevelString + TEXT("_");
	/*
	ParameterSet.AppendKeyString(KeyString);

	KeyString += TEXT("_");
	KeyString += FString::FromInt(Usage);
	KeyString += TEXT("_");

	// Add any referenced functions to the key so that we will recompile when they are changed
	for (int32 FunctionIndex = 0; FunctionIndex < ReferencedFunctions.Num(); FunctionIndex++)
	{
		KeyString += ReferencedFunctions[FunctionIndex].ToString();
	}
	*/
}


/**
 * Enqueues a compilation for a new shader of this type.
 * @param script - The script to link the shader with.
 */
FNiagaraShaderCompileJob* FNiagaraShaderType::BeginCompileShader(
	uint32 ShaderMapId,
	const FNiagaraScript* Script,
	FShaderCompilerEnvironment* CompilationEnvironment,
	EShaderPlatform Platform,
	TArray<FNiagaraShaderCompileJob*>& NewJobs,
	FShaderTarget Target,
	TArray< TArray<DIGPUBufferParamDescriptor> >&InBufferDescriptors
	)
{
	FNiagaraShaderCompileJob* NewJob = new FNiagaraShaderCompileJob(ShaderMapId, this, Script->HlslOutput);

	NewJob->DIBufferDescriptors = InBufferDescriptors;	// from hlsl translation and need to be passed to the FNiagaraShader on completion

	NewJob->Input.SharedEnvironment = CompilationEnvironment;
	NewJob->Input.Target = Target;
	NewJob->Input.ShaderFormat = LegacyShaderPlatformToShaderFormat(Platform);
	NewJob->Input.VirtualSourceFilePath = TEXT("/Engine/Private/NiagaraEmitterInstanceShader.usf");
	NewJob->Input.EntryPointName = TEXT("SimulateMainComputeCS");
	NewJob->Input.Environment.SetDefine(TEXT("GPU_SIMULATION"), 1);
	NewJob->Input.Environment.IncludeVirtualPathToContentsMap.Add(TEXT("/Engine/Generated/NiagaraEmitterInstance.usf"), StringToArray<ANSICHAR>(*Script->HlslOutput, Script->HlslOutput.Len() + 1));
	FShaderCompilerEnvironment& ShaderEnvironment = NewJob->Input.Environment;

	UE_LOG(LogShaders, Verbose, TEXT("			%s"), GetName());
	COOK_STAT(NiagaraShaderCookStats::ShadersCompiled++);

	//update script shader stats
	UpdateNiagaraShaderCompilingStats(Script);

	// Allow the shader type to modify the compile environment.
	SetupCompileEnvironment(Platform, Script, ShaderEnvironment);

	NewJobs.Add(NewJob);

	return NewJob;
}

/**
 * Either creates a new instance of this type or returns an equivalent existing shader.
 * @param CurrentJob - Compile job that was enqueued by BeginCompileShader.
 */
FShader* FNiagaraShaderType::FinishCompileShader(
	//const FUniformExpressionSet& UniformExpressionSet,
	const FSHAHash& ShaderMapHash,
	const FNiagaraShaderCompileJob& CurrentJob,
	const FString& InDebugDescription
	)
{
	check(CurrentJob.bSucceeded);

	FShaderType* SpecificType = CurrentJob.ShaderType->LimitShaderResourceToThisType() ? CurrentJob.ShaderType : NULL;

	// Reuse an existing resource with the same key or create a new one based on the compile output
	// This allows FShaders to share compiled bytecode and RHI shader references
	FShaderResource* Resource = FShaderResource::FindOrCreateShaderResource(CurrentJob.Output, SpecificType);

	// Find a shader with the same key in memory
	FShader* Shader = CurrentJob.ShaderType->FindShaderById(FShaderId(ShaderMapHash, nullptr, nullptr, CurrentJob.ShaderType, CurrentJob.Input.Target));

	// There was no shader with the same key so create a new one with the compile output, which will bind shader parameters
	if (!Shader)
	{
		Shader = (*ConstructCompiledRef)(CompiledShaderInitializerType(this, CurrentJob.Output, Resource, ShaderMapHash, InDebugDescription, CurrentJob.DIBufferDescriptors));
		CurrentJob.Output.ParameterMap.VerifyBindingsAreComplete(GetName(), CurrentJob.Output.Target, nullptr);
	}

	return Shader;
}

/**
* Finds the shader map for a script.
* @param ShaderMapId - The script id and static parameter set identifying the shader map
* @param Platform - The platform to lookup for
* @return NULL if no cached shader map was found.
*/
FNiagaraShaderMap* FNiagaraShaderMap::FindId(const FNiagaraShaderMapId& ShaderMapId, EShaderPlatform InPlatform)
{
	check(ShaderMapId.BaseScriptID != FGuid());
	return GIdToNiagaraShaderMap[InPlatform].FindRef(ShaderMapId);
}

/** Flushes the given shader types from any loaded FNiagaraShaderMap's. */
void FNiagaraShaderMap::FlushShaderTypes(TArray<FShaderType*>& ShaderTypesToFlush)
{
	for (int32 ShaderMapIndex = 0; ShaderMapIndex < AllNiagaraShaderMaps.Num(); ShaderMapIndex++)
	{
		FNiagaraShaderMap* CurrentShaderMap = AllNiagaraShaderMaps[ShaderMapIndex];

		for (int32 ShaderTypeIndex = 0; ShaderTypeIndex < ShaderTypesToFlush.Num(); ShaderTypeIndex++)
		{
			CurrentShaderMap->FlushShadersByShaderType(ShaderTypesToFlush[ShaderTypeIndex]);
		}
	}
}

void FNiagaraShaderMap::FixupShaderTypes(EShaderPlatform Platform, const TMap<FShaderType*, FString>& ShaderTypeNames)
{
	TArray<FNiagaraShaderMapId> Keys;
	FNiagaraShaderMap::GIdToNiagaraShaderMap[Platform].GenerateKeyArray(Keys);

	TArray<FNiagaraShaderMap*> Values;
	FNiagaraShaderMap::GIdToNiagaraShaderMap[Platform].GenerateValueArray(Values);

	//@todo - what about the shader maps in AllNiagaraShaderMaps that are not in GIdToNiagaraShaderMap?
	FNiagaraShaderMap::GIdToNiagaraShaderMap[Platform].Empty();

	for (int32 PairIndex = 0; PairIndex < Keys.Num(); PairIndex++)
	{
		FNiagaraShaderMapId& Key = Keys[PairIndex];
		FNiagaraShaderMap::GIdToNiagaraShaderMap[Platform].Add(Key, Values[PairIndex]);
	}
}


void NiagaraShaderMapAppendKeyString(EShaderPlatform Platform, FString& KeyString)
{
	// does nothing at the moment, but needs to append to keystring if runtime options impacting selection of sim shader permutations are added
	// for example static switches will need to go here
}

#if WITH_EDITOR

/** Creates a string key for the derived data cache given a shader map id. */
static FString GetNiagaraShaderMapKeyString(const FNiagaraShaderMapId& ShaderMapId, EShaderPlatform Platform)
{
	FName Format = LegacyShaderPlatformToShaderFormat(Platform);
	FString ShaderMapKeyString = Format.ToString() + TEXT("_") + FString(FString::FromInt(GetTargetPlatformManagerRef().ShaderFormatVersion(Format))) + TEXT("_");
	NiagaraShaderMapAppendKeyString(Platform, ShaderMapKeyString);
	ShaderMapId.AppendKeyString(ShaderMapKeyString);
	return FDerivedDataCacheInterface::BuildCacheKey(TEXT("NIAGARASM"), NIAGARASHADERMAP_DERIVEDDATA_VER, *ShaderMapKeyString);
}



void FNiagaraShaderMap::LoadFromDerivedDataCache(const FNiagaraScript* Script, const FNiagaraShaderMapId& ShaderMapId, EShaderPlatform Platform, TRefCountPtr<FNiagaraShaderMap>& InOutShaderMap)
{
	if (InOutShaderMap != NULL)
	{
		check(InOutShaderMap->Platform == Platform);
		// If the shader map was non-NULL then it was found in memory but is incomplete, attempt to load the missing entries from memory
		InOutShaderMap->LoadMissingShadersFromMemory(Script);
	}
	else
	{
		// Shader map was not found in memory, try to load it from the DDC
		STAT(double NiagaraShaderDDCTime = 0);
		{
			SCOPE_SECONDS_COUNTER(NiagaraShaderDDCTime);
			COOK_STAT(auto Timer = NiagaraShaderCookStats::UsageStats.TimeSyncWork());

			TArray<uint8> CachedData;
			const FString DataKey = GetNiagaraShaderMapKeyString(ShaderMapId, Platform);

			if (GetDerivedDataCacheRef().GetSynchronous(*DataKey, CachedData))
			{
				COOK_STAT(Timer.AddHit(CachedData.Num()));
				InOutShaderMap = new FNiagaraShaderMap();
				FMemoryReader Ar(CachedData, true);

				// Deserialize from the cached data
				InOutShaderMap->Serialize(Ar);
				InOutShaderMap->RegisterSerializedShaders();

				checkSlow(InOutShaderMap->GetShaderMapId() == ShaderMapId);

				// Register in the global map
				InOutShaderMap->Register(Platform);
			}
			else
			{
				// We should be build the data later, and we can track that the resource was built there when we push it to the DDC.
				COOK_STAT(Timer.TrackCyclesOnly());
				InOutShaderMap = nullptr;
			}
		}
		INC_FLOAT_STAT_BY(STAT_ShaderCompiling_DDCLoading,(float)NiagaraShaderDDCTime);
	}
}

void FNiagaraShaderMap::SaveToDerivedDataCache()
{
	COOK_STAT(auto Timer = NiagaraShaderCookStats::UsageStats.TimeSyncWork());
	TArray<uint8> SaveData;
	FMemoryWriter Ar(SaveData, true);
	Serialize(Ar);

	GetDerivedDataCacheRef().Put(*GetNiagaraShaderMapKeyString(ShaderMapId, Platform), SaveData);
	COOK_STAT(Timer.AddMiss(SaveData.Num()));
}

TArray<uint8>* FNiagaraShaderMap::BackupShadersToMemory()
{
	TArray<uint8>* SavedShaderData = new TArray<uint8>();
	FMemoryWriter Ar(*SavedShaderData);

	SerializeInline(Ar, true, true);
	RegisterSerializedShaders();
	Empty();

	return SavedShaderData;
}

void FNiagaraShaderMap::RestoreShadersFromMemory(const TArray<uint8>& ShaderData)
{
	FMemoryReader Ar(ShaderData);
	SerializeInline(Ar, true, true);
	RegisterSerializedShaders();
}

void FNiagaraShaderMap::SaveForRemoteRecompile(FArchive& Ar, const TMap<FString, TArray<TRefCountPtr<FNiagaraShaderMap> > >& CompiledShaderMaps, const TArray<FShaderResourceId>& ClientResourceIds)
{
	UE_LOG(LogShaders, Display, TEXT("Niagara shader map looking for unique resources, %d were on client"), ClientResourceIds.Num());

	// first, we look for the unique shader resources
	TArray<FShaderResource*> UniqueResources;
	int32 NumSkippedResources = 0;

	for (TMap<FString, TArray<TRefCountPtr<FNiagaraShaderMap> > >::TConstIterator It(CompiledShaderMaps); It; ++It)
	{
		const TArray<TRefCountPtr<FNiagaraShaderMap> >& ShaderMapArray = It.Value();

		for (int32 Index = 0; Index < ShaderMapArray.Num(); Index++)
		{
			FNiagaraShaderMap* ShaderMap = ShaderMapArray[Index];

			if (ShaderMap)
			{
				// get all shaders in the shader map
				TMap<FShaderId, FShader*> ShaderList;
				ShaderMap->GetShaderList(ShaderList);

				// get the resources from the shaders
				for (auto& KeyValue : ShaderList)
				{
					FShader* Shader = KeyValue.Value;
					FShaderResourceId ShaderId = Shader->GetResourceId();

					// skip this shader if the Id was already on the client (ie, it didn't change)
					if (ClientResourceIds.Contains(ShaderId) == false)
					{
						// lookup the resource by ID
						FShaderResource* Resource = FShaderResource::FindShaderResourceById(ShaderId);
						// add it if it's unique
						UniqueResources.AddUnique(Resource);
					}
					else
					{
						NumSkippedResources++;
					}
				}
			}
		}
	}

	UE_LOG(LogShaders, Display, TEXT("Sending %d new Niagara shader resources, skipped %d existing"), UniqueResources.Num(), NumSkippedResources);

	// now serialize them
	int32 NumUniqueResources = UniqueResources.Num();
	Ar << NumUniqueResources;

	for (int32 Index = 0; Index < NumUniqueResources; Index++)
	{
		UniqueResources[Index]->Serialize(Ar);
	}

	// now we serialize a map (for each script)
	int32 MapSize = CompiledShaderMaps.Num();
	Ar << MapSize;

	for (TMap<FString, TArray<TRefCountPtr<FNiagaraShaderMap> > >::TConstIterator It(CompiledShaderMaps); It; ++It)
	{
		const TArray<TRefCountPtr<FNiagaraShaderMap> >& ShaderMapArray = It.Value();

		FString ScriptName = It.Key();
		Ar << ScriptName;

		int32 NumShaderMaps = ShaderMapArray.Num();
		Ar << NumShaderMaps;

		for (int32 Index = 0; Index < ShaderMapArray.Num(); Index++)
		{
			FNiagaraShaderMap* ShaderMap = ShaderMapArray[Index];

			if (ShaderMap && NumUniqueResources > 0)
			{
				uint8 bIsValid = 1;
				Ar << bIsValid;
				ShaderMap->Serialize(Ar, false);
			}
			else
			{
				uint8 bIsValid = 0;
				Ar << bIsValid;
			}
		}
	}
}

void FNiagaraShaderMap::LoadForRemoteRecompile(FArchive& Ar, EShaderPlatform ShaderPlatform, const TArray<FString>& ScriptsForShaderMaps)
{
	check(false);	// TODO: implement
	/*
	int32 NumResources;
	Ar << NumResources;

	// KeepAliveReferences keeps resources alive until we are finished serializing in this function
	TArray<TRefCountPtr<FShaderResource> > KeepAliveReferences;

	// load and register the resources
	for (int32 Index = 0; Index < NumResources; Index++)
	{
		// Load the inlined shader resource
		FShaderResource* Resource = new FShaderResource();
		Resource->Serialize(Ar);

		// if this Id is already in memory, that means that this is a repeated resource and so we skip it
		if (FShaderResource::FindShaderResourceById(Resource->GetId()) != NULL)
		{
			delete Resource;
		}
		// otherwise, it's a new resource, so we register it for the maps to find below
		else
		{
			Resource->Register();

			// Keep this guy alive until we finish serializing all the FShaders in
			// The FShaders which are discarded may cause these resources to be discarded 
			KeepAliveReferences.Add( Resource );
		}
	}

	int32 MapSize;
	Ar << MapSize;

	for (int32 MaterialIndex = 0; MaterialIndex < MapSize; MaterialIndex++)
	{
		FString MaterialName;
		Ar << MaterialName;

		UMaterialInterface* MatchingMaterial = FindObjectChecked<UMaterialInterface>(NULL, *MaterialName);

		int32 NumShaderMaps = 0;
		Ar << NumShaderMaps;

		TArray<TRefCountPtr<FNiagaraShaderMap> > LoadedShaderMaps;

		for (int32 ShaderMapIndex = 0; ShaderMapIndex < NumShaderMaps; ShaderMapIndex++)
		{
			uint8 bIsValid = 0;
			Ar << bIsValid;

			if (bIsValid)
			{
				FNiagaraShaderMap* ShaderMap = new FNiagaraShaderMap;

				// serialize the id and the material shader map
				ShaderMap->Serialize(Ar, false);

				// Register in the global map
				ShaderMap->Register(ShaderPlatform);

				LoadedShaderMaps.Add(ShaderMap);
			}
		}

		// Assign in two passes: first pass for shader maps with unspecified quality levels,
		// Second pass for shader maps with a specific quality level
			for (int32 ShaderMapIndex = 0; ShaderMapIndex < LoadedShaderMaps.Num(); ShaderMapIndex++)
			{
				FNiagaraShaderMap* LoadedShaderMap = LoadedShaderMaps[ShaderMapIndex];

				if (LoadedShaderMap->GetShaderPlatform() == ShaderPlatform 
					&& LoadedShaderMap->GetShaderMapId().FeatureLevel == GetMaxSupportedFeatureLevel(ShaderPlatform))
				{
					FMaterialResource* MaterialResource = MatchingMaterial->GetMaterialResource(GetMaxSupportedFeatureLevel(ShaderPlatform), (EMaterialQualityLevel::Type)QualityLevelIndex);
					MaterialResource->SetGameThreadShaderMap(LoadedShaderMap);
					MaterialResource->RegisterInlineShaderMap();

					ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
						FSetShaderMapOnMaterialResources,
						FMaterial*,MaterialResource,MaterialResource,
						FNiagaraShaderMap*,LoadedShaderMap,LoadedShaderMap,
					{
						MaterialResource->SetRenderingThreadShaderMap(LoadedShaderMap);
					});
				}
			}
		}
	}
	*/
}




/**
* Compiles the shaders for a script and caches them in this shader map.
* @param script - The script to compile shaders for.
* @param InShaderMapId - the script id and set of static parameters to compile for
* @param Platform - The platform to compile to
*/
void FNiagaraShaderMap::Compile(
	FNiagaraScript* Script,
	const FNiagaraShaderMapId& InShaderMapId,
	TRefCountPtr<FShaderCompilerEnvironment> CompilationEnvironment,
	const FNiagaraComputeShaderCompilationOutput& InNiagaraCompilationOutput,
	EShaderPlatform InPlatform,
	bool bSynchronousCompile,
	bool bApplyCompletedShaderMapForRendering)
{
	if (FPlatformProperties::RequiresCookedData())
	{
		UE_LOG(LogShaders, Fatal, TEXT("Trying to compile Niagara shader %s at run-time, which is not supported on consoles!"), *Script->GetFriendlyName() );
	}
	else
	{
		// Make sure we are operating on a referenced shader map or the below Find will cause this shader map to be deleted,
		// Since it creates a temporary ref counted pointer.
		check(NumRefs > 0);
  
		// Add this shader map and to NiagaraShaderMapsBeingCompiled
		TArray<FNiagaraScript*>* CorrespondingScripts = NiagaraShaderMapsBeingCompiled.Find(this);
  
		if (CorrespondingScripts)
		{
			check(!bSynchronousCompile);
			CorrespondingScripts->AddUnique(Script);
		}
		else
		{
			// Assign a unique identifier so that shaders from this shader map can be associated with it after a deferred compile
			CompilingId = NextCompilingId;
			check(NextCompilingId < UINT_MAX);
			NextCompilingId++;
  
			TArray<FNiagaraScript*> NewCorrespondingScripts;
			NewCorrespondingScripts.Add(Script);
			NiagaraShaderMapsBeingCompiled.Add(this, NewCorrespondingScripts);
#if DEBUG_INFINITESHADERCOMPILE
			UE_LOG(LogTemp, Display, TEXT("Added Niagara ShaderMap 0x%08X%08X with Script 0x%08X%08X to NiagaraShaderMapsBeingCompiled"), (int)((int64)(this) >> 32), (int)((int64)(this)), (int)((int64)(Script) >> 32), (int)((int64)(Script)));
#endif  
			// Setup the compilation environment.
			Script->SetupShaderCompilationEnvironment(InPlatform, *CompilationEnvironment);
  
			// Store the script name for debugging purposes.
			FriendlyName = Script->GetFriendlyName();
			NiagaraCompilationOutput = InNiagaraCompilationOutput;
			ShaderMapId = InShaderMapId;
			Platform = InPlatform;

			uint32 NumShaders = 0;
			TArray<FNiagaraShaderCompileJob*> NewJobs;
	
			// Iterate over all shader types.
			TMap<FShaderType*, FNiagaraShaderCompileJob*> SharedShaderJobs;
			for(TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList());ShaderTypeIt;ShaderTypeIt.Next())
			{
				FNiagaraShaderType* ShaderType = ShaderTypeIt->GetNiagaraShaderType();
				if (ShaderType && ShouldCacheNiagaraShader(ShaderType, InPlatform, Script))
				{
					// Compile this niagara shader .
					TArray<FString> ShaderErrors;
  
					// Only compile the shader if we don't already have it
					if (!HasShader(ShaderType))
					{
						auto* Job = ShaderType->BeginCompileShader(
							CompilingId,
							Script,
							CompilationEnvironment,
							Platform,
							NewJobs,
							FShaderTarget(ShaderType->GetFrequency(), Platform),
							Script->GetDataInterfaceBufferDescriptors()
							);
						check(!SharedShaderJobs.Find(ShaderType));
						SharedShaderJobs.Add(ShaderType, Job);
					}
					NumShaders++;
				}
			}
  
			if (!CorrespondingScripts)
			{
				UE_LOG(LogShaders, Warning, TEXT("		%u Shaders"), NumShaders);
			}

			// Register this shader map in the global script->shadermap map
			Register(InPlatform);
  
			// Mark the shader map as not having been finalized with ProcessCompilationResults
			bCompilationFinalized = false;
  
			// Mark as not having been compiled
			bCompiledSuccessfully = false;
  
			GNiagaraShaderCompilationManager.AddJobs(NewJobs);
  
			// Compile the shaders for this shader map now if not deferring and deferred compiles are not enabled globally
			if (bSynchronousCompile)
			{
				TArray<int32> CurrentShaderMapId;
				CurrentShaderMapId.Add(CompilingId);
				//GNiagaraShaderCompilationManager->FinishCompilation(*FriendlyName, CurrentShaderMapId);
			}
		}
	}
}

FShader* FNiagaraShaderMap::ProcessCompilationResultsForSingleJob(FNiagaraShaderCompileJob* SingleJob, const FSHAHash& ShaderMapHash)
{
	check(SingleJob);
	const FNiagaraShaderCompileJob& CurrentJob = *SingleJob;
	check(CurrentJob.Id == CompilingId);

	FShader* Shader = nullptr;

	FNiagaraShaderType* NiagaraShaderType = CurrentJob.ShaderType->GetNiagaraShaderType();
	check(NiagaraShaderType);
	Shader = NiagaraShaderType->FinishCompileShader(ShaderMapHash, CurrentJob, FriendlyName);
	FNiagaraShader *NiagaraShader = static_cast<FNiagaraShader*>(Shader);
	check(Shader);
	check(!HasShader(NiagaraShaderType));
	AddShader(NiagaraShaderType, Shader);

	return Shader;
}

bool FNiagaraShaderMap::ProcessCompilationResults(const TArray<FNiagaraShaderCompileJob*>& InCompilationResults, int32& InOutJobIndex, float& TimeBudget)
{
	check(InOutJobIndex < InCompilationResults.Num());

	double StartTime = FPlatformTime::Seconds();

	FSHAHash ShaderMapHash;
	ShaderMapId.GetScriptHash(ShaderMapHash);

	do
	{
		FNiagaraShaderCompileJob* SingleJob = InCompilationResults[InOutJobIndex];
		ensure(SingleJob);

		{
			ProcessCompilationResultsForSingleJob(SingleJob, ShaderMapHash);
		}

		InOutJobIndex++;
		
		double NewStartTime = FPlatformTime::Seconds();
		TimeBudget -= NewStartTime - StartTime;
		StartTime = NewStartTime;
	}
	while ((TimeBudget > 0.0f) && (InOutJobIndex < InCompilationResults.Num()));

	if (InOutJobIndex == InCompilationResults.Num())
	{
		SaveToDerivedDataCache();
		// The shader map can now be used on the rendering thread
		bCompilationFinalized = true;
		return true;
	}

	return false;
}

bool FNiagaraShaderMap::TryToAddToExistingCompilationTask(FNiagaraScript* Script)
{
	check(NumRefs > 0);
	TArray<FNiagaraScript*>* CorrespondingScripts = FNiagaraShaderMap::NiagaraShaderMapsBeingCompiled.Find(this);

	if (CorrespondingScripts)
	{
		CorrespondingScripts->AddUnique(Script);
#if DEBUG_INFINITESHADERCOMPILE
		UE_LOG(LogTemp, Display, TEXT("Added shader map 0x%08X%08X from Niagara script 0x%08X%08X"), (int)((int64)(this) >> 32), (int)((int64)(this)), (int)((int64)(Script) >> 32), (int)((int64)(Script)));
#endif
		return true;
	}

	return false;
}

bool FNiagaraShaderMap::IsNiagaraShaderComplete(const FNiagaraScript* Script, const FNiagaraShaderType* ShaderType, bool bSilent)
{
	// If we should cache this script, it's incomplete if the shader is missing
	if (ShouldCacheNiagaraShader(ShaderType, Platform, Script) &&	!HasShader((FShaderType*)ShaderType))
	{
		if (!bSilent)
		{
			UE_LOG(LogShaders, Warning, TEXT("Incomplete shader %s, missing FNiagaraShader %s."), *Script->GetFriendlyName(), ShaderType->GetName());
		}
		return false;
	}

	return true;
}

bool FNiagaraShaderMap::IsComplete(const FNiagaraScript* Script, bool bSilent)
{
	check(!IsInRenderingThread());
	// Make sure we are operating on a referenced shader map or the below Find will cause this shader map to be deleted,
	// Since it creates a temporary ref counted pointer.
	check(NumRefs > 0);
	const TArray<FNiagaraScript*>* CorrespondingScripts = FNiagaraShaderMap::NiagaraShaderMapsBeingCompiled.Find(this);

	if (CorrespondingScripts)
	{
		check(!bCompilationFinalized);
		return false;
	}

	// Iterate over all shader types.
	for(TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList());ShaderTypeIt;ShaderTypeIt.Next())
	{
		// Find this shader type in the script's shader map.
		const FNiagaraShaderType* ShaderType = ShaderTypeIt->GetNiagaraShaderType();
		if (ShaderType && !IsNiagaraShaderComplete(Script, ShaderType, bSilent))
		{
			return false;
		}
	}

	return true;
}

void FNiagaraShaderMap::LoadMissingShadersFromMemory(const FNiagaraScript* Script)
{
	// Make sure we are operating on a referenced shader map or the below Find will cause this shader map to be deleted,
	// Since it creates a temporary ref counted pointer.
	check(NumRefs > 0);

	const TArray<FNiagaraScript*>* CorrespondingScripts = FNiagaraShaderMap::NiagaraShaderMapsBeingCompiled.Find(this);

	if (CorrespondingScripts)
	{
		check(!bCompilationFinalized);
		return;
	}

	FSHAHash ShaderMapHash;
	ShaderMapId.GetScriptHash(ShaderMapHash);

	// Try to find necessary FNiagaraShaderType's in memory
	for (TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList());ShaderTypeIt;ShaderTypeIt.Next())
	{
		FNiagaraShaderType* ShaderType = ShaderTypeIt->GetNiagaraShaderType();
		if (ShaderType && ShouldCacheNiagaraShader(ShaderType, Platform, Script) && !HasShader(ShaderType))
		{
			FShaderId ShaderId(ShaderMapHash, nullptr, nullptr, ShaderType, FShaderTarget(ShaderType->GetFrequency(), Platform));
			FShader* FoundShader = ShaderType->FindShaderById(ShaderId);
			if (FoundShader)
			{
				AddShader(ShaderType, FoundShader);
			}
		}
	}
}
#endif

void FNiagaraShaderMap::GetShaderList(TMap<FShaderId, FShader*>& OutShaders) const
{
	TShaderMap<FNiagaraShaderType>::GetShaderList(OutShaders);
}

/**
 * Registers a Niagara shader map in the global map so it can be used by scripts.
 */
void FNiagaraShaderMap::Register(EShaderPlatform InShaderPlatform)
{
	extern int32 GCreateNiagaraShadersOnLoad;
	if (GCreateNiagaraShadersOnLoad && Platform == InShaderPlatform)
	{
		for (auto KeyValue : GetShaders())
		{
			FShader* Shader = KeyValue.Value;
			if (Shader)
			{
				Shader->BeginInitializeResources();
			}
		}
	}

	if (!bRegistered)
	{
		INC_DWORD_STAT(STAT_Shaders_NumShaderMaps);
		INC_DWORD_STAT_BY(STAT_Shaders_ShaderMapMemory, GetSizeBytes());
	}

	GIdToNiagaraShaderMap[Platform].Add(ShaderMapId,this);
	bRegistered = true;
}

void FNiagaraShaderMap::AddRef()
{
	check(!bDeletedThroughDeferredCleanup);
	++NumRefs;
}

void FNiagaraShaderMap::Release()
{
	check(NumRefs > 0);
	if(--NumRefs == 0)
	{
		if (bRegistered)
		{
			DEC_DWORD_STAT(STAT_Shaders_NumShaderMaps);
			DEC_DWORD_STAT_BY(STAT_Shaders_ShaderMapMemory, GetSizeBytes());

			GIdToNiagaraShaderMap[Platform].Remove(ShaderMapId);
			bRegistered = false;
		}

		BeginCleanup(this);
	}
}

FNiagaraShaderMap::FNiagaraShaderMap() :
	TShaderMap<FNiagaraShaderType>(SP_NumPlatforms),
	Platform(SP_NumPlatforms),
	CompilingId(1),
	NumRefs(0),
	bDeletedThroughDeferredCleanup(false),
	bRegistered(false),
	bCompilationFinalized(true),
	bCompiledSuccessfully(true),
	bIsPersistent(true) 
{
	checkSlow(IsInGameThread() || IsAsyncLoading());
	AllNiagaraShaderMaps.Add(this);
}

FNiagaraShaderMap::~FNiagaraShaderMap()
{ 
	checkSlow(IsInGameThread() || IsAsyncLoading());
	check(bDeletedThroughDeferredCleanup);
	check(!bRegistered);
	AllNiagaraShaderMaps.RemoveSwap(this);
}

/**
 * Removes all entries in the cache with exceptions based on a shader type
 * @param ShaderType - The shader type to flush
 */
void FNiagaraShaderMap::FlushShadersByShaderType(FShaderType* ShaderType)
{
	if (ShaderType->GetNiagaraShaderType())
	{
		RemoveShaderType(ShaderType->GetNiagaraShaderType());	
	}
}



void FNiagaraShaderMap::Serialize(FArchive& Ar, bool bInlineShaderResources)
{
	// Note: This is saved to the DDC, not into packages (except when cooked)
	// Backwards compatibility therefore will not work based on the version of Ar
	// Instead, just bump NIAGARASHADERMAP_DERIVEDDATA_VER

	ShaderMapId.Serialize(Ar);

	// serialize the platform enum as a uint8
	int32 TempPlatform = (int32)Platform;
	Ar << TempPlatform;
	Platform = (EShaderPlatform)TempPlatform;

	Ar << FriendlyName;

	NiagaraCompilationOutput.Serialize(Ar);

	Ar << DebugDescription;

	if (Ar.IsSaving())
	{
		TShaderMap<FNiagaraShaderType>::SerializeInline(Ar, bInlineShaderResources, false);
		RegisterSerializedShaders();
	}

	if (Ar.IsLoading())
	{
		TShaderMap<FNiagaraShaderType>::SerializeInline(Ar, bInlineShaderResources, false);
	}
}

void FNiagaraShaderMap::RegisterSerializedShaders()
{
	check(IsInGameThread());

	TShaderMap<FNiagaraShaderType>::RegisterSerializedShaders();
}

void FNiagaraShaderMap::DiscardSerializedShaders()
{
	TShaderMap<FNiagaraShaderType>::DiscardSerializedShaders();
}

void FNiagaraShaderMap::RemovePendingScript(FNiagaraScript* Script)
{
	for (TMap<TRefCountPtr<FNiagaraShaderMap>, TArray<FNiagaraScript*> >::TIterator It(NiagaraShaderMapsBeingCompiled); It; ++It)
	{
		TArray<FNiagaraScript*>& Scripts = It.Value();
		int32 Result = Scripts.Remove(Script);
#if DEBUG_INFINITESHADERCOMPILE
		if ( Result )
		{
			UE_LOG(LogTemp, Display, TEXT("Removed shader map 0x%08X%08X from script 0x%08X%08X"), (int)((int64)(It.Key().GetReference()) >> 32), (int)((int64)(It.Key().GetReference())), (int)((int64)(Script) >> 32), (int)((int64)(Script)));
		}
#endif
	}
}

const FNiagaraShaderMap* FNiagaraShaderMap::GetShaderMapBeingCompiled(const FNiagaraScript* Script)
{
	// Inefficient search, but only when compiling a lot of shaders
	for (TMap<TRefCountPtr<FNiagaraShaderMap>, TArray<FNiagaraScript*> >::TIterator It(NiagaraShaderMapsBeingCompiled); It; ++It)
	{
		TArray<FNiagaraScript*>& Scripts = It.Value();
		
		for (int32 ScriptIndex = 0; ScriptIndex < Scripts.Num(); ScriptIndex++)
		{
			if (Scripts[ScriptIndex] == Script)
			{
				return It.Key();
			}
		}
	}

	return NULL;
}




FNiagaraShader::FNiagaraShader(const FNiagaraShaderType::CompiledShaderInitializerType& Initializer)
	: FShader(Initializer)
	, DebugDescription(Initializer.DebugDescription)
{
	check(!DebugDescription.IsEmpty());
	SetDatainterfaceBufferDescriptors(Initializer.DIBufferDescriptors);
	BindParams(Initializer.ParameterMap);
}



bool FNiagaraShader::Serialize(FArchive& Ar)
{
	const bool bShaderHasOutdatedParameters = FShader::Serialize(Ar);
	Ar << NiagaraUniformBuffer;	// do we need to keep about uniform buffer parameters?

//	Ar.UsingCustomVersion(FNiagaraShaderCustomVersion::GUID);		// only changes version if not loading
//	const int32 NiagaraVer = Ar.CustomVer(FNiagaraShaderCustomVersion::GUID);

	Ar << DebugDescription;

	Ar << FloatInputBufferParam;
	Ar << IntInputBufferParam;
	Ar << FloatOutputBufferParam;
	Ar << IntOutputBufferParam;
	Ar << OutputIndexBufferParam;
	Ar << EmitterTickCounterParam;

	Ar << NumInstancesPerThreadParam;
	Ar << NumInstancesParam;
	Ar << StartInstanceParam;
	Ar << GroupStartInstanceParam;
	Ar << ComponentBufferSizeReadParam;
	Ar << ComponentBufferSizeWriteParam;

	for (uint32 i = 0; i < MAX_CONCURRENT_EVENT_DATASETS; i++)
	{
		Ar << EventFloatUAVParams[i];
		Ar << EventIntUAVParams[i];
		Ar << EventFloatSRVParams[i];
		Ar << EventIntSRVParams[i];
	}

	Ar << DIBufferDescriptors;

	// params for data interface buffers
	if (Ar.IsLoading())
	{
		for (TArray<DIGPUBufferParamDescriptor> &InterfaceDescs : DIBufferDescriptors)
		{
			int32 Idx = NameToDIBufferParamMap.AddDefaulted(1);
			for (DIGPUBufferParamDescriptor &Desc : InterfaceDescs)
			{
				FShaderResourceParameter &Param = NameToDIBufferParamMap[Idx].Add(*Desc.BufferParamName);
				Ar << Param;
			}
		}
	}
	else
	{
		uint32 Idx = 0;
		for (TArray<DIGPUBufferParamDescriptor> &InterfaceDescs : DIBufferDescriptors)
		{
			for (DIGPUBufferParamDescriptor &Desc : InterfaceDescs)
			{
				FShaderResourceParameter *Param = NameToDIBufferParamMap[Idx].Find(*Desc.BufferParamName);
				check(Param);
				Ar << *Param;
			}
			Idx++;
		}
	}

	Ar << SimulateStartInstanceParam;
	Ar << NumThreadGroupsParam;
	Ar << EmitterConstantBufferParam;
	Ar << DataInterfaceUniformBufferParam;
	Ar << NumEventsPerParticleParam;
	Ar << NumParticlesPerEventParam;
	Ar << CopyInstancesBeforeStartParam;

	return bShaderHasOutdatedParameters;
}


uint32 FNiagaraShader::GetAllocatedSize() const
{
	return FShader::GetAllocatedSize()
		//+ ParameterCollectionUniformBuffers.GetAllocatedSize()
		+ DebugDescription.GetAllocatedSize();
}


FArchive &operator << (FArchive &Ar, DIGPUBufferParamDescriptor &Desc)
{
	Ar << Desc.BufferParamName;
	Ar << Desc.Index;
	return Ar;
}
