// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FRHICommandListImmediate;

extern UTILITYSHADERS_API void IssueScalableLongGPUTask(FRHICommandListImmediate& RHICmdList, int32 NumIteration = -1);
extern UTILITYSHADERS_API void MeasureLongGPUTaskExecutionTime(FRHICommandListImmediate& RHICmdList);
