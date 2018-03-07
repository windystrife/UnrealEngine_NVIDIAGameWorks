// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "OculusHMDPrivate.h"

#define OCULUS_STRESS_TESTS_ENABLED (OCULUS_HMD_SUPPORTED_PLATFORMS && !UE_BUILD_SHIPPING && !PLATFORM_ANDROID)

#if OCULUS_STRESS_TESTS_ENABLED

namespace OculusHMD
{

//-------------------------------------------------------------------------------------------------
// FStressTester
//-------------------------------------------------------------------------------------------------

class FStressTester
{

public:
	const float MinPixelDensity = 0.4f;
	const float MaxPixelDensity = 2.0f;

	enum EStressTestMode
	{
		STM_None,
		STM_EyeBufferRealloc = 0x01,
		STM_CPUSpin			 = 0x02,
		STM_GPU				 = 0x04,

		STM__All = ((STM_GPU<<1)-1)
	};

	// multiple masks could be set, see EStressTestMode
	void SetStressMode(uint32 InStressMask);
	uint32 GetStressMode() const { return Mode; }

	// sets limits for CPUSpin mode, per frame
	void SetCPUSpinOffPerFrameInSeconds(double InCPUSpinOffInSeconds) { CPUSpinOffInSeconds = InCPUSpinOffInSeconds; }

	// set GPU load multiplier
	// if IterationsMultiplier is 0 then the multiplier will be randomly changed in 1..20 range.
	// the bigger the multiplier the longer it takes GPU to draw the quad.
	void SetGPULoadMultiplier(int IterationsMultiplier) { GPUIterationsMultiplier = IterationsMultiplier; }

	// sets time limit for STM_EyeBufferRealloc mode; 0 - unlimited
	void SetPDsTimeLimitInSeconds(double InSeconds) { PDsTimeLimitInSeconds = InSeconds; }

	// sets time limit for STM_CPUSpin mode; 0 - unlimited
	void SetCPUsTimeLimitInSeconds(double InSeconds) { CPUsTimeLimitInSeconds = InSeconds; }

	// sets time limit for STM_GPU mode; 0 - unlimited
	void SetGPUsTimeLimitInSeconds(double InSeconds) { GPUsTimeLimitInSeconds = InSeconds; }

	static TSharedRef<class FStressTester, ESPMode::ThreadSafe> Get();

	static void TickCPU_GameThread(class FOculusHMD* pPlugin)
	{
		CheckInGameThread();

		if (SharedInstance.IsValid())
		{
			SharedInstance->DoTickCPU_GameThread(pPlugin);
		}
	}

	static void TickGPU_RenderThread(FRHICommandListImmediate& RHICmdList, class FRHITexture2D* BackBuffer, class FRHITexture2D* SrcTexture)
	{
		CheckInRenderThread();

		if (SharedInstance.IsValid())
		{
			SharedInstance->DoTickGPU_RenderThread(RHICmdList, BackBuffer, SrcTexture);
		}
	}

protected:
	void DoTickCPU_GameThread(class FOculusHMD* pPlugin);
	void DoTickGPU_RenderThread(FRHICommandListImmediate& RHICmdList, class FRHITexture2D* BackBuffer, class FRHITexture2D* SrcTexture);

	FStressTester();

	uint32 Mode; // bit mask, see EStressTestMode
	double CPUSpinOffInSeconds; // limit of additional CPU load per frame, STM_CPUSpin
	double PDsTimeLimitInSeconds;  // time limit for STM_EyeBufferRealloc mode; 0 - unlimited
	double CPUsTimeLimitInSeconds; // time limit for STM_CPUSpin mode; 0 - unlimited
	double GPUsTimeLimitInSeconds; // time limit for STM_GPU mode; 0 - unlimited

	// the higher multiplier the longer it takes GPU to draw
	int GPUIterationsMultiplier; // if 0 - then it is dynamically changed.

	double CPUStartTimeInSeconds;
	double GPUStartTimeInSeconds;
	double PDStartTimeInSeconds;

	static TSharedPtr<class FStressTester, ESPMode::ThreadSafe> SharedInstance;
};


} // namespace OculusHMD

#endif // #if OCULUS_STRESS_TESTS_ENABLED
