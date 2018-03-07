// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ShaderCacheTypes.cpp: Implementation for Shader Cache specific types.
=============================================================================*/

#include "ShaderCacheTypes.h"
#include "ShaderCache.h"
#include "Serialization/MemoryWriter.h"

uint32 FShaderDrawKey::CurrentMaxResources = EShaderCacheMaxNumResources;

FArchive& operator<<( FArchive& Ar, FShaderPlatformCache& Info )
{
	uint32 CacheVersion = Ar.IsLoading() ? ((uint32)~0u) : ((uint32)FShaderCacheCustomVersion::Latest);
	uint32 GameVersion = Ar.IsLoading() ? ((uint32)~0u) : ((uint32)FShaderCache::GetGameVersion());

	Ar << CacheVersion;
	if (!Ar.IsError() && CacheVersion == FShaderCacheCustomVersion::Latest)
	{
		Ar << GameVersion;
		if (!Ar.IsError() && GameVersion == FShaderCache::GetGameVersion())
		{
			uint8 ShaderPlatform = (uint8)Info.ShaderPlatform;
			Ar << ShaderPlatform;
			Info.ShaderPlatform = (EShaderPlatform)ShaderPlatform;
			
			Ar << Info.Shaders; 
			Ar << Info.BoundShaderStates; 
			Ar << Info.DrawStates; 
			Ar << Info.RenderTargets; 
			Ar << Info.Resources; 
			Ar << Info.SamplerStates; 
			Ar << Info.PreDrawEntries; 
			Ar << Info.ShaderStateMembership; 
			Ar << Info.StreamingDrawStates;
			Ar << Info.PipelineStates;
		}
	}
	return Ar;
}

FArchive& operator<<( FArchive& Ar, FShaderCodeCache& Info )
{
	uint32 CacheVersion = Ar.IsLoading() ? (uint32)~0u : ((uint32)FShaderCacheCustomVersion::Latest);
	uint32 GameVersion = Ar.IsLoading() ? (uint32)~0u : ((uint32)FShaderCache::GetGameVersion());
	
	Ar << CacheVersion;
	if ( !Ar.IsError() && CacheVersion == FShaderCacheCustomVersion::Latest )
	{
		Ar << GameVersion;
		
		if ( !Ar.IsError() && GameVersion == FShaderCache::GetGameVersion() )
		{
			Ar << Info.Shaders;
			Ar << Info.Pipelines;
		}
	}
	return Ar;
}

static FORCEINLINE uint32 CalculateSizeOfSamplerStateInitializer()
{
	static uint32 SizeOfSamplerStateInitializer = 0;
	if (SizeOfSamplerStateInitializer == 0)
	{
		TArray<uint8> Data;
		FMemoryWriter Writer(Data);
		FSamplerStateInitializerRHI State;
		Writer << State;
		SizeOfSamplerStateInitializer = Data.Num();
	}
	return SizeOfSamplerStateInitializer;
}

bool FSamplerStateInitializerRHIKeyFuncs::Matches(KeyInitType A,KeyInitType B)
{
	return FMemory::Memcmp(&A, &B, CalculateSizeOfSamplerStateInitializer()) == 0;
}

uint32 FSamplerStateInitializerRHIKeyFuncs::GetKeyHash(KeyInitType Key)
{
	return FCrc::MemCrc_DEPRECATED(&Key, CalculateSizeOfSamplerStateInitializer());
}
