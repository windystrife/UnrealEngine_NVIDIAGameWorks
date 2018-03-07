// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

extern void CompileShader_Metal(const struct FShaderCompilerInput& Input, struct FShaderCompilerOutput& Output, const class FString& WorkingDirectory);
extern uint32 GetMetalFormatVersion(FName Format);

// Set this to 0 to get shader source in the graphics debugger
// Note: Offline and runtime compiled shaders have separate DDC versions and can co-exist
#define METAL_OFFLINE_COMPILE (PLATFORM_MAC || PLATFORM_WINDOWS || PLATFORM_LINUX)

