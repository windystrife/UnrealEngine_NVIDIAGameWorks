// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MetalRHIPrivate.h"
#include "MetalCaptureManager.h"
#include "MetalCommandQueue.h"

bool GMetalSupportsCaptureManager = false;

FMetalCaptureManager::FMetalCaptureManager(id<MTLDevice> InDevice, FMetalCommandQueue& InQueue)
: Device(InDevice)
, Queue(InQueue)
{
#if METAL_SUPPORTS_CAPTURE_MANAGER
	if (FApplePlatformMisc::IsOSAtLeastVersion((uint32[]){10, 13, 0}, (uint32[]){11, 0, 0}, (uint32[]){11, 0, 0}))
	{
		GMetalSupportsCaptureManager = true;
	}
	
	if (GMetalSupportsCaptureManager)
	{
		MTLCaptureManager* Manager = [MTLCaptureManager sharedCaptureManager];
		
		Manager.defaultCaptureScope = [Manager newCaptureScopeWithDevice:Device];
		Manager.defaultCaptureScope.label = @"1 Frame";
		
		FMetalCaptureScope DefaultScope;
		DefaultScope.MTLScope = (id<IMTLCaptureScope>)Manager.defaultCaptureScope;
		DefaultScope.Type = EMetalCaptureTypePresent;
		DefaultScope.StepCount = 1;
		DefaultScope.LastTrigger = 0;
		ActiveScopes.Add(DefaultScope);
		[*DefaultScope.MTLScope beginScope];
		
		uint32 PresentStepCounts[] = {2, 5, 10, 15, 30, 60, 90, 120};
		for (uint32 i = 0; i < (sizeof(PresentStepCounts) / sizeof(uint32)); i++)
		{
			FMetalCaptureScope Scope;
			Scope.MTLScope = (id<IMTLCaptureScope>)[Manager newCaptureScopeWithDevice:Device];
			(*Scope.MTLScope).label = [NSString stringWithFormat:@"%u Frame", PresentStepCounts[i]];
			Scope.Type = EMetalCaptureTypePresent;
			Scope.StepCount = PresentStepCounts[i];
			Scope.LastTrigger = 0;
			ActiveScopes.Add(Scope);
			[*Scope.MTLScope beginScope];
		}
	}
#endif
}

FMetalCaptureManager::~FMetalCaptureManager()
{
}

void FMetalCaptureManager::PresentFrame(uint32 FrameNumber)
{
#if METAL_SUPPORTS_CAPTURE_MANAGER
	if (GMetalSupportsCaptureManager)
	{
		for (FMetalCaptureScope& Scope : ActiveScopes)
		{
			uint32 Diff = 0;
			if (FrameNumber > Scope.LastTrigger)
			{
				Diff = FrameNumber - Scope.LastTrigger;
			}
			else
			{
				Diff = (UINT32_MAX - Scope.LastTrigger) + FrameNumber;
			}
			
			if (Diff >= Scope.StepCount)
			{
				[*Scope.MTLScope endScope];
				[*Scope.MTLScope beginScope];
				Scope.LastTrigger = FrameNumber;
			}
		}
	}
	else
#endif
	{
		Queue.InsertDebugCaptureBoundary();
	}
}

void FMetalCaptureManager::BeginCapture(void)
{
#if METAL_SUPPORTS_CAPTURE_MANAGER
	if (GMetalSupportsCaptureManager)
	{
		[[MTLCaptureManager sharedCaptureManager] startCaptureWithDevice:Device];
	}
#endif
}

void FMetalCaptureManager::EndCapture(void)
{
#if METAL_SUPPORTS_CAPTURE_MANAGER
	if (GMetalSupportsCaptureManager)
	{
		[[MTLCaptureManager sharedCaptureManager] stopCapture];
	}
#endif
}

