// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GlobalShader.cpp: Global shader implementation.
=============================================================================*/

#include "GlobalShader.h"
//
#include "Misc/MessageDialog.h"
#include "HAL/FileManager.h"
#include "Serialization/NameAsStringProxyArchive.h"
#include "Misc/Paths.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/App.h"
#include "StaticBoundShaderState.h"

/** The global shader map. */
TShaderMap<FGlobalShaderType>* GGlobalShaderMap[SP_NumPlatforms] = {};

IMPLEMENT_SHADER_TYPE(,FNULLPS,TEXT("/Engine/Private/NullPixelShader.usf"),TEXT("Main"),SF_Pixel);

/** Used to identify the global shader map in compile queues. */
const int32 GlobalShaderMapId = 0;

FGlobalShaderMapId::FGlobalShaderMapId(EShaderPlatform Platform)
{
	TArray<FShaderType*> ShaderTypes;
	TArray<const FShaderPipelineType*> ShaderPipelineTypes;

	for (TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList()); ShaderTypeIt; ShaderTypeIt.Next())
	{
		FGlobalShaderType* GlobalShaderType = ShaderTypeIt->GetGlobalShaderType();
		if (GlobalShaderType && GlobalShaderType->ShouldCache(Platform))
		{
			ShaderTypes.Add(GlobalShaderType);
		}
	}

	for (TLinkedList<FShaderPipelineType*>::TIterator ShaderPipelineIt(FShaderPipelineType::GetTypeList()); ShaderPipelineIt; ShaderPipelineIt.Next())
	{
		const FShaderPipelineType* Pipeline = *ShaderPipelineIt;
		if (Pipeline->IsGlobalTypePipeline())
		{
			int32 NumStagesNeeded = 0;
			auto& StageTypes = Pipeline->GetStages();
			for (const FShaderType* Shader : StageTypes)
			{
				const FGlobalShaderType* GlobalShaderType = Shader->GetGlobalShaderType();
				if (GlobalShaderType->ShouldCache(Platform))
				{
					++NumStagesNeeded;
				}
				else
				{
					break;
				}
			}

			if (NumStagesNeeded == StageTypes.Num())
			{
				ShaderPipelineTypes.Add(Pipeline);
			}
		}
	}

	// Individual shader dependencies
	ShaderTypes.Sort(FCompareShaderTypes());
	for (int32 TypeIndex = 0; TypeIndex < ShaderTypes.Num(); TypeIndex++)
	{
		FShaderTypeDependency Dependency;
		Dependency.ShaderType = ShaderTypes[TypeIndex];
		Dependency.SourceHash = ShaderTypes[TypeIndex]->GetSourceHash();
		ShaderTypeDependencies.Add(Dependency);
	}

	// Shader pipeline dependencies
	ShaderPipelineTypes.Sort(FCompareShaderPipelineNameTypes());
	for (int32 TypeIndex = 0; TypeIndex < ShaderPipelineTypes.Num(); TypeIndex++)
	{
		const FShaderPipelineType* Pipeline = ShaderPipelineTypes[TypeIndex];
		FShaderPipelineTypeDependency Dependency;
		Dependency.ShaderPipelineType = Pipeline;
		Dependency.StagesSourceHash = Pipeline->GetSourceHash();
		ShaderPipelineTypeDependencies.Add(Dependency);
	}
}

void FGlobalShaderMapId::AppendKeyString(FString& KeyString) const
{
	TMap<const TCHAR*,FCachedUniformBufferDeclaration> ReferencedUniformBuffers;

	for (int32 ShaderIndex = 0; ShaderIndex < ShaderTypeDependencies.Num(); ShaderIndex++)
	{
		const FShaderTypeDependency& ShaderTypeDependency = ShaderTypeDependencies[ShaderIndex];

		KeyString += TEXT("_");
		KeyString += ShaderTypeDependency.ShaderType->GetName();

		// Add the type's source hash so that we can invalidate cached shaders when .usf changes are made
		KeyString += ShaderTypeDependency.SourceHash.ToString();

		// Add the serialization history to the key string so that we can detect changes to global shader serialization without a corresponding .usf change
		ShaderTypeDependency.ShaderType->GetSerializationHistory().AppendKeyString(KeyString);

		const TMap<const TCHAR*,FCachedUniformBufferDeclaration>& ReferencedUniformBufferStructsCache = ShaderTypeDependency.ShaderType->GetReferencedUniformBufferStructsCache();

		// Gather referenced uniform buffers
		for (TMap<const TCHAR*,FCachedUniformBufferDeclaration>::TConstIterator It(ReferencedUniformBufferStructsCache); It; ++It)
		{
			ReferencedUniformBuffers.Add(It.Key(), It.Value());
		}
	}

	for (int32 Index = 0; Index < ShaderPipelineTypeDependencies.Num(); ++Index)
	{
		const FShaderPipelineTypeDependency& Dependency = ShaderPipelineTypeDependencies[Index];

		KeyString += TEXT("_");
		KeyString += Dependency.ShaderPipelineType->GetName();

		// Add the type's source hash so that we can invalidate cached shaders when .usf changes are made
		KeyString += Dependency.StagesSourceHash.ToString();

		for (const FShaderType* ShaderType : Dependency.ShaderPipelineType->GetStages())
		{
			const TMap<const TCHAR*, FCachedUniformBufferDeclaration>& ReferencedUniformBufferStructsCache = ShaderType->GetReferencedUniformBufferStructsCache();

			// Gather referenced uniform buffers
			for (TMap<const TCHAR*, FCachedUniformBufferDeclaration>::TConstIterator It(ReferencedUniformBufferStructsCache); It; ++It)
			{
				ReferencedUniformBuffers.Add(It.Key(), It.Value());
			}
		}
	}

	{
		TArray<uint8> TempData;
		FSerializationHistory SerializationHistory;
		FMemoryWriter Ar(TempData, true);
		FShaderSaveArchive SaveArchive(Ar, SerializationHistory);

		// Save uniform buffer member info so we can detect when layout has changed
		SerializeUniformBufferInfo(SaveArchive, ReferencedUniformBuffers);

		SerializationHistory.AppendKeyString(KeyString);
	}
}

FGlobalShader::FGlobalShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
:	FShader(Initializer)
{
}

void BackupGlobalShaderMap(FGlobalShaderBackupData& OutGlobalShaderBackup)
{
	for (int32 i = (int32)ERHIFeatureLevel::ES2; i < (int32)ERHIFeatureLevel::Num; ++i)
	{
		EShaderPlatform ShaderPlatform = GetFeatureLevelShaderPlatform((ERHIFeatureLevel::Type)i);
		if (ShaderPlatform < EShaderPlatform::SP_NumPlatforms && GGlobalShaderMap[ShaderPlatform] != nullptr)
		{
			TUniquePtr<TArray<uint8>> ShaderData = MakeUnique<TArray<uint8>>();
			FMemoryWriter Ar(*ShaderData);
			GGlobalShaderMap[ShaderPlatform]->SerializeInline(Ar, true, true);
			GGlobalShaderMap[ShaderPlatform]->RegisterSerializedShaders();
			GGlobalShaderMap[ShaderPlatform]->Empty();
			OutGlobalShaderBackup.FeatureLevelShaderData[i] = MoveTemp(ShaderData);
		}
	}

	// Remove cached references to global shaders
	for (TLinkedList<FGlobalBoundShaderStateResource*>::TIterator It(FGlobalBoundShaderStateResource::GetGlobalBoundShaderStateList()); It; It.Next())
	{
		BeginUpdateResourceRHI(*It);
	}
}

void RestoreGlobalShaderMap(const FGlobalShaderBackupData& GlobalShaderBackup)
{
	for (int32 i = (int32)ERHIFeatureLevel::ES2; i < (int32)ERHIFeatureLevel::Num; ++i)
	{
		EShaderPlatform ShaderPlatform = GetFeatureLevelShaderPlatform((ERHIFeatureLevel::Type)i);		
		if (GlobalShaderBackup.FeatureLevelShaderData[i] != nullptr
			&& ShaderPlatform < EShaderPlatform::SP_NumPlatforms
			&& GGlobalShaderMap[ShaderPlatform] != nullptr)
		{
			FMemoryReader Ar(*GlobalShaderBackup.FeatureLevelShaderData[i]);
			GGlobalShaderMap[ShaderPlatform]->SerializeInline(Ar, true, true);
			GGlobalShaderMap[ShaderPlatform]->RegisterSerializedShaders();
		}
	}
}


TShaderMap<FGlobalShaderType>* GetGlobalShaderMap(EShaderPlatform Platform)
{
	// If the global shader map hasn't been created yet
	check(GGlobalShaderMap[Platform]);

	return GGlobalShaderMap[Platform];
}
