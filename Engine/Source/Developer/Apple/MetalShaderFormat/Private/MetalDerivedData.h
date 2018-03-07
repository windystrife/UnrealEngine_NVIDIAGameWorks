// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "SecureHash.h"
#include "DerivedDataPluginInterface.h"
#include "ShaderCompilerCommon.h"
#include "HlslccDefinitions.h"
#include "MetalBackend.h"

struct FMetalShaderDebugInfoJob
{
	FName ShaderFormat;
	FSHAHash Hash;
	FString CompilerVersion;
	FString MinOSVersion;
	FString DebugInfo;
	FString MathMode;
	FString Standard;
	uint32 SourceCRCLen;
	uint32 SourceCRC;
	
	FString MetalCode;
};

struct FMetalShaderDebugInfo
{
	uint32 UncompressedSize;
	TArray<uint8> CompressedData;
	
	friend FArchive& operator<<( FArchive& Ar, FMetalShaderDebugInfo& Info )
	{
		Ar << Info.UncompressedSize << Info.CompressedData;
		return Ar;
	}
};

class FMetalShaderDebugInfoCooker : public FDerivedDataPluginInterface
{
public:
	FMetalShaderDebugInfoCooker(FMetalShaderDebugInfoJob& Job);
	virtual ~FMetalShaderDebugInfoCooker();
	
#if PLATFORM_MAC || PLATFORM_IOS
#pragma mark - FDerivedDataPluginInterface Interface -
#endif
	virtual const TCHAR* GetPluginName() const override;
	virtual const TCHAR* GetVersionString() const override;
	virtual FString GetPluginSpecificCacheKeySuffix() const override;
	virtual bool IsBuildThreadsafe() const override;
	virtual bool Build(TArray<uint8>& OutData) override;
	
private:
	FMetalShaderDebugInfoJob& Job;
	
	FMetalShaderDebugInfo Output;
};

struct FMetalShaderBytecodeJob
{
	FName ShaderFormat;
	FSHAHash Hash;
	FString TmpFolder;
	FString InputFile;
	FString InputPCHFile;
	FString OutputFile;
	FString OutputObjectFile;
	FString CompilerVersion;
	FString MinOSVersion;
	FString DebugInfo;
	FString MathMode;
	FString Standard;
	uint32 SourceCRCLen;
	uint32 SourceCRC;
	bool bRetainObjectFile;
	bool bCompileAsPCH;
	
	FString Message;
	FString Results;
	FString Errors;
	int32 ReturnCode;
};

struct FMetalShaderBytecode
{
	FString NativePath;
	TArray<uint8> OutputFile;
	TArray<uint8> ObjectFile;
	
	friend FArchive& operator<<( FArchive& Ar, FMetalShaderBytecode& Info )
	{
		Ar << Info.NativePath << Info.OutputFile << Info.ObjectFile;
		return Ar;
	}
};

class FMetalShaderBytecodeCooker : public FDerivedDataPluginInterface
{
public:
	FMetalShaderBytecodeCooker(FMetalShaderBytecodeJob& Job);
	virtual ~FMetalShaderBytecodeCooker();

#if PLATFORM_MAC || PLATFORM_IOS
#pragma mark - FDerivedDataPluginInterface Interface - 
#endif
	virtual const TCHAR* GetPluginName() const override;
	virtual const TCHAR* GetVersionString() const override;
	virtual FString GetPluginSpecificCacheKeySuffix() const override;
	virtual bool IsBuildThreadsafe() const override;
	virtual bool Build(TArray<uint8>& OutData) override;
	
private:
	FMetalShaderBytecodeJob& Job;
	
	FMetalShaderBytecode Output;
};

struct FMetalShaderPreprocessed
{
	FString NativePath;
	TArray<uint8> OutputFile;
	TArray<uint8> ObjectFile;
	
	friend FArchive& operator<<( FArchive& Ar, FMetalShaderPreprocessed& Info )
	{
		Ar << Info.NativePath << Info.OutputFile << Info.ObjectFile;
		return Ar;
	}
};

struct FMetalShaderOutputJob
{
	const FShaderCompilerInput& Input;
	FShaderCompilerOutput& Output;
	const FString& WorkingDirectory;
	FString PreprocessedShader;
	FSHAHash GUIDHash;
	uint8 VersionEnum;
	uint32 CCFlags;
	EHlslCompileTarget HlslCompilerTarget;
	EHlslCompileTarget MetalCompilerTarget;
	EMetalGPUSemantics Semantics;
	EMetalTypeBufferMode TypeMode;
	uint32 MaxUnrollLoops;
	EHlslShaderFrequency Frequency;
	bool bDumpDebugInfo;
	FString Standard;
	FString MinOSVersion;
};

class FMetalShaderOutputCooker : public FDerivedDataPluginInterface
{
public:
	FMetalShaderOutputCooker(const FShaderCompilerInput& _Input,FShaderCompilerOutput& Output,const FString& WorkingDirectory, FString PreprocessedShader, FSHAHash GUIDHash, uint8 VersionEnum, uint32 CCFlags, EHlslCompileTarget HlslCompilerTarget, EHlslCompileTarget MetalCompilerTarget, EMetalGPUSemantics Semantics, EMetalTypeBufferMode TypeMode, uint32 MaxUnrollLoops, EHlslShaderFrequency Frequency, bool bDumpDebugInfo, FString Standard, FString MinOSVersion);
	virtual ~FMetalShaderOutputCooker();

#if PLATFORM_MAC || PLATFORM_IOS
#pragma mark - FDerivedDataPluginInterface Interface - 
#endif
	virtual const TCHAR* GetPluginName() const override;
	virtual const TCHAR* GetVersionString() const override;
	virtual FString GetPluginSpecificCacheKeySuffix() const override;
	virtual bool IsBuildThreadsafe() const override;
	virtual bool Build(TArray<uint8>& OutData) override;
	
private:
	const FShaderCompilerInput& Input;
	FShaderCompilerOutput& Output;
	const FString& WorkingDirectory;
	FString PreprocessedShader;
	FSHAHash GUIDHash;
	uint8 VersionEnum;
	uint32 CCFlags;
	EHlslCompileTarget HlslCompilerTarget;
	EHlslCompileTarget MetalCompilerTarget;
	EMetalGPUSemantics Semantics;
	EMetalTypeBufferMode TypeMode;
	uint32 MaxUnrollLoops;
	EHlslShaderFrequency Frequency;
	bool bDumpDebugInfo;
	FString Standard;
	FString MinOSVersion;
};
