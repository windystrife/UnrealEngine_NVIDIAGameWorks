// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MetalDerivedData.h"
#include "MetalShaderFormat.h"
#include "Serialization/MemoryWriter.h"
#include "RHIDefinitions.h"
#include "Misc/FileHelper.h"
#include "hlslcc.h"
#include "MetalShaderFormat.h"
#include "MetalShaderResources.h"
#include "Misc/Paths.h"
#include "Misc/Compression.h"
#include "MetalBackend.h"

extern bool ExecRemoteProcess(const TCHAR* Command, const TCHAR* Params, int32* OutReturnCode, FString* OutStdOut, FString* OutStdErr);
extern FString GetXcodePath();
extern FString GetMetalStdLibPath(FString const& PlatformPath);
extern FString GetMetalCompilerVers(FString const& PlatformPath);
extern bool RemoteFileExists(const FString& Path);
extern FString MakeRemoteTempFolder(FString Path);
extern FString LocalPathToRemote(const FString& LocalPath, const FString& RemoteFolder);
extern bool CopyLocalFileToRemote(FString const& LocalPath, FString const& RemotePath);
extern bool CopyRemoteFileToLocal(FString const& RemotePath, FString const& LocalPath);
extern bool ChecksumRemoteFile(FString const& RemotePath, uint32* CRC, uint32* Len);
extern bool RemoveRemoteFile(FString const& RemotePath);
extern FString GetMetalBinaryPath(uint32 ShaderPlatform);
extern FString GetMetalToolsPath(uint32 ShaderPlatform);
extern FString GetMetalLibraryPath(uint32 ShaderPlatform);
extern FString GetMetalCompilerVersion(uint32 ShaderPlatform);
extern EShaderPlatform MetalShaderFormatToLegacyShaderPlatform(FName ShaderFormat);
extern void BuildMetalShaderOutput(
	FShaderCompilerOutput& ShaderOutput,
	const FShaderCompilerInput& ShaderInput,
	FSHAHash const& GUIDHash,
	const ANSICHAR* InShaderSource,
	uint32 SourceLen,
	uint32 SourceCRCLen,
	uint32 SourceCRC,
	uint8 Version,
	TCHAR const* Standard,
	TCHAR const* MinOSVersion,
	TArray<FShaderCompilerError>& OutErrors,
	FMetalTessellationOutputs const& TessOutputAttribs,
	uint8 const AtomicUAVs,
	bool bAllowFastIntriniscs
	);

FMetalShaderDebugInfoCooker::FMetalShaderDebugInfoCooker(FMetalShaderDebugInfoJob& InJob)
: Job(InJob)
{
}

FMetalShaderDebugInfoCooker::~FMetalShaderDebugInfoCooker()
{
}

#if PLATFORM_MAC || PLATFORM_IOS
#pragma mark - FDerivedDataPluginInterface Interface -
#endif
const TCHAR* FMetalShaderDebugInfoCooker::GetPluginName() const
{
	return TEXT("FMetalShaderDebugInfo");
}

const TCHAR* FMetalShaderDebugInfoCooker::GetVersionString() const
{
	static FString Version = FString::Printf(TEXT("%u"), (uint32)GetMetalFormatVersion(Job.ShaderFormat));
	return *Version;
}

FString FMetalShaderDebugInfoCooker::GetPluginSpecificCacheKeySuffix() const
{
	FString CompilerVersion = GetMetalCompilerVersion(MetalShaderFormatToLegacyShaderPlatform(Job.ShaderFormat));
	
	FString VersionedName = FString::Printf(TEXT("%s%u%u%s%s%s%s%s%s"), *Job.ShaderFormat.GetPlainNameString(), Job.SourceCRCLen, Job.SourceCRC, *Job.Hash.ToString(), *Job.CompilerVersion, *Job.MinOSVersion, *Job.DebugInfo, *Job.MathMode, *Job.Standard);
	// get rid of some not so filename-friendly characters ('=',' ' -> '_')
    VersionedName = VersionedName.Replace(TEXT("="), TEXT("_")).Replace(TEXT(" "), TEXT("_"));

	return VersionedName;
}

bool FMetalShaderDebugInfoCooker::IsBuildThreadsafe() const
{
	return false;
}

bool FMetalShaderDebugInfoCooker::Build(TArray<uint8>& OutData)
{
	bool bSucceeded = false;
	
	uint32 CodeSize = FCStringAnsi::Strlen(TCHAR_TO_UTF8(*Job.MetalCode))+1;
	
	int32 CompressedSize = FCompression::CompressMemoryBound( ECompressionFlags::COMPRESS_ZLIB, CodeSize );
	Output.CompressedData.SetNum(CompressedSize);
	
	if (FCompression::CompressMemory(ECompressionFlags::COMPRESS_ZLIB, Output.CompressedData.GetData(), CompressedSize, TCHAR_TO_UTF8(*Job.MetalCode), CodeSize))
	{
		Output.UncompressedSize = CodeSize;
		Output.CompressedData.SetNum(CompressedSize);
		Output.CompressedData.Shrink();
		bSucceeded = true;
		
		FMemoryWriter Ar(OutData);
		Ar << Output;
	}
	
	return bSucceeded;
}

FMetalShaderBytecodeCooker::FMetalShaderBytecodeCooker(FMetalShaderBytecodeJob& InJob)
: Job(InJob)
{
}

FMetalShaderBytecodeCooker::~FMetalShaderBytecodeCooker()
{
}

#if PLATFORM_MAC || PLATFORM_IOS
#pragma mark - FDerivedDataPluginInterface Interface - 
#endif
const TCHAR* FMetalShaderBytecodeCooker::GetPluginName() const
{
	return TEXT("MetalShaderBytecode");
}

const TCHAR* FMetalShaderBytecodeCooker::GetVersionString() const
{
	static FString Version = FString::Printf(TEXT("%u"), (uint32)GetMetalFormatVersion(Job.ShaderFormat));
	return *Version;
}

FString FMetalShaderBytecodeCooker::GetPluginSpecificCacheKeySuffix() const
{
	FString CompilerVersion = GetMetalCompilerVersion(MetalShaderFormatToLegacyShaderPlatform(Job.ShaderFormat));
	FString CompilerPath = GetMetalToolsPath(MetalShaderFormatToLegacyShaderPlatform(Job.ShaderFormat));
	
	FString VersionedName = FString::Printf(TEXT("%s%u%u%s%s%s%s%s%s%s%d"), *Job.ShaderFormat.GetPlainNameString(), Job.SourceCRCLen, Job.SourceCRC, *Job.Hash.ToString(), *Job.CompilerVersion, *Job.MinOSVersion, *Job.DebugInfo, *Job.MathMode, *Job.Standard, Job.bRetainObjectFile ? TEXT("+Object") : TEXT(""), GetTypeHash(CompilerPath));
	// get rid of some not so filename-friendly characters ('=',' ' -> '_')
    VersionedName = VersionedName.Replace(TEXT("="), TEXT("_")).Replace(TEXT(" "), TEXT("_"));

	return VersionedName;
}

bool FMetalShaderBytecodeCooker::IsBuildThreadsafe() const
{
	return false;
}

bool FMetalShaderBytecodeCooker::Build(TArray<uint8>& OutData)
{
	bool bSucceeded = false;
	
#if PLATFORM_MAC
	// Unset the SDKROOT to avoid problems with the incorrect path being used when compiling with the shared PCH.
	TCHAR SdkRoot[4096];
	FPlatformMisc::GetEnvironmentVariable(TEXT("SDKROOT"), SdkRoot, ARRAY_COUNT(SdkRoot));
	if (FCStringWide::Strlen(SdkRoot))
	{
	    unsetenv("SDKROOT");
	}
#endif

	bool bRemoteBuildingConfigured = IsRemoteBuildingConfigured();

	const FString RemoteFolder = MakeRemoteTempFolder(Job.TmpFolder);
	const FString RemoteInputFile = LocalPathToRemote(Job.InputFile, RemoteFolder);			// Input file to the compiler - Copied from local machine to remote machine
	const FString RemoteInputPCHFile = LocalPathToRemote(Job.InputPCHFile, RemoteFolder);			// Input file to the compiler - Copied from local machine to remote machine
	const FString RemoteObjFile = LocalPathToRemote(Job.OutputObjectFile, RemoteFolder);				// Output from the compiler -> Input file to the archiver
	const FString RemoteOutputFilename = LocalPathToRemote(Job.OutputFile, RemoteFolder);	// Output from the library generator - Copied from remote machine to local machine

	uint32 ShaderPlatform = MetalShaderFormatToLegacyShaderPlatform(Job.ShaderFormat);
	FString MetalPath = GetMetalBinaryPath(ShaderPlatform);
	FString MetalToolsPath = GetMetalToolsPath(ShaderPlatform);
	FString MetalLibPath = MetalToolsPath + TEXT("/metallib");

	FString MetalParams;
	if (Job.bCompileAsPCH)
	{
		MetalParams = FString::Printf(TEXT("-x metal-header %s %s %s %s -o %s"), *Job.MinOSVersion, *Job.MathMode, *Job.Standard, *Job.InputFile, *RemoteOutputFilename);
	}
	else
	{
		CopyLocalFileToRemote(Job.InputFile, RemoteInputFile);
		
		// PCH 
		bool bUseSharedPCH = Job.InputPCHFile.Len() && IFileManager::Get().FileExists(*Job.InputPCHFile);
		if (bUseSharedPCH)
        {
            CopyLocalFileToRemote(Job.InputPCHFile, RemoteInputPCHFile);
			MetalParams = FString::Printf(TEXT("-include-pch %s %s %s %s -Wno-null-character -fbracket-depth=1024 %s %s -o %s"), *RemoteInputPCHFile, *Job.MinOSVersion, *Job.DebugInfo, *Job.MathMode, *Job.Standard, *RemoteInputFile, *RemoteObjFile);
        }
        else
        {
            MetalParams = FString::Printf(TEXT("%s %s %s -Wno-null-character -fbracket-depth=1024 %s %s -o %s"), *Job.MinOSVersion, *Job.DebugInfo, *Job.MathMode, *Job.Standard, *RemoteInputFile, *RemoteObjFile);
        }
	}
				
	TCHAR const* CompileType = bRemoteBuildingConfigured ? TEXT("remotely") : TEXT("locally");
	
    bSucceeded = (ExecRemoteProcess( *MetalPath, *MetalParams, &Job.ReturnCode, &Job.Results, &Job.Errors ) && Job.ReturnCode == 0);
	if (bSucceeded)
	{
		if (!Job.bCompileAsPCH)
		{
			FString LibraryParams = FString::Printf(TEXT("-o %s %s"), *RemoteOutputFilename, *RemoteObjFile);
			bSucceeded = (ExecRemoteProcess(*MetalLibPath, *LibraryParams, &Job.ReturnCode, &Job.Results, &Job.Errors) && Job.ReturnCode == 0);
			if (bSucceeded)
			{
				if (Job.bRetainObjectFile)
				{
					CopyRemoteFileToLocal(RemoteObjFile, Job.OutputObjectFile);
					
					bSucceeded = FFileHelper::LoadFileToArray(Output.ObjectFile, *Job.OutputObjectFile);
					
					if (!bSucceeded)
					{
						Job.Message = FString::Printf(TEXT("Failed to load object file: %s"), *Job.OutputObjectFile);
					}
					
					RemoveRemoteFile(RemoteObjFile);
				}
			}
			else
			{
				Job.Message = FString::Printf(TEXT("Failed to package into library %s, code: %d, output: %s %s"), CompileType, Job.ReturnCode, *Job.Results, *Job.Errors);
			}
		}
		
		CopyRemoteFileToLocal(RemoteOutputFilename, Job.OutputFile);
		Output.NativePath = RemoteInputFile;
		bSucceeded = FFileHelper::LoadFileToArray(Output.OutputFile, *Job.OutputFile);
		
		if (!Job.bCompileAsPCH)
		{
			RemoveRemoteFile(RemoteOutputFilename);
		}
		
		if (!bSucceeded)
		{
			Job.Message = FString::Printf(TEXT("Failed to load output file: %s"), *Job.OutputFile);
		}
	}
	else
	{
		if (Job.bCompileAsPCH)
		{
	    	Job.Message = FString::Printf(TEXT("Metal Shared PCH generation failed %s: %s."), CompileType, *Job.Errors);
		}
		else
		{
            Job.Message = FString::Printf(TEXT("Failed to compile to bytecode %s, code: %d, output: %s %s"), CompileType, Job.ReturnCode, *Job.Results, *Job.Errors);
		}
	}
	
	if (bSucceeded)
	{
		FMemoryWriter Ar(OutData);
		Ar << Output;
	}
                   
#if PLATFORM_MAC
	// Reset the SDKROOT environment we unset earlier.
	if (FCStringWide::Strlen(SdkRoot))
	{
		setenv("SDKROOT", TCHAR_TO_UTF8(SdkRoot), 1);
	}
#endif
	
	return bSucceeded;
}

FMetalShaderOutputCooker::FMetalShaderOutputCooker(const FShaderCompilerInput& _Input,FShaderCompilerOutput& _Output,const FString& _WorkingDirectory, FString _PreprocessedShader, FSHAHash _GUIDHash, uint8 _VersionEnum, uint32 _CCFlags, EHlslCompileTarget _HlslCompilerTarget, EHlslCompileTarget _MetalCompilerTarget, EMetalGPUSemantics _Semantics, EMetalTypeBufferMode _TypeMode, uint32 _MaxUnrollLoops, EHlslShaderFrequency _Frequency, bool _bDumpDebugInfo, FString _Standard, FString _MinOSVersion)
: Input(_Input)
, Output(_Output)
, WorkingDirectory(_WorkingDirectory)
, PreprocessedShader(_PreprocessedShader)
, GUIDHash(_GUIDHash)
, VersionEnum(_VersionEnum)
, CCFlags(_CCFlags)
, HlslCompilerTarget(_HlslCompilerTarget)
, MetalCompilerTarget(_MetalCompilerTarget)
, Semantics(_Semantics)
, TypeMode(_TypeMode)
, MaxUnrollLoops(_MaxUnrollLoops)
, Frequency(_Frequency)
, bDumpDebugInfo(_bDumpDebugInfo)
, Standard(_Standard)
, MinOSVersion(_MinOSVersion)
{
}

FMetalShaderOutputCooker::~FMetalShaderOutputCooker()
{
}

#if PLATFORM_MAC || PLATFORM_IOS
#pragma mark - FDerivedDataPluginInterface Interface -
#endif

const TCHAR* FMetalShaderOutputCooker::GetPluginName() const
{
	return TEXT("MetalShaderOutput");
}

const TCHAR* FMetalShaderOutputCooker::GetVersionString() const
{
	static FString Version = FString::Printf(TEXT("%u"), (uint32)GetMetalFormatVersion(Input.ShaderFormat));
	return *Version;
}

FString FMetalShaderOutputCooker::GetPluginSpecificCacheKeySuffix() const
{
	FString CachedOutputName;
	{
		FSHAHash Hash;
		FSHA1::HashBuffer(*PreprocessedShader, PreprocessedShader.Len() * sizeof(TCHAR), Hash.Hash);
		
		uint32 Len = PreprocessedShader.Len();
		
		uint16 FormatVers = GetMetalFormatVersion(Input.ShaderFormat);
		
		uint64 Flags = 0;
		for (uint32 Flag : Input.Environment.CompilerFlags)
		{
			Flags |= (1llu << uint64(Flag));
		}
		
		CachedOutputName = FString::Printf(TEXT("%s-%s_%s-%u_%hu_%llu_%hhu_%s_%s"), *Input.ShaderFormat.GetPlainNameString(), *Input.EntryPointName, *Hash.ToString(), Len, FormatVers, Flags, VersionEnum, *GUIDHash.ToString(), *Standard);
	}
	
	return CachedOutputName;
}

bool FMetalShaderOutputCooker::IsBuildThreadsafe() const
{
	return false;
}

bool FMetalShaderOutputCooker::Build(TArray<uint8>& OutData)
{
	Output.bSucceeded = false;
	
	char* MetalShaderSource = NULL;
	char* ErrorLog = NULL;
	
	bool const bZeroInitialise = Input.Environment.CompilerFlags.Contains(CFLAG_ZeroInitialise);
	bool const bBoundsChecks = Input.Environment.CompilerFlags.Contains(CFLAG_BoundsChecking);
	
	bool bAllowFastIntriniscs = false;
	FString const* FastIntrinsics = Input.Environment.GetDefinitions().Find(TEXT("METAL_USE_FAST_INTRINSICS"));
	if (FastIntrinsics)
	{
		LexicalConversion::FromString(bAllowFastIntriniscs, *(*FastIntrinsics));
	}
	
	FMetalTessellationOutputs Attribs;
	FMetalCodeBackend MetalBackEnd(Attribs, CCFlags, MetalCompilerTarget, VersionEnum, Semantics, TypeMode, MaxUnrollLoops, bZeroInitialise, bBoundsChecks, bAllowFastIntriniscs);
	FMetalLanguageSpec MetalLanguageSpec(VersionEnum);

	int32 Result = 0;
	FHlslCrossCompilerContext CrossCompilerContext(CCFlags, Frequency, HlslCompilerTarget);
	if (CrossCompilerContext.Init(TCHAR_TO_ANSI(*Input.VirtualSourceFilePath), &MetalLanguageSpec))
	{
		Result = CrossCompilerContext.Run(
			TCHAR_TO_ANSI(*PreprocessedShader),
			TCHAR_TO_ANSI(*Input.EntryPointName),
			&MetalBackEnd,
			&MetalShaderSource,
			&ErrorLog
			) ? 1 : 0;
	}

	uint32 CRCLen = MetalShaderSource ? (uint32)FCStringAnsi::Strlen(MetalShaderSource) : 0u;
	uint32 CRC = CRCLen ? FCrc::MemCrc_DEPRECATED(MetalShaderSource, CRCLen) : 0u;
	uint32 SourceLen = CRCLen;
	if(MetalShaderSource)
	{
		ANSICHAR* Main = FCStringAnsi::Strstr(MetalShaderSource, "Main_00000000_00000000");
		check(Main);
		
		ANSICHAR MainCRC[24];
		int32 NewLen = FCStringAnsi::Snprintf(MainCRC, 24, "Main_%0.8x_%0.8x", CRCLen, CRC);
		FMemory::Memcpy(Main, MainCRC, NewLen);
	
		uint32 Len = FCStringAnsi::Strlen(TCHAR_TO_ANSI(*Input.GetSourceFilename())) + FCStringAnsi::Strlen(TCHAR_TO_ANSI(*Input.DebugGroupName)) + FCStringAnsi::Strlen(TCHAR_TO_ANSI(*Input.EntryPointName)) + FCStringAnsi::Strlen(MetalShaderSource) + 21;
		char* Dest = (char*)malloc(Len);
		FCStringAnsi::Snprintf(Dest, Len, "// ! %s/%s.usf:%s\n%s", (const char*)TCHAR_TO_ANSI(*Input.DebugGroupName), (const char*)TCHAR_TO_ANSI(*Input.GetSourceFilename()), (const char*)TCHAR_TO_ANSI(*Input.EntryPointName), (const char*)MetalShaderSource);
		free(MetalShaderSource);
		MetalShaderSource = Dest;
		SourceLen = (uint32)FCStringAnsi::Strlen(MetalShaderSource);
	}
	
	if (bDumpDebugInfo)
	{
		if (SourceLen > 0u)
		{
			FArchive* FileWriter = IFileManager::Get().CreateFileWriter(*(Input.DumpDebugInfoPath / FPaths::GetBaseFilename(Input.GetSourceFilename()) + TEXT(".metal")));
			if (FileWriter)
			{
				FileWriter->Serialize(MetalShaderSource, SourceLen + 1);
				FileWriter->Close();
				delete FileWriter;
			}
		}
	}

	if (Result != 0)
	{
		Output.Target = Input.Target;
		BuildMetalShaderOutput(Output, Input, GUIDHash, MetalShaderSource, SourceLen, CRCLen, CRC, VersionEnum, *Standard, *MinOSVersion, Output.Errors, Attribs, MetalBackEnd.AtomicUAVs, bAllowFastIntriniscs);
		
		FMemoryWriter Ar(OutData);
		Ar << Output;
	}
	else
	{
		FString Tmp = ANSI_TO_TCHAR(ErrorLog);
		TArray<FString> ErrorLines;
		Tmp.ParseIntoArray(ErrorLines, TEXT("\n"), true);
		bool const bDirectCompile = FParse::Param(FCommandLine::Get(), TEXT("directcompile"));
		for (int32 LineIndex = 0; LineIndex < ErrorLines.Num(); ++LineIndex)
		{
			const FString& Line = ErrorLines[LineIndex];
			CrossCompiler::ParseHlslccError(Output.Errors, Line, bDirectCompile);
			if (bDirectCompile)
			{
				UE_LOG(LogShaders, Error, TEXT("%s"), *Line);
			}
		}
	}

	if (MetalShaderSource)
	{
		free(MetalShaderSource);
	}
	if (ErrorLog)
	{
		free(ErrorLog);
	}

	return Output.bSucceeded;
}
