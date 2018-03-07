// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ShaderCache.cpp: Bound shader state cache implementation.
 =============================================================================*/

#include "ShaderCache.h"
#include "RenderingThread.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ArchiveSaveCompressedProxy.h"
#include "Serialization/ArchiveLoadCompressedProxy.h"
#include "Serialization/CustomVersion.h"
#include "Shader.h"
#include "Misc/EngineVersion.h"
#include "PipelineStateCache.h"
#include "ScopeRWLock.h"
#include "CoreDelegates.h"

DECLARE_STATS_GROUP(TEXT("Shader Cache"),STATGROUP_ShaderCache, STATCAT_Advanced);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Num Shaders Cached"),STATGROUP_NumShadersCached,STATGROUP_ShaderCache);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Num BSS Cached"),STATGROUP_NumBSSCached,STATGROUP_ShaderCache);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Num New Draw-States Cached"),STATGROUP_NumDrawsCached,STATGROUP_ShaderCache);
DECLARE_DWORD_COUNTER_STAT(TEXT("Shaders Precompiled"),STATGROUP_NumPrecompiled,STATGROUP_ShaderCache);
DECLARE_DWORD_COUNTER_STAT(TEXT("Shaders Predrawn"),STATGROUP_NumPredrawn,STATGROUP_ShaderCache);
DECLARE_DWORD_COUNTER_STAT(TEXT("Draw States Predrawn"),STATGROUP_NumStatesPredrawn,STATGROUP_ShaderCache);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Total Shaders Precompiled"),STATGROUP_TotalPrecompiled,STATGROUP_ShaderCache);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Total Shaders Predrawn"),STATGROUP_TotalPredrawn,STATGROUP_ShaderCache);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Total Draw States Predrawn"),STATGROUP_TotalStatesPredrawn,STATGROUP_ShaderCache);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num To Precompile Per Frame"),STATGROUP_NumToPrecompile,STATGROUP_ShaderCache);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("Binary Cache Load Time (s)"),STATGROUP_BinaryCacheLoadTime,STATGROUP_ShaderCache);

const FGuid FShaderCacheCustomVersion::Key(0xB954F018, 0xC9624DD6, 0xA74E79B1, 0x8EA113C2);
const FGuid FShaderCacheCustomVersion::GameKey(0x03D4EB48, 0xB50B4CC3, 0xA598DE41, 0x5C6CC993);
FCustomVersionRegistration GRegisterShaderCacheVersion(FShaderCacheCustomVersion::Key, FShaderCacheCustomVersion::Latest, TEXT("ShaderCacheVersion"));
FCustomVersionRegistration GRegisterShaderCacheGameVersion(FShaderCacheCustomVersion::GameKey, 0, TEXT("ShaderCacheGameVersion"));

#define SHADER_CACHE_ENABLED (!WITH_EDITOR && PLATFORM_MAC)

static const ECompressionFlags ShaderCacheCompressionFlag = ECompressionFlags::COMPRESS_ZLIB;

// Only the Mac build defaults to using the shader cache for now, Editor uses a separate cache from the game to avoid ever-growing cache being propagated to the game.
int32 FShaderCache::bUseShaderCaching = SHADER_CACHE_ENABLED;
FAutoConsoleVariableRef FShaderCache::CVarUseShaderCaching(
														   TEXT("r.UseShaderCaching"),
														   bUseShaderCaching,
														   TEXT("If true, log all shaders & bound-shader-states, so that they may be instantiated in the RHI on deserialisation rather than waiting for first use."),
														   ECVF_ReadOnly|ECVF_RenderThreadSafe
														   );

int32 FShaderCache::bUseUserShaderCache = 1;
FAutoConsoleVariableRef FShaderCache::CVarUseUserShaderCache(
	TEXT("r.UseUserShaderCache"),
	bUseUserShaderCache,
	TEXT("If true, shader caching will use (and store) draw-log from a user directory, otherwise only draw-log stored in game content directory"),
	ECVF_RenderThreadSafe
	);

// Predrawing takes an existing shader cache with draw log & renders each shader + draw-state combination before use to avoid in-driver recompilation
// This requires plenty of setup & is done in batches at frame-end.
int32 FShaderCache::bUseShaderPredraw = SHADER_CACHE_ENABLED;
FAutoConsoleVariableRef FShaderCache::CVarUseShaderPredraw(
														   TEXT("r.UseShaderPredraw"),
														   bUseShaderPredraw,
														   TEXT("Use an existing draw-log to predraw shaders in batches before being used to reduce hitches due to in-driver recompilation."),
														   ECVF_ReadOnly|ECVF_RenderThreadSafe
														   );

// The actual draw loggging is even more expensive as it has to cache all the RHI draw state & is disabled by default.
int32 FShaderCache::bUseShaderDrawLog = SHADER_CACHE_ENABLED;
FAutoConsoleVariableRef FShaderCache::CVarUseShaderDrawLog(
														   TEXT("r.UseShaderDrawLog"),
														   bUseShaderDrawLog,
														   TEXT("If true, log all the draw states used for each shader pipeline, so that they may be pre-drawn in batches (see: r.UseShaderPredraw). This can be expensive & should be used only when generating the shader cache."),
														   ECVF_ReadOnly|ECVF_RenderThreadSafe
														   );

// As predrawing can take significant time then batch the draws up into chunks defined by frame time
int32 FShaderCache::PredrawBatchTime = -1;
FAutoConsoleVariableRef FShaderCache::CVarPredrawBatchTime(
														   TEXT("r.PredrawBatchTime"),
														   PredrawBatchTime,
														   TEXT("Time in ms to spend predrawing shaders each frame, or -1 to perform all predraws immediately."),
														   ECVF_RenderThreadSafe
														   );

// A separate cache of used shader binaries for even earlier submission - may be platform or even device specific.
int32 FShaderCache::bUseShaderBinaryCache = 0;
FAutoConsoleVariableRef FShaderCache::CVarUseShaderBinaryCache(
															   TEXT("r.UseShaderBinaryCache"),
															   bUseShaderBinaryCache,
															   TEXT("If true generates & uses a separate cache of used shader binaries for even earlier submission - may be platform or even device specific. Defaults to false."),
															   ECVF_ReadOnly|ECVF_RenderThreadSafe
															   );

// Whether to try and perform shader precompilation asynchronously.
int32 FShaderCache::bUseAsyncShaderPrecompilation = 0;
FAutoConsoleVariableRef FShaderCache::CVarUseAsyncShaderPrecompilation(
																	   TEXT("r.UseAsyncShaderPrecompilation"),
																	   bUseAsyncShaderPrecompilation,
																	   TEXT("If true tries to perform inital shader precompilation asynchronously on a background thread. Defaults to false."),
																	   ECVF_ReadOnly|ECVF_RenderThreadSafe
																	   );

// As async precompile can take significant time specify a desired max. frame time that the cache will try to remain below while precompiling. We can't specify the time to spend directly as under GL compile operations are deferred and take no time on the user thread.
int32 FShaderCache::TargetPrecompileFrameTime = -1;
FAutoConsoleVariableRef FShaderCache::CVarTargetPrecompileFrameTime(
																	TEXT("r.TargetPrecompileFrameTime"),
																	TargetPrecompileFrameTime,
																	TEXT("Upper limit in ms for total frame time while precompiling, allowing the shader cache to adjust how many shaders to precompile each frame. Defaults to -1 which will precompile all shaders immediately."),
																	ECVF_RenderThreadSafe
																	);

int32 FShaderCache::AccelPredrawBatchTime = 0;
FAutoConsoleVariableRef FShaderCache::CVarAccelPredrawBatchTime(
																TEXT("r.AccelPredrawBatchTime"),
																AccelPredrawBatchTime,
																TEXT("Override value for r.PredrawBatchTime when showing a loading-screen or similar to do more work while the player won't notice, or 0 to use r.PredrawBatchTime. Defaults to 0."),
																ECVF_RenderThreadSafe
																);

int32 FShaderCache::AccelTargetPrecompileFrameTime = 0;
FAutoConsoleVariableRef FShaderCache::CVarAccelTargetPrecompileFrameTime(
																		 TEXT("r.AccelTargetPrecompileFrameTime"),
																		 AccelTargetPrecompileFrameTime,
																		 TEXT("Override value for r.TargetPrecompileFrameTime when showing a loading-screen or similar to do more work while the player won't notice, or 0 to use r.TargetPrecompileFrameTime. Defaults to 0."),
																		 ECVF_RenderThreadSafe
																		 );

float FShaderCache::InitialShaderLoadTime = -1.f;
FAutoConsoleVariableRef FShaderCache::CVarInitialShaderLoadTime(
																TEXT("r.InitialShaderLoadTime"),
																InitialShaderLoadTime,
																TEXT("Time to spend loading the shader cache synchronously on startup before falling back to asynchronous precompilation/predraw. Defaults to -1 which will perform all work synchronously."),
																ECVF_RenderThreadSafe
																);

int32 GShaderCacheBinaryCacheLogging = 0;
FAutoConsoleVariableRef GCVarShaderCacheBinaryCacheLogging(
														   TEXT("r.BinaryShaderCacheLogging"),
														   GShaderCacheBinaryCacheLogging,
														   TEXT("Log duplicate shader code entries within a project and report on shader code details when generating the binary shader cache. Defaults to 0."),
														   ECVF_RenderThreadSafe
														   );

uint8 FShaderCache::MaxResources = EShaderCacheMaxNumResources;

//
//FShaderCache Library
//

static void FShaderCacheHelperUncompressCode(uint32 UncompressedSize, const TArray<uint8>& Code, TArray<uint8>& UncompressedCode)
{
	UncompressedCode.SetNum(UncompressedSize);
	bool bSucceed = FCompression::UncompressMemory(ShaderCacheCompressionFlag, UncompressedCode.GetData(), UncompressedSize, Code.GetData(), Code.Num());
	check(bSucceed);
}

#if WITH_EDITOR
static TCHAR const* GShaderCacheFileName = TEXT("EditorDrawCache");
static TCHAR const* GShaderCodeCacheFileName = TEXT("EditorCodeCache.ushadercode"); // this gets saved into platform specific dir
#else
static TCHAR const* GShaderCacheFileName = TEXT("DrawCache");
static TCHAR const* GShaderCodeCacheFileName = TEXT("ByteCodeCache.ushadercode");  // this gets saved into platform specific dir
#endif

FString GetShaderCacheFilename(const FString& BaseDir, EShaderPlatform InPlatform)
{
	FName PlatformName = LegacyShaderPlatformToShaderFormat(InPlatform);
	FString Filename = BaseDir / GShaderCacheFileName + TEXT("-") + PlatformName.ToString() + TEXT(".ushadercache");
	return Filename;
}

//
//FShaderCache Library
//

class SHADERCORE_API FShaderCacheLibrary final : public FShaderFactoryInterface
{
	friend FShaderCache;
public:
	FShaderCacheLibrary(EShaderPlatform Platform, FString Name);
	virtual ~FShaderCacheLibrary();
	
	bool Load(FString Path);
	
	FPixelShaderRHIRef CreatePixelShader(const FSHAHash& Hash) override final;
	
	FVertexShaderRHIRef CreateVertexShader(const FSHAHash& Hash)  override final;
	
	FHullShaderRHIRef CreateHullShader(const FSHAHash& Hash) override final;
	
	FDomainShaderRHIRef CreateDomainShader(const FSHAHash& Hash) override final;
	
	FGeometryShaderRHIRef CreateGeometryShader(const FSHAHash& Hash) override final;
	
	FGeometryShaderRHIRef CreateGeometryShaderWithStreamOutput(const FSHAHash& Hash, const FStreamOutElementList& ElementList, uint32 NumStrides, const uint32* Strides, int32 RasterizedStream) override final;
	
	FComputeShaderRHIRef CreateComputeShader(const FSHAHash& Hash) override final;
	
	FName GetFormat( void ) const;
	
	//Archive override add Shader
	bool AddShader( uint8 Frequency, const FSHAHash& Hash, TArray<uint8> const& Code, uint32 const UncompressedSize );
	
	bool Finalize( FString OutputDir, TArray<FString>* OutputFiles );
	
	friend FArchive& operator<<( FArchive& Ar, FShaderCacheLibrary& Info );
	
	class FShaderCacheLibraryIterator : public FRHIShaderLibrary::FShaderLibraryIterator
	{
	public:
		FShaderCacheLibraryIterator(FShaderCacheLibrary* Library, TMap<FShaderCacheKey, TPair<uint32,TArray<uint8>>>::TIterator It)
		: FRHIShaderLibrary::FShaderLibraryIterator(Library)
		, IteratorImpl(It) {}
		
		virtual bool IsValid() const final override
		{
			return !!IteratorImpl;
		}
		virtual FRHIShaderLibrary::FShaderLibraryEntry operator*() const final override
		{
			FShaderLibraryEntry Entry;
			
			const auto& Key = IteratorImpl.Key();
			const auto& Value = IteratorImpl.Value();
			
			Entry.Hash = Key.SHAHash;
			Entry.Frequency = Key.Frequency;
			Entry.Platform = GetLibrary()->GetPlatform();
			
			return Entry;
		}
		virtual FRHIShaderLibrary::FShaderLibraryIterator& operator++() final override
		{
			++IteratorImpl;
			return *this;
		}
		
	private:
		TMap<FShaderCacheKey, TPair<uint32,TArray<uint8>>>::TIterator IteratorImpl;
	};
	
	virtual TRefCountPtr<FRHIShaderLibrary::FShaderLibraryIterator> CreateIterator(void) override final
	{
		return new FShaderCacheLibraryIterator(this, Shaders.CreateIterator());
	}
	
	virtual uint32 GetShaderCount(void) const override final
	{
		return Shaders.Num();
	}
	
private:
	// Serialised
	TMap<FShaderCacheKey, TPair<uint32,TArray<uint8>>> Shaders;
	TMap<FShaderCacheKey, TSet<FShaderPipelineKey>> Pipelines;
	
	// Non-serialised
#if WITH_EDITORONLY_DATA
	TMap<FShaderCacheKey, TArray<TPair<int32, TArray<uint8>>>> Counts;
#endif
	
	FString FileName;
};

FShaderCacheLibrary::FShaderCacheLibrary(EShaderPlatform InPlatform, FString Name)
: FShaderFactoryInterface(InPlatform)
, FileName(Name)
{
}

FShaderCacheLibrary::~FShaderCacheLibrary()
{
}

bool FShaderCacheLibrary::Load(FString Path)
{
	bool bLoadedCache = false;
	
	FString BinaryShaderFile = Path / (GetFormat().GetPlainNameString() + TEXT("_") + FileName);
	
	if ( IFileManager::Get().FileSize(*BinaryShaderFile) > 0 )
	{
		FArchive* BinaryShaderAr = IFileManager::Get().CreateFileReader(*BinaryShaderFile);
		
		if ( BinaryShaderAr != nullptr )
		{
			*BinaryShaderAr << *this;
			
			if ( !BinaryShaderAr->IsError() && BinaryShaderAr->CustomVer(FShaderCacheCustomVersion::Key) == FShaderCacheCustomVersion::Latest && BinaryShaderAr->CustomVer(FShaderCacheCustomVersion::GameKey) == FShaderCache::GameVersion )
			{
				bLoadedCache = true;
			}
			else
			{
				IFileManager::Get().Delete(*BinaryShaderFile);
			}
			
			delete BinaryShaderAr;
		}
	}
	
	return bLoadedCache;
}

FPixelShaderRHIRef FShaderCacheLibrary::CreatePixelShader(const FSHAHash& Hash)
{
	FShaderCacheKey Key;
	Key.Frequency = SF_Pixel;
	Key.SHAHash = Hash;
	Key.bActive = true;
	
	FPixelShaderRHIRef Shader;
	TPair<uint32,TArray<uint8>> const* CacheCode = Shaders.Find(Key);
	if (CacheCode && CacheCode->Value.Num())
	{
		if (CacheCode->Key != CacheCode->Value.Num() && RHISupportsShaderCompression(Platform))
		{
			TArray<uint8> UncompressedCode;
			FShaderCacheHelperUncompressCode(CacheCode->Key, CacheCode->Value, UncompressedCode);
			Shader = RHICreatePixelShader(UncompressedCode);
		}
		else
		{
			Shader = RHICreatePixelShader(CacheCode->Value);
		}
		check(IsValidRef(Shader));
		Shader->SetHash(Key.SHAHash);
	}
	
	return Shader;
}

FVertexShaderRHIRef FShaderCacheLibrary::CreateVertexShader(const FSHAHash& Hash)
{
	FShaderCacheKey Key;
	Key.Frequency = SF_Vertex;
	Key.SHAHash = Hash;
	Key.bActive = true;
	
	FVertexShaderRHIRef Shader;
	TPair<uint32,TArray<uint8>> const* CacheCode = Shaders.Find(Key);
	if (CacheCode && CacheCode->Value.Num())
	{
		if (CacheCode->Key != CacheCode->Value.Num() && RHISupportsShaderCompression(Platform))
		{
			TArray<uint8> UncompressedCode;
			FShaderCacheHelperUncompressCode(CacheCode->Key, CacheCode->Value, UncompressedCode);
			Shader = RHICreateVertexShader(UncompressedCode);
		}
		else
		{
			Shader = RHICreateVertexShader(CacheCode->Value);
		}
		check(IsValidRef(Shader));
		Shader->SetHash(Key.SHAHash);
	}
	
	return Shader;
}

FHullShaderRHIRef FShaderCacheLibrary::CreateHullShader(const FSHAHash& Hash)
{
	FShaderCacheKey Key;
	Key.Frequency = SF_Hull;
	Key.SHAHash = Hash;
	Key.bActive = true;
	
	FHullShaderRHIRef Shader;
	TPair<uint32,TArray<uint8>> const* CacheCode = Shaders.Find(Key);
	if (CacheCode && CacheCode->Value.Num())
	{
		if (CacheCode->Key != CacheCode->Value.Num() && RHISupportsShaderCompression(Platform))
		{
			TArray<uint8> UncompressedCode;
			FShaderCacheHelperUncompressCode(CacheCode->Key, CacheCode->Value, UncompressedCode);
			Shader = RHICreateHullShader(UncompressedCode);
		}
		else
		{
			Shader = RHICreateHullShader(CacheCode->Value);
		}
		check(IsValidRef(Shader));
		Shader->SetHash(Key.SHAHash);
	}
	
	return Shader;
}

FDomainShaderRHIRef FShaderCacheLibrary::CreateDomainShader(const FSHAHash& Hash)
{
	FShaderCacheKey Key;
	Key.Frequency = SF_Domain;
	Key.SHAHash = Hash;
	Key.bActive = true;
	
	FDomainShaderRHIRef Shader;
	TPair<uint32,TArray<uint8>> const* CacheCode = Shaders.Find(Key);
	if (CacheCode && CacheCode->Value.Num())
	{
		if (CacheCode->Key != CacheCode->Value.Num() && RHISupportsShaderCompression(Platform))
		{
			TArray<uint8> UncompressedCode;
			FShaderCacheHelperUncompressCode(CacheCode->Key, CacheCode->Value, UncompressedCode);
			Shader = RHICreateDomainShader(UncompressedCode);
		}
		else
		{
			Shader = RHICreateDomainShader(CacheCode->Value);
		}
		check(IsValidRef(Shader));
		Shader->SetHash(Key.SHAHash);
	}
	
	return Shader;
}

FGeometryShaderRHIRef FShaderCacheLibrary::CreateGeometryShader(const FSHAHash& Hash)
{
	FShaderCacheKey Key;
	Key.Frequency = SF_Geometry;
	Key.SHAHash = Hash;
	Key.bActive = true;
	
	FGeometryShaderRHIRef Shader;
	TPair<uint32,TArray<uint8>> const* CacheCode = Shaders.Find(Key);
	if (CacheCode && CacheCode->Value.Num())
	{
		if (CacheCode->Key != CacheCode->Value.Num() && RHISupportsShaderCompression(Platform))
		{
			TArray<uint8> UncompressedCode;
			FShaderCacheHelperUncompressCode(CacheCode->Key, CacheCode->Value, UncompressedCode);
			Shader = RHICreateGeometryShader(UncompressedCode);
		}
		else
		{
			Shader = RHICreateGeometryShader(CacheCode->Value);
		}
		check(IsValidRef(Shader));
		Shader->SetHash(Key.SHAHash);
	}
	
	return Shader;
}

FGeometryShaderRHIRef FShaderCacheLibrary::CreateGeometryShaderWithStreamOutput(const FSHAHash& Hash, const FStreamOutElementList& ElementList, uint32 NumStrides, const uint32* Strides, int32 RasterizedStream)
{
	FShaderCacheKey Key;
	Key.Frequency = SF_Geometry;
	Key.SHAHash = Hash;
	Key.bActive = true;
	
	FGeometryShaderRHIRef Shader;
	TPair<uint32,TArray<uint8>> const* CacheCode = Shaders.Find(Key);
	if (CacheCode && CacheCode->Value.Num())
	{
		if (CacheCode->Key != CacheCode->Value.Num() && RHISupportsShaderCompression(Platform))
		{
			TArray<uint8> UncompressedCode;
			FShaderCacheHelperUncompressCode(CacheCode->Key, CacheCode->Value, UncompressedCode);
			Shader = RHICreateGeometryShaderWithStreamOutput(UncompressedCode, ElementList, NumStrides, Strides, RasterizedStream);
		}
		else
		{
			Shader = RHICreateGeometryShaderWithStreamOutput(CacheCode->Value, ElementList, NumStrides, Strides, RasterizedStream);
		}
		check(IsValidRef(Shader));
		Shader->SetHash(Key.SHAHash);
	}
	
	return Shader;
}

FComputeShaderRHIRef FShaderCacheLibrary::CreateComputeShader(const FSHAHash& Hash)
{
	FShaderCacheKey Key;
	Key.Frequency = SF_Compute;
	Key.SHAHash = Hash;
	Key.bActive = true;
	
	FComputeShaderRHIRef Shader;
	TPair<uint32,TArray<uint8>> const* CacheCode = Shaders.Find(Key);
	if (CacheCode && CacheCode->Value.Num())
	{
		if (CacheCode->Key != CacheCode->Value.Num() && RHISupportsShaderCompression(Platform))
		{
			TArray<uint8> UncompressedCode;
			FShaderCacheHelperUncompressCode(CacheCode->Key, CacheCode->Value, UncompressedCode);
			Shader = RHICreateComputeShader(UncompressedCode);
		}
		else
		{
			Shader = RHICreateComputeShader(CacheCode->Value);
		}
		check(IsValidRef(Shader));
		Shader->SetHash(Key.SHAHash);
	}
	
	return Shader;
}

FName FShaderCacheLibrary::GetFormat( void ) const
{
	return LegacyShaderPlatformToShaderFormat(Platform);
}

bool FShaderCacheLibrary::AddShader( uint8 Frequency, const FSHAHash& Hash, TArray<uint8> const& UncompressedCode, uint32 const UncompressedSize )
{
	bool bCompressedCode = false;
	TArray<uint8> CompressedCode;
	
	// Perform in-memory compression
	if (RHISupportsShaderCompression(Platform) && (UncompressedCode.Num() == UncompressedSize))
	{
		// Recompress
		int32 CompressedSize = UncompressedCode.Num();
		
		// Make sure Code is large enough just in case archiving actually increased the size somehow
		CompressedCode.SetNum(CompressedSize);
		
		bool bCompressOK = FCompression::CompressMemory( ShaderCacheCompressionFlag, CompressedCode.GetData(), CompressedSize, UncompressedCode.GetData(), UncompressedCode.Num() );
		check(bCompressOK);
		
		// Reset array size to final compressed size
		CompressedCode.SetNum(CompressedSize);
		
		// Use uncompressed if compression results in larger data block
		if((uint32)CompressedSize < UncompressedSize)
		{
			bCompressedCode = true;
		}
	}
	
	// NOTE: Adds with uncompresssed size key
	FShaderCacheKey Key;
	Key.Frequency = (EShaderFrequency)Frequency;
	Key.SHAHash = Hash;
	Key.bActive = true;
	
	bool bAdded = !Shaders.Contains(Key);
	if (bAdded)
	{
		TArray<uint8> const& Code = bCompressedCode ? CompressedCode : UncompressedCode;
		Shaders.Add(Key, TPairInitializer<uint32, TArray<uint8>>(UncompressedSize, Code));
		
#if WITH_EDITORONLY_DATA
		if (GShaderCacheBinaryCacheLogging > 0)
		{
			auto& List = Counts.FindOrAdd(Key);
			bool bFound = false;
			for (int32 Index = 0; Index < List.Num(); ++Index)
			{
				if (Code == List[Index].Value)
				{
					++List[Index].Key;
					bFound = true;
				}
			}
			if (!bFound)
			{
				int32 Index = List.AddDefaulted();
				List[Index].Value = Code;
				List[Index].Key = 1;
			}
		}
#endif
	}
	
	return bAdded;
}

bool FShaderCacheLibrary::Finalize( FString OutputDir, TArray<FString>* OutputFiles )
{
	bool bWroteShaders = false;
	
	if(Shaders.Num() > 0)
	{
		FString BinaryShaderFile = OutputDir / (GetFormat().GetPlainNameString() + TEXT("_") + FileName);
		FArchive* BinaryShaderAr = IFileManager::Get().CreateFileWriter(*BinaryShaderFile);
		if( BinaryShaderAr != NULL )
		{
			*BinaryShaderAr << *this;
			BinaryShaderAr->Flush();
			delete BinaryShaderAr;
			
			if (OutputFiles)
			{
				OutputFiles->Add(BinaryShaderFile);
			}
			
			bWroteShaders = true;
		}
	}
	
	return bWroteShaders;
}

FArchive& operator<<( FArchive& Ar, FShaderCacheLibrary& Info )
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

//
//ShaderCache
//
FShaderCache* FShaderCache::Cache = nullptr;
int32 FShaderCache::GameVersion = 0;
uint32 FShaderCache::MaxTextureSamplers = EShaderCacheMaxNumSamplers;
static double LoadTimeStart = 0;

static bool ShaderPlatformCanPrebindBoundShaderState(EShaderPlatform Platform)
{
	switch (Platform)
	{
		case SP_PCD3D_SM5:
		case SP_PS4:
		case SP_XBOXONE_D3D12:
		case SP_PCD3D_SM4:
		case SP_PCD3D_ES2:
		case SP_METAL:
		case SP_METAL_MRT:
		case SP_METAL_MRT_MAC:
		case SP_METAL_SM5:
		case SP_METAL_MACES3_1:
		case SP_METAL_MACES2:
		case SP_OPENGL_PCES2:
		case SP_OPENGL_ES2_ANDROID:
		case SP_OPENGL_ES3_1_ANDROID:
		case SP_OPENGL_ES31_EXT:
		case SP_OPENGL_ES2_IOS:
		case SP_SWITCH:
		case SP_SWITCH_FORWARD:
		{
			return true;
		}
		case SP_OPENGL_SM4:
		case SP_OPENGL_SM5:
		case SP_OPENGL_ES2_WEBGL:
		default:
		{
			return false;
		}
	}
}

static inline bool ShaderPlatformPrebindRequiresResource(EShaderPlatform Platform)
{
	return IsOpenGLPlatform(Platform);
}

static inline bool ShaderPlatformPSOOnly(EShaderPlatform Platform)
{
	return IsMetalPlatform(Platform);
}

static bool IsShaderUsable(EShaderPlatform Platform, EShaderFrequency Frequency)
{
	bool bUsable = true;
	switch (Frequency)
	{
		case SF_Geometry:
			bUsable &= RHISupportsGeometryShaders(Platform);
			break;
			
		case SF_Hull:
		case SF_Domain:
			bUsable &= RHISupportsTessellation(Platform);
			break;
			
		case SF_Compute:
			bUsable &= RHISupportsComputeShaders(Platform);
			break;
			
		default:
			break;
	}
	
	return bUsable;
}

FShaderCache::FShaderCache(uint32 InOptions, EShaderPlatform InShaderPlatform)
: FTickableObjectRenderThread(true)
, CurrentPlatform(InShaderPlatform)
, CodeCache(nullptr)
, StreamingKey(0)
, Options(InOptions)
, OverridePrecompileTime(0)
, OverridePredrawBatchTime(0)
, bBatchingPaused(false)
, DefaultCacheState(nullptr)
{
	SaveShaderCacheCmd = IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("r.SaveShaderCache"),
			TEXT("Save shader cache."),
			FConsoleCommandDelegate::CreateRaw(this, &FShaderCache::SaveAll),
			ECVF_Default
			);

	FCoreDelegates::ApplicationWillDeactivateDelegate.AddRaw(this, &FShaderCache::OnAppDeactivate);
	MaxTextureSamplers = FMath::Min<uint32>(GetMaxTextureSamplers(), EShaderCacheMaxNumSamplers);
		
	// We expect the RHI to be created at this point
	CurrentShaderPlatformCache.ShaderPlatform = CurrentPlatform;

	if (IsMobilePlatform(CurrentPlatform))
	{
		// Make sure this is disabled on mobile
		// Mobile only needs FShaderCache::bUseShaderCaching
		FShaderCache::bUseShaderPredraw = 0;
		FShaderCache::bUseShaderDrawLog = 0;
	}
		
	DefaultCacheState = InternalCreateOrFindCacheStateForContext(GDynamicRHI->RHIGetDefaultContext());
			
	// Try to load user cache, making sure that if we fail version test we still try game-content version.
	bool bLoadedUserCache = false;
	if (bUseUserShaderCache)
	{
		FString UserBinaryShaderFile = GetShaderCacheFilename(FPaths::ProjectSavedDir(), CurrentPlatform);
		bLoadedUserCache = LoadShaderCache(UserBinaryShaderFile, CurrentShaderPlatformCache);
	}
	
	// Fallback to game-content version.
	if (!bLoadedUserCache)
	{
		FString GameBinaryShaderFile = GetShaderCacheFilename(FPaths::ProjectContentDir(), CurrentPlatform);
		LoadShaderCache(GameBinaryShaderFile, CurrentShaderPlatformCache);
	}
}

FShaderCache::~FShaderCache()
{
	IConsoleManager::Get().UnregisterConsoleObject(SaveShaderCacheCmd);
	
	if (bUseUserShaderCache)
	{
		FString BinaryShaderFile = GetShaderCacheFilename(FPaths::ProjectSavedDir(), CurrentPlatform);
		SaveShaderCache(BinaryShaderFile, CurrentShaderPlatformCache);
		SaveBinaryCache(FPaths::ProjectSavedDir(), FPlatformProperties::PlatformName());
	}
}

void FShaderCache::SaveAll()
{
	FRWScopeLock Lock(ShaderCacheGlobalStateMutex,SLT_Write);
	FRWScopeLock ResourceLock(PipelineStateMutex,SLT_Write);
	FRWScopeLock DrawLock(DrawLogMutex,SLT_Write);
	
	FString BinaryShaderFile = GetShaderCacheFilename(FPaths::ProjectSavedDir(), CurrentPlatform);
	SaveShaderCache(BinaryShaderFile, CurrentShaderPlatformCache);
	SaveBinaryCache(FPaths::ProjectSavedDir(), FPlatformProperties::PlatformName());
}

void FShaderCache::OnAppDeactivate()
{
	if (bUseUserShaderCache)
	{
		SaveAll();
	}
}

void FShaderCache::SetGameVersion(int32 InGameVersion)
{
	check(!Cache);
	GameVersion = InGameVersion;
}

void FShaderCache::InitShaderCache(uint32 Options, EShaderPlatform InShaderPlatform)
{
	check(!Cache);
	
	// Set the GameVersion to the FEngineVersion::Current().GetChangelist() value if it wasn't set to ensure we don't load invalidated caches.
	if (GameVersion == 0)
	{
		GameVersion = (int32)FEngineVersion::Current().GetChangelist();
	}
	
	if(bUseShaderCaching)
	{
		Cache = new FShaderCache(Options, InShaderPlatform);
	}
}

void FShaderCache::ShutdownShaderCache()
{
	if (Cache)
	{
		delete Cache;
		Cache = nullptr;
	}
}

void FShaderCache::LoadBinaryCache()
{
	if (!Cache)
	{
		return;
	}

	FShaderCacheState* CacheState = Cache->InternalCreateOrFindCacheStateForContext(&GRHICommandList.GetImmediateCommandList().GetContext());

	//I think we can get away without any locks in here specifically during the initial load phase as we are iterating the code cache
	LoadTimeStart = FPlatformTime::Seconds();
	Cache->ShadersToPrecompile = 0;

	if (bUseShaderBinaryCache)
	{
		if (FShaderCodeLibrary::GetShaderCount() > 0)
		{
			Cache->ShadersToPrecompile = FShaderCodeLibrary::GetShaderCount();
			Cache->ShaderLibraryPreCompileProgress.Add(FShaderCodeLibrary::CreateIterator());

			EShaderPlatform Platform = FShaderCodeLibrary::GetRuntimeShaderPlatform();
			TSet<FShaderCodeLibraryPipeline> const* CachedPipelines = FShaderCodeLibrary::GetShaderPipelines(Platform);
			if (CachedPipelines)
			{
				FShaderPipelineKey PipelineKey;
				PipelineKey.VertexShader.Frequency = SF_Vertex;
				PipelineKey.GeometryShader.Frequency = SF_Geometry;
				PipelineKey.HullShader.Frequency = SF_Hull;
				PipelineKey.DomainShader.Frequency = SF_Domain;
				PipelineKey.PixelShader.Frequency = SF_Pixel;

				FSHAHash Null;

				for (FShaderCodeLibraryPipeline const& Pipeline : *CachedPipelines)
				{
					PipelineKey.Hash = 0;

					PipelineKey.VertexShader.SHAHash = Pipeline.VertexShader;
					PipelineKey.VertexShader.Hash = 0;
					PipelineKey.VertexShader.bActive = (Null != Pipeline.VertexShader);

					PipelineKey.GeometryShader.SHAHash = Pipeline.GeometryShader;
					PipelineKey.GeometryShader.Hash = 0;
					PipelineKey.GeometryShader.bActive = (Null != Pipeline.GeometryShader);

					PipelineKey.HullShader.SHAHash = Pipeline.HullShader;
					PipelineKey.HullShader.Hash = 0;
					PipelineKey.HullShader.bActive = (Null != Pipeline.HullShader);

					PipelineKey.DomainShader.SHAHash = Pipeline.DomainShader;
					PipelineKey.DomainShader.Hash = 0;
					PipelineKey.DomainShader.bActive = (Null != Pipeline.DomainShader);

					PipelineKey.PixelShader.SHAHash = Pipeline.PixelShader;
					PipelineKey.PixelShader.Hash = 0;
					PipelineKey.PixelShader.bActive = (Null != Pipeline.PixelShader);

					if (PipelineKey.VertexShader.bActive)
					{
						TSet<FShaderPipelineKey>& ShaderPipelines = Cache->Pipelines.FindOrAdd(PipelineKey.VertexShader);
						ShaderPipelines.Add(PipelineKey);
					}

					if (PipelineKey.GeometryShader.bActive)
					{
						TSet<FShaderPipelineKey>& ShaderPipelines = Cache->Pipelines.FindOrAdd(PipelineKey.GeometryShader);
						ShaderPipelines.Add(PipelineKey);
					}

					if (PipelineKey.HullShader.bActive)
					{
						TSet<FShaderPipelineKey>& ShaderPipelines = Cache->Pipelines.FindOrAdd(PipelineKey.HullShader);
						ShaderPipelines.Add(PipelineKey);
					}

					if (PipelineKey.DomainShader.bActive)
					{
						TSet<FShaderPipelineKey>& ShaderPipelines = Cache->Pipelines.FindOrAdd(PipelineKey.DomainShader);
						ShaderPipelines.Add(PipelineKey);
					}

					if (PipelineKey.PixelShader.bActive)
					{
						TSet<FShaderPipelineKey>& ShaderPipelines = Cache->Pipelines.FindOrAdd(PipelineKey.PixelShader);
						ShaderPipelines.Add(PipelineKey);
					}
				}
			}
		}

		// Regardless of the presence of the platform specific file we want an modifiable FShaderCacheLibrary to catch any outliers.
		FShaderCacheLibrary* ShaderCacheLib = new FShaderCacheLibrary(Cache->CurrentPlatform, GShaderCodeCacheFileName);
		ShaderCacheLib->AddRef();

		// Try to load user cache, making sure that if we fail version test we still try game-content version.
		bool bLoadedCache = ShaderCacheLib->Load(FPaths::ProjectSavedDir());
		Cache->CodeCache = ShaderCacheLib;

		if (bLoadedCache)
		{
			for (auto const& ShaderPipelinePair : ShaderCacheLib->Pipelines)
			{
				TSet<FShaderPipelineKey>& ShaderPipelines = Cache->Pipelines.FindOrAdd(ShaderPipelinePair.Key);
				ShaderPipelines.Append(ShaderPipelinePair.Value);
			}
			if (ShaderCacheLib->GetShaderCount())
			{
				Cache->ShadersToPrecompile += ShaderCacheLib->GetShaderCount();
				Cache->ShaderLibraryPreCompileProgress.Add(ShaderCacheLib->CreateIterator());
			}
		}

		bool const bUseAsync = bUseAsyncShaderPrecompilation != 0;
		{
			double StartTime = FPlatformTime::Seconds();
			bUseAsyncShaderPrecompilation = false;

			TArray<uint8> DummyCode;

			while (!bUseAsyncShaderPrecompilation && Cache->ShaderLibraryPreCompileProgress.Num() > 0)
			{
				TRefCountPtr<FRHIShaderLibrary::FShaderLibraryIterator> ShaderIterator = Cache->ShaderLibraryPreCompileProgress[0];

				while (ShaderIterator->IsValid() && !bUseAsyncShaderPrecompilation)
				{
					auto LibraryEntry = **ShaderIterator;

					if (LibraryEntry.IsValid())
					{
						FShaderCacheKey Key;
						Key.Frequency = LibraryEntry.Frequency;
						Key.SHAHash = LibraryEntry.Hash;
						Key.bActive = true;

						Cache->InternalSubmitShader(Key, DummyCode, ShaderIterator->GetLibrary(), CacheState);
					}

					double Duration = FPlatformTime::Seconds() - StartTime;
					if (bUseAsync && InitialShaderLoadTime >= 0.0f && Duration >= InitialShaderLoadTime)
					{
						bUseAsyncShaderPrecompilation = bUseAsync;
					}

					--(Cache->ShadersToPrecompile);
					++(*ShaderIterator);
				}

				//Invalid iterator - continue otherwise we're in async mode leave iterator in list
				if (!ShaderIterator->IsValid())
				{
					Cache->ShaderLibraryPreCompileProgress.RemoveAt(0);
				}
				else
				{
					break;
				}
			}
		}
	}
}

void FShaderCache::SaveBinaryCache(FString OutputDir, FName PlatformName)
{
	if (bUseShaderBinaryCache && Cache)
	{
		Cache->CodeCache->Finalize(OutputDir, nullptr);
		Cache->CodeCache->Release();
		Cache->CodeCache = nullptr;
}
}

FVertexShaderRHIRef FShaderCache::GetVertexShader(EShaderPlatform Platform, FSHAHash Hash, TArray<uint8> const& Code)
{
	FVertexShaderRHIRef Shader = nullptr;
	
	FShaderCacheKey Key;
	Key.Frequency = SF_Vertex;
	Key.SHAHash = Hash;
	Key.bActive = true;
	
	if (Platform == CurrentPlatform)
	{
		FRWScopeLock Lock(ShaderCacheGlobalStateMutex,SLT_ReadOnly);
		Shader = CachedVertexShaders.FindRef(Key);
	}
	
	if(!IsValidRef(Shader))
	{
		Shader = FShaderCodeLibrary::CreateVertexShader(Platform, Hash, Code);
		check(IsValidRef(Shader));
		Shader->SetHash(Hash);
		
		if (Platform == CurrentPlatform)
		{
			FRWScopeLock Lock(ShaderCacheGlobalStateMutex,SLT_Write);
			CurrentShaderPlatformCache.Shaders.Add(Key);
			CachedVertexShaders.Add(Key, Shader);
		}
		
		INC_DWORD_STAT(STATGROUP_NumShadersCached);
	}
	
	return Shader;
}

FPixelShaderRHIRef FShaderCache::GetPixelShader(EShaderPlatform Platform, FSHAHash Hash, TArray<uint8> const& Code)
{
	FPixelShaderRHIRef Shader = nullptr;
	
	FShaderCacheKey Key;
	Key.Frequency = SF_Pixel;
	Key.SHAHash = Hash;
	Key.bActive = true;
	
	if (Platform == CurrentPlatform)
	{
		FRWScopeLock Lock(ShaderCacheGlobalStateMutex,SLT_ReadOnly);
		Shader = CachedPixelShaders.FindRef(Key);
	}
	
	if(!IsValidRef(Shader))
	{
		Shader = FShaderCodeLibrary::CreatePixelShader(Platform, Hash, Code);
		check(IsValidRef(Shader));
		Shader->SetHash(Hash);
		
		if (Platform == CurrentPlatform)
		{
			FRWScopeLock Lock(ShaderCacheGlobalStateMutex,SLT_Write);
			CurrentShaderPlatformCache.Shaders.Add(Key);
			CachedPixelShaders.Add(Key, Shader);
		}
		
		INC_DWORD_STAT(STATGROUP_NumShadersCached);
	}
	
	return Shader;
}

FGeometryShaderRHIRef FShaderCache::GetGeometryShader(EShaderPlatform Platform, FSHAHash Hash, TArray<uint8> const& Code)
{
	FGeometryShaderRHIRef Shader = nullptr;
	
	FShaderCacheKey Key;
	Key.Frequency = SF_Geometry;
	Key.SHAHash = Hash;
	Key.bActive = true;
	
	if (Platform == CurrentPlatform)
	{
		FRWScopeLock Lock(ShaderCacheGlobalStateMutex,SLT_ReadOnly);
		Shader = CachedGeometryShaders.FindRef(Key);
	}
	
	if(!IsValidRef(Shader))
	{
		Shader = FShaderCodeLibrary::CreateGeometryShader(Platform, Hash, Code);
		check(IsValidRef(Shader));
		Shader->SetHash(Hash);
		
		if (Platform == CurrentPlatform)
		{
			FRWScopeLock Lock(ShaderCacheGlobalStateMutex,SLT_Write);
			CurrentShaderPlatformCache.Shaders.Add(Key);
			CachedGeometryShaders.Add(Key, Shader);
		}
		
		INC_DWORD_STAT(STATGROUP_NumShadersCached);
	}
	
	return Shader;
}

FHullShaderRHIRef FShaderCache::GetHullShader(EShaderPlatform Platform, FSHAHash Hash, TArray<uint8> const& Code)
{
	FHullShaderRHIRef Shader = nullptr;
	
	FShaderCacheKey Key;
	Key.Frequency = SF_Hull;
	Key.SHAHash = Hash;
	Key.bActive = true;
	
	if (Platform == CurrentPlatform)
	{
		FRWScopeLock Lock(ShaderCacheGlobalStateMutex,SLT_ReadOnly);
		Shader = CachedHullShaders.FindRef(Key);
	}
	
	if(!IsValidRef(Shader))
	{
		Shader = RHICreateHullShader(Code);
		check(IsValidRef(Shader));
		Shader->SetHash(Hash);
		
		if (Platform == CurrentPlatform)
		{
			FRWScopeLock Lock(ShaderCacheGlobalStateMutex,SLT_Write);
			CurrentShaderPlatformCache.Shaders.Add(Key);
			CachedHullShaders.Add(Key, Shader);
		}
		
		INC_DWORD_STAT(STATGROUP_NumShadersCached);
	}
	
	return Shader;
}

FDomainShaderRHIRef FShaderCache::GetDomainShader(EShaderPlatform Platform, FSHAHash Hash, TArray<uint8> const& Code)
{
	FDomainShaderRHIRef Shader = nullptr;
	
	FShaderCacheKey Key;
	Key.Frequency = SF_Domain;
	Key.SHAHash = Hash;
	Key.bActive = true;
	
	if (Platform == CurrentPlatform)
	{
		FRWScopeLock Lock(ShaderCacheGlobalStateMutex,SLT_ReadOnly);
		Shader = CachedDomainShaders.FindRef(Key);
	}
	
	if(!Shader.IsValid())
	{
		Shader = FShaderCodeLibrary::CreateDomainShader(Platform, Hash, Code);
		check(IsValidRef(Shader));
		Shader->SetHash(Hash);
		
		if (Platform == CurrentPlatform)
		{
			FRWScopeLock Lock(ShaderCacheGlobalStateMutex,SLT_Write);
			CurrentShaderPlatformCache.Shaders.Add(Key);
			CachedDomainShaders.Add(Key, Shader);
		}
		
		INC_DWORD_STAT(STATGROUP_NumShadersCached);
	}
	
	return Shader;
}

FComputeShaderRHIRef FShaderCache::GetComputeShader(EShaderPlatform Platform, FSHAHash Hash, TArray<uint8> const& Code)
{
	FComputeShaderRHIRef Shader = nullptr;
	
	FShaderCacheKey Key;
	Key.Frequency = SF_Compute;
	Key.SHAHash = Hash;
	Key.bActive = true;
	
	if (Platform == CurrentPlatform)
	{
		FRWScopeLock Lock(ShaderCacheGlobalStateMutex,SLT_ReadOnly);
		Shader = CachedComputeShaders.FindRef(Key);
	}
	
	if(!Shader.IsValid())
	{
		Shader = FShaderCodeLibrary::CreateComputeShader(Platform, Hash, Code);
		check(IsValidRef(Shader));
		
		if (Platform == CurrentPlatform)
		{
			FRWScopeLock Lock(ShaderCacheGlobalStateMutex,SLT_Write);
			CurrentShaderPlatformCache.Shaders.Add(Key);
			CachedComputeShaders.Add(Key, Shader);
		}
		
		INC_DWORD_STAT(STATGROUP_NumShadersCached);
	}
	
	return Shader;
}

void FShaderCache::InternalLogStreamingKey(uint32 StreamKey, bool const bActive)
{
	if( bUseShaderPredraw || bUseShaderDrawLog )
	{
		FRWScopeLock Lock(DrawLogMutex,SLT_Write);
		
		if(bActive)
		{
			ActiveStreamingKeys.Add(StreamKey);
		}
		else
		{
			ActiveStreamingKeys.Remove(StreamKey);
		}
		
		uint32 NewStreamingKey = 0;
		for(uint32 Key : ActiveStreamingKeys)
		{
			NewStreamingKey ^= Key;
		}
		
		StreamingKey = NewStreamingKey;
		
		if(!ShadersToDraw.Contains(NewStreamingKey))
		{
			ShadersToDraw.Add(NewStreamingKey, CurrentShaderPlatformCache.StreamingDrawStates.FindRef(NewStreamingKey));
		}
	}
}

void FShaderCache::InternalLogShader(EShaderPlatform Platform, EShaderFrequency Frequency, FSHAHash Hash, uint32 UncompressedSize, TArray<uint8> const& Code, FShaderCacheState* CacheState)
{
	if (Platform == CurrentPlatform && IsShaderUsable(Platform, Frequency))
	{
		FShaderCacheKey Key;
		Key.SHAHash = Hash;
		Key.Frequency = Frequency;
		Key.bActive = true;

		// On mobile we pre-compile shader only if it's found in the recorded drawlog
		if (IsMobilePlatform(CurrentPlatform))
		{
			FRWScopeLock Lock(ShaderCacheGlobalStateMutex, SLT_ReadOnly);
			if (!CurrentShaderPlatformCache.Shaders.Contains(Key))
			{
				return;
			}
		}

		FShaderCache* ShaderCache = this;

		ENQUEUE_RENDER_COMMAND(LogShader)(
			[ShaderCache, Key, UncompressedSize, Code](FRHICommandListImmediate& RHICmdList)
		{
			FShaderCacheState* ShaderCacheState = ShaderCache->GetDefaultCacheState();

			// No need to cache if the binary cache is disabled or the shader is already present,
			// plus archived shaders will have no bytecode so can't be cached
			bool bSubmit = !ShaderCache->bUseShaderBinaryCache || !ShaderCache->bUseAsyncShaderPrecompilation;

			if (ShaderCache->bUseShaderBinaryCache && Code.Num())
			{
				FRWScopeLock Lock(ShaderCache->ShaderCacheGlobalStateMutex, SLT_ReadOnly);

				FShaderCacheLibrary* ShaderCacheLibrary = ShaderCache->CodeCache;
				if (ShaderCacheLibrary && !ShaderCacheLibrary->Shaders.Contains(Key))
				{
					Lock.RaiseLockToWrite();
					ShaderCacheLibrary->AddShader(Key.Frequency, Key.SHAHash, Code, UncompressedSize);
					bSubmit = true;
				}
			}

			if (!(Cache->Options & SCO_NoShaderPreload) && bSubmit)
			{
				if (Code.Num() != UncompressedSize && RHISupportsShaderCompression(ShaderCache->CurrentPlatform))
				{
					TArray<uint8> UncompressedCode;
					FShaderCacheHelperUncompressCode(UncompressedSize, Code, UncompressedCode);
					ShaderCache->InternalSubmitShader(Key, UncompressedCode, nullptr, ShaderCacheState);
				}
				else
				{
					ShaderCache->InternalSubmitShader(Key, Code, nullptr, ShaderCacheState);
				}
			}
		});
	}
}

void FShaderCache::InternalLogVertexDeclaration(const FShaderCacheState& CacheState, const FVertexDeclarationElementList& VertexElements, FVertexDeclarationRHIParamRef VertexDeclaration)
{
	if ((!CacheState.bIsPreBind && !CacheState.bIsPreDraw))
	{
		FRWScopeLock Lock(ShaderCacheGlobalStateMutex,SLT_Write);
		
		VertexDeclarations.Add(VertexDeclaration, VertexElements);
	}
}

void FShaderCache::InternalLogGraphicsPipelineState(const FShaderCacheState& CacheState,
													EShaderPlatform Platform,
													const FGraphicsPipelineStateInitializer& Initializer,
													FGraphicsPipelineStateRHIParamRef PSO)
{
	if ( (bUseShaderPredraw || bUseShaderDrawLog) && !CacheState.bIsPreBind && !CacheState.bIsPreDraw)
	{
		FRWScopeLock Lock(ShaderCacheGlobalStateMutex,SLT_Write);
		InternalPrelockedLogGraphicsPipelineState(Platform, Initializer, PSO);
	}
}

void FShaderCache::InternalPrelockedLogGraphicsPipelineState(EShaderPlatform Platform,
													const FGraphicsPipelineStateInitializer& Initializer,
													FGraphicsPipelineStateRHIParamRef PSO)
{
	FShaderCacheGraphicsPipelineState State;
	
	// Cache the BSS from the PSO - we need its index.
	State.BoundShaderState = InternalPrelockedLogBoundShaderState(Platform, Initializer.BoundShaderState.VertexDeclarationRHI, Initializer.BoundShaderState.VertexShaderRHI, Initializer.BoundShaderState.PixelShaderRHI, Initializer.BoundShaderState.HullShaderRHI, Initializer.BoundShaderState.DomainShaderRHI, Initializer.BoundShaderState.GeometryShaderRHI, nullptr);
	
	State.BlendState = BlendStates.FindChecked(Initializer.BlendState);
	State.RasterizerState = RasterizerStates.FindChecked(Initializer.RasterizerState);
	State.DepthStencilState = DepthStencilStates.FindChecked(Initializer.DepthStencilState);
	State.PrimitiveType = Initializer.PrimitiveType;
	
	State.SampleCount = Initializer.NumSamples;
	State.ActiveRenderTargets = Initializer.RenderTargetsEnabled;
	
	uint32 const NumActive = Initializer.ComputeNumValidRenderTargets();
	check(NumActive < MaxSimultaneousRenderTargets);

	for (uint32 i = 0; i < NumActive; i++)
	{
		State.RenderTargets[i] = Initializer.RenderTargetFormats[i];
		State.RenderTargetFlags[i] = Initializer.RenderTargetFlags[i];
		State.RenderTargetLoad[i] = (uint8)Initializer.RenderTargetLoadActions[i];
		State.RenderTargetStore[i] = (uint8)Initializer.RenderTargetStoreActions[i];
	}
	State.DepthStencilTarget = Initializer.DepthStencilTargetFormat;
	State.DepthStencilTargetFlags = Initializer.DepthStencilTargetFlag;
	State.DepthLoad = (uint8)Initializer.DepthTargetLoadAction;
	State.DepthStore = (uint8)Initializer.DepthTargetStoreAction;
	State.StencilLoad = (uint8)Initializer.StencilTargetLoadAction;
	State.StencilStore = (uint8)Initializer.StencilTargetStoreAction;
	
	FShaderPlatformCache& PlatformCache = CurrentShaderPlatformCache;
	int32 InfoId = PlatformCache.PipelineStates.Add(State);
	
	State.Index = InfoId;
	
	GraphicsPSOs.Add(State, PSO);
	GraphicsPSOStates.Add(PSO, State);
}


void FShaderCache::InternalLogBoundShaderState(const FShaderCacheState& CacheState, EShaderPlatform Platform, FVertexDeclarationRHIParamRef VertexDeclaration,
											   FVertexShaderRHIParamRef VertexShader,
											   FPixelShaderRHIParamRef PixelShader,
											   FHullShaderRHIParamRef HullShader,
											   FDomainShaderRHIParamRef DomainShader,
											   FGeometryShaderRHIParamRef GeometryShader,
											   FBoundShaderStateRHIParamRef BoundState)
{
	if (Platform == CurrentPlatform && !CacheState.bIsPreBind && !CacheState.bIsPreDraw)
	{
		FRWScopeLock Lock(ShaderCacheGlobalStateMutex,SLT_Write);
		InternalPrelockedLogBoundShaderState( Platform, VertexDeclaration, VertexShader, PixelShader, HullShader, DomainShader, GeometryShader, BoundState);
	}
}

int32 FShaderCache::InternalPrelockedLogBoundShaderState( EShaderPlatform Platform, FVertexDeclarationRHIParamRef VertexDeclaration,
														FVertexShaderRHIParamRef VertexShader,
														FPixelShaderRHIParamRef PixelShader,
														FHullShaderRHIParamRef HullShader,
														FDomainShaderRHIParamRef DomainShader,
														FGeometryShaderRHIParamRef GeometryShader,
														FBoundShaderStateRHIParamRef BoundState)
{
	FShaderPipelineKey PipelineKey;
	FShaderCacheBoundState Info;
	if(VertexDeclaration)
	{
		Info.VertexDeclaration = VertexDeclarations.FindChecked(VertexDeclaration);
	}
	if(VertexShader)
	{
		Info.VertexShader.Frequency = SF_Vertex;
		Info.VertexShader.SHAHash = VertexShader->GetHash();
		Info.VertexShader.bActive = true;
		PipelineKey.VertexShader = Info.VertexShader;
	}
	if(PixelShader)
	{
		Info.PixelShader.Frequency = SF_Pixel;
		Info.PixelShader.SHAHash = PixelShader->GetHash();
		Info.PixelShader.bActive = true;
		PipelineKey.PixelShader = Info.PixelShader;
	}
	if(GeometryShader)
	{
		Info.GeometryShader.Frequency = SF_Geometry;
		Info.GeometryShader.SHAHash = GeometryShader->GetHash();
		Info.GeometryShader.bActive = true;
		PipelineKey.GeometryShader = Info.GeometryShader;
	}
	if(HullShader)
	{
		Info.HullShader.Frequency = SF_Hull;
		Info.HullShader.SHAHash = HullShader->GetHash();
		Info.HullShader.bActive = true;
		PipelineKey.HullShader = Info.HullShader;
	}
	if(DomainShader)
	{
		Info.DomainShader.Frequency = SF_Domain;
		Info.DomainShader.SHAHash = DomainShader->GetHash();
		Info.DomainShader.bActive = true;
		PipelineKey.DomainShader = Info.DomainShader;
	}
	
	FShaderPlatformCache& PlatformCache = CurrentShaderPlatformCache;
	int32 InfoId = PlatformCache.BoundShaderStates.Add(Info);
	
	if (BoundState)
	{
		BoundShaderStates.Add(Info, BoundState);
	}
	
	if(VertexShader)
	{
		TSet<int32>& Set = PlatformCache.ShaderStateMembership.FindOrAdd(PlatformCache.Shaders.FindIndexChecked(Info.VertexShader));
		if(!Set.Find(InfoId))
		{
			Set.Add(InfoId);
		}
		if(bUseShaderBinaryCache && IsOpenGLPlatform(Platform))
		{
			TSet<FShaderPipelineKey>& ShaderPipelines = CodeCache->Pipelines.FindOrAdd(Info.VertexShader);
			ShaderPipelines.Add(PipelineKey);
		}
	}
	if(PixelShader)
	{
		TSet<int32>& Set = PlatformCache.ShaderStateMembership.FindOrAdd(PlatformCache.Shaders.FindIndexChecked(Info.PixelShader));
		if(!Set.Find(InfoId))
		{
			Set.Add(InfoId);
		}
		if(bUseShaderBinaryCache && IsOpenGLPlatform(Platform))
		{
			TSet<FShaderPipelineKey>& ShaderPipelines = CodeCache->Pipelines.FindOrAdd(Info.PixelShader);
			ShaderPipelines.Add(PipelineKey);
		}
	}
	if(GeometryShader)
	{
		TSet<int32>& Set = PlatformCache.ShaderStateMembership.FindOrAdd(PlatformCache.Shaders.FindIndexChecked(Info.GeometryShader));
		if(!Set.Find(InfoId))
		{
			Set.Add(InfoId);
		}
		if(bUseShaderBinaryCache && IsOpenGLPlatform(Platform))
		{
			TSet<FShaderPipelineKey>& ShaderPipelines = CodeCache->Pipelines.FindOrAdd(Info.GeometryShader);
			ShaderPipelines.Add(PipelineKey);
		}
	}
	if(HullShader)
	{
		TSet<int32>& Set = PlatformCache.ShaderStateMembership.FindOrAdd(PlatformCache.Shaders.FindIndexChecked(Info.HullShader));
		if(!Set.Find(InfoId))
		{
			Set.Add(InfoId);
		}
		if(bUseShaderBinaryCache && IsOpenGLPlatform(Platform))
		{
			TSet<FShaderPipelineKey>& ShaderPipelines = CodeCache->Pipelines.FindOrAdd(Info.HullShader);
			ShaderPipelines.Add(PipelineKey);
		}
	}
	if(DomainShader)
	{
		TSet<int32>& Set = PlatformCache.ShaderStateMembership.FindOrAdd(PlatformCache.Shaders.FindIndexChecked(Info.DomainShader));
		if(!Set.Find(InfoId))
		{
			Set.Add(InfoId);
		}
		if(bUseShaderBinaryCache && IsOpenGLPlatform(Platform))
		{
			TSet<FShaderPipelineKey>& ShaderPipelines = CodeCache->Pipelines.FindOrAdd(Info.DomainShader);
			ShaderPipelines.Add(PipelineKey);
		}
	}
	
	if (BoundState)
	{
		INC_DWORD_STAT(STATGROUP_NumBSSCached);
		ShaderStates.Add(BoundState, Info);
	}
	
	return InfoId;
}

void FShaderCache::InternalLogBlendState(FShaderCacheState const& CacheState, FBlendStateInitializerRHI const& Init, FBlendStateRHIParamRef State)
{
	if ( (bUseShaderPredraw || bUseShaderDrawLog) && !CacheState.bIsPreDraw )
	{
		FRWScopeLock Lock(PipelineStateMutex,SLT_Write);
		BlendStates.Add(State, Init);
	}
}

void FShaderCache::InternalLogRasterizerState(FShaderCacheState const& CacheState, FRasterizerStateInitializerRHI const& Init, FRasterizerStateRHIParamRef State)
{
	if ( (bUseShaderPredraw || bUseShaderDrawLog) && !CacheState.bIsPreDraw )
	{
		FRWScopeLock Lock(PipelineStateMutex,SLT_Write);
		RasterizerStates.Add(State, Init);
	}
}

void FShaderCache::InternalLogDepthStencilState(FShaderCacheState const& CacheState, FDepthStencilStateInitializerRHI const& Init, FDepthStencilStateRHIParamRef State)
{
	if ( (bUseShaderPredraw || bUseShaderDrawLog) && !CacheState.bIsPreDraw)
	{
		FRWScopeLock Lock(PipelineStateMutex,SLT_Write);
		DepthStencilStates.Add(State, Init);
	}
}

void FShaderCache::InternalLogSamplerState(FShaderCacheState const& CacheState, FSamplerStateInitializerRHI const& Init, FSamplerStateRHIParamRef State)
{
	check(ShaderPlatformPrebindRequiresResource(CurrentPlatform) && "Called by an RHI that doesn't require binding for pre-draw");
	
	if ( (bUseShaderPredraw || bUseShaderDrawLog) && !CacheState.bIsPreDraw )
	{
		int32 ID = CurrentShaderPlatformCache.SamplerStates.Add(Init);
		SamplerStates.Add(State, ID);
	}
}

void FShaderCache::InternalLogTexture(FShaderTextureKey const& Init, FTextureRHIParamRef State)
{
	check(ShaderPlatformPrebindRequiresResource(CurrentPlatform) && "Called by an RHI that doesn't require binding for pre-draw");
	
	if ( (bUseShaderPredraw || bUseShaderDrawLog) )
	{
		FShaderResourceKey Key;
		Key.Tex = Init;
		Key.Format = Init.Format;
		int32 ID = CurrentShaderPlatformCache.Resources.Add(Key);
		
		Textures.Add(State, ID);
		CachedTextures.Add(Init, State);
	}
}

void FShaderCache::InternalLogSRV(FShaderResourceViewRHIParamRef SRV, FTextureRHIParamRef Texture, uint8 StartMip, uint8 NumMips, uint8 Format)
{
	check(ShaderPlatformPrebindRequiresResource(CurrentPlatform) && "Called by an RHI that doesn't require binding for pre-draw");
	
	if ( (bUseShaderPredraw || bUseShaderDrawLog) )
	{
		FShaderResourceKey& TexKey = CurrentShaderPlatformCache.Resources[Textures.FindChecked(Texture)];
		
		FShaderResourceKey Key;
		Key.Tex = TexKey.Tex;
		Key.BaseMip = StartMip;
		Key.MipLevels = NumMips;
		Key.Format = Format;
		Key.bSRV = true;
		
		SRVs.Add(SRV, Key);
		CachedSRVs.Add(Key, FShaderResourceViewBinding(SRV, nullptr, Texture));
		
		CurrentShaderPlatformCache.Resources.Add(Key);
	}
}

void FShaderCache::InternalLogSRV(FShaderResourceViewRHIParamRef SRV, FVertexBufferRHIParamRef Vb, uint32 Stride, uint8 Format)
{
	check(ShaderPlatformPrebindRequiresResource(CurrentPlatform) && "Called by an RHI that doesn't require binding for pre-draw");
	
	if ( (bUseShaderPredraw || bUseShaderDrawLog) )
	{
		FShaderResourceKey Key;
		Key.Tex.Type = SCTT_Buffer;
		Key.Tex.X = Vb->GetSize();
		Key.Tex.Y = Vb->GetUsage();
		Key.Tex.Z = Stride;
		Key.Tex.Format = Format;
		Key.bSRV = true;
		
		SRVs.Add(SRV, Key);
		CachedSRVs.Add(Key, FShaderResourceViewBinding(SRV, Vb, nullptr));
		
		CurrentShaderPlatformCache.Resources.Add(Key);
	}
}

void FShaderCache::InternalRemoveSRV(FShaderResourceViewRHIParamRef SRV)
{
	check(ShaderPlatformPrebindRequiresResource(CurrentPlatform) && "Called by an RHI that doesn't require binding for pre-draw");
	
	if ( (bUseShaderPredraw || bUseShaderDrawLog) )
	{
		auto Key = SRVs.FindRef(SRV);
		CachedSRVs.Remove(Key);
		SRVs.Remove(SRV);
	}
}

void FShaderCache::InternalRemoveTexture(FTextureRHIParamRef Texture)
{
	check(ShaderPlatformPrebindRequiresResource(CurrentPlatform) && "Called by an RHI that doesn't require binding for pre-draw");
	
	if ( (bUseShaderPredraw || bUseShaderDrawLog) )
	{
		FShaderResourceKey& TexKey = CurrentShaderPlatformCache.Resources[Textures.FindChecked(Texture)];
		
		CachedTextures.Remove(TexKey.Tex);
		Textures.Remove(Texture);
	}
}

void FShaderCache::InternalSetBlendState(FShaderCacheState& CacheState, FBlendStateRHIParamRef State)
{
	if ( (bUseShaderPredraw || bUseShaderDrawLog) && !CacheState.bIsPreDraw && State )
	{
		FRWScopeLock Lock(PipelineStateMutex,SLT_ReadOnly);
		
		CacheState.CurrentPSO.BlendState = BlendStates.FindChecked(State);
		CacheState.CurrentPSO.Hash = 0;
	}
}

void FShaderCache::InternalSetRasterizerState(FShaderCacheState& CacheState, FRasterizerStateRHIParamRef State)
{
	if ( (bUseShaderPredraw || bUseShaderDrawLog) && !CacheState.bIsPreDraw && State )
	{
		FRWScopeLock Lock(PipelineStateMutex,SLT_ReadOnly);
		
		CacheState.CurrentPSO.RasterizerState = RasterizerStates.FindChecked(State);
		CacheState.CurrentPSO.Hash = 0;
	}
}

void FShaderCache::InternalSetDepthStencilState(FShaderCacheState& CacheState, FDepthStencilStateRHIParamRef State)
{
	if ( (bUseShaderPredraw || bUseShaderDrawLog) && !CacheState.bIsPreDraw && State )
	{
		FRWScopeLock Lock(PipelineStateMutex,SLT_ReadOnly);
		
		CacheState.CurrentPSO.DepthStencilState = DepthStencilStates.FindChecked(State);
		CacheState.CurrentPSO.Hash = 0;
	}
}

void FShaderCache::InternalSetRenderTargets(FShaderCacheState& CacheState, uint32 NumSimultaneousRenderTargets, const FRHIRenderTargetView* NewRenderTargetsRHI, const FRHIDepthRenderTargetView* NewDepthStencilTargetRHI )
{
	if ( bUseShaderDrawLog && !CacheState.bIsPreDraw )
	{
		CacheState.CurrentNumRenderTargets = NumSimultaneousRenderTargets;
		CacheState.bCurrentDepthStencilTarget = (NewDepthStencilTargetRHI != nullptr);
		
		FMemory::Memzero(CacheState.CurrentRenderTargets, sizeof(FRHIRenderTargetView) * MaxSimultaneousRenderTargets);
		FMemory::Memcpy(CacheState.CurrentRenderTargets, NewRenderTargetsRHI, sizeof(FRHIRenderTargetView) * NumSimultaneousRenderTargets);
		
		if( NewDepthStencilTargetRHI )
		{
			CacheState.CurrentDepthStencilTarget = *NewDepthStencilTargetRHI;
		}
		
		if(ShaderPlatformPrebindRequiresResource(CurrentPlatform))
		{
			FMemory::Memset(CacheState.CurrentPSO.RenderTargets, 255, sizeof(uint32) * MaxSimultaneousRenderTargets);
			for( int32 RenderTargetIndex = NumSimultaneousRenderTargets - 1; RenderTargetIndex >= 0; --RenderTargetIndex )
			{
				FRHIRenderTargetView const& Target = NewRenderTargetsRHI[RenderTargetIndex];
				CacheState.InvalidResourceCount -= (uint32)(CacheState.CurrentPSO.RenderTargets[RenderTargetIndex] == EShaderCacheInvalidState);
				if ( Target.Texture )
				{
					int32* TexIndex = Textures.Find(Target.Texture);
					if(TexIndex)
					{
						FShaderRenderTargetKey Key;
						FShaderResourceKey& TexKey = CurrentShaderPlatformCache.Resources[*TexIndex];
						Key.Texture = TexKey.Tex;
						check(Key.Texture.MipLevels == Target.Texture->GetNumMips());
						Key.MipLevel = Key.Texture.MipLevels > Target.MipIndex ? Target.MipIndex : 0;
						Key.ArrayIndex = Target.ArraySliceIndex;
						CacheState.CurrentPSO.RenderTargets[RenderTargetIndex] = CurrentShaderPlatformCache.RenderTargets.Add(Key);
					}
					else
					{
						UE_LOG(LogShaders, Warning, TEXT("Binding invalid texture %p to render target index %d, draw logging will be suspended until this is reset to a valid or null reference."), Target.Texture, RenderTargetIndex);
						CacheState.CurrentPSO.RenderTargets[RenderTargetIndex] = EShaderCacheInvalidState;
						CacheState.InvalidResourceCount++;
					}
				}
				else
				{
					CacheState.CurrentPSO.RenderTargets[RenderTargetIndex] = EShaderCacheNullState;
				}
			}
			
			CacheState.InvalidResourceCount -= (uint32)(CacheState.CurrentPSO.DepthStencilTarget == EShaderCacheInvalidState);
			if ( NewDepthStencilTargetRHI && NewDepthStencilTargetRHI->Texture )
			{
				int32* TexIndex = Textures.Find(NewDepthStencilTargetRHI->Texture);
				if(TexIndex)
				{
					FShaderRenderTargetKey Key;
					FShaderResourceKey& TexKey = CurrentShaderPlatformCache.Resources[*TexIndex];
					Key.Texture = TexKey.Tex;
					CacheState.CurrentPSO.DepthStencilTarget = CurrentShaderPlatformCache.RenderTargets.Add(Key);
				}
				else
				{
					UE_LOG(LogShaders, Warning, TEXT("Binding invalid texture %p to denpth-stencil target, draw logging will be suspended until this is reset to a valid or null reference."), NewDepthStencilTargetRHI->Texture);
					CacheState.CurrentPSO.DepthStencilTarget = EShaderCacheInvalidState;
					CacheState.InvalidResourceCount++;
				}
			}
			else
			{
				CacheState.CurrentPSO.DepthStencilTarget = EShaderCacheNullState;
			}
		}
		else
		{
			//Non resource handling just record the format - no locking required
			for( int32 RenderTargetIndex = NumSimultaneousRenderTargets - 1; RenderTargetIndex >= 0; --RenderTargetIndex )
			{
				FRHIRenderTargetView const& Target = NewRenderTargetsRHI[RenderTargetIndex];
				if ( Target.Texture )
				{
					CacheState.CurrentPSO.RenderTargets[RenderTargetIndex] = Target.Texture->GetFormat();
				}
				else
				{
					CacheState.CurrentPSO.RenderTargets[RenderTargetIndex] = EPixelFormat::PF_Unknown;
				}
			}
			
			if ( NewDepthStencilTargetRHI && NewDepthStencilTargetRHI->Texture )
			{
				CacheState.CurrentPSO.DepthStencilTarget = NewDepthStencilTargetRHI->Texture->GetFormat();
			}
			else
			{
				CacheState.CurrentPSO.DepthStencilTarget = EPixelFormat::PF_Unknown;
			}
		}
		
		CacheState.CurrentPSO.Hash = 0;
	}
}

void FShaderCache::InternalSetSamplerState(FShaderCacheState& CacheState, EShaderFrequency Frequency, uint32 Index, FSamplerStateRHIParamRef State)
{
	check(ShaderPlatformPrebindRequiresResource(CurrentPlatform) && "Called by an RHI that doesn't require binding for pre-draw");
	
	if ( (bUseShaderDrawLog && !CacheState.bIsPreDraw) )
	{
		if (Index >= EShaderCacheMaxNumSamplers)
		{
			// Hardware can support more samplers than EShaderCacheMaxNumSamplers, so just skip those we can't fit in cache
			return;
		}
		CacheState.InvalidResourceCount -= (uint32)(CacheState.CurrentDrawKey.SamplerStates[Frequency][Index] == EShaderCacheInvalidState);
		if ( State )
		{
			int32 const* SamplerIndex = SamplerStates.Find(State);
			if(SamplerIndex)
			{
				CacheState.CurrentDrawKey.SamplerStates[Frequency][Index] = *SamplerIndex;
			}
			else
			{
				UE_LOG(LogShaders, Warning, TEXT("Binding invalid sampler %p to shader stage %u index %u, draw logging will be suspended until this is reset to a valid or null reference."), State, (uint32)Frequency, Index);
				CacheState.CurrentDrawKey.SamplerStates[Frequency][Index] = EShaderCacheInvalidState;
				CacheState.InvalidResourceCount++;
			}
		}
		else
		{
			CacheState.CurrentDrawKey.SamplerStates[Frequency][Index] = EShaderCacheNullState;
		}
		
		CacheState.CurrentDrawKey.Hash = 0;
	}
}

void FShaderCache::InternalSetTexture(FShaderCacheState& CacheState, EShaderFrequency Frequency, uint32 Index, FTextureRHIParamRef State)
{
	check(ShaderPlatformPrebindRequiresResource(CurrentPlatform) && "Called by an RHI that doesn't require binding for pre-draw");
	
	if ( (bUseShaderDrawLog && !CacheState.bIsPreDraw) )
	{
		checkf(Index < MaxResources, TEXT("Attempting to texture bind at index %u which exceeds RHI max. %d"), Index, MaxResources);
		CacheState.InvalidResourceCount -= (uint32)(CacheState.CurrentDrawKey.Resources[Frequency][Index] == EShaderCacheInvalidState);
		if ( State )
		{
			FShaderResourceKey Key;
			
			FTextureRHIParamRef Tex = State;
			if ( State->GetTextureReference() )
			{
				Tex = State->GetTextureReference()->GetReferencedTexture();
			}
			
			int32* TexIndex = Textures.Find(Tex);
			if(TexIndex)
			{
				CacheState.CurrentDrawKey.Resources[Frequency][Index] = *TexIndex;
				if (Index < 64)
				{
					CacheState.CurrentDrawKey.UsedResourcesLo[Frequency] |= (uint64)((uint64)1 << Index);
				}
				else
				{
					CacheState.CurrentDrawKey.UsedResourcesHi[Frequency] |= (uint64)((uint64)1 << (Index - 64));
				}
			}
			else
			{
				UE_LOG(LogShaders, Warning, TEXT("Binding invalid texture %p to shader stage %u index %u, draw logging will be suspended until this is reset to a valid or null reference."), State, (uint32)Frequency, Index);
				CacheState.CurrentDrawKey.Resources[Frequency][Index] = EShaderCacheInvalidState;
				if (Index < 64)
				{
					CacheState.CurrentDrawKey.UsedResourcesLo[Frequency] |= (uint64)((uint64)1 << Index);
				}
				else
				{
					CacheState.CurrentDrawKey.UsedResourcesHi[Frequency] |= (uint64)((uint64)1 << (Index - 64));
				}
				CacheState.InvalidResourceCount++;
			}
		}
		else
		{
			CacheState.CurrentDrawKey.Resources[Frequency][Index] = EShaderCacheNullState;
			if (Index < 64)
			{
				CacheState.CurrentDrawKey.UsedResourcesLo[Frequency] &= ~(uint64)((uint64)1 << Index);
			}
			else
			{
				CacheState.CurrentDrawKey.UsedResourcesHi[Frequency] &= ~(uint64)((uint64)1 << (Index - 64));
			}
		}
		
		CacheState.CurrentDrawKey.Hash = 0;
	}
}

void FShaderCache::InternalSetSRV(FShaderCacheState& CacheState, EShaderFrequency Frequency, uint32 Index, FShaderResourceViewRHIParamRef SRV)
{
	check(ShaderPlatformPrebindRequiresResource(CurrentPlatform) && "Called by an RHI that doesn't require binding for pre-draw");
	
	if ( (bUseShaderDrawLog && !CacheState.bIsPreDraw) )
	{
		checkf(Index < MaxResources, TEXT("Attempting to bind SRV at index %u which exceeds RHI max. %d"), Index, MaxResources);
		CacheState.InvalidResourceCount -= (uint32)(CacheState.CurrentDrawKey.Resources[Frequency][Index] == EShaderCacheInvalidState);
		if ( SRV )
		{
			FShaderResourceKey* Key = SRVs.Find(SRV);
			if(Key)
			{
				CacheState.CurrentDrawKey.Resources[Frequency][Index] = CurrentShaderPlatformCache.Resources.Add(*Key);
				if (Index < 64)
				{
					CacheState.CurrentDrawKey.UsedResourcesLo[Frequency] |= (uint64)((uint64)1 << Index);
				}
				else
				{
					CacheState.CurrentDrawKey.UsedResourcesHi[Frequency] |= (uint64)((uint64)1 << (Index - 64));
				}
			}
			else
			{
				UE_LOG(LogShaders, Warning, TEXT("Binding invalid SRV %p to shader stage %u index %u, draw logging will be suspended until this is reset to a valid or null reference."), SRV, (uint32)Frequency, Index);
				CacheState.CurrentDrawKey.Resources[Frequency][Index] = EShaderCacheInvalidState;
				if (Index < 64)
				{
					CacheState.CurrentDrawKey.UsedResourcesLo[Frequency] |= (uint64)((uint64)1 << Index);
				}
				else
				{
					CacheState.CurrentDrawKey.UsedResourcesHi[Frequency] |= (uint64)((uint64)1 << (Index - 64));
				}
				CacheState.InvalidResourceCount++;
			}
		}
		else
		{
			CacheState.CurrentDrawKey.Resources[Frequency][Index] = EShaderCacheNullState;
			if (Index < 64)
			{
				CacheState.CurrentDrawKey.UsedResourcesLo[Frequency] &= ~(uint64)((uint64)1 << Index);
			}
			else
			{
				CacheState.CurrentDrawKey.UsedResourcesHi[Frequency] &= ~(uint64)((uint64)1 << (Index - 64));
			}
		}
		
		CacheState.CurrentDrawKey.Hash = 0;
	}
}

void FShaderCache::InternalSetBoundShaderState(FShaderCacheState& CacheState, FBoundShaderStateRHIParamRef State)
{
	if ( (bUseShaderPredraw || bUseShaderDrawLog) && !CacheState.bIsPreDraw )
	{
		FMemory::Memset(CacheState.CurrentDrawKey.SamplerStates, 255, sizeof(CacheState.CurrentDrawKey.SamplerStates));
		FMemory::Memset(CacheState.CurrentDrawKey.Resources, 255, sizeof(CacheState.CurrentDrawKey.Resources));
		FMemory::Memzero(CacheState.CurrentDrawKey.UsedResourcesLo, sizeof(CacheState.CurrentDrawKey.UsedResourcesLo));
		FMemory::Memzero(CacheState.CurrentDrawKey.UsedResourcesHi, sizeof(CacheState.CurrentDrawKey.UsedResourcesHi));
		
		if ( State )
		{
			FRWScopeLock Lock(ShaderCacheGlobalStateMutex,SLT_ReadOnly);
			
			FShaderCacheBoundState* NewState = ShaderStates.Find(State);
			int32 StateIndex = NewState ? CurrentShaderPlatformCache.BoundShaderStates.FindIndexChecked(*NewState) : -1;
			if(NewState && StateIndex >= 0)
			{
				CacheState.CurrentPSO.BoundShaderState = StateIndex;
			}
			else
			{
				UE_LOG(LogShaders, Fatal, TEXT("Binding invalid bound-shader-state %p"), State);
				CacheState.CurrentPSO.BoundShaderState = EShaderCacheNullState;
			}
		}
		else
		{
			CacheState.CurrentPSO.BoundShaderState = EShaderCacheNullState;
		}
		
		CacheState.CurrentPSO.Hash = 0;
		CacheState.CurrentDrawKey.Hash = 0;
	}
}

void FShaderCache::InternalSetViewport(FShaderCacheState& CacheState, uint32 MinX, uint32 MinY, float MinZ, uint32 MaxX, uint32 MaxY, float MaxZ)
{
	if ( (bUseShaderPredraw || bUseShaderDrawLog) && !CacheState.bIsPreDraw )
	{
		CacheState.Viewport[0] = MinX;
		CacheState.Viewport[1] = MinY;
		CacheState.Viewport[2] = MaxX;
		CacheState.Viewport[3] = MaxY;
		CacheState.DepthRange[0] = MinZ;
		CacheState.DepthRange[1] = MaxZ;
	}
}

void FShaderCache::InternalSetGraphicsPipelineState(FShaderCacheState& CacheState, FGraphicsPipelineStateRHIParamRef State)
{
	if ( (bUseShaderPredraw || bUseShaderDrawLog) && !CacheState.bIsPreDraw )
	{
		FMemory::Memset(CacheState.CurrentDrawKey.SamplerStates, 255, sizeof(CacheState.CurrentDrawKey.SamplerStates));
		FMemory::Memset(CacheState.CurrentDrawKey.Resources, 255, sizeof(CacheState.CurrentDrawKey.Resources));
		FMemory::Memzero(CacheState.CurrentDrawKey.UsedResourcesLo, sizeof(CacheState.CurrentDrawKey.UsedResourcesLo));
		FMemory::Memzero(CacheState.CurrentDrawKey.UsedResourcesHi, sizeof(CacheState.CurrentDrawKey.UsedResourcesHi));
		
		if ( State )
		{
			FRWScopeLock Lock(ShaderCacheGlobalStateMutex,SLT_ReadOnly);
			
			FShaderCacheGraphicsPipelineState* NewState = GraphicsPSOStates.Find(State);
			if(NewState)
			{
				CacheState.CurrentPSO = *NewState;
			}
			else
			{
				UE_LOG(LogShaders, Fatal, TEXT("Binding invalid bound-shader-state %p"), State);
			}
		}
		
		CacheState.CurrentDrawKey.Hash = 0;
	}
}

void FShaderCache::InternalLogDraw(FShaderCacheState& CacheState, uint32 PrimitiveType, uint8 IndexType)
{
	if ( bUseShaderDrawLog && !CacheState.bIsPreDraw && CacheState.InvalidResourceCount == 0 )
	{
		bool bShaderDrawSetEntryExists = true;
		{
			FRWScopeLock Lock(DrawLogMutex,SLT_ReadOnly);
			
			CacheState.CurrentDrawKey.IndexType = IndexType;
			if (!ShaderPlatformPSOOnly(GMaxRHIShaderPlatform))
			{
				CacheState.CurrentPSO.PrimitiveType = PrimitiveType;
				CacheState.CurrentPSO.Hash = 0;
			}
			
			int32 DrawId = CurrentShaderPlatformCache.DrawStates.FindIndex(CacheState.CurrentDrawKey);
			int32 PSOId = ShaderPlatformPSOOnly(CurrentPlatform) ? CacheState.CurrentPSO.Index : CurrentShaderPlatformCache.PipelineStates.FindIndex(CacheState.CurrentPSO);
			
			if (DrawId < 0)
			{
				Lock.RaiseLockToWrite();
				DrawId = CurrentShaderPlatformCache.DrawStates.Add(CacheState.CurrentDrawKey);
			}
			
			if (PSOId < 0)
			{
				Lock.RaiseLockToWrite();
				PSOId = ShaderPlatformPSOOnly(CurrentPlatform) ? CacheState.CurrentPSO.Index : CurrentShaderPlatformCache.PipelineStates.Add(CacheState.CurrentPSO);
			}

			check(DrawId >= 0 && PSOId >= 0);
			
			FShaderPreDrawEntry Entry;
			Entry.PSOIndex = PSOId;
			Entry.DrawKeyIndex = DrawId;
			Entry.bPredrawn = true;
			
			int32 EntryId = CurrentShaderPlatformCache.PreDrawEntries.FindIndex(Entry);
			
			if (EntryId < 0)
			{
				Lock.RaiseLockToWrite();
				EntryId = CurrentShaderPlatformCache.PreDrawEntries.Add(Entry);
			}
			
			FShaderStreamingCache* StreamCache = CurrentShaderPlatformCache.StreamingDrawStates.Find(StreamingKey);
			if (!StreamCache)
			{
				Lock.RaiseLockToWrite();
				StreamCache = &CurrentShaderPlatformCache.StreamingDrawStates.FindOrAdd(StreamingKey);
			}
			
			TSet<int32>* ShaderDrawSet = StreamCache->ShaderDrawStates.Find(CacheState.CurrentPSO.BoundShaderState);
			if (!ShaderDrawSet)
			{
				Lock.RaiseLockToWrite();
				ShaderDrawSet = &StreamCache->ShaderDrawStates.FindOrAdd(CacheState.CurrentPSO.BoundShaderState);
			}
			
			if (!ShaderDrawSet->Contains(EntryId))
			{
				Lock.RaiseLockToWrite();
				ShaderDrawSet->Add(EntryId,&bShaderDrawSetEntryExists);
			}
		}
		
		if( !bShaderDrawSetEntryExists )
		{
			INC_DWORD_STAT(STATGROUP_NumDrawsCached);
		}
	}
}

void FShaderCache::Tick( float DeltaTime )
{
	if ( Cache && Cache->bBatchingPaused == false )
	{
		Cache->InternalPreDrawShaders(GRHICommandList.GetImmediateCommandList(), DeltaTime);
	}
}

bool FShaderCache::IsTickable() const
{
	bool bTickable = false;
	
	if( !bBatchingPaused && bUseShaderBinaryCache && bUseAsyncShaderPrecompilation )
	{
		FRWScopeLock GlobalLock(ShaderCacheGlobalStateMutex,SLT_ReadOnly);
		bTickable = ShadersToPrecompile > 0;
	}
	
	if( !bTickable && !bBatchingPaused && bUseShaderPredraw )
	{
		FRWScopeLock DrawLock(DrawLogMutex,SLT_ReadOnly);
		bTickable = ShadersToDraw.FindRef(StreamingKey).ShaderDrawStates.Num() > 0;
	}
	
	return bTickable;
}

bool FShaderCache::ShouldPreDrawShaders(int64 CurrentPreDrawTime) const
{
	FRWScopeLock Lock(DrawLogMutex,SLT_ReadOnly);
	return bUseShaderPredraw && (GetPredrawBatchTime() == -1 || CurrentPreDrawTime < GetPredrawBatchTime()) && ShadersToDraw.FindRef(StreamingKey).ShaderDrawStates.Num() > 0;
}


// NOTE: I'm going to assume there is going to only be one thread running this function at any one time
// Currently This should be the only predraw function to have any locking
void FShaderCache::InternalPreDrawShaders(FRHICommandList& RHICmdList, float DeltaTime)
{
	static uint32 NumShadersToCompile = 1;
	
	static uint32 FrameNum = 0;
	
	if(FrameNum != GFrameNumberRenderThread || OverridePrecompileTime != 0 || OverridePredrawBatchTime != 0)
	{
		FShaderCacheState* CacheState = InternalCreateOrFindCacheStateForContext(&RHICmdList.GetContext());
		
		FrameNum = GFrameNumberRenderThread;
		
		uint32 NumCompiled = 0;
		int64 TimeForPredrawing = 0;
		TArray<uint8> DummyCode;
		
		if ( bUseShaderBinaryCache && bUseAsyncShaderPrecompilation && ShaderLibraryPreCompileProgress.Num() > 0 )
		{
			SET_DWORD_STAT(STATGROUP_NumToPrecompile, NumShadersToCompile);
			
			while(GetTargetPrecompileFrameTime() == -1 && ShaderLibraryPreCompileProgress.Num() > 0)
			{
				TRefCountPtr<FRHIShaderLibrary::FShaderLibraryIterator> ShaderIterator =  ShaderLibraryPreCompileProgress[0];
				
				while (ShaderIterator->IsValid() && NumCompiled < NumShadersToCompile)
				{
					auto LibraryEntry = **ShaderIterator;
					
					if(LibraryEntry.IsValid())
					{
						FShaderCacheKey Key;
						Key.Frequency = LibraryEntry.Frequency;
						Key.SHAHash = LibraryEntry.Hash;
						Key.bActive = true;
						
						InternalSubmitShader( Key, DummyCode, ShaderIterator->GetLibrary(), CacheState );
						
						INC_DWORD_STAT(STATGROUP_NumPrecompiled);
						INC_DWORD_STAT(STATGROUP_TotalPrecompiled);
						
						++NumCompiled;
					}
					
					--(ShadersToPrecompile);
					++(*ShaderIterator);
				}
				
				//Only remove if exhausted this iterator otherwise we've bailed for some other reason, break out
				if (!ShaderIterator->IsValid())
				{
					ShaderLibraryPreCompileProgress.RemoveAt(0);
				}
				else
				{
					break;
				}
			}
			
			if(GetTargetPrecompileFrameTime() != -1)
			{
				int64 MSec = DeltaTime * 1000.0;
				if( MSec < GetTargetPrecompileFrameTime() )
				{
					NumShadersToCompile++;
				}
				else
				{
					NumShadersToCompile = FMath::Max(1u, NumShadersToCompile / 2u);
				}
				
				if(GetPredrawBatchTime() != -1)
				{
					TimeForPredrawing += FMath::Max((MSec - (int64)GetTargetPrecompileFrameTime()), (int64)0);
				}
			}
			
			double LoadTimeUpdate = FPlatformTime::Seconds();
			SET_FLOAT_STAT(STATGROUP_BinaryCacheLoadTime, LoadTimeUpdate - LoadTimeStart);
		}
		
		bool bDoPreDraw = FrameNum > 1 && ShouldPreDrawShaders(TimeForPredrawing);
		
		if ( bDoPreDraw )
		{
			CacheState->bIsPreDraw = true;
			
			if ( !IsValidRef(IndexBufferUInt16) )
			{
				FRHIResourceCreateInfo Info;
				uint32 Stride = sizeof(uint16);
				uint32 Size = sizeof(uint16) * 3;
				void* Data = nullptr;
				IndexBufferUInt16 = RHICreateAndLockIndexBuffer(Stride, Size, BUF_Static, Info, Data);
				if ( Data )
				{
					FMemory::Memzero(Data, Size);
				}
				RHIUnlockIndexBuffer(IndexBufferUInt16);
			}
			if ( !IsValidRef(IndexBufferUInt32) )
			{
				FRHIResourceCreateInfo Info;
				uint32 Stride = sizeof(uint32);
				uint32 Size = sizeof(uint32) * 3;
				void* Data = nullptr;
				IndexBufferUInt32 = RHICreateAndLockIndexBuffer(Stride, Size, BUF_Static, Info, Data);
				if ( Data )
				{
					FMemory::Memzero(Data, Size);
				}
				RHIUnlockIndexBuffer(IndexBufferUInt32);
			}
			
			RHICmdList.SetViewport(0, 0, FLT_MIN, 3, 3, FLT_MAX);
			
			//
			//All Locks Required
			//
			{
				FRWScopeLock Lock(ShaderCacheGlobalStateMutex,SLT_Write);
				FRWScopeLock ResourceLock(PipelineStateMutex,SLT_Write);
				FRWScopeLock DrawLock(DrawLogMutex,SLT_Write);
				
				TMap<int32, TSet<int32>>& ShaderDrawStates = ShadersToDraw.FindOrAdd(StreamingKey).ShaderDrawStates;
				for ( auto It = ShaderDrawStates.CreateIterator(); (GetPredrawBatchTime() == -1 || TimeForPredrawing < GetPredrawBatchTime()) && It; ++It )
				{
					uint32 Start = FPlatformTime::Cycles();
					
					auto Shader = *It;
					if(Shader.Key >= 0)
					{
						TSet<int32>& ShaderDrawSet = Shader.Value;
						FShaderCacheBoundState& BSS = CurrentShaderPlatformCache.BoundShaderStates[Shader.Key];
						InternalPreDrawShader(RHICmdList, BSS, ShaderDrawSet);
					}
					
					It.RemoveCurrent();
					
					uint32 End = FPlatformTime::Cycles();
					TimeForPredrawing += FPlatformTime::ToMilliseconds(End - Start);
				}
			}
			
			// This is a bit dirty/naughty but it forces the draw commands to be flushed through on OS X
			// which means we can delete the resources without crashing MTGL.
			RHIFlushResources();
			
			//
			//Draw Lock Required
			//
			{
				FRWScopeLock DrawLock(DrawLogMutex,SLT_ReadOnly);
				if ( ShadersToDraw.FindOrAdd(StreamingKey).ShaderDrawStates.Num() == 0 )
				{
					PredrawRTs.Empty();
					PredrawBindings.Empty();
					PredrawVBs.Empty();
				}
			}
			
			CacheState->bIsPreDraw = false;
			
			double LoadTimeUpdate = FPlatformTime::Seconds();
			SET_FLOAT_STAT(STATGROUP_BinaryCacheLoadTime, LoadTimeUpdate - LoadTimeStart);
		}
		
		if(OverridePrecompileTime == -1)
		{
			OverridePrecompileTime = 0;
		}
		
		if(OverridePredrawBatchTime == -1)
		{
			OverridePredrawBatchTime = 0;
		}
	}
}

void FShaderCache::BeginAcceleratedBatching()
{
	if ( Cache )
	{
		if(AccelTargetPrecompileFrameTime)
		{
			Cache->OverridePrecompileTime = AccelTargetPrecompileFrameTime;
		}
		if(AccelPredrawBatchTime)
		{
			Cache->OverridePredrawBatchTime = AccelPredrawBatchTime;
		}
	}
}

void FShaderCache::EndAcceleratedBatching()
{
	if ( Cache )
	{
		Cache->OverridePrecompileTime = 0;
		Cache->OverridePredrawBatchTime = 0;
	}
}

void FShaderCache::FlushOutstandingBatches()
{
	if ( Cache )
	{
		Cache->OverridePrecompileTime = -1;
		Cache->OverridePredrawBatchTime = -1;
	}
}

void FShaderCache::PauseBatching()
{
	if ( Cache )
	{
		Cache->bBatchingPaused = true;
	}
}

void FShaderCache::ResumeBatching()
{
	if ( Cache )
	{
		Cache->bBatchingPaused = false;
	}
}

uint32 FShaderCache::NumShaderPrecompilesRemaining()
{
	if ( Cache && bUseShaderBinaryCache && bUseAsyncShaderPrecompilation )
	{
		FRWScopeLock Lock(Cache->ShaderCacheGlobalStateMutex,SLT_ReadOnly);
		return Cache->ShadersToPrecompile;
	}
	return 0;
}

bool FShaderCache::NeedsRenderingResumedForRenderingThreadTick() const
{
	return true;
}

TStatId FShaderCache::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FShaderCache, STATGROUP_Tickables);
}

void FShaderCache::InternalPrebindShader(FShaderCacheKey const& Key, FShaderCacheState* CacheState)
{
	bool const bCanPreBind = ShaderPlatformCanPrebindBoundShaderState(CurrentPlatform) || (CacheState != nullptr && CacheState->CurrentNumRenderTargets > 0);
	if ((bCanPreBind || bUseShaderPredraw) && CacheState != nullptr)
	{
		FRWScopeLock Lock(ShaderCacheGlobalStateMutex,SLT_Write);
		
		CacheState->bIsPreBind = true;
		
		// This only applies to OpenGL.
		if(IsOpenGLPlatform(CurrentPlatform))
		{
			TSet<FShaderPipelineKey> const* ShaderPipelines = Pipelines.Find(Key);
			if(ShaderPipelines && bCanPreBind)
			{
				for (FShaderPipelineKey const& Pipeline : *ShaderPipelines)
				{
					FVertexShaderRHIRef VertexShader = Pipeline.VertexShader.bActive ? CachedVertexShaders.FindRef(Pipeline.VertexShader) : nullptr;
					FPixelShaderRHIRef PixelShader = Pipeline.PixelShader.bActive ? CachedPixelShaders.FindRef(Pipeline.PixelShader) : nullptr;
					FGeometryShaderRHIRef GeometryShader = Pipeline.GeometryShader.bActive ? CachedGeometryShaders.FindRef(Pipeline.GeometryShader) : nullptr;
					FHullShaderRHIRef HullShader = Pipeline.HullShader.bActive ? CachedHullShaders.FindRef(Pipeline.HullShader) : nullptr;
					FDomainShaderRHIRef DomainShader = Pipeline.DomainShader.bActive ? CachedDomainShaders.FindRef(Pipeline.DomainShader) : nullptr;
					
					bool bOK = true;
					bOK &= (Pipeline.VertexShader.bActive == IsValidRef(VertexShader));
					bOK &= (Pipeline.PixelShader.bActive == IsValidRef(PixelShader));
					bOK &= (Pipeline.GeometryShader.bActive == IsValidRef(GeometryShader));
					bOK &= (Pipeline.HullShader.bActive == IsValidRef(HullShader));
					bOK &= (Pipeline.DomainShader.bActive == IsValidRef(DomainShader));
					
					if (bOK)
					{
						// Will return nullptr because there's no vertex declaration - we are just forcing the LinkedProgram creation.
						RHICreateBoundShaderState(nullptr,
												  VertexShader,
												  HullShader,
												  DomainShader,
												  PixelShader,
												  GeometryShader);
					}
				}
			}
		}
		
		int32 ShaderIndex = CurrentShaderPlatformCache.Shaders.FindIndex(Key);
		TSet<int32>* BoundStatesPtr = CurrentShaderPlatformCache.ShaderStateMembership.Find(ShaderIndex);
		if (BoundStatesPtr)
		{
			TSet<int32>& BoundStates = *BoundStatesPtr;
			for (int32 StateIndex : BoundStates)
			{
				FShaderCacheBoundState& State = CurrentShaderPlatformCache.BoundShaderStates[StateIndex];
				FVertexShaderRHIRef VertexShader = State.VertexShader.bActive ? CachedVertexShaders.FindRef(State.VertexShader) : nullptr;
				FPixelShaderRHIRef PixelShader = State.PixelShader.bActive ? CachedPixelShaders.FindRef(State.PixelShader) : nullptr;
				FGeometryShaderRHIRef GeometryShader = State.GeometryShader.bActive ? CachedGeometryShaders.FindRef(State.GeometryShader) : nullptr;
				FHullShaderRHIRef HullShader = State.HullShader.bActive ? CachedHullShaders.FindRef(State.HullShader) : nullptr;
				FDomainShaderRHIRef DomainShader = State.DomainShader.bActive ? CachedDomainShaders.FindRef(State.DomainShader) : nullptr;

				bool bOK = true;
				bOK &= (State.VertexShader.bActive == IsValidRef(VertexShader));
				bOK &= (State.PixelShader.bActive == IsValidRef(PixelShader));
				bOK &= (State.GeometryShader.bActive == IsValidRef(GeometryShader));
				bOK &= (State.HullShader.bActive == IsValidRef(HullShader));
				bOK &= (State.DomainShader.bActive == IsValidRef(DomainShader));

				if (bOK)
				{
					FVertexDeclarationRHIRef VertexDeclaration = RHICreateVertexDeclaration(State.VertexDeclaration);
					bOK &= IsValidRef(VertexDeclaration);

					if (bOK)
					{
						//NOTE: adding vertex declarations manually in here...
						VertexDeclarations.Add(VertexDeclaration, State.VertexDeclaration);

						if (ShaderPlatformPSOOnly(CurrentPlatform))
						{
							if (bUseShaderPredraw)
							{
								FRWScopeLock LockDraw(DrawLogMutex, SLT_Write);

								TSet<int32>& StreamCache = CurrentShaderPlatformCache.StreamingDrawStates.FindOrAdd(StreamingKey).ShaderDrawStates.FindOrAdd(StateIndex);
								if (!ShadersToDraw.FindOrAdd(StreamingKey).ShaderDrawStates.Contains(StateIndex))
								{
									ShadersToDraw.FindOrAdd(StreamingKey).ShaderDrawStates.Add(StateIndex, StreamCache);
								}
							}
						}
						else if (bCanPreBind)
						{
							FBoundShaderStateRHIRef BoundState = RHICreateBoundShaderState(VertexDeclaration,
								VertexShader,
								HullShader,
								DomainShader,
								PixelShader,
								GeometryShader);
							if (IsValidRef(BoundState))
							{
								//NOTE: adding vertex declarations manually in here...
								InternalPrelockedLogBoundShaderState(CurrentPlatform, VertexDeclaration, VertexShader, PixelShader, HullShader, DomainShader, GeometryShader, BoundState);

								if (bUseShaderPredraw)
								{
									FRWScopeLock LockDraw(DrawLogMutex, SLT_Write);

									TSet<int32>& StreamCache = CurrentShaderPlatformCache.StreamingDrawStates.FindOrAdd(StreamingKey).ShaderDrawStates.FindOrAdd(StateIndex);
									if (!ShadersToDraw.FindOrAdd(StreamingKey).ShaderDrawStates.Contains(StateIndex))
									{
										ShadersToDraw.FindOrAdd(StreamingKey).ShaderDrawStates.Add(StateIndex, StreamCache);
									}
								}
							}
						}
						else if (bUseShaderPredraw)
						{
							FRWScopeLock LockDraw(DrawLogMutex, SLT_Write);

							TSet<int32>& StreamCache = CurrentShaderPlatformCache.StreamingDrawStates.FindOrAdd(StreamingKey).ShaderDrawStates.FindOrAdd(StateIndex);
							if (!ShadersToDraw.FindOrAdd(StreamingKey).ShaderDrawStates.Contains(StateIndex))
							{
								ShadersToDraw.FindOrAdd(StreamingKey).ShaderDrawStates.Add(StateIndex, StreamCache);
							}
						}
					}
				}
			}
		}
		CacheState->bIsPreBind = false;
	}
}

//Library could be
// - Native RHIShaderLibrary
// - FShaderCacheLibrary
// - FShaderCodeArchive
void FShaderCache::InternalSubmitShader(FShaderCacheKey const& Key, TArray<uint8> const& Code, FRHIShaderLibrary* Library, FShaderCacheState* CacheState)
{
	switch (Key.Frequency)
	{
		case SF_Vertex:
		{
			FVertexShaderRHIRef Shader = nullptr;

			{
				FRWScopeLock Lock(ShaderCacheGlobalStateMutex, SLT_ReadOnly);
				Shader = CachedVertexShaders.FindRef(Key);
			}

			if (!Shader.IsValid())
			{
				if (Library != nullptr)
				{
					if (Library->IsNativeLibrary())
					{
						Shader = RHICreateVertexShader(Library, Key.SHAHash);
					}
					else
					{
						Shader = static_cast<FShaderFactoryInterface*>(Library)->CreateVertexShader(Key.SHAHash);
					}
				}

				if (!Shader.IsValid())
				{
					Shader = FShaderCodeLibrary::CreateVertexShader(CurrentPlatform, Key.SHAHash, Code);
				}

				if (Shader.IsValid())
				{
					Shader->SetHash(Key.SHAHash);

					{
						FRWScopeLock Lock(ShaderCacheGlobalStateMutex, SLT_Write);
						CurrentShaderPlatformCache.Shaders.Add(Key);
						CachedVertexShaders.Add(Key, Shader);
					}

					INC_DWORD_STAT(STATGROUP_NumShadersCached);
					InternalPrebindShader(Key, CacheState);
				}
			}
			break;
		}
		case SF_Pixel:
		{
			FPixelShaderRHIRef Shader = nullptr;

			{
				FRWScopeLock Lock(ShaderCacheGlobalStateMutex, SLT_ReadOnly);
				Shader = CachedPixelShaders.FindRef(Key);
			}

			if (!Shader.IsValid())
			{
				if (Library != nullptr)
				{
					if (Library->IsNativeLibrary())
					{
						Shader = RHICreatePixelShader(Library, Key.SHAHash);
					}
					else
					{
						Shader = static_cast<FShaderFactoryInterface*>(Library)->CreatePixelShader(Key.SHAHash);
					}
				}

				if (!Shader.IsValid())
				{
					Shader = FShaderCodeLibrary::CreatePixelShader(CurrentPlatform, Key.SHAHash, Code);
				}

				if (Shader.IsValid())
				{
					Shader->SetHash(Key.SHAHash);

					{
						FRWScopeLock Lock(ShaderCacheGlobalStateMutex, SLT_Write);
						CurrentShaderPlatformCache.Shaders.Add(Key);
						CachedPixelShaders.Add(Key, Shader);
					}

					INC_DWORD_STAT(STATGROUP_NumShadersCached);
					InternalPrebindShader(Key, CacheState);
				}
			}
			break;
		}
		case SF_Geometry:
		{
			FGeometryShaderRHIRef Shader = nullptr;

			{
				FRWScopeLock Lock(ShaderCacheGlobalStateMutex, SLT_ReadOnly);
				Shader = CachedGeometryShaders.FindRef(Key);
			}

			if (!Shader.IsValid())
			{
				if (Library != nullptr)
				{
					if (Library->IsNativeLibrary())
					{
						Shader = RHICreateGeometryShader(Library, Key.SHAHash);
					}
					else
					{
						Shader = static_cast<FShaderFactoryInterface*>(Library)->CreateGeometryShader(Key.SHAHash);
					}
				}

				if (!Shader.IsValid())
				{
					Shader = FShaderCodeLibrary::CreateGeometryShader(CurrentPlatform, Key.SHAHash, Code);
				}

				if (Shader.IsValid())
				{
					Shader->SetHash(Key.SHAHash);

					{
						FRWScopeLock Lock(ShaderCacheGlobalStateMutex, SLT_Write);
						CurrentShaderPlatformCache.Shaders.Add(Key);
						CachedGeometryShaders.Add(Key, Shader);
					}

					INC_DWORD_STAT(STATGROUP_NumShadersCached);
					InternalPrebindShader(Key, CacheState);
				}
			}
			break;
		}
		case SF_Hull:
		{
			FHullShaderRHIRef Shader = nullptr;

			{
				FRWScopeLock Lock(ShaderCacheGlobalStateMutex, SLT_ReadOnly);
				Shader = CachedHullShaders.FindRef(Key);
			}

			if (!Shader.IsValid())
			{
				if (Library != nullptr)
				{
					if (Library->IsNativeLibrary())
					{
						Shader = RHICreateHullShader(Library, Key.SHAHash);
					}
					else
					{
						Shader = static_cast<FShaderFactoryInterface*>(Library)->CreateHullShader(Key.SHAHash);
					}
				}

				if (!Shader.IsValid())
				{
					Shader = FShaderCodeLibrary::CreateHullShader(CurrentPlatform, Key.SHAHash, Code);
				}

				if (Shader.IsValid())
				{
					Shader->SetHash(Key.SHAHash);

					{
						FRWScopeLock Lock(ShaderCacheGlobalStateMutex, SLT_Write);
						CurrentShaderPlatformCache.Shaders.Add(Key);
						CachedHullShaders.Add(Key, Shader);
					}

					INC_DWORD_STAT(STATGROUP_NumShadersCached);
					InternalPrebindShader(Key, CacheState);
				}
			}
			break;
		}
		case SF_Domain:
		{
			FDomainShaderRHIRef Shader = nullptr;

			{
				FRWScopeLock Lock(ShaderCacheGlobalStateMutex, SLT_ReadOnly);
				Shader = CachedDomainShaders.FindRef(Key);
			}

			if (!Shader.IsValid())
			{
				if (Library != nullptr)
				{
					if (Library->IsNativeLibrary())
					{
						Shader = RHICreateDomainShader(Library, Key.SHAHash);
					}
					else
					{
						Shader = static_cast<FShaderFactoryInterface*>(Library)->CreateDomainShader(Key.SHAHash);
					}
				}

				if (!Shader.IsValid())
				{
					Shader = FShaderCodeLibrary::CreateDomainShader(CurrentPlatform, Key.SHAHash, Code);
				}

				if (Shader.IsValid())
				{
					Shader->SetHash(Key.SHAHash);

					{
						FRWScopeLock Lock(ShaderCacheGlobalStateMutex, SLT_Write);
						CurrentShaderPlatformCache.Shaders.Add(Key);
						CachedDomainShaders.Add(Key, Shader);
					}

					INC_DWORD_STAT(STATGROUP_NumShadersCached);
					InternalPrebindShader(Key, CacheState);
				}
			}
			break;
		}
		case SF_Compute:
		{
			bool const bCanPreBind = ShaderPlatformCanPrebindBoundShaderState(CurrentPlatform) || (CacheState && CacheState->CurrentNumRenderTargets > 0);
			if (!CachedComputeShaders.Find(Key) && bCanPreBind)
			{
				FComputeShaderRHIRef Shader = nullptr;

				{
					FRWScopeLock Lock(ShaderCacheGlobalStateMutex, SLT_ReadOnly);
					Shader = CachedComputeShaders.FindRef(Key);
				}

				if (!Shader.IsValid())
				{
					if (Library != nullptr)
					{
						if (Library->IsNativeLibrary())
						{
							Shader = RHICreateComputeShader(Library, Key.SHAHash);
						}
						else
						{
							Shader = static_cast<FShaderFactoryInterface*>(Library)->CreateComputeShader(Key.SHAHash);
						}
					}

					if (!Shader.IsValid())
					{
						Shader = FShaderCodeLibrary::CreateComputeShader(CurrentPlatform, Key.SHAHash, Code);
					}

					if (Shader.IsValid())
					{
						// @todo WARNING: The RHI is responsible for hashing Compute shaders, unlike other stages because of how OpenGLDrv implements compute.
						FShaderCacheKey ComputeKey = Key;
						ComputeKey.SHAHash = Shader->GetHash();

						{
							FRWScopeLock Lock(ShaderCacheGlobalStateMutex, SLT_Write);
							CurrentShaderPlatformCache.Shaders.Add(ComputeKey);
							CachedComputeShaders.Add(ComputeKey, Shader);
						}

						INC_DWORD_STAT(STATGROUP_NumShadersCached);
						InternalPrebindShader(ComputeKey, CacheState);
					}
				}
			}
			break;
		}
		default:
		{
			check(false);
			break;
		}
	}
}

FTextureRHIRef FShaderCache::InternalCreateTexture(FShaderTextureKey const& TextureKey, bool const bCached)
{
	FTextureRHIRef Tex = bCached ? CachedTextures.FindRef(TextureKey) : nullptr;
	
	if ( !IsValidRef(Tex) )
	{
		FRHIResourceCreateInfo Info;
		
		// Remove the presentable flag if present because it will be wrong during pre-draw and will cause some RHIs (e.g. Metal) to crash because it is invalid without an attached viewport.
		uint32 Flags = (TextureKey.Flags & ~(TexCreate_Presentable));
		
		switch(TextureKey.Type)
		{
			case SCTT_Texture2D:
			{
				Tex = RHICreateTexture2D(TextureKey.X, TextureKey.Y, TextureKey.Format, TextureKey.MipLevels, TextureKey.Samples, Flags, Info);
				break;
			}
			case SCTT_Texture2DArray:
			{
				Tex = RHICreateTexture2DArray(TextureKey.X, TextureKey.Y, TextureKey.Z, TextureKey.Format, TextureKey.MipLevels, Flags, Info);
				break;
			}
			case SCTT_Texture3D:
			{
				Tex = RHICreateTexture3D(TextureKey.X, TextureKey.Y, TextureKey.Z, TextureKey.Format, TextureKey.MipLevels, Flags, Info);
				break;
			}
			case SCTT_TextureCube:
			{
				Tex = RHICreateTextureCube(TextureKey.X, TextureKey.Format, TextureKey.MipLevels, Flags, Info);
				break;
			}
			case SCTT_TextureCubeArray:
			{
				Tex = RHICreateTextureCubeArray(TextureKey.X, TextureKey.Z, TextureKey.Format, TextureKey.MipLevels, Flags, Info);
				break;
			}
			case SCTT_Buffer:
			case SCTT_Texture1D:
			case SCTT_Texture1DArray:
			case SCTT_TextureExternal2D:
			default:
			{
				check(false);
				break;
			}
		}
	}
	return Tex;
}

FShaderTextureBinding FShaderCache::InternalCreateSRV(FShaderResourceKey const& ResourceKey)
{
	FShaderTextureBinding Binding = CachedSRVs.FindRef(ResourceKey);
	if ( !IsValidRef(Binding.SRV) )
	{
		FShaderTextureKey const& TextureKey = ResourceKey.Tex;
		switch(TextureKey.Type)
		{
			case SCTT_Buffer:
			{
				FRHIResourceCreateInfo Info;
				Binding.VertexBuffer = RHICreateVertexBuffer(TextureKey.X, TextureKey.Y, Info);
				Binding.SRV = RHICreateShaderResourceView(Binding.VertexBuffer, TextureKey.Z, TextureKey.Format);
				break;
			}
			case SCTT_Texture2D:
			{
				Binding.Texture = InternalCreateTexture(TextureKey, true);
				
				if(ResourceKey.Format == PF_Unknown)
				{
					Binding.SRV = RHICreateShaderResourceView(Binding.Texture->GetTexture2D(), ResourceKey.BaseMip);
				}
				else
				{
					// Make sure that the mip count is valid.
					uint32 NumMips = (Binding.Texture->GetNumMips() - ResourceKey.BaseMip);
					if(ResourceKey.MipLevels > 0)
					{
						NumMips = FMath::Min(NumMips, ResourceKey.MipLevels);
					}
					
					Binding.SRV = RHICreateShaderResourceView(Binding.Texture->GetTexture2D(), ResourceKey.BaseMip, NumMips, ResourceKey.Format);
				}
				break;
			}
			default:
			{
				check(false);
				break;
			}
		}
	}
	
	return Binding;
}


FTextureRHIRef FShaderCache::InternalCreateRenderTarget(FShaderRenderTargetKey const& TargetKey)
{
	FTextureRHIRef Texture;
	if( TargetKey.Texture.Format != PF_Unknown )
	{
		Texture = PredrawRTs.FindRef(TargetKey);
		if ( !IsValidRef(Texture) )
		{
			Texture = InternalCreateTexture(TargetKey.Texture, false);
			PredrawRTs.Add(TargetKey, Texture);
		}
	}
	return Texture;
}

template <typename TShaderRHIRef>
void FShaderCache::InternalSetShaderSamplerTextures( FRHICommandList& RHICmdList, FShaderDrawKey const& DrawKey, EShaderFrequency Frequency, TShaderRHIRef Shader, bool bClear )
{
	for ( uint32 i = 0; i < MaxTextureSamplers; i++ )
	{
		checkf(DrawKey.SamplerStates[Frequency][i] != EShaderCacheInvalidState, TEXT("Resource state cannot be 'InvalidState' as that indicates a resource lifetime error in the application."));
		
		if ( DrawKey.SamplerStates[Frequency][i] != EShaderCacheNullState )
		{
			FSamplerStateInitializerRHI SamplerInit = CurrentShaderPlatformCache.SamplerStates[DrawKey.SamplerStates[Frequency][i]];
			FSamplerStateRHIRef State = RHICreateSamplerState(SamplerInit);
			RHICmdList.SetShaderSampler(Shader, i, State);
		}
	}
	
	for ( uint32 i = 0; i < MaxResources; i++ )
	{
		checkf(DrawKey.Resources[Frequency][i] != EShaderCacheInvalidState, TEXT("Resource state cannot be 'InvalidState' as that indicates a resource lifetime error in the application."));
		
		FShaderTextureBinding Bind;
		if ( DrawKey.Resources[Frequency][i] != EShaderCacheNullState )
		{
			FShaderResourceKey Resource = CurrentShaderPlatformCache.Resources[DrawKey.Resources[Frequency][i]];
			if( Resource.bSRV == false )
			{
				if ( !bClear && Resource.Tex.Type != SCTT_Invalid )
				{
					Bind.Texture = InternalCreateTexture(Resource.Tex, true);
					RHICmdList.SetShaderTexture(Shader, i, Bind.Texture.GetReference());
				}
				else
				{
					RHICmdList.SetShaderTexture(Shader, i, nullptr);
				}
			}
			else
			{
				if ( !bClear )
				{
					Bind = InternalCreateSRV(Resource);
					RHICmdList.SetShaderResourceViewParameter(Shader, i, Bind.SRV.GetReference());
				}
				else
				{
					RHICmdList.SetShaderResourceViewParameter(Shader, i, nullptr);
				}
			}
		}
		else
		{
			RHICmdList.SetShaderTexture(Shader, i, nullptr);
		}
		
		if ( IsValidRef(Bind.Texture) || IsValidRef(Bind.SRV) )
		{
			PredrawBindings.Add(Bind);
		}
	}
}

void FShaderCache::InternalPreDrawShader(FRHICommandList& RHICmdList, FShaderCacheBoundState const& Shader, TSet<int32> const& DrawStates)
{
	if (ShaderPlatformPSOOnly(CurrentPlatform))
	{
		// Modern path for PSO-clean RHIs like MetalRHI.
		for ( auto PreDrawKeyIdx : DrawStates )
		{
			FShaderPreDrawEntry& Entry = CurrentShaderPlatformCache.PreDrawEntries[PreDrawKeyIdx];
			if(Entry.bPredrawn)
			{
				continue;
			}
			
			FShaderCacheGraphicsPipelineState& PSOState = CurrentShaderPlatformCache.PipelineStates[Entry.PSOIndex];
			if (!GraphicsPSOs.Find(PSOState))
			{
				FVertexShaderRHIRef VertexShader = Shader.VertexShader.bActive ? CachedVertexShaders.FindRef(Shader.VertexShader) : nullptr;
				FPixelShaderRHIRef PixelShader = Shader.PixelShader.bActive ? CachedPixelShaders.FindRef(Shader.PixelShader) : nullptr;
				FGeometryShaderRHIRef GeometryShader = Shader.GeometryShader.bActive ? CachedGeometryShaders.FindRef(Shader.GeometryShader) : nullptr;
				FHullShaderRHIRef HullShader = Shader.HullShader.bActive ? CachedHullShaders.FindRef(Shader.HullShader) : nullptr;
				FDomainShaderRHIRef DomainShader = Shader.DomainShader.bActive ? CachedDomainShaders.FindRef(Shader.DomainShader) : nullptr;
				
				bool bOK = true;
				bOK &= (Shader.VertexShader.bActive == IsValidRef(VertexShader));
				bOK &= (Shader.PixelShader.bActive == IsValidRef(PixelShader));
				bOK &= (Shader.GeometryShader.bActive == IsValidRef(GeometryShader));
				bOK &= (Shader.HullShader.bActive == IsValidRef(HullShader));
				bOK &= (Shader.DomainShader.bActive == IsValidRef(DomainShader));
				
				if ( bOK )
				{
					FGraphicsPipelineStateInitializer Init;
					
					FVertexDeclarationRHIRef VertexDesc = RHICreateVertexDeclaration(Shader.VertexDeclaration);
					
					Init.BoundShaderState.VertexDeclarationRHI = VertexDesc;
					Init.NumSamples = PSOState.SampleCount;
					
					FBlendStateRHIRef BlendState = RHICreateBlendState(PSOState.BlendState);
					Init.BlendState = BlendState;
					
					FDepthStencilStateRHIRef DepthStencilState = RHICreateDepthStencilState(PSOState.DepthStencilState);
					Init.DepthStencilState = DepthStencilState;
					
					FRasterizerStateRHIRef RasterizerState = RHICreateRasterizerState(PSOState.RasterizerState);
					Init.RasterizerState = RasterizerState;
					
					Init.PrimitiveType = (EPrimitiveType)PSOState.PrimitiveType;
					
					if (PSOState.DepthStencilTarget != EShaderCacheNullState && PSOState.DepthStencilTarget != EShaderCacheInvalidState)
					{
						Init.DepthStencilTargetFormat = (EPixelFormat)PSOState.DepthStencilTarget;
						Init.DepthStencilTargetFlag = PSOState.DepthStencilTargetFlags;
						Init.DepthTargetLoadAction = (ERenderTargetLoadAction)PSOState.DepthLoad;
						Init.DepthTargetStoreAction = (ERenderTargetStoreAction)PSOState.DepthStore;
						Init.StencilTargetLoadAction = (ERenderTargetLoadAction)PSOState.StencilLoad;
						Init.StencilTargetStoreAction = (ERenderTargetStoreAction)PSOState.StencilStore;
					}
					else
					{
						Init.DepthStencilTargetFormat = PF_Unknown;
						Init.DepthStencilTargetFlag = 0;
						Init.DepthTargetLoadAction = ERenderTargetLoadAction::ENoAction;
						Init.DepthTargetStoreAction = ERenderTargetStoreAction::ENoAction;
						Init.StencilTargetLoadAction = ERenderTargetLoadAction::ENoAction;
						Init.StencilTargetStoreAction = ERenderTargetStoreAction::ENoAction;
					}
					
					Init.RenderTargetsEnabled = PSOState.ActiveRenderTargets;
					for ( uint32 i = 0; i < MaxSimultaneousRenderTargets; i++ )
					{
						if (PSOState.RenderTargets[i] != EShaderCacheNullState && PSOState.RenderTargets[i] != EShaderCacheInvalidState)
						{
							Init.RenderTargetFormats[i] = EPixelFormat(PSOState.RenderTargets[i]);
							if (Init.RenderTargetFormats[i] > PF_Unknown && Init.RenderTargetFormats[i] < PF_MAX)
							{
								Init.RenderTargetFlags[i] = PSOState.RenderTargetFlags[i];
								Init.RenderTargetLoadActions[i] = (ERenderTargetLoadAction)PSOState.RenderTargetLoad[i];
								Init.RenderTargetStoreActions[i] = (ERenderTargetStoreAction)PSOState.RenderTargetStore[i];
							}
							else
							{
								Init.RenderTargetFormats[i] = PF_Unknown;
							}
						}
					}
					
					//NOTE: manually log in predraw
					BlendStates.Add(Init.BlendState,PSOState.BlendState);
					DepthStencilStates.Add(Init.DepthStencilState,PSOState.DepthStencilState);
					RasterizerStates.Add(Init.RasterizerState,PSOState.RasterizerState);
					
					Init.BoundShaderState.VertexShaderRHI = VertexShader;
					Init.BoundShaderState.PixelShaderRHI = PixelShader;
					Init.BoundShaderState.HullShaderRHI = HullShader;
					Init.BoundShaderState.DomainShaderRHI = DomainShader;
					Init.BoundShaderState.GeometryShaderRHI = GeometryShader;
					
					FGraphicsPipelineStateRHIRef PSO = RHICreateGraphicsPipelineState(Init);
					
					int32 InfoId = CurrentShaderPlatformCache.PipelineStates.Add(PSOState);
					
					PSOState.Index = InfoId;
					
					GraphicsPSOs.Add(PSOState, PSO);
					GraphicsPSOStates.Add(PSO, PSOState);
					
					INC_DWORD_STAT(STATGROUP_NumStatesPredrawn);
					INC_DWORD_STAT(STATGROUP_TotalStatesPredrawn);
					Entry.bPredrawn = true;
				}
			}
			else
			{
				Entry.bPredrawn = true;
			}
		}
		
		INC_DWORD_STAT(STATGROUP_NumPredrawn);
		INC_DWORD_STAT(STATGROUP_TotalPredrawn);
	}
	else // OpenGL path that is used for all non-PSO clean RHIs.
	{
		FBoundShaderStateRHIRef ShaderBoundState = BoundShaderStates.FindRef( Shader );
		uint32 VertexBufferSize = 0;
		for( auto VertexDec : Shader.VertexDeclaration )
		{
			VertexBufferSize = VertexBufferSize > (uint32)(VertexDec.Stride + VertexDec.Offset) ? VertexBufferSize : (uint32)(VertexDec.Stride + VertexDec.Offset);
		}
		
		FRHIResourceCreateInfo Info;
		if ( VertexBufferSize > 0 && (( !IsValidRef(PredrawVB) || !IsValidRef(PredrawZVB) ) || PredrawVB->GetSize() < VertexBufferSize || PredrawZVB->GetSize() < VertexBufferSize) )
		{
			// Retain previous VBs for outstanding draws
			PredrawVBs.Add(PredrawVB);
			PredrawVBs.Add(PredrawZVB);
			
			PredrawVB = RHICreateVertexBuffer(VertexBufferSize, BUF_Static, Info);
			{
				void* Data = RHILockVertexBuffer(PredrawVB, 0, VertexBufferSize, RLM_WriteOnly);
				if ( Data )
				{
					FMemory::Memzero(Data, VertexBufferSize);
				}
				RHIUnlockVertexBuffer(PredrawVB);
			}
			PredrawZVB = RHICreateVertexBuffer(VertexBufferSize, BUF_Static|BUF_ZeroStride, Info);
			{
				void* Data = RHILockVertexBuffer(PredrawZVB, 0, VertexBufferSize, RLM_WriteOnly);
				if ( Data )
				{
					FMemory::Memzero(Data, VertexBufferSize);
				}
				RHIUnlockVertexBuffer(PredrawZVB);
			}
		}
		
		bool bWasBound = false;
		
		for ( auto PreDrawKeyIdx : DrawStates )
		{
			FShaderPreDrawEntry& Entry = CurrentShaderPlatformCache.PreDrawEntries[PreDrawKeyIdx];
			if(Entry.bPredrawn)
			{
				continue;
			}
			
			FShaderCacheGraphicsPipelineState& PSOState = CurrentShaderPlatformCache.PipelineStates[Entry.PSOIndex];
			FShaderDrawKey& DrawKey = CurrentShaderPlatformCache.DrawStates[Entry.DrawKeyIndex];
			
			FBlendStateRHIRef BlendState = RHICreateBlendState(PSOState.BlendState);
			FDepthStencilStateRHIRef DepthStencil = RHICreateDepthStencilState(PSOState.DepthStencilState);
			FRasterizerStateRHIRef Rasterizer = RHICreateRasterizerState(PSOState.RasterizerState);
			
			//NOTE: manually log in predraw
			BlendStates.Add(BlendState,PSOState.BlendState);
			DepthStencilStates.Add(DepthStencil,PSOState.DepthStencilState);
			RasterizerStates.Add(Rasterizer,PSOState.RasterizerState);
			
			uint32 NewNumRenderTargets = 0;
			FRHIRenderTargetView RenderTargets[MaxSimultaneousRenderTargets];
			
			bool bDepthStencilTarget = false;
			FRHIDepthRenderTargetView DepthStencilTarget;
			if (ShaderPlatformPrebindRequiresResource(CurrentPlatform))
			{
				for ( uint32 i = 0; i < MaxSimultaneousRenderTargets; i++ )
				{
					checkf(PSOState.RenderTargets[i] != EShaderCacheInvalidState, TEXT("Resource state cannot be 'InvalidState' as that indicates a resource lifetime error in the application."));
					
					if( PSOState.RenderTargets[i] != EShaderCacheNullState )
					{
						FShaderTextureBinding Bind;
						FShaderRenderTargetKey& RTKey = CurrentShaderPlatformCache.RenderTargets[PSOState.RenderTargets[i]];
						Bind.Texture = InternalCreateRenderTarget(RTKey);
						RenderTargets[i].MipIndex = Bind.Texture->GetNumMips() > RTKey.MipLevel ? RTKey.MipLevel : 0;
						RenderTargets[i].ArraySliceIndex = RTKey.ArrayIndex;
						PredrawBindings.Add(Bind);
						RenderTargets[i].Texture = Bind.Texture;
						NewNumRenderTargets++;
					}
					else
					{
						break;
					}
				}
				
				bDepthStencilTarget = (PSOState.DepthStencilTarget != EShaderCacheNullState);
				if ( bDepthStencilTarget )
				{
					checkf(PSOState.DepthStencilTarget != EShaderCacheInvalidState, TEXT("Resource state cannot be 'InvalidState' as that indicates a resource lifetime error in the application."));
					
					FShaderTextureBinding Bind;
					FShaderRenderTargetKey& RTKey = CurrentShaderPlatformCache.RenderTargets[PSOState.DepthStencilTarget];
					Bind.Texture = InternalCreateRenderTarget(RTKey);
					PredrawBindings.Add(Bind);
					DepthStencilTarget.Texture = Bind.Texture;
				}
			}
			else
			{
				for ( uint32 i = 0; i < MaxSimultaneousRenderTargets; i++ )
				{
					EPixelFormat PixelFormat = EPixelFormat(PSOState.RenderTargets[i]);
					if( PixelFormat > EPixelFormat::PF_Unknown )
					{
						FShaderTextureBinding Bind;
						FShaderRenderTargetKey RTKey;
						
						RTKey.Texture.Flags = (PSOState.RenderTargetFlags[i]);
						RTKey.Texture.Format = PixelFormat;
						RTKey.Texture.X = RTKey.Texture.Y = RTKey.Texture.MipLevels = 1;
						RTKey.Texture.Type = SCTT_Texture2D;
						
						Bind.Texture = InternalCreateRenderTarget(RTKey);
						RenderTargets[i].MipIndex = Bind.Texture->GetNumMips() > RTKey.MipLevel ? RTKey.MipLevel : 0;
						RenderTargets[i].ArraySliceIndex = RTKey.ArrayIndex;
						PredrawBindings.Add(Bind);
						RenderTargets[i].Texture = Bind.Texture;
						NewNumRenderTargets++;
					}
					else
					{
						break;
					}
				}
				
				bDepthStencilTarget = (PSOState.DepthStencilTarget > EPixelFormat::PF_Unknown);
				if ( bDepthStencilTarget )
				{
					FShaderTextureBinding Bind;
					FShaderRenderTargetKey RTKey;
					
					RTKey.Texture.Flags = PSOState.DepthStencilTargetFlags;
					RTKey.Texture.Format = PSOState.DepthStencilTarget ;
					RTKey.Texture.X = RTKey.Texture.Y = RTKey.Texture.MipLevels = 1;
					RTKey.Texture.Type = SCTT_Texture2D;
					
					Bind.Texture = InternalCreateRenderTarget(RTKey);
					PredrawBindings.Add(Bind);
					DepthStencilTarget.Texture = Bind.Texture;
				}
			}
			
			RHICmdList.SetRenderTargets(NewNumRenderTargets, RenderTargets, bDepthStencilTarget ? &DepthStencilTarget : nullptr, 0, nullptr);
			
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
			GraphicsPSOInit.BlendState = BlendState;
			GraphicsPSOInit.DepthStencilState = DepthStencil;
			GraphicsPSOInit.RasterizerState = Rasterizer;
			
			for( auto VertexDec : Shader.VertexDeclaration )
			{
				if ( VertexDec.Stride > 0 )
				{
					check(IsValidRef(PredrawVB));
					RHICmdList.SetStreamSource(VertexDec.StreamIndex, PredrawVB, VertexDec.Offset);
				}
				else
				{
					check(IsValidRef(PredrawZVB));
					RHICmdList.SetStreamSource(VertexDec.StreamIndex, PredrawZVB, VertexDec.Offset);
				}
			}
			
			if ( !IsValidRef(ShaderBoundState) )
			{
				FVertexShaderRHIRef VertexShader = Shader.VertexShader.bActive ? CachedVertexShaders.FindRef(Shader.VertexShader) : nullptr;
				FPixelShaderRHIRef PixelShader = Shader.PixelShader.bActive ? CachedPixelShaders.FindRef(Shader.PixelShader) : nullptr;
				FGeometryShaderRHIRef GeometryShader = Shader.GeometryShader.bActive ? CachedGeometryShaders.FindRef(Shader.GeometryShader) : nullptr;
				FHullShaderRHIRef HullShader = Shader.HullShader.bActive ? CachedHullShaders.FindRef(Shader.HullShader) : nullptr;
				FDomainShaderRHIRef DomainShader = Shader.DomainShader.bActive ? CachedDomainShaders.FindRef(Shader.DomainShader) : nullptr;
				
				bool bOK = true;
				bOK &= (Shader.VertexShader.bActive == IsValidRef(VertexShader));
				bOK &= (Shader.PixelShader.bActive == IsValidRef(PixelShader));
				bOK &= (Shader.GeometryShader.bActive == IsValidRef(GeometryShader));
				bOK &= (Shader.HullShader.bActive == IsValidRef(HullShader));
				bOK &= (Shader.DomainShader.bActive == IsValidRef(DomainShader));
				
				if ( bOK )
				{
					FVertexDeclarationRHIRef VertexDeclaration = RHICreateVertexDeclaration(Shader.VertexDeclaration);
					bOK &= IsValidRef(VertexDeclaration);
					
					if ( bOK )
					{
						ShaderBoundState = RHICreateBoundShaderState( VertexDeclaration,
																	 VertexShader,
																	 HullShader,
																	 DomainShader,
																	 PixelShader,
																	 GeometryShader );
					}
				}
			}
			
			if ( IsValidRef( ShaderBoundState ) )
			{
				bWasBound = true;
				
				//NOTE: Urgent refactor - We should be caching PSOInit type objects and/or objects to contruct PSO from
				FVertexShaderRHIRef VertexShader = Shader.VertexShader.bActive ? CachedVertexShaders.FindRef(Shader.VertexShader) : nullptr;
				FPixelShaderRHIRef PixelShader = Shader.PixelShader.bActive ? CachedPixelShaders.FindRef(Shader.PixelShader) : nullptr;
				FGeometryShaderRHIRef GeometryShader = Shader.GeometryShader.bActive ? CachedGeometryShaders.FindRef(Shader.GeometryShader) : nullptr;
				FHullShaderRHIRef HullShader = Shader.HullShader.bActive ? CachedHullShaders.FindRef(Shader.HullShader) : nullptr;
				FDomainShaderRHIRef DomainShader = Shader.DomainShader.bActive ? CachedDomainShaders.FindRef(Shader.DomainShader) : nullptr;
				FVertexDeclarationRHIRef VertexDeclaration = RHICreateVertexDeclaration(Shader.VertexDeclaration);
				
				bool bOK = true;
				bOK &= (Shader.VertexShader.bActive == IsValidRef(VertexShader));
				bOK &= (Shader.PixelShader.bActive == IsValidRef(PixelShader));
				bOK &= (Shader.GeometryShader.bActive == IsValidRef(GeometryShader));
				bOK &= (Shader.HullShader.bActive == IsValidRef(HullShader));
				bOK &= (Shader.DomainShader.bActive == IsValidRef(DomainShader));
				bOK &= IsValidRef(VertexDeclaration);
				
				if(bOK)
				{
					GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = VertexDeclaration;
					GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader;
					GraphicsPSOInit.BoundShaderState.HullShaderRHI = HullShader;
					GraphicsPSOInit.BoundShaderState.DomainShaderRHI = DomainShader;
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader;
					GraphicsPSOInit.BoundShaderState.GeometryShaderRHI = GeometryShader;
					
					GraphicsPSOInit.PrimitiveType = PT_TriangleList;
					
					SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
					
					bWasBound = true;
				}
			}
			else
			{
				break;
			}
			
			if (ShaderPlatformPrebindRequiresResource(CurrentPlatform))
			{
				if ( Shader.VertexShader.bActive )
				{
					InternalSetShaderSamplerTextures(RHICmdList, DrawKey, SF_Vertex, CachedVertexShaders.FindRef(Shader.VertexShader).GetReference());
				}
				if ( Shader.PixelShader.bActive )
				{
					InternalSetShaderSamplerTextures(RHICmdList, DrawKey, SF_Pixel, CachedPixelShaders.FindRef(Shader.PixelShader).GetReference());
				}
				if ( Shader.GeometryShader.bActive )
				{
					InternalSetShaderSamplerTextures(RHICmdList, DrawKey, SF_Geometry, CachedGeometryShaders.FindRef(Shader.GeometryShader).GetReference());
				}
				if ( Shader.HullShader.bActive )
				{
					InternalSetShaderSamplerTextures(RHICmdList, DrawKey, SF_Hull, CachedHullShaders.FindRef(Shader.HullShader).GetReference());
				}
				if ( Shader.DomainShader.bActive )
				{
					InternalSetShaderSamplerTextures(RHICmdList, DrawKey, SF_Domain, CachedDomainShaders.FindRef(Shader.DomainShader).GetReference());
				}
			}
			
			switch ( DrawKey.IndexType )
			{
				case 0:
				{
					RHICmdList.DrawPrimitive(PT_TriangleList, 0, 1, 1);
					break;
				}
				case 2:
				{
					RHICmdList.DrawIndexedPrimitive(IndexBufferUInt16, PT_TriangleList, 0, 0, 3, 0, 1, 1);
					break;
				}
				case 4:
				{
					RHICmdList.DrawIndexedPrimitive(IndexBufferUInt32, PT_TriangleList, 0, 0, 3, 0, 1, 1);
					break;
				}
				default:
				{
					break;
				}
			}
			INC_DWORD_STAT(STATGROUP_NumStatesPredrawn);
			INC_DWORD_STAT(STATGROUP_TotalStatesPredrawn);
		}
		
		if (bWasBound && IsValidRef(ShaderBoundState) && DrawStates.Num() && ShaderPlatformPrebindRequiresResource(CurrentPlatform))
		{
			FShaderCacheState const * CacheState = InternalCreateOrFindCacheStateForContext(&RHICmdList.GetContext());
			
			if ( Shader.VertexShader.bActive )
			{
				InternalSetShaderSamplerTextures(RHICmdList, CacheState->CurrentDrawKey, SF_Vertex, CachedVertexShaders.FindRef(Shader.VertexShader).GetReference(), true);
			}
			if ( Shader.PixelShader.bActive )
			{
				InternalSetShaderSamplerTextures(RHICmdList, CacheState->CurrentDrawKey, SF_Pixel, CachedPixelShaders.FindRef(Shader.PixelShader).GetReference(), true);
			}
			if ( Shader.GeometryShader.bActive )
			{
				InternalSetShaderSamplerTextures(RHICmdList, CacheState->CurrentDrawKey, SF_Geometry, CachedGeometryShaders.FindRef(Shader.GeometryShader).GetReference(), true);
			}
			if ( Shader.HullShader.bActive )
			{
				InternalSetShaderSamplerTextures(RHICmdList, CacheState->CurrentDrawKey, SF_Hull, CachedHullShaders.FindRef(Shader.HullShader).GetReference(), true);
			}
			if ( Shader.DomainShader.bActive )
			{
				InternalSetShaderSamplerTextures(RHICmdList, CacheState->CurrentDrawKey, SF_Domain, CachedDomainShaders.FindRef(Shader.DomainShader).GetReference(), true);
			}
		}
		
		for( auto VertexDec : Shader.VertexDeclaration )
		{
			RHICmdList.SetStreamSource(VertexDec.StreamIndex, nullptr, 0);
		}
		
		INC_DWORD_STAT(STATGROUP_NumPredrawn);
		INC_DWORD_STAT(STATGROUP_TotalPredrawn);
	}
}

int32 FShaderCache::GetPredrawBatchTime() const
{
	return OverridePredrawBatchTime == 0 ? PredrawBatchTime : OverridePredrawBatchTime;
}

int32 FShaderCache::GetTargetPrecompileFrameTime() const
{
	return OverridePrecompileTime == 0 ? TargetPrecompileFrameTime : OverridePrecompileTime;
}

bool FShaderCache::LoadShaderCache(FString Path, FShaderPlatformCache& InCache)
{
	bool bLoadedCache = false;
	if ( IFileManager::Get().FileSize(*Path) > 0 )
	{
		FArchive* BinaryShaderAr = IFileManager::Get().CreateFileReader(*Path);
		
		if ( BinaryShaderAr != nullptr )
		{
			*BinaryShaderAr << InCache;
			
			bool const bNoError = !BinaryShaderAr->IsError();
			bool const bMatchedCustomLatest = BinaryShaderAr->CustomVer(FShaderCacheCustomVersion::Key) == FShaderCacheCustomVersion::Latest;
			bool const bMatchedGameVersion = BinaryShaderAr->CustomVer(FShaderCacheCustomVersion::GameKey) == FShaderCache::GameVersion;
			
			bLoadedCache = ( bNoError && bMatchedCustomLatest && bMatchedGameVersion );
			
			if (!bLoadedCache)
			{
				IFileManager::Get().Delete(*Path);
			}
			
			delete BinaryShaderAr;
		}
	}
	return bLoadedCache;
}

bool FShaderCache::SaveShaderCache(FString Path, FShaderPlatformCache& InCache)
{
	UE_LOG(LogRHI, Log, TEXT("Saving shader cache: %s"), *Path);

	FArchive* BinaryShaderAr = IFileManager::Get().CreateFileWriter(*Path);
	if( BinaryShaderAr != NULL )
	{
		*BinaryShaderAr << InCache;
		BinaryShaderAr->Flush();
		delete BinaryShaderAr;
		return true;
	}
	
	return false;
}

FShaderCacheState* FShaderCache::InternalCreateOrFindCacheStateForContext(const IRHICommandContext* Context)
{
	FShaderCacheState* Result = nullptr;
	
	{
		FRWScopeLock Lock(ContextCacheStatesMutex,SLT_Write);
		
		FShaderCacheState** CacheStateRef = ContextCacheStates.Find(Context);
		
		if(CacheStateRef != nullptr)
		{
			Result = *CacheStateRef;
		}
		else
		{
			Result = new FShaderCacheState;
			ContextCacheStates.Add(Context,Result);
		}
		
		check(Result);
	}
	
	return Result;
}

void FShaderCache::InternalRemoveCacheStateForContext(const IRHICommandContext* Context)
{
	FRWScopeLock Lock(ContextCacheStatesMutex,SLT_Write);
	ContextCacheStates.Remove(Context);
}

