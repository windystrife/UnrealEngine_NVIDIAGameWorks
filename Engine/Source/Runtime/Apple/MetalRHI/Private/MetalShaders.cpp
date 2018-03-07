// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalShaders.cpp: Metal shader RHI implementation.
=============================================================================*/

#include "MetalRHIPrivate.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "MetalShaderResources.h"
#include "MetalResources.h"
#include "ShaderCache.h"
#include "MetalProfiler.h"
#include "MetalCommandBuffer.h"
#include "Serialization/MemoryReader.h"
#include "Misc/FileHelper.h"
#include "ScopeRWLock.h"
#include "Misc/Compression.h"
#include "Misc/MessageDialog.h"

#define SHADERCOMPILERCOMMON_API
#	include "Developer/ShaderCompilerCommon/Public/ShaderCompilerCommon.h"
#undef SHADERCOMPILERCOMMON_API

/** Set to 1 to enable shader debugging (makes the driver save the shader source) */
#define DEBUG_METAL_SHADERS (UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT)

static FString METAL_LIB_EXTENSION(TEXT(".metallib"));
static FString METAL_MAP_EXTENSION(TEXT(".metalmap"));

class FMetalCompiledShaderKey
{
public:
	FMetalCompiledShaderKey(
		uint32 InCodeSize,
		uint32 InCodeCRC
		)
		: CodeSize(InCodeSize)
		, CodeCRC(InCodeCRC)
	{}

	friend bool operator ==(const FMetalCompiledShaderKey& A, const FMetalCompiledShaderKey& B)
	{
		return A.CodeSize == B.CodeSize && A.CodeCRC == B.CodeCRC;
	}

	friend uint32 GetTypeHash(const FMetalCompiledShaderKey &Key)
	{
		return HashCombine(GetTypeHash(Key.CodeSize), GetTypeHash(Key.CodeCRC));
	}

private:
	uint32 CodeSize;
	uint32 CodeCRC;
};

struct FMetalCompiledShaderCache
{
public:
	FMetalCompiledShaderCache()
	{
	}
	
	~FMetalCompiledShaderCache()
	{
		for (TPair<FMetalCompiledShaderKey, id<MTLFunction>> Pair : Cache)
		{
			[Pair.Value release];
		}
	}
	
	id<MTLFunction> FindRef(FMetalCompiledShaderKey Key)
	{
		FRWScopeLock(Lock, SLT_ReadOnly);
		id<MTLFunction> Func = Cache.FindRef(Key);
		return Func;
	}
	
	void Add(FMetalCompiledShaderKey Key, id<MTLFunction> Function)
	{
		FRWScopeLock(Lock, SLT_Write);
		Cache.Add(Key, Function);
	}
	
private:
	FRWLock Lock;
	TMap<FMetalCompiledShaderKey, id<MTLFunction>> Cache;
};

static FMetalCompiledShaderCache& GetMetalCompiledShaderCache()
{
	static FMetalCompiledShaderCache CompiledShaderCache;
	return CompiledShaderCache;
}

NSString* DecodeMetalSourceCode(uint32 CodeSize, TArray<uint8> const& CompressedSource)
{
	NSString* GlslCodeNSString = nil;
	if (CodeSize && CompressedSource.Num())
	{
		TArray<ANSICHAR> UncompressedCode;
		UncompressedCode.AddZeroed(CodeSize+1);
		bool bSucceed = FCompression::UncompressMemory(ECompressionFlags::COMPRESS_ZLIB, UncompressedCode.GetData(), CodeSize, CompressedSource.GetData(), CompressedSource.Num());
		if (bSucceed)
		{
			GlslCodeNSString = [[NSString stringWithUTF8String:UncompressedCode.GetData()] retain];
		}
	}
	if (!GlslCodeNSString)
	{
		GlslCodeNSString = [@"Offline" retain];
	}
	return GlslCodeNSString;
}

static MTLLanguageVersion ValidateVersion(uint8 Version)
{
	static uint32 MetalMacOSVersions[][3] = {
		{10,11,6},
		{10,11,6},
		{10,12,6},
		{10,13,0},
	};
	static uint32 MetaliOSVersions[][3] = {
		{8,0,0},
		{9,0,0},
		{10,0,0},
		{11,0,0},
	};
	static TCHAR const* StandardNames[] =
	{
		TEXT("Metal 1.0"),
		TEXT("Metal 1.1"),
		TEXT("Metal 1.2"),
		TEXT("Metal 2.0"),
	};
	
	Version = FMath::Min(Version, (uint8)3);
	
	MTLLanguageVersion Result = MTLLanguageVersion1_1;
	if (Version < 3)
	{
#if PLATFORM_MAC
		Result = Version == 0 ? MTLLanguageVersion1_1 : (MTLLanguageVersion)((1 << 16) + FMath::Min(Version, (uint8)2u));
#else
		Result = (MTLLanguageVersion)((1 << 16) + FMath::Min(Version, (uint8)2u));
#endif
	}
	else if (Version == 3)
	{
		Result = (MTLLanguageVersion)(2 << 16);
	}
	
	if (!FApplePlatformMisc::IsOSAtLeastVersion(MetalMacOSVersions[Version], MetaliOSVersions[Version], MetaliOSVersions[Version]))
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("ShaderVersion"), FText::FromString(FString(StandardNames[Version])));
#if PLATFORM_MAC
		Args.Add(TEXT("RequiredOS"), FText::FromString(FString::Printf(TEXT("macOS %d.%d.%d"), MetalMacOSVersions[Version][0], MetalMacOSVersions[Version][1], MetalMacOSVersions[Version][2])));
#else
		Args.Add(TEXT("RequiredOS"), FText::FromString(FString::Printf(TEXT("macOS %d.%d.%d"), MetaliOSVersions[Version][0], MetaliOSVersions[Version][1], MetaliOSVersions[Version][2])));
#endif
		FText LocalizedMsg = FText::Format(NSLOCTEXT("MetalRHI", "ShaderVersionUnsupported","The current OS version does not support {Version} required by the project. You must upgrade to {RequiredOS} to run this project."),Args);
		
		FText Title = NSLOCTEXT("MetalRHI", "ShaderVersionUnsupported","Shader Version Unsupported");
		FMessageDialog::Open(EAppMsgType::Ok, LocalizedMsg, &Title);
		
		FPlatformMisc::RequestExit(true);
	}
	
	return Result;
}

/** Initialization constructor. */
template<typename BaseResourceType, int32 ShaderType>
void TMetalBaseShader<BaseResourceType, ShaderType>::Init(const TArray<uint8>& InShaderCode, FMetalCodeHeader& Header, id<MTLLibrary> InLibrary)
{
	FShaderCodeReader ShaderCode(InShaderCode);

	FMemoryReader Ar(InShaderCode, true);
	
	Ar.SetLimitSize(ShaderCode.GetActualShaderCodeSize());

	// was the shader already compiled offline?
	uint8 OfflineCompiledFlag;
	Ar << OfflineCompiledFlag;
	check(OfflineCompiledFlag == 0 || OfflineCompiledFlag == 1);

	// get the header
	Header = { 0 };
	Ar << Header;
	
	ValidateVersion(Header.Version);
	
	// Validate that the compiler flags match the offline compiled flag - somehow they sometimes don't..
	checkf((Header.CompileFlags & (1 << CFLAG_Debug)) == ((!OfflineCompiledFlag) << CFLAG_Debug), TEXT("Header: 0x%x, Offline: 0x%x, 0x%x"), Header.CompileFlags, OfflineCompiledFlag, !OfflineCompiledFlag);
	
    SourceLen = Header.SourceLen;
    SourceCRC = Header.SourceCRC;

	// remember where the header ended and code (precompiled or source) begins
	int32 CodeOffset = Ar.Tell();
	uint32 BufferSize = ShaderCode.GetActualShaderCodeSize() - CodeOffset;
	const ANSICHAR* SourceCode = (ANSICHAR*)InShaderCode.GetData() + CodeOffset;

	// Only archived shaders should be in here.
	UE_CLOG(InLibrary && !(Header.CompileFlags & (1 << CFLAG_Archive)), LogMetal, Warning, TEXT("Shader being loaded wasn't marked for archiving but a MTLLibrary was provided - this is unsupported."));
	
	if (!OfflineCompiledFlag)
	{
		UE_LOG(LogMetal, Display, TEXT("Loaded a text shader (will be slower to load)"));
	}
	
	FMetalCompiledShaderKey Key(Header.SourceLen, Header.SourceCRC);

	bool bOfflineCompile = (OfflineCompiledFlag > 0);
	
	const ANSICHAR* ShaderSource = ShaderCode.FindOptionalData('c');
	bool bHasShaderSource = (ShaderSource && FCStringAnsi::Strlen(ShaderSource) > 0);
    
    static bool bForceTextShaders = FMetalCommandQueue::SupportsFeature(EMetalFeaturesGPUTrace);
    if (!bHasShaderSource)
    {
        int32 LZMASourceSize = 0;
        int32 SourceSize = 0;
        const uint8* LZMASource = ShaderCode.FindOptionalDataAndSize('z', LZMASourceSize);
        const uint8* UnSourceLen = ShaderCode.FindOptionalDataAndSize('u', SourceSize);
        if (LZMASource && LZMASourceSize > 0 && UnSourceLen && SourceSize == sizeof(uint32))
        {
            CompressedSource.Append(LZMASource, LZMASourceSize);
            memcpy(&CodeSize, UnSourceLen, sizeof(uint32));
			bHasShaderSource = false;
        }
        if (bForceTextShaders)
        {
            bHasShaderSource = (GetSourceCode() != nil);
        }
    }
    else if (bOfflineCompile && bHasShaderSource)
	{
		GlslCodeNSString = [NSString stringWithUTF8String:ShaderSource];
		check(GlslCodeNSString);
		[GlslCodeNSString retain];
	}
	
	// Find the existing compiled shader in the cache.
	if (!Header.bFunctionConstants)
	{
		Function = [GetMetalCompiledShaderCache().FindRef(Key) retain];
	}
	if (!Function)
	{
		if (bOfflineCompile && bHasShaderSource)
		{
			// For debug/dev/test builds we can use the stored code for debugging - but shipping builds shouldn't have this as it is inappropriate.
#if METAL_DEBUG_OPTIONS
			// For iOS/tvOS we must use runtime compilation to make the shaders debuggable, but
			bool bSavedSource = false;
			
#if PLATFORM_MAC
			const ANSICHAR* ShaderPath = ShaderCode.FindOptionalData('p');		
			bool const bHasShaderPath = (ShaderPath && FCStringAnsi::Strlen(ShaderPath) > 0);
			
			// on Mac if we have a path for the shader we can access the shader code
			if (bHasShaderPath && !bForceTextShaders && (GetSourceCode() != nil))
			{
				FString ShaderPathString(ShaderPath);
				
				if (IFileManager::Get().MakeDirectory(*FPaths::GetPath(ShaderPathString), true))
				{
					FString Source(GetSourceCode());
					bSavedSource = FFileHelper::SaveStringToFile(Source, *ShaderPathString);
				}
				
				static bool bAttemptedAuth = false;
				if (!bSavedSource && !bAttemptedAuth)
				{
					bAttemptedAuth = true;
					
					if (IFileManager::Get().MakeDirectory(*FPaths::GetPath(ShaderPathString), true))
					{
						bSavedSource = FFileHelper::SaveStringToFile(FString(GlslCodeNSString), *ShaderPathString);
					}
					
					if (!bSavedSource)
					{
						FPlatformMisc::MessageBoxExt(EAppMsgType::Ok,
													*NSLOCTEXT("MetalRHI", "ShaderDebugAuthFail", "Could not access directory required for debugging optimised Metal shaders. Falling back to slower runtime compilation of shaders for debugging.").ToString(), TEXT("Error"));
					}
				}
			}
#endif
			// Switch the compile mode so we get debuggable shaders even if we failed to save - if we didn't want
			// shader debugging we wouldn't have included the code...
			bOfflineCompile = bSavedSource || (bOfflineCompile && !bForceTextShaders);
#endif
		}

		if (bOfflineCompile METAL_DEBUG_OPTION(&& !(bHasShaderSource && bForceTextShaders)))
		{
			if (InLibrary)
			{
				Library = [InLibrary retain];
			}
			else
			{
				// Archived shaders should never get in here.
				check(!(Header.CompileFlags & (1 << CFLAG_Archive)) || BufferSize > 0);
				
				// allow GCD to copy the data into its own buffer
				//		dispatch_data_t GCDBuffer = dispatch_data_create(InShaderCode.GetTypedData() + CodeOffset, ShaderCode.GetActualShaderCodeSize() - CodeOffset, nil, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
				void* Buffer = FMemory::Malloc( BufferSize );
				FMemory::Memcpy( Buffer, InShaderCode.GetData() + CodeOffset, BufferSize );
				dispatch_data_t GCDBuffer = dispatch_data_create(Buffer, BufferSize, dispatch_get_main_queue(), ^(void) { FMemory::Free(Buffer); } );

				// load up the already compiled shader
				NSError* Error;
				Library = [GetMetalDeviceContext().GetDevice() newLibraryWithData:GCDBuffer error:&Error];
				if (Library == nil)
				{
					NSLog(@"Failed to create library: %@", Error);
				}
				
				dispatch_release(GCDBuffer);
			}
		}
		else
		{
			NSError* Error;
			NSString* ShaderString = ((OfflineCompiledFlag == 0) ? [NSString stringWithUTF8String:SourceCode] : GlslCodeNSString);

			if(Header.ShaderName.Len())
			{
				ShaderString = [NSString stringWithFormat:@"// %@\n%@", Header.ShaderName.GetNSString(), ShaderString];
			}
			
			MTLCompileOptions *CompileOptions = [[MTLCompileOptions alloc] init];
			CompileOptions.fastMathEnabled = (BOOL)(!(Header.CompileFlags & (1 << CFLAG_NoFastMath)));
			if (GetMetalDeviceContext().SupportsFeature(EMetalFeaturesShaderVersions))
			{
                if (Header.Version < 3)
                {
#if PLATFORM_MAC
                    CompileOptions.languageVersion = Header.Version == 0 ? MTLLanguageVersion1_1 : (MTLLanguageVersion)((1 << 16) + FMath::Min(Header.Version, (uint8)2u));
#else
                    CompileOptions.languageVersion = (MTLLanguageVersion)((1 << 16) + FMath::Min(Header.Version, (uint8)2u));
#endif
                }
                else if (Header.Version == 3)
                {
                    CompileOptions.languageVersion = (MTLLanguageVersion)(2 << 16);
                }
			}
#if DEBUG_METAL_SHADERS
			NSDictionary *PreprocessorMacros = @{ @"MTLSL_ENABLE_DEBUG_INFO" : @(1)};
			[CompileOptions setPreprocessorMacros : PreprocessorMacros];
#endif
			Library = [GetMetalDeviceContext().GetDevice() newLibraryWithSource:ShaderString options : CompileOptions error : &Error];
			[CompileOptions release];

			if (Library == nil)
			{
				UE_LOG(LogRHI, Error, TEXT("*********** Error\n%s"), *FString(ShaderString));
				UE_LOG(LogRHI, Fatal, TEXT("Failed to create shader: %s"), *FString([Error description]));
			}
			else if (Error != nil)
			{
				// Warning...
				UE_LOG(LogRHI, Warning, TEXT("*********** Warning\n%s"), *FString(ShaderString));
				UE_LOG(LogRHI, Warning, TEXT("Created shader with warnings: %s"), *FString([Error description]));
			}

			GlslCodeNSString = ShaderString;
			[GlslCodeNSString retain];
		}

		if (!Header.bFunctionConstants)
		{
			// Get the function from the library - the function name is "Main" followed by the CRC32 of the source MTLSL as 0-padded hex.
			// This ensures that even if we move to a unified library that the function names will be unique - duplicates will only have one entry in the library.
			NSString* Name = [NSString stringWithFormat:@"Main_%0.8x_%0.8x", Header.SourceLen, Header.SourceCRC];
			Function = [[Library newFunctionWithName:Name] retain];
			check(Function);
			GetMetalCompiledShaderCache().Add(Key, Function);
			[Library release];
			Library = nil;
			TRACK_OBJECT(STAT_MetalFunctionCount, Function);
		}
	}

	Bindings = Header.Bindings;
	UniformBuffersCopyInfo = Header.UniformBuffersCopyInfo;
	SideTableBinding = Header.SideTable;
}

/** Destructor */
template<typename BaseResourceType, int32 ShaderType>
TMetalBaseShader<BaseResourceType, ShaderType>::~TMetalBaseShader()
{
	UNTRACK_OBJECT(STAT_MetalFunctionCount, Function);
	[Function release];
	[Library release];
	[GlslCodeNSString release];
}

FMetalComputeShader::FMetalComputeShader(const TArray<uint8>& InCode)
: Pipeline(nil)
, NumThreadsX(0)
, NumThreadsY(0)
, NumThreadsZ(0)
{
	FMetalCodeHeader Header = {0};
	Init(InCode, Header);
	
	NumThreadsX = FMath::Max((int32)Header.NumThreadsX, 1);
	NumThreadsY = FMath::Max((int32)Header.NumThreadsY, 1);
	NumThreadsZ = FMath::Max((int32)Header.NumThreadsZ, 1);
	
	NSError* Error;
	
	id<MTLComputePipelineState> Kernel = nil;
	MTLComputePipelineReflection* Reflection = nil;
	
#if METAL_DEBUG_OPTIONS
	if (GetMetalDeviceContext().GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelFastValidation)
	{
		MTLAutoreleasedComputePipelineReflection ComputeReflection;
		Kernel = [[GetMetalDeviceContext().GetDevice() newComputePipelineStateWithFunction:Function options:MTLPipelineOptionArgumentInfo reflection:&ComputeReflection error:&Error] autorelease];
		Reflection = ComputeReflection;
	}
	else
#endif
	{
		Kernel = [[GetMetalDeviceContext().GetDevice() newComputePipelineStateWithFunction:Function error:&Error] autorelease];
	}
	
	if (Kernel == nil)
	{
        UE_LOG(LogRHI, Error, TEXT("*********** Error\n%s"), *FString(GetSourceCode()));
        UE_LOG(LogRHI, Fatal, TEXT("Failed to create compute kernel: %s"), *FString([Error description]));
	}
	
	Pipeline = [FMetalShaderPipeline new];
	Pipeline.ComputePipelineState = Kernel;
#if METAL_DEBUG_OPTIONS
    Pipeline.ComputePipelineReflection = Reflection;
	Pipeline.ComputeSource = GetSourceCode();
#endif
	METAL_DEBUG_OPTION(FMemory::Memzero(Pipeline->ResourceMask, sizeof(Pipeline->ResourceMask)));
	TRACK_OBJECT(STAT_MetalComputePipelineStateCount, Pipeline);
}

FMetalComputeShader::FMetalComputeShader(const TArray<uint8>& InCode, id<MTLLibrary> InLibrary)
: Pipeline(nil)
, NumThreadsX(0)
, NumThreadsY(0)
, NumThreadsZ(0)
{
	FMetalCodeHeader Header = {0};
	Init(InCode, Header, InLibrary);
	
	NumThreadsX = FMath::Max((int32)Header.NumThreadsX, 1);
	NumThreadsY = FMath::Max((int32)Header.NumThreadsY, 1);
	NumThreadsZ = FMath::Max((int32)Header.NumThreadsZ, 1);
	
	if (Function)
	{
		NSError* Error;
		
		id<MTLComputePipelineState> Kernel = nil;
		MTLComputePipelineReflection* Reflection = nil;
		
	#if METAL_DEBUG_OPTIONS
		if (GetMetalDeviceContext().GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelFastValidation)
		{
			MTLAutoreleasedComputePipelineReflection ComputeReflection;
			Kernel = [[GetMetalDeviceContext().GetDevice() newComputePipelineStateWithFunction:Function options:MTLPipelineOptionArgumentInfo reflection:&ComputeReflection error:&Error] autorelease];
			Reflection = ComputeReflection;
		}
		else
	#endif
		{
			Kernel = [[GetMetalDeviceContext().GetDevice() newComputePipelineStateWithFunction:Function error:&Error] autorelease];
		}
		
		if (Kernel == nil)
		{
			UE_LOG(LogRHI, Error, TEXT("*********** Error\n%s"), *FString(GetSourceCode()));
			UE_LOG(LogRHI, Fatal, TEXT("Failed to create compute kernel: %s"), *FString([Error description]));
		}
		
		Pipeline = [FMetalShaderPipeline new];
		Pipeline.ComputePipelineState = Kernel;
#if METAL_DEBUG_OPTIONS
        Pipeline.ComputePipelineReflection = Reflection;
		Pipeline.ComputeSource = GetSourceCode();
#endif
		METAL_DEBUG_OPTION(FMemory::Memzero(Pipeline->ResourceMask, sizeof(Pipeline->ResourceMask)));
		TRACK_OBJECT(STAT_MetalComputePipelineStateCount, Pipeline);
	}
}

FMetalComputeShader::~FMetalComputeShader()
{
	if (Pipeline)
	{
		UNTRACK_OBJECT(STAT_MetalComputePipelineStateCount, Pipeline);
	}
	[Pipeline release];
	Pipeline = nil;
}

FMetalVertexShader::FMetalVertexShader(const TArray<uint8>& InCode)
{
	FMetalCodeHeader Header = {0};
	Init(InCode, Header);
	
	TessellationOutputAttribs = Header.TessellationOutputAttribs;
	TessellationPatchCountBuffer = Header.TessellationPatchCountBuffer;
	TessellationIndexBuffer = Header.TessellationIndexBuffer;
	TessellationHSOutBuffer = Header.TessellationHSOutBuffer;
	TessellationHSTFOutBuffer = Header.TessellationHSTFOutBuffer;
	TessellationControlPointOutBuffer = Header.TessellationControlPointOutBuffer;
	TessellationControlPointIndexBuffer = Header.TessellationControlPointIndexBuffer;
	TessellationOutputControlPoints = Header.TessellationOutputControlPoints;
	TessellationDomain = Header.TessellationDomain;
	TessellationInputControlPoints = Header.TessellationInputControlPoints;
	TessellationMaxTessFactor = Header.TessellationMaxTessFactor;
	TessellationPatchesPerThreadGroup = Header.TessellationPatchesPerThreadGroup;
}

FMetalVertexShader::FMetalVertexShader(const TArray<uint8>& InCode, id<MTLLibrary> InLibrary)
{
	FMetalCodeHeader Header = {0};
	Init(InCode, Header, InLibrary);
	
	TessellationOutputAttribs = Header.TessellationOutputAttribs;
	TessellationPatchCountBuffer = Header.TessellationPatchCountBuffer;
	TessellationIndexBuffer = Header.TessellationIndexBuffer;
	TessellationHSOutBuffer = Header.TessellationHSOutBuffer;
	TessellationHSTFOutBuffer = Header.TessellationHSTFOutBuffer;
	TessellationControlPointOutBuffer = Header.TessellationControlPointOutBuffer;
	TessellationControlPointIndexBuffer = Header.TessellationControlPointIndexBuffer;
	TessellationOutputControlPoints = Header.TessellationOutputControlPoints;
	TessellationDomain = Header.TessellationDomain;
	TessellationInputControlPoints = Header.TessellationInputControlPoints;
	TessellationMaxTessFactor = Header.TessellationMaxTessFactor;
	TessellationPatchesPerThreadGroup = Header.TessellationPatchesPerThreadGroup;
}

FMetalPixelShader::FMetalPixelShader(const TArray<uint8>& InCode)
{
	FMetalCodeHeader Header = {0};
	Init(InCode, Header);
}

FMetalPixelShader::FMetalPixelShader(const TArray<uint8>& InCode, id<MTLLibrary> InLibrary)
{
	FMetalCodeHeader Header = {0};
	Init(InCode, Header, InLibrary);
}

FMetalHullShader::FMetalHullShader(const TArray<uint8>& InCode)
{
	FMetalCodeHeader Header = {0};
	Init(InCode, Header);
}

FMetalHullShader::FMetalHullShader(const TArray<uint8>& InCode, id<MTLLibrary> InLibrary)
{
	FMetalCodeHeader Header = {0};
	Init(InCode, Header, InLibrary);
}

FMetalDomainShader::FMetalDomainShader(const TArray<uint8>& InCode)
{
	FMetalCodeHeader Header = {0};
	Init(InCode, Header);
	
	// for VSHS
	TessellationHSOutBuffer = Header.TessellationHSOutBuffer;
	TessellationControlPointOutBuffer = Header.TessellationControlPointOutBuffer;
	
	switch (Header.TessellationOutputWinding)
	{
		// NOTE: cw and ccw are flipped
		case EMetalOutputWindingMode::Clockwise:		TessellationOutputWinding = MTLWindingCounterClockwise; break;
		case EMetalOutputWindingMode::CounterClockwise:	TessellationOutputWinding = MTLWindingClockwise; break;
		default: check(0);
	}
	
	switch (Header.TessellationPartitioning)
	{
		case EMetalPartitionMode::Pow2:				TessellationPartitioning = MTLTessellationPartitionModePow2; break;
		case EMetalPartitionMode::Integer:			TessellationPartitioning = MTLTessellationPartitionModeInteger; break;
		case EMetalPartitionMode::FractionalOdd:	TessellationPartitioning = MTLTessellationPartitionModeFractionalOdd; break;
		case EMetalPartitionMode::FractionalEven:	TessellationPartitioning = MTLTessellationPartitionModeFractionalEven; break;
		default: check(0);
	}
}

FMetalDomainShader::FMetalDomainShader(const TArray<uint8>& InCode, id<MTLLibrary> InLibrary)
{
	FMetalCodeHeader Header = {0};
	Init(InCode, Header, InLibrary);
	
	// for VSHS
	TessellationHSOutBuffer = Header.TessellationHSOutBuffer;
	TessellationControlPointOutBuffer = Header.TessellationControlPointOutBuffer;
	
	switch (Header.TessellationOutputWinding)
	{
		// NOTE: cw and ccw are flipped
		case EMetalOutputWindingMode::Clockwise:		TessellationOutputWinding = MTLWindingCounterClockwise; break;
		case EMetalOutputWindingMode::CounterClockwise:	TessellationOutputWinding = MTLWindingClockwise; break;
		default: check(0);
	}
	
	switch (Header.TessellationPartitioning)
	{
		case EMetalPartitionMode::Pow2:				TessellationPartitioning = MTLTessellationPartitionModePow2; break;
		case EMetalPartitionMode::Integer:			TessellationPartitioning = MTLTessellationPartitionModeInteger; break;
		case EMetalPartitionMode::FractionalOdd:	TessellationPartitioning = MTLTessellationPartitionModeFractionalOdd; break;
		case EMetalPartitionMode::FractionalEven:	TessellationPartitioning = MTLTessellationPartitionModeFractionalEven; break;
		default: check(0);
	}
}

FVertexShaderRHIRef FMetalDynamicRHI::RHICreateVertexShader(const TArray<uint8>& Code)
{
    @autoreleasepool {
	FMetalVertexShader* Shader = new FMetalVertexShader(Code);
	return Shader;
	}
}

FVertexShaderRHIRef FMetalDynamicRHI::RHICreateVertexShader(FRHIShaderLibraryParamRef Library, FSHAHash Hash)
{
	@autoreleasepool {

	checkSlow(Library && Library->IsNativeLibrary() && IsMetalPlatform(Library->GetPlatform()) && Library->GetPlatform() <= GMaxRHIShaderPlatform);
	
	FMetalShaderLibrary* MetalLibrary = ResourceCast(Library);
	return MetalLibrary->CreateVertexShader(Hash);
	
	}
}

FPixelShaderRHIRef FMetalDynamicRHI::RHICreatePixelShader(const TArray<uint8>& Code)
{
	@autoreleasepool {
	FMetalPixelShader* Shader = new FMetalPixelShader(Code);
	return Shader;
	}
}

FPixelShaderRHIRef FMetalDynamicRHI::RHICreatePixelShader(FRHIShaderLibraryParamRef Library, FSHAHash Hash)
{
	@autoreleasepool {
	
	checkSlow(Library && Library->IsNativeLibrary() && IsMetalPlatform(Library->GetPlatform()) && Library->GetPlatform() <= GMaxRHIShaderPlatform);
	
	FMetalShaderLibrary* MetalLibrary = ResourceCast(Library);
	return MetalLibrary->CreatePixelShader(Hash);
	
	}
}

FHullShaderRHIRef FMetalDynamicRHI::RHICreateHullShader(const TArray<uint8>& Code) 
{
	@autoreleasepool {
	FMetalHullShader* Shader = new FMetalHullShader(Code);
	return Shader;
	}
}

FHullShaderRHIRef FMetalDynamicRHI::RHICreateHullShader(FRHIShaderLibraryParamRef Library, FSHAHash Hash)
{
	@autoreleasepool {
	
	checkSlow(Library && Library->IsNativeLibrary() && IsMetalPlatform(Library->GetPlatform()) && Library->GetPlatform() <= GMaxRHIShaderPlatform);
	
	FMetalShaderLibrary* MetalLibrary = ResourceCast(Library);
	return MetalLibrary->CreateHullShader(Hash);
	
	}
}

FDomainShaderRHIRef FMetalDynamicRHI::RHICreateDomainShader(const TArray<uint8>& Code) 
{
	@autoreleasepool {
	FMetalDomainShader* Shader = new FMetalDomainShader(Code);
	return Shader;
	}
}

FDomainShaderRHIRef FMetalDynamicRHI::RHICreateDomainShader(FRHIShaderLibraryParamRef Library, FSHAHash Hash)
{
	@autoreleasepool {
	
	checkSlow(Library && Library->IsNativeLibrary() && IsMetalPlatform(Library->GetPlatform()) && Library->GetPlatform() <= GMaxRHIShaderPlatform);
	
	FMetalShaderLibrary* MetalLibrary = ResourceCast(Library);
	return MetalLibrary->CreateDomainShader(Hash);
	
	}
}

FGeometryShaderRHIRef FMetalDynamicRHI::RHICreateGeometryShader(const TArray<uint8>& Code) 
{
	@autoreleasepool {
	FMetalGeometryShader* Shader = new FMetalGeometryShader;
	FMetalCodeHeader Header = {0};
	Shader->Init(Code, Header);
	return Shader;
	}
}

FGeometryShaderRHIRef FMetalDynamicRHI::RHICreateGeometryShader(FRHIShaderLibraryParamRef Library, FSHAHash Hash)
{
	@autoreleasepool {
	
	checkSlow(Library && Library->IsNativeLibrary() && IsMetalPlatform(Library->GetPlatform()) && Library->GetPlatform() <= GMaxRHIShaderPlatform);

	FMetalShaderLibrary* MetalLibrary = ResourceCast(Library);
	return MetalLibrary->CreateGeometryShader(Hash);
	
	}
}

FGeometryShaderRHIRef FMetalDynamicRHI::RHICreateGeometryShaderWithStreamOutput(const TArray<uint8>& Code, const FStreamOutElementList& ElementList,
	uint32 NumStrides, const uint32* Strides, int32 RasterizedStream)
{
	checkf(0, TEXT("Not supported yet"));
	return NULL;
}

FGeometryShaderRHIRef FMetalDynamicRHI::RHICreateGeometryShaderWithStreamOutput(const FStreamOutElementList& ElementList,
	uint32 NumStrides, const uint32* Strides, int32 RasterizedStream, FRHIShaderLibraryParamRef Library, FSHAHash Hash)
{
	@autoreleasepool {
	
	checkSlow(Library && Library->IsNativeLibrary() && IsMetalPlatform(Library->GetPlatform()) && Library->GetPlatform() <= GMaxRHIShaderPlatform);
	
	FMetalShaderLibrary* MetalLibrary = ResourceCast(Library);
	return MetalLibrary->CreateGeometryShaderWithStreamOutput(Hash, ElementList, NumStrides, Strides, RasterizedStream);
	
	}
}

FComputeShaderRHIRef FMetalDynamicRHI::RHICreateComputeShader(const TArray<uint8>& Code) 
{
	@autoreleasepool {
	FMetalComputeShader* Shader = new FMetalComputeShader(Code);
	
	// @todo WARNING: We have to hash here because of the way we immediately link and don't afford the cache a chance to set the OutputHash from ShaderCore.
	if (FShaderCache::GetShaderCache())
	{
		FSHAHash Hash;
		FSHA1::HashBuffer(Code.GetData(), Code.Num(), Hash.Hash);
		Shader->SetHash(Hash);
	}
	
	return Shader;
	}
}

FComputeShaderRHIRef FMetalDynamicRHI::RHICreateComputeShader(FRHIShaderLibraryParamRef Library, FSHAHash Hash) 
{ 
	@autoreleasepool {
	
	checkSlow(Library && Library->IsNativeLibrary() && IsMetalPlatform(Library->GetPlatform()) && Library->GetPlatform() <= GMaxRHIShaderPlatform);

	FMetalShaderLibrary* MetalLibrary = ResourceCast(Library);
	FComputeShaderRHIRef Shader = MetalLibrary->CreateComputeShader(Hash);
	
	if(Shader.IsValid() && FShaderCache::GetShaderCache())
	{
		// @todo WARNING: We have to hash here because of the way we immediately link and don't afford the cache a chance to set the OutputHash from ShaderCore.
		Shader->SetHash(Hash);
	}
	
	return Shader;
	
	}
}

FMetalShaderLibrary::FMetalShaderLibrary(EShaderPlatform InPlatform, id<MTLLibrary> InLibrary, FMetalShaderMap const& InMap)
: FRHIShaderLibrary(InPlatform)
, Library(InLibrary)
, Map(InMap)
{
	
}

FMetalShaderLibrary::~FMetalShaderLibrary()
{
	
}

FPixelShaderRHIRef FMetalShaderLibrary::CreatePixelShader(const FSHAHash& Hash)
{
	TPair<uint8, TArray<uint8>>* Code = Map.HashMap.Find(Hash);
	if (Code)
	{
		FMetalPixelShader* Shader = new FMetalPixelShader(Code->Value, Library);
		if (Shader->Library || Shader->Function)
		{
			return Shader;
		}
		else
		{
			delete Shader;
		}
	}
	
	UE_LOG(LogMetal, Error, TEXT("Failed to find Pixel Shader with SHA: %s"), *Hash.ToString());
	return FPixelShaderRHIRef();
}

FVertexShaderRHIRef FMetalShaderLibrary::CreateVertexShader(const FSHAHash& Hash)
{
	TPair<uint8, TArray<uint8>>* Code = Map.HashMap.Find(Hash);
	if (Code)
	{
		FMetalVertexShader* Shader = new FMetalVertexShader(Code->Value, Library);
		if (Shader->Library || Shader->Function)
		{
			return Shader;
		}
		else
		{
			delete Shader;
		}
	}
	UE_LOG(LogMetal, Error, TEXT("Failed to find Vertex Shader with SHA: %s"), *Hash.ToString());
	return FVertexShaderRHIRef();
}

FHullShaderRHIRef FMetalShaderLibrary::CreateHullShader(const FSHAHash& Hash)
{
	TPair<uint8, TArray<uint8>>* Code = Map.HashMap.Find(Hash);
	if (Code)
	{
		FMetalHullShader* Shader = new FMetalHullShader(Code->Value, Library);
		if(Shader->Library || Shader->Function)
		{
			return Shader;
		}
		else
		{
			delete Shader;
		}
	}
	UE_LOG(LogMetal, Error, TEXT("Failed to find Hull Shader with SHA: %s"), *Hash.ToString());
	return FHullShaderRHIRef();
}

FDomainShaderRHIRef FMetalShaderLibrary::CreateDomainShader(const FSHAHash& Hash)
{
	TPair<uint8, TArray<uint8>>* Code = Map.HashMap.Find(Hash);
	if (Code)
	{
		FMetalDomainShader* Shader = new FMetalDomainShader(Code->Value, Library);
		if (Shader->Library || Shader->Function)
		{
			return Shader;
		}
		else
		{
			delete Shader;
		}
	}
	UE_LOG(LogMetal, Error, TEXT("Failed to find Domain Shader with SHA: %s"), *Hash.ToString());
	return FDomainShaderRHIRef();
}

FGeometryShaderRHIRef FMetalShaderLibrary::CreateGeometryShader(const FSHAHash& Hash)
{
	checkf(0, TEXT("Not supported yet"));
	return NULL;
}

FGeometryShaderRHIRef FMetalShaderLibrary::CreateGeometryShaderWithStreamOutput(const FSHAHash& Hash, const FStreamOutElementList& ElementList, uint32 NumStrides, const uint32* Strides, int32 RasterizedStream)
{
	checkf(0, TEXT("Not supported yet"));
	return NULL;
}

FComputeShaderRHIRef FMetalShaderLibrary::CreateComputeShader(const FSHAHash& Hash)
{
	TPair<uint8, TArray<uint8>>* Code = Map.HashMap.Find(Hash);
	if (Code)
	{
		FMetalComputeShader* Shader = new FMetalComputeShader(Code->Value, Library);
		if (Shader->Library || Shader->Function)
		{
			return Shader;
		}
		else
		{
			delete Shader;
		}
	}
	UE_LOG(LogMetal, Error, TEXT("Failed to find Compute Shader with SHA: %s"), *Hash.ToString());
	return FComputeShaderRHIRef();
}

//
//Library Iterator
//
FRHIShaderLibrary::FShaderLibraryEntry FMetalShaderLibrary::FMetalShaderLibraryIterator::operator*() const
{
	FShaderLibraryEntry Entry;
	
	Entry.Hash = IteratorImpl->Key;
	Entry.Frequency = (EShaderFrequency)IteratorImpl->Value.Key;
	Entry.Platform = GetLibrary()->GetPlatform();
	
	return Entry;
}

FRHIShaderLibraryRef FMetalDynamicRHI::RHICreateShaderLibrary(EShaderPlatform Platform, FString FolderPath)
{
	@autoreleasepool {
	FRHIShaderLibraryRef Result = nullptr;
	
	FName PlatformName = LegacyShaderPlatformToShaderFormat(Platform);
	
	FMetalShaderMap Map;
	FString BinaryShaderFile = FolderPath / PlatformName.GetPlainNameString() + METAL_MAP_EXTENSION;
	FArchive* BinaryShaderAr = IFileManager::Get().CreateFileReader(*BinaryShaderFile);
	if( BinaryShaderAr != NULL )
	{
		*BinaryShaderAr << Map;
		BinaryShaderAr->Flush();
		delete BinaryShaderAr;
		
		// Would be good to check the language version of the library with the archive format here.
		if (Map.Format == PlatformName.GetPlainNameString())
		{
			FString MetalLibraryFilePath = FolderPath / PlatformName.GetPlainNameString() + METAL_LIB_EXTENSION;
			MetalLibraryFilePath = FPaths::ConvertRelativePathToFull(MetalLibraryFilePath);
#if !PLATFORM_MAC
			MetalLibraryFilePath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*MetalLibraryFilePath);
#endif
			
			NSError* Error;
			id<MTLLibrary> Library = [GetMetalDeviceContext().GetDevice() newLibraryWithFile:MetalLibraryFilePath.GetNSString() error:&Error];
			if (Library != nil)
			{
				if (Map.HashMap.Num() != Library.functionNames.count)
				{
					UE_LOG(LogMetal, Error, TEXT("Mistmatch between map (%d) & library (%d) shader count"), Map.HashMap.Num(), Library.functionNames.count);
				}
				
				Result = new FMetalShaderLibrary(Platform, Library, Map);
			}
			else
			{
				UE_LOG(LogMetal, Display, TEXT("Failed to create library: %s"), *FString(Error.description));
			}
		}
		else
		{
			UE_LOG(LogMetal, Display, TEXT("Wrong shader platform wanted: %s, got: %s"), *PlatformName.GetPlainNameString(), *Map.Format);
		}
	}
	else
	{
		UE_LOG(LogMetal, Display, TEXT("No .metalmap file found for %s!"), *PlatformName.GetPlainNameString());
	}
	
	return Result;
	}
}

FBoundShaderStateRHIRef FMetalDynamicRHI::RHICreateBoundShaderState(
	FVertexDeclarationRHIParamRef VertexDeclarationRHI, 
	FVertexShaderRHIParamRef VertexShaderRHI, 
	FHullShaderRHIParamRef HullShaderRHI, 
	FDomainShaderRHIParamRef DomainShaderRHI, 
	FPixelShaderRHIParamRef PixelShaderRHI,
	FGeometryShaderRHIParamRef GeometryShaderRHI
	)
{
	NOT_SUPPORTED("RHICreateBoundShaderState");
	return nullptr;
}

FMetalShaderParameterCache::FMetalShaderParameterCache()
{
	for (int32 ArrayIndex = 0; ArrayIndex < CrossCompiler::PACKED_TYPEINDEX_MAX; ++ArrayIndex)
	{
		PackedGlobalUniforms[ArrayIndex] = nullptr;
		PackedGlobalUniformsSizes[ArrayIndex] = 0;
		PackedGlobalUniformDirty[ArrayIndex].LowVector = 0;
		PackedGlobalUniformDirty[ArrayIndex].HighVector = 0;
	}
}

void FMetalShaderParameterCache::ResizeGlobalUniforms(uint32 TypeIndex, uint32 UniformArraySize)
{
	PackedGlobalUniforms[TypeIndex] = (uint8*)FMemory::Realloc(PackedGlobalUniforms[TypeIndex], UniformArraySize);
	PackedGlobalUniformsSizes[TypeIndex] = UniformArraySize;
	PackedGlobalUniformDirty[TypeIndex].LowVector = 0;
	PackedGlobalUniformDirty[TypeIndex].HighVector = 0;
}

/** Destructor. */
FMetalShaderParameterCache::~FMetalShaderParameterCache()
{
	for (int32 ArrayIndex = 0; ArrayIndex < CrossCompiler::PACKED_TYPEINDEX_MAX; ++ArrayIndex)
	{
		FMemory::Free(PackedGlobalUniforms[ArrayIndex]);
	}
}

/**
 * Marks all uniform arrays as dirty.
 */
void FMetalShaderParameterCache::MarkAllDirty()
{
	for (int32 ArrayIndex = 0; ArrayIndex < CrossCompiler::PACKED_TYPEINDEX_MAX; ++ArrayIndex)
	{
		PackedGlobalUniformDirty[ArrayIndex].LowVector = 0;
		PackedGlobalUniformDirty[ArrayIndex].HighVector = 0;//UniformArraySize / 16;
	}
}

const int SizeOfFloat4 = 4 * sizeof(float);

/**
 * Set parameter values.
 */
void FMetalShaderParameterCache::Set(uint32 BufferIndexName, uint32 ByteOffset, uint32 NumBytes, const void* NewValues)
{
	uint32 BufferIndex = CrossCompiler::PackedTypeNameToTypeIndex(BufferIndexName);
	check(BufferIndex < CrossCompiler::PACKED_TYPEINDEX_MAX);
	check(PackedGlobalUniforms[BufferIndex]);
	check(ByteOffset + NumBytes <= PackedGlobalUniformsSizes[BufferIndex]);
	PackedGlobalUniformDirty[BufferIndex].LowVector = FMath::Min(PackedGlobalUniformDirty[BufferIndex].LowVector, ByteOffset / SizeOfFloat4);
	PackedGlobalUniformDirty[BufferIndex].HighVector = FMath::Max(PackedGlobalUniformDirty[BufferIndex].HighVector, (ByteOffset + NumBytes + SizeOfFloat4 - 1) / SizeOfFloat4);
	FMemory::Memcpy(PackedGlobalUniforms[BufferIndex] + ByteOffset, NewValues, NumBytes);
}

void FMetalShaderParameterCache::CommitPackedGlobals(FMetalStateCache* Cache, FMetalCommandEncoder* Encoder, EShaderFrequency Frequency, const FMetalShaderBindings& Bindings)
{
	// copy the current uniform buffer into the ring buffer to submit
	for (int32 Index = 0; Index < Bindings.PackedGlobalArrays.Num(); ++Index)
	{
		int32 UniformBufferIndex = Bindings.PackedGlobalArrays[Index].TypeIndex;
 
		// is there any data that needs to be copied?
		if (PackedGlobalUniformDirty[UniformBufferIndex].HighVector > 0)
		{
			uint32 TotalSize = Bindings.PackedGlobalArrays[Index].Size;
			uint32 SizeToUpload = PackedGlobalUniformDirty[UniformBufferIndex].HighVector * SizeOfFloat4;
			
			//@todo-rco: Temp workaround
			SizeToUpload = TotalSize;
			
			//@todo-rco: Temp workaround
			uint8 const* Bytes = PackedGlobalUniforms[UniformBufferIndex];
			uint32 Size = FMath::Min(TotalSize, SizeToUpload);
			
			uint32 Offset = Encoder->GetRingBuffer().Allocate(Size, 0);
			id<MTLBuffer> Buffer = Encoder->GetRingBuffer().Buffer->Buffer;
			
			FMemory::Memcpy((uint8*)[Buffer contents] + Offset, Bytes, Size);
			
			Cache->SetShaderBuffer(Frequency, Buffer, nil, Offset, Size, UniformBufferIndex);

			// mark as clean
			PackedGlobalUniformDirty[UniformBufferIndex].HighVector = 0;
		}
	}
}

void FMetalShaderParameterCache::CommitPackedUniformBuffers(FMetalStateCache* Cache, TRefCountPtr<FMetalGraphicsPipelineState> BoundShaderState, FMetalComputeShader* ComputeShader, int32 Stage, const TRefCountPtr<FRHIUniformBuffer>* RHIUniformBuffers, const TArray<CrossCompiler::FUniformBufferCopyInfo>& UniformBuffersCopyInfo)
{
//	SCOPE_CYCLE_COUNTER(STAT_MetalConstantBufferUpdateTime);
	// Uniform Buffers are split into precision/type; the list of RHI UBs is traversed and if a new one was set, its
	// contents are copied per precision/type into corresponding scratch buffers which are then uploaded to the program
	if (Stage == CrossCompiler::SHADER_STAGE_PIXEL && !IsValidRef(BoundShaderState->PixelShader))
	{
		return;
	}

	auto& Bindings = [this, &Stage, &BoundShaderState, ComputeShader]() -> FMetalShaderBindings& {
		switch(Stage) {
			default: check(0);
			case CrossCompiler::SHADER_STAGE_VERTEX: return BoundShaderState->VertexShader->Bindings;
			case CrossCompiler::SHADER_STAGE_PIXEL: return BoundShaderState->PixelShader->Bindings;
			case CrossCompiler::SHADER_STAGE_COMPUTE: return ComputeShader->Bindings;
			case CrossCompiler::SHADER_STAGE_HULL: return BoundShaderState->HullShader->Bindings;
			case CrossCompiler::SHADER_STAGE_DOMAIN: return BoundShaderState->DomainShader->Bindings;
		}
	}();

	if (!Bindings.bHasRegularUniformBuffers && !FShaderCache::IsPredrawCall(Cache->GetShaderCacheStateObject()))
	{
		check(Bindings.NumUniformBuffers <= ML_MaxBuffers);
		int32 LastInfoIndex = 0;
		for (int32 BufferIndex = 0; BufferIndex < Bindings.NumUniformBuffers; ++BufferIndex)
		{
			const FRHIUniformBuffer* RHIUniformBuffer = RHIUniformBuffers[BufferIndex];
			check(RHIUniformBuffer);
			FMetalUniformBuffer* EmulatedUniformBuffer = (FMetalUniformBuffer*)RHIUniformBuffer;
			const uint32* RESTRICT SourceData = (uint32 const*)((uint8 const*)EmulatedUniformBuffer->GetData() + EmulatedUniformBuffer->Offset);
			for (int32 InfoIndex = LastInfoIndex; InfoIndex < UniformBuffersCopyInfo.Num(); ++InfoIndex)
			{
				const CrossCompiler::FUniformBufferCopyInfo& Info = UniformBuffersCopyInfo[InfoIndex];
				if (Info.SourceUBIndex == BufferIndex)
				{
					float* RESTRICT ScratchMem = (float*)PackedGlobalUniforms[Info.DestUBTypeIndex];
					ScratchMem += Info.DestOffsetInFloats;
					FMemory::Memcpy(ScratchMem, SourceData + Info.SourceOffsetInFloats, Info.SizeInFloats * sizeof(float));
					PackedGlobalUniformDirty[Info.DestUBTypeIndex].LowVector = FMath::Min(PackedGlobalUniformDirty[Info.DestUBTypeIndex].LowVector, uint32(Info.DestOffsetInFloats / SizeOfFloat4));
					PackedGlobalUniformDirty[Info.DestUBTypeIndex].HighVector = FMath::Max(PackedGlobalUniformDirty[Info.DestUBTypeIndex].HighVector, uint32(((Info.DestOffsetInFloats + Info.SizeInFloats) * sizeof(float) + SizeOfFloat4 - 1) / SizeOfFloat4));
				}
				else
				{
					LastInfoIndex = InfoIndex;
					break;
				}
			}
		}
	}
}
