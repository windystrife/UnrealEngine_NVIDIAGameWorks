#pragma once

// NvFlow begin

struct FFlowTimeStepper
{
	float DeltaTime;
	float TimeError;
	float FixedDt;
	int32 MaxSteps;
	int32 NumSteps;

	FFlowTimeStepper();
	int32 GetNumSteps(float TimeStep);
};

// NvFlow end