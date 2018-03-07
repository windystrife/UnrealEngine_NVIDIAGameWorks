// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DebugViewModeHelpers.cpp: debug view shader helpers.
=============================================================================*/
#include "DebugViewModeHelpers.h"
#include "DebugViewModeMaterialProxy.h"
#include "ShaderCompiler.h"
#include "ScopedSlowTask.h"
#include "FeedbackContext.h"
#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"

#define LOCTEXT_NAMESPACE "LogDebugViewMode"

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

static bool PlatformSupportsDebugViewShaders(EShaderPlatform Platform)
{
	// List of platforms that have been tested and proved functional.
	return Platform == SP_PCD3D_SM4 || Platform == SP_PCD3D_SM5 || Platform == SP_OPENGL_SM4;
}
 
bool AllowDebugViewPS(EDebugViewShaderMode ShaderMode, EShaderPlatform Platform)
{
#if WITH_EDITOR
	// Those options are used to test compilation on specific platforms
	static const bool bForceQuadOverdraw = FParse::Param(FCommandLine::Get(), TEXT("quadoverdraw"));
	static const bool bForceStreamingAccuracy = FParse::Param(FCommandLine::Get(), TEXT("streamingaccuracy"));
	static const bool bForceTextureStreamingBuild = FParse::Param(FCommandLine::Get(), TEXT("streamingbuild"));

	switch (ShaderMode)
	{
	case DVSM_None:
		return false;
	case DVSM_ShaderComplexity:
		return true;
	case DVSM_ShaderComplexityContainedQuadOverhead:
	case DVSM_ShaderComplexityBleedingQuadOverhead:
	case DVSM_QuadComplexity:
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && (bForceQuadOverdraw || PlatformSupportsDebugViewShaders(Platform));
	case DVSM_PrimitiveDistanceAccuracy:
	case DVSM_MeshUVDensityAccuracy:
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4) && (bForceStreamingAccuracy || PlatformSupportsDebugViewShaders(Platform));
	case DVSM_MaterialTextureScaleAccuracy:
	case DVSM_RequiredTextureResolution:
	case DVSM_OutputMaterialTextureScales:
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4) && (bForceTextureStreamingBuild || PlatformSupportsDebugViewShaders(Platform));
	default:
		return false;
	}
#else
	return ShaderMode == DVSM_ShaderComplexity;
#endif
}

bool AllowDebugViewVSDSHS(EShaderPlatform Platform)
{
#if WITH_EDITOR
	// Those options are used to test compilation on specific platforms
	static const bool bForce = 
		FParse::Param(FCommandLine::Get(), TEXT("quadoverdraw")) || 
		FParse::Param(FCommandLine::Get(), TEXT("streamingaccuracy")) || 
		FParse::Param(FCommandLine::Get(), TEXT("streamingbuild"));

	return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4) && (bForce || PlatformSupportsDebugViewShaders(Platform));
#else
	return false;
#endif
}

bool AllowDebugViewShaderMode(EDebugViewShaderMode ShaderMode)
{
	return AllowDebugViewPS(ShaderMode, GMaxRHIShaderPlatform);
}

#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)


int32 GetNumActorsInWorld(UWorld* InWorld)
{
	int32 ActorCount = 0;
	for (int32 LevelIndex = 0; LevelIndex < InWorld->GetNumLevels(); LevelIndex++)
	{
		ULevel* Level = InWorld->GetLevel(LevelIndex);
		if (!Level)
		{
			continue;
		}

		ActorCount += Level->Actors.Num();
	}
	return ActorCount;
}

bool WaitForShaderCompilation(const FText& Message, FSlowTask& ProgressTask)
{
	FlushRenderingCommands();

	const int32 NumShadersToBeCompiled = GShaderCompilingManager->GetNumRemainingJobs();
	int32 RemainingShaders = NumShadersToBeCompiled;
	if (NumShadersToBeCompiled > 0)
	{
		FScopedSlowTask SlowTask(1.f, Message);

		while (RemainingShaders > 0)
		{
			FPlatformProcess::Sleep(0.01f);
			GShaderCompilingManager->ProcessAsyncResults(false, true);

			const int32 RemainingShadersThisFrame = GShaderCompilingManager->GetNumRemainingJobs();
			if (RemainingShadersThisFrame > 0)
			{
				const int32 NumberOfShadersCompiledThisFrame = RemainingShaders - RemainingShadersThisFrame;

				const float FrameProgress = (float)NumberOfShadersCompiledThisFrame / (float)NumShadersToBeCompiled;
				ProgressTask.EnterProgressFrame(FrameProgress);
				SlowTask.EnterProgressFrame(FrameProgress);
				if (GWarn->ReceivedUserCancel())
				{
					return false;
				}
			}
			RemainingShaders = RemainingShadersThisFrame;
		}
	}
	else
	{
		ProgressTask.EnterProgressFrame();
		if (GWarn->ReceivedUserCancel())
		{
			return false;
		}
	}

	// Extra safety to make sure every shader map is updated
	GShaderCompilingManager->FinishAllCompilation();
	FlushRenderingCommands();

	return true;
}

/** Get the list of all material used in a world 
 *
 * @return true if the operation is a success, false if it was canceled.
 */
bool GetUsedMaterialsInWorld(UWorld* InWorld, OUT TSet<UMaterialInterface*>& OutMaterials, FSlowTask& ProgressTask)
{
#if WITH_EDITORONLY_DATA
	if (!InWorld)
	{
		return false;
	}

	const int32 NumActorsInWorld = GetNumActorsInWorld(InWorld);
	if (!NumActorsInWorld)
	{
		ProgressTask.EnterProgressFrame();
		return true;
	}

	const float OneOverNumActorsInWorld = 1.f / (float)NumActorsInWorld;

	FScopedSlowTask SlowTask(1.f, (LOCTEXT("TextureStreamingBuild_GetTextureStreamingBuildMaterials", "Getting materials to rebuild")));

	for (int32 LevelIndex = 0; LevelIndex < InWorld->GetNumLevels(); LevelIndex++)
	{
		ULevel* Level = InWorld->GetLevel(LevelIndex);
		if (!Level)
		{
			continue;
		}

		for (AActor* Actor : Level->Actors)
		{
			ProgressTask.EnterProgressFrame(OneOverNumActorsInWorld);
			SlowTask.EnterProgressFrame(OneOverNumActorsInWorld);
			if (GWarn->ReceivedUserCancel())
			{
				return false;
			}

			// Check the actor after incrementing the progress.
			if (!Actor)
			{
				continue;
			}

			TInlineComponentArray<UPrimitiveComponent*> Primitives;
			Actor->GetComponents<UPrimitiveComponent>(Primitives);

			for (UPrimitiveComponent* Primitive : Primitives)
			{
				if (!Primitive)
				{
					continue;
				}

				TArray<UMaterialInterface*> Materials;
				Primitive->GetUsedMaterials(Materials);

				for (UMaterialInterface* Material : Materials)
				{
					if (Material)
					{
						OutMaterials.Add(Material);
					}
				}
			}
		}
	}
	return true;
#else
	return false;
#endif
}

/**
 * Build Shaders to compute scales per texture.
 *
 * @param QualityLevel		The quality level for the shaders.
 * @param FeatureLevel		The feature level for the shaders.
 * @param bFullRebuild		Clear all debug shaders before generating the new ones..
 * @param bWaitForPreviousShaders Whether to wait for previous shaders to complete.
 * @param Materials			The materials to update, the one that failed compilation will be removed (IN OUT).
 * @return true if the operation is a success, false if it was canceled.
 */
bool CompileDebugViewModeShaders(EDebugViewShaderMode ShaderMode, EMaterialQualityLevel::Type QualityLevel, ERHIFeatureLevel::Type FeatureLevel, bool bFullRebuild, bool bWaitForPreviousShaders, TSet<UMaterialInterface*>& Materials, FSlowTask& ProgressTask)
{
#if WITH_EDITORONLY_DATA
	if (!GShaderCompilingManager)
	{
		return false;
	}

	check(Materials.Num())

	EMaterialShaderMapUsage::Type ShaderMapUsage = EMaterialShaderMapUsage::Default;

	switch (ShaderMode)
	{
	case DVSM_MaterialTextureScaleAccuracy:
	case DVSM_OutputMaterialTextureScales:	
		ShaderMapUsage = EMaterialShaderMapUsage::DebugViewModeTexCoordScale;
		break;
	case DVSM_RequiredTextureResolution:
		ShaderMapUsage = EMaterialShaderMapUsage::DebugViewModeRequiredTextureResolution;
		break;
	default:
		return false;
	}

	// Finish compiling pending shaders first.
	if (!bWaitForPreviousShaders)
	{
		FlushRenderingCommands();
	}
	else if (!WaitForShaderCompilation(LOCTEXT("TextureStreamingBuild_FinishPendingShadersCompilation", "Waiting For Pending Shaders Compilation"), ProgressTask))
	{
		return false;
	}

	const double StartTime = FPlatformTime::Seconds();
	const float OneOverNumMaterials = 1.f / (float)Materials.Num();

	if (bFullRebuild)
	{
		FDebugViewModeMaterialProxy::ClearAllShaders();
	}

	TArray<UMaterialInterface*> MaterialsToRemove;
	for (UMaterialInterface* MaterialInterface : Materials)
	{
		check(MaterialInterface); // checked for null in GetTextureStreamingBuildMaterials

		const FMaterial* Material = MaterialInterface->GetMaterialResource(FeatureLevel);
		if (!Material)
		{
			continue;
		}

		bool bSkipShader = false;
		if (Material->GetMaterialDomain() != MD_Surface)
		{
			UE_LOG(TextureStreamingBuild, Verbose, TEXT("Only material domain surface %s is supported, skipping shader"), *MaterialInterface->GetName());
			bSkipShader = true;
		}
		else if (Material->IsUsedWithLandscape())
		{
			UE_LOG(TextureStreamingBuild, Verbose, TEXT("Landscape material %s not supported, skipping shader"), *MaterialInterface->GetName());
			bSkipShader = true;
		}

		if (bSkipShader)
		{
			// Clear the data as it won't be udpated.
			MaterialsToRemove.Add(MaterialInterface);
			MaterialInterface->SetTextureStreamingData(TArray<FMaterialTextureInfo>());
			continue;
		}

		FDebugViewModeMaterialProxy::AddShader(MaterialInterface, QualityLevel, FeatureLevel, !bWaitForPreviousShaders, ShaderMapUsage);
	}

	for (UMaterialInterface* RemovedMaterial : MaterialsToRemove)
	{
		Materials.Remove(RemovedMaterial);
	}

	if (!bWaitForPreviousShaders || WaitForShaderCompilation(LOCTEXT("CompileDebugViewModeShaders", "Compiling Optional Engine Shaders"), ProgressTask))
	{
		// Check The validity of all shaders, removing invalid entries
		FDebugViewModeMaterialProxy::ValidateAllShaders(Materials);

		UE_LOG(TextureStreamingBuild, Display, TEXT("Compiling optional shaders took %.3f seconds."), FPlatformTime::Seconds() - StartTime);
		return true;
	}
	else
	{
		FDebugViewModeMaterialProxy::ClearAllShaders();
		return false;
	}
#else
	return false;
#endif
}

#undef LOCTEXT_NAMESPACE
