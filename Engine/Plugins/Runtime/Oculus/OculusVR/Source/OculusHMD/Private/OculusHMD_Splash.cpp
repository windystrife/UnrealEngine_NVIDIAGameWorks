// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OculusHMD_Splash.h"

#if OCULUS_HMD_SUPPORTED_PLATFORMS
#include "OculusHMD.h"
#include "RenderingThread.h"
#include "Misc/ScopeLock.h"
#include "OculusHMDRuntimeSettings.h"
#if PLATFORM_ANDROID
#include "Android/AndroidJNI.h"
#include "Android/AndroidEGL.h"
#include "AndroidApplication.h"
#include "OculusHMDTypes.h"
#endif

namespace OculusHMD
{

//-------------------------------------------------------------------------------------------------
// FSplash
//-------------------------------------------------------------------------------------------------

FSplash::FSplash(FOculusHMD* InOculusHMD) :
	OculusHMD(InOculusHMD),
	CustomPresent(InOculusHMD->GetCustomPresent_Internal()),
	NextLayerId(1),
	bInitialized(false),
	bTickable(false),
	bLoadingStarted(false),
	bLoadingCompleted(false),
	bLoadingIconMode(false),
	bAutoShow(true),
	bIsBlack(true),
	SystemDisplayInterval(1 / 90.0f),
	ShowFlags(0)
{
	UOculusHMDRuntimeSettings* HMDSettings = GetMutableDefault<UOculusHMDRuntimeSettings>();
	check(HMDSettings);
	bAutoShow = HMDSettings->bAutoEnabled;
	for (const FOculusSplashDesc& SplashDesc : HMDSettings->SplashDescs)
	{
		AddSplash(SplashDesc);
	}

	// Create empty quad layer for black frame
	{
		IStereoLayers::FLayerDesc LayerDesc;
		LayerDesc.QuadSize = FVector2D(0.01f, 0.01f);
		LayerDesc.Priority = 0;
		LayerDesc.PositionType = IStereoLayers::TrackerLocked;
		LayerDesc.ShapeType = IStereoLayers::QuadLayer;
		LayerDesc.Texture = GBlackTexture->TextureRHI;
		BlackLayer = MakeShareable(new FLayer(NextLayerId++, LayerDesc));
	}
}


FSplash::~FSplash()
{
	// Make sure RenTicker is freed in Shutdown
	check(!Ticker.IsValid())
}

void FSplash::Tick_RenderThread(float DeltaTime)
{
	CheckInRenderThread();

	const double TimeInSeconds = FPlatformTime::Seconds();
	const double DeltaTimeInSeconds = TimeInSeconds - LastTimeInSeconds;

	// Render every 1/3 secs if nothing animating, or every other frame if rotation is needed
	bool bRenderFrame = DeltaTimeInSeconds > 1.f / 3.f;

	if(DeltaTimeInSeconds > 2.f * SystemDisplayInterval)
	{
		FScopeLock ScopeLock(&RenderThreadLock);

		for (int32 SplashLayerIndex = 0; SplashLayerIndex < SplashLayers.Num(); SplashLayerIndex++)
		{
			FSplashLayer& SplashLayer = SplashLayers[SplashLayerIndex];

			if (SplashLayer.Layer.IsValid() && !SplashLayer.Desc.DeltaRotation.Equals(FQuat::Identity))
			{
				IStereoLayers::FLayerDesc LayerDesc = SplashLayer.Layer->GetDesc();
				LayerDesc.Transform.SetRotation(SplashLayer.Desc.DeltaRotation * LayerDesc.Transform.GetRotation());

				FLayer* Layer = new FLayer(*SplashLayer.Layer);
				Layer->SetDesc(LayerDesc);
				SplashLayer.Layer = MakeShareable(Layer);

				bRenderFrame = true;
			}
		}
	}

	if (bRenderFrame)
	{
		RenderFrame_RenderThread(FRHICommandListExecutor::GetImmediateCommandList(), TimeInSeconds);
	}
}


void FSplash::RenderFrame_RenderThread(FRHICommandListImmediate& RHICmdList, double TimeInSeconds)
{
	CheckInRenderThread();

	// RenderFrame
	FSettingsPtr XSettings;
	FGameFramePtr XFrame;
	TArray<FLayerPtr> XLayers;
	{
		FScopeLock ScopeLock(&RenderThreadLock);
		XSettings = Settings->Clone();
		XFrame = Frame->Clone();
		XFrame->FrameNumber = OculusHMD->NextFrameNumber++;

		if(!bIsBlack)
		{
			for(int32 SplashLayerIndex = 0; SplashLayerIndex < SplashLayers.Num(); SplashLayerIndex++)
			{
				const FSplashLayer& SplashLayer = SplashLayers[SplashLayerIndex];

				if(SplashLayer.Layer.IsValid())
				{
					XLayers.Add(SplashLayer.Layer->Clone());
				}
			}

			XLayers.Sort(FLayerPtr_CompareId());
		}
		else
		{
			XLayers.Add(BlackLayer->Clone());
		}
	}

//	UE_LOG(LogHMD, Log, TEXT("Splash ovrp_WaitToBeginFrame %u"), XFrame->FrameNumber);
	ovrp_WaitToBeginFrame(XFrame->FrameNumber);
	ovrp_Update3(ovrpStep_Render, XFrame->FrameNumber, 0.0);

	{
		int32 LayerIndex = 0;
		int32 LayerIndex_RenderThread = 0;

		while(LayerIndex < XLayers.Num() && LayerIndex_RenderThread < Layers_RenderThread.Num())
		{
			uint32 LayerIdA = XLayers[LayerIndex]->GetId();
			uint32 LayerIdB = Layers_RenderThread[LayerIndex_RenderThread]->GetId();

			if (LayerIdA < LayerIdB)
			{
				XLayers[LayerIndex++]->Initialize_RenderThread(CustomPresent, RHICmdList);
			}
			else if (LayerIdA > LayerIdB)
			{
				LayerIndex_RenderThread++;
			}
			else
			{
				XLayers[LayerIndex++]->Initialize_RenderThread(CustomPresent, RHICmdList, Layers_RenderThread[LayerIndex_RenderThread++].Get());
			}
		}

		while(LayerIndex < XLayers.Num())
		{
			XLayers[LayerIndex++]->Initialize_RenderThread(CustomPresent, RHICmdList);
		}
	}

	Layers_RenderThread = XLayers;

	for (int32 LayerIndex = 0; LayerIndex < Layers_RenderThread.Num(); LayerIndex++)
	{
		Layers_RenderThread[LayerIndex]->UpdateTexture_RenderThread(CustomPresent, RHICmdList);
	}


	// RHIFrame
	for (int32 LayerIndex = 0; LayerIndex < XLayers.Num(); LayerIndex++)
	{
		XLayers[LayerIndex] = XLayers[LayerIndex]->Clone();
	}

	ExecuteOnRHIThread_DoNotWait([this, XSettings, XFrame, XLayers]()
	{
//		UE_LOG(LogHMD, Log, TEXT("Splash ovrp_BeginFrame4 %u"), XFrame->FrameNumber);
		ovrp_BeginFrame4(XFrame->FrameNumber, CustomPresent->GetOvrpCommandQueue());

		Layers_RHIThread = XLayers;
		Layers_RHIThread.Sort(FLayerPtr_ComparePriority());
		TArray<const ovrpLayerSubmit*> LayerSubmitPtr;
		LayerSubmitPtr.SetNum(Layers_RHIThread.Num());

		for (int32 LayerIndex = 0; LayerIndex < Layers_RHIThread.Num(); LayerIndex++)
		{
			LayerSubmitPtr[LayerIndex] = Layers_RHIThread[LayerIndex]->UpdateLayer_RHIThread(XSettings.Get(), XFrame.Get());
		}

//		UE_LOG(LogHMD, Log, TEXT("Splash ovrp_EndFrame4 %u"), XFrame->FrameNumber);
		ovrp_EndFrame4(XFrame->FrameNumber, LayerSubmitPtr.GetData(), LayerSubmitPtr.Num(), CustomPresent->GetOvrpCommandQueue());

		for (int32 LayerIndex = 0; LayerIndex < Layers_RHIThread.Num(); LayerIndex++)
		{
			Layers_RHIThread[LayerIndex]->IncrementSwapChainIndex_RHIThread(CustomPresent);
		}
	});

	LastTimeInSeconds = TimeInSeconds;
}


bool FSplash::IsShown() const
{
	return (ShowFlags != 0) || (bAutoShow && bLoadingStarted && !bLoadingCompleted);
}


void FSplash::Startup()
{
	CheckInGameThread();

	if (!bInitialized)
	{
		Ticker = MakeShareable(new FTicker(this));

		ExecuteOnRenderThread_DoNotWait([this]()
		{
			Ticker->Register();
		});

		// Check to see if we want to use autoloading splash screens from the config
		const TCHAR* OculusSettings = TEXT("Oculus.Settings");
		bool bUseAutoShow = true;
		GConfig->GetBool(OculusSettings, TEXT("bUseAutoLoadingSplashScreen"), bUseAutoShow, GEngineIni);
		bAutoShow = bUseAutoShow;

		// Add a delegate to start playing movies when we start loading a map
		FCoreUObjectDelegates::PreLoadMap.AddSP(this, &FSplash::OnPreLoadMap);
		FCoreUObjectDelegates::PostLoadMapWithWorld.AddSP(this, &FSplash::OnPostLoadMap);

		bInitialized = true;
	}
}


void FSplash::PreShutdown()
{
	CheckInGameThread();

	// force Ticks to stop
	bTickable = false;
}


void FSplash::Shutdown()
{
	CheckInGameThread();

	if (bInitialized)
	{
		bTickable = false;

		ExecuteOnRenderThread([this]()
		{
			Ticker->Unregister();
			Ticker = nullptr;

			ExecuteOnRHIThread([this]()
			{
				SplashLayers.Reset();
				Layers_RenderThread.Reset();
				Layers_RHIThread.Reset();
			});
		});

		FCoreUObjectDelegates::PreLoadMap.RemoveAll(this);
		FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);

		ShowFlags = 0;
		bIsBlack = false;
		bLoadingCompleted = false;
		bLoadingStarted = false;
		bInitialized = false;
	}
}


void FSplash::OnLoadingBegins()
{
	CheckInGameThread();

	if (bAutoShow)
	{
		UE_LOG(LogLoadingSplash, Log, TEXT("Loading begins"));
		bLoadingStarted = true;
		bLoadingCompleted = false;
		Show(ShowAutomatically);
	}
}


void FSplash::OnLoadingEnds()
{
	CheckInGameThread();

	if (bAutoShow)
	{
		UE_LOG(LogLoadingSplash, Log, TEXT("Loading ends"));
		bLoadingStarted = false;
		bLoadingCompleted = true;
		Hide(ShowAutomatically);
	}
}


bool FSplash::AddSplash(const FOculusSplashDesc& Desc)
{
	CheckInGameThread();

	FScopeLock ScopeLock(&RenderThreadLock);
	SplashLayers.Add(FSplashLayer(Desc));
	return true;
}


void FSplash::ClearSplashes()
{
	CheckInGameThread();

	FScopeLock ScopeLock(&RenderThreadLock);
	SplashLayers.Reset();
}


bool FSplash::GetSplash(unsigned InSplashLayerIndex, FOculusSplashDesc& OutDesc)
{
	CheckInGameThread();

	FScopeLock ScopeLock(&RenderThreadLock);
	if (InSplashLayerIndex < unsigned(SplashLayers.Num()))
	{
		OutDesc = SplashLayers[int32(InSplashLayerIndex)].Desc;
		return true;
	}
	return false;
}


void FSplash::Show(uint32 InShowFlags)
{
	CheckInGameThread();

	uint32 OldShowFlags = ShowFlags;
	ShowFlags |= InShowFlags;

	if (ShowFlags && !OldShowFlags)
	{
		OnShow();
	}
}


void FSplash::Hide(uint32 InShowFlags)
{
	CheckInGameThread();

	uint32 NewShowFlags = ShowFlags;
	NewShowFlags &= ~InShowFlags;

	if (!NewShowFlags && ShowFlags)
	{
		OnHide();
	}

	ShowFlags = NewShowFlags;
}


void FSplash::OnShow()
{
	CheckInGameThread();

	// Create new textures
	{
		FScopeLock ScopeLock(&RenderThreadLock);
			
		UnloadTextures();

		// Make sure all UTextures are loaded and contain Resource->TextureRHI
		bool bWaitForRT = false;

		for (int32 SplashLayerIndex = 0; SplashLayerIndex < SplashLayers.Num(); ++SplashLayerIndex)
		{
			FSplashLayer& SplashLayer = SplashLayers[SplashLayerIndex];

			if (SplashLayer.Desc.TexturePath.IsValid())
			{
				// load temporary texture (if TexturePath was specified)
				LoadTexture(SplashLayer);
			}
			if (SplashLayer.Desc.LoadingTexture && SplashLayer.Desc.LoadingTexture->IsValidLowLevel())
			{
				SplashLayer.Desc.LoadingTexture->UpdateResource();
				bWaitForRT = true;
			}
		}

		if (bWaitForRT)
		{
			FlushRenderingCommands();
		}

		bIsBlack = true;

		for (int32 SplashLayerIndex = 0; SplashLayerIndex < SplashLayers.Num(); ++SplashLayerIndex)
		{
			FSplashLayer& SplashLayer = SplashLayers[SplashLayerIndex];

			//@DBG BEGIN
			if (SplashLayer.Desc.LoadingTexture->IsValidLowLevel())
			{
				if (SplashLayer.Desc.LoadingTexture->Resource && SplashLayer.Desc.LoadingTexture->Resource->TextureRHI)
				{
					SplashLayer.Desc.LoadedTexture = SplashLayer.Desc.LoadingTexture->Resource->TextureRHI;
				}
				else
				{
					UE_LOG(LogHMD, Warning, TEXT("Splash, %s - no Resource"), *SplashLayer.Desc.LoadingTexture->GetDesc());
				}
			}
			//@DBG END

			if (SplashLayer.Desc.LoadedTexture)
			{
				IStereoLayers::FLayerDesc LayerDesc;
				LayerDesc.Transform = SplashLayer.Desc.TransformInMeters;
				LayerDesc.QuadSize = SplashLayer.Desc.QuadSizeInMeters;
				LayerDesc.UVRect = FBox2D(SplashLayer.Desc.TextureOffset, SplashLayer.Desc.TextureOffset + SplashLayer.Desc.TextureScale);
				LayerDesc.Priority = INT32_MAX - (int32) (SplashLayer.Desc.TransformInMeters.GetTranslation().X * 1000.f);
				LayerDesc.PositionType = IStereoLayers::TrackerLocked;
				LayerDesc.ShapeType = IStereoLayers::QuadLayer;
				LayerDesc.Texture = SplashLayer.Desc.LoadedTexture;
				LayerDesc.Flags = SplashLayer.Desc.bNoAlphaChannel ? IStereoLayers::LAYER_FLAG_TEX_NO_ALPHA_CHANNEL : 0;

				SplashLayer.Layer = MakeShareable(new FLayer(NextLayerId++, LayerDesc));

				bIsBlack = false;
			}
		}
	}

	// If no textures are loaded, this will push black frame
	if (PushFrame())
	{
		bTickable = true;
	}

	UE_LOG(LogHMD, Log, TEXT("FSplash::OnShow"));
}


void FSplash::OnHide()
{
	CheckInGameThread();

	UE_LOG(LogHMD, Log, TEXT("FSplash::OnHide"));
	bTickable = false;
	bIsBlack = true;
	PushFrame();
	UnloadTextures();

#if PLATFORM_ANDROID
	ExecuteOnRenderThread([this]()
	{
		ExecuteOnRHIThread([this]()
		{
			FAndroidApplication::DetachJavaEnv();
		});
	});
#endif
}


bool FSplash::PushFrame()
{
	CheckInGameThread();

	check(!bTickable);

	if (!OculusHMD->InitDevice())
	{
		return false;
	}

	{
		FScopeLock ScopeLock(&RenderThreadLock);
		Settings = OculusHMD->CreateNewSettings();
		Frame = OculusHMD->CreateNewGameFrame();
		// keep units in meters rather than UU (because UU make not much sense).
		Frame->WorldToMetersScale = 1.0f;

		float SystemDisplayFrequency;
		if (OVRP_SUCCESS(ovrp_GetSystemDisplayFrequency2(&SystemDisplayFrequency)))
		{
			SystemDisplayInterval = 1.0f / SystemDisplayFrequency;
		}
	}

	ExecuteOnRenderThread([this](FRHICommandListImmediate& RHICmdList)
	{
		RenderFrame_RenderThread(RHICmdList, FPlatformTime::Seconds());
	});

	return true;
}


void FSplash::UnloadTextures()
{
	CheckInGameThread();

	// unload temporary loaded textures
	FScopeLock ScopeLock(&RenderThreadLock);
	for (int32 SplashLayerIndex = 0; SplashLayerIndex < SplashLayers.Num(); ++SplashLayerIndex)
	{
		if (SplashLayers[SplashLayerIndex].Desc.TexturePath.IsValid())
		{
			UnloadTexture(SplashLayers[SplashLayerIndex]);
		}
	}
}


void FSplash::LoadTexture(FSplashLayer& InSplashLayer)
{
	CheckInGameThread();

	UnloadTexture(InSplashLayer);

	UE_LOG(LogLoadingSplash, Log, TEXT("Loading texture for splash %s..."), *InSplashLayer.Desc.TexturePath.GetAssetName());
	InSplashLayer.Desc.LoadingTexture = Cast<UTexture2D>(InSplashLayer.Desc.TexturePath.TryLoad());
	if (InSplashLayer.Desc.LoadingTexture != nullptr)
	{
		UE_LOG(LogLoadingSplash, Log, TEXT("...Success. "));
	}
	InSplashLayer.Desc.LoadedTexture = nullptr;
	InSplashLayer.Layer.Reset();
}


void FSplash::UnloadTexture(FSplashLayer& InSplashLayer)
{
	CheckInGameThread();

	if (InSplashLayer.Desc.LoadingTexture && InSplashLayer.Desc.LoadingTexture->IsValidLowLevel())
	{
		InSplashLayer.Desc.LoadingTexture = nullptr;
	}

	InSplashLayer.Desc.LoadedTexture = nullptr;
	InSplashLayer.Layer.Reset();
}


} // namespace OculusHMD

#endif // OCULUS_HMD_SUPPORTED_PLATFORMS