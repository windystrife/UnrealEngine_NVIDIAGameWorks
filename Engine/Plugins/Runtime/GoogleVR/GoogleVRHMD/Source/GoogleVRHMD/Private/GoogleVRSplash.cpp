// Copyright 2017 Google Inc.

#include "GoogleVRSplash.h"
#include "PipelineStateCache.h"
#include "ClearQuad.h"

#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
#include "GoogleVRHMD.h"
#include "ScreenRendering.h"

static constexpr gvr_mat4f GVRHeadPoseIdentity = {{{1.0f, 0.0f, 0.0f, 0.0f},
											       {0.0f, 1.0f, 0.0f, 0.0f},
											       {0.0f, 0.0f, 1.0f, 0.0f},
											       {0.0f, 0.0f, 0.0f, 1.0f}}};

FGoogleVRSplash::FGoogleVRSplash(FGoogleVRHMD* InGVRHMD)
	: bEnableSplashScreen(true)
	, SplashTexture(nullptr)
	, RenderDistanceInMeter(2.0f)
	, RenderScale(1.0f)
	, ViewAngleInDegree(180.0f)
	, GVRHMD(InGVRHMD)
	, RendererModule(nullptr)
	, GVRCustomPresent(nullptr)
	, bInitialized(false)
	, bIsShown(false)
	, bSplashScreenRendered(false)
{
	// Make sure we have a valid GoogleVRHMD plugin;
	check(GVRHMD && GVRHMD->CustomPresent);

	RendererModule = GVRHMD->RendererModule;
	GVRCustomPresent = GVRHMD->CustomPresent;
}

FGoogleVRSplash::~FGoogleVRSplash()
{
	Hide();

	if (bInitialized)
	{
		FCoreUObjectDelegates::PreLoadMap.RemoveAll(this);
		FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
	}
}

void FGoogleVRSplash::Init()
{
	if (!bInitialized)
	{
		FCoreUObjectDelegates::PreLoadMap.AddSP(this, &FGoogleVRSplash::OnPreLoadMap);
		FCoreUObjectDelegates::PostLoadMapWithWorld.AddSP(this, &FGoogleVRSplash::OnPostLoadMap);

		LoadDefaultSplashTexturePath();
		bInitialized = true;
	}
}

void FGoogleVRSplash::OnPreLoadMap(const FString&)
{
	Show();
}

void FGoogleVRSplash::OnPostLoadMap(UWorld*)
{
	Hide();
}

void FGoogleVRSplash::AllocateSplashScreenRenderTarget()
{
	if (!GVRCustomPresent->TextureSet.IsValid())
	{
		const uint32 NumLayers = (GVRHMD->IsMobileMultiViewDirect()) ? 2 : 1;
		GVRCustomPresent->AllocateRenderTargetTexture(0, GVRHMD->GVRRenderTargetSize.X, GVRHMD->GVRRenderTargetSize.Y, PF_B8G8R8A8, NumLayers, 1, TexCreate_None, TexCreate_RenderTargetable);
	}
}

void FGoogleVRSplash::Show()
{
	check(IsInGameThread());

	if (!bEnableSplashScreen || bIsShown)
	{
		return;
	}

	// Load the splash screen texture if it is specified from the path.
	if (!SplashTexturePath.IsEmpty())
	{
		LoadTexture();
	}

	bSplashScreenRendered = false;

	GVRHMD->UpdateGVRViewportList();

	// Render the splash screen in the front direction in start space
	// In this case, recenter will always put the splash screen in front of the user.
	SplashScreenRenderingHeadPose = GVRHeadPoseIdentity;
	SplashScreenRenderingOrientation = FRotator(0.0f, 0.0f, 0.0f);

	// Uncomment the following code if you want to put the splash screen using the current
	// head pose.
	// Be aware, if you do this, when user recenter the hmd using the controller when
	// the splash screen displaying, the splash screen may not be in front of the user.
	// You probably want to call ForceRerenderSplashScreen to rerender the splash screen after
	// the recentering happens.
	//GVRHMD->UpdateHeadPose();
	//SplashScreenRenderingHeadPose = GVRHMD->CachedHeadPose;
	//SplashScreenRenderingOrientation = FRotator(GVRHMD->CachedFinalHeadRotation);

	RenderThreadTicker = MakeShareable(new FGoogleVRSplashTicker(this));
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(RegisterAsyncTick,
	FTickableObjectRenderThread*, RenderThreadTicker, RenderThreadTicker.Get(),
	FGoogleVRSplash*, pGVRSplash, this,
	{
		pGVRSplash->AllocateSplashScreenRenderTarget();
		RenderThreadTicker->Register();
	});

	bIsShown = true;
}

void FGoogleVRSplash::Hide()
{
	check(IsInGameThread());

	if (!bIsShown)
	{
		return;
	}

	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(UnregisterAsyncTick,
	TSharedPtr<FGoogleVRSplashTicker>&, RenderThreadTicker, RenderThreadTicker,
	FGoogleVRSplash*, pGVRSplash, this,
	{
		pGVRSplash->SubmitBlackFrame();

		RenderThreadTicker->Unregister();
		RenderThreadTicker = nullptr;
	});
	FlushRenderingCommands();

	if (!SplashTexturePath.IsEmpty())
	{
		UnloadTexture();
	}

	bIsShown = false;
}

void FGoogleVRSplash::Tick(float DeltaTime)
{
	check(IsInRenderingThread());

	if (!bSplashScreenRendered)
	{
		RenderStereoSplashScreen(FRHICommandListExecutor::GetImmediateCommandList(), GVRCustomPresent->TextureSet->GetTexture2D());
		bSplashScreenRendered = true;
	}
}

bool FGoogleVRSplash::IsTickable() const
{
	return bIsShown;
}

void FGoogleVRSplash::RenderStereoSplashScreen(FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef DstTexture)
{
	UpdateSplashScreenEyeOffset();

	check(IsInRenderingThread());

	// Make sure we have a valid render target
	check(GVRCustomPresent->TextureSet.IsValid());

	// Bind GVR RenderTarget
	GVRCustomPresent->BeginRendering(SplashScreenRenderingHeadPose);

	const uint32 ViewportWidth = DstTexture->GetSizeX();
	const uint32 ViewportHeight = DstTexture->GetSizeY();

	FIntRect DstRect = FIntRect(0, 0, ViewportWidth, ViewportHeight);

	const auto FeatureLevel = GMaxRHIFeatureLevel;
	// Should really be SetRT and Clear
	SetRenderTarget(RHICmdList, DstTexture, FTextureRHIRef());
	DrawClearQuad(RHICmdList, FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));

	// If the texture is not avaliable, we just clear the DstTexture with black.
	if (SplashTexture && SplashTexture->IsValidLowLevel())
	{
		RHICmdList.SetViewport(DstRect.Min.X, DstRect.Min.Y, 0, DstRect.Max.X, DstRect.Max.Y, 1.0f);

		auto ShaderMap = GetGlobalShaderMap(FeatureLevel);

		TShaderMapRef<FScreenVS> VertexShader(ShaderMap);
		TShaderMapRef<FScreenPS> PixelShader(ShaderMap);

		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = RendererModule->GetFilterVertexDeclaration().VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

		PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Bilinear>::GetRHI(), SplashTexture->Resource->TextureRHI);

		// Modify the v to flip the texture. Somehow it renders flipped if not.
		float U = SplashTextureUVOffset.X;
		float V = SplashTextureUVOffset.Y + SplashTextureUVSize.Y;
		float USize = SplashTextureUVSize.X;
		float VSize = -SplashTextureUVSize.Y;

		float ViewportWidthPerEye = ViewportWidth * 0.5f;

		float RenderOffsetX = SplashScreenEyeOffset.X * ViewportWidthPerEye + (1.0f - RenderScale) * ViewportWidthPerEye * 0.5f;
		float RenderOffsetY = SplashScreenEyeOffset.Y * ViewportHeight + (1.0f - RenderScale) * ViewportHeight * 0.5f;;
		float RenderSizeX = ViewportWidthPerEye * RenderScale;
		float RenderSizeY = ViewportHeight * RenderScale;

		// Render left eye texture
		RendererModule->DrawRectangle(
			RHICmdList,
			RenderOffsetX , RenderOffsetY,
			RenderSizeX, RenderSizeY,
			U, V,
			USize, VSize,
			FIntPoint(ViewportWidth, ViewportHeight),
			FIntPoint(1, 1),
			*VertexShader,
			EDRF_Default);

		RenderOffsetX = -SplashScreenEyeOffset.X * ViewportWidthPerEye + (1.0f - RenderScale) * ViewportWidthPerEye * 0.5f;

		// In case the right eye offset is excceed the borader
		if (RenderOffsetX < 0)
		{
			U = U - RenderOffsetX / RenderSizeX;
			RenderOffsetX = 0;
		}

		// Render right eye texture
		RendererModule->DrawRectangle(
			RHICmdList,
			ViewportWidthPerEye + RenderOffsetX, RenderOffsetY,
			RenderSizeX, RenderSizeY,
			U, V,
			USize, VSize,
			FIntPoint(ViewportWidth, ViewportHeight),
			FIntPoint(1, 1),
			*VertexShader,
			EDRF_Default);
	}

	// Submit frame to async reprojection
	GVRCustomPresent->FinishRendering();
}

void FGoogleVRSplash::ForceRerenderSplashScreen()
{
	bSplashScreenRendered = false;
}

void FGoogleVRSplash::SubmitBlackFrame()
{
	check(IsInRenderingThread());

	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	FTexture2DRHIParamRef DstTexture = GVRCustomPresent->TextureSet->GetTexture2D();

	GVRCustomPresent->BeginRendering(GVRHMD->CachedHeadPose);

	SetRenderTarget(RHICmdList, DstTexture, FTextureRHIRef());
	DrawClearQuad(RHICmdList, FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));

	GVRCustomPresent->FinishRendering();
}

bool FGoogleVRSplash::LoadDefaultSplashTexturePath()
{
	// Default Setting for Daydream splash screen
	SplashTexturePath = "";
	SplashTextureUVOffset = FVector2D(0.0f, 0.0f);
	SplashTextureUVSize = FVector2D(1.0f, 1.0f);
	RenderDistanceInMeter = 2.0f;
	RenderScale = 1.0f;
	ViewAngleInDegree = 180.0f;

	const TCHAR* SplashSettings = TEXT("Daydream.Splash.Settings");
	GConfig->GetString(SplashSettings, *FString("TexturePath"), SplashTexturePath, GEngineIni);
	GConfig->GetVector2D(SplashSettings, *FString("TextureUVOffset"), SplashTextureUVOffset, GEngineIni);
	GConfig->GetVector2D(SplashSettings, *FString("TextureUVSize"), SplashTextureUVSize, GEngineIni);
	GConfig->GetFloat(SplashSettings, *FString("RenderDistanceInMeter"), RenderDistanceInMeter, GEngineIni);
	GConfig->GetFloat(SplashSettings, *FString("RenderScale"), RenderScale, GEngineIni);
	GConfig->GetFloat(SplashSettings, *FString("ViewAngleInDegree"), ViewAngleInDegree, GEngineIni);

	UE_LOG(LogHMD, Log, TEXT("Daydream Splash Screen Settings:"), *SplashTexturePath);
	UE_LOG(LogHMD, Log, TEXT("TexturePath:%s"), *SplashTexturePath);
	UE_LOG(LogHMD, Log, TEXT("TextureUVOffset: (%f, %f)"), SplashTextureUVOffset.X, SplashTextureUVOffset.Y);
	UE_LOG(LogHMD, Log, TEXT("TextureUVSize: (%f, %f)"), SplashTextureUVSize.X, SplashTextureUVSize.Y);
	UE_LOG(LogHMD, Log, TEXT("RenderDistance: %f"), RenderDistanceInMeter);
	UE_LOG(LogHMD, Log, TEXT("RenderScale: %f"), RenderScale);
	UE_LOG(LogHMD, Log, TEXT("ViewAngleInDegree: %f"), ViewAngleInDegree);

	return true;
}

bool FGoogleVRSplash::LoadTexture()
{
	SplashTexture = LoadObject<UTexture2D>(nullptr, *SplashTexturePath, nullptr, LOAD_None, nullptr);
	if (SplashTexture && SplashTexture->IsValidLowLevel())
	{
		SplashTexture->AddToRoot();
		UE_LOG(LogHMD, Log, TEXT("Splash Texture load successful!"));
		SplashTexture->AddToRoot();
		SplashTexture->UpdateResource();
		FlushRenderingCommands();
	}
	else
	{
		UE_LOG(LogHMD, Warning, TEXT("Failed to load the Splash Texture at path %s"), *SplashTexturePath);
	}

	return SplashTexture != nullptr;
}

void FGoogleVRSplash::UnloadTexture()
{
	if (SplashTexture && SplashTexture->IsValidLowLevel())
	{
		SplashTexture->RemoveFromRoot();
		SplashTexture = nullptr;
	}
}

void FGoogleVRSplash::UpdateSplashScreenEyeOffset()
{
	// Get eye offset from GVR
	float WorldToMeterScale = GVRHMD->GetWorldToMetersScale();
	float HalfEyeDistance = GVRHMD->GetInterpupillaryDistance() * 0.5f;
	float Depth = RenderDistanceInMeter * WorldToMeterScale;
	// Get eye Fov from GVR
	gvr_rectf LeftEyeFOV = GVRHMD->GetGVREyeFOV(0);

	// Have to flip left/right and top/bottom to match UE4 expectations
	float LeftTan = FPlatformMath::Tan(FMath::DegreesToRadians(LeftEyeFOV.left));
	float RightTan = FPlatformMath::Tan(FMath::DegreesToRadians(LeftEyeFOV.right));
	float TopTan = FPlatformMath::Tan(FMath::DegreesToRadians(LeftEyeFOV.top));
	float BottomTan = FPlatformMath::Tan(FMath::DegreesToRadians(LeftEyeFOV.bottom));

	float SumLR = LeftTan + RightTan;
	float SubLR = RightTan - LeftTan;
	float SumTB = TopTan + BottomTan;
	float SubTB = BottomTan - TopTan;

	// This calculate the offset to the center of the left eye area.
	SplashScreenEyeOffset.X = HalfEyeDistance / SumLR / Depth - SubLR / SumLR * 0.5f;
	SplashScreenEyeOffset.Y = SubTB / SumTB * 0.5f;
}
#endif //GOOGLEVRHMD_SUPPORTED_PLATFORMS
