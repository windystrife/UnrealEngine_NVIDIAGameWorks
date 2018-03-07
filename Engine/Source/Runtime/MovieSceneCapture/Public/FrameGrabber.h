// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/ThreadSafeCounter.h"
#include "HAL/ThreadSafeBool.h"
#include "Slate/SceneViewport.h"

class SWindow;

/** A single, managed surface used as a render target resolution destination  */
struct MOVIESCENECAPTURE_API FViewportSurfaceReader
{
	/** Constructor */
	FViewportSurfaceReader(EPixelFormat InPixelFormat, FIntPoint InBufferSize);

	/** Destructor */
	~FViewportSurfaceReader();

	/** Initialize this reader so that it can be waited on. */
	void Initialize();

	/** Wait for this reader to become available, if it's currently in use */
	void BlockUntilAvailable();

	/**
	 * Resolve the specified viewport RHI, calling the specified callback with the result.
	 *
	 * @param	ViewportRHI		The viewport to resolve
	 * @param	Callback 		Callback to call with the locked texture data. This will be called on an undefined thread.
	 */
	void ResolveRenderTarget(const FViewportRHIRef& ViewportRHI, TFunction<void(FColor*, int32, int32)> Callback);

	/** Get the current size of the texture */
	FIntPoint GetCurrentSize() const;

	/** Set the rectangle within which to read pixels */
	void SetCaptureRect(FIntRect InCaptureRect) { CaptureRect = InCaptureRect; }

protected:

	/** Set up this surface to the specified width/height */
	void Resize(uint32 Width, uint32 Height);

	/** Whether this surface reader is enabled or not */
	FThreadSafeBool bEnabled;

	/** Optional event that is triggered when the surface is no longer in use */
	FEvent* AvailableEvent;

	/** Texture used to store the resolved render target */
	FTexture2DRHIRef ReadbackTexture;

	/** The rectangle to read from the surface */
	FIntRect CaptureRect;

	/** The desired pixel format of the resolved textures */
	EPixelFormat PixelFormat;

	/** Whether this reader is enabled or not. */
	bool bIsEnabled;
};

struct IFramePayload
{
	virtual ~IFramePayload() {}
};

typedef TSharedPtr<IFramePayload, ESPMode::ThreadSafe> FFramePayloadPtr;

/** Structure representing a captured frame */
struct FCapturedFrameData
{
	FCapturedFrameData(FIntPoint InBufferSize, FFramePayloadPtr InPayload) : BufferSize(InBufferSize), Payload(MoveTemp(InPayload)) {}

	FCapturedFrameData(FCapturedFrameData&& In) : ColorBuffer(MoveTemp(In.ColorBuffer)), BufferSize(In.BufferSize), Payload(MoveTemp(In.Payload)) {}
	FCapturedFrameData& operator=(FCapturedFrameData&& In){ ColorBuffer = MoveTemp(In.ColorBuffer); BufferSize = In.BufferSize; Payload = MoveTemp(In.Payload); return *this; }

	template<typename T>
	T* GetPayload() { return static_cast<T*>(Payload.Get()); }

	/** The color buffer of the captured frame */
	TArray<FColor> ColorBuffer;

	/** The size of the resulting color buffer */
	FIntPoint BufferSize;

	/** Optional user-specified payload */
	FFramePayloadPtr Payload;
	
private:
	FCapturedFrameData(const FCapturedFrameData& In);
	FCapturedFrameData& operator=(const FCapturedFrameData& In);
};

/**
 * Class responsible for resolving render target data for a specific viewport in an efficient manner
 * Internally, the class uses a fixed array of resolution surfaces, and dispatches rendering commands
 * to resolve the viewport render target into a specific index into this array. This means we can
 * resolve the render target data without having to wait, or flush rendering commands.
 */
class MOVIESCENECAPTURE_API FFrameGrabber
{
public:
	/**
	 * Construct this frame grabber
	 *
	 * @param InViewport		The viewport we are to grab frames for
	 * @param DesiredBufferSize	The desired size of captured frames
	 * @param InPixelFormat		The desired pixel format to store captured frames as
	 * @param InNumSurfaces		The number of destination surfaces contained in our buffer 
	 */
	FFrameGrabber(TSharedRef<FSceneViewport> Viewport, FIntPoint DesiredBufferSize, EPixelFormat InPixelFormat = PF_B8G8R8A8, uint32 NumSurfaces = 2);

	/** Destructor */
	~FFrameGrabber();

public:

	/** Instruct the frame grabber to start capturing frames */
	void StartCapturingFrames();

	/** Instruct the frame grabber capture this frame, when it receives an event from slate */
	void CaptureThisFrame(FFramePayloadPtr Payload);

	/** Stop capturing frames */
	void StopCapturingFrames();

	/** Shut down this grabber, ensuring that any threaded operations are finished */
	void Shutdown();

public:

	/** Check whether we have any outstanding frames or not */
	bool HasOutstandingFrames() const;

	/** Retrieve any frames we may have captured */
	TArray<FCapturedFrameData> GetCapturedFrames();

protected:
	
	/** Callback for when slate has rendered a window (called on game thread) */
	void OnSlateWindowRendered( SWindow& SlateWindow, void* ViewportRHIPtr );

	/** Called when the specified surface index has been locked for reading with the render target data (called on render thread)  */
	void OnFrameReady(int32 SurfaceIndex, FColor* ColorBuffer, int32 Width, int32 Height);

private:

	/** Non-copyable */
	FFrameGrabber(const FFrameGrabber&);
	FFrameGrabber& operator=(const FFrameGrabber&);

	/** The window we'll capture, and the capture rectangle */
	TWeakPtr<SWindow> CaptureWindow;

	/** Delegate handle for the OnSlateOnWindowRendered event */
	FDelegateHandle OnWindowRendered;

	/** Array of captured frames */
	TArray<FCapturedFrameData> CapturedFrames;

	/** Lock to protect the above array */
	mutable FCriticalSection CapturedFramesMutex;

	/** Array of surfaces that we resolve the viewport RHI to. Fixed allocation - should never be resized */
	struct FResolveSurface
	{
		FResolveSurface(EPixelFormat InPixelFormat, FIntPoint BufferSize) : Surface(InPixelFormat, BufferSize) {}

		FFramePayloadPtr Payload;
		FViewportSurfaceReader Surface;
	};
	TArray<FResolveSurface> Surfaces;

	/** Index into the above array to the next surface that we should use - only accessed on main thread */
	int32 CurrentFrameIndex;

	/** The index that we should capture the next rendered slate window into */
	int32 SlateRenderIndex;

	/** The total number of frames we are currently waiting on */
	FThreadSafeCounter OutstandingFrameCount;

	/** Pending frame payloads to be passed with frames captured from slate  */
	FCriticalSection PendingFramePayloadsMutex;
	TArray<FFramePayloadPtr> PendingFramePayloads;

	/** Optional RAII shutdown functor */
	TFunction<void()> OnShutdown;

	/** The current state of the grabber */
	enum class EFrameGrabberState
	{
		Inactive, Active, PendingShutdown
	};
	EFrameGrabberState State;

	/** The desired target size to resolve frames to */
	FIntPoint TargetSize;
};
