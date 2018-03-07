// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once


namespace EAutomationEventName
{
	enum Type
	{
		FPSCapture,
		AutomationTestResults,

		NUM_ENGINE_EVENT_NAMES
	};
}


namespace EAutomationAnalyticParam
{
	enum Type
	{
		MapName,
		MatineeName,
		TimeStamp,
		Platform,
		Spec,
		CL,
		FPS,
		BuildConfiguration,
		AverageFrameTime,
		AverageGameThreadTime,
		AverageRenderThreadTime,
		AverageGPUTime,
		PercentOfFramesAtLeast30FPS,
		PercentOfFramesAtLeast60FPS,

		TestName,
		BeautifiedName,
		ExecutionCount,
		IsSuccess,
		Duration,
		ErrorCount,
		WarningCount,

		NUM_ENGINE_PARAMS
	};
}
