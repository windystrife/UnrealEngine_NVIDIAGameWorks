// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D12Viewport.cpp: D3D viewport RHI implementation.
=============================================================================*/

#include "D3D12RHIPrivate.h"
#include "RenderCore.h"

#include "AllowWindowsPlatformTypes.h"
#include "Windows.h"

extern FD3D12Texture2D* GetSwapChainSurface(FD3D12Device* Parent, EPixelFormat PixelFormat, IDXGISwapChain* SwapChain, const uint32 &backBufferIndex);

FD3D12Viewport::FD3D12Viewport(class FD3D12Adapter* InParent, HWND InWindowHandle, uint32 InSizeX, uint32 InSizeY, bool bInIsFullscreen, EPixelFormat InPreferredPixelFormat) :
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
	bHDRMetaDataSet(false),
	ColorSpace(DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709),
	bIsValid(true),
	NumBackBuffers(DefaultNumBackBuffers),
	CurrentBackBufferIndex_RenderThread(0),
	BackBuffer_RenderThread(nullptr),
	CurrentBackBufferIndex_RHIThread(0),
	BackBuffer_RHIThread(nullptr),
	Fence(InParent, L"Viewport Fence"),
	LastSignaledValue(0),
	pCommandQueue(nullptr),
#if PLATFORM_SUPPORTS_MGPU
	FramePacerRunnable(nullptr),
#endif //PLATFORM_SUPPORTS_MGPU
	SDRBackBuffer_RenderThread(nullptr),
	SDRBackBuffer_RHIThread(nullptr),
	SDRPixelFormat(PF_B8G8R8A8),
	FD3D12AdapterChild(InParent)
{
	check(IsInGameThread());
	GetParentAdapter()->GetViewports().Add(this);
}

//Init for a Viewport that will do the presenting
void FD3D12Viewport::Init()
{

	FD3D12Adapter* Adapter = GetParentAdapter();

	bAllowTearing = false;
	IDXGIFactory* Factory = Adapter->GetDXGIFactory2();
	if(Factory)
	{
		TRefCountPtr<IDXGIFactory5> Factory5;
		Factory->QueryInterface(IID_PPV_ARGS(Factory5.GetInitReference()));
		if (Factory5.IsValid())
		{
			BOOL AllowTearing;
			if(SUCCEEDED(Factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &AllowTearing, sizeof(AllowTearing))) && AllowTearing)
			{
				bAllowTearing = true;
			}
		}
	}

	Fence.CreateFence();

	CalculateSwapChainDepth();

	UINT SwapChainFlags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	if (bAllowTearing)
	{
		SwapChainFlags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
	}

	const DXGI_MODE_DESC BufferDesc = SetupDXGI_MODE_DESC();

	// Create the swapchain.
	{
		DXGI_SWAP_CHAIN_DESC SwapChainDesc = {};
		SwapChainDesc.BufferDesc = BufferDesc;
		// MSAA Sample count
		SwapChainDesc.SampleDesc.Count = 1;
		SwapChainDesc.SampleDesc.Quality = 0;
		SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
		// 1:single buffering, 2:double buffering, 3:triple buffering
		SwapChainDesc.BufferCount = NumBackBuffers;
		SwapChainDesc.OutputWindow = WindowHandle;
		SwapChainDesc.Windowed = !bIsFullscreen;
		// DXGI_SWAP_EFFECT_DISCARD / DXGI_SWAP_EFFECT_SEQUENTIAL
		SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		SwapChainDesc.Flags = SwapChainFlags;

		pCommandQueue = Adapter->GetDevice()->GetCommandListManager().GetD3DCommandQueue();

		TRefCountPtr<IDXGISwapChain> SwapChain;
		VERIFYD3D12RESULT(Adapter->GetDXGIFactory2()->CreateSwapChain(pCommandQueue, &SwapChainDesc, SwapChain.GetInitReference()));
		VERIFYD3D12RESULT(SwapChain->QueryInterface(IID_PPV_ARGS(SwapChain1.GetInitReference())));

		// Get a SwapChain4 if supported.
		SwapChain->QueryInterface(IID_PPV_ARGS(SwapChain4.GetInitReference()));
	}

	// Set the DXGI message hook to not change the window behind our back.
	Adapter->GetDXGIFactory2()->MakeWindowAssociation(WindowHandle, DXGI_MWA_NO_WINDOW_CHANGES);

	// Resize to setup mGPU correctly.
	Resize(BufferDesc.Width, BufferDesc.Height, bIsFullscreen, PixelFormat);

	// Tell the window to redraw when they can.
	// @todo: For Slate viewports, it doesn't make sense to post WM_PAINT messages (we swallow those.)
	::PostMessage(WindowHandle, WM_PAINT, 0, 0);
}

void FD3D12Viewport::ConditionalResetSwapChain(bool bIgnoreFocus)
{
}

void FD3D12Viewport::ResizeInternal()
{
	FD3D12Adapter* Adapter = GetParentAdapter();

	CalculateSwapChainDepth();

	UINT SwapChainFlags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	if (bAllowTearing)
	{
		SwapChainFlags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
	}

#if PLATFORM_SUPPORTS_MGPU
	if (Adapter->AlternateFrameRenderingEnabled())
	{
		TArray<ID3D12CommandQueue*> CommandQueues;
		TArray<uint32> NodeMasks;

		uint32 GPUIndex = 0;

		// Interleave the swapchains between the AFR devices
		for (uint32 i = 0; i < NumBackBuffers; ++i)
		{
			FD3D12Device* Device = Adapter->GetDeviceByIndex(GPUIndex);

			CommandQueues.Add(Device->GetCommandListManager().GetD3DCommandQueue());
			NodeMasks.Add(Device->GetNodeMask());

			GPUIndex++;
			GPUIndex %= Adapter->GetNumGPUNodes();
		}

		TRefCountPtr<IDXGISwapChain3> SwapChain3;
		VERIFYD3D12RESULT(SwapChain1->QueryInterface(IID_PPV_ARGS(SwapChain3.GetInitReference())));
		VERIFYD3D12RESULT_EX(SwapChain3->ResizeBuffers1(NumBackBuffers, SizeX, SizeY, GetRenderTargetFormat(PixelFormat), SwapChainFlags, NodeMasks.GetData(), (IUnknown**)CommandQueues.GetData()), Adapter->GetD3DDevice());

		GPUIndex = 0;
		for (uint32 i = 0; i < NumBackBuffers; ++i)
		{
			FD3D12Device* Device = Adapter->GetDeviceByIndex(GPUIndex);

			check(BackBuffers[i].GetReference() == nullptr);
			BackBuffers[i] = GetSwapChainSurface(Device, PixelFormat, SwapChain1, i);

			GPUIndex++;
			GPUIndex %= Adapter->GetNumGPUNodes();
		}
	}
	else
#endif // PLATFORM_SUPPORTS_MGPU
	{
		VERIFYD3D12RESULT_EX(SwapChain1->ResizeBuffers(NumBackBuffers, SizeX, SizeY, GetRenderTargetFormat(PixelFormat), SwapChainFlags), Adapter->GetD3DDevice());

		FD3D12Device* Device = Adapter->GetDeviceByIndex(0);
		for (uint32 i = 0; i < NumBackBuffers; ++i)
		{
			check(BackBuffers[i].GetReference() == nullptr);
			BackBuffers[i] = GetSwapChainSurface(Device, PixelFormat, SwapChain1, i);
		}
	}

	CurrentBackBufferIndex_RenderThread = 0;
	BackBuffer_RenderThread = BackBuffers[CurrentBackBufferIndex_RenderThread].GetReference();
	CurrentBackBufferIndex_RHIThread = 0;
	BackBuffer_RHIThread = BackBuffers[CurrentBackBufferIndex_RHIThread].GetReference();

	SDRBackBuffer_RenderThread = SDRBackBuffers[CurrentBackBufferIndex_RenderThread].GetReference();
	SDRBackBuffer_RHIThread = SDRBackBuffers[CurrentBackBufferIndex_RHIThread].GetReference();

}

HRESULT FD3D12Viewport::PresentInternal(int32 SyncInterval)
{
	UINT Flags = 0;

	if(!SyncInterval && !bIsFullscreen && bAllowTearing)
	{
		Flags |= DXGI_PRESENT_ALLOW_TEARING;
	}

	return SwapChain1->Present(SyncInterval, Flags);
}

void FD3D12Viewport::EnableHDR()
{
	if ( GRHISupportsHDROutput && IsHDREnabled() )
	{
		static const auto CVarHDROutputDevice = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.HDR.Display.OutputDevice"));
		const EDisplayFormat OutputDevice = EDisplayFormat(CVarHDROutputDevice->GetValueOnAnyThread());

		const float DisplayMaxOutputNits = (OutputDevice == DF_ACES2000_ST_2084 || OutputDevice == DF_ACES2000_ScRGB) ? 2000.f : 1000.f;
		const float DisplayMinOutputNits = 0.0f;	// Min output of the display
		const float DisplayMaxCLL = 0.0f;			// Max content light level in lumens (0.0 == unknown)
		const float DisplayFALL = 0.0f;				// Frame average light level (0.0 == unknown)

		// Ideally we can avoid setting TV meta data and instead the engine can do tone mapping based on the
		// actual current display properties (display mapping).
		static const auto CVarHDRColorGamut = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.HDR.Display.ColorGamut"));
		const EDisplayGamut DisplayGamut = EDisplayGamut(CVarHDRColorGamut->GetValueOnAnyThread());
		SetHDRTVMode(true,
			DisplayGamut,
			DisplayMaxOutputNits,
			DisplayMinOutputNits,
			DisplayMaxCLL,
			DisplayFALL);

		// Ensure we have the correct color space set.
		EnsureColorSpace(DisplayGamut, OutputDevice);
	}
}

void FD3D12Viewport::ShutdownHDR()
{
	if (GRHISupportsHDROutput)
	{
		// Default SDR display data
		const EDisplayGamut DisplayGamut = DG_Rec709;
		const EDisplayFormat OutputDevice = DF_sRGB;

		// Note: These values aren't actually used.
		const float DisplayMaxOutputNits = 100.0f;	// Max output of the display
		const float DisplayMinOutputNits = 0.0f;	// Min output of the display
		const float DisplayMaxCLL = 100.0f;			// Max content light level in lumens
		const float DisplayFALL = 20.0f;			// Frame average light level

		// Ideally we can avoid setting TV meta data and instead the engine can do tone mapping based on the
		// actual current display properties (display mapping).
		SetHDRTVMode(
			false,
			DisplayGamut,
			DisplayMaxOutputNits,
			DisplayMinOutputNits,
			DisplayMaxCLL,
			DisplayFALL);

		// Ensure we have the correct color space set.
		EnsureColorSpace(DisplayGamut, OutputDevice);
	}
}

bool FD3D12Viewport::CurrentOutputSupportsHDR() const
{
	// Default to no support.
	bool bOutputSupportsHDR = false;

	if (SwapChain4.GetReference())
	{
		// Output information is cached on the DXGI Factory. If it is stale we need to create
		// a new factory which will re-enumerate the displays.
		FD3D12Adapter* Adapter = GetParentAdapter();
		IDXGIFactory2* DxgiFactory2 = Adapter->GetDXGIFactory2();
		if (DxgiFactory2)
		{
			if (!DxgiFactory2->IsCurrent())
			{
				Adapter->CreateDXGIFactory();
			}

			check(Adapter->GetDXGIFactory2()->IsCurrent());

			// Get information about the display we are presenting to.
			TRefCountPtr<IDXGIOutput> Output;
			VERIFYD3D12RESULT(SwapChain4->GetContainingOutput(Output.GetInitReference()));

			TRefCountPtr<IDXGIOutput6> Output6;
			if (SUCCEEDED(Output->QueryInterface(IID_PPV_ARGS(Output6.GetInitReference()))))
			{
				DXGI_OUTPUT_DESC1 OutputDesc;
				VERIFYD3D12RESULT(Output6->GetDesc1(&OutputDesc));

				// Check for HDR support on the display.
				bOutputSupportsHDR = (OutputDesc.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020);
			}
		}
	}

	return bOutputSupportsHDR;
}

static const FString GetDXGIColorSpaceString(DXGI_COLOR_SPACE_TYPE ColorSpace)
{
	switch (ColorSpace)
	{
	case DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709:
		return TEXT("RGB_FULL_G22_NONE_P709");
	case DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709:
		return TEXT("RGB_FULL_G10_NONE_P709");
	case DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020:
		return TEXT("RGB_FULL_G2084_NONE_P2020");
	case DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P2020:
		return TEXT("RGB_FULL_G22_NONE_P2020");
	default:
		break;
	}

	return FString::FromInt(ColorSpace);
};

void FD3D12Viewport::EnsureColorSpace(EDisplayGamut DisplayGamut, EDisplayFormat OutputDevice)
{
	ensure(SwapChain4.GetReference());

	DXGI_COLOR_SPACE_TYPE NewColorSpace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;	// sRGB;
	const bool bPrimaries2020 = (DisplayGamut == DG_Rec2020);

	// See console variable r.HDR.Display.OutputDevice.
	switch (OutputDevice)
	{
		// Gamma 2.2
	case DF_sRGB:
	case DF_Rec709:
		NewColorSpace = bPrimaries2020 ? DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P2020 : DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
		break;

		// Gamma ST.2084
	case DF_ACES1000_ST_2084:
	case DF_ACES2000_ST_2084:
		NewColorSpace = DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
		break;

		// Gamma 1.0 (Linear)
	case DF_ACES1000_ScRGB:
	case DF_ACES2000_ScRGB:
		// Linear. Still supports expanded color space with values >1.0f and <0.0f.
		// The actual range is determined by the pixel format (e.g. a UNORM format can only ever have 0-1).
		NewColorSpace = DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709;
		break;
	}

	if (ColorSpace != NewColorSpace)
	{
		uint32 ColorSpaceSupport = 0;
		if (SUCCEEDED(SwapChain4->CheckColorSpaceSupport(NewColorSpace, &ColorSpaceSupport)) &&
			((ColorSpaceSupport & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT) != 0))
		{
			VERIFYD3D12RESULT(SwapChain4->SetColorSpace1(NewColorSpace));
			UE_LOG(LogD3D12RHI, Log, TEXT("Setting color space on swap chain (%#016llx): %s"), SwapChain4.GetReference(), *GetDXGIColorSpaceString(NewColorSpace));
			ColorSpace = NewColorSpace;
		}
	}
}

void FD3D12Viewport::SetHDRTVMode(bool bEnableHDR, EDisplayGamut DisplayGamut, float MaxOutputNits, float MinOutputNits, float MaxCLL, float MaxFALL)
{
	ensure(SwapChain4.GetReference());

	static const DisplayChromacities DisplayChromacityList[] =
	{
		{ 0.64000f, 0.33000f, 0.30000f, 0.60000f, 0.15000f, 0.06000f, 0.31270f, 0.32900f }, // DG_Rec709
		{ 0.68000f, 0.32000f, 0.26500f, 0.69000f, 0.15000f, 0.06000f, 0.31270f, 0.32900f }, // DG_DCI-P3 D65
		{ 0.70800f, 0.29200f, 0.17000f, 0.79700f, 0.13100f, 0.04600f, 0.31270f, 0.32900f }, // DG_Rec2020
		{ 0.73470f, 0.26530f, 0.00000f, 1.00000f, 0.00010f,-0.07700f, 0.32168f, 0.33767f }, // DG_ACES
		{ 0.71300f, 0.29300f, 0.16500f, 0.83000f, 0.12800f, 0.04400f, 0.32168f, 0.33767f }, // DG_ACEScg
	};

	if (bEnableHDR)
	{
		const DisplayChromacities& Chroma = DisplayChromacityList[DisplayGamut];

		// Set HDR meta data
		DXGI_HDR_METADATA_HDR10 HDR10MetaData = {};
		HDR10MetaData.RedPrimary[0] = static_cast<uint16>(Chroma.RedX * 50000.0f);
		HDR10MetaData.RedPrimary[1] = static_cast<uint16>(Chroma.RedY * 50000.0f);
		HDR10MetaData.GreenPrimary[0] = static_cast<uint16>(Chroma.GreenX * 50000.0f);
		HDR10MetaData.GreenPrimary[1] = static_cast<uint16>(Chroma.GreenY * 50000.0f);
		HDR10MetaData.BluePrimary[0] = static_cast<uint16>(Chroma.BlueX * 50000.0f);
		HDR10MetaData.BluePrimary[1] = static_cast<uint16>(Chroma.BlueY * 50000.0f);
		HDR10MetaData.WhitePoint[0] = static_cast<uint16>(Chroma.WpX * 50000.0f);
		HDR10MetaData.WhitePoint[1] = static_cast<uint16>(Chroma.WpY * 50000.0f);
		HDR10MetaData.MaxMasteringLuminance = static_cast<uint32>(MaxOutputNits * 10000.0f);
		HDR10MetaData.MinMasteringLuminance = static_cast<uint32>(MinOutputNits * 10000.0f);
		HDR10MetaData.MaxContentLightLevel = static_cast<uint16>(MaxCLL);
		HDR10MetaData.MaxFrameAverageLightLevel = static_cast<uint16>(MaxFALL);

		VERIFYD3D12RESULT(SwapChain4->SetHDRMetaData(DXGI_HDR_METADATA_TYPE_HDR10, sizeof(HDR10MetaData), &HDR10MetaData));
		UE_LOG(LogD3D12RHI, Log, TEXT("Setting HDR meta data on swap chain (%#016llx) using DisplayGamut %u:"), SwapChain4.GetReference(), static_cast<uint32>(DisplayGamut));
		UE_LOG(LogD3D12RHI, Log, TEXT("\t\tMaxMasteringLuminance = %.4f nits"), HDR10MetaData.MaxMasteringLuminance * .0001f);
		UE_LOG(LogD3D12RHI, Log, TEXT("\t\tMinMasteringLuminance = %.4f nits"), HDR10MetaData.MinMasteringLuminance * .0001f);
		UE_LOG(LogD3D12RHI, Log, TEXT("\t\tMaxContentLightLevel = %u nits"), HDR10MetaData.MaxContentLightLevel);
		UE_LOG(LogD3D12RHI, Log, TEXT("\t\tMaxFrameAverageLightLevel %u = nits"), HDR10MetaData.MaxFrameAverageLightLevel);
		bHDRMetaDataSet = true;
	}
	else
	{
		if (bHDRMetaDataSet)
		{
			// Clear meta data.
			VERIFYD3D12RESULT(SwapChain4->SetHDRMetaData(DXGI_HDR_METADATA_TYPE_NONE, 0, nullptr));
			UE_LOG(LogD3D12RHI, Log, TEXT("Clearing HDR meta data on swap chain (%#016llx)."), SwapChain4.GetReference());
			bHDRMetaDataSet = false;
		}
	}
}

#include "HideWindowsPlatformTypes.h"
