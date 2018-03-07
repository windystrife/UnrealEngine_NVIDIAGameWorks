// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
RHIUtilities.cpp:
=============================================================================*/

#include "CoreMinimal.h"
#include "HAL/PlatformStackWalk.h"
#include "HAL/IConsoleManager.h"
#include "RHI.h"


TAutoConsoleVariable<FString> FDumpTransitionsHelper::CVarDumpTransitionsForResource(
	TEXT("r.DumpTransitionsForResource"),
	TEXT(""),
	TEXT("Prints callstack when the given resource is transitioned. Only implemented for DX11 at the moment.")
	TEXT("Name of the resource to dump"),
	ECVF_Default);

RHI_API FRHILockTracker GRHILockTracker;

FName FDumpTransitionsHelper::DumpTransitionForResource = NAME_None;
void FDumpTransitionsHelper::DumpTransitionForResourceHandler()
{
	const FString NewValue = CVarDumpTransitionsForResource.GetValueOnGameThread();
	DumpTransitionForResource = FName(*NewValue);
}

void FDumpTransitionsHelper::DumpResourceTransition(const FName& ResourceName, const EResourceTransitionAccess TransitionType)
{
	const FName ResourceDumpName = FDumpTransitionsHelper::DumpTransitionForResource;
	if ((ResourceDumpName != NAME_None) && (ResourceDumpName == ResourceName))
	{
		const uint32 DumpCallstackSize = 2047;
		ANSICHAR DumpCallstack[DumpCallstackSize] = { 0 };

		FPlatformStackWalk::StackWalkAndDump(DumpCallstack, DumpCallstackSize, 2);
		UE_LOG(LogRHI, Log, TEXT("%s transition to: %s"), *ResourceDumpName.ToString(), *FResourceTransitionUtility::ResourceTransitionAccessStrings[(int32)TransitionType]);
		UE_LOG(LogRHI, Log, TEXT("%s"), ANSI_TO_TCHAR(DumpCallstack));
	}
}

FAutoConsoleVariableSink FDumpTransitionsHelper::CVarDumpTransitionsForResourceSink(FConsoleCommandDelegate::CreateStatic(&FDumpTransitionsHelper::DumpTransitionForResourceHandler));

void EnableDepthBoundsTest(FRHICommandList& RHICmdList, float WorldSpaceDepthNear, float WorldSpaceDepthFar, const FMatrix& ProjectionMatrix)
{
	if (GSupportsDepthBoundsTest)
	{
		FVector4 Near = ProjectionMatrix.TransformFVector4(FVector4(0, 0, WorldSpaceDepthNear));
		FVector4 Far = ProjectionMatrix.TransformFVector4(FVector4(0, 0, WorldSpaceDepthFar));
		float DepthNear = Near.Z / Near.W;
		float DepthFar = Far.Z / Far.W;

		DepthFar = FMath::Clamp(DepthFar, 0.0f, 1.0f);
		DepthNear = FMath::Clamp(DepthNear, 0.0f, 1.0f);

		if (DepthNear <= DepthFar)
		{
			DepthNear = 1.0f;
			DepthFar = 0.0f;
		}

		// Note, using a reversed z depth surface
		RHICmdList.EnableDepthBoundsTest(true, DepthFar, DepthNear);
	}
}

void DisableDepthBoundsTest(FRHICommandList& RHICmdList)
{
	if (GSupportsDepthBoundsTest)
	{
		RHICmdList.EnableDepthBoundsTest(false, 0, 1);
	}
}
