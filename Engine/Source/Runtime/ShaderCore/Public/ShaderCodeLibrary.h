// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ShaderCodeLibrary.h: 
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"

DECLARE_LOG_CATEGORY_EXTERN(LogShaderLibrary, Log, All);

class FShaderPipeline;

struct SHADERCORE_API FShaderCodeLibraryPipeline
{
	FSHAHash VertexShader;
	FSHAHash PixelShader;
	FSHAHash GeometryShader;
	FSHAHash HullShader;
	FSHAHash DomainShader;
	mutable uint32 Hash;
	
	FShaderCodeLibraryPipeline() : Hash(0) {}
	
	friend bool operator ==(const FShaderCodeLibraryPipeline& A,const FShaderCodeLibraryPipeline& B)
	{
		return A.VertexShader == B.VertexShader && A.PixelShader == B.PixelShader && A.GeometryShader == B.GeometryShader && A.HullShader == B.HullShader && A.DomainShader == B.DomainShader;
	}
	
	friend uint32 GetTypeHash(const FShaderCodeLibraryPipeline &Key)
	{
		if(!Key.Hash)
		{
			Key.Hash = FCrc::MemCrc32(Key.VertexShader.Hash, sizeof(Key.VertexShader.Hash));
			Key.Hash = FCrc::MemCrc32(Key.PixelShader.Hash, sizeof(Key.PixelShader.Hash), Key.Hash);
			Key.Hash = FCrc::MemCrc32(Key.GeometryShader.Hash, sizeof(Key.GeometryShader.Hash), Key.Hash);
			Key.Hash = FCrc::MemCrc32(Key.HullShader.Hash, sizeof(Key.HullShader.Hash), Key.Hash);
			Key.Hash = FCrc::MemCrc32(Key.DomainShader.Hash, sizeof(Key.DomainShader.Hash), Key.Hash);
		}
		return Key.Hash;
	}
	
	friend FArchive& operator<<( FArchive& Ar, FShaderCodeLibraryPipeline& Info )
	{
		return Ar << Info.VertexShader << Info.PixelShader << Info.GeometryShader << Info.HullShader << Info.DomainShader << Info.Hash;
	}
};

class FShaderFactoryInterface : public FRHIShaderLibrary
{
public:
	FShaderFactoryInterface(EShaderPlatform InPlatform) : FRHIShaderLibrary(InPlatform) {}
	
	virtual bool IsNativeLibrary() const override final {return false;}
	
	virtual FPixelShaderRHIRef CreatePixelShader(const FSHAHash& Hash) = 0; 
	virtual FVertexShaderRHIRef CreateVertexShader(const FSHAHash& Hash) = 0;
	virtual FHullShaderRHIRef CreateHullShader(const FSHAHash& Hash) = 0;
	virtual FDomainShaderRHIRef CreateDomainShader(const FSHAHash& Hash) = 0;
	virtual FGeometryShaderRHIRef CreateGeometryShader(const FSHAHash& Hash) = 0;
	virtual FGeometryShaderRHIRef CreateGeometryShaderWithStreamOutput(const FSHAHash& Hash, const FStreamOutElementList& ElementList, uint32 NumStrides, const uint32* Strides, int32 RasterizedStream) = 0;
	virtual FComputeShaderRHIRef CreateComputeShader(const FSHAHash& Hash) = 0;
};

// Collection of unique shader code
// Populated at cook time
struct SHADERCORE_API FShaderCodeLibrary
{
	static void InitForRuntime(EShaderPlatform ShaderPlatform);
	static void InitForCooking(bool bNativeFormat);
	static void Shutdown();
	
	// At cook time, add shader code to collection
	static bool AddShaderCode(EShaderPlatform ShaderPlatform, EShaderFrequency Frequency, const FSHAHash& Hash, const TArray<uint8>& InCode, uint32 const UncompressedSize);
	
	// At cook time, add shader pipeline to collection
	static bool AddShaderPipeline(FShaderPipeline* Pipeline);
	
	/** Instantiate or retrieve a vertex shader from the cache for the provided code & hash. */
	static FVertexShaderRHIRef CreateVertexShader(EShaderPlatform Platform, FSHAHash Hash, TArray<uint8> const& Code);
	/** Instantiate or retrieve a pixel shader from the cache for the provided code & hash. */
	static FPixelShaderRHIRef CreatePixelShader(EShaderPlatform Platform, FSHAHash Hash, TArray<uint8> const& Code);
	/** Instantiate or retrieve a geometry shader from the cache for the provided code & hash. */
	static FGeometryShaderRHIRef CreateGeometryShader(EShaderPlatform Platform, FSHAHash Hash, TArray<uint8> const& Code);
	/** Instantiate or retrieve a geometry shader from the cache for the provided code & hash. */
	static FGeometryShaderRHIRef CreateGeometryShaderWithStreamOutput(EShaderPlatform Platform, FSHAHash Hash, const TArray<uint8>& Code, const FStreamOutElementList& ElementList, uint32 NumStrides, const uint32* Strides, int32 RasterizedStream);
	/** Instantiate or retrieve a hull shader from the cache for the provided code & hash. */
	static FHullShaderRHIRef CreateHullShader(EShaderPlatform Platform, FSHAHash Hash, TArray<uint8> const& Code);
	/** Instantiate or retrieve a domain shader from the cache for the provided code & hash. */
	static FDomainShaderRHIRef CreateDomainShader(EShaderPlatform Platform, FSHAHash Hash, TArray<uint8> const& Code);
	/** Instantiate or retrieve a compute shader from the cache for the provided code & hash. */
	static FComputeShaderRHIRef CreateComputeShader(EShaderPlatform Platform, FSHAHash Hash, TArray<uint8> const& Code);

	// Place a request to preload shader code
	// Blocking call if no Archive is provided or Archive is not a type of FLinkerLoad
	// Shader code preload will be finished before owning UObject PostLoad call
	static bool RequestShaderCode(const FSHAHash& Hash, FArchive* Ar);

	// Request to release shader code
	// Must match RequestShaderCode call
	// Invalid to call before owning UObject PostLoad call
	static void ReleaseShaderCode(const FSHAHash& Hash);
	
	// Create an iterator over all the shaders in the library
	static TRefCountPtr<FRHIShaderLibrary::FShaderLibraryIterator> CreateIterator(void);
	
	// Total number of shader entries in the library
	static uint32 GetShaderCount(void);
	
	// The shader platform that the library manages - at runtime this will only be one
	static EShaderPlatform GetRuntimeShaderPlatform(void);
	
	// Get the shader pipelines in the library - only ever valid for OpenGL which can link without full PSO state
	static TSet<FShaderCodeLibraryPipeline> const* GetShaderPipelines(EShaderPlatform Platform);

#if WITH_EDITOR
	// Save collected shader code to a file for each specified shader platform
	static bool SaveShaderCode(const FString& OutputDir, const FString& DebugOutputDir, const TArray<FName>& ShaderFormats);
	
	// Package the separate shader bytecode files into a single native shader library.
	static bool PackageNativeShaderLibrary(const FString& ShaderCodeDir, const FString& DebugShaderCodeDir, const TArray<FName>& ShaderFormats);
	
	// Dump collected stats for each shader platform
	static void DumpShaderCodeStats();
#endif
	
	// Safely assign the hash to a shader object
	static void SafeAssignHash(FRHIShader* InShader, const FSHAHash& Hash);
};
