// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "hlslcc.h"

enum class EVulkanShaderVersion
{
	ES3_1,
	SM4_UB,
	ES3_1_ANDROID,
	SM4,
	SM5,
	SM5_UB,
};
extern void CompileShader_Windows_Vulkan(const struct FShaderCompilerInput& Input,struct FShaderCompilerOutput& Output,const class FString& WorkingDirectory, EVulkanShaderVersion Version);


// Hold information to be able to call the compilers
struct FCompilerInfo
{
	const struct FShaderCompilerInput& Input;
	FString WorkingDirectory;
	FString Profile;
	uint32 CCFlags;
	EHlslShaderFrequency Frequency;
	bool bDebugDump;
	FString BaseSourceFilename;

	FCompilerInfo(const struct FShaderCompilerInput& InInput, const FString& InWorkingDirectory, EHlslShaderFrequency InFrequency);
};


struct FSpirv
{
	TArray<uint8> Data;
	struct FEntry
	{
		FString Name;
		int32 Binding;
	};
	TArray<FEntry> ReflectionInfo;

	int32 FindBinding(const FString& Name, bool bOuter = false) const
	{
		for (const auto& Entry : ReflectionInfo)
		{
			if (Entry.Name == Name)
			{
				if (Entry.Binding == -1 && !bOuter)
				{
					// Try the outer group variable; eg 
					// layout(set=0,binding=0) buffer  CulledObjectBounds_BUFFER { vec4 CulledObjectBounds[]; };
					FString OuterName = Name;
					OuterName += TEXT("_BUFFER");
					return FindBinding(OuterName, true);
				}

				return Entry.Binding;
			}
		}

		return -1;
	}
};
extern bool GenerateSpirv(const ANSICHAR* Source, FCompilerInfo& CompilerInfo, FString& OutErrors, const FString& DumpDebugInfoPath, FSpirv& OutSpirv);
