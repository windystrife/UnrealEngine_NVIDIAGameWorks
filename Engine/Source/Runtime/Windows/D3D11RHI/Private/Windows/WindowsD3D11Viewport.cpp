// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D11Viewport.cpp: D3D viewport RHI implementation.
=============================================================================*/

#include "D3D11RHIPrivate.h"
#include "RenderCore.h"
#include "Misc/CommandLine.h"

#include "AllowWindowsPlatformTypes.h"
#include <dwmapi.h>

extern FD3D11Texture2D* GetSwapChainSurface(FD3D11DynamicRHI* D3DRHI, EPixelFormat PixelFormat, IDXGISwapChain* SwapChain);

FD3D11Viewport::FD3D11Viewport(FD3D11DynamicRHI* InD3DRHI,HWND InWindowHandle,uint32 InSizeX,uint32 InSizeY,bool bInIsFullscreen, EPixelFormat InPreferredPixelFormat):
	D3DRHI(InD3DRHI),
	LastFlipTime(0),
	LastFrameComplete(0),
	LastCompleteTime(0),
	SyncCounter(0),
	bSyncedLastFrame(false),
	WindowHandle(InWindowHandle),
	MaximumFrameLatency(3),
	SizeX(InSizeX),
	SizeY(InSizeY),
	bIsFullscreen(bInIsFullscreen),
	PixelFormat(InPreferredPixelFormat),
	bIsValid(true),
	FrameSyncEvent(InD3DRHI)
{
	check(IsInGameThread());
	D3DRHI->Viewports.Add(this);

	// Ensure that the D3D device has been created.
	D3DRHI->InitD3DDevice();

	// Create a backbuffer/swapchain for each viewport
	TRefCountPtr<IDXGIDevice> DXGIDevice;
	VERIFYD3D11RESULT_EX(D3DRHI->GetDevice()->QueryInterface(IID_IDXGIDevice, (void**)DXGIDevice.GetInitReference()), D3DRHI->GetDevice());

	// If requested, keep a handle to a DXGIOutput so we can force that display on fullscreen swap
	uint32 DisplayIndex = D3DRHI->GetHDRDetectedDisplayIndex();
	bForcedFullscreenDisplay = FParse::Value(FCommandLine::Get(), TEXT("FullscreenDisplay="), DisplayIndex);

	if (bForcedFullscreenDisplay || GRHISupportsHDROutput)
	{
		TRefCountPtr<IDXGIAdapter> DXGIAdapter;
		DXGIDevice->GetAdapter((IDXGIAdapter**)DXGIAdapter.GetInitReference());

		if (S_OK != DXGIAdapter->EnumOutputs(DisplayIndex, ForcedFullscreenOutput.GetInitReference()))
		{
			UE_LOG(LogD3D11RHI, Log, TEXT("Failed to find requested output display (%i)."), DisplayIndex);
			ForcedFullscreenOutput = nullptr;
			bForcedFullscreenDisplay = false;
		}
	}
	else
	{
		ForcedFullscreenOutput = nullptr;
	}

	if (PixelFormat == PF_FloatRGBA && bIsFullscreen)
	{
		// Send HDR meta data to enable
		D3DRHI->EnableHDR();
	}

	// Create the swapchain.
	DXGI_SWAP_CHAIN_DESC SwapChainDesc;
	FMemory::Memzero( &SwapChainDesc, sizeof(DXGI_SWAP_CHAIN_DESC) );

	SwapChainDesc.BufferDesc = SetupDXGI_MODE_DESC();
	// MSAA Sample count
	SwapChainDesc.SampleDesc.Count = 1;
	SwapChainDesc.SampleDesc.Quality = 0;
	SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
	// 1:single buffering, 2:double buffering, 3:triple buffering
	SwapChainDesc.BufferCount = 1;
	SwapChainDesc.OutputWindow = WindowHandle;
	SwapChainDesc.Windowed = !bIsFullscreen;
	// DXGI_SWAP_EFFECT_DISCARD / DXGI_SWAP_EFFECT_SEQUENTIAL
	SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	SwapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	VERIFYD3D11RESULT_EX(D3DRHI->GetFactory()->CreateSwapChain(DXGIDevice,&SwapChainDesc,SwapChain.GetInitReference()), D3DRHI->GetDevice());

	// Set the DXGI message hook to not change the window behind our back.
	D3DRHI->GetFactory()->MakeWindowAssociation(WindowHandle,DXGI_MWA_NO_WINDOW_CHANGES);

	// Create a RHI surface to represent the viewport's back buffer.
	BackBuffer = GetSwapChainSurface(D3DRHI, PixelFormat, SwapChain);

	// Tell the window to redraw when they can.
	// @todo: For Slate viewports, it doesn't make sense to post WM_PAINT messages (we swallow those.)
	::PostMessage( WindowHandle, WM_PAINT, 0, 0 );

	BeginInitResource(&FrameSyncEvent);
}

void FD3D11Viewport::ConditionalResetSwapChain(bool bIgnoreFocus)
{
	if(!bIsValid)
	{
		// Check if the viewport's window is focused before resetting the swap chain's fullscreen state.
		HWND FocusWindow = ::GetFocus();
		const bool bIsFocused = FocusWindow == WindowHandle;
		const bool bIsIconic = !!::IsIconic( WindowHandle );
		if(bIgnoreFocus || (bIsFocused && !bIsIconic) )
		{
			FlushRenderingCommands();

			// Explicit output selection in fullscreen only (commandline or HDR enabled)
			bool bNeedsForcedDisplay = bIsFullscreen && (bForcedFullscreenDisplay || PixelFormat == PF_FloatRGBA);
			HRESULT Result = SwapChain->SetFullscreenState(bIsFullscreen, bNeedsForcedDisplay ? ForcedFullscreenOutput : nullptr);

			if(SUCCEEDED(Result))
			{
				bIsValid = true;
			}
			else
			{
				// Even though the docs say SetFullscreenState always returns S_OK, that doesn't always seem to be the case.
				UE_LOG(LogD3D11RHI, Log, TEXT("IDXGISwapChain::SetFullscreenState returned %08x; waiting for the next frame to try again."),Result);
			}
		}
	}
}

#include "HideWindowsPlatformTypes.h"
