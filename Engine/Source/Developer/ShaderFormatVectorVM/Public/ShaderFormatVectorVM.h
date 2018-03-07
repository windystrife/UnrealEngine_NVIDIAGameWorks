// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VectorVM.h"

/** Data which is generated from the hlsl by the VectorVMBackend and fed back into the */
struct FVectorVMCompilationOutput
{
	FVectorVMCompilationOutput() {}

	TArray<uint8> ByteCode;

	TArray<int32> InternalConstantOffsets;
	TArray<uint8> InternalConstantData;
	TArray<EVectorVMBaseTypes> InternalConstantTypes;

	/** Ordered table of functions actually called by the VM script. */
	struct FCalledVMFunction
	{
		FString Name;
		TArray<bool> InputParamLocations;
		int32 NumOutputs;
		FCalledVMFunction() :NumOutputs(0) {}
	};
	TArray<FCalledVMFunction> CalledVMFunctionTable;

	FString Errors;
};

extern bool SHADERFORMATVECTORVM_API CompileShader_VectorVM(const struct FShaderCompilerInput& Input, struct FShaderCompilerOutput& Output, const class FString& WorkingDirectory, uint8 Version);

//Cheating hack version. To be removed when we add all the plumbing for VVM scripts to be treat like real shaders.
extern bool SHADERFORMATVECTORVM_API CompileShader_VectorVM(const struct FShaderCompilerInput& Input,struct FShaderCompilerOutput& Output,const class FString& WorkingDirectory, uint8 Version, struct FVectorVMCompilationOutput& VMCompilationOutput);
