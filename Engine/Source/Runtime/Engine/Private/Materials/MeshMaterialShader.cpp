// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MeshMaterialShader.cpp: Mesh material shader implementation.
=============================================================================*/

#include "MeshMaterialShader.h"
#include "ShaderCompiler.h"
#include "ProfilingDebugging/CookStats.h"

#if ENABLE_COOK_STATS
namespace MaterialMeshCookStats
{
	static int32 ShadersCompiled = 0;
	static FCookStatsManager::FAutoRegisterCallback RegisterCookStats([](FCookStatsManager::AddStatFuncRef AddStat)
	{
		AddStat(TEXT("MeshMaterial.Misc"), FCookStatsManager::CreateKeyValueArray(
			TEXT("ShadersCompiled"), ShadersCompiled
			));
	});
}
#endif

static inline bool ShouldCacheMeshShader(const FMeshMaterialShaderType* ShaderType, EShaderPlatform Platform, const FMaterial* Material, FVertexFactoryType* InVertexFactoryType)
{
	return ShaderType->ShouldCache(Platform, Material, InVertexFactoryType) &&
		Material->ShouldCache(Platform, ShaderType, InVertexFactoryType) &&
		InVertexFactoryType->ShouldCache(Platform, Material, ShaderType);
}

/**
 * Enqueues a compilation for a new shader of this type.
 * @param Platform - The platform to compile for.
 * @param Material - The material to link the shader with.
 * @param VertexFactoryType - The vertex factory to compile with.
 */
FShaderCompileJob* FMeshMaterialShaderType::BeginCompileShader(
	uint32 ShaderMapId,
	EShaderPlatform Platform,
	const FMaterial* Material,
	FShaderCompilerEnvironment* MaterialEnvironment,
	FVertexFactoryType* VertexFactoryType,
	const FShaderPipelineType* ShaderPipeline,
	TArray<FShaderCommonCompileJob*>& NewJobs
	)
{
	FShaderCompileJob* NewJob = new FShaderCompileJob(ShaderMapId, VertexFactoryType, this);

	NewJob->Input.SharedEnvironment = MaterialEnvironment;
	FShaderCompilerEnvironment& ShaderEnvironment = NewJob->Input.Environment;

	// apply the vertex factory changes to the compile environment
	check(VertexFactoryType);
	VertexFactoryType->ModifyCompilationEnvironment(Platform, Material, ShaderEnvironment);

	//update material shader stats
	UpdateMaterialShaderCompilingStats(Material);

	UE_LOG(LogShaders, Verbose, TEXT("			%s"), GetName());
	COOK_STAT(MaterialMeshCookStats::ShadersCompiled++);

	// Allow the shader type to modify the compile environment.
	SetupCompileEnvironment(Platform, Material, ShaderEnvironment);

	bool bAllowDevelopmentShaderCompile = Material->GetAllowDevelopmentShaderCompile();

	// Compile the shader environment passed in with the shader type's source code.
	::GlobalBeginCompileShader(
		Material->GetFriendlyName(),
		VertexFactoryType,
		this,
		ShaderPipeline,
		GetShaderFilename(),
		GetFunctionName(),
		FShaderTarget(GetFrequency(),Platform),
		NewJob,
		NewJobs,
		bAllowDevelopmentShaderCompile
		);
	return NewJob;
}

void FMeshMaterialShaderType::BeginCompileShaderPipeline(
	uint32 ShaderMapId,
	EShaderPlatform Platform,
	const FMaterial* Material,
	FShaderCompilerEnvironment* MaterialEnvironment,
	FVertexFactoryType* VertexFactoryType,
	const FShaderPipelineType* ShaderPipeline,
	const TArray<FMeshMaterialShaderType*>& ShaderStages,
	TArray<FShaderCommonCompileJob*>& NewJobs)
{
	check(ShaderStages.Num() > 0);
	check(ShaderPipeline);
	UE_LOG(LogShaders, Verbose, TEXT("	Pipeline: %s"), ShaderPipeline->GetName());

	// Add all the jobs as individual first, then add the dependencies into a pipeline job
	auto* NewPipelineJob = new FShaderPipelineCompileJob(ShaderMapId, ShaderPipeline, ShaderStages.Num());
	for (int32 Index = 0; Index < ShaderStages.Num(); ++Index)
	{
		auto* ShaderStage = ShaderStages[Index];
		ShaderStage->BeginCompileShader(ShaderMapId, Platform, Material, MaterialEnvironment, VertexFactoryType, ShaderPipeline, NewPipelineJob->StageJobs);
	}

	NewJobs.Add(NewPipelineJob);
}

/**
 * Either creates a new instance of this type or returns an equivalent existing shader.
 * @param Material - The material to link the shader with.
 * @param CurrentJob - Compile job that was enqueued by BeginCompileShader.
 */
FShader* FMeshMaterialShaderType::FinishCompileShader(
	const FUniformExpressionSet& UniformExpressionSet, 
	const FSHAHash& MaterialShaderMapHash,
	const FShaderCompileJob& CurrentJob,
	const FShaderPipelineType* ShaderPipelineType,
	const FString& InDebugDescription)
{
	check(CurrentJob.bSucceeded);
	check(CurrentJob.VFType);

	FShaderType* SpecificType = CurrentJob.ShaderType->LimitShaderResourceToThisType() ? CurrentJob.ShaderType : NULL;

	// Reuse an existing resource with the same key or create a new one based on the compile output
	// This allows FShaders to share compiled bytecode and RHI shader references
	FShaderResource* Resource = FShaderResource::FindOrCreateShaderResource(CurrentJob.Output, SpecificType);

	if (ShaderPipelineType && !ShaderPipelineType->ShouldOptimizeUnusedOutputs())
	{
		// If sharing shaders in this pipeline, remove it from the type/id so it uses the one in the shared shadermap list
		ShaderPipelineType = nullptr;
	}

	// Find a shader with the same key in memory
	FShader* Shader = CurrentJob.ShaderType->FindShaderById(FShaderId(MaterialShaderMapHash, ShaderPipelineType, CurrentJob.VFType, CurrentJob.ShaderType, CurrentJob.Input.Target));

	// There was no shader with the same key so create a new one with the compile output, which will bind shader parameters
	if (!Shader)
	{
		Shader = (*ConstructCompiledRef)(CompiledShaderInitializerType(this, CurrentJob.Output, Resource, UniformExpressionSet, MaterialShaderMapHash, InDebugDescription, ShaderPipelineType, CurrentJob.VFType));
		CurrentJob.Output.ParameterMap.VerifyBindingsAreComplete(GetName(), CurrentJob.Output.Target, CurrentJob.VFType);
	}

	return Shader;
}

/**
 * Enqueues compilation for all shaders for a material and vertex factory type.
 * @param Material - The material to compile shaders for.
 * @param VertexFactoryType - The vertex factory type to compile shaders for.
 * @param Platform - The platform to compile for.
 */
uint32 FMeshMaterialShaderMap::BeginCompile(
	uint32 ShaderMapId,
	const FMaterialShaderMapId& InShaderMapId, 
	const FMaterial* Material,
	FShaderCompilerEnvironment* MaterialEnvironment,
	EShaderPlatform InPlatform,
	TArray<FShaderCommonCompileJob*>& NewJobs
	)
{
	if (!VertexFactoryType)
	{
		return 0;
	}

	uint32 NumShadersPerVF = 0;
	TSet<FString> ShaderTypeNames;

	// Iterate over all mesh material shader types.
	TMap<FShaderType*, FShaderCompileJob*> SharedShaderJobs;
	for (TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList());ShaderTypeIt;ShaderTypeIt.Next())
	{
		FMeshMaterialShaderType* ShaderType = ShaderTypeIt->GetMeshMaterialShaderType();
		if (ShaderType && ShouldCacheMeshShader(ShaderType, InPlatform, Material, VertexFactoryType))
		{
			// Verify that the shader map Id contains inputs for any shaders that will be put into this shader map
			check(InShaderMapId.ContainsVertexFactoryType(VertexFactoryType));
			check(InShaderMapId.ContainsShaderType(ShaderType));

			NumShadersPerVF++;
			// only compile the shader if we don't already have it
			if (!HasShader(ShaderType))
			{
				// Compile this mesh material shader for this material and vertex factory type.
				auto* Job = ShaderType->BeginCompileShader(
					ShaderMapId,
					InPlatform,
					Material,
					MaterialEnvironment,
					VertexFactoryType,
					nullptr,
					NewJobs
					);
				check(!SharedShaderJobs.Find(ShaderType));
				SharedShaderJobs.Add(ShaderType, Job);
			}
		}
	}

	// Now the pipeline jobs; if it's a shareable pipeline, do not add duplicate jobs
	const bool bHasTessellation = Material->GetTessellationMode() != MTM_NoTessellation;
	for (TLinkedList<FShaderPipelineType*>::TIterator ShaderPipelineIt(FShaderPipelineType::GetTypeList());ShaderPipelineIt;ShaderPipelineIt.Next())
	{
		const FShaderPipelineType* Pipeline = *ShaderPipelineIt;
		if (Pipeline->IsMeshMaterialTypePipeline() && Pipeline->HasTessellation() == bHasTessellation)
		{
			auto& StageTypes = Pipeline->GetStages();
			int32 NumShaderStagesToCompile = 0;
			for (auto* Shader : StageTypes)
			{
				const FMeshMaterialShaderType* ShaderType = Shader->GetMeshMaterialShaderType();
				if (ShouldCacheMeshShader(ShaderType, InPlatform, Material, VertexFactoryType))
				{
					++NumShaderStagesToCompile;
				}
				else
				{
					break;
				}
			}

			if (NumShaderStagesToCompile == StageTypes.Num())
			{
				// Verify that the shader map Id contains inputs for any shaders that will be put into this shader map
				check(InShaderMapId.ContainsShaderPipelineType(Pipeline));

				if (Pipeline->ShouldOptimizeUnusedOutputs())
				{
					NumShadersPerVF += NumShaderStagesToCompile;
					TArray<FMeshMaterialShaderType*> ShaderStagesToCompile;
					for (auto* Shader : StageTypes)
					{
						const FMeshMaterialShaderType* ShaderType = Shader->GetMeshMaterialShaderType();

						// Verify that the shader map Id contains inputs for any shaders that will be put into this shader map
						check(InShaderMapId.ContainsVertexFactoryType(VertexFactoryType));
						check(InShaderMapId.ContainsShaderType(ShaderType));
						ShaderStagesToCompile.Add((FMeshMaterialShaderType*)ShaderType);
					}

					// Make a pipeline job with all the stages
					FMeshMaterialShaderType::BeginCompileShaderPipeline(ShaderMapId, InPlatform, Material, MaterialEnvironment, VertexFactoryType, Pipeline, ShaderStagesToCompile, NewJobs);
				}
				else
				{
					// If sharing shaders amongst pipelines, add this pipeline as a dependency of an existing job
					for (const FShaderType* ShaderType : StageTypes)
					{
						FShaderCompileJob** Job = SharedShaderJobs.Find(ShaderType);
						checkf(Job, TEXT("Couldn't find existing shared job for mesh shader %s on pipeline %s!"), ShaderType->GetName(), Pipeline->GetName());
						auto* SingleJob = (*Job)->GetSingleShaderJob();
						auto& PipelinesToShare = SingleJob->SharingPipelines.FindOrAdd(VertexFactoryType);
						check(!PipelinesToShare.Contains(Pipeline));
						PipelinesToShare.Add(Pipeline);
					}
				}
			}
		}
	}

	if (NumShadersPerVF > 0)
	{
		UE_LOG(LogShaders, Verbose, TEXT("			%s - %u shaders"), VertexFactoryType->GetName(), NumShadersPerVF);
	}

	return NumShadersPerVF;
}

inline bool FMeshMaterialShaderMap::IsMeshShaderComplete(const FMeshMaterialShaderMap* MeshShaderMap, EShaderPlatform Platform, const FMaterial* Material, const FMeshMaterialShaderType* ShaderType, const FShaderPipelineType* Pipeline, FVertexFactoryType* InVertexFactoryType, bool bSilent)
{
	// If we should cache this shader then the map is empty IF
	//		The shadermap is empty
	//		OR If it doesn't have a pipeline and needs one
	//		OR it's not in the shadermap
	if (ShouldCacheMeshShader(ShaderType, Platform, Material, InVertexFactoryType) &&
		(!MeshShaderMap || (Pipeline && !MeshShaderMap->HasShaderPipeline(Pipeline)) || (!Pipeline && !MeshShaderMap->HasShader((FShaderType*)ShaderType))))
	{
		if (!bSilent)
		{
			if (Pipeline)
			{
				UE_LOG(LogShaders, Warning, TEXT("Incomplete material %s, missing pipeline %s from %s."), *Material->GetFriendlyName(), Pipeline->GetName(), InVertexFactoryType->GetName());
			}
			else
			{
				UE_LOG(LogShaders, Warning, TEXT("Incomplete material %s, missing %s from %s."), *Material->GetFriendlyName(), ShaderType->GetName(), InVertexFactoryType->GetName());
			}
		}
		return false;
	}

	return true;
};


bool FMeshMaterialShaderMap::IsComplete(
	const FMeshMaterialShaderMap* MeshShaderMap,
	EShaderPlatform Platform,
	const FMaterial* Material,
	FVertexFactoryType* InVertexFactoryType,
	bool bSilent
	)
{
	// Iterate over all mesh material shader types.
	for (TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList());ShaderTypeIt;ShaderTypeIt.Next())
	{
		FMeshMaterialShaderType* ShaderType = ShaderTypeIt->GetMeshMaterialShaderType();
		if (ShaderType && !IsMeshShaderComplete(MeshShaderMap, Platform, Material, ShaderType, nullptr, InVertexFactoryType, bSilent))
		{
			return false;
		}
	}

	// Iterate over all pipeline types
	const bool bHasTessellation = Material->GetTessellationMode() != MTM_NoTessellation;
	for (TLinkedList<FShaderPipelineType*>::TIterator ShaderPipelineIt(FShaderPipelineType::GetTypeList());ShaderPipelineIt;ShaderPipelineIt.Next())
	{
		const FShaderPipelineType* ShaderPipelineType = *ShaderPipelineIt;
		if (ShaderPipelineType->IsMeshMaterialTypePipeline() && ShaderPipelineType->HasTessellation() == bHasTessellation)
		{
			auto& Stages = ShaderPipelineType->GetStages();

			// Verify all the ShouldCache are in sync
			int32 NumShouldCache = 0;
			for (int32 Index = 0; Index < Stages.Num(); ++Index)
			{
				auto* ShaderType = Stages[Index]->GetMeshMaterialShaderType();
				if (ShouldCacheMeshShader(ShaderType, Platform, Material, InVertexFactoryType))
				{
					++NumShouldCache;
				}
				else
				{
					break;
				}
			}

			if (NumShouldCache == Stages.Num())
			{
				// Now check the completeness of the shader map
				for (int32 Index = 0; Index < Stages.Num(); ++Index)
				{
					auto* ShaderType = Stages[Index]->GetMeshMaterialShaderType();
					if (ShaderType && !IsMeshShaderComplete(MeshShaderMap, Platform, Material, ShaderType, ShaderPipelineType, InVertexFactoryType, bSilent))
					{
						return false;
					}
				}
			}
		}
	}

	return true;
}

void FMeshMaterialShaderMap::LoadMissingShadersFromMemory(
	const FSHAHash& MaterialShaderMapHash, 
	const FMaterial* Material, 
	EShaderPlatform InPlatform)
{
	for (TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList());ShaderTypeIt;ShaderTypeIt.Next())
	{
		FMeshMaterialShaderType* ShaderType = ShaderTypeIt->GetMeshMaterialShaderType();
		if (ShaderType && ShouldCacheMeshShader(ShaderType, InPlatform, Material, VertexFactoryType) && !HasShader((FShaderType*)ShaderType))
		{
			const FShaderId ShaderId(MaterialShaderMapHash, nullptr, VertexFactoryType, (FShaderType*)ShaderType, FShaderTarget(ShaderType->GetFrequency(), InPlatform));
			FShader* FoundShader = ((FShaderType*)ShaderType)->FindShaderById(ShaderId);

			if (FoundShader)
			{
				AddShader((FShaderType*)ShaderType, FoundShader);
			}
		}
	}

	// Try to find necessary FShaderPipelineTypes in memory
	const bool bHasTessellation = Material->GetTessellationMode() != MTM_NoTessellation;
	for (TLinkedList<FShaderPipelineType*>::TIterator ShaderPipelineIt(FShaderPipelineType::GetTypeList());ShaderPipelineIt;ShaderPipelineIt.Next())
	{
		const FShaderPipelineType* PipelineType = *ShaderPipelineIt;
		if (PipelineType && PipelineType->IsMeshMaterialTypePipeline() && !HasShaderPipeline(PipelineType) && PipelineType->HasTessellation() == bHasTessellation)
		{
			auto& Stages = PipelineType->GetStages();
			int32 NumShaders = 0;
			for (const FShaderType* Shader : Stages)
			{
				FMeshMaterialShaderType* ShaderType = (FMeshMaterialShaderType*)Shader->GetMeshMaterialShaderType();
				if (ShaderType && ShouldCacheMeshShader(ShaderType, InPlatform, Material, VertexFactoryType))
				{
					++NumShaders;
				}
				else
				{
					break;
				}
			}

			if (NumShaders == Stages.Num())
			{
				TArray<FShader*> ShadersForPipeline;
				for (auto* Shader : Stages)
				{
					FMeshMaterialShaderType* ShaderType = (FMeshMaterialShaderType*)Shader->GetMeshMaterialShaderType();
					if (!HasShader(ShaderType))
					{
						const FShaderId ShaderId(MaterialShaderMapHash, PipelineType->ShouldOptimizeUnusedOutputs() ? PipelineType : nullptr, VertexFactoryType, ShaderType, FShaderTarget(ShaderType->GetFrequency(), InPlatform));
						FShader* FoundShader = ShaderType->FindShaderById(ShaderId);
						if (FoundShader)
						{
							AddShader(ShaderType, FoundShader);
							ShadersForPipeline.Add(FoundShader);
						}
					}
				}

				if (ShadersForPipeline.Num() == NumShaders && !HasShaderPipeline(PipelineType))
				{
					auto* Pipeline = new FShaderPipeline(PipelineType, ShadersForPipeline);
					AddShaderPipeline(PipelineType, Pipeline);
				}
			}
		}
	}
}

/**
 * Removes all entries in the cache with exceptions based on a shader type
 * @param ShaderType - The shader type to flush
 */
void FMeshMaterialShaderMap::FlushShadersByShaderType(FShaderType* ShaderType)
{
	if (ShaderType->GetMeshMaterialShaderType())
	{
		RemoveShaderType(ShaderType);
	}
}

void FMeshMaterialShaderMap::FlushShadersByShaderPipelineType(const FShaderPipelineType* ShaderPipelineType)
{
	if (ShaderPipelineType->IsMeshMaterialTypePipeline())
	{
		RemoveShaderPipelineType(ShaderPipelineType);
	}
}
