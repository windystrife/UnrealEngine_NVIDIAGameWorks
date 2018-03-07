// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../../RHI/Public/RHIDefinitions.h"

// Cross compiler support/common functionality
extern SHADERCOMPILERCOMMON_API FString CreateCrossCompilerBatchFileContents(
											const FString& ShaderFile,
											const FString& OutputFile,
											const FString& FrequencySwitch,
											const FString& EntryPoint,
											const FString& VersionSwitch,
											const FString& ExtraArguments = TEXT(""));
