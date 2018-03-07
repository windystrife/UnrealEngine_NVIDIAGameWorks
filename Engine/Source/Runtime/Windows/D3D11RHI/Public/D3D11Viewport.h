// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D11Viewport.h: D3D viewport RHI definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RenderResource.h"
#include "RenderUtils.h"

/** A D3D event query resource. */
class FD3D11EventQuery : public FRenderResource
{
public:

	/** Initialization constructor. */
	FD3D11EventQuery(class FD3D11DynamicRHI* InD3DRHI):
		D3DRHI(InD3DRHI)
	{
	}

	/** Issues an event for the query to poll. */
	void IssueEvent();

	/** Waits for the event query to finish. */
	void WaitForCompletion();

	// FRenderResource interface.
	virtual void InitDynamicRHI() override;
	virtual void ReleaseDynamicRHI() override;

private:
	FD3D11DynamicRHI* D3DRHI;
	TRefCountPtr<ID3D11Query> Query;
};

static DXGI_FORMAT GetRenderTargetFormat(EPixelFormat PixelFormat)
{
	DXGI_FORMAT	DXFormat = (DXGI_FORMAT)GPixelFormats[PixelFormat].PlatformFormat;
	switch(DXFormat)
	{
		case DXGI_FORMAT_B8G8R8A8_TYPELESS:		return DXGI_FORMAT_B8G8R8A8_UNORM;
		case DXGI_FORMAT_BC1_TYPELESS:			return DXGI_FORMAT_BC1_UNORM;
		case DXGI_FORMAT_BC2_TYPELESS:			return DXGI_FORMAT_BC2_UNORM;
		case DXGI_FORMAT_BC3_TYPELESS:			return DXGI_FORMAT_BC3_UNORM;
		case DXGI_FORMAT_R16_TYPELESS:			return DXGI_FORMAT_R16_UNORM;
		case DXGI_FORMAT_R8G8B8A8_TYPELESS:		return DXGI_FORMAT_R8G8B8A8_UNORM;
		default: 								return DXFormat;
	}
}

class FD3D11Viewport : public FRHIViewport
{
public:

	FD3D11Viewport(class FD3D11DynamicRHI* InD3DRHI,HWND InWindowHandle,uint32 InSizeX,uint32 InSizeY,bool bInIsFullscreen, EPixelFormat InPreferredPixelFormat);
	~FD3D11Viewport();

	void Resize(uint32 InSizeX, uint32 InSizeY, bool bInIsFullscreen, EPixelFormat PreferredPixelFormat);

	/**
	 * If the swap chain has been invalidated by DXGI, resets the swap chain to the expected state; otherwise, does nothing.
	 * Called once/frame by the game thread on all viewports.
	 * @param bIgnoreFocus - Whether the reset should happen regardless of whether the window is focused.
     */
	void ConditionalResetSwapChain(bool bIgnoreFocus);

	/** Presents the swap chain. 
	 * Returns true if Present was done by Engine.
 	 */
	bool Present(bool bLockToVsync);

	// Accessors.
	FIntPoint GetSizeXY() const { return FIntPoint(SizeX, SizeY); }
	FD3D11Texture2D* GetBackBuffer() const { return BackBuffer; }

	void WaitForFrameEventCompletion()
	{
		FrameSyncEvent.WaitForCompletion();
	}

	void IssueFrameEvent()
	{
		FrameSyncEvent.IssueEvent();
	}

	IDXGISwapChain* GetSwapChain() const { return SwapChain; } 

	virtual void* GetNativeSwapChain() const override { return GetSwapChain(); }
	virtual void* GetNativeBackBufferTexture() const override { return BackBuffer->GetResource(); }
	virtual void* GetNativeBackBufferRT() const override { return BackBuffer->GetRenderTargetView(0, 0); }

	virtual void SetCustomPresent(FRHICustomPresent* InCustomPresent) override
	{
		CustomPresent = InCustomPresent;
	}
	virtual FRHICustomPresent* GetCustomPresent() const { return CustomPresent; }

	virtual void* GetNativeWindow(void** AddParam = nullptr) const override { return (void*)WindowHandle; }

private:

	/** Presents the frame synchronizing with DWM. */
	void PresentWithVsyncDWM();

	/**
	 * Presents the swap chain checking the return result. 
	 * Returns true if Present was done by Engine.
	 */
	bool PresentChecked(int32 SyncInterval);

	FD3D11DynamicRHI* D3DRHI;
	uint64 LastFlipTime;
	uint64 LastFrameComplete;
	uint64 LastCompleteTime;
	int32 SyncCounter;
	bool bSyncedLastFrame;
	HWND WindowHandle;
	uint32 MaximumFrameLatency;
	uint32 SizeX;
	uint32 SizeY;
	bool bIsFullscreen;
	EPixelFormat PixelFormat;
	bool bIsValid;
	TRefCountPtr<IDXGISwapChain> SwapChain;
	TRefCountPtr<FD3D11Texture2D> BackBuffer;

	// Support for selecting non-default output for display in fullscreen exclusive
	TRefCountPtr<IDXGIOutput>	ForcedFullscreenOutput;
	bool						bForcedFullscreenDisplay;

	/** An event used to track the GPU's progress. */
	FD3D11EventQuery FrameSyncEvent;

	FCustomPresentRHIRef CustomPresent;

	DXGI_MODE_DESC SetupDXGI_MODE_DESC() const;
};

template<>
struct TD3D11ResourceTraits<FRHIViewport>
{
	typedef FD3D11Viewport TConcreteType;
};
