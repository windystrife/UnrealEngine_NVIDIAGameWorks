// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RendererSupport.cpp: Central place for various rendering functionality that exists in Engine
=============================================================================*/

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FeedbackContext.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectHash.h"
#include "EngineGlobals.h"
#include "RenderingThread.h"
#include "Containers/List.h"
#include "Shader.h"
#include "VertexFactory.h"
#include "SceneTypes.h"
#include "Templates/ScopedPointer.h"
#include "Materials/MaterialInterface.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "UObject/UObjectIterator.h"
#include "ComponentReregisterContext.h"
#include "UnrealEngine.h"
#include "Framework/Application/SlateApplication.h"
#include "EditorSupportDelegates.h"
#include "RendererInterface.h"
#include "FXSystem.h"
#include "GlobalShader.h"
#include "EngineModule.h"
#include "Misc/HotReloadInterface.h"
#include "ComponentReregisterContext.h"
#include "UniquePtr.h"
#include "ShaderCompiler.h"
#include "SceneInterface.h"

/** Clears and optionally backs up all references to renderer module classes in other modules, particularly engine. */
static void ClearReferencesToRendererModuleClasses(
	TMap<UWorld*, bool>& WorldsToUpdate, 
	TMap<FMaterialShaderMap*, TUniquePtr<TArray<uint8> > >& ShaderMapToSerializedShaderData,
	FGlobalShaderBackupData& GlobalShaderBackup,
	TMap<FShaderType*, FString>& ShaderTypeNames,
	TMap<const FShaderPipelineType*, FString>& ShaderPipelineTypeNames,
	TMap<FVertexFactoryType*, FString>& VertexFactoryTypeNames)
{
	// Destroy all renderer scenes
	for (TObjectIterator<UWorld> WorldIt; WorldIt; ++WorldIt)
	{
		UWorld* World = *WorldIt;

		if (World->Scene)
		{
			WorldsToUpdate.Add(World, World->FXSystem != NULL);

			for (int32 LevelIndex = 0; LevelIndex < World->GetNumLevels(); LevelIndex++)
			{
				ULevel* Level = World->GetLevel(LevelIndex);
				Level->ReleaseRenderingResources();
			}

			if (World->FXSystem != NULL)
			{
				FFXSystemInterface::Destroy(World->FXSystem);
				World->FXSystem = NULL;
			}

			World->Scene->Release();
			World->Scene = NULL;
		}
	}
		
	// For each feature level save off its shaders by serializing them into memory, and remove all shader map references to FShaders
	BackupGlobalShaderMap(GlobalShaderBackup);
	UMaterial::BackupMaterialShadersToMemory(ShaderMapToSerializedShaderData);

	// Verify no FShaders still in memory
	for (TLinkedList<FShaderType*>::TIterator It(FShaderType::GetTypeList()); It; It.Next())
	{
		FShaderType* ShaderType = *It;
		check(ShaderType->GetNumShaders() == 0);
		ShaderTypeNames.Add(ShaderType, ShaderType->GetName());
	}

	for (TLinkedList<FShaderPipelineType*>::TIterator It(FShaderPipelineType::GetTypeList()); It; It.Next())
	{
		const FShaderPipelineType* ShaderPipelineType = *It;
		ShaderPipelineTypeNames.Add(ShaderPipelineType, ShaderPipelineType->GetName());
	}

	for(TLinkedList<FVertexFactoryType*>::TIterator It(FVertexFactoryType::GetTypeList()); It; It.Next())
	{
		FVertexFactoryType* VertexFactoryType = *It;
		VertexFactoryTypeNames.Add(VertexFactoryType, VertexFactoryType->GetName());
	}

	// Destroy misc renderer module classes and remove references
	FSceneViewStateReference::DestroyAll();
	FSlateApplication::Get().InvalidateAllViewports();

	// Invalidate cached shader type data
	UninitializeShaderTypes();

	// Delete pending cleanup objects to remove those references, which are potentially renderer module classes
	FPendingCleanupObjects* PendingCleanupObjects = GetPendingCleanupObjects();
	delete PendingCleanupObjects;
	GEngine->EngineLoop->ClearPendingCleanupObjects();

	ResetCachedRendererModule();
}

/** Recompiles the renderer module, retrying until successful. */
void RecompileRendererModule()
{
	IHotReloadInterface* HotReload = IHotReloadInterface::GetPtr();
	if(HotReload != nullptr)
	{
		const FName RendererModuleName = TEXT("Renderer");
		// Unload first so that RecompileModule will not using a rolling module name
		verify(FModuleManager::Get().UnloadModule(RendererModuleName));

		bool bCompiledSuccessfully = false;
		do 
		{
			const bool bForceCodeProject = false;
			const bool bFailIfGeneratedCodeChanges = true;
			bCompiledSuccessfully = HotReload->RecompileModule(RendererModuleName, false, *GLog, bFailIfGeneratedCodeChanges, bForceCodeProject);

			if (!bCompiledSuccessfully)
			{
				// Pop up a blocking dialog if there were compilation errors
				// Compiler output will be in the log
				FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *FText::Format(
					NSLOCTEXT("UnrealEd", "Error_RetryCompilation", "C++ compilation of module {0} failed!  Details in the log.  \r\nFix the error then click Ok to retry."),
					FText::FromName(RendererModuleName)).ToString(), TEXT("Error"));
			}
		} 
		while (!bCompiledSuccessfully);

		verify(FModuleManager::Get().LoadModule(RendererModuleName, true) != nullptr);
	}
}

/** Restores systems that need references to classes in the renderer module. */
static void RestoreReferencesToRendererModuleClasses(
	const TMap<UWorld*, bool>& WorldsToUpdate, 
	const TMap<FMaterialShaderMap*, TUniquePtr<TArray<uint8> > >& ShaderMapToSerializedShaderData,
	const FGlobalShaderBackupData& GlobalShaderBackup,
	const TMap<FShaderType*, FString>& ShaderTypeNames,
	const TMap<const FShaderPipelineType*, FString>& ShaderPipelineTypeNames,
	const TMap<FVertexFactoryType*, FString>& VertexFactoryTypeNames)
{
	FlushShaderFileCache();

	// Initialize cached shader type data
	InitializeShaderTypes();

	IRendererModule& RendererModule = GetRendererModule();

	FSceneViewStateReference::AllocateAll();

	// Recreate all renderer scenes
	for (TMap<UWorld*, bool>::TConstIterator It(WorldsToUpdate); It; ++It)
	{
		UWorld* World = It.Key();

		RendererModule.AllocateScene(World, World->RequiresHitProxies(), It.Value(), World->FeatureLevel);

		for (int32 LevelIndex = 0; LevelIndex < World->GetNumLevels(); LevelIndex++)
		{
			ULevel* Level = World->GetLevel(LevelIndex);
			Level->InitializeRenderingResources();
		}
	}

	// Restore FShaders from the serialized memory blobs
	// Shader maps may still not be complete after this due to code changes picked up in the recompile
	RestoreGlobalShaderMap(GlobalShaderBackup);
	UMaterial::RestoreMaterialShadersFromMemory(ShaderMapToSerializedShaderData);

	for (int32 i = (int32)ERHIFeatureLevel::ES2; i < (int32)ERHIFeatureLevel::Num; ++i)
	{
		if (GlobalShaderBackup.FeatureLevelShaderData[i] != nullptr)
		{
			EShaderPlatform ShaderPlatform = GetFeatureLevelShaderPlatform((ERHIFeatureLevel::Type)i);
			check(ShaderPlatform < EShaderPlatform::SP_NumPlatforms);
			FMaterialShaderMap::FixupShaderTypes(ShaderPlatform, ShaderTypeNames, ShaderPipelineTypeNames, VertexFactoryTypeNames);
		}
	}

	TArray<FShaderType*> OutdatedShaderTypes;
	TArray<const FVertexFactoryType*> OutdatedFactoryTypes;
	TArray<const FShaderPipelineType*> OutdatedShaderPipelineTypes;
	FShaderType::GetOutdatedTypes(OutdatedShaderTypes, OutdatedFactoryTypes);

	// Recompile any missing shaders
	UMaterialInterface::IterateOverActiveFeatureLevels([&](ERHIFeatureLevel::Type FeatureLevel) 
	{
		auto ShaderPlatform = GetFeatureLevelShaderPlatform(FeatureLevel);
		check(ShaderPlatform < EShaderPlatform::SP_NumPlatforms);
		BeginRecompileGlobalShaders(OutdatedShaderTypes, OutdatedShaderPipelineTypes, ShaderPlatform);
		UMaterial::UpdateMaterialShaders(OutdatedShaderTypes, OutdatedShaderPipelineTypes, OutdatedFactoryTypes, ShaderPlatform);
	});

	// Block on global shader jobs
	FinishRecompileGlobalShaders();
}

/** 
 * Handles recompiling the renderer module, including removing all references, recompiling the dll and restoring references.
 */
void RecompileRenderer(const TArray<FString>& Args)
{
	// So that we can see the slow task dialog
	FSlateApplication::Get().DismissAllMenus();

	GWarn->BeginSlowTask( NSLOCTEXT("Renderer", "BeginRecompileRendererTask", "Recompiling Rendering Module..."), true);

	const double StartTime = FPlatformTime::Seconds();
	double EndShutdownTime;
	double EndRecompileTime;

	{
		// Deregister all components from their renderer scenes
		FGlobalComponentReregisterContext ReregisterContext;
		// Shut down the rendering thread so that the game thread will process all rendering commands during this scope
		SCOPED_SUSPEND_RENDERING_THREAD(true);

		TMap<UWorld*, bool> WorldsToUpdate;
		TMap<FMaterialShaderMap*, TUniquePtr<TArray<uint8> > > ShaderMapToSerializedShaderData;
		FGlobalShaderBackupData GlobalShaderBackup;
		TMap<FShaderType*, FString> ShaderTypeNames;
		TMap<const FShaderPipelineType*, FString> ShaderPipelineTypeNames;
		TMap<FVertexFactoryType*, FString> VertexFactoryTypeNames;

		ClearReferencesToRendererModuleClasses(WorldsToUpdate, ShaderMapToSerializedShaderData, GlobalShaderBackup, ShaderTypeNames, ShaderPipelineTypeNames, VertexFactoryTypeNames);

		EndShutdownTime = FPlatformTime::Seconds();
		UE_LOG(LogShaders, Warning, TEXT("Shutdown complete %.1fs"),(float)(EndShutdownTime - StartTime));

		RecompileRendererModule();

		EndRecompileTime = FPlatformTime::Seconds();
		UE_LOG(LogShaders, Warning, TEXT("Recompile complete %.1fs"),(float)(EndRecompileTime - EndShutdownTime));

		RestoreReferencesToRendererModuleClasses(WorldsToUpdate, ShaderMapToSerializedShaderData, GlobalShaderBackup, ShaderTypeNames, ShaderPipelineTypeNames, VertexFactoryTypeNames);
	}

#if WITH_EDITOR
	// Refresh viewports
	FEditorSupportDelegates::RedrawAllViewports.Broadcast();
#endif

	const double EndTime = FPlatformTime::Seconds();
	UE_LOG(LogShaders, Warning, 
		TEXT("Recompile of Renderer module complete \n")
		TEXT("                                     Total = %.1fs, Shutdown = %.1fs, Recompile = %.1fs, Reload = %.1fs"),
		(float)(EndTime - StartTime),
		(float)(EndShutdownTime - StartTime),
		(float)(EndRecompileTime - EndShutdownTime),
		(float)(EndTime - EndRecompileTime));
	
	GWarn->EndSlowTask();
}

FAutoConsoleCommand RecompileRendererCommand(
	TEXT("r.RecompileRenderer"),
	TEXT("Recompiles the renderer module on the fly."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&RecompileRenderer)
	);
