// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Protocols/CompositionGraphCaptureProtocol.h"
#include "Misc/CommandLine.h"
#include "HAL/IConsoleManager.h"
#include "EngineGlobals.h"
#include "Engine/Scene.h"
#include "Materials/MaterialInterface.h"
#include "SceneView.h"
#include "Engine/Engine.h"
#include "SceneViewExtension.h"
#include "Materials/Material.h"
#include "BufferVisualizationData.h"
#include "MovieSceneCaptureSettings.h"

struct FFrameCaptureViewExtension : public FSceneViewExtensionBase
{
	FFrameCaptureViewExtension( const FAutoRegister& AutoRegister, const TArray<FString>& InRenderPasses, bool bInCaptureFramesInHDR, int32 InHDRCompressionQuality, int32 InCaptureGamut, UMaterialInterface* InPostProcessingMaterial)
		: FSceneViewExtensionBase(AutoRegister)
		, RenderPasses(InRenderPasses)
		, bNeedsCapture(true)
		, bCaptureFramesInHDR(bInCaptureFramesInHDR)
		, HDRCompressionQuality(InHDRCompressionQuality)
		, CaptureGamut(InCaptureGamut)
		, PostProcessingMaterial(InPostProcessingMaterial)
		, RestoreDumpHDR(0)
		, RestoreHDRCompressionQuality(0)
		, RestoreDumpGamut(HCGM_Rec709)
	{
		CVarDumpFrames = IConsoleManager::Get().FindConsoleVariable(TEXT("r.BufferVisualizationDumpFrames"));
		CVarDumpFramesAsHDR = IConsoleManager::Get().FindConsoleVariable(TEXT("r.BufferVisualizationDumpFramesAsHDR"));
		CVarHDRCompressionQuality = IConsoleManager::Get().FindConsoleVariable(TEXT("r.SaveEXR.CompressionQuality"));
		CVarDumpGamut = IConsoleManager::Get().FindConsoleVariable(TEXT("r.HDR.Display.ColorGamut"));

		Disable();
	}

	virtual ~FFrameCaptureViewExtension()
	{
		Disable();
	}

	bool IsEnabled() const
	{
		return bNeedsCapture;
	}

	void Enable(FString&& InFilename)
	{
		OutputFilename = MoveTemp(InFilename);

		bNeedsCapture = true;

		RestoreDumpHDR = CVarDumpFramesAsHDR->GetInt();
		RestoreHDRCompressionQuality = CVarHDRCompressionQuality->GetInt();
		RestoreDumpGamut = CVarDumpGamut->GetInt();

		CVarDumpFramesAsHDR->Set(bCaptureFramesInHDR);
		CVarHDRCompressionQuality->Set(HDRCompressionQuality);
		CVarDumpGamut->Set(CaptureGamut);
		CVarDumpFrames->Set(1);
	}

	void Disable(bool bFinalize = false)
	{
		if (bNeedsCapture || bFinalize)
		{
			bNeedsCapture = false;
			if (bFinalize)
			{
				RestoreDumpHDR = 0;
				RestoreHDRCompressionQuality = 0;
			}
			CVarDumpFramesAsHDR->Set(RestoreDumpHDR);
			CVarHDRCompressionQuality->Set(RestoreHDRCompressionQuality);
			CVarDumpGamut->Set(RestoreDumpGamut);
			CVarDumpFrames->Set(0);
		}
	}

	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
	{
		if (!bNeedsCapture)
		{
			return;
		}

		InView.FinalPostProcessSettings.bBufferVisualizationDumpRequired = true;
		InView.FinalPostProcessSettings.BufferVisualizationOverviewMaterials.Empty();
		InView.FinalPostProcessSettings.BufferVisualizationDumpBaseFilename = MoveTemp(OutputFilename);

		struct FIterator
		{
			FFinalPostProcessSettings& FinalPostProcessSettings;
			const TArray<FString>& RenderPasses;

			FIterator(FFinalPostProcessSettings& InFinalPostProcessSettings, const TArray<FString>& InRenderPasses)
				: FinalPostProcessSettings(InFinalPostProcessSettings), RenderPasses(InRenderPasses)
			{}

			void ProcessValue(const FString& InName, UMaterial* Material, const FText& InText)
			{
				if (!RenderPasses.Num() || RenderPasses.Contains(InName) || RenderPasses.Contains(InText.ToString()))
				{
					FinalPostProcessSettings.BufferVisualizationOverviewMaterials.Add(Material);
				}
			}
		} Iterator(InView.FinalPostProcessSettings, RenderPasses);
		GetBufferVisualizationData().IterateOverAvailableMaterials(Iterator);

		if (PostProcessingMaterial)
		{
			FWeightedBlendable Blendable(1.f, PostProcessingMaterial);
			PostProcessingMaterial->OverrideBlendableSettings(InView, 1.f);
		}


		// Ensure we're rendering at full size
		InView.ViewRect = InView.UnscaledViewRect;

		bNeedsCapture = false;
	}

	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) {}
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) {}
	virtual void PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily) {}
	virtual void PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) {}

	virtual bool IsActiveThisFrame(class FViewport* InViewport) const override { return IsEnabled(); }

private:
	const TArray<FString>& RenderPasses;

	bool bNeedsCapture;
	FString OutputFilename;

	bool bCaptureFramesInHDR;
	int32 HDRCompressionQuality;
	int32 CaptureGamut;

	UMaterialInterface* PostProcessingMaterial;

	IConsoleVariable* CVarDumpFrames;
	IConsoleVariable* CVarDumpFramesAsHDR;
	IConsoleVariable* CVarHDRCompressionQuality;
	IConsoleVariable* CVarDumpGamut;

	int32 RestoreDumpHDR;
	int32 RestoreHDRCompressionQuality;
	int32 RestoreDumpGamut;
};

void UCompositionGraphCaptureSettings::OnReleaseConfig(FMovieSceneCaptureSettings& InSettings)
{
	// Remove {material} if it exists
	InSettings.OutputFormat = InSettings.OutputFormat.Replace(TEXT("{material}"), TEXT(""));

	// Remove .{frame} if it exists
	InSettings.OutputFormat = InSettings.OutputFormat.Replace(TEXT(".{frame}"), TEXT(""));

	Super::OnReleaseConfig(InSettings);
}

void UCompositionGraphCaptureSettings::OnLoadConfig(FMovieSceneCaptureSettings& InSettings)
{
	// Add .{frame} if it doesn't already exist
	FString OutputFormat = InSettings.OutputFormat;

	if (!OutputFormat.Contains(TEXT("{frame}")))
	{
		OutputFormat.Append(TEXT(".{frame}"));

		InSettings.OutputFormat = OutputFormat;
	}

	// Add {material} if it doesn't already exist
	if (!OutputFormat.Contains(TEXT("{material}")))
	{
		int32 FramePosition = OutputFormat.Find(TEXT(".{frame}"));
		if (FramePosition != INDEX_NONE)
		{
			OutputFormat.InsertAt(FramePosition, TEXT("{material}"));
		}
		else
		{
			OutputFormat.Append(TEXT("{material}"));
		}

		InSettings.OutputFormat = OutputFormat;
	}

	Super::OnLoadConfig(InSettings);
}

bool FCompositionGraphCaptureProtocol::Initialize(const FCaptureProtocolInitSettings& InSettings, const ICaptureProtocolHost& Host)
{
	SceneViewport = InSettings.SceneViewport;

	bool bCaptureFramesInHDR = false;
	int32 HDRCompressionQuality = 0;
	int32 CaptureGamut = HCGM_Rec709;

	UMaterialInterface* PostProcessingMaterial = nullptr;
	UCompositionGraphCaptureSettings* ProtocolSettings = CastChecked<UCompositionGraphCaptureSettings>(InSettings.ProtocolSettings);
	if (ProtocolSettings)
	{
		RenderPasses = ProtocolSettings->IncludeRenderPasses.Value;
		bCaptureFramesInHDR = ProtocolSettings->bCaptureFramesInHDR;
		HDRCompressionQuality = ProtocolSettings->HDRCompressionQuality;
		CaptureGamut = ProtocolSettings->CaptureGamut;
		PostProcessingMaterial = Cast<UMaterialInterface>(ProtocolSettings->PostProcessingMaterial.TryLoad());

		FString OverrideRenderPasses;
		if (FParse::Value(FCommandLine::Get(), TEXT("-CustomRenderPasses="), OverrideRenderPasses))
		{
			OverrideRenderPasses.ParseIntoArray(RenderPasses, TEXT(","), true);
		}

		bool bOverrideCaptureFramesInHDR;
		if (FParse::Bool(FCommandLine::Get(), TEXT("-CaptureFramesInHDR="), bOverrideCaptureFramesInHDR))
		{
			bCaptureFramesInHDR = bOverrideCaptureFramesInHDR;
		}

		int32 OverrideHDRCompressionQuality;
		if( FParse::Value( FCommandLine::Get(), TEXT( "-HDRCompressionQuality=" ), OverrideHDRCompressionQuality ) )
		{
			HDRCompressionQuality = OverrideHDRCompressionQuality;
		}

		int32 OverrideCaptureGamut;
		if (FParse::Value(FCommandLine::Get(), TEXT("-CaptureGamut="), OverrideCaptureGamut))
		{
			CaptureGamut = OverrideCaptureGamut;
		}
	}

	ViewExtension = FSceneViewExtensions::NewExtension<FFrameCaptureViewExtension>(RenderPasses, bCaptureFramesInHDR, HDRCompressionQuality, CaptureGamut, PostProcessingMaterial);

	return true;
}

void FCompositionGraphCaptureProtocol::Finalize()
{
	ViewExtension->Disable(true);
}

void FCompositionGraphCaptureProtocol::CaptureFrame(const FFrameMetrics& FrameMetrics, const ICaptureProtocolHost& Host)
{
	ViewExtension->Enable(Host.GenerateFilename(FrameMetrics, TEXT("")));
}

bool FCompositionGraphCaptureProtocol::HasFinishedProcessing() const
{
	return !ViewExtension->IsEnabled();
}

void FCompositionGraphCaptureProtocol::Tick()
{
	if (!ViewExtension->IsEnabled())
	{
		ViewExtension->Disable();
	}
}
