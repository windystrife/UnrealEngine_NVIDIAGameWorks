// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "FrameGrabber.h"
#include "Misc/ScopeLock.h"
#include "Modules/ModuleManager.h"
#include "RenderingThread.h"
#include "RendererInterface.h"
#include "StaticBoundShaderState.h"
#include "Layout/ArrangedChildren.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "RHIStaticStates.h"
#include "Shader.h"
#include "GlobalShader.h"
#include "ScreenRendering.h"
#include "PipelineStateCache.h"

FViewportSurfaceReader::FViewportSurfaceReader(EPixelFormat InPixelFormat, FIntPoint InBufferSize)
{
	AvailableEvent = nullptr;
	ReadbackTexture = nullptr;
	PixelFormat = InPixelFormat;

	Resize(InBufferSize.X, InBufferSize.Y);
}

FViewportSurfaceReader::~FViewportSurfaceReader()
{
	BlockUntilAvailable();

	ReadbackTexture.SafeRelease();
}

void FViewportSurfaceReader::Initialize()
{
	check(!AvailableEvent);
	AvailableEvent = FPlatformProcess::GetSynchEventFromPool();
}

void FViewportSurfaceReader::Resize(uint32 Width, uint32 Height)
{
	ReadbackTexture.SafeRelease();

	ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
		CreateCaptureFrameTexture,
		int32, InWidth, Width,
		int32, InHeight, Height,
		FViewportSurfaceReader*, This, this,
	{
		FRHIResourceCreateInfo CreateInfo;

		This->ReadbackTexture = RHICreateTexture2D(
			InWidth,
			InHeight,
			This->PixelFormat,
			1,
			1,
			TexCreate_CPUReadback,
			CreateInfo
			);
	});
}

void FViewportSurfaceReader::BlockUntilAvailable()
{
	if (AvailableEvent)
	{
		AvailableEvent->Wait(~0);

		FPlatformProcess::ReturnSynchEventToPool(AvailableEvent);
		AvailableEvent = nullptr;
	}
}

void FViewportSurfaceReader::ResolveRenderTarget(const FViewportRHIRef& ViewportRHI, TFunction<void(FColor*, int32, int32)> Callback)
{
	static const FName RendererModuleName( "Renderer" );
	// @todo: JIRA UE-41879 and UE-43829 - added defensive guards against memory trampling on this render command to try and ascertain why it occasionally crashes
	uint32 MemoryGuard1 = 0xaffec7ed;

	// Load the renderermodule on the main thread, as the module manager is not thread-safe, and copy the ptr into the render command, along with 'this' (which is protected by BlockUntilAvailable in ~FViewportSurfaceReader())
	IRendererModule* RendererModule = &FModuleManager::GetModuleChecked<IRendererModule>(RendererModuleName);

	uint32 MemoryGuard2 = 0xaffec7ed;
	IRendererModule* RendererModuleDebug = RendererModule;

	auto RenderCommand = [=](FRHICommandListImmediate& RHICmdList){

		// @todo: JIRA UE-41879 and UE-43829. If any of these ensures go off, something has overwritten the memory for this render command (buffer underflow/overflow?)
		bool bMemoryTrample = !ensureMsgf(RendererModule, TEXT("RendererModule has become null. This indicates a memory trample.")) ||
			!ensureMsgf(RendererModule == RendererModuleDebug, TEXT("RendererModule and RendererModuleDebug are not equal (0x%016x != 0x%016x). This indicates a memory trample."), (void*)RendererModule, (void*)RendererModuleDebug) ||
			!ensureMsgf(MemoryGuard1 == 0xaffec7ed, TEXT("Memory guard 1 is now 0x%08x, expected 0xaffec7ed."), MemoryGuard1) ||
			!ensureMsgf(MemoryGuard2 == 0xaffec7ed, TEXT("Memory guard 2 is now 0x%08x, expected 0xaffec7ed."), MemoryGuard2);

		if (bMemoryTrample)
		{
			// In the hope that 'this' is still ok, triggering the event will prevent a deadlock. If it's not ok, this may crash, but it was going to crash anyway
			AvailableEvent->Trigger();
			return;
		}

		const FIntPoint TargetSize(ReadbackTexture->GetSizeX(), ReadbackTexture->GetSizeY());

		FPooledRenderTargetDesc OutputDesc = FPooledRenderTargetDesc::Create2DDesc(
			TargetSize,
			ReadbackTexture->GetFormat(),
			FClearValueBinding::None,
			TexCreate_None,
			TexCreate_RenderTargetable,
			false);

		// @todo: JIRA UE-41879 and UE-43829. If any of these ensures go off, something has overwritten the memory for this render command (buffer underflow/overflow?)
		bMemoryTrample = !ensureMsgf(RendererModule, TEXT("RendererModule has become null. This indicates a memory trample.")) ||
			!ensureMsgf(RendererModule == RendererModuleDebug, TEXT("RendererModule and RendererModuleDebug are not equal (0x%16x != 0x%16x). This indicates a memory trample."), (void*)RendererModule, (void*)RendererModuleDebug) ||
			!ensureMsgf(MemoryGuard1 == 0xaffec7ed, TEXT("Memory guard 1 is now 0x%08x, expected 0xaffec7ed."), MemoryGuard1) ||
			!ensureMsgf(MemoryGuard2 == 0xaffec7ed, TEXT("Memory guard 2 is now 0x%08x, expected 0xaffec7ed."), MemoryGuard2);

		if (bMemoryTrample)
		{
			// In the hope that 'this' is still ok, triggering the event will prevent a deadlock. If it's not ok, this may crash, but it was going to crash anyway
			AvailableEvent->Trigger();
			return;
		}

		TRefCountPtr<IPooledRenderTarget> ResampleTexturePooledRenderTarget;
		RendererModule->RenderTargetPoolFindFreeElement(RHICmdList, OutputDesc, ResampleTexturePooledRenderTarget, TEXT("ResampleTexture"));
		check(ResampleTexturePooledRenderTarget);

		const FSceneRenderTargetItem& DestRenderTarget = ResampleTexturePooledRenderTarget->GetRenderTargetItem();

		SetRenderTarget(RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());
		RHICmdList.SetViewport(0, 0, 0.0f, TargetSize.X, TargetSize.Y, 1.0f);

		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false,CF_Always>::GetRHI();

		const ERHIFeatureLevel::Type FeatureLevel = GMaxRHIFeatureLevel;
		
		TShaderMap<FGlobalShaderType>* ShaderMap = GetGlobalShaderMap(FeatureLevel);
		TShaderMapRef<FScreenVS> VertexShader(ShaderMap);
		TShaderMapRef<FScreenPS> PixelShader(ShaderMap);

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = RendererModule->GetFilterVertexDeclaration().VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

		FTexture2DRHIRef SourceBackBuffer = RHICmdList.GetViewportBackBuffer(ViewportRHI);

		if (TargetSize.X != SourceBackBuffer->GetSizeX() || TargetSize.Y != SourceBackBuffer->GetSizeY())
		{
			PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Bilinear>::GetRHI(), SourceBackBuffer);
		}
		else
		{
			PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Point>::GetRHI(), SourceBackBuffer);
		}

		float U = float(CaptureRect.Min.X) / float(SourceBackBuffer->GetSizeX());
		float V = float(CaptureRect.Min.Y) / float(SourceBackBuffer->GetSizeY());
		float SizeU = float(CaptureRect.Max.X) / float(SourceBackBuffer->GetSizeX()) - U;
		float SizeV = float(CaptureRect.Max.Y) / float(SourceBackBuffer->GetSizeY()) - V;

		RendererModule->DrawRectangle(
			RHICmdList,
			0, 0,									// Dest X, Y
			TargetSize.X,							// Dest Width
			TargetSize.Y,							// Dest Height
			U, V,									// Source U, V
			1, 1,									// Source USize, VSize
			CaptureRect.Max - CaptureRect.Min,		// Target buffer size
			FIntPoint(1, 1),						// Source texture size
			*VertexShader,
			EDRF_Default);

		// Asynchronously copy render target from GPU to CPU
		const bool bKeepOriginalSurface = false;
		RHICmdList.CopyToResolveTarget(
			DestRenderTarget.TargetableTexture,
			ReadbackTexture,
			bKeepOriginalSurface,
			FResolveParams());

		void* ColorDataBuffer = nullptr;

		int32 Width = 0, Height = 0;
		RHICmdList.MapStagingSurface(ReadbackTexture, ColorDataBuffer, Width, Height);

		Callback((FColor*)ColorDataBuffer, Width, Height);

		RHICmdList.UnmapStagingSurface(ReadbackTexture);
		AvailableEvent->Trigger();
	};

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		ResolveCaptureFrameTexture,
		TFunction<void(FRHICommandListImmediate&)>, InRenderCommand, RenderCommand,
	{
		InRenderCommand(RHICmdList);
	});
}

FFrameGrabber::FFrameGrabber(TSharedRef<FSceneViewport> Viewport, FIntPoint DesiredBufferSize, EPixelFormat InPixelFormat, uint32 NumSurfaces)
{
	State = EFrameGrabberState::Inactive;

	// cause the viewport to always flush on draw
	Viewport->IncrementFlushOnDraw();

	{
		// Setup a functor to decrement the flag on destruction (this class isn't necessarily tied to scene viewports)
		TWeakPtr<FSceneViewport> WeakViewport = Viewport;
		OnShutdown = [WeakViewport]{
			TSharedPtr<FSceneViewport> PinnedViewport = WeakViewport.Pin();
			if (PinnedViewport.IsValid())
			{
				PinnedViewport->DecrementFlushOnDraw();
			}
		};
	}

	TargetSize = DesiredBufferSize;

	CurrentFrameIndex = 0;

	check(NumSurfaces != 0);

	FIntRect CaptureRect(0,0,Viewport->GetSize().X, Viewport->GetSize().Y);

	// Set up the capture rectangle
	TSharedPtr<SViewport> ViewportWidget = Viewport->GetViewportWidget().Pin();
	if (ViewportWidget.IsValid())
	{
		TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(ViewportWidget.ToSharedRef());
		if (Window.IsValid())
		{
			CaptureWindow = Window;
			FGeometry InnerWindowGeometry = Window->GetWindowGeometryInWindow();
			
			// Find the widget path relative to the window
			FArrangedChildren JustWindow(EVisibility::Visible);
			JustWindow.AddWidget(FArrangedWidget(Window.ToSharedRef(), InnerWindowGeometry));

			FWidgetPath WidgetPath(Window.ToSharedRef(), JustWindow);
			if (WidgetPath.ExtendPathTo(FWidgetMatcher(ViewportWidget.ToSharedRef()), EVisibility::Visible))
			{
				FArrangedWidget ArrangedWidget = WidgetPath.FindArrangedWidget(ViewportWidget.ToSharedRef()).Get(FArrangedWidget::NullWidget);

				FVector2D Position = ArrangedWidget.Geometry.AbsolutePosition;
				FVector2D Size = ArrangedWidget.Geometry.GetDrawSize();

				CaptureRect = FIntRect(
					Position.X,
					Position.Y,
					Position.X + Size.X,
					Position.Y + Size.Y);
			}
		}
	}

	// This can never be reallocated
	Surfaces.Reserve(NumSurfaces);
	for (uint32 Index = 0; Index < NumSurfaces; ++Index)
	{
		Surfaces.Emplace(InPixelFormat, DesiredBufferSize);
		Surfaces.Last().Surface.SetCaptureRect(CaptureRect);
	}

	// Ensure textures are setup
	FlushRenderingCommands();
}

FFrameGrabber::~FFrameGrabber()
{
	if (OnWindowRendered.IsValid())
	{
		FSlateApplication::Get().GetRenderer()->OnSlateWindowRendered().Remove(OnWindowRendered);
	}
	
	if (OnShutdown)
	{
		OnShutdown();
	}
}

void FFrameGrabber::StartCapturingFrames()
{
	if (!ensure(State == EFrameGrabberState::Inactive))
	{
		return;
	}

	State = EFrameGrabberState::Active;
	OnWindowRendered = FSlateApplication::Get().GetRenderer()->OnSlateWindowRendered().AddRaw(this, &FFrameGrabber::OnSlateWindowRendered);
}

void FFrameGrabber::CaptureThisFrame(FFramePayloadPtr Payload)
{
	if (!ensure(State == EFrameGrabberState::Active))
	{
		return;
	}

	OutstandingFrameCount.Increment();

	FScopeLock Lock(&PendingFramePayloadsMutex);
	PendingFramePayloads.Add(Payload);
}

void FFrameGrabber::StopCapturingFrames()
{
	if (!ensure(State == EFrameGrabberState::Active))
	{
		return;
	}

	State = EFrameGrabberState::PendingShutdown;
}

void FFrameGrabber::Shutdown()
{
	State = EFrameGrabberState::Inactive;

	for (FResolveSurface& Surface : Surfaces)
	{
		Surface.Surface.BlockUntilAvailable();
	}

	FSlateApplication::Get().GetRenderer()->OnSlateWindowRendered().Remove(OnWindowRendered);
	OnWindowRendered = FDelegateHandle();
}

bool FFrameGrabber::HasOutstandingFrames() const
{
	FScopeLock Lock(&CapturedFramesMutex);

	// Check whether we have any outstanding frames while we have the array locked, to prevent race condition
	return OutstandingFrameCount.GetValue() || CapturedFrames.Num();
}

TArray<FCapturedFrameData> FFrameGrabber::GetCapturedFrames()
{
	TArray<FCapturedFrameData> ReturnFrames;

	bool bShouldStop = false;
	{
		FScopeLock Lock(&CapturedFramesMutex);
		Swap(ReturnFrames, CapturedFrames);
		CapturedFrames.Reset();

		// Check whether we have any outstanding frames while we have the array locked, to prevent race condition
		if (State == EFrameGrabberState::PendingShutdown)
		{
			bShouldStop = OutstandingFrameCount.GetValue() == 0;
		}
	}

	if (bShouldStop)
	{
		Shutdown();
	}

	return ReturnFrames;
}

void FFrameGrabber::OnSlateWindowRendered( SWindow& SlateWindow, void* ViewportRHIPtr )
{
	// We only care about our own Slate window
	TSharedPtr<SWindow> Window = CaptureWindow.Pin();
	if (Window.Get() != &SlateWindow)
	{
		return;
	}

	FFramePayloadPtr Payload;

	{
		FScopeLock Lock(&PendingFramePayloadsMutex);
		if (!PendingFramePayloads.Num())
		{
			// No frames to capture
			return;
		}
		Payload = PendingFramePayloads[0];
		PendingFramePayloads.RemoveAt(0, 1, false);
	}

	const int32 ThisCaptureIndex = CurrentFrameIndex;

	FResolveSurface* ThisFrameTarget = &Surfaces[ThisCaptureIndex];
	ThisFrameTarget->Surface.BlockUntilAvailable();

	ThisFrameTarget->Surface.Initialize();
	ThisFrameTarget->Payload = Payload;

	const FViewportRHIRef* RHIViewport = (const FViewportRHIRef*)ViewportRHIPtr;

	Surfaces[ThisCaptureIndex].Surface.ResolveRenderTarget(*RHIViewport, [=](FColor* ColorBuffer, int32 Width, int32 Height){
		// Handle the frame
		OnFrameReady(ThisCaptureIndex, ColorBuffer, Width, Height);
	});

	CurrentFrameIndex = (CurrentFrameIndex+1) % Surfaces.Num();
}

void FFrameGrabber::OnFrameReady(int32 BufferIndex, FColor* ColorBuffer, int32 Width, int32 Height)
{
	if (!ensure(ColorBuffer))
	{
		OutstandingFrameCount.Decrement();
		return;
	}
	
	const FResolveSurface& Surface = Surfaces[BufferIndex];

	FCapturedFrameData ResolvedFrameData(TargetSize, Surface.Payload);

	ResolvedFrameData.ColorBuffer.InsertUninitialized(0, TargetSize.X * TargetSize.Y);
	FColor* Dest = &ResolvedFrameData.ColorBuffer[0];

	const int32 MaxWidth = FMath::Min(TargetSize.X, Width);
	for (int32 Row = 0; Row < FMath::Min(Height, TargetSize.Y); ++Row)
	{
		FMemory::Memcpy(Dest, ColorBuffer, sizeof(FColor)*MaxWidth);
		ColorBuffer += Width;
		Dest += MaxWidth;
	}

	{
		FScopeLock Lock(&CapturedFramesMutex);
		CapturedFrames.Add(MoveTemp(ResolvedFrameData));
	}
	OutstandingFrameCount.Decrement();
}
