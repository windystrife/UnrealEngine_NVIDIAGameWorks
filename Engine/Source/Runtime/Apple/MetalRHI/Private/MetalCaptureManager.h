// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include <Metal/Metal.h>

NS_ASSUME_NONNULL_BEGIN

/**
 * Equivalent to MTLCaptureScope so that we may compile against older SDK versions.
 */
@protocol IMTLCaptureScope <NSObject>
- (void)beginScope;
- (void)endScope;

@property (nullable, copy, atomic) NSString *label;
@property (nonnull, readonly, nonatomic)  id<MTLDevice> device;
@property (nullable, readonly, nonatomic) id<MTLCommandQueue> commandQueue;
@end

NS_ASSUME_NONNULL_END

class FMetalCommandQueue;

class FMetalCaptureManager
{
public:
	FMetalCaptureManager(id<MTLDevice> Device, FMetalCommandQueue& Queue);
	~FMetalCaptureManager();
	
	// Called by the MetalRHI code to trigger the provided capture scopes visible in Xcode.
	void PresentFrame(uint32 FrameNumber);
	
	// Programmatic captures without an Xcode capture scope.
	// Use them to instrument the code manually to debug issues.
	void BeginCapture(void);
	void EndCapture(void);
	
private:
	TMetalPtr<id<MTLDevice>> Device;
	FMetalCommandQueue& Queue;
	bool bSupportsCaptureManager;
	
private:
	enum EMetalCaptureType
	{
		EMetalCaptureTypeUnknown,
		EMetalCaptureTypeFrame, // (BeginFrame-EndFrame) * StepCount
		EMetalCaptureTypePresent, // (Present-Present) * StepCount
		EMetalCaptureTypeViewport, // (Present-Present) * Viewports * StepCount
	};

	struct FMetalCaptureScope
	{
		EMetalCaptureType Type;
		uint32 StepCount;
		uint32 LastTrigger;
		TMetalPtr<id<IMTLCaptureScope>> MTLScope;
	};
	
	TArray<FMetalCaptureScope> ActiveScopes;
};
