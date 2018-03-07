// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ShaderPipelineCompiler.cpp: Platform independent shader pipeline compilations.
=============================================================================*/

#include "CoreMinimal.h"
#include "Interfaces/IShaderFormat.h"
#include "ShaderCompiler.h"

bool CompileShaderPipeline(const IShaderFormat* Compiler, FName Format, FShaderPipelineCompileJob* PipelineJob, const FString& Dir)
{
	check(PipelineJob && PipelineJob->StageJobs.Num() > 0);

	FShaderCompileJob* CurrentJob = PipelineJob->StageJobs[0]->GetSingleShaderJob();

	CurrentJob->Input.bCompilingForShaderPipeline = true;

	// First job doesn't have to trim outputs
	CurrentJob->Input.bIncludeUsedOutputs = false;

	if (IsValidRef(CurrentJob->Input.SharedEnvironment))
	{
		// Merge the shared environment into the per-shader environment before calling into the compile function
		// Normally this happens in the worker
		CurrentJob->Input.Environment.Merge(*CurrentJob->Input.SharedEnvironment);
	}

	// Compile the shader directly through the platform dll (directly from the shader dir as the working directory)
	Compiler->CompileShader(Format, CurrentJob->Input, CurrentJob->Output, Dir);

	CurrentJob->bSucceeded = CurrentJob->Output.bSucceeded;

	if (!CurrentJob->Output.bSucceeded)
	{
		// Can't carry on compiling the pipeline
		return false;
	}

	// Generate a hash of the output and cache it
	// The shader processing this output will use it to search for existing FShaderResources
	CurrentJob->Output.GenerateOutputHash();

	// This tells the shader compiler we do want to remove unused outputs
	bool bEnableRemovingUnused = true;
	// This tells us the hlsl parser failed at removing unused outputs
	bool bJobFailedRemovingUnused = false;

	//#todo-rco: Currently only removes for pure VS & PS stages
	for (int32 Index = 0; Index < PipelineJob->StageJobs.Num(); ++Index)
	{
		auto Stage = PipelineJob->StageJobs[Index]->GetSingleShaderJob()->Input.Target.Frequency;
		if (Stage != SF_Vertex && Stage != SF_Pixel)
		{
			bEnableRemovingUnused = false;
			break;
		}
	}

	for (int32 Index = 1; Index < PipelineJob->StageJobs.Num(); ++Index)
	{
		auto* PreviousJob = CurrentJob;
		CurrentJob = PipelineJob->StageJobs[Index]->GetSingleShaderJob();
		bEnableRemovingUnused = bEnableRemovingUnused && PreviousJob->Output.bSupportsQueryingUsedAttributes;
		if (bEnableRemovingUnused)
		{
			CurrentJob->Input.bIncludeUsedOutputs = true;
			CurrentJob->Input.bCompilingForShaderPipeline = true;
			CurrentJob->Input.UsedOutputs = PreviousJob->Output.UsedAttributes;
		}

		if (IsValidRef(CurrentJob->Input.SharedEnvironment))
		{
			// Merge the shared environment into the per-shader environment before calling into the compile function
			// Normally this happens in the worker
			CurrentJob->Input.Environment.Merge(*CurrentJob->Input.SharedEnvironment);
		}

		// Compile the shader directly through the platform dll (directly from the shader dir as the working directory)
		Compiler->CompileShader(Format, CurrentJob->Input, CurrentJob->Output, Dir);

		CurrentJob->bSucceeded = CurrentJob->Output.bSucceeded;

		if (!CurrentJob->Output.bSucceeded)
		{
			// Can't carry on compiling the pipeline
			return false;
		}

		bJobFailedRemovingUnused = CurrentJob->Output.bFailedRemovingUnused || bJobFailedRemovingUnused;

		// Generate a hash of the output and cache it
		// The shader processing this output will use it to search for existing FShaderResources
		CurrentJob->Output.GenerateOutputHash();
	}

	PipelineJob->bSucceeded = true;
	PipelineJob->bFailedRemovingUnused = bJobFailedRemovingUnused;
	return true;
}
