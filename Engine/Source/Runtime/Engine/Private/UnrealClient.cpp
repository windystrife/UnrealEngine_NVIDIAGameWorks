// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "UnrealClient.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "EngineStats.h"
#include "EngineGlobals.h"
#include "RenderingThread.h"
#include "Templates/ScopedPointer.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Misc/ConfigCacheIni.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "UnrealEngine.h"
#include "Components/PostProcessComponent.h"
#include "Matinee/MatineeActor.h"
#include "EditorSupportDelegates.h"
#include "HighResScreenshot.h"
#include "GameFramework/GameUserSettings.h"

#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Engine/PostProcessVolume.h"
#include "RendererInterface.h"
#include "EngineModule.h"
#include "Performance/EnginePerformanceTargets.h"
#include "UniquePtr.h"

DEFINE_LOG_CATEGORY_STATIC(LogClient, Log, All);

IMPLEMENT_STRUCT(PostProcessSettings);

bool FViewport::bIsGameRenderingEnabled = true;
int32 FViewport::PresentAndStopMovieDelay = 0;

/**
* Reads the viewport's displayed pixels into a preallocated color buffer.
* @param OutImageData - RGBA8 values will be stored in this buffer
* @param TopLeftX - Top left X pixel to capture
* @param TopLeftY - Top left Y pixel to capture
* @param Width - Width of image in pixels to capture
* @param Height - Height of image in pixels to capture
* @return True if the read succeeded.
*/
bool FRenderTarget::ReadPixels(TArray< FColor >& OutImageData, FReadSurfaceDataFlags InFlags, FIntRect InRect)
{
	if(InRect == FIntRect(0, 0, 0, 0))
	{
		InRect = FIntRect(0, 0, GetSizeXY().X, GetSizeXY().Y);
	}

	// Read the render target surface data back.	
	struct FReadSurfaceContext
	{
		FRenderTarget* SrcRenderTarget;
		TArray<FColor>* OutData;
		FIntRect Rect;
		FReadSurfaceDataFlags Flags;
	};

	OutImageData.Reset();
	FReadSurfaceContext ReadSurfaceContext =
	{
		this,
		&OutImageData,
		InRect,
		InFlags
	};

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		ReadSurfaceCommand,
		FReadSurfaceContext,Context,ReadSurfaceContext,
	{
		RHICmdList.ReadSurfaceData(
			Context.SrcRenderTarget->GetRenderTargetTexture(),
			Context.Rect,
			*Context.OutData,
			Context.Flags
			);
	});
	FlushRenderingCommands();

	return OutImageData.Num() > 0;
}


/**
* Reads the viewport's displayed pixels into a preallocated color buffer.
* @param OutputBuffer - RGBA8 values will be stored in this buffer
* @return True if the read succeeded.
*/
bool FRenderTarget::ReadPixelsPtr(FColor* OutImageBytes, FReadSurfaceDataFlags InFlags, FIntRect InRect)
{
	TArray<FColor> SurfaceData;

	bool bResult = ReadPixels( SurfaceData, InFlags, InRect );
	if( bResult )
	{
		FMemory::Memcpy( OutImageBytes, &SurfaceData[ 0 ], SurfaceData.Num() * sizeof(FColor) );
	}

	return bResult;
}

/**
 * Reads the viewport's displayed pixels into a preallocated color buffer.
 * @param OutImageBytes - RGBA16F values will be stored in this buffer.  Buffer must be preallocated with the correct size!
 * @param CubeFace - optional cube face for when reading from a cube render target
 * @return True if the read succeeded.
 */
bool FRenderTarget::ReadFloat16Pixels(FFloat16Color* OutImageData,ECubeFace CubeFace)
{
	// Read the render target surface data back.	
	struct FReadSurfaceFloatContext
	{
		FRenderTarget* SrcRenderTarget;
		TArray<FFloat16Color>* OutData;
		FIntRect Rect;
		ECubeFace CubeFace;
	};
	
	TArray<FFloat16Color> SurfaceData;
	FReadSurfaceFloatContext ReadSurfaceFloatContext =
	{
		this,
		&SurfaceData,
		FIntRect(0, 0, GetSizeXY().X, GetSizeXY().Y),
		CubeFace	
	};

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		ReadSurfaceFloatCommand,
		FReadSurfaceFloatContext,Context,ReadSurfaceFloatContext,
	{
		RHICmdList.ReadSurfaceFloatData(
			Context.SrcRenderTarget->GetRenderTargetTexture(),
			Context.Rect,
			*Context.OutData,
			Context.CubeFace,
			0,
			0
			);
	});
	FlushRenderingCommands();

	// Copy the surface data into the output array.
	FFloat16Color* OutImageColors = reinterpret_cast< FFloat16Color* >(OutImageData);

	// Cache width and height as its very expensive to call these virtuals in inner loop (never inlined)
	const int32 ImageWidth = GetSizeXY().X;
	const int32 ImageHeight = GetSizeXY().Y;
	for (int32 Y = 0; Y < ImageHeight; Y++)
	{
		FFloat16Color* SourceData = (FFloat16Color*)SurfaceData.GetData() + Y * ImageWidth;
		for (int32 X = 0; X < ImageWidth; X++)
		{
			OutImageColors[ Y * ImageWidth + X ] = SourceData[X];
		}
	}

	return true;
}

/**
 * Reads the viewport's displayed pixels into the given color buffer.
 * @param OutputBuffer - RGBA16F values will be stored in this buffer
 * @param CubeFace - optional cube face for when reading from a cube render target
 * @return True if the read succeeded.
 */
bool FRenderTarget::ReadFloat16Pixels(TArray<FFloat16Color>& OutputBuffer,ECubeFace CubeFace)
{
	// Copy the surface data into the output array.
	OutputBuffer.Empty();
	OutputBuffer.AddUninitialized(GetSizeXY().X * GetSizeXY().Y);
	return ReadFloat16Pixels((FFloat16Color*)&(OutputBuffer[0]), CubeFace);
}

/**
* Reads the viewport's displayed pixels into a preallocated color buffer.
* @param OutImageData - LinearColor array to fill!
* @param CubeFace - optional cube face for when reading from a cube render target
* @return True if the read succeeded.
*/
bool FRenderTarget::ReadLinearColorPixels(TArray<FLinearColor> &OutImageData, FReadSurfaceDataFlags InFlags, FIntRect InRect)
{
	if (InRect == FIntRect(0, 0, 0, 0))
	{
		InRect = FIntRect(0, 0, GetSizeXY().X, GetSizeXY().Y);
	}

	// Read the render target surface data back.	
	struct FReadSurfaceContext
	{
		FRenderTarget* SrcRenderTarget;
		TArray<FLinearColor>* OutData;
		FIntRect Rect;
		FReadSurfaceDataFlags Flags;
	};

	OutImageData.Reset();
	FReadSurfaceContext ReadSurfaceContext =
	{
		this,
		&OutImageData,
		InRect,
		InFlags
	};

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		ReadSurfaceCommand,
		FReadSurfaceContext, Context, ReadSurfaceContext,
		{
			RHICmdList.ReadSurfaceData(
			Context.SrcRenderTarget->GetRenderTargetTexture(),
				Context.Rect,
				*Context.OutData,
				Context.Flags
				);
		});
	FlushRenderingCommands();

	return true;
}

/**
* Reads the viewport's displayed pixels into a preallocated color buffer.
* @param OutputBuffer - RGBA8 values will be stored in this buffer
* @return True if the read succeeded.
*/
bool FRenderTarget::ReadLinearColorPixelsPtr(FLinearColor* OutImageBytes, FReadSurfaceDataFlags InFlags, FIntRect InRect)
{
	TArray<FLinearColor> SurfaceData;

	bool bResult = ReadLinearColorPixels(SurfaceData, InFlags, InRect);
	if (bResult)
	{
		FMemory::Memcpy(OutImageBytes, &SurfaceData[0], SurfaceData.Num() * sizeof(FLinearColor));
	}

	return bResult;
}

/** 
* @return display gamma expected for rendering to this render target 
*/
float FRenderTarget::GetDisplayGamma() const
{
	if (GEngine == NULL)
	{
		return 2.2f;
	}
	else
	{
		if (FMath::Abs(GEngine->DisplayGamma) <= 0.0f)
		{
			UE_LOG(LogClient, Error, TEXT("Invalid DisplayGamma! Resetting to the default of 2.2"));
			GEngine->DisplayGamma = 2.2f;
		}
		return GEngine->DisplayGamma;
	}
}

/**
* Accessor for the surface RHI when setting this render target
* @return render target surface RHI resource
*/
const FTexture2DRHIRef& FRenderTarget::GetRenderTargetTexture() const
{
	return RenderTargetTextureRHI;
}

FUnorderedAccessViewRHIRef FRenderTarget::GetRenderTargetUAV() const
{
	return FUnorderedAccessViewRHIRef();
}

void FScreenshotRequest::RequestScreenshot(bool bInShowUI)
{
	// empty string means we'll later pick the name
	RequestScreenshot(TEXT(""), bInShowUI, true);
}

void FScreenshotRequest::RequestScreenshot(const FString& InFilename, bool bInShowUI, bool bAddUniqueSuffix)
{
	FString GeneratedFilename = InFilename;
	CreateViewportScreenShotFilename(GeneratedFilename);

	if (bAddUniqueSuffix)
	{
		const bool bRemovePath = false;
		GeneratedFilename = FPaths::GetBaseFilename(GeneratedFilename, bRemovePath);
		FFileHelper::GenerateNextBitmapFilename(GeneratedFilename, TEXT("png"), Filename);
	}
	else
	{
		Filename = GeneratedFilename;
	}

	// Register the screenshot
	if (!Filename.IsEmpty())
	{
		bShowUI = bInShowUI;
		bIsScreenshotRequested = true;
	}

	GScreenMessagesRestoreState = GAreScreenMessagesEnabled;

	// Disable Screen Messages when the screenshot is requested without UI.
	if (bInShowUI == false)
	{
		GAreScreenMessagesEnabled = false;
	}
}


void FScreenshotRequest::Reset()
{
	bIsScreenshotRequested = false;
	Filename.Empty();
	bShowUI = false;
}

void FScreenshotRequest::CreateViewportScreenShotFilename(FString& InOutFilename)
{
	FString TypeName;

	if(GIsDumpingMovie)
	{
		TypeName = TEXT("MovieFrame");

		if(GIsDumpingMovie > 0)
		{
			// <=0:off (default), <0:remains on, >0:remains on for n frames (n is the number specified)
			--GIsDumpingMovie;
		}
	}
	else if(GIsHighResScreenshot)
	{
		FString FilenameOverride = GetHighResScreenshotConfig().FilenameOverride;
		TypeName = FilenameOverride.IsEmpty() ? TEXT("HighresScreenshot") : FilenameOverride;
	}
	else
	{
		TypeName = InOutFilename.IsEmpty() ? TEXT("ScreenShot") : InOutFilename;
	}
	check(!TypeName.IsEmpty());

	//default to using the path that is given
	InOutFilename = TypeName;
	if (!TypeName.Contains(TEXT("/")))
	{
		InOutFilename = GetDefault<UEngine>()->GameScreenshotSaveDirectory.Path / TypeName;
	}
}

TArray<FColor>* FScreenshotRequest::GetHighresScreenshotMaskColorArray()
{
	return &HighresScreenshotMaskColorArray;
}


bool FScreenshotRequest::bIsScreenshotRequested = false;
FString FScreenshotRequest::Filename;
FString FScreenshotRequest::NextScreenshotName;
bool FScreenshotRequest::bShowUI = false;
TArray<FColor> FScreenshotRequest::HighresScreenshotMaskColorArray;


int32 FStatUnitData::DrawStat(FViewport* InViewport, FCanvas* InCanvas, int32 InX, int32 InY)
{
	float DiffTime;
	if (FApp::IsBenchmarking() || FApp::UseFixedTimeStep())
	{
		/** If we're in fixed time step mode, FApp::GetCurrentTime() will be incorrect for benchmarking */
		const double CurrentTime = FPlatformTime::Seconds();
		if (LastTime == 0)
		{
			LastTime = CurrentTime;
		}
		DiffTime = CurrentTime - LastTime;
		LastTime = CurrentTime;
	}
	else
	{
		/** Use the DiffTime we computed last frame, because it correctly handles the end of frame idling and corresponds better to the other unit times. */
		DiffTime = FApp::GetCurrentTime() - FApp::GetLastTime();
	}

	RawFrameTime = DiffTime * 1000.0f;
	FrameTime = 0.9 * FrameTime + 0.1 * RawFrameTime;

	/** Number of milliseconds the gamethread was used last frame. */
	RawGameThreadTime = FPlatformTime::ToMilliseconds(GGameThreadTime);
	GameThreadTime = 0.9 * GameThreadTime + 0.1 * RawGameThreadTime;

	/** Number of milliseconds the renderthread was used last frame. */
	RawRenderThreadTime = FPlatformTime::ToMilliseconds(GRenderThreadTime);
	RenderThreadTime = 0.9 * RenderThreadTime + 0.1 * RawRenderThreadTime;

	/** Number of milliseconds the GPU was busy last frame. */
	const uint32 GPUCycles = RHIGetGPUFrameCycles();
	RawGPUFrameTime = FPlatformTime::ToMilliseconds(GPUCycles);
	GPUFrameTime = 0.9 * GPUFrameTime + 0.1 * RawGPUFrameTime;

	// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
	GDynamicRHI->RHIVXGIGetGPUTime(RawVxgiWorldSpaceTime, RawVxgiScreenSpaceTime);
	VxgiWorldSpaceTime = 0.9 * VxgiWorldSpaceTime + 0.1 * RawVxgiWorldSpaceTime;
	VxgiScreenSpaceTime = 0.9 * VxgiScreenSpaceTime + 0.1 * RawVxgiScreenSpaceTime;
#endif
	// NVCHANGE_END: Add VXGI

	SET_FLOAT_STAT(STAT_UnitFrame, FrameTime);
	SET_FLOAT_STAT(STAT_UnitRender, RenderThreadTime);
	SET_FLOAT_STAT(STAT_UnitGame, GameThreadTime);
	SET_FLOAT_STAT(STAT_UnitGPU, GPUFrameTime);

	GEngine->SetAverageUnitTimes(FrameTime, RenderThreadTime, GameThreadTime, GPUFrameTime);

	float Max_RenderThreadTime = 0.0f;
	float Max_GameThreadTime = 0.0f;
	float Max_GPUFrameTime = 0.0f;
	float Max_FrameTime = 0.0f;

	const bool bShowUnitMaxTimes = InViewport->GetClient() ? InViewport->GetClient()->IsStatEnabled(TEXT("UnitMax")) : false;
#if !UE_BUILD_SHIPPING
	const bool bShowRawUnitTimes = InViewport->GetClient() ? InViewport->GetClient()->IsStatEnabled(TEXT("Raw")) : false;
	RenderThreadTimes[CurrentIndex] = bShowRawUnitTimes ? RawRenderThreadTime : RenderThreadTime;
	GameThreadTimes[CurrentIndex] = bShowRawUnitTimes ? RawGameThreadTime : GameThreadTime;
	GPUFrameTimes[CurrentIndex] = bShowRawUnitTimes ? RawGPUFrameTime : GPUFrameTime;
	FrameTimes[CurrentIndex] = bShowRawUnitTimes ? RawFrameTime : FrameTime;
	CurrentIndex++;
	if (CurrentIndex == NumberOfSamples)
	{
		CurrentIndex = 0;
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (bShowUnitMaxTimes)
	{
		for (int32 MaxIndex = 0; MaxIndex < NumberOfSamples; MaxIndex++)
		{
			if (Max_RenderThreadTime < RenderThreadTimes[MaxIndex])
			{
				Max_RenderThreadTime = RenderThreadTimes[MaxIndex];
			}
			if (Max_GameThreadTime < GameThreadTimes[MaxIndex])
			{
				Max_GameThreadTime = GameThreadTimes[MaxIndex];
			}
			if (Max_GPUFrameTime < GPUFrameTimes[MaxIndex])
			{
				Max_GPUFrameTime = GPUFrameTimes[MaxIndex];
			}
			if (Max_FrameTime < FrameTimes[MaxIndex])
			{
				Max_FrameTime = FrameTimes[MaxIndex];
			}
		}
	}
#endif // #if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
#endif // #if !UE_BUILD_SHIPPING

	static const auto DebugTextCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DebugTextScale"));
	const float TextScale = DebugTextCVar->GetFloat();

	// Render CPU thread and GPU frame times.
	const bool bStereoRendering = GEngine->IsStereoscopic3D(InViewport);
	UFont* Font = (!FPlatformProperties::SupportsWindowedMode() && GEngine->GetMediumFont()) ? GEngine->GetMediumFont() : GEngine->GetSmallFont();

	// Note InX should already be within the safe zone
	int32 X3 = InX * (bStereoRendering ? 0.5f : 1.0f);
	if (bShowUnitMaxTimes)
	{
		X3 -= (int32)((float)Font->GetStringSize(TEXT(" 000.00 ms ")) * TextScale);
	}

	int32 X2 = bShowUnitMaxTimes ? X3 - (int32)((float)Font->GetStringSize(TEXT(" 000.00 ms ")) * TextScale) : X3;
	int32 X1 = X2 - (int32)((float)Font->GetStringSize(TEXT("Frame: ")) * TextScale);
	const int32 RowHeight = FMath::TruncToInt(Font->GetMaxCharHeight() * 1.1f  * TextScale);
	const bool bShowUnitTimeGraph = InViewport->GetClient() ? InViewport->GetClient()->IsStatEnabled(TEXT("UnitGraph")) : false;

	{
		const FColor FrameTimeAverageColor = GEngine->GetFrameTimeDisplayColor(FrameTime);
		InCanvas->DrawShadowedString(X1, InY, TEXT("Frame:"), Font, bShowUnitTimeGraph ? FColor(100, 255, 100) : FColor::White, TextScale);
		InCanvas->DrawShadowedString(X2, InY, *FString::Printf(TEXT("%3.2f ms"), FrameTime), Font, FrameTimeAverageColor, TextScale);
		if (bShowUnitMaxTimes)
		{
			const FColor MaxFrameTimeColor = GEngine->GetFrameTimeDisplayColor(Max_FrameTime);
			InCanvas->DrawShadowedString(X3, InY, *FString::Printf(TEXT("%4.2f ms"), Max_FrameTime), Font, MaxFrameTimeColor, TextScale);
		}
		InY += RowHeight;
	}

	{
		const FColor GameThreadAverageColor = GEngine->GetFrameTimeDisplayColor(GameThreadTime);
		InCanvas->DrawShadowedString(X1, InY, TEXT("Game:"), Font, bShowUnitTimeGraph ? FColor(255, 100, 100) : FColor::White, TextScale);
		InCanvas->DrawShadowedString(X2, InY, *FString::Printf(TEXT("%3.2f ms"), GameThreadTime), Font, GameThreadAverageColor, TextScale);
		if (bShowUnitMaxTimes)
		{
			const FColor GameThreadMaxColor = GEngine->GetFrameTimeDisplayColor(Max_GameThreadTime);
			InCanvas->DrawShadowedString(X3, InY, *FString::Printf(TEXT("%4.2f ms"), Max_GameThreadTime), Font, GameThreadMaxColor, TextScale);
		}
		InY += RowHeight;
	}

	{
		const FColor RenderThreadAverageColor = GEngine->GetFrameTimeDisplayColor(RenderThreadTime);
		InCanvas->DrawShadowedString(X1, InY, TEXT("Draw:"), Font, bShowUnitTimeGraph ? FColor(100, 100, 255) : FColor::White, TextScale);
		InCanvas->DrawShadowedString(X2, InY, *FString::Printf(TEXT("%3.2f ms"), RenderThreadTime), Font, RenderThreadAverageColor, TextScale);
		if (bShowUnitMaxTimes)
		{
			const FColor RenderThreadMaxColor = GEngine->GetFrameTimeDisplayColor(Max_RenderThreadTime);
			InCanvas->DrawShadowedString(X3, InY, *FString::Printf(TEXT("%4.2f ms"), Max_RenderThreadTime), Font, RenderThreadMaxColor, TextScale);
		}
		InY += RowHeight;
	}

	const bool bHaveGPUData = GPUCycles > 0;
	if (bHaveGPUData)
	{
		const FColor GPUAverageColor = GEngine->GetFrameTimeDisplayColor(GPUFrameTime);
		InCanvas->DrawShadowedString(X1, InY, TEXT("GPU:"), Font, bShowUnitTimeGraph ? FColor(255, 255, 100) : FColor::White, TextScale);
		InCanvas->DrawShadowedString(X2, InY, *FString::Printf(TEXT("%3.2f ms"), GPUFrameTime), Font, GPUAverageColor, TextScale);
		if (bShowUnitMaxTimes)
		{
			const FColor GPUMaxColor = GEngine->GetFrameTimeDisplayColor(Max_GPUFrameTime);
			InCanvas->DrawShadowedString(X3, InY, *FString::Printf(TEXT("%4.2f ms"), Max_GPUFrameTime), Font, GPUMaxColor, TextScale);
		}
		if(GMaxRHIShaderPlatform == SP_PS4)
		{
			FString Warnings;

			{
				static const auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.PS4ContinuousSubmits"));
				int32 Value = CVar->GetInt();

				if (!Value)
				{
					// good for profiling (avoids bubles) but bad for high fps
					Warnings += TEXT(" r.PS4ContinuousSubmits");
				}
			}
			{
				static const auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.PS4StallsOnMarkers"));
				int32 Value = CVar->GetInt();

				if (Value)
				{
					// good to get Razor aligned GPU profiling but bad for high fps
					Warnings += TEXT(" r.PS4StallsOnMarkers");
				}
			}

			if(!Warnings.IsEmpty())
			{
				InCanvas->DrawShadowedString(X3 + 100, InY, *Warnings, Font, FColor::Red, TextScale);
			}
		}
		InY += RowHeight;
	}

	// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
	const bool bHaveVxgiData = (VxgiWorldSpaceTime > 0) || (VxgiScreenSpaceTime > 0);
	if (bHaveVxgiData)
	{
		const int32 X1V = X2 - Font->GetStringSize(TEXT("VXGI WS: "));
		FColor Color = VxgiWorldSpaceTime < 10.0f ? FColor::Green : (VxgiWorldSpaceTime < 25.0f ? FColor::Yellow : FColor::Red);
		InCanvas->DrawShadowedString(X1V, InY, TEXT("VXGI WS:"), Font, FColor::White);
		InCanvas->DrawShadowedString(X2, InY, *FString::Printf(TEXT("%3.2f ms"), VxgiWorldSpaceTime), Font, Color);
		InY += RowHeight;
		Color = VxgiScreenSpaceTime < 10.0f ? FColor::Green : (VxgiScreenSpaceTime < 25.0f ? FColor::Yellow : FColor::Red);
		InCanvas->DrawShadowedString(X1V, InY, TEXT("VXGI SS:"), Font, FColor::White);
		InCanvas->DrawShadowedString(X2, InY, *FString::Printf(TEXT("%3.2f ms"), VxgiScreenSpaceTime), Font, Color);
		InY += RowHeight;
	}
#endif
	// NVCHANGE_END: Add VXGI

#if !UE_BUILD_SHIPPING
	// Draw simple unit time graph
	if (bShowUnitTimeGraph)
	{
		UFont* SmallFont = GEngine->GetSmallFont();
		check(SmallFont);
		int32 AlertPrintWidth = SmallFont->GetStringSize(TEXT("000.0")) * TextScale;
		int32 AlertPrintHeight = SmallFont->GetStringHeightSize(TEXT("000.0")) * TextScale;

		// The vertical axis is time in milliseconds
		// The horizontal axis is the frame number (NOT time!!!)

		// Threshold where graph lines will pulsate for slow frames
		extern TAutoConsoleVariable<float> GTargetFrameTimeThresholdCVar;
		const float TargetTimeMS = GTargetFrameTimeThresholdCVar.GetValueOnGameThread();

		const float AlertTimeMS = TargetTimeMS;

		// Graph layout
		const float GraphLeftXPos = 80.0f;
		const float GraphBottomYPos = InViewport->GetSizeXY().Y - 50.0f;
		const float GraphHorizPixelsPerFrame = 2.0f * TextScale;
		const float GraphVerticalPixelsPerMS = 10.0f * TextScale;
		const float GraphHeightInMS = 40.0f;

		const FLinearColor GraphBorderColor(0.1f, 0.1f, 0.1f);
		const FLinearColor AlertLineColor(0.1f, 0.03f, 0.03f);

		// Compute pulse effect for lines above alert threshold
		const float AlertPulseFreq = 8.0f;
		const float AlertPulse = 0.5f + 0.5f * FMath::Sin((0.25f * PI * 2.0) + (FApp::GetCurrentTime() * PI * 2.0) * AlertPulseFreq);


		// For each type of statistic that we want to graph (0=Render, 1=Game, 2=GPU, 3=Frame)
		enum EGraphStats
		{
			EGS_Render = 0,
			EGS_Game,
			EGS_GPU,
			EGS_Frame,

			EGS_Count
		};


		FBatchedElements* BatchedElements = InCanvas->GetBatchedElements(FCanvas::ET_Line);
		FHitProxyId HitProxyId = InCanvas->GetHitProxyId();

		// Reserve line vertices (4 border lines, then up to the maximum number of graph lines)
		BatchedElements->AddReserveLines(4 + EGS_Count * NumberOfSamples);


		// Left
		BatchedElements->AddLine(
			FVector(GraphLeftXPos - 1.0f, GraphBottomYPos - GraphVerticalPixelsPerMS * GraphHeightInMS, 0.0f),
			FVector(GraphLeftXPos - 1.0f, GraphBottomYPos - 1.0f, 0.0f),
			GraphBorderColor,
			HitProxyId);

		// Right
		BatchedElements->AddLine(
			FVector(GraphLeftXPos + GraphHorizPixelsPerFrame * NumberOfSamples + 1.0f, GraphBottomYPos - GraphVerticalPixelsPerMS * GraphHeightInMS, 0.0f),
			FVector(GraphLeftXPos + GraphHorizPixelsPerFrame * NumberOfSamples + 1.0f, GraphBottomYPos - 1.0f, 0.0f),
			GraphBorderColor,
			HitProxyId);

		// Bottom
		BatchedElements->AddLine(
			FVector(GraphLeftXPos - 1.0f, GraphBottomYPos - 1.0f, 0.0f),
			FVector(GraphLeftXPos + GraphHorizPixelsPerFrame * NumberOfSamples + 1.0f, GraphBottomYPos - 1.0f, 0.0f),
			GraphBorderColor,
			HitProxyId);

		// Alert line
		BatchedElements->AddLine(
			FVector(GraphLeftXPos - 8.0f, GraphBottomYPos - AlertTimeMS * GraphVerticalPixelsPerMS, 0.0f),
			FVector(GraphLeftXPos + GraphHorizPixelsPerFrame * NumberOfSamples + 8.0f, GraphBottomYPos - AlertTimeMS * GraphVerticalPixelsPerMS, 0.0f),
			AlertLineColor,
			HitProxyId);

		int32 PrintY = GraphBottomYPos - AlertTimeMS * GraphVerticalPixelsPerMS - 2 * AlertPrintHeight;

		const bool bShowFrameTimeInUnitGraph = InViewport->GetClient() ? InViewport->GetClient()->IsStatEnabled(TEXT("UnitTime")) : false;
		for (int32 StatIndex = 0; StatIndex < EGS_Count; ++StatIndex)
		{
			int32 LastPrintX = 0xFFFFFFFF;
			PrintY -= AlertPrintHeight;

			// If we don't have GPU data to display, then skip this line
			if ((StatIndex == EGS_GPU && !bHaveGPUData)
				|| (StatIndex == EGS_Frame && bShowFrameTimeInUnitGraph == false && bHaveGPUData))
			{
				continue;
			}

			FLinearColor StatColor;
			float* TimeValues = NULL;
			switch (StatIndex)
			{
			case EGS_Render:
				TimeValues = RenderThreadTimes.GetData();
				StatColor = FLinearColor(0.1f, 0.1f, 1.0f);		// Blue
				break;

			case EGS_Game:
				TimeValues = GameThreadTimes.GetData();
				StatColor = FLinearColor(1.0f, 0.1f, 0.1f);		// Red
				break;

			case EGS_GPU:
				TimeValues = GPUFrameTimes.GetData();
				StatColor = FLinearColor(1.0f, 1.0f, 0.1f);		// Yellow
				break;

			case EGS_Frame:
				TimeValues = FrameTimes.GetData();
				StatColor = FLinearColor(0.1f, 1.0f, 0.1f);		// Green
				break;
			}

			// For each sample in our data set
			for (int32 CurFrameIndex = 0; CurFrameIndex < NumberOfSamples; ++CurFrameIndex)
			{
				const int32 PrevFrameIndex = FMath::Max(0, CurFrameIndex - 1);

				int32 PrevUnitIndex = (CurrentIndex - NumberOfSamples) + PrevFrameIndex;
				if (PrevUnitIndex < 0)
				{
					PrevUnitIndex += NumberOfSamples;
				}
				const FVector LineStart(
					GraphLeftXPos + (float)PrevFrameIndex * GraphHorizPixelsPerFrame,
					GraphBottomYPos - TimeValues[PrevUnitIndex] * GraphVerticalPixelsPerMS,
					0.0f);

				int32 CurUnitIndex = (CurrentIndex - NumberOfSamples) + CurFrameIndex;
				if (CurUnitIndex < 0)
				{
					CurUnitIndex += NumberOfSamples;
				}
				const FVector LineEnd(
					GraphLeftXPos + (float)CurFrameIndex * GraphHorizPixelsPerFrame,
					GraphBottomYPos - TimeValues[CurUnitIndex] * GraphVerticalPixelsPerMS,
					0.0f);

				FLinearColor FinalLineColor = StatColor;
				if (false && TimeValues[CurUnitIndex] > AlertTimeMS)
				{
					// Alert!
					FinalLineColor.R *= AlertPulse;
					FinalLineColor.G *= AlertPulse;
					FinalLineColor.B *= AlertPulse;
				}

				BatchedElements->AddLine(LineStart, LineEnd, FinalLineColor, HitProxyId);

				if (TimeValues[CurUnitIndex] > AlertTimeMS && (CurFrameIndex == 0 || TimeValues[PrevUnitIndex] <= AlertTimeMS))
				{
					const int32 AlertPadding = 1;
					float MaxValue = TimeValues[CurUnitIndex];
					int32 MinCheckFrames = FMath::Min<int32>(FPlatformMath::CeilToInt((float)AlertPrintWidth / GraphHorizPixelsPerFrame) + 10, NumberOfSamples);
					int32 CheckIndex = CurUnitIndex + 1;
					for (; CheckIndex < MinCheckFrames; ++CheckIndex)
					{
						MaxValue = FMath::Max<float>(MaxValue, TimeValues[CheckIndex]);
					}
					for (; CheckIndex < NumberOfSamples; ++CheckIndex)
					{
						if (TimeValues[CheckIndex] <= AlertTimeMS)
						{
							break;
						}
						MaxValue = FMath::Max<float>(MaxValue, TimeValues[CheckIndex]);
					}

					int32 StartX = GraphLeftXPos + (float)PrevFrameIndex * GraphHorizPixelsPerFrame - AlertPrintWidth;
					if (StartX > LastPrintX)
					{

						InCanvas->DrawShadowedString(StartX, PrintY, *FString::Printf(TEXT("%3.1f"), TimeValues[CurUnitIndex]), SmallFont, StatColor, TextScale);
						LastPrintX = StartX + AlertPrintWidth + AlertPadding;
					}
				}
			}
		}
	}
#endif	// !UE_BUILD_SHIPPING

	return InY;
}

int32 FStatHitchesData::DrawStat(FViewport* InViewport, FCanvas* InCanvas, int32 InX, int32 InY)
{
	const double CurrentTime = FPlatformTime::Seconds();
	if (LastTime > 0)
	{
		const float HitchThresholdSecs = FEnginePerformanceTargets::GetHitchFrameTimeThresholdMS() * 0.001f;

		const float DeltaSeconds = CurrentTime - LastTime;
		if (DeltaSeconds > HitchThresholdSecs)
		{
			Hitches[OverwriteIndex] = DeltaSeconds;
			When[OverwriteIndex] = CurrentTime;
			OverwriteIndex = (OverwriteIndex + 1) % NumHitches;
			if (GEngine->ActiveMatinee.IsValid())
			{
				float MatineeTime = GEngine->ActiveMatinee.Get()->InterpPosition;
				float MatineeMM = FPlatformMath::TruncToFloat(MatineeTime / 60.0f);
				float MatineeSS = FPlatformMath::TruncToFloat(MatineeTime - MatineeMM * 60.0f);
				float MatineeMS = FPlatformMath::TruncToFloat((MatineeTime - MatineeMM * 60.0f - MatineeSS) * 1000.0f);
				UE_LOG(LogClient, Warning, TEXT("HITCH @ %02dm:%02d.%03ds,%d,%d,%d"),
					(int32)MatineeMM, (int32)MatineeSS, (int32)MatineeMS, int32(MatineeTime * 1000), int32(DeltaSeconds * 1000), Count++);
			}
			else
			{
				UE_LOG(LogClient, Warning, TEXT("HITCH %d              running cnt = %5d"), int32(DeltaSeconds * 1000), Count++);
			}
		}

		const int32 MaxY = InViewport->GetSizeXY().Y;
		static const double TravelTime = 4.2;
		for (int32 i = 0; i < NumHitches; i++)
		{
			if (When[i] > 0 && When[i] <= CurrentTime && When[i] >= CurrentTime - TravelTime)
			{
				const float MyHitchSecs = Hitches[i];
				const float MyHitchMS = MyHitchSecs * 1000.0f;

				// Scale the time before passing in so that hitches aren't all red
				const FColor MyColor = GEngine->GetFrameTimeDisplayColor(MyHitchMS * 0.25f);

				const int32 MyY = InY + int32(float(MaxY - InY) * float((CurrentTime - When[i]) / TravelTime));
				const FString Hitch = FString::Printf(TEXT("%5d"), int32(MyHitchMS));
				InCanvas->DrawShadowedString(InX, MyY, *Hitch, GEngine->GetSmallFont(), MyColor);
			}
		}
	}
	LastTime = CurrentTime;
	return InY;
}


/*=============================================================================
//
// FViewport implementation.
//
=============================================================================*/

/** Send when a viewport is resized */
FViewport::FOnViewportResized FViewport::ViewportResizedEvent;

FViewport::FViewport(FViewportClient* InViewportClient):
	ViewportClient(InViewportClient),
	SizeX(0),
	SizeY(0),
	WindowMode(IsRunningGame() ? GEngine->GetGameUserSettings()->GetDefaultWindowMode() : EWindowMode::Windowed),
	bHitProxiesCached(false),
	bHasRequestedToggleFreeze(false),
	bIsSlateViewport(false),
	FlushOnDrawCount(0),
	bTakeHighResScreenShot(false)
{
	//initialize the hit proxy kernel
	HitProxySize = 5;
	if (GIsEditor) 
	{
		GConfig->GetInt( TEXT("UnrealEd.HitProxy"), TEXT("HitProxySize"), (int32&)HitProxySize, GEditorIni );
		FMath::Clamp( HitProxySize, (uint32)1, (uint32)MAX_HITPROXYSIZE );
	}

	// Cache the viewport client's hit proxy storage requirement.
	bRequiresHitProxyStorage = ViewportClient && ViewportClient->RequiresHitProxyStorage();
#if !WITH_EDITOR && !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if ( bRequiresHitProxyStorage )
	{
		UE_LOG(LogClient, Warning, TEXT("Consoles don't need hitproxy storage - wasting memory!?") );
	}
#endif

	AppVersionString = FString::Printf( TEXT( "Build: %s" ), FApp::GetBuildVersion() );

	bIsPlayInEditorViewport = false;
}

FViewport::~FViewport()
{
}

bool FViewport::TakeHighResScreenShot()
{
	if( GScreenshotResolutionX == 0 && GScreenshotResolutionY == 0 )
	{
		GScreenshotResolutionX = SizeX * GetHighResScreenshotConfig().ResolutionMultiplier;
		GScreenshotResolutionY = SizeY * GetHighResScreenshotConfig().ResolutionMultiplier;
	}

	uint32 MaxTextureDimension = GetMax2DTextureDimension();

	// Check that we can actually create a destination texture of this size
	if (GScreenshotResolutionX > MaxTextureDimension || GScreenshotResolutionY > MaxTextureDimension)
	{
		// Send a notification to tell the user the screenshot has failed
		auto Message = NSLOCTEXT("UnrealClient", "HighResScreenshotTooBig", "The high resolution screenshot multiplier is too large for your system. Please try again with a smaller value!");
		FNotificationInfo Info(Message);
		Info.bFireAndForget = true;
		Info.ExpireDuration = 5.0f;
		Info.bUseSuccessFailIcons = false;
		Info.bUseLargeFont = false;
		FSlateNotificationManager::Get().AddNotification(Info); 

		UE_LOG(LogClient, Warning, TEXT("The specified multiplier for high resolution screenshot is too large for your system! Please try again with a smaller value."));

		GIsHighResScreenshot = false;
		return false;
	}
	else
	{
		// Everything is OK. Take the shot.
		bTakeHighResScreenShot = true;

		//Force a redraw.
		Invalidate();	

		return true;
	}
}

void FViewport::HighResScreenshot()
{
	// We need to cache this as FScreenshotRequest is a global and the filename is
	// cleared out before we use it below
	const FString CachedScreenshotName = FScreenshotRequest::GetFilename();

	FIntPoint RestoreSize(SizeX, SizeY);

	FDummyViewport* DummyViewport = new FDummyViewport(ViewportClient);

	DummyViewport->SizeX = (GScreenshotResolutionX > 0) ? GScreenshotResolutionX : SizeX;
	DummyViewport->SizeY = (GScreenshotResolutionY > 0) ? GScreenshotResolutionY : SizeY;

	BeginInitResource(DummyViewport);

	bool MaskShowFlagBackup = ViewportClient->GetEngineShowFlags()->HighResScreenshotMask;
	const auto MotionBlurShowFlagBackup = ViewportClient->GetEngineShowFlags()->MotionBlur;

	ViewportClient->GetEngineShowFlags()->SetHighResScreenshotMask(GetHighResScreenshotConfig().bMaskEnabled);
	ViewportClient->GetEngineShowFlags()->SetMotionBlur(false);

	// Forcing 128-bit rendering pipeline
	static auto CVarSceneColorFormat = IConsoleManager::Get().FindConsoleVariable(TEXT("r.SceneColorFormat"));
	static auto CVarPostColorFormat = IConsoleManager::Get().FindConsoleVariable(TEXT("r.PostProcessingColorFormat"));
	static auto CVarForceLOD = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ForceLOD"));

	check(CVarSceneColorFormat && CVarPostColorFormat);
	const int32 OldSceneColorFormat = CVarSceneColorFormat->GetInt();
	const int32 OldPostColorFormat = CVarPostColorFormat->GetInt();
	const int32 OldForceLOD = CVarForceLOD ? CVarForceLOD->GetInt() : -1;
	if (GetHighResScreenshotConfig().bForce128BitRendering)
	{
		CVarSceneColorFormat->Set(5, ECVF_SetByCode);
		CVarPostColorFormat->Set(1, ECVF_SetByCode);
	}

	if(CVarForceLOD)
	{
		// Force highest LOD
		CVarForceLOD->Set(0, ECVF_SetByCode);
	}

	// Render the requested number of frames (at least once)
	static const auto HighResScreenshotDelay = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.HighResScreenshotDelay"));
	const uint32 DefaultScreenshotDelay = 4;
	uint32 FrameDelay = HighResScreenshotDelay ? FMath::Max(HighResScreenshotDelay->GetValueOnGameThread(), 1) : DefaultScreenshotDelay;

	while (FrameDelay)
	{
		DummyViewport->EnqueueBeginRenderFrame();

		FCanvas Canvas(DummyViewport, NULL, ViewportClient->GetWorld(), ViewportClient->GetWorld()->FeatureLevel);
		{
			ViewportClient->Draw(DummyViewport, &Canvas);
		}
		Canvas.Flush_GameThread();

		// Draw the debug canvas
		DummyViewport->GetDebugCanvas()->Flush_GameThread(true);
		FlushRenderingCommands();

		--FrameDelay;
	}

	ViewportClient->GetEngineShowFlags()->SetHighResScreenshotMask(MaskShowFlagBackup);
	ViewportClient->GetEngineShowFlags()->MotionBlur = MotionBlurShowFlagBackup;
	ViewportClient->ProcessScreenShots(DummyViewport);

	CVarSceneColorFormat->Set(OldSceneColorFormat, ECVF_SetByCode);
	CVarPostColorFormat->Set(OldPostColorFormat, ECVF_SetByCode);
	if (CVarForceLOD)
	{
		CVarForceLOD->Set(OldForceLOD, ECVF_SetByCode);
	}

	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		EndDrawingCommand,
		FViewport*,Viewport,DummyViewport,
		FIntPoint,InRestoreSize,RestoreSize,
	{
		Viewport->EndRenderFrame(RHICmdList, false, false);
		GetRendererModule().SceneRenderTargetsSetBufferSize(InRestoreSize.X, InRestoreSize.Y);
	});

	BeginReleaseResource(DummyViewport);
	FlushRenderingCommands();
	delete DummyViewport;

	// once the screenshot is done we disable the feature to get only one frame
	GIsHighResScreenshot = false;
	bTakeHighResScreenShot = false;

	// Notification of a successful screenshot
	if ((GIsEditor || !IsFullscreen()) && !GIsAutomationTesting )
	{
		auto Message = NSLOCTEXT("UnrealClient", "HighResScreenshotSavedAs", "High resolution screenshot saved as");
		FNotificationInfo Info(Message);
		Info.bFireAndForget = true;
		Info.ExpireDuration = 5.0f;
		Info.bUseSuccessFailIcons = false;
		Info.bUseLargeFont = false;
		
		const FString HyperLinkText = FPaths::ConvertRelativePathToFull(CachedScreenshotName);
		Info.Hyperlink = FSimpleDelegate::CreateStatic([](FString SourceFilePath) 
		{
			FPlatformProcess::ExploreFolder(*(FPaths::GetPath(SourceFilePath)));
		}, HyperLinkText);
		Info.HyperlinkText = FText::FromString(HyperLinkText);
		
		FSlateNotificationManager::Get().AddNotification(Info);
		UE_LOG(LogClient, Log, TEXT("%s %s"), *Message.ToString(), *HyperLinkText);
	}
}

struct FEndDrawingCommandParams
{
	FViewport* Viewport;
	uint32 bLockToVsync : 1;
	uint32 bShouldTriggerTimerEvent : 1;
	uint32 bShouldPresent : 1;
};

/**
 * Helper function used in ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER below. Needed to be split out due to
 * use of macro and former already being one.
 *
 * @param Parameters	Parameters passed from the gamethread to the renderthread command.
 */
static void ViewportEndDrawing(FRHICommandListImmediate& RHICmdList, FEndDrawingCommandParams Parameters)
{	
	GInputLatencyTimer.RenderThreadTrigger = Parameters.bShouldTriggerTimerEvent;
	Parameters.Viewport->EndRenderFrame(RHICmdList, Parameters.bShouldPresent, Parameters.bLockToVsync);
}

/** Starts a new rendering frame. Called from the rendering thread. */
void FViewport::BeginRenderFrame(FRHICommandListImmediate& RHICmdList)
{
	check( IsInRenderingThread() );
	RHICmdList.BeginDrawingViewport(GetViewportRHI(), FTextureRHIRef());
	UpdateRenderTargetSurfaceRHIToCurrentBackBuffer();
}

/**
 *	Ends a rendering frame. Called from the rendering thread.
 *	@param bPresent		Whether the frame should be presented to the screen
 *	@param bLockToVsync	Whether the GPU should block until VSYNC before presenting
 */
void FViewport::EndRenderFrame(FRHICommandListImmediate& RHICmdList, bool bPresent, bool bLockToVsync)
{
	check( IsInRenderingThread() );

	uint32 StartTime = FPlatformTime::Cycles();
	RHICmdList.EndDrawingViewport(GetViewportRHI(), bPresent, bLockToVsync);
	uint32 EndTime = FPlatformTime::Cycles();

	GRenderThreadIdle[ERenderThreadIdleTypes::WaitingForGPUPresent] += EndTime - StartTime;
	GRenderThreadNumIdle[ERenderThreadIdleTypes::WaitingForGPUPresent]++;
}

void InsertVolume(IInterface_PostProcessVolume* Volume, TArray< IInterface_PostProcessVolume* >& VolumeArray)
{
	const int32 NumVolumes = VolumeArray.Num();
	float TargetPriority = Volume->GetProperties().Priority;
	int32 InsertIndex = 0;
	// TODO: replace with binary search.
	for (; InsertIndex < NumVolumes ; InsertIndex++)
	{
		IInterface_PostProcessVolume* CurrentVolume = VolumeArray[InsertIndex];
		float CurrentPriority = CurrentVolume->GetProperties().Priority;

		if (TargetPriority < CurrentPriority)
		{
			break;
		}
		if (CurrentVolume == Volume)
		{
			return;
		}
	}
	VolumeArray.Insert(Volume, InsertIndex);
}

void APostProcessVolume::PostUnregisterAllComponents()
{
	// Route clear to super first.
	Super::PostUnregisterAllComponents();
	// World will be NULL during exit purge.
	if (GetWorld())
	{
		GetWorld()->PostProcessVolumes.RemoveSingle(this);
	}
}

void APostProcessVolume::PostRegisterAllComponents()
{
	// Route update to super first.
	Super::PostRegisterAllComponents();
	InsertVolume(this, GetWorld()->PostProcessVolumes);
}

void UPostProcessComponent::OnRegister()
{
	Super::OnRegister();
	InsertVolume(this, GetWorld()->PostProcessVolumes);
}

void UPostProcessComponent::OnUnregister()
{
	Super::OnUnregister();
	GetWorld()->PostProcessVolumes.RemoveSingle(this);
}

void UPostProcessComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if(Ar.IsLoading())
	{
		Settings.OnAfterLoad();
	}
}

/**
*	Starts a new rendering frame. Called from the game thread thread.
*/
void FViewport::EnqueueBeginRenderFrame()
{
	AdvanceFrameRenderPrerequisite();
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		BeginDrawingCommand,
		FViewport*,Viewport,this,
	{
		Viewport->BeginRenderFrame(RHICmdList);
	});
}


// true: The CompositionInspectur Slate UI requests it's data
bool GCaptureCompositionNextFrame = false;


void FViewport::Draw( bool bShouldPresent /*= true */)
{
	SCOPED_NAMED_EVENT(FViewport_Draw, FColor::Red);
	UWorld* World = GetClient()->GetWorld();
	static TUniquePtr<FSuspendRenderingThread> GRenderingThreadSuspension;

	// Ignore reentrant draw calls, since we can only redraw one viewport at a time.
	static bool bReentrant = false;
	if(!bReentrant)
	{
		// See what screenshot related features are required
		static const auto CVarDumpFrames = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.BufferVisualizationDumpFrames"));
		GIsHighResScreenshot = GIsHighResScreenshot || bTakeHighResScreenShot;
		bool bAnyScreenshotsRequired = FScreenshotRequest::IsScreenshotRequested() || GIsHighResScreenshot || GIsDumpingMovie;
		bool bBufferVisualizationDumpingRequired = bAnyScreenshotsRequired && CVarDumpFrames && CVarDumpFrames->GetValueOnGameThread();


		if(GCaptureCompositionNextFrame)
		{
			// To capture the CompositionGraph we go into single threaded for one frame
			// so that the Slate UI gets the data on the game thread.
			GRenderingThreadSuspension = MakeUnique<FSuspendRenderingThread>(true);
		}

		// if this is a game viewport, and game rendering is disabled, then we don't want to actually draw anything
		if ( World && World->IsGameWorld() && !bIsGameRenderingEnabled)
		{
			// since we aren't drawing the viewport, we still need to update streaming
			World->UpdateLevelStreaming();
		}
		else
		{
			if( GIsHighResScreenshot )
			{
				const bool bShowUI = false;
				const bool bAddFilenameSuffix = GetHighResScreenshotConfig().FilenameOverride.IsEmpty();
				FScreenshotRequest::RequestScreenshot( FString(), bShowUI, bAddFilenameSuffix );
				HighResScreenshot();
			}
			else if(bAnyScreenshotsRequired && bBufferVisualizationDumpingRequired)
			{
				// request the screenshot early so we have the name setup that BufferVisualization can dump it's content
				const bool bShowUI = false;
				const bool bAddFilenameSuffix = true;
				FScreenshotRequest::RequestScreenshot( FString(), bShowUI, bAddFilenameSuffix );
			}
	
			if( SizeX > 0 && SizeY > 0 )
			{
				static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.VSync"));
				bool bLockToVsync = CVar->GetValueOnGameThread() != 0;
				ULocalPlayer* Player = (GEngine && World) ? GEngine->GetFirstGamePlayer(World) : NULL;
				if ( Player )
				{
					bLockToVsync |= (Player && Player->PlayerController && Player->PlayerController->bCinematicMode);
				}
	 			EnqueueBeginRenderFrame();

				// Calculate gamethread time (excluding idle time)
				{
					static uint32 Lastimestamp = 0;
					static bool bStarted = false;
					uint32 CurrentTime	= FPlatformTime::Cycles();
					FThreadIdleStats& GameThread = FThreadIdleStats::Get();
					if (bStarted)
					{
						uint32 ThreadTime	= CurrentTime - Lastimestamp;
						// add any stalls via sleep or fevent
						GGameThreadTime		= (ThreadTime > GameThread.Waits) ? (ThreadTime - GameThread.Waits) : ThreadTime;
					}
					else
					{
						bStarted = true;
					}

					Lastimestamp		= CurrentTime;
					GameThread.Waits = 0;
				}

				UWorld* ViewportWorld = ViewportClient->GetWorld();
				FCanvas Canvas(this, NULL, ViewportWorld, ViewportWorld ? ViewportWorld->FeatureLevel : GMaxRHIFeatureLevel);
				Canvas.SetRenderTargetRect(FIntRect(0, 0, SizeX, SizeY));
				{
					// Make sure the Canvas is not rendered upside down
					Canvas.SetAllowSwitchVerticalAxis(false);
					ViewportClient->Draw(this, &Canvas);
				}
				Canvas.Flush_GameThread();
				ViewportClient->ProcessScreenShots(this);
	
				// Slate doesn't present immediately. Tag the viewport as requiring vsync so that it happens.
				SetRequiresVsync(bLockToVsync);

				//@todo UE4: If Slate controls this viewport, we should not present
				FEndDrawingCommandParams Params = { this, (uint32)bLockToVsync, (uint32)GInputLatencyTimer.GameThreadTrigger, (uint32)(PresentAndStopMovieDelay > 0 ? 0 : bShouldPresent) };
				ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
					EndDrawingCommand,
					FEndDrawingCommandParams,Parameters,Params,
				{
					ViewportEndDrawing(RHICmdList, Parameters);
				});

				GInputLatencyTimer.GameThreadTrigger = false;
			}
		}

		// Reset the camera cut flags if we are in a viewport that has a world
		if (World)
		{
			for( FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator )
			{
				APlayerController* PlayerController = Iterator->Get();
				if (PlayerController && PlayerController->PlayerCameraManager)
				{
					PlayerController->PlayerCameraManager->bGameCameraCutThisFrame = false;
				}
			}
		}

		// countdown the present delay, and then stop the movie at the end
		// this doesn't need to be on rendering thread as long as we have a long enough delay (2 or 3 frames), because
		// the rendering thread will never be more than one frame behind
		if (PresentAndStopMovieDelay > 0)
		{
			PresentAndStopMovieDelay--;
			// stop any playing movie
			if (PresentAndStopMovieDelay == 0)
			{
				// Enable game rendering again if it isn't already.
				bIsGameRenderingEnabled = true;
			}
		}

		if (FlushOnDrawCount != 0)
		{
			FlushRenderingCommands();
		}

		if(GCaptureCompositionNextFrame)
		{
			GRenderingThreadSuspension.Reset();
			GCaptureCompositionNextFrame = false;
		}
	}
}


void FViewport::InvalidateHitProxy()
{
	bHitProxiesCached = false;
	HitProxyMap.Invalidate();
}



void FViewport::Invalidate()
{
	DeferInvalidateHitProxy();
	InvalidateDisplay();
}


void FViewport::DeferInvalidateHitProxy()
{
	// Default implementation does not defer.  Overridden implementations may.
	InvalidateHitProxy();
}

const TArray<FColor>& FViewport::GetRawHitProxyData(FIntRect InRect)
{
	FScopedConditionalWorldSwitcher WorldSwitcher(ViewportClient);

	const bool bIsRenderingStereo = GEngine->IsStereoscopic3D( this ) && this->IsStereoRenderingAllowed();

	bool bFetchHitProxyBytes = !bIsRenderingStereo && ( !bHitProxiesCached || (SizeY*SizeX) != CachedHitProxyData.Num() );

	if( bIsRenderingStereo )
	{
		// Stereo viewports don't support hit proxies, and we don't want to update them because it will adversely
		// affect performance.
		CachedHitProxyData.SetNumZeroed( SizeY * SizeX );
	}
	// If the hit proxy map isn't up to date, render the viewport client's hit proxies to it.
	else if (!bHitProxiesCached)
	{
		EnqueueBeginRenderFrame();

		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			BeginDrawingCommandHitProxy,
			FViewport*, Viewport, this,
			{
			// Set the hit proxy map's render target.
			// Clear the hit proxy map to white, which is overloaded to mean no hit proxy.
			SetRenderTarget(RHICmdList, Viewport->HitProxyMap.GetRenderTargetTexture(), FTextureRHIRef(), ESimpleRenderTargetMode::EClearColorExistingDepth, FExclusiveDepthStencil::DepthWrite_StencilWrite, true);
		});

		// Let the viewport client draw its hit proxies.
		auto World = ViewportClient->GetWorld();
		FCanvas Canvas(&HitProxyMap, &HitProxyMap, World, World ? World->FeatureLevel : GMaxRHIFeatureLevel);
		{
			ViewportClient->Draw(this, &Canvas);
		}
		Canvas.Flush_GameThread();

		//Resolve surface to texture.
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			UpdateHitProxyRTCommand,
			FHitProxyMap*, HitProxyMap, &HitProxyMap,
			{
			// Copy (resolve) the rendered thumbnail from the render target to its texture
			RHICmdList.CopyToResolveTarget(HitProxyMap->GetRenderTargetTexture(), HitProxyMap->GetHitProxyTexture(), false, FResolveParams());
			RHICmdList.CopyToResolveTarget(HitProxyMap->GetRenderTargetTexture(), HitProxyMap->GetHitProxyCPUTexture(), false, FResolveParams());
		});

		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			EndDrawingCommand,
			FViewport*, Viewport, this,
			{
				Viewport->EndRenderFrame(RHICmdList, false, false);
			});

		// Cache the hit proxies for the next GetHitProxyMap call.
		bHitProxiesCached = true;
	}

	if (bFetchHitProxyBytes)
	{
		// Read the hit proxy map surface data back.
		FIntRect ViewportRect(0, 0, SizeX, SizeY);
		struct FReadSurfaceContext
		{
			FViewport* Viewport;
			TArray<FColor>* OutData;
			FIntRect Rect;
		};
		FReadSurfaceContext ReadSurfaceContext =
		{
			this,
			&CachedHitProxyData,
			ViewportRect
		};

		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			ReadSurfaceCommand,
			FReadSurfaceContext, Context, ReadSurfaceContext,
			{
			RHICmdList.ReadSurfaceData(
			Context.Viewport->HitProxyMap.GetHitProxyCPUTexture(),
			Context.Rect,
			*Context.OutData,
			FReadSurfaceDataFlags()
			);
		});
		FlushRenderingCommands();
	}

	return CachedHitProxyData;

}
void FViewport::GetHitProxyMap(FIntRect InRect,TArray<HHitProxy*>& OutMap)
{
	const TArray<FColor>& CachedData = GetRawHitProxyData(InRect);
	if (CachedData.Num()==0)
	{
		return;
	}
	
	// Map the hit proxy map surface data to hit proxies.
	OutMap.Empty(InRect.Width() * InRect.Height());
	for(int32 Y = InRect.Min.Y; Y < InRect.Max.Y; Y++)
	{
		if (!CachedData.IsValidIndex(int32(Y * SizeX)))
		{
			break;
		}

		const FColor* SourceData = &CachedData[Y * SizeX];
		for(int32 X = InRect.Min.X ;X < InRect.Max.X; X++)
		{
			FHitProxyId HitProxyId(SourceData[X]);
			OutMap.Add(GetHitProxyById(HitProxyId));
		}
	}
}

HHitProxy* FViewport::GetHitProxy(int32 X,int32 Y)
{
	// Compute a HitProxySize x HitProxySize test region with the center at (X,Y).
	int32	MinX = X - HitProxySize,
			MinY = Y - HitProxySize,
			MaxX = X + HitProxySize,
			MaxY = Y + HitProxySize;

	FIntPoint VPSize = GetSizeXY();
	
	// Clip the region to the viewport bounds.
	MinX = FMath::Clamp(MinX, 0, VPSize.X - 1);
	MinY = FMath::Clamp(MinY, 0, VPSize.Y - 1);
	MaxX = FMath::Clamp(MaxX, 0, VPSize.X - 1);
	MaxY = FMath::Clamp(MaxY, 0, VPSize.Y - 1);

	int32 TestSizeX	= MaxX - MinX + 1;
	int32 TestSizeY	= MaxY - MinY + 1;
	HHitProxy* HitProxy = NULL;

	if(TestSizeX > 0 && TestSizeY > 0)
	{
		// Read the hit proxy map from the device.
		TArray<HHitProxy*>	ProxyMap;
		GetHitProxyMap(FIntRect(MinX, MinY, MaxX + 1, MaxY + 1),ProxyMap);
		check(ProxyMap.Num() == TestSizeX * TestSizeY);

		// Find the hit proxy in the test region with the highest order.
		int32 ProxyIndex = TestSizeY/2 * TestSizeX + TestSizeX/2;
		check(ProxyIndex<ProxyMap.Num());
		HitProxy = ProxyMap[ProxyIndex];
	
		bool bIsOrtho = GetClient()->IsOrtho();

		for(int32 TestY = 0;TestY < TestSizeY;TestY++)
		{
			for(int32 TestX = 0;TestX < TestSizeX;TestX++)
			{
				HHitProxy* TestProxy = ProxyMap[TestY * TestSizeX + TestX];
				if(TestProxy && (!HitProxy || (bIsOrtho ? TestProxy->OrthoPriority : TestProxy->Priority) > (bIsOrtho ? HitProxy->OrthoPriority : HitProxy->Priority)))
				{
					HitProxy = TestProxy;
				}
			}
		}
	}

	return HitProxy;
}

void FViewport::UpdateViewportRHI(bool bDestroyed, uint32 NewSizeX, uint32 NewSizeY, EWindowMode::Type NewWindowMode, EPixelFormat PreferredPixelFormat)
{
	// Make sure we're not in the middle of streaming textures.
	(*GFlushStreamingFunc)();

	{
		// Temporarily stop rendering thread.
		SCOPED_SUSPEND_RENDERING_THREAD(true);

		// Update the viewport attributes.
		// This is done AFTER the command flush done by UpdateViewportRHI, to avoid disrupting rendering thread accesses to the old viewport size.
		SizeX = NewSizeX;
		SizeY = NewSizeY;
		WindowMode = NewWindowMode;

		// Release the viewport's resources.
		BeginReleaseResource(this);

		// Don't reinitialize the viewport RHI if the viewport has been destroyed.
		if(bDestroyed)
		{
			if(IsValidRef(ViewportRHI))
			{
				// If the viewport RHI has already been initialized, release it.
				ViewportRHI.SafeRelease();
			}
		}
		else
		{
			if(IsValidRef(ViewportRHI))
			{
				// If the viewport RHI has already been initialized, resize it.
				RHIResizeViewport(
					ViewportRHI,
					SizeX,
					SizeY,
					IsFullscreen(),
					PreferredPixelFormat
					);
			}
			else
			{
				// Initialize the viewport RHI with the new viewport state.
				ViewportRHI = RHICreateViewport(
					GetWindow(),
					SizeX,
					SizeY,
					IsFullscreen(),
					EPixelFormat::PF_Unknown /* ie, use default format */);
			}
		
			// Initialize the viewport's resources.
			BeginInitResource(this);
		}
	}

	if ( !bDestroyed )
	{
		// send a notification that the viewport has been resized
		ViewportResizedEvent.Broadcast(this, 0);
	}
}

FIntRect FViewport::CalculateViewExtents(float AspectRatio, const FIntRect& ViewRect)
{
	FIntRect Result = ViewRect;

	const float CurrentSizeX = ViewRect.Width();
	const float CurrentSizeY = ViewRect.Height();

	// the viewport's SizeX/SizeY may not always match the GetDesiredAspectRatio(), so adjust the requested AspectRatio to compensate
	const float AdjustedAspectRatio = AspectRatio / (GetDesiredAspectRatio() / ((float)GetSizeXY().X / (float)GetSizeXY().Y));

	// If desired, enforce a particular aspect ratio for the render of the scene. 
	// Results in black bars at top/bottom etc.
	const float AspectRatioDifference = AdjustedAspectRatio - (CurrentSizeX / CurrentSizeY);

	if( FMath::Abs( AspectRatioDifference ) > 0.01f )
	{
		// If desired aspect ratio is bigger than current - we need black bars on top and bottom.
		if( AspectRatioDifference > 0.0f )
		{
			// Calculate desired Y size.
			const int32 NewSizeY = FMath::Max(1, FMath::RoundToInt( CurrentSizeX / AdjustedAspectRatio ) );
			Result.Min.Y = FMath::RoundToInt( 0.5f * (CurrentSizeY - NewSizeY) );
			Result.Max.Y = Result.Min.Y + NewSizeY;
			Result.Min.Y += ViewRect.Min.Y;
			Result.Max.Y += ViewRect.Min.Y;
		}
		// Otherwise - will place bars on the sides.
		else
		{
			const int32 NewSizeX = FMath::Max(1, FMath::RoundToInt( CurrentSizeY * AdjustedAspectRatio ) );
			Result.Min.X = FMath::RoundToInt( 0.5f * (CurrentSizeX - NewSizeX) );
			Result.Max.X = Result.Min.X + NewSizeX;
			Result.Min.X += ViewRect.Min.X;
			Result.Max.X += ViewRect.Min.X;
		}
	}

	return Result;
}

/**
 *	Sets a viewport client if one wasn't provided at construction time.
 *	@param InViewportClient	- The viewport client to set.
 **/
void FViewport::SetViewportClient( FViewportClient* InViewportClient )
{
	ViewportClient = InViewportClient;
}

void FViewport::InitDynamicRHI()
{
	UpdateRenderTargetSurfaceRHIToCurrentBackBuffer();

	if(bRequiresHitProxyStorage)
	{
		// Initialize the hit proxy map.
		HitProxyMap.Init(SizeX,SizeY);
	}
}

void FViewport::ReleaseDynamicRHI()
{
	HitProxyMap.Release();
	RenderTargetTextureRHI.SafeRelease();
}

void FViewport::ReleaseRHI()
{
	SCOPED_SUSPEND_RENDERING_THREAD(true);
	ViewportRHI.SafeRelease();
}

void FViewport::InitRHI()
{
	SCOPED_SUSPEND_RENDERING_THREAD(true);

	if(!IsValidRef(ViewportRHI))
	{
		ViewportRHI = RHICreateViewport(
			GetWindow(),
			SizeX,
			SizeY,
			IsFullscreen(),
			EPixelFormat::PF_Unknown
			);
		UpdateRenderTargetSurfaceRHIToCurrentBackBuffer();
	}
}

ENGINE_API bool IsCtrlDown(FViewport* Viewport) { return (Viewport->KeyState(EKeys::LeftControl) || Viewport->KeyState(EKeys::RightControl)); }
ENGINE_API bool IsShiftDown(FViewport* Viewport) { return (Viewport->KeyState(EKeys::LeftShift) || Viewport->KeyState(EKeys::RightShift)); }
ENGINE_API bool IsAltDown(FViewport* Viewport) { return (Viewport->KeyState(EKeys::LeftAlt) || Viewport->KeyState(EKeys::RightAlt)); }


/** Constructor */
FViewport::FHitProxyMap::FHitProxyMap()
{
#if WITH_EDITOR
	FEditorSupportDelegates::CleanseEditor.AddRaw(this, &FViewport::FHitProxyMap::Invalidate);
#endif // WITH_EDITOR
}


/** Destructor */
FViewport::FHitProxyMap::~FHitProxyMap()
{
#if WITH_EDITOR
	FEditorSupportDelegates::CleanseEditor.RemoveAll(this);
#endif // WITH_EDITOR
}


void FViewport::FHitProxyMap::Init(uint32 NewSizeX,uint32 NewSizeY)
{
	SizeX = NewSizeX;
	SizeY = NewSizeY;

	// Create a render target to store the hit proxy map.
	{
		FRHIResourceCreateInfo CreateInfo(FClearValueBinding::White);
		RHICreateTargetableShaderResource2D(SizeX,SizeY,PF_B8G8R8A8,1,TexCreate_None,TexCreate_RenderTargetable,false,CreateInfo,RenderTargetTextureRHI,HitProxyTexture);
	}
	{
		FRHIResourceCreateInfo CreateInfo;
		HitProxyCPUTexture = RHICreateTexture2D(SizeX, SizeY, PF_B8G8R8A8,1,1,TexCreate_CPUReadback,CreateInfo);
	}
}

void FViewport::FHitProxyMap::Release()
{
	HitProxyTexture.SafeRelease();
	HitProxyCPUTexture.SafeRelease();
	RenderTargetTextureRHI.SafeRelease();
}

void FViewport::FHitProxyMap::Invalidate()
{
	HitProxies.Empty();
}

void FViewport::FHitProxyMap::AddHitProxy(HHitProxy* HitProxy)
{
	HitProxies.Add(HitProxy);
}


/** FGCObject: add UObject references to GC */
void FViewport::FHitProxyMap::AddReferencedObjects( FReferenceCollector& Collector )
{
	// Allow all of our hit proxy objects to serialize their references
	for( int32 CurProxyIndex = 0; CurProxyIndex < HitProxies.Num(); ++CurProxyIndex )
	{
		HHitProxy* CurProxy = HitProxies[ CurProxyIndex ];
		if( CurProxy != NULL )
		{
			CurProxy->AddReferencedObjects( Collector );
		}
	}
}

/**
 * Globally enables/disables rendering
 *
 * @param bIsEnabled true if drawing should occur
 * @param PresentAndStopMovieDelay Number of frames to delay before enabling bPresent in RHIEndDrawingViewport, and before stopping the movie
 */
void FViewport::SetGameRenderingEnabled(bool bIsEnabled, int32 InPresentAndStopMovieDelay)
{
	bIsGameRenderingEnabled = bIsEnabled;
	PresentAndStopMovieDelay = InPresentAndStopMovieDelay;
}

/**
 * Handles freezing/unfreezing of rendering
 */
void FViewport::ProcessToggleFreezeCommand()
{
	bHasRequestedToggleFreeze = 1;
}

/**
 * Returns if there is a command to toggle freezing
 */
bool FViewport::HasToggleFreezeCommand()
{
	// save the current command
	bool ReturnVal = bHasRequestedToggleFreeze;
	
	// make sure that we no longer have the command, as we are now passing off "ownership"
	// of the command
	bHasRequestedToggleFreeze = false;

	// return what it was
	return ReturnVal;
}

/**
 * Update the render target surface RHI to the current back buffer 
 */
void FViewport::UpdateRenderTargetSurfaceRHIToCurrentBackBuffer()
{
	if(IsValidRef(ViewportRHI))
	{
		RenderTargetTextureRHI = RHIGetViewportBackBuffer(ViewportRHI);
	}
}

void FViewport::SetInitialSize( FIntPoint InitialSizeXY )
{
	// Initial size only works if the viewport has not yet been resized
	if( GetSizeXY() == FIntPoint::ZeroValue )
	{
		UpdateViewportRHI( false, InitialSizeXY.X, InitialSizeXY.Y, EWindowMode::Windowed, PF_Unknown );
	}
}


ENGINE_API bool GetViewportScreenShot(FViewport* Viewport, TArray<FColor>& Bitmap, const FIntRect& ViewRect /*= FIntRect()*/)
{
	// Read the contents of the viewport into an array.
	if (Viewport->ReadPixels(Bitmap, FReadSurfaceDataFlags(), ViewRect))
	{
		check(Bitmap.Num() == ViewRect.Area() || (Bitmap.Num() == Viewport->GetSizeXY().X * Viewport->GetSizeXY().Y));
		return true;
	}

	return false;
}

extern bool ParseResolution( const TCHAR* InResolution, uint32& OutX, uint32& OutY, int32& WindowMode );

ENGINE_API bool GetHighResScreenShotInput(const TCHAR* Cmd, FOutputDevice& Ar, uint32& OutXRes, uint32& OutYRes, float& OutResMult, FIntRect& OutCaptureRegion, bool& OutShouldEnableMask, bool& OutDumpBufferVisualizationTargets, bool& OutCaptureHDR, FString& OutFilenameOverride)
{
	FString CmdString = Cmd;
	int32 SeperatorPos = -1;
	int32 LastSeperatorPos = 0;
	TArray<FString> Arguments;

	// Look for an optional filename to override from the default filename and strip it if found.
	FString FilenameSearchString = TEXT("filename=");
	int32 FilenamePos = CmdString.Find(FilenameSearchString, ESearchCase::IgnoreCase);
	if (FilenamePos != INDEX_NONE)
	{
		FString FilenameOverride;
		FParse::Value(Cmd, TEXT("filename="), FilenameOverride);
		OutFilenameOverride = FilenameOverride;
		CmdString.RemoveAt(FilenamePos, FilenameSearchString.Len() + FilenameOverride.Len());
		CmdString.TrimStartAndEndInline(); 
	}

	while (CmdString.FindChar(TCHAR(' '), SeperatorPos))
	{
		Arguments.Add(CmdString.Mid(LastSeperatorPos, SeperatorPos));
		CmdString = CmdString.Mid(SeperatorPos + 1);
	}

	if (CmdString.Len() > 0)
	{
		Arguments.Add(CmdString);
	}

	int32 NumArguments = Arguments.Num();

	if (NumArguments >= 1)
	{
		int32 WindowModeDummy;
		if( !ParseResolution( *Arguments[0], OutXRes, OutYRes, WindowModeDummy ) )
		{
			//If Cmd is valid and it's not a resolution then the input must be a multiplier.
			float Mult = FCString::Atof(*Arguments[0]);

			if( Mult > 0.0f && Arguments[0].IsNumeric() )
			{
				OutResMult = Mult;
			}
			else
			{
				Ar.Logf(TEXT("Error: Bad input. Input should be in either the form \"HighResShot 1920x1080\" or \"HighResShot 2\""));
				return false;
			}
		}
		else if( OutXRes <= 0 || OutYRes <= 0  )
		{
			Ar.Logf(TEXT("Error: Values must be greater than 0 in both dimensions"));
			return false;
		}
		else if( OutXRes > GetMax2DTextureDimension() || OutYRes > GetMax2DTextureDimension()  )
		{
			Ar.Logf(TEXT("Error: Screenshot size exceeds the maximum allowed texture size (%d x %d)"), GetMax2DTextureDimension(), GetMax2DTextureDimension());
			return false;
		}

		// Try and extract capture region from string
		int32 CaptureRegionX = NumArguments > 1 ? FCString::Atoi(*Arguments[1]) : 0;
		int32 CaptureRegionY = NumArguments > 2 ? FCString::Atoi(*Arguments[2]) : 0;
		int32 CaptureRegionWidth = NumArguments > 3 ? FCString::Atoi(*Arguments[3]) : 0;
		int32 CaptureRegionHeight = NumArguments > 4 ? FCString::Atoi(*Arguments[4]) : 0;
		OutCaptureRegion = FIntRect(CaptureRegionX, CaptureRegionY, CaptureRegionX + CaptureRegionWidth, CaptureRegionY + CaptureRegionHeight);

		OutShouldEnableMask = NumArguments > 5 ? FCString::Atoi(*Arguments[5]) != 0 : false;
		OutDumpBufferVisualizationTargets = NumArguments > 6 ? FCString::Atoi(*Arguments[6]) != 0 : false;
		OutCaptureHDR = NumArguments > 7 ? FCString::Atoi(*Arguments[7]) != 0 : false;

		return true;
	}
	else
	{
		Ar.Logf(TEXT("Error: Bad input. Input should be in either the form \"HighResShot 1920x1080\" or \"HighResShot 2\""));
	}

	return false;
}

/** Tracks the viewport client that should process the stat command, can be NULL */
FCommonViewportClient* GStatProcessingViewportClient = NULL;

void FCommonViewportClient::DrawHighResScreenshotCaptureRegion(FCanvas& Canvas)
{
	const FLinearColor BoxColor = FLinearColor::Red;
	FHighResScreenshotConfig& Config = GetHighResScreenshotConfig();

	FCanvasLineItem LineItem;

	// Draw the line a line in X and Y extending out from the center.
	LineItem.SetColor( BoxColor );
	LineItem.Draw( &Canvas, FVector2D(Config.UnscaledCaptureRegion.Min.X, Config.UnscaledCaptureRegion.Min.Y), FVector2D(Config.UnscaledCaptureRegion.Max.X, Config.UnscaledCaptureRegion.Min.Y) );
	LineItem.Draw( &Canvas, FVector2D(Config.UnscaledCaptureRegion.Max.X, Config.UnscaledCaptureRegion.Min.Y), FVector2D(Config.UnscaledCaptureRegion.Max.X, Config.UnscaledCaptureRegion.Max.Y));
	LineItem.Draw( &Canvas, FVector2D(Config.UnscaledCaptureRegion.Max.X, Config.UnscaledCaptureRegion.Max.Y), FVector2D(Config.UnscaledCaptureRegion.Min.X, Config.UnscaledCaptureRegion.Max.Y));
	LineItem.Draw( &Canvas, FVector2D(Config.UnscaledCaptureRegion.Min.X, Config.UnscaledCaptureRegion.Max.Y), FVector2D(Config.UnscaledCaptureRegion.Min.X, Config.UnscaledCaptureRegion.Min.Y));
}

#if WITH_EDITOR

void FCommonViewportClient::RequestUpdateEditorScreenPercentage()
{
	bShouldUpdateScreenPercentage = true;
}

TOptional<float> FCommonViewportClient::GetEditorScreenPercentage()
{
	// When in high res screenshot do not modify screen percentage based on dpi scale
	if (GIsHighResScreenshot)
	{
		return TOptional<float>();
	}
	else
	{
		if (bShouldUpdateScreenPercentage)
		{
			static auto CVarEnableEditorScreenPercentageOverride = IConsoleManager::Get().FindConsoleVariable(TEXT("Editor.OverrideDPIBasedEditorViewportScaling"));
			if (CVarEnableEditorScreenPercentageOverride->GetInt() == 0)
			{
				float EditorScreenPercentageValue;
				float DPIScale = GetViewportClientWindowDPIScale();

				if (DPIScale > 1.0f)
				{
					EditorScreenPercentageValue = 100.f / DPIScale;
				}
				else
				{
					EditorScreenPercentageValue = 100.0f;
				}

				EditorScreenPercentage = TOptional<float>(EditorScreenPercentageValue);
			}
			else
			{
				EditorScreenPercentage.Reset();
			}

			bShouldUpdateScreenPercentage = false;
		}

		return EditorScreenPercentage;
	}
}
#endif

/**
 * FDummyViewport
 */

FDummyViewport::FDummyViewport(FViewportClient* InViewportClient)
	: FViewport(InViewportClient)
	, DebugCanvas(NULL)
{
	UWorld* CurWorld = (InViewportClient != NULL ? InViewportClient->GetWorld() : NULL);
	DebugCanvas = new FCanvas(this, NULL, CurWorld, (CurWorld != NULL ? CurWorld->FeatureLevel : GMaxRHIFeatureLevel));
		
	DebugCanvas->SetAllowedModes(0);
}

FDummyViewport::~FDummyViewport()
{
	if (DebugCanvas != NULL)
	{
		delete DebugCanvas;
		DebugCanvas = NULL;
	}
}
