// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessing.cpp: The center for all post processing activities.
=============================================================================*/

#include "PostProcess/PostProcessing.h"
#include "EngineGlobals.h"
#include "ScenePrivate.h"
#include "RendererModule.h"
#include "PostProcess/PostProcessInput.h"
#include "PostProcess/PostProcessAA.h"
#if WITH_EDITOR
	#include "PostProcess/PostProcessBufferInspector.h"
#endif
#include "PostProcess/PostProcessMaterial.h"
#include "PostProcess/PostProcessWeightedSampleSum.h"
#include "PostProcess/PostProcessBloomSetup.h"
#include "PostProcess/PostProcessMobile.h"
#include "PostProcess/PostProcessDownsample.h"
#include "PostProcess/PostProcessHistogram.h"
#include "PostProcess/PostProcessHistogramReduce.h"
#include "PostProcess/PostProcessVisualizeHDR.h"
#include "PostProcess/VisualizeShadingModels.h"
#include "PostProcess/PostProcessSelectionOutline.h"
#include "PostProcess/PostProcessGBufferHints.h"
#include "PostProcess/PostProcessVisualizeBuffer.h"
#include "PostProcess/PostProcessEyeAdaptation.h"
#include "PostProcess/PostProcessTonemap.h"
#include "PostProcess/PostProcessLensFlares.h"
#include "PostProcess/PostProcessLensBlur.h"
#include "PostProcess/PostProcessBokehDOF.h"
#include "PostProcess/PostProcessBokehDOFRecombine.h"
#include "PostProcess/PostProcessCombineLUTs.h"
#include "PostProcess/PostProcessTemporalAA.h"
#include "PostProcess/PostProcessMotionBlur.h"
#include "PostProcess/PostProcessDOF.h"
#include "PostProcess/PostProcessCircleDOF.h"
#include "PostProcess/PostProcessUpscale.h"
#include "PostProcess/PostProcessHMD.h"
#include "PostProcess/PostProcessVisualizeComplexity.h"
#include "PostProcess/PostProcessCompositeEditorPrimitives.h"
#include "CompositionLighting/PostProcessPassThrough.h"
#include "PostProcess/PostProcessTestImage.h"
#include "HighResScreenshot.h"
#include "PostProcess/PostProcessSubsurface.h"
#include "PostProcess/PostProcessMorpheus.h"
#include "IHeadMountedDisplay.h"
#include "IXRTrackingSystem.h"
#include "BufferVisualizationData.h"
#include "CompositionLighting/PostProcessLpvIndirect.h"
#include "PostProcess/PostProcessStreamingAccuracyLegend.h"
#include "DeferredShadingRenderer.h"
#include "PostProcess/PostProcessFFTBloom.h"
#include "MobileSeparateTranslucencyPass.h"

/** The global center for all post processing activities. */
FPostProcessing GPostProcessing;

static TAutoConsoleVariable<int32> CVarUseMobileBloom(
	TEXT("r.UseMobileBloom"),
	0,
	TEXT("HACK: Set to 1 to use mobile bloom."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarDepthOfFieldNearBlurSizeThreshold(
	TEXT("r.DepthOfField.NearBlurSizeThreshold"),
	0.01f,
	TEXT("Sets the minimum near blur size before the effect is forcably disabled. Currently only affects Gaussian DOF.\n")
	TEXT(" (default: 0.01)"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarDepthOfFieldMaxSize(
	TEXT("r.DepthOfField.MaxSize"),
	100.0f,
	TEXT("Allows to clamp the gaussian depth of field radius (for better performance), default: 100"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarRenderTargetSwitchWorkaround(
	TEXT("r.RenderTargetSwitchWorkaround"),
	0,
	TEXT("Workaround needed on some mobile platforms to avoid a performance drop related to switching render targets.\n")
	TEXT("Only enabled on some hardware. This affects the bloom quality a bit. It runs slower than the normal code path but\n")
	TEXT("still faster as it avoids the many render target switches. (Default: 0)\n")
	TEXT("We want this enabled (1) on all 32 bit iOS devices (implemented through DeviceProfiles)."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarUpscaleQuality(
	TEXT("r.Upscale.Quality"),
	3,
	TEXT("Defines the quality in which ScreenPercentage and WindowedFullscreen scales the 3d rendering.\n")
	TEXT(" 0: Nearest filtering\n")
	TEXT(" 1: Simple Bilinear\n")
	TEXT(" 2: Directional blur with unsharp mask upsample.\n")
	TEXT(" 3: 5-tap Catmull-Rom bicubic, approximating Lanczos 2. (default)\n")
	TEXT(" 4: 13-tap Lanczos 3.\n")
	TEXT(" 5: 36-tap Gaussian-filtered unsharp mask (very expensive, but good for extreme upsampling).\n"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CDownsampleQuality(
	TEXT("r.Downsample.Quality"),
	3,
	TEXT("Defines the quality in which the Downsample passes. we might add more quality levels later.\n")
	TEXT(" 0: low quality\n")
	TEXT(">0: high quality (default: 3)\n"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarMotionBlurSoftEdgeSize(
	TEXT("r.MotionBlurSoftEdgeSize"),
	1.0f,
	TEXT("Defines how wide the object motion blur is blurred (percent of screen width) to allow soft edge motion blur.\n")
	TEXT("This scales linearly with the size (up to a maximum of 32 samples, 2.5 is about 18 samples) and with screen resolution\n")
	TEXT("Smaller values are better for performance and provide more accurate motion vectors but the blurring outside the object is reduced.\n")
	TEXT("If needed this can be exposed like the other motionblur settings.\n")
	TEXT(" 0:off (not free and does never completely disable), >0, 1.0 (default)"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarBloomCross(
	TEXT("r.Bloom.Cross"),
	0.0f,
	TEXT("Experimental feature to give bloom kernel a more bright center sample (values between 1 and 3 work without causing aliasing)\n")
	TEXT("Existing bloom get lowered to match the same brightness\n")
	TEXT("<0 for a anisomorphic lens flare look (X only)\n")
	TEXT(" 0 off (default)\n")
	TEXT(">0 for a cross look (X and Y)"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarTonemapperMergeMode(
	TEXT("r.Tonemapper.MergeWithUpscale.Mode"),
	0,
	TEXT("ScreenPercentage upscale integrated into tonemapper pass (if certain conditions apply, e.g., no FXAA)\n")
	TEXT(" if enabled both features are done in one pass (faster, affects post process passes after the tonemapper including material post process e.g. sharpen)\n")
	TEXT("  0: off, the features run in separate passes (default)\n")
	TEXT("  1: always enabled, try to merge the passes unless something makes it impossible\n")
	TEXT("  2: merge when the ratio of areas is above the r.Tonemapper.MergeWithUpscale.Threshold and it is otherwise possible"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarTonemapperMergeThreshold(
	TEXT("r.Tonemapper.MergeWithUpscale.Threshold"),
	0.49f,
	TEXT("If r.Tonemapper.MergeWithUpscale.Mode is 2, the ratio of the area before upscale/downscale to the area afterwards\n")
	TEXT("is compared to this threshold when deciding whether or not to merge the passes.  The reasoning is that if the ratio\n")
	TEXT("is too low, running the tonemapper on the higher number of pixels is more expensive than doing two passes\n")
	TEXT("\n")
	TEXT("Defauls to 0.49 (e.g., if r.ScreenPercentage is 70 or higher, try to merge)"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarMotionBlurScatter(
	TEXT("r.MotionBlurScatter"),
	0,
	TEXT("Forces scatter based max velocity method (slower)."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarMotionBlurSeparable(
	TEXT("r.MotionBlurSeparable"),
	0,
	TEXT("Adds a second motion blur pass that smooths noise for a higher quality blur."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarAlphaChannel(
	TEXT("r.PostProcessing.PropagateAlpha"),
	0,
	TEXT("0 to disable scene alpha channel support in the post processing.\n")
	TEXT(" 0: disabled (default)\n")
	TEXT(" 1: enabled"),
	ECVF_ReadOnly);
	
static TAutoConsoleVariable<int32> CVarPostProcessingPreferCompute(
	TEXT("r.PostProcessing.PreferCompute"),
	0,
	TEXT("Will use compute shaders for post processing where implementations available."),
	ECVF_RenderThreadSafe);

#if !(UE_BUILD_SHIPPING)
static TAutoConsoleVariable<int32> CVarPostProcessingForceAsyncDispatch(
	TEXT("r.PostProcessing.ForceAsyncDispatch"),
	0,
	TEXT("Will force asynchronous dispatch for post processing compute shaders where implementations available.\n")
	TEXT("Only available for testing in non-shipping builds."),
	ECVF_RenderThreadSafe);
#endif

TAutoConsoleVariable<int32> CVarHalfResFFTBloom(
	TEXT("r.Bloom.HalfResoluionFFT"),
	0,
	TEXT("Experimental half-resolution FFT Bloom convolution. \n")
	TEXT(" 0: Standard full resolution convolution bloom.")
	TEXT(" 1: Half-resolution convoltuion that excludes the center of the kernel.\n"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

IMPLEMENT_SHADER_TYPE(,FPostProcessVS,TEXT("/Engine/Private/PostProcessBloom.usf"),TEXT("MainPostprocessCommonVS"),SF_Vertex);

static bool HasPostProcessMaterial(FPostprocessContext& Context, EBlendableLocation InLocation);

// -------------------------------------------------------

FPostprocessContext::FPostprocessContext(FRHICommandListImmediate& InRHICmdList, FRenderingCompositionGraph& InGraph, const FViewInfo& InView)
: RHICmdList(InRHICmdList)
	, Graph(InGraph)
	, View(InView)
	, SceneColor(0)
	, SceneDepth(0)
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(InRHICmdList);
	if(SceneContext.IsSceneColorAllocated())
	{
		SceneColor = Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessInput(SceneContext.GetSceneColor()));
	}

	SceneDepth = Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessInput(SceneContext.SceneDepthZ));

	FinalOutput = FRenderingCompositeOutputRef(SceneColor);
}

// Array of downsampled color with optional log2 luminance stored in alpha
template <int32 DownSampleStages>
class TBloomDownSampleArray
{
public:
	// Convenience typedefs
	typedef FRenderingCompositeOutputRef         FRenderingRefArray[DownSampleStages];
	typedef TSharedPtr<TBloomDownSampleArray>    Ptr;

	// Constructor: Generates and registers the downsamples with the Context Graph.
	TBloomDownSampleArray(FPostprocessContext& InContext, FRenderingCompositeOutputRef SourceDownsample, bool bGenerateLog2Alpha) :
		bHasLog2Alpha(bGenerateLog2Alpha), Context(InContext)
	{

		static const TCHAR* PassLabels[] =
		{ NULL, TEXT("BloomDownsample1"), TEXT("BloomDownsample2"), TEXT("BloomDownsample3"), TEXT("BloomDownsample4"), TEXT("BloomDownsample5") };
		static_assert(ARRAY_COUNT(PassLabels) == DownSampleStages, "PassLabel count must be equal to DownSampleStages.");

		// The first down sample is the input
		PostProcessDownsamples[0] = SourceDownsample;

		const bool bIsComputePass = CVarPostProcessingPreferCompute.GetValueOnRenderThread() && Context.View.FeatureLevel >= ERHIFeatureLevel::SM5;

		// Queue the down samples. 
		for (int i = 1; i < DownSampleStages; i++)
		{
			FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessDownsample(PF_Unknown, 1, bIsComputePass, PassLabels[i]));
			Pass->SetInput(ePId_Input0, PostProcessDownsamples[i - 1]);
			PostProcessDownsamples[i] = FRenderingCompositeOutputRef(Pass);

			// Add log2 data to the alpha channel after doing the 1st (i==1) down sample pass
			if (bHasLog2Alpha && i == 1 ) {
				FRenderingCompositePass* BasicEyeSetupPass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessBasicEyeAdaptationSetUp());
				BasicEyeSetupPass->SetInput(ePId_Input0, PostProcessDownsamples[i]);
				PostProcessDownsamples[i] = FRenderingCompositeOutputRef(BasicEyeSetupPass);
			}
		}

		// Calculate the final viewrect size (matching FRCPassPostProcessDownsample behavior)
		FinalViewRectSize.X = FMath::Max(1, FMath::DivideAndRoundUp(InContext.View.ViewRect.Width(), 1 << DownSampleStages));
		FinalViewRectSize.Y = FMath::Max(1, FMath::DivideAndRoundUp(InContext.View.ViewRect.Height(), 1 << DownSampleStages));
	}

	// The number of elements in the array.
	inline static int32 Num() { return DownSampleStages; }

	FIntPoint GetFinalViewRectSize() const
	{
		return FinalViewRectSize;
	}

	// Member data kept public for simplicity
	bool bHasLog2Alpha;
	FPostprocessContext& Context;
	FRenderingRefArray PostProcessDownsamples;

private:
	// no default constructor.
	TBloomDownSampleArray() {};

	FIntPoint	FinalViewRectSize;
};

// Standard DownsampleArray shared by Bloom, Tint, and Eye-Adaptation. 
typedef TBloomDownSampleArray<6/*DownSampleStages*/>   FBloomDownSampleArray;  

FBloomDownSampleArray::Ptr CreateDownSampleArray(FPostprocessContext& Context, FRenderingCompositeOutputRef SourceToDownSample, bool bAddLog2)
{
	return FBloomDownSampleArray::Ptr(new FBloomDownSampleArray(Context, SourceToDownSample, bAddLog2));
}


static FRenderingCompositeOutputRef RenderHalfResBloomThreshold(FPostprocessContext& Context, FRenderingCompositeOutputRef SceneColorHalfRes, FRenderingCompositeOutputRef EyeAdaptation)
{
	// with multiple view ports the Setup pass also isolates the view from the others which allows for simpler simpler/faster blur passes.
	if(Context.View.FinalPostProcessSettings.BloomThreshold <= -1 && Context.View.Family->Views.Num() == 1)
	{
		// no need for threshold, we don't need this pass
		return SceneColorHalfRes;
	}
	else
	{
		// todo: optimize later, the missing node causes some wrong behavior
		//	if(Context.View.FinalPostProcessSettings.BloomIntensity <= 0.0f)
		//	{
		//		// this pass is not required
		//		return FRenderingCompositeOutputRef();
		//	}
		// bloom threshold
		const bool bIsComputePass = CVarPostProcessingPreferCompute.GetValueOnRenderThread() && Context.View.FeatureLevel >= ERHIFeatureLevel::SM5;
		FRenderingCompositePass* PostProcessBloomSetup = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessBloomSetup(bIsComputePass));
		PostProcessBloomSetup->SetInput(ePId_Input0, SceneColorHalfRes);
		PostProcessBloomSetup->SetInput(ePId_Input1, EyeAdaptation);

		return FRenderingCompositeOutputRef(PostProcessBloomSetup);
	}
}


// 2 pass Gaussian blur using uni-linear filtering
// @param CrossCenterWeight see r.Bloom.Cross (positive for X and Y, otherwise for X only)
static FRenderingCompositeOutputRef RenderGaussianBlur(
	FPostprocessContext& Context,
	const TCHAR* DebugNameX,
	const TCHAR* DebugNameY,
	const FRenderingCompositeOutputRef& Input,
	float SizeScale,
	FLinearColor Tint = FLinearColor::White,
	const FRenderingCompositeOutputRef Additive = FRenderingCompositeOutputRef(),
	float CrossCenterWeight = 0.0f)
{
	const bool bIsComputePass = CVarPostProcessingPreferCompute.GetValueOnRenderThread() && Context.View.FeatureLevel >= ERHIFeatureLevel::SM5;

	// Gaussian blur in x
	FRCPassPostProcessWeightedSampleSum* PostProcessBlurX = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessWeightedSampleSum(EFS_Horiz, EFCM_Weighted, SizeScale, bIsComputePass, DebugNameX));
	PostProcessBlurX->SetInput(ePId_Input0, Input);
	if(CrossCenterWeight > 0)
	{
		PostProcessBlurX->SetCrossCenterWeight(CrossCenterWeight);
	}

	// Gaussian blur in y
	FRCPassPostProcessWeightedSampleSum* PostProcessBlurY = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessWeightedSampleSum(EFS_Vert, EFCM_Weighted, SizeScale, bIsComputePass, DebugNameY, Tint));
	PostProcessBlurY->SetInput(ePId_Input0, FRenderingCompositeOutputRef(PostProcessBlurX));
	PostProcessBlurY->SetInput(ePId_Input1, Additive);
	PostProcessBlurY->SetCrossCenterWeight(FMath::Abs(CrossCenterWeight));

	return FRenderingCompositeOutputRef(PostProcessBlurY);
}

// render one bloom pass and add another optional texture to it
static FRenderingCompositeOutputRef RenderBloom(
	FPostprocessContext& Context,
	const FRenderingCompositeOutputRef& PreviousBloom,
	float Size,
	FLinearColor Tint = FLinearColor::White,
	const FRenderingCompositeOutputRef Additive = FRenderingCompositeOutputRef())
{
	const float CrossBloom = CVarBloomCross.GetValueOnRenderThread();

	return RenderGaussianBlur(Context, TEXT("BloomBlurX"), TEXT("BloomBlurY"), PreviousBloom, Size, Tint, Additive,CrossBloom);
}

static FRCPassPostProcessTonemap* AddTonemapper(
	FPostprocessContext& Context,
	const FRenderingCompositeOutputRef& BloomOutputCombined,
	const FRenderingCompositeOutputRef& EyeAdaptation,
	const EAutoExposureMethod& EyeAdapationMethodId,
	const bool bDoGammaOnly,
	const bool bHDRTonemapperOutput)
{
	const FViewInfo& View = Context.View;
	const EStereoscopicPass StereoPass = View.StereoPass;

	const FEngineShowFlags& EngineShowFlags = View.Family->EngineShowFlags;
	const bool bIsComputePass = CVarPostProcessingPreferCompute.GetValueOnRenderThread() && View.FeatureLevel >= ERHIFeatureLevel::SM5;

	FRenderingCompositeOutputRef TonemapperCombinedLUTOutputRef;
	if (StereoPass != eSSP_RIGHT_EYE)
	{
		FRenderingCompositePass* CombinedLUT = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessCombineLUTs(View.GetShaderPlatform(), View.State == nullptr, bIsComputePass));
		TonemapperCombinedLUTOutputRef =  FRenderingCompositeOutputRef(CombinedLUT);
	}

	const bool bDoEyeAdaptation = IsAutoExposureMethodSupported(View.GetFeatureLevel(), EyeAdapationMethodId);
	FRCPassPostProcessTonemap* PostProcessTonemap =	Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessTonemap(View, bDoGammaOnly, bDoEyeAdaptation, bHDRTonemapperOutput, bIsComputePass));

	PostProcessTonemap->SetInput(ePId_Input0, Context.FinalOutput);
	PostProcessTonemap->SetInput(ePId_Input1, BloomOutputCombined);
	PostProcessTonemap->SetInput(ePId_Input2, EyeAdaptation);
	PostProcessTonemap->SetInput(ePId_Input3, TonemapperCombinedLUTOutputRef);

	Context.FinalOutput = FRenderingCompositeOutputRef(PostProcessTonemap);

	return PostProcessTonemap;
}

#if WITH_EDITOR
static void AddSelectionOutline(FPostprocessContext& Context)
{
	FRenderingCompositePass* SelectionColorPass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessSelectionOutlineColor());
	SelectionColorPass->SetInput(ePId_Input0, FRenderingCompositeOutputRef(Context.FinalOutput));

	FRenderingCompositePass* Node = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessSelectionOutline());
	Node->SetInput(ePId_Input0, FRenderingCompositeOutputRef(Context.FinalOutput));
	Node->SetInput(ePId_Input1, FRenderingCompositeOutputRef(FRenderingCompositeOutputRef(SelectionColorPass)));

	Context.FinalOutput = FRenderingCompositeOutputRef(Node);
}
#endif

static void AddGammaOnlyTonemapper(FPostprocessContext& Context)
{
	const bool bIsComputePass = CVarPostProcessingPreferCompute.GetValueOnRenderThread() && Context.View.FeatureLevel >= ERHIFeatureLevel::SM5;
	FRenderingCompositePass* PostProcessTonemap = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessTonemap(Context.View, true, false/*eye*/, false, bIsComputePass));

	PostProcessTonemap->SetInput(ePId_Input0, Context.FinalOutput);

	Context.FinalOutput = FRenderingCompositeOutputRef(PostProcessTonemap);
}

static void AddPostProcessAA(FPostprocessContext& Context)
{
	// console variable override
	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.PostProcessAAQuality")); 

	uint32 Quality = FMath::Clamp(CVar->GetValueOnRenderThread(), 1, 6);

	FRenderingCompositePass* Node = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessAA(Quality));

	Node->SetInput(ePId_Input0, FRenderingCompositeOutputRef(Context.FinalOutput));

	Context.FinalOutput = FRenderingCompositeOutputRef(Node);
}


static FRenderingCompositeOutputRef AddPostProcessBasicEyeAdaptation(const FViewInfo& View, FBloomDownSampleArray& BloomAndEyeDownSamples)
{
	// Extract the context
	FPostprocessContext& Context = BloomAndEyeDownSamples.Context;

	// Extract the last (i.e. smallest) down sample
	static const int32 FinalDSIdx = FBloomDownSampleArray::Num() - 1;
	FRenderingCompositeOutputRef PostProcessPriorReduction = BloomAndEyeDownSamples.PostProcessDownsamples[FinalDSIdx];

	const FIntPoint DownsampledViewRectSize = BloomAndEyeDownSamples.GetFinalViewRectSize();

	// Compute the eye adaptation value based on average luminance from log2 luminance buffer, history, and specific shader parameters.
	FRenderingCompositePass* Node = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessBasicEyeAdaptation(DownsampledViewRectSize));
	Node->SetInput(ePId_Input0, PostProcessPriorReduction);
	return FRenderingCompositeOutputRef(Node);
}

static FRenderingCompositeOutputRef AddPostProcessHistogramEyeAdaptation(FPostprocessContext& Context, FRenderingCompositeOutputRef& Histogram)
{
	const bool bIsComputePass = CVarPostProcessingPreferCompute.GetValueOnRenderThread() && Context.View.FeatureLevel >= ERHIFeatureLevel::SM5;
	FRenderingCompositePass* Node = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessEyeAdaptation(bIsComputePass));

	Node->SetInput(ePId_Input0, Histogram);
	return FRenderingCompositeOutputRef(Node);
}

static void AddVisualizeBloomSetup(FPostprocessContext& Context)
{
	auto Node = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessVisualizeBloomSetup());

	Node->SetInput(ePId_Input0, Context.FinalOutput);

	Context.FinalOutput = FRenderingCompositeOutputRef(Node);
}

static void AddVisualizeBloomOverlay(FPostprocessContext& Context, FRenderingCompositeOutputRef& HDRColor, FRenderingCompositeOutputRef& BloomOutputCombined)
{
	auto Node = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessVisualizeBloomOverlay());

	Node->SetInput(ePId_Input0, Context.FinalOutput);
	Node->SetInput(ePId_Input1, HDRColor);
	Node->SetInput(ePId_Input2, BloomOutputCombined);

	Context.FinalOutput = FRenderingCompositeOutputRef(Node);
}

static void AddPostProcessDepthOfFieldBokeh(FPostprocessContext& Context, FRenderingCompositeOutputRef& SeparateTranslucency, FRenderingCompositeOutputRef& VelocityInput)
{
	// downsample, mask out the in focus part, depth in alpha
	const bool bIsComputePass = CVarPostProcessingPreferCompute.GetValueOnRenderThread() && Context.View.FeatureLevel >= ERHIFeatureLevel::SM5;
	FRenderingCompositePass* DOFSetup = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessBokehDOFSetup(bIsComputePass));
	DOFSetup->SetInput(ePId_Input0, FRenderingCompositeOutputRef(Context.FinalOutput));
	DOFSetup->SetInput(ePId_Input1, FRenderingCompositeOutputRef(Context.SceneDepth));

	FSceneViewState* ViewState = (FSceneViewState*)Context.View.State;
	
	FRenderingCompositePass* DOFInputPass = DOFSetup;
	if( Context.View.AntiAliasingMethod == AAM_TemporalAA && ViewState )
	{
		FRenderingCompositePass* HistoryInput;
		if( ViewState->DOFHistoryRT && ViewState->bDOFHistory && !Context.View.bCameraCut )
		{
			HistoryInput = Context.Graph.RegisterPass( new(FMemStack::Get()) FRCPassPostProcessInput( ViewState->DOFHistoryRT ) );
		}
		else
		{
			// No history so use current as history
			HistoryInput = DOFSetup;
		}

		FRenderingCompositePass* NodeTemporalAA = Context.Graph.RegisterPass( new(FMemStack::Get()) FRCPassPostProcessDOFTemporalAA(bIsComputePass) );
		NodeTemporalAA->SetInput( ePId_Input0, DOFSetup );
		NodeTemporalAA->SetInput( ePId_Input1, FRenderingCompositeOutputRef( HistoryInput ) );
		NodeTemporalAA->SetInput( ePId_Input2, FRenderingCompositeOutputRef( HistoryInput ) );
		NodeTemporalAA->SetInput( ePId_Input3, VelocityInput );

		DOFInputPass = NodeTemporalAA;
		ViewState->bDOFHistory = true;
	}

	FRenderingCompositePass* NodeBlurred = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessBokehDOF());
	NodeBlurred->SetInput(ePId_Input0, DOFInputPass);
	NodeBlurred->SetInput(ePId_Input1, Context.SceneColor);
	NodeBlurred->SetInput(ePId_Input2, Context.SceneDepth);

	FRenderingCompositePass* NodeRecombined = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessBokehDOFRecombine(bIsComputePass));
	NodeRecombined->SetInput(ePId_Input0, Context.FinalOutput);
	NodeRecombined->SetInput(ePId_Input1, NodeBlurred);
	NodeRecombined->SetInput(ePId_Input2, SeparateTranslucency);

	Context.FinalOutput = FRenderingCompositeOutputRef(NodeRecombined);
}

static bool AddPostProcessDepthOfFieldGaussian(FPostprocessContext& Context, FDepthOfFieldStats& Out, FRenderingCompositeOutputRef& VelocityInput, FRenderingCompositeOutputRef& SeparateTranslucencyRef)
{
	// GaussianDOFPass performs Gaussian setup, blur and recombine.
	auto GaussianDOFPass = [&Context, &Out, &VelocityInput](FRenderingCompositeOutputRef& SeparateTranslucency, float FarSize, float NearSize)
	{
		// GenerateGaussianDOFBlur produces a blurred image from setup or potentially from taa result.
		auto GenerateGaussianDOFBlur = [&Context, &VelocityInput](FRenderingCompositeOutputRef& DOFSetup, bool bFarPass, float BlurSize)
		{
			FSceneViewState* ViewState = (FSceneViewState*)Context.View.State;

			FRenderingCompositeOutputRef DOFInputPass = DOFSetup;
			const bool bMobileQuality = (Context.View.GetFeatureLevel() <= ERHIFeatureLevel::ES3_1);
			if (Context.View.AntiAliasingMethod == AAM_TemporalAA && ViewState && !bMobileQuality)
			{
				// If no history use current as history
				FRenderingCompositeOutputRef HistoryInput = DOFSetup;

				TRefCountPtr<IPooledRenderTarget> DOFHistoryRT = bFarPass ? ViewState->DOFHistoryRT : ViewState->DOFHistoryRT2;
				bool& bDOFHistory = bFarPass ? ViewState->bDOFHistory : ViewState->bDOFHistory2;

				if (DOFHistoryRT && !bDOFHistory && !Context.View.bCameraCut)
				{
					HistoryInput = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessInput(DOFHistoryRT));
				}

				const bool bIsComputePass = CVarPostProcessingPreferCompute.GetValueOnRenderThread() && Context.View.FeatureLevel >= ERHIFeatureLevel::SM5;
				FRenderingCompositePass* NodeTemporalAA = bFarPass ?
					(FRenderingCompositePass*)Context.Graph.RegisterPass(new (FMemStack::Get()) FRCPassPostProcessDOFTemporalAA(bIsComputePass)) :
					(FRenderingCompositePass*)Context.Graph.RegisterPass(new (FMemStack::Get()) FRCPassPostProcessDOFTemporalAANear);

				NodeTemporalAA->SetInput(ePId_Input0, DOFSetup);
				NodeTemporalAA->SetInput(ePId_Input1, HistoryInput);
				NodeTemporalAA->SetInput(ePId_Input2, HistoryInput);
				NodeTemporalAA->SetInput(ePId_Input3, VelocityInput);

				DOFInputPass = FRenderingCompositeOutputRef(NodeTemporalAA);
				bDOFHistory = false;
			}

			const TCHAR* BlurDebugX = bFarPass ? TEXT("FarDOFBlurX") : TEXT("NearDOFBlurX");
			const TCHAR* BlurDebugY = bFarPass ? TEXT("FarDOFBlurY") : TEXT("NearDOFBlurY");

			return RenderGaussianBlur(Context, BlurDebugX, BlurDebugY, DOFInputPass, BlurSize);
		};

		const bool bFar = FarSize > 0.0f;
		const bool bNear = NearSize > 0.0f;
		const bool bCombinedNearFarPass = bFar && bNear;
		const bool bMobileQuality = Context.View.FeatureLevel < ERHIFeatureLevel::SM4;

		FRenderingCompositeOutputRef SetupInput(Context.FinalOutput);
		if (bMobileQuality)
		{
			FRenderingCompositePass* HalfResFar = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessDownsample(PF_FloatRGBA, 1, false, TEXT("GausSetupHalfRes")));
			HalfResFar->SetInput(ePId_Input0, FRenderingCompositeOutputRef(SetupInput));
			SetupInput = HalfResFar;
		}

		FRenderingCompositePass* DOFSetupPass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessDOFSetup(bFar, bNear));
		DOFSetupPass->SetInput(ePId_Input0, FRenderingCompositeOutputRef(SetupInput));
		DOFSetupPass->SetInput(ePId_Input1, FRenderingCompositeOutputRef(Context.SceneDepth));
		FRenderingCompositeOutputRef DOFSetupFar(DOFSetupPass);
		FRenderingCompositeOutputRef DOFSetupNear(DOFSetupPass, bCombinedNearFarPass ? ePId_Output1 : ePId_Output0);

		FRenderingCompositeOutputRef DOFFarBlur, DOFNearBlur;
		if (bFar)
		{
			DOFFarBlur = GenerateGaussianDOFBlur(DOFSetupFar, true, FarSize);
		}

		if (bNear)
		{
			DOFNearBlur = GenerateGaussianDOFBlur(DOFSetupNear, false, NearSize);
		}

		FRenderingCompositePass* GaussianDOFRecombined = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessDOFRecombine());
		GaussianDOFRecombined->SetInput(ePId_Input0, Context.FinalOutput);
		GaussianDOFRecombined->SetInput(ePId_Input1, DOFFarBlur);
		GaussianDOFRecombined->SetInput(ePId_Input2, DOFNearBlur);
		GaussianDOFRecombined->SetInput(ePId_Input3, SeparateTranslucency);

		Context.FinalOutput = FRenderingCompositeOutputRef(GaussianDOFRecombined);
	};

	float FarSize = Context.View.FinalPostProcessSettings.DepthOfFieldFarBlurSize;
	float NearSize = Context.View.FinalPostProcessSettings.DepthOfFieldNearBlurSize;
	const float MaxSize = CVarDepthOfFieldMaxSize.GetValueOnRenderThread();
	FarSize = FMath::Min(FarSize, MaxSize);
	NearSize = FMath::Min(NearSize, MaxSize);
	Out.bFar = FarSize >= 0.01f;

	{
		const float CVarThreshold = CVarDepthOfFieldNearBlurSizeThreshold.GetValueOnRenderThread();
		Out.bNear = (NearSize >= CVarThreshold);
	}

	if (Context.View.Family->EngineShowFlags.VisualizeDOF)
	{
		// no need for this pass
		Out.bFar = false;
		Out.bNear = false;
	}

	if (Out.bFar || Out.bNear)
	{
		GaussianDOFPass(SeparateTranslucencyRef, Out.bFar ? FarSize : 0, Out.bNear ? NearSize : 0);

		const bool bMobileQuality = Context.View.FeatureLevel < ERHIFeatureLevel::SM4;
		return SeparateTranslucencyRef.IsValid() && !bMobileQuality;
	}
	else
	{
		return false;
	}
}

static void AddPostProcessDepthOfFieldCircle(FPostprocessContext& Context, FDepthOfFieldStats& Out, FRenderingCompositeOutputRef& VelocityInput)
{
	if(Context.View.Family->EngineShowFlags.VisualizeDOF)
	{
		// no need for this pass
		return;
	}

	FRenderingCompositePass* DOFSetup = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessCircleDOFSetup());
	DOFSetup->SetInput(ePId_Input0, FRenderingCompositeOutputRef(Context.FinalOutput));
	DOFSetup->SetInput(ePId_Input1, FRenderingCompositeOutputRef(Context.SceneDepth));

	FSceneViewState* ViewState = (FSceneViewState*)Context.View.State;

	auto ColorSetup = FRenderingCompositeOutputRef( DOFSetup, ePId_Output0 );
	auto CocSetup = FRenderingCompositeOutputRef( DOFSetup, FPostProcessing::HasAlphaChannelSupport() ? ePId_Output1 : ePId_Output0 );
	if( Context.View.AntiAliasingMethod == AAM_TemporalAA && ViewState )
	{
		FRenderingCompositePass* HistoryInput;
		if( ViewState->DOFHistoryRT && !ViewState->bDOFHistory && !Context.View.bCameraCut )
		{
			HistoryInput = Context.Graph.RegisterPass( new(FMemStack::Get()) FRCPassPostProcessInput( ViewState->DOFHistoryRT ) );
		}
		else
		{
			// No history so use current as history
			HistoryInput = DOFSetup;
		}

		const bool bIsComputePass = CVarPostProcessingPreferCompute.GetValueOnRenderThread() && Context.View.FeatureLevel >= ERHIFeatureLevel::SM5;
		FRenderingCompositePass* NodeTemporalAA = Context.Graph.RegisterPass( new(FMemStack::Get()) FRCPassPostProcessDOFTemporalAA(bIsComputePass) );
		NodeTemporalAA->SetInput( ePId_Input0, DOFSetup );
		NodeTemporalAA->SetInput( ePId_Input1, FRenderingCompositeOutputRef( HistoryInput ) );
		NodeTemporalAA->SetInput( ePId_Input2, FRenderingCompositeOutputRef( HistoryInput ) );
		NodeTemporalAA->SetInput( ePId_Input3, VelocityInput );

		ColorSetup = FRenderingCompositeOutputRef(NodeTemporalAA);
		ViewState->bDOFHistory = false;
	}

	FRenderingCompositePass* DOFNear = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessCircleDOFDilate());
	DOFNear->SetInput(ePId_Input0, CocSetup);
	FRenderingCompositeOutputRef Near = FRenderingCompositeOutputRef(DOFNear, ePId_Output0);

	FRenderingCompositePass* DOFApply = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessCircleDOF());
	DOFApply->SetInput(ePId_Input0, ColorSetup);
	DOFApply->SetInput(ePId_Input1, Near);
	DOFApply->SetInput(ePId_Input2, CocSetup);
	FRenderingCompositeOutputRef Far = FRenderingCompositeOutputRef(DOFApply, ePId_Output0);
	FRenderingCompositeOutputRef FarCoc = FPostProcessing::HasAlphaChannelSupport() ? FRenderingCompositeOutputRef(DOFApply, ePId_Output1) : Far;

	FRenderingCompositePass* NodeRecombined = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessCircleDOFRecombine());
	NodeRecombined->SetInput(ePId_Input0, Context.FinalOutput);
	NodeRecombined->SetInput(ePId_Input1, Far);
	NodeRecombined->SetInput(ePId_Input2, FarCoc);

	Context.FinalOutput = FRenderingCompositeOutputRef(NodeRecombined);
}


static FRenderingCompositeOutputRef AddBloom(FBloomDownSampleArray& BloomDownSampleArray, bool bVisualizeBloom)
{
	
	// Quality level to bloom stages table. Note: 0 is omitted, ensure element count tallys with the range documented with 'r.BloomQuality' definition.
	const static uint32 BloomQualityStages[] =
	{
		3,// Q1
		3,// Q2
		4,// Q3
		5,// Q4
		6,// Q5
	};

	int32 BloomQuality;
	{
		// console variable override
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.BloomQuality"));
		BloomQuality = FMath::Clamp(CVar->GetValueOnRenderThread(), 0, (int32)ARRAY_COUNT(BloomQualityStages));
	}

	// Extract the Context
	FPostprocessContext& Context = BloomDownSampleArray.Context;

	const bool bOldMetalNoFFT = (IsMetalPlatform(Context.View.GetShaderPlatform()) && RHIGetShaderLanguageVersion(Context.View.GetShaderPlatform()) < 2);
	const bool bUseFFTBloom = (Context.View.FinalPostProcessSettings.BloomMethod == EBloomMethod::BM_FFT
		&& Context.View.FeatureLevel >= ERHIFeatureLevel::SM5);
		
	static bool bWarnAboutOldMetalFFTOnce = false;
	if (bOldMetalNoFFT && bUseFFTBloom && !bWarnAboutOldMetalFFTOnce)
	{
		UE_LOG(LogRenderer, Error, TEXT("Metal v1.2 and above is required to enable FFT Bloom. Set Max. Shader Standard to target to Metal v1.2 in Project Settings > Mac/iOS and recook."));
		bWarnAboutOldMetalFFTOnce = true;
	}

	// Extract the downsample array.
	FBloomDownSampleArray::FRenderingRefArray& PostProcessDownsamples = BloomDownSampleArray.PostProcessDownsamples;

	FRenderingCompositeOutputRef BloomOutput;  
	if (BloomQuality == 0)
	{
		// No bloom, provide substitute source for lens flare.
		BloomOutput = PostProcessDownsamples[0];
	}
	else if (bUseFFTBloom && !bOldMetalNoFFT)
	{
		
		// verify the physical kernel is valid, or fail gracefully by skipping bloom 
		if (FRCPassFFTBloom::HasValidPhysicalKernel(Context))
		{

			// Use the first down sample as the source:
			const uint32 DownSampleIndex = 0;
			FRenderingCompositeOutputRef HalfResolutionRef = PostProcessDownsamples[DownSampleIndex];
			FRenderingCompositeOutputRef FullResolutionRef = Context.FinalOutput;

			FRenderingCompositePass* FFTPass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassFFTBloom());
			const bool bDoFullResBloom = (CVarHalfResFFTBloom.GetValueOnRenderThread() != 1);
			if (bDoFullResBloom)
			{
				FFTPass->SetInput(ePId_Input0, FRenderingCompositeOutputRef(FullResolutionRef));
			}
			else
			{
				FFTPass->SetInput(ePId_Input0, FRenderingCompositeOutputRef(HalfResolutionRef));
				FFTPass->SetInput(ePId_Input1, FRenderingCompositeOutputRef(FullResolutionRef));
			}

			Context.FinalOutput = FRenderingCompositeOutputRef(FFTPass);
		}
	}
	else
	{
		// Perform bloom blur + accumulate.
		struct FBloomStage
		{
			float BloomSize;
			const FLinearColor* Tint;
		};
		const FFinalPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;

		FBloomStage BloomStages[] =
		{
			{ Settings.Bloom6Size, &Settings.Bloom6Tint },
			{ Settings.Bloom5Size, &Settings.Bloom5Tint },
			{ Settings.Bloom4Size, &Settings.Bloom4Tint },
			{ Settings.Bloom3Size, &Settings.Bloom3Tint },
			{ Settings.Bloom2Size, &Settings.Bloom2Tint },
			{ Settings.Bloom1Size, &Settings.Bloom1Tint },
		};
		static const uint32 NumBloomStages = ARRAY_COUNT(BloomStages);

		const uint32 BloomStageCount = BloomQualityStages[BloomQuality - 1];
		check(BloomStageCount <= NumBloomStages);
		float TintScale = 1.0f / NumBloomStages;
		for (uint32 i = 0, SourceIndex = NumBloomStages - 1; i < BloomStageCount; i++, SourceIndex--)
		{
			FBloomStage& Op = BloomStages[i];

			FLinearColor Tint = (*Op.Tint) * TintScale;

			// Visualize bloom show effect of this modified bloom kernel on a single ray of green at the center of the screen
			// Note: This bloom visualization is pretty bogus for two reasons.  1) The bloom kernel is really 3 kernels (one for each r,g,b),
			// and replacing it by a single kernel for visualization isn't very sound.  2) The actual visualizer compares the response to
			// an arbitrary function..
			if (bVisualizeBloom)
			{
				float LumScale = Tint.ComputeLuminance();

				// R is used to pass down the reference, G is the emulated bloom
				Tint.R = 0;
				Tint.G = LumScale;
				Tint.B = 0;
			}
			// Only bloom this down-sampled input if the bloom size is non-zero
			if (Op.BloomSize > SMALL_NUMBER)
			{

				BloomOutput = RenderBloom(Context, PostProcessDownsamples[SourceIndex], Op.BloomSize * Settings.BloomSizeScale, Tint, BloomOutput);
			}
		}

		if (!BloomOutput.IsValid())
		{
			// Bloom was disabled by setting bloom size to zero in the post process.
			// No bloom, provide substitute source for lens flare.
			BloomOutput = PostProcessDownsamples[0];
		}
	}

	//do not default bloomoutput to PostProcessDownsamples[0] or you will get crazy overbloom with some FFT settings
	//however flares require an input.
	FRenderingCompositeOutputRef BloomFlareInput;
	if (BloomOutput.IsValid())
	{
		BloomFlareInput = BloomOutput;
	}
	else
	{
		BloomFlareInput = PostProcessDownsamples[0];
	}

	// Lens Flares
	FLinearColor LensFlareHDRColor = Context.View.FinalPostProcessSettings.LensFlareTint * Context.View.FinalPostProcessSettings.LensFlareIntensity;
	static const int32 MaxLensFlareQuality = 3;
	int32 LensFlareQuality;
	{
		// console variable override
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.LensFlareQuality"));
		LensFlareQuality = FMath::Clamp(CVar->GetValueOnRenderThread(), 0, MaxLensFlareQuality);
	}

	if (!LensFlareHDRColor.IsAlmostBlack() && LensFlareQuality > 0 && !bVisualizeBloom)
	{
		float PercentKernelSize = Context.View.FinalPostProcessSettings.LensFlareBokehSize;

		bool bLensBlur = PercentKernelSize > 0.3f;

		FRenderingCompositePass* PostProcessFlares = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessLensFlares(bLensBlur ? 2.0f : 1.0f, !bUseFFTBloom));

		PostProcessFlares->SetInput(ePId_Input0, BloomFlareInput);

		FRenderingCompositeOutputRef LensFlareInput = PostProcessDownsamples[MaxLensFlareQuality - LensFlareQuality];

		if (bLensBlur)
		{
			float Threshold = Context.View.FinalPostProcessSettings.LensFlareThreshold;

			FRenderingCompositePass* PostProcessLensBlur = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessLensBlur(PercentKernelSize, Threshold));
			PostProcessLensBlur->SetInput(ePId_Input0, LensFlareInput);
			PostProcessFlares->SetInput(ePId_Input1, FRenderingCompositeOutputRef(PostProcessLensBlur));
		}
		else
		{
			// fast: no blurring or blurring shared from bloom
			PostProcessFlares->SetInput(ePId_Input1, LensFlareInput);
		}

		BloomOutput = FRenderingCompositeOutputRef(PostProcessFlares);
	}

	return BloomOutput;
}

static void AddTemporalAA( FPostprocessContext& Context, FRenderingCompositeOutputRef& VelocityInput )
{
	check(VelocityInput.IsValid());

	FSceneViewState* ViewState = (FSceneViewState*)Context.View.State;

	FRenderingCompositePass* HistoryInput;
	if( ViewState && ViewState->TemporalAAHistoryRT && !Context.View.bCameraCut )
	{
		HistoryInput = Context.Graph.RegisterPass( new(FMemStack::Get()) FRCPassPostProcessInput( ViewState->TemporalAAHistoryRT ) );
	}
	else
	{
		// No history so use current as history
		HistoryInput = Context.Graph.RegisterPass( new(FMemStack::Get()) FRCPassPostProcessInput( FSceneRenderTargets::Get(Context.RHICmdList).GetSceneColor() ) );
	}

	const bool bIsComputePass = CVarPostProcessingPreferCompute.GetValueOnRenderThread() && Context.View.FeatureLevel >= ERHIFeatureLevel::SM5;
	FRenderingCompositePass* TemporalAAPass = Context.Graph.RegisterPass( new(FMemStack::Get()) FRCPassPostProcessTemporalAA(bIsComputePass) );
	TemporalAAPass->SetInput( ePId_Input0, Context.FinalOutput );
	TemporalAAPass->SetInput( ePId_Input1, FRenderingCompositeOutputRef( HistoryInput ) );
	TemporalAAPass->SetInput( ePId_Input2, FRenderingCompositeOutputRef( HistoryInput ) );
	TemporalAAPass->SetInput( ePId_Input3, VelocityInput );
	Context.FinalOutput = FRenderingCompositeOutputRef( TemporalAAPass );
}

FPostProcessMaterialNode* IteratePostProcessMaterialNodes(const FFinalPostProcessSettings& Dest, EBlendableLocation InLocation, FBlendableEntry*& Iterator)
{
	for(;;)
	{
		FPostProcessMaterialNode* DataPtr = Dest.BlendableManager.IterateBlendables<FPostProcessMaterialNode>(Iterator);

		if(!DataPtr || DataPtr->GetLocation() == InLocation)
		{
			return DataPtr;
		}
	}
}


static FRenderingCompositePass* AddSinglePostProcessMaterial(FPostprocessContext& Context, EBlendableLocation InLocation)
{
	if(!Context.View.Family->EngineShowFlags.PostProcessing || !Context.View.Family->EngineShowFlags.PostProcessMaterial)
	{
		return 0;
	}

	FBlendableEntry* Iterator = 0;
	FPostProcessMaterialNode PPNode;

	while(FPostProcessMaterialNode* Data = IteratePostProcessMaterialNodes(Context.View.FinalPostProcessSettings, InLocation, Iterator))
	{
		check(Data->GetMaterialInterface());

		if(PPNode.IsValid())
		{
			FPostProcessMaterialNode::FCompare Dummy;

			// take the one with the highest priority
			if(!Dummy.operator()(PPNode, *Data))
			{
				continue;
			}
		}

		PPNode = *Data;
	}

	if(UMaterialInterface* MaterialInterface = PPNode.GetMaterialInterface())
	{
		FMaterialRenderProxy* Proxy = MaterialInterface->GetRenderProxy(false);

		check(Proxy);

		const FMaterial* Material = Proxy->GetMaterial(Context.View.GetFeatureLevel());

		check(Material);

		if(Material->NeedsGBuffer())
		{
			// AdjustGBufferRefCount(-1) call is done when the pass gets executed
			FSceneRenderTargets::Get(Context.RHICmdList).AdjustGBufferRefCount(Context.RHICmdList, 1);
		}

		FRenderingCompositePass* Node = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessMaterial(MaterialInterface, Context.View.GetFeatureLevel()));

		return Node;
	}

	return 0;
}

// simplied version of AddPostProcessMaterial(), side effect free
static bool HasPostProcessMaterial(FPostprocessContext& Context, EBlendableLocation InLocation)
{
	if(!Context.View.Family->EngineShowFlags.PostProcessing || !Context.View.Family->EngineShowFlags.PostProcessMaterial)
	{
		return false;
	}

	if(Context.View.Family->EngineShowFlags.VisualizeBuffer)
	{	
		// Apply requested material to the full screen
		UMaterial* Material = GetBufferVisualizationData().GetMaterial(Context.View.CurrentBufferVisualizationMode);
		
		if(Material && Material->BlendableLocation == InLocation)
		{
			return true;
		}
	}

	FBlendableEntry* Iterator = 0;
	FPostProcessMaterialNode* Data = IteratePostProcessMaterialNodes(Context.View.FinalPostProcessSettings, InLocation, Iterator);

	if(Data)
	{
		return true;
	}
	
	return false;
}

static void AddPostProcessMaterial(FPostprocessContext& Context, EBlendableLocation InLocation, FRenderingCompositeOutputRef SeparateTranslucency, FRenderingCompositeOutputRef PreTonemapHDRColor = FRenderingCompositeOutputRef(), FRenderingCompositeOutputRef PostTonemapHDRColor = FRenderingCompositeOutputRef())
{
	if( !Context.View.Family->EngineShowFlags.PostProcessing ||
		!Context.View.Family->EngineShowFlags.PostProcessMaterial ||
		Context.View.Family->EngineShowFlags.VisualizeShadingModels)		// we should add more
	{
		return;
	}

	// hard coded - this should be a reasonable limit
	const uint32 MAX_PPMATERIALNODES = 10;
	FBlendableEntry* Iterator = 0;
	FPostProcessMaterialNode PPNodes[MAX_PPMATERIALNODES];
	uint32 PPNodeCount = 0;
	bool bVisualizingBuffer = false;

	if(Context.View.Family->EngineShowFlags.VisualizeBuffer)
	{	
		// Apply requested material to the full screen
		UMaterial* Material = GetBufferVisualizationData().GetMaterial(Context.View.CurrentBufferVisualizationMode);
		
		if(Material && Material->BlendableLocation == InLocation)
		{
			PPNodes[0] = FPostProcessMaterialNode(Material, InLocation, Material->BlendablePriority);
			++PPNodeCount;
			bVisualizingBuffer = true;
		}
	}
	for(;PPNodeCount < MAX_PPMATERIALNODES; ++PPNodeCount)
	{
		FPostProcessMaterialNode* Data = IteratePostProcessMaterialNodes(Context.View.FinalPostProcessSettings, InLocation, Iterator);

		if(!Data)
		{
			break;
		}

		check(Data->GetMaterialInterface());

		PPNodes[PPNodeCount] = *Data;
	}
	
	::Sort(PPNodes, PPNodeCount, FPostProcessMaterialNode::FCompare());

	ERHIFeatureLevel::Type FeatureLevel = Context.View.GetFeatureLevel();

	for(uint32 i = 0; i < PPNodeCount; ++i)
	{
		UMaterialInterface* MaterialInterface = PPNodes[i].GetMaterialInterface();

		FMaterialRenderProxy* Proxy = MaterialInterface->GetRenderProxy(false);

		check(Proxy);

		const FMaterial* Material = Proxy->GetMaterial(Context.View.GetFeatureLevel());

		check(Material);

		if(Material->NeedsGBuffer())
		{
			// AdjustGBufferRefCount(-1) call is done when the pass gets executed
			FSceneRenderTargets::Get(Context.RHICmdList).AdjustGBufferRefCount(Context.RHICmdList, 1);
		}

		FRenderingCompositePass* Node = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessMaterial(MaterialInterface,FeatureLevel));
		Node->SetInput(ePId_Input0, FRenderingCompositeOutputRef(Context.FinalOutput));

		// We are binding separate translucency here because the post process SceneTexture node can reference 
		// the separate translucency buffers through ePId_Input1. 
		// TODO: Check if material actually uses this texture and only bind if needed.
		Node->SetInput(ePId_Input1, SeparateTranslucency);

		// This input is only needed for visualization and frame dumping
		if (bVisualizingBuffer)
		{
			Node->SetInput(ePId_Input2, PreTonemapHDRColor);
			Node->SetInput(ePId_Input3, PostTonemapHDRColor);
		}

		Context.FinalOutput = FRenderingCompositeOutputRef(Node);
	}
}

static void AddHighResScreenshotMask(FPostprocessContext& Context, FRenderingCompositeOutputRef& SeparateTranslucencyInput)
{
	if (Context.View.Family->EngineShowFlags.HighResScreenshotMask != 0)
	{
		check(Context.View.FinalPostProcessSettings.HighResScreenshotMaterial);

		FRenderingCompositeOutputRef Input = Context.FinalOutput;

		FRenderingCompositePass* CompositePass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessMaterial(Context.View.FinalPostProcessSettings.HighResScreenshotMaterial, Context.View.GetFeatureLevel()));
		CompositePass->SetInput(ePId_Input0, FRenderingCompositeOutputRef(Input));
		Context.FinalOutput = FRenderingCompositeOutputRef(CompositePass);

		if (GIsHighResScreenshot)
		{
			check(Context.View.FinalPostProcessSettings.HighResScreenshotMaskMaterial); 

			FRenderingCompositePass* MaskPass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessMaterial(Context.View.FinalPostProcessSettings.HighResScreenshotMaskMaterial, Context.View.GetFeatureLevel()));
			MaskPass->SetInput(ePId_Input0, FRenderingCompositeOutputRef(Input));
			CompositePass->AddDependency(MaskPass);

			FString BaseFilename = FString(Context.View.FinalPostProcessSettings.BufferVisualizationDumpBaseFilename);
			MaskPass->SetOutputColorArray(ePId_Output0, FScreenshotRequest::GetHighresScreenshotMaskColorArray());
		}
	}

	// Draw the capture region if a material was supplied
	if (Context.View.FinalPostProcessSettings.HighResScreenshotCaptureRegionMaterial)
	{
		auto Material = Context.View.FinalPostProcessSettings.HighResScreenshotCaptureRegionMaterial;

		FRenderingCompositePass* CaptureRegionVisualizationPass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessMaterial(Material, Context.View.GetFeatureLevel()));
		CaptureRegionVisualizationPass->SetInput(ePId_Input0, FRenderingCompositeOutputRef(Context.FinalOutput));
		Context.FinalOutput = FRenderingCompositeOutputRef(CaptureRegionVisualizationPass);

		auto Proxy = Material->GetRenderProxy(false);
		const FMaterial* RendererMaterial = Proxy->GetMaterial(Context.View.GetFeatureLevel());
		if (RendererMaterial->NeedsGBuffer())
		{
			// AdjustGBufferRefCount(-1) call is done when the pass gets executed
			FSceneRenderTargets::Get(Context.RHICmdList).AdjustGBufferRefCount(Context.RHICmdList, 1);
		}
	}
}

static void AddGBufferVisualizationOverview(FPostprocessContext& Context, FRenderingCompositeOutputRef& SeparateTranslucencyInput, FRenderingCompositeOutputRef& PreTonemapHDRColorInput, FRenderingCompositeOutputRef& PostTonemapHDRColorInput)
{
	static const auto CVarDumpFrames = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.BufferVisualizationDumpFrames"));
	static const auto CVarDumpFramesAsHDR = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.BufferVisualizationDumpFramesAsHDR"));

	bool bVisualizationEnabled = Context.View.Family->EngineShowFlags.VisualizeBuffer;
	bool bOverviewModeEnabled = bVisualizationEnabled && (Context.View.CurrentBufferVisualizationMode == NAME_None);
	bool bHighResBufferVisualizationDumpRequried = GIsHighResScreenshot && GetHighResScreenshotConfig().bDumpBufferVisualizationTargets;
	bool bDumpFrames = Context.View.FinalPostProcessSettings.bBufferVisualizationDumpRequired && (CVarDumpFrames->GetValueOnRenderThread() || bHighResBufferVisualizationDumpRequried);
	bool bCaptureAsHDR = CVarDumpFramesAsHDR->GetValueOnRenderThread() || GetHighResScreenshotConfig().bCaptureHDR;
	FString BaseFilename;

	if (bDumpFrames)
	{
		BaseFilename = FString(Context.View.FinalPostProcessSettings.BufferVisualizationDumpBaseFilename);
	}
	
	if (bDumpFrames || bVisualizationEnabled)
	{	
		FRenderingCompositeOutputRef IncomingStage = Context.FinalOutput;

		if (bDumpFrames || bOverviewModeEnabled)
		{
			FRenderingCompositePass* CompositePass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessVisualizeBuffer());
			CompositePass->SetInput(ePId_Input0, FRenderingCompositeOutputRef(IncomingStage));
			Context.FinalOutput = FRenderingCompositeOutputRef(CompositePass);
			EPixelFormat OutputFormat = bCaptureAsHDR ? PF_FloatRGBA : PF_Unknown;

			// Loop over materials, creating stages for generation and downsampling of the tiles.			
			for (TArray<UMaterialInterface*>::TConstIterator It = Context.View.FinalPostProcessSettings.BufferVisualizationOverviewMaterials.CreateConstIterator(); It; ++It)
			{
				auto MaterialInterface = *It;
				if (MaterialInterface)
				{
					// Apply requested material
					FRenderingCompositePass* MaterialPass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessMaterial(*It, Context.View.GetFeatureLevel(), OutputFormat));
					MaterialPass->SetInput(ePId_Input0, FRenderingCompositeOutputRef(IncomingStage));
					MaterialPass->SetInput(ePId_Input1, FRenderingCompositeOutputRef(SeparateTranslucencyInput));
					MaterialPass->SetInput(ePId_Input2, FRenderingCompositeOutputRef(PreTonemapHDRColorInput));
					MaterialPass->SetInput(ePId_Input3, FRenderingCompositeOutputRef(PostTonemapHDRColorInput));

					auto Proxy = MaterialInterface->GetRenderProxy(false);
					const FMaterial* Material = Proxy->GetMaterial(Context.View.GetFeatureLevel());
					if (Material->NeedsGBuffer())
					{
						// AdjustGBufferRefCount(-1) call is done when the pass gets executed
						FSceneRenderTargets::Get(Context.RHICmdList).AdjustGBufferRefCount(Context.RHICmdList, 1);
					}

					if (BaseFilename.Len())
					{
						// First off, allow the user to specify the pass as a format arg (using {material})
						TMap<FString, FStringFormatArg> FormatMappings;
						FormatMappings.Add(TEXT("material"), (*It)->GetName());

						FString MaterialFilename = FString::Format(*BaseFilename, FormatMappings);

						// If the format made no change to the string, we add the name of the material to ensure uniqueness
						if (MaterialFilename == BaseFilename)
						{
							MaterialFilename = BaseFilename + TEXT("_") + (*It)->GetName();
						}

						MaterialFilename.Append(TEXT(".png"));
						MaterialPass->SetOutputDumpFilename(ePId_Output0, *MaterialFilename);
					}

					// If the overview mode is activated, downsample the material pass to quarter size
					if (bOverviewModeEnabled)
					{
						// Down-sample to 1/2 size
						FRenderingCompositePass* HalfSize = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessDownsample(PF_Unknown, 0, false, TEXT("MaterialHalfSize")));
						HalfSize->SetInput(ePId_Input0, FRenderingCompositeOutputRef(MaterialPass));

						// Down-sample to 1/4 size
						FRenderingCompositePass* QuarterSize = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessDownsample(PF_Unknown, 0, false, TEXT("MaterialQuarterSize")));
						QuarterSize->SetInput(ePId_Input0, FRenderingCompositeOutputRef(HalfSize));

						// Mark the quarter size target as the dependency for the composite pass
						((FRCPassPostProcessVisualizeBuffer*)CompositePass)->AddVisualizationBuffer(FRenderingCompositeOutputRef(QuarterSize), (*It)->GetName());
					}
					else
					{
						// We are just dumping the frames, so the material pass is the dependency of the composite
						CompositePass->AddDependency(MaterialPass);
					}
				}
				else
				{
					if (bOverviewModeEnabled)
					{
						((FRCPassPostProcessVisualizeBuffer*)CompositePass)->AddVisualizationBuffer(FRenderingCompositeOutputRef(), FString());
					}
				}
			}
		}
	}
}

// could be moved into the graph
// allows for Framebuffer blending optimization with the composition graph
void OverrideRenderTarget(FRenderingCompositeOutputRef It, TRefCountPtr<IPooledRenderTarget>& RT, FPooledRenderTargetDesc& Desc)
{
	for(;;)
	{
		It.GetOutput()->PooledRenderTarget = RT;
		It.GetOutput()->RenderTargetDesc = Desc;

		if(!It.GetPass()->FrameBufferBlendingWithInput0())
		{
			break;
		}

		It = *It.GetPass()->GetInput(ePId_Input0);
	}
}

bool FPostProcessing::AllowFullPostProcessing(const FViewInfo& View, ERHIFeatureLevel::Type FeatureLevel)
{
	return View.Family->EngineShowFlags.PostProcessing 
		&& FeatureLevel >= ERHIFeatureLevel::SM4 
		&& !View.Family->EngineShowFlags.VisualizeDistanceFieldAO
		&& !View.Family->EngineShowFlags.VisualizeDistanceFieldGI
		&& !View.Family->EngineShowFlags.VisualizeShadingModels
		&& !View.Family->EngineShowFlags.VisualizeMeshDistanceFields
		&& !View.Family->EngineShowFlags.VisualizeGlobalDistanceField;
}

void FPostProcessing::Process(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, TRefCountPtr<IPooledRenderTarget>& VelocityRT)
{
	QUICK_SCOPE_CYCLE_COUNTER( STAT_PostProcessing_Process );

	check(IsInRenderingThread());

	const auto FeatureLevel = View.GetFeatureLevel();

	GRenderTargetPool.AddPhaseEvent(TEXT("PostProcessing"));

	// This page: https://udn.epicgames.com/Three/RenderingOverview#Rendering%20state%20defaults 
	// describes what state a pass can expect and to what state it need to be set back.

	// All post processing is happening on the render thread side. All passes can access FinalPostProcessSettings and all
	// view settings. Those are copies for the RT then never get access by the main thread again.
	// Pointers to other structures might be unsafe to touch.


	// so that the passes can register themselves to the graph
	{
		FMemMark Mark(FMemStack::Get());
		FRenderingCompositePassContext CompositeContext(RHICmdList, View);

		FPostprocessContext Context(RHICmdList, CompositeContext.Graph, View);
		
		// not always valid
		FRenderingCompositeOutputRef HistogramOverScreen;
		FRenderingCompositeOutputRef Histogram;
		FRenderingCompositeOutputRef PreTonemapHDRColor;
		FRenderingCompositeOutputRef PostTonemapHDRColor;

		class FAutoExposure
		{
		public:
			FAutoExposure(const FViewInfo& InView) : 
				MethodId(GetAutoExposureMethod(InView))
			{}
			// distinguish between Basic and Histogram-based
			EAutoExposureMethod          MethodId;
			// not always valid
			FRenderingCompositeOutputRef EyeAdaptation;
		} AutoExposure(View);
	
		// not always valid
		FRenderingCompositeOutputRef SeparateTranslucency;
		// optional
		FRenderingCompositeOutputRef BloomOutputCombined;
		// not always valid
		FRenderingCompositePass* VelocityFlattenPass = 0;
		// in the following code some feature might set this to false
		bool bAllowTonemapper = FeatureLevel >= ERHIFeatureLevel::SM4;
		//
		bool bStereoRenderingAndHMD = View.Family->EngineShowFlags.StereoRendering && View.Family->EngineShowFlags.HMDDistortion;
		//
		FRCPassPostProcessUpscale::PaniniParams PaniniConfig(View);
		//
		EStereoscopicPass StereoPass = View.StereoPass;
		//
		// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
		{
			// Disable the tonemapper for opacity voxels visualization, enable it for emittance and irradiance
			if (View.Family->EngineShowFlags.VxgiOpacityVoxels && View.Family->Views.Num() == 1)
			{
				bAllowTonemapper = false;
			}
		}
		{
			static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.VXGI.ForceDisableTonemapper"));
			if (CVar->GetValueOnRenderThread())
			{
				bAllowTonemapper = false;
			}
		}
#endif
		// NVCHANGE_END: Add VXGI
		FSceneViewState* ViewState = (FSceneViewState*)Context.View.State;
		//
		bool bDoScreenPercentage;
		{
			//
			bool bHMDWantsUpscale = bStereoRenderingAndHMD && GEngine->XRSystem->GetHMDDevice()->NeedsUpscalePostProcessPass();
			// Do not use upscale if SeparateRenderTarget is in use! (stereo rendering wants to control this)
			bool bAllowScreenPercentage = bHMDWantsUpscale || !View.Family->EngineShowFlags.StereoRendering || (!View.Family->EngineShowFlags.HMDDistortion && !View.Family->bUseSeparateRenderTarget);
			// is Upscale from a lower resolution needed and allowed
			bDoScreenPercentage = bAllowScreenPercentage && (View.UnscaledViewRect != View.ViewRect);
		}

		{
			if (FSceneRenderTargets::Get(RHICmdList).SeparateTranslucencyRT)
			{
				FRenderingCompositePass* NodeSeparateTranslucency = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessInput(FSceneRenderTargets::Get(RHICmdList).SeparateTranslucencyRT));
				SeparateTranslucency = FRenderingCompositeOutputRef(NodeSeparateTranslucency);

				// make sure we only release if this is the last view we're rendering
				int32 LastView = View.Family->Views.Num() - 1;
				if (View.Family->Views[LastView] == &View)
				{
					// the node keeps another reference so the RT will not be release too early
					FSceneRenderTargets::Get(RHICmdList).FreeSeparateTranslucency();
					check(!FSceneRenderTargets::Get(RHICmdList).SeparateTranslucencyRT);
				}
			}
		}

		bool bVisualizeHDR = View.Family->EngineShowFlags.VisualizeHDR && FeatureLevel >= ERHIFeatureLevel::SM5;
		bool bVisualizeBloom = View.Family->EngineShowFlags.VisualizeBloom && FeatureLevel >= ERHIFeatureLevel::SM4;
		bool bVisualizeMotionBlur = View.Family->EngineShowFlags.VisualizeMotionBlur && FeatureLevel >= ERHIFeatureLevel::SM4;

		if(bVisualizeHDR || bVisualizeBloom || bVisualizeMotionBlur)
		{
			bAllowTonemapper = false;
		}

		const bool bHDROutputEnabled = GRHISupportsHDROutput && IsHDREnabled();

		static const auto CVarDumpFramesAsHDR = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.BufferVisualizationDumpFramesAsHDR"));
		const bool bHDRTonemapperOutput = bAllowTonemapper && (GetHighResScreenshotConfig().bCaptureHDR || CVarDumpFramesAsHDR->GetValueOnRenderThread() || bHDROutputEnabled);

		FRCPassPostProcessTonemap* Tonemapper = 0;

		// add the passes we want to add to the graph (commenting a line means the pass is not inserted into the graph) ---------

		if (AllowFullPostProcessing(View, FeatureLevel))
		{
			FRenderingCompositeOutputRef VelocityInput;
			if(VelocityRT)
			{
				VelocityInput = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessInput(VelocityRT));
			}

			AddPostProcessMaterial(Context, BL_BeforeTranslucency, SeparateTranslucency);

			static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DepthOfFieldQuality"));
			check(CVar)
			bool bDepthOfField = View.Family->EngineShowFlags.DepthOfField && CVar->GetValueOnRenderThread() > 0;

			FDepthOfFieldStats DepthOfFieldStat;

			bool bSepTransWasApplied = false;

			if(bDepthOfField && View.FinalPostProcessSettings.DepthOfFieldMethod != DOFM_BokehDOF)
			{
				bool bCircleDOF = View.FinalPostProcessSettings.DepthOfFieldMethod == DOFM_CircleDOF;
				if(!bCircleDOF)
				{
					if (FPostProcessing::HasAlphaChannelSupport())
					{
						UE_LOG(LogRenderer, Log, TEXT("Gaussian depth of field does not have alpha channel support. Only Circle DOF has."));
					}
					if(VelocityInput.IsValid())
					{
						bSepTransWasApplied = AddPostProcessDepthOfFieldGaussian(Context, DepthOfFieldStat, VelocityInput, SeparateTranslucency);
					}
					else
					{
						// todo: black/white default is a compositing graph feature, no need to hook up a node

						// black is how we clear the velocity buffer so this means no velocity
						FRenderingCompositePass* NoVelocity = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessInput(GSystemTextures.BlackDummy));
						FRenderingCompositeOutputRef NoVelocityRef(NoVelocity);
						bSepTransWasApplied = AddPostProcessDepthOfFieldGaussian(Context, DepthOfFieldStat, NoVelocityRef, SeparateTranslucency);
					}
				}
				else
				{
					if(VelocityInput.IsValid())
					{
						AddPostProcessDepthOfFieldCircle(Context, DepthOfFieldStat, VelocityInput);
					}
					else
					{
						// todo: black/white default is a compositing graph feature, no need to hook up a node

						// black is how we clear the velocity buffer so this means no velocity
						FRenderingCompositePass* NoVelocity = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessInput(GSystemTextures.BlackDummy));
						FRenderingCompositeOutputRef NoVelocityRef(NoVelocity);
						AddPostProcessDepthOfFieldCircle(Context, DepthOfFieldStat, NoVelocityRef);
					}
				}
			}

			bool bBokehDOF = bDepthOfField
				&& View.FinalPostProcessSettings.DepthOfFieldScale > 0
				&& View.FinalPostProcessSettings.DepthOfFieldMethod == DOFM_BokehDOF
				&& !Context.View.Family->EngineShowFlags.VisualizeDOF;

			if(bBokehDOF)
			{
				if (FPostProcessing::HasAlphaChannelSupport())
				{
					UE_LOG(LogRenderer, Log, TEXT("Boked depth of field does not have alpha channel support. Only Circle DOF has."));
				}
				if(VelocityInput.IsValid())
				{
					AddPostProcessDepthOfFieldBokeh(Context, SeparateTranslucency, VelocityInput);
				}
				else
				{
					// todo: black/white default is a compositing graph feature, no need to hook up a node

					// black is how we clear the velocity buffer so this means no velocity
					FRenderingCompositePass* NoVelocity = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessInput(GSystemTextures.BlackDummy));
					FRenderingCompositeOutputRef NoVelocityRef(NoVelocity);
					AddPostProcessDepthOfFieldBokeh(Context, SeparateTranslucency, NoVelocityRef);
				}
				bSepTransWasApplied = true;
			}

			if(SeparateTranslucency.IsValid() && !bSepTransWasApplied)
			{
				checkf(!FPostProcessing::HasAlphaChannelSupport(), TEXT("Separate translucency was supposed to be disabled automatically."));
				const bool bIsComputePass = CVarPostProcessingPreferCompute.GetValueOnRenderThread() && Context.View.FeatureLevel >= ERHIFeatureLevel::SM5;
				// separate translucency is done here or in AddPostProcessDepthOfFieldBokeh()
				FRenderingCompositePass* NodeRecombined = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessBokehDOFRecombine(bIsComputePass));
				NodeRecombined->SetInput(ePId_Input0, Context.FinalOutput);
				NodeRecombined->SetInput(ePId_Input2, SeparateTranslucency);

				Context.FinalOutput = FRenderingCompositeOutputRef(NodeRecombined);
			}

			AddPostProcessMaterial(Context, BL_BeforeTonemapping, SeparateTranslucency);

			EAntiAliasingMethod AntiAliasingMethod = Context.View.AntiAliasingMethod;

			if( AntiAliasingMethod == AAM_TemporalAA && ViewState)
			{
				if(VelocityInput.IsValid())
				{
					AddTemporalAA( Context, VelocityInput );
				}
				else
				{
					// black is how we clear the velocity buffer so this means no velocity
					FRenderingCompositePass* NoVelocity = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessInput(GSystemTextures.BlackDummy));
					FRenderingCompositeOutputRef NoVelocityRef(NoVelocity);
					AddTemporalAA( Context, NoVelocityRef );
				}
			}

			if(IsMotionBlurEnabled(View) && VelocityInput.IsValid() && !bVisualizeMotionBlur)
			{
				// Motion blur

				FRenderingCompositeOutputRef MaxTileVelocity;

				{
					check(!VelocityFlattenPass);
					VelocityFlattenPass = Context.Graph.RegisterPass( new(FMemStack::Get()) FRCPassPostProcessVelocityFlatten() );
					VelocityFlattenPass->SetInput( ePId_Input0, VelocityInput );
					VelocityFlattenPass->SetInput( ePId_Input1, Context.SceneDepth );

					VelocityInput	= FRenderingCompositeOutputRef( VelocityFlattenPass, ePId_Output0 );
					MaxTileVelocity	= FRenderingCompositeOutputRef( VelocityFlattenPass, ePId_Output1 );
				}

				const float SizeX = View.ViewRect.Width();

				// 0:no 1:full screen width, percent conversion
				float MaxVelocity = View.FinalPostProcessSettings.MotionBlurMax / 100.0f;
				float MaxVelocityTiles = MaxVelocity * SizeX * (0.5f / 16.0f);
				float MaxTileDistGathered = 3.0f;
				if( MaxVelocityTiles > MaxTileDistGathered || CVarMotionBlurScatter.GetValueOnRenderThread() || (ViewState && ViewState->bSequencerIsPaused) )
				{
					FRenderingCompositePass* VelocityScatterPass = Context.Graph.RegisterPass( new(FMemStack::Get()) FRCPassPostProcessVelocityScatter() );
					VelocityScatterPass->SetInput( ePId_Input0, MaxTileVelocity );

					MaxTileVelocity	= FRenderingCompositeOutputRef( VelocityScatterPass );
				}
				else
				{
					FRenderingCompositePass* VelocityGatherPass = Context.Graph.RegisterPass( new(FMemStack::Get()) FRCPassPostProcessVelocityGather() );
					VelocityGatherPass->SetInput( ePId_Input0, MaxTileVelocity );

					MaxTileVelocity	= FRenderingCompositeOutputRef( VelocityGatherPass );
				}

				bool bTwoPass = CVarMotionBlurSeparable.GetValueOnRenderThread() != 0;
				{
					const bool bIsComputePass = CVarPostProcessingPreferCompute.GetValueOnRenderThread() && Context.View.FeatureLevel >= ERHIFeatureLevel::SM5;
					FRenderingCompositePass* MotionBlurPass = Context.Graph.RegisterPass( new(FMemStack::Get()) FRCPassPostProcessMotionBlur( GetMotionBlurQualityFromCVar(), bTwoPass ? 0 : -1, bIsComputePass ) );
					MotionBlurPass->SetInput( ePId_Input0, Context.FinalOutput );
					MotionBlurPass->SetInput( ePId_Input1, Context.SceneDepth );
					MotionBlurPass->SetInput( ePId_Input2, VelocityInput );
					MotionBlurPass->SetInput( ePId_Input3, MaxTileVelocity );
					
					Context.FinalOutput = FRenderingCompositeOutputRef( MotionBlurPass );
				}

				if( bTwoPass )
				{
					const bool bIsComputePass = CVarPostProcessingPreferCompute.GetValueOnRenderThread() && Context.View.FeatureLevel >= ERHIFeatureLevel::SM5;
					FRenderingCompositePass* MotionBlurPass = Context.Graph.RegisterPass( new(FMemStack::Get()) FRCPassPostProcessMotionBlur( GetMotionBlurQualityFromCVar(), 1, bIsComputePass ) );
					MotionBlurPass->SetInput( ePId_Input0, Context.FinalOutput );
					MotionBlurPass->SetInput( ePId_Input1, Context.SceneDepth );
					MotionBlurPass->SetInput( ePId_Input2, VelocityInput );
					MotionBlurPass->SetInput( ePId_Input3, MaxTileVelocity );
					
					Context.FinalOutput = FRenderingCompositeOutputRef( MotionBlurPass );
				}
			}

			if(VelocityInput.IsValid() && bVisualizeMotionBlur)
			{
				auto VisualizePass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessVisualizeMotionBlur());
				VisualizePass->SetInput(ePId_Input0, Context.FinalOutput);
				VisualizePass->SetInput(ePId_Input1, Context.SceneDepth);
				VisualizePass->SetInput(ePId_Input2, VelocityInput);

				Context.FinalOutput = FRenderingCompositeOutputRef(VisualizePass);
			}

			if(bVisualizeBloom)
			{
				AddVisualizeBloomSetup(Context);
			}

			// down sample Scene color from full to half res
			FRenderingCompositeOutputRef SceneColorHalfRes;
			{
				// doesn't have to be as high quality as the Scene color
				int32 DownsampleQuality = FMath::Clamp(CDownsampleQuality.GetValueOnRenderThread(), 0, 1);
				const bool bIsComputePass = CVarPostProcessingPreferCompute.GetValueOnRenderThread() && Context.View.FeatureLevel >= ERHIFeatureLevel::SM5;

				FRenderingCompositePass* HalfResPass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessDownsample(PF_FloatRGB, DownsampleQuality, bIsComputePass, TEXT("SceneColorHalfRes")));
				HalfResPass->SetInput(ePId_Input0, FRenderingCompositeOutputRef(Context.FinalOutput));

				SceneColorHalfRes = FRenderingCompositeOutputRef(HalfResPass);
			}

			{
				bool bHistogramNeeded = false;

				if (View.Family->EngineShowFlags.EyeAdaptation && (AutoExposure.MethodId == EAutoExposureMethod::AEM_Histogram)
					&& View.FinalPostProcessSettings.AutoExposureMinBrightness < View.FinalPostProcessSettings.AutoExposureMaxBrightness
					&& !View.bIsSceneCapture // Eye adaption is not available for scene captures.
					&& !bVisualizeBloom)
				{
					bHistogramNeeded = true;
				}

				if(!bAllowTonemapper)
				{
					bHistogramNeeded = false;
				}

				if(View.Family->EngineShowFlags.VisualizeHDR)
				{
					bHistogramNeeded = true;
				}

				if (!GIsHighResScreenshot && bHistogramNeeded && FeatureLevel >= ERHIFeatureLevel::SM5 && StereoPass != eSSP_RIGHT_EYE)
				{
					FRenderingCompositePass* NodeHistogram = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessHistogram());

					NodeHistogram->SetInput(ePId_Input0, SceneColorHalfRes);

					HistogramOverScreen = FRenderingCompositeOutputRef(NodeHistogram);

					FRenderingCompositePass* NodeHistogramReduce = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessHistogramReduce());

					NodeHistogramReduce->SetInput(ePId_Input0, NodeHistogram);

					Histogram = FRenderingCompositeOutputRef(NodeHistogramReduce);
				}
			}

			// Compute DownSamples passes used by bloom, tint and eye-adaptation if possible.
			FBloomDownSampleArray::Ptr BloomAndEyeDownSamplesPtr;
			if (View.FinalPostProcessSettings.BloomIntensity > 0.f) // do bloom
			{
				// No Threshold:  We can share with Eye-Adaptation.
				if (Context.View.FinalPostProcessSettings.BloomThreshold <= -1 && Context.View.Family->Views.Num() == 1)
				{
					if (!GIsHighResScreenshot && View.State &&
						(StereoPass != eSSP_RIGHT_EYE) &&
						(AutoExposure.MethodId == EAutoExposureMethod::AEM_Basic))
					{
						BloomAndEyeDownSamplesPtr = CreateDownSampleArray(Context, SceneColorHalfRes, true /*bGenerateLog2Alpha*/);
					}
				}
			}

			// some views don't have a state (thumbnail rendering)
			if(!GIsHighResScreenshot && View.State && (StereoPass != eSSP_RIGHT_EYE))
			{
				
				const bool bUseBasicEyeAdaptation = (AutoExposure.MethodId == EAutoExposureMethod::AEM_Basic);  

				if (bUseBasicEyeAdaptation) // log average ps reduction ( non histogram ) 
				{
					
					if (!BloomAndEyeDownSamplesPtr.IsValid()) 
					{
						// need downsamples for eye-adaptation.
						FBloomDownSampleArray::Ptr EyeDownSamplesPtr = CreateDownSampleArray(Context, SceneColorHalfRes, true /*bGenerateLog2Alpha*/);
						AutoExposure.EyeAdaptation = AddPostProcessBasicEyeAdaptation(View, *EyeDownSamplesPtr);
					}
					else
					{
						// Use the alpha channel in the last downsample (smallest) to compute eye adaptations values.			
						AutoExposure.EyeAdaptation = AddPostProcessBasicEyeAdaptation(View, *BloomAndEyeDownSamplesPtr);
					}
				}
				else  // Use histogram version version
				{
					// we always add eye adaptation, if the engine show flag is disabled we set the ExposureScale in the texture to a fixed value
					AutoExposure.EyeAdaptation = AddPostProcessHistogramEyeAdaptation(Context, Histogram);
				}
			}

			if(View.FinalPostProcessSettings.BloomIntensity > 0.0f)
			{
				if (CVarUseMobileBloom.GetValueOnRenderThread() == 0)
				{
					if (!BloomAndEyeDownSamplesPtr.IsValid())
					{
						FRenderingCompositeOutputRef HalfResBloomThreshold = RenderHalfResBloomThreshold(Context, SceneColorHalfRes, AutoExposure.EyeAdaptation);
						BloomAndEyeDownSamplesPtr = CreateDownSampleArray(Context, HalfResBloomThreshold, false /*bGenerateLog2Alpha*/);
					}
					BloomOutputCombined = AddBloom(*BloomAndEyeDownSamplesPtr, bVisualizeBloom);
				}
				else
				{
					FIntPoint PrePostSourceViewportSize = View.ViewRect.Size();

					// Bloom.
					FRenderingCompositeOutputRef PostProcessDownsample2;
					FRenderingCompositeOutputRef PostProcessDownsample3;
					FRenderingCompositeOutputRef PostProcessDownsample4;
					FRenderingCompositeOutputRef PostProcessDownsample5;
					FRenderingCompositeOutputRef PostProcessUpsample4;
					FRenderingCompositeOutputRef PostProcessUpsample3;
					FRenderingCompositeOutputRef PostProcessUpsample2;
					FRenderingCompositeOutputRef PostProcessSunMerge;

					float DownScale = 0.66f * 4.0f;
					// Downsample by 2
					{
						FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessBloomDownES2(PrePostSourceViewportSize/4, DownScale));
						Pass->SetInput(ePId_Input0, SceneColorHalfRes);
						PostProcessDownsample2 = FRenderingCompositeOutputRef(Pass);
					}

					// Downsample by 2
					{
						FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessBloomDownES2(PrePostSourceViewportSize/8, DownScale));
						Pass->SetInput(ePId_Input0, PostProcessDownsample2);
						PostProcessDownsample3 = FRenderingCompositeOutputRef(Pass);
					}

					// Downsample by 2
					{
						FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessBloomDownES2(PrePostSourceViewportSize/16, DownScale));
						Pass->SetInput(ePId_Input0, PostProcessDownsample3);
						PostProcessDownsample4 = FRenderingCompositeOutputRef(Pass);
					}

					// Downsample by 2
					{
						FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessBloomDownES2(PrePostSourceViewportSize/32, DownScale));
						Pass->SetInput(ePId_Input0, PostProcessDownsample4);
						PostProcessDownsample5 = FRenderingCompositeOutputRef(Pass);
					}

					const FFinalPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;

					float UpScale = 0.66f * 2.0f;
					// Upsample by 2
					{
						FVector4 TintA = FVector4(Settings.Bloom4Tint.R, Settings.Bloom4Tint.G, Settings.Bloom4Tint.B, 0.0f);
						FVector4 TintB = FVector4(Settings.Bloom5Tint.R, Settings.Bloom5Tint.G, Settings.Bloom5Tint.B, 0.0f);
						TintA *= View.FinalPostProcessSettings.BloomIntensity;
						TintB *= View.FinalPostProcessSettings.BloomIntensity;
						FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessBloomUpES2(PrePostSourceViewportSize/32, FVector2D(UpScale, UpScale), TintA, TintB));
						Pass->SetInput(ePId_Input0, PostProcessDownsample4);
						Pass->SetInput(ePId_Input1, PostProcessDownsample5);
						PostProcessUpsample4 = FRenderingCompositeOutputRef(Pass);
					}

					// Upsample by 2
					{
						FVector4 TintA = FVector4(Settings.Bloom3Tint.R, Settings.Bloom3Tint.G, Settings.Bloom3Tint.B, 0.0f);
						TintA *= View.FinalPostProcessSettings.BloomIntensity;
						FVector4 TintB = FVector4(1.0f, 1.0f, 1.0f, 0.0f);
						FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessBloomUpES2(PrePostSourceViewportSize/16, FVector2D(UpScale, UpScale), TintA, TintB));
						Pass->SetInput(ePId_Input0, PostProcessDownsample3);
						Pass->SetInput(ePId_Input1, PostProcessUpsample4);
						PostProcessUpsample3 = FRenderingCompositeOutputRef(Pass);
					}

					// Upsample by 2
					{
						FVector4 TintA = FVector4(Settings.Bloom2Tint.R, Settings.Bloom2Tint.G, Settings.Bloom2Tint.B, 0.0f);
						TintA *= View.FinalPostProcessSettings.BloomIntensity;
						// Scaling Bloom2 by extra factor to match filter area difference between PC default and mobile.
						TintA *= 0.5;
						FVector4 TintB = FVector4(1.0f, 1.0f, 1.0f, 0.0f);
						FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessBloomUpES2(PrePostSourceViewportSize/8, FVector2D(UpScale, UpScale), TintA, TintB));
						Pass->SetInput(ePId_Input0, PostProcessDownsample2);
						Pass->SetInput(ePId_Input1, PostProcessUpsample3);
						PostProcessUpsample2 = FRenderingCompositeOutputRef(Pass);
					}

					{
						FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessSunMergeES2(PrePostSourceViewportSize));
						Pass->SetInput(ePId_Input1, SceneColorHalfRes);
						Pass->SetInput(ePId_Input2, PostProcessUpsample2);
						PostProcessSunMerge = FRenderingCompositeOutputRef(Pass);
						BloomOutputCombined = PostProcessSunMerge;
					}
				}
			}

			PreTonemapHDRColor = Context.FinalOutput;

			if(bAllowTonemapper)
			{
				auto Node = AddSinglePostProcessMaterial(Context, BL_ReplacingTonemapper);

				if(Node)
				{
					// a custom tonemapper is provided
					Node->SetInput(ePId_Input0, Context.FinalOutput);

					// We are binding separate translucency here because the post process SceneTexture node can reference 
					// the separate translucency buffers through ePId_Input1. 
					// TODO: Check if material actually uses this texture and only bind if needed.
					Node->SetInput(ePId_Input1, SeparateTranslucency);
					Node->SetInput(ePId_Input2, BloomOutputCombined);
					Context.FinalOutput = Node;
				}
				else
				{
					Tonemapper = AddTonemapper(Context, BloomOutputCombined, AutoExposure.EyeAdaptation, AutoExposure.MethodId, false, bHDRTonemapperOutput);
				}

				PostTonemapHDRColor = Context.FinalOutput;

				// Add a pass-through as tonemapper will be forced LDR if final pass in chain 
				if (bHDRTonemapperOutput && !bHDROutputEnabled)
				{
					FRenderingCompositePass* PassthroughNode = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessPassThrough(nullptr));
					PassthroughNode->SetInput(ePId_Input0, FRenderingCompositeOutputRef(Context.FinalOutput));
					Context.FinalOutput = FRenderingCompositeOutputRef(PassthroughNode);
				}
			}

			if(AntiAliasingMethod == AAM_FXAA)
			{
				AddPostProcessAA(Context);
			}
			
			if(bDepthOfField && Context.View.Family->EngineShowFlags.VisualizeDOF)
			{
				FRenderingCompositePass* VisualizeNode = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessVisualizeDOF(DepthOfFieldStat));
				VisualizeNode->SetInput(ePId_Input0, FRenderingCompositeOutputRef(Context.FinalOutput));

				Context.FinalOutput = FRenderingCompositeOutputRef(VisualizeNode);
				bAllowTonemapper = false;
			}
		}
		else
		{
			if (SeparateTranslucency.IsValid())
			{
				const bool bIsComputePass = CVarPostProcessingPreferCompute.GetValueOnRenderThread() && Context.View.FeatureLevel >= ERHIFeatureLevel::SM5;
				// separate translucency is done here or in AddPostProcessDepthOfFieldBokeh()
				FRenderingCompositePass* NodeRecombined = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessBokehDOFRecombine(bIsComputePass));
				NodeRecombined->SetInput(ePId_Input0, Context.FinalOutput);
				NodeRecombined->SetInput(ePId_Input2, SeparateTranslucency);

				Context.FinalOutput = FRenderingCompositeOutputRef(NodeRecombined);
			}

			// Shader complexity does not actually output a color
			if (!View.Family->EngineShowFlags.ShaderComplexity)
			{
				AddGammaOnlyTonemapper(Context);
			}
		}
		
		if(View.Family->EngineShowFlags.StationaryLightOverlap)
		{
			FRenderingCompositePass* Node = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessVisualizeComplexity(GEngine->StationaryLightOverlapColors, FVisualizeComplexityApplyPS::CS_RAMP, 1.f, false));
			Node->SetInput(ePId_Input0, FRenderingCompositeOutputRef(Context.SceneColor));
			Context.FinalOutput = FRenderingCompositeOutputRef(Node);
		}

		const EDebugViewShaderMode DebugViewShaderMode = View.Family->GetDebugViewShaderMode();
		if(DebugViewShaderMode == DVSM_QuadComplexity)
		{
			float ComplexityScale = 1.f / (float)(GEngine->QuadComplexityColors.Num() - 1) / NormalizedQuadComplexityValue; // .1f comes from the values used in LightAccumulator_GetResult
			FRenderingCompositePass* Node = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessVisualizeComplexity(GEngine->QuadComplexityColors, FVisualizeComplexityApplyPS::CS_STAIR, ComplexityScale, true));
			Node->SetInput(ePId_Input0, FRenderingCompositeOutputRef(Context.FinalOutput));
			Context.FinalOutput = FRenderingCompositeOutputRef(Node);
		}

		if(DebugViewShaderMode == DVSM_ShaderComplexity || DebugViewShaderMode == DVSM_ShaderComplexityContainedQuadOverhead || DebugViewShaderMode == DVSM_ShaderComplexityBleedingQuadOverhead)
		{
			FRenderingCompositePass* Node = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessVisualizeComplexity(GEngine->ShaderComplexityColors, FVisualizeComplexityApplyPS::CS_RAMP, 1.f, true));
			Node->SetInput(ePId_Input0, FRenderingCompositeOutputRef(Context.FinalOutput));
			Context.FinalOutput = FRenderingCompositeOutputRef(Node);
		}

		if (DebugViewShaderMode == DVSM_PrimitiveDistanceAccuracy || DebugViewShaderMode == DVSM_MeshUVDensityAccuracy || DebugViewShaderMode == DVSM_MaterialTextureScaleAccuracy ||DebugViewShaderMode == DVSM_RequiredTextureResolution)
		{
			FRenderingCompositePass* Node = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessStreamingAccuracyLegend(GEngine->StreamingAccuracyColors));
			Node->SetInput(ePId_Input0, FRenderingCompositeOutputRef(Context.FinalOutput));
			Context.FinalOutput = FRenderingCompositeOutputRef(Node);
		}

		if(View.Family->EngineShowFlags.VisualizeLightCulling) 
		{
			float ComplexityScale = 1.f / (float)(GEngine->LightComplexityColors.Num() - 1) / .1f; // .1f comes from the values used in LightAccumulator_GetResult
			FRenderingCompositePass* Node = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessVisualizeComplexity(GEngine->LightComplexityColors, FVisualizeComplexityApplyPS::CS_LINEAR,  ComplexityScale, false));
			Node->SetInput(ePId_Input0, FRenderingCompositeOutputRef(Context.SceneColor));
			Context.FinalOutput = FRenderingCompositeOutputRef(Node);
		}

		if(View.Family->EngineShowFlags.VisualizeLPV && !View.Family->EngineShowFlags.VisualizeHDR)
		{
			FRenderingCompositePass* Node = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessVisualizeLPV());
			Node->SetInput(ePId_Input0, Context.FinalOutput);
			Context.FinalOutput = FRenderingCompositeOutputRef(Node);
		}

#if WITH_EDITOR
		// Show the selection outline if it is in the editor and we aren't in wireframe 
		// If the engine is in demo mode and game view is on we also do not show the selection outline
		if ( GIsEditor
			&& View.Family->EngineShowFlags.SelectionOutline
			&& !(View.Family->EngineShowFlags.Wireframe)
			&& !bVisualizeBloom
			&& !View.Family->EngineShowFlags.VisualizeHDR)
		{
			// Selection outline is after bloom, but before AA
			AddSelectionOutline(Context);
		}

		// Composite editor primitives if we had any to draw and compositing is enabled
		if (FSceneRenderer::ShouldCompositeEditorPrimitives(View) && !bVisualizeBloom)
		{
			FRenderingCompositePass* Node = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessCompositeEditorPrimitives(true));
			Node->SetInput(ePId_Input0, FRenderingCompositeOutputRef(Context.FinalOutput));
			Context.FinalOutput = FRenderingCompositeOutputRef(Node);
		}
#endif
		if(View.Family->EngineShowFlags.VisualizeShadingModels && FeatureLevel >= ERHIFeatureLevel::SM4)
		{
			FRenderingCompositePass* Node = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessVisualizeShadingModels(RHICmdList));
			Node->SetInput(ePId_Input0, FRenderingCompositeOutputRef(Context.FinalOutput));
			Context.FinalOutput = FRenderingCompositeOutputRef(Node);
		}

		if (View.Family->EngineShowFlags.GBufferHints && FeatureLevel >= ERHIFeatureLevel::SM4)
		{
			FRenderingCompositePass* Node = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessGBufferHints(RHICmdList));
			Node->SetInput(ePId_Input0, FRenderingCompositeOutputRef(Context.FinalOutput));
			// Ideally without lighting as we want the emissive, we should do that later.
			Node->SetInput(ePId_Input1, FRenderingCompositeOutputRef(Context.SceneColor));
			Context.FinalOutput = FRenderingCompositeOutputRef(Node);
		}
		
		AddPostProcessMaterial(Context, BL_AfterTonemapping, SeparateTranslucency, PreTonemapHDRColor, PostTonemapHDRColor);

#if WITH_EDITOR
		//Inspect the Final color, GBuffer and HDR
		//No more postprocess Final color should be the real one
		//The HDR was save before the tonemapping
		//GBuffer should not be change during post process 
		if (View.bUsePixelInspector && FeatureLevel >= ERHIFeatureLevel::SM4)
		{
			FRenderingCompositePass* Node = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessBufferInspector(RHICmdList));
			Node->SetInput(ePId_Input0, Context.FinalOutput);
			Node->SetInput(ePId_Input1, PreTonemapHDRColor);
			Node->SetInput(ePId_Input2, Context.SceneColor);
			Context.FinalOutput = FRenderingCompositeOutputRef(Node);
		}
#endif //WITH_EDITOR

		if(bVisualizeBloom)
		{
			AddVisualizeBloomOverlay(Context, PreTonemapHDRColor, BloomOutputCombined);
		}

		if (View.Family->EngineShowFlags.VisualizeSSS)
		{
			// the setup pass also does visualization, based on EngineShowFlags.VisualizeSSS
			FRenderingCompositePass* PassVisualize = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessSubsurfaceVisualize(RHICmdList));
			PassVisualize->SetInput(ePId_Input0, Context.FinalOutput);
			Context.FinalOutput = FRenderingCompositeOutputRef(PassVisualize);
		}

		AddGBufferVisualizationOverview(Context, SeparateTranslucency, PreTonemapHDRColor, PostTonemapHDRColor);

		if (bStereoRenderingAndHMD)
		{
			FRenderingCompositePass* Node = NULL;
			const EHMDDeviceType::Type DeviceType = GEngine->XRSystem->GetHMDDevice() ? GEngine->XRSystem->GetHMDDevice()->GetHMDDeviceType() : EHMDDeviceType::DT_ES2GenericStereoMesh;
			if((DeviceType == EHMDDeviceType::DT_OculusRift) || (DeviceType == EHMDDeviceType::DT_GoogleVR))
			{
				Node = Context.Graph.RegisterPass(new FRCPassPostProcessHMD());
			}
			else if(DeviceType == EHMDDeviceType::DT_Morpheus && GEngine->StereoRenderingDevice->IsStereoEnabled())
			{
				
#if defined(MORPHEUS_ENGINE_DISTORTION) && MORPHEUS_ENGINE_DISTORTION
				FRCPassPostProcessMorpheus* MorpheusPass = new FRCPassPostProcessMorpheus();
				MorpheusPass->SetInput(ePId_Input0, FRenderingCompositeOutputRef(Context.FinalOutput));
				Node = Context.Graph.RegisterPass(MorpheusPass);
#endif
			}

			if(Node)
			{
				Node->SetInput(ePId_Input0, FRenderingCompositeOutputRef(Context.FinalOutput));
				Context.FinalOutput = FRenderingCompositeOutputRef(Node);
			}
		}

		if(bVisualizeHDR)
		{
			FRenderingCompositePass* Node = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessVisualizeHDR());
			Node->SetInput(ePId_Input0, FRenderingCompositeOutputRef(Context.FinalOutput));
			Node->SetInput(ePId_Input1, Histogram);
			Node->SetInput(ePId_Input2, PreTonemapHDRColor);
			Node->SetInput(ePId_Input3, HistogramOverScreen);
			Node->AddDependency(AutoExposure.EyeAdaptation);

			Context.FinalOutput = FRenderingCompositeOutputRef(Node);
		}

		if(View.Family->EngineShowFlags.TestImage && FeatureLevel >= ERHIFeatureLevel::SM4)
		{
			FRenderingCompositePass* Node = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessTestImage());
			Node->SetInput(ePId_Input0, FRenderingCompositeOutputRef(Context.FinalOutput));
			Context.FinalOutput = FRenderingCompositeOutputRef(Node);
		}

		AddHighResScreenshotMask(Context, SeparateTranslucency);

		if(bDoScreenPercentage)
		{
			// Check if we can save the Upscale pass and do it in the Tonemapper to save performance
			if(Tonemapper && !PaniniConfig.IsEnabled() && !Tonemapper->bDoGammaOnly)
			{
				if (Context.FinalOutput.GetPass() == Tonemapper)
				{
					const int32 TonemapperMergeMode = CVarTonemapperMergeMode.GetValueOnRenderThread();
					bool bCombineTonemapperAndUpsample = false;

					if (TonemapperMergeMode == 1)
					{
						bCombineTonemapperAndUpsample = true;
					}
					else if (TonemapperMergeMode == 2)
					{
						const float TonemapperMergeThreshold = CVarTonemapperMergeThreshold.GetValueOnRenderThread();
						const float AreaRatio = View.ViewRect.Area() / (float)View.UnscaledViewRect.Area();
						bCombineTonemapperAndUpsample = AreaRatio > TonemapperMergeThreshold;
					}

					if (bCombineTonemapperAndUpsample)
					{
						Tonemapper->bDoScreenPercentageInTonemapper = true;
						// the following pass is no longer needed
						bDoScreenPercentage = false;
					}
				}
			}

			if (PaniniConfig.IsEnabled() || bDoScreenPercentage)
			{
				int32 UpscaleQuality = CVarUpscaleQuality.GetValueOnRenderThread();
				UpscaleQuality = FMath::Clamp(UpscaleQuality, 0, 5);
				FRenderingCompositePass* Node = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessUpscale(View, UpscaleQuality, PaniniConfig));
				Node->SetInput(ePId_Input0, FRenderingCompositeOutputRef(Context.FinalOutput)); // Bilinear sampling.
				Node->SetInput(ePId_Input1, FRenderingCompositeOutputRef(Context.FinalOutput)); // Point sampling.
				Context.FinalOutput = FRenderingCompositeOutputRef(Node);
			}
		}

		// After the graph is built but before the graph is processed.
		// If a postprocess material is using a GBuffer it adds the refcount int FRCPassPostProcessMaterial::Process()
		// and when it gets processed it removes the refcount
		// We only release the GBuffers after the last view was processed (SplitScreen)
		if(View.Family->Views[View.Family->Views.Num() - 1] == &View)
		{
			// Generally we no longer need the GBuffers, anyone that wants to keep the GBuffers for longer should have called AdjustGBufferRefCount(1) to keep it for longer
			// and call AdjustGBufferRefCount(-1) once it's consumed. This needs to happen each frame. PostProcessMaterial do that automatically
			FSceneRenderTargets::Get(RHICmdList).AdjustGBufferRefCount(RHICmdList, -1);
		}

		// Add a pass-through for the final step if a backbuffer UAV is required but unsupported by this RHI
		if (Context.FinalOutput.IsComputePass() && !View.Family->RenderTarget->GetRenderTargetUAV().IsValid())
		{
			FRenderingCompositePass* PassthroughNode = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessPassThrough(nullptr));
			PassthroughNode->SetInput(ePId_Input0, FRenderingCompositeOutputRef(Context.FinalOutput));
			Context.FinalOutput = FRenderingCompositeOutputRef(PassthroughNode);
		}

		// The graph setup should be finished before this line ----------------------------------------
		{
			// currently created on the heap each frame but View.Family->RenderTarget could keep this object and all would be cleaner
			TRefCountPtr<IPooledRenderTarget> Temp;
			FSceneRenderTargetItem Item;
			Item.TargetableTexture = (FTextureRHIRef&)View.Family->RenderTarget->GetRenderTargetTexture();
			Item.ShaderResourceTexture = (FTextureRHIRef&)View.Family->RenderTarget->GetRenderTargetTexture();
			Item.UAV = View.Family->RenderTarget->GetRenderTargetUAV();

			FPooledRenderTargetDesc Desc;

			// Texture could be bigger than viewport
			if (View.Family->RenderTarget->GetRenderTargetTexture())
			{
				Desc.Extent.X = View.Family->RenderTarget->GetRenderTargetTexture()->GetSizeX();
				Desc.Extent.Y = View.Family->RenderTarget->GetRenderTargetTexture()->GetSizeY();
			}
			else
			{
				Desc.Extent = View.Family->RenderTarget->GetSizeXY();
			}

			const bool bIsFinalOutputComputePass = Context.FinalOutput.IsComputePass();
			Desc.TargetableFlags |= bIsFinalOutputComputePass ? TexCreate_UAV : TexCreate_RenderTargetable;
			Desc.Format = bIsFinalOutputComputePass ? PF_R8G8B8A8 : PF_B8G8R8A8;

			// todo: this should come from View.Family->RenderTarget
			Desc.Format = bHDROutputEnabled ? GRHIHDRDisplayOutputFormat : Desc.Format;
			Desc.NumMips = 1;
			Desc.DebugName = TEXT("FinalPostProcessColor");

			GRenderTargetPool.CreateUntrackedElement(Desc, Temp, Item);

			OverrideRenderTarget(Context.FinalOutput, Temp, Desc);

			// execute the graph/DAG
			CompositeContext.Process(Context.FinalOutput.GetPass(), TEXT("PostProcessing"));

			// May need to wait on the final pass to complete
			if (Context.FinalOutput.IsAsyncComputePass())
			{
				FComputeFenceRHIParamRef ComputeFinalizeFence = Context.FinalOutput.GetComputePassEndFence();
				if (ComputeFinalizeFence)
				{
					Context.RHICmdList.WaitComputeFence(ComputeFinalizeFence);
				}
			}
		}
	}

	GRenderTargetPool.AddPhaseEvent(TEXT("AfterPostprocessing"));
}

static bool IsGaussianActive(FPostprocessContext& Context)
{

	float FarSize = Context.View.FinalPostProcessSettings.DepthOfFieldFarBlurSize;
	float NearSize = Context.View.FinalPostProcessSettings.DepthOfFieldNearBlurSize;

	float MaxSize = CVarDepthOfFieldMaxSize.GetValueOnRenderThread();

	FarSize = FMath::Min(FarSize, MaxSize);
	NearSize = FMath::Min(NearSize, MaxSize);
	const float CVarThreshold = CVarDepthOfFieldNearBlurSizeThreshold.GetValueOnRenderThread();

	if ((FarSize < 0.01f) && (NearSize < CVarThreshold))
	{
		return false;
	}
	return true;
}

void FPostProcessing::ProcessES2(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, bool bUsedFramebufferFetch)
{
	check(IsInRenderingThread());

	// This page: https://udn.epicgames.com/Three/RenderingOverview#Rendering%20state%20defaults 
	// describes what state a pass can expect and to what state it need to be set back.

	// All post processing is happening on the render thread side. All passes can access FinalPostProcessSettings and all
	// view settings. Those are copies for the RT then never get access by the main thread again.
	// Pointers to other structures might be unsafe to touch.


	// so that the passes can register themselves to the graph
	{
		FMemMark Mark(FMemStack::Get());
		FRenderingCompositePassContext CompositeContext(RHICmdList, View);

		FPostprocessContext Context(RHICmdList, CompositeContext.Graph, View);
		FRenderingCompositeOutputRef BloomOutput;
		FRenderingCompositeOutputRef DofOutput;

		bool bUseAa = View.AntiAliasingMethod == AAM_TemporalAA;

		// AA with Mobile32bpp mode requires this outside of bUsePost.
		if(bUseAa)
		{
			// Handle pointer swap for double buffering.
			FSceneViewState* ViewState = (FSceneViewState*)View.State;
			if(ViewState)
			{
				// Note that this drops references to the render targets from two frames ago. This
				// causes them to be added back to the pool where we can grab them again.
				ViewState->MobileAaBloomSunVignette1 = ViewState->MobileAaBloomSunVignette0;
				ViewState->MobileAaColor1 = ViewState->MobileAaColor0;
			}
		}

		const FIntPoint FinalTargetSize = View.Family->RenderTarget->GetSizeXY();
		FIntRect FinalOutputViewRect = View.ViewRect;
		FIntPoint PrePostSourceViewportSize = View.ViewRect.Size();
		// ES2 preview uses a subsection of the scene RT, bUsedFramebufferFetch == true deals with this case.  
		FIntPoint SceneColorSize = FSceneRenderTargets::Get(RHICmdList).GetBufferSizeXY();
		bool bViewRectSource = bUsedFramebufferFetch || SceneColorSize != PrePostSourceViewportSize;
		bool bMobileHDR32bpp = IsMobileHDR32bpp();

		// temporary solution for SP_METAL using HW sRGB flag during read vs all other mob platforms using
		// incorrect UTexture::SRGB state. (UTexture::SRGB != HW texture state)
		bool bSRGBAwareTarget = View.Family->RenderTarget->GetDisplayGamma() == 1.0f
			&& View.bIsSceneCapture
			&& View.GetShaderPlatform() == EShaderPlatform::SP_METAL;

		// add the passes we want to add to the graph (commenting a line means the pass is not inserted into the graph) ---------
		if( View.Family->EngineShowFlags.PostProcessing )
		{
			const EMobileHDRMode HDRMode = GetMobileHDRMode();
			bool bUseEncodedHDR = HDRMode == EMobileHDRMode::EnabledRGBE;
			bool bHDRModeAllowsPost = bUseEncodedHDR || HDRMode == EMobileHDRMode::EnabledFloat16;

			bool bUseSun = !bUseEncodedHDR && View.bLightShaftUse;
			bool bUseDof = !bUseEncodedHDR && GetMobileDepthOfFieldScale(View) > 0.0f && !Context.View.Family->EngineShowFlags.VisualizeDOF;
			bool bUseBloom = View.FinalPostProcessSettings.BloomIntensity > 0.0f;
			bool bUseVignette = View.FinalPostProcessSettings.VignetteIntensity > 0.0f;

			bool bWorkaround = CVarRenderTargetSwitchWorkaround.GetValueOnRenderThread() != 0;

			// Use original mobile Dof on ES2 devices regardless of bMobileHQGaussian.
			// HQ gaussian 
			bool bUseMobileDof = bUseDof && (!View.FinalPostProcessSettings.bMobileHQGaussian || (Context.View.GetFeatureLevel() < ERHIFeatureLevel::ES3_1));

			// This is a workaround to avoid a performance cliff when using many render targets. 
			bool bUseBloomSmall = bUseBloom && !bUseSun && !bUseDof && bWorkaround;

			bool bUsePost = bUseSun | bUseDof | bUseBloom | bUseVignette;

			// Post is not supported on ES2 devices using mosaic.
			bUsePost &= bHDRModeAllowsPost;
			bUsePost &= IsMobileHDR();

			if(bUsePost)
			{
				AddPostProcessMaterial(Context, BL_BeforeTranslucency, nullptr);
				AddPostProcessMaterial(Context, BL_BeforeTonemapping, nullptr);
						
				// Skip this pass if the pass was done prior before resolve.
				if ((!bUsedFramebufferFetch) && (bUseSun || bUseDof))
				{
					// Convert depth to {circle of confusion, sun shaft intensity} before resolve.
				//	FRenderingCompositePass* PostProcessSunMask = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessSunMaskES2(PrePostSourceViewportSize, false));
					FRenderingCompositePass* PostProcessSunMask = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessSunMaskES2(SceneColorSize, false));
					PostProcessSunMask->SetInput(ePId_Input0, Context.FinalOutput);
					Context.FinalOutput = FRenderingCompositeOutputRef(PostProcessSunMask);
					//@todo Ronin sunmask pass isnt clipping to image only.
				}

				FRenderingCompositeOutputRef PostProcessBloomSetup;
				if (bUseSun || bUseMobileDof || bUseBloom)
				{
					if(bUseBloomSmall)
					{
						FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessBloomSetupSmallES2(PrePostSourceViewportSize, bViewRectSource));
						Pass->SetInput(ePId_Input0, Context.FinalOutput);
						PostProcessBloomSetup = FRenderingCompositeOutputRef(Pass);
					}
					else
					{
						FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessBloomSetupES2(FinalOutputViewRect, bViewRectSource));
						Pass->SetInput(ePId_Input0, Context.FinalOutput);
						PostProcessBloomSetup = FRenderingCompositeOutputRef(Pass);
					}
				}

				if (bUseDof)
				{
					if (bUseMobileDof)
					{
						// Near dilation circle of confusion size.
						// Samples at 1/16 area, writes to 1/16 area.
						FRenderingCompositeOutputRef PostProcessNear;
						{
							FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessDofNearES2(FinalOutputViewRect.Size()));
							Pass->SetInput(ePId_Input0, PostProcessBloomSetup);
							PostProcessNear = FRenderingCompositeOutputRef(Pass);
						}

						// DOF downsample pass.
						// Samples at full resolution, writes to 1/4 area.
						FRenderingCompositeOutputRef PostProcessDofDown;
						{
							FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessDofDownES2(FinalOutputViewRect, bViewRectSource));
							Pass->SetInput(ePId_Input0, Context.FinalOutput);
							Pass->SetInput(ePId_Input1, PostProcessNear);
							PostProcessDofDown = FRenderingCompositeOutputRef(Pass);
						}

						// DOF blur pass.
						// Samples at 1/4 area, writes to 1/4 area.
						FRenderingCompositeOutputRef PostProcessDofBlur;
						{
							FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessDofBlurES2(FinalOutputViewRect.Size()));
							Pass->SetInput(ePId_Input0, PostProcessDofDown);
							Pass->SetInput(ePId_Input1, PostProcessNear);
							PostProcessDofBlur = FRenderingCompositeOutputRef(Pass);
							DofOutput = PostProcessDofBlur;
						}
					}
					else
					{
						// black is how we clear the velocity buffer so this means no velocity
						FRenderingCompositePass* NoVelocity = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessInput(GSystemTextures.BlackDummy));
						FRenderingCompositeOutputRef NoVelocityRef(NoVelocity);
						if(View.FinalPostProcessSettings.DepthOfFieldMethod == DOFM_Gaussian && IsGaussianActive(Context))
						{
							FDepthOfFieldStats DepthOfFieldStat;
							FRenderingCompositeOutputRef DummySeparateTranslucency;
							AddPostProcessDepthOfFieldGaussian(Context, DepthOfFieldStat, NoVelocityRef, DummySeparateTranslucency);
						}
					}
				}

				// Bloom.
				FRenderingCompositeOutputRef PostProcessDownsample2;
				FRenderingCompositeOutputRef PostProcessDownsample3;
				FRenderingCompositeOutputRef PostProcessDownsample4;
				FRenderingCompositeOutputRef PostProcessDownsample5;
				FRenderingCompositeOutputRef PostProcessUpsample4;
				FRenderingCompositeOutputRef PostProcessUpsample3;
				FRenderingCompositeOutputRef PostProcessUpsample2;

				if(bUseBloomSmall)
				{
					float DownScale = 0.66f * 4.0f;
					// Downsample by 2
					{
						FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessBloomDownES2(PrePostSourceViewportSize/4, DownScale * 2.0f));
						Pass->SetInput(ePId_Input0, PostProcessBloomSetup);
						PostProcessDownsample2 = FRenderingCompositeOutputRef(Pass);
					}
				}

				if(bUseBloom && (!bUseBloomSmall))
				{
					float DownScale = 0.66f * 4.0f;
					// Downsample by 2
					{
						FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessBloomDownES2(PrePostSourceViewportSize/4, DownScale));
						Pass->SetInput(ePId_Input0, PostProcessBloomSetup);
						PostProcessDownsample2 = FRenderingCompositeOutputRef(Pass);
					}

					// Downsample by 2
					{
						FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessBloomDownES2(PrePostSourceViewportSize/8, DownScale));
						Pass->SetInput(ePId_Input0, PostProcessDownsample2);
						PostProcessDownsample3 = FRenderingCompositeOutputRef(Pass);
					}

					// Downsample by 2
					{
						FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessBloomDownES2(PrePostSourceViewportSize/16, DownScale));
						Pass->SetInput(ePId_Input0, PostProcessDownsample3);
						PostProcessDownsample4 = FRenderingCompositeOutputRef(Pass);
					}

					// Downsample by 2
					{
						FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessBloomDownES2(PrePostSourceViewportSize/32, DownScale));
						Pass->SetInput(ePId_Input0, PostProcessDownsample4);
						PostProcessDownsample5 = FRenderingCompositeOutputRef(Pass);
					}

					const FFinalPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;

					float UpScale = 0.66f * 2.0f;
					// Upsample by 2
					{
						FVector4 TintA = FVector4(Settings.Bloom4Tint.R, Settings.Bloom4Tint.G, Settings.Bloom4Tint.B, 0.0f);
						FVector4 TintB = FVector4(Settings.Bloom5Tint.R, Settings.Bloom5Tint.G, Settings.Bloom5Tint.B, 0.0f);
						TintA *= View.FinalPostProcessSettings.BloomIntensity;
						TintB *= View.FinalPostProcessSettings.BloomIntensity;
						FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessBloomUpES2(PrePostSourceViewportSize/32, FVector2D(UpScale, UpScale), TintA, TintB));
						Pass->SetInput(ePId_Input0, PostProcessDownsample4);
						Pass->SetInput(ePId_Input1, PostProcessDownsample5);
						PostProcessUpsample4 = FRenderingCompositeOutputRef(Pass);
					}

					// Upsample by 2
					{
						FVector4 TintA = FVector4(Settings.Bloom3Tint.R, Settings.Bloom3Tint.G, Settings.Bloom3Tint.B, 0.0f);
						TintA *= View.FinalPostProcessSettings.BloomIntensity;
						FVector4 TintB = FVector4(1.0f, 1.0f, 1.0f, 0.0f);
						FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessBloomUpES2(PrePostSourceViewportSize/16, FVector2D(UpScale, UpScale), TintA, TintB));
						Pass->SetInput(ePId_Input0, PostProcessDownsample3);
						Pass->SetInput(ePId_Input1, PostProcessUpsample4);
						PostProcessUpsample3 = FRenderingCompositeOutputRef(Pass);
					}

					// Upsample by 2
					{
						FVector4 TintA = FVector4(Settings.Bloom2Tint.R, Settings.Bloom2Tint.G, Settings.Bloom2Tint.B, 0.0f);
						TintA *= View.FinalPostProcessSettings.BloomIntensity;
						// Scaling Bloom2 by extra factor to match filter area difference between PC default and mobile.
						TintA *= 0.5;
						FVector4 TintB = FVector4(1.0f, 1.0f, 1.0f, 0.0f);
						FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessBloomUpES2(PrePostSourceViewportSize/8, FVector2D(UpScale, UpScale), TintA, TintB));
						Pass->SetInput(ePId_Input0, PostProcessDownsample2);
						Pass->SetInput(ePId_Input1, PostProcessUpsample3);
						PostProcessUpsample2 = FRenderingCompositeOutputRef(Pass);
					}
				}

				FRenderingCompositeOutputRef PostProcessSunBlur;
				if(bUseSun)
				{
					// Sunshaft depth blur using downsampled alpha.
					FRenderingCompositeOutputRef PostProcessSunAlpha;
					{
						FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessSunAlphaES2(PrePostSourceViewportSize));
						Pass->SetInput(ePId_Input0, PostProcessBloomSetup);
						PostProcessSunAlpha = FRenderingCompositeOutputRef(Pass);
					}

					// Sunshaft blur number two.
					{
						FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessSunBlurES2(PrePostSourceViewportSize));
						Pass->SetInput(ePId_Input0, PostProcessSunAlpha);
						PostProcessSunBlur = FRenderingCompositeOutputRef(Pass);
					}
				}

				if(bUseSun | bUseVignette | bUseBloom)
				{
					FRenderingCompositeOutputRef PostProcessSunMerge;
					if(bUseBloomSmall) 
					{
						FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessSunMergeSmallES2(PrePostSourceViewportSize));
						Pass->SetInput(ePId_Input0, PostProcessBloomSetup);
						Pass->SetInput(ePId_Input1, PostProcessDownsample2);
						PostProcessSunMerge = FRenderingCompositeOutputRef(Pass);
						BloomOutput = PostProcessSunMerge;
					}
					else
					{
						FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessSunMergeES2(PrePostSourceViewportSize));
						if(bUseSun)
						{
							Pass->SetInput(ePId_Input0, PostProcessSunBlur);
						}
						if(bUseBloom)
						{
							Pass->SetInput(ePId_Input1, PostProcessBloomSetup);
							Pass->SetInput(ePId_Input2, PostProcessUpsample2);
						}
						PostProcessSunMerge = FRenderingCompositeOutputRef(Pass);
						BloomOutput = PostProcessSunMerge;
					}

					// Mobile temporal AA requires a composite of two of these frames.
					if(bUseAa && (bUseBloom || bUseSun))
					{
						FSceneViewState* ViewState = (FSceneViewState*)View.State;
						FRenderingCompositeOutputRef PostProcessSunMerge2;
						if(ViewState && ViewState->MobileAaBloomSunVignette1)
						{
							FRenderingCompositePass* History;
							History = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessInput(ViewState->MobileAaBloomSunVignette1));
							PostProcessSunMerge2 = FRenderingCompositeOutputRef(History);
						}
						else
						{
							PostProcessSunMerge2 = PostProcessSunMerge;
						}

						FRenderingCompositeOutputRef PostProcessSunAvg;
						{
							FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessSunAvgES2(PrePostSourceViewportSize));
							Pass->SetInput(ePId_Input0, PostProcessSunMerge);
							Pass->SetInput(ePId_Input1, PostProcessSunMerge2);
							PostProcessSunAvg = FRenderingCompositeOutputRef(Pass);
						}
						BloomOutput = PostProcessSunAvg;
					}
				}
			} // bUsePost

			// mobile separate translucency 
			if (IsMobileSeparateTranslucencyActive(Context.View))
			{
				FRCSeparateTranslucensyPassES2* Pass = (FRCSeparateTranslucensyPassES2*)Context.Graph.RegisterPass(new(FMemStack::Get()) FRCSeparateTranslucensyPassES2());
				Pass->SetInput(ePId_Input0, Context.FinalOutput);
				Context.FinalOutput = FRenderingCompositeOutputRef(Pass);
			}
		}
		
		static const auto VarTonemapperFilm = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.TonemapperFilm"));
		const bool bUseTonemapperFilm = IsMobileHDR() && !bMobileHDR32bpp && GSupportsRenderTargetFormat_PF_FloatRGBA && (VarTonemapperFilm && VarTonemapperFilm->GetValueOnRenderThread());


		static const auto VarTonemapperUpscale = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileTonemapperUpscale"));
		bool bDisableUpscaleInTonemapper = Context.View.Family->bUseSeparateRenderTarget || IsMobileHDRMosaic() || !VarTonemapperUpscale || VarTonemapperUpscale->GetValueOnRenderThread() == 0;

		bool* DoScreenPercentageInTonemapperPtr = nullptr;
		if (bUseTonemapperFilm)
		{
			//@todo Ronin Set to EAutoExposureMethod::AEM_Basic for PC vk crash.
			FRCPassPostProcessTonemap* PostProcessTonemap = AddTonemapper(Context, BloomOutput, nullptr, EAutoExposureMethod::AEM_Histogram, false, false);
			DoScreenPercentageInTonemapperPtr = &PostProcessTonemap->bDoScreenPercentageInTonemapper;
		}
		else
		{
			// Must run to blit to back buffer even if post processing is off.
			FRCPassPostProcessTonemapES2* PostProcessTonemap = (FRCPassPostProcessTonemapES2*)Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessTonemapES2(Context.View, bViewRectSource, bSRGBAwareTarget));
			PostProcessTonemap->SetInput(ePId_Input0, Context.FinalOutput);
			PostProcessTonemap->SetInput(ePId_Input1, BloomOutput);
			PostProcessTonemap->SetInput(ePId_Input2, DofOutput);
			Context.FinalOutput = FRenderingCompositeOutputRef(PostProcessTonemap);
			DoScreenPercentageInTonemapperPtr = &PostProcessTonemap->bDoScreenPercentageInTonemapper;
		}

		// remember the tonemapper pass so we can check if it's last
		FRenderingCompositePass* TonemapperPass = Context.FinalOutput.GetPass();
		
		// if Context.FinalOutput was the clipped result of sunmask stage then this stage also restores Context.FinalOutput back original target size.
		FinalOutputViewRect = View.UnscaledViewRect;

		if (View.Family->EngineShowFlags.PostProcessing)
		{
			if (IsMobileHDR() && !IsMobileHDRMosaic())
			{
				AddPostProcessMaterial(Context, BL_AfterTonemapping, nullptr);

				// Tonemapper is not the final pass so if we may need to use a separate upscale pass
				if (Context.FinalOutput.GetPass() != TonemapperPass)
				{
					bDisableUpscaleInTonemapper = true;
				}
			}

			if (bUseAa)
			{
				// Double buffer post output.
				FSceneViewState* ViewState = (FSceneViewState*)View.State;

				FRenderingCompositeOutputRef PostProcessPrior = Context.FinalOutput;
				if(ViewState && ViewState->MobileAaColor1)
				{
					FRenderingCompositePass* History;
					History = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessInput(ViewState->MobileAaColor1));
					PostProcessPrior = FRenderingCompositeOutputRef(History);
				}

				// Mobile temporal AA is done after tonemapping.
				FRenderingCompositePass* PostProcessAa = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessAaES2());
				PostProcessAa->SetInput(ePId_Input0, Context.FinalOutput);
				PostProcessAa->SetInput(ePId_Input1, PostProcessPrior);
				Context.FinalOutput = FRenderingCompositeOutputRef(PostProcessAa);
			}
		}

		// Screenshot mask
		{
			FRenderingCompositeOutputRef EmptySeparateTranslucency;
			AddHighResScreenshotMask(Context, EmptySeparateTranslucency);
		}
		
	
#if WITH_EDITOR
		// Show the selection outline if it is in the editor and we aren't in wireframe 
		// If the engine is in demo mode and game view is on we also do not show the selection outline
		if ( GIsEditor
			&& View.Family->EngineShowFlags.SelectionOutline
			&& !(View.Family->EngineShowFlags.Wireframe)
			)
		{
			// Editor selection outline
			AddSelectionOutline(Context);
		}

		if (FSceneRenderer::ShouldCompositeEditorPrimitives(View))
		{
			FRenderingCompositePass* EditorCompNode = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessCompositeEditorPrimitives(false));
			EditorCompNode->SetInput(ePId_Input0, FRenderingCompositeOutputRef(Context.FinalOutput));
			Context.FinalOutput = FRenderingCompositeOutputRef(EditorCompNode);
		}
#endif

		// Apply ScreenPercentage
		if (View.UnscaledViewRect != View.ViewRect)
		{
			if (bDisableUpscaleInTonemapper)
			{
				FRenderingCompositePass* Node = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessUpscaleES2(View));
				Node->SetInput(ePId_Input0, FRenderingCompositeOutputRef(Context.FinalOutput)); // Bilinear sampling.
				Node->SetInput(ePId_Input1, FRenderingCompositeOutputRef(Context.FinalOutput)); // Point sampling.
				Context.FinalOutput = FRenderingCompositeOutputRef(Node);
				*DoScreenPercentageInTonemapperPtr = false;
			}
			else
			{
				check(DoScreenPercentageInTonemapperPtr != nullptr);
				*DoScreenPercentageInTonemapperPtr = true;
			}
		}
		else
		{
			*DoScreenPercentageInTonemapperPtr = false;
		}


		const EDebugViewShaderMode DebugViewShaderMode = View.Family->GetDebugViewShaderMode();
		if(DebugViewShaderMode == DVSM_QuadComplexity)
		{
			// Legend is costly so we don't do it for ES2, ideally we make a shader permutation
			FRenderingCompositePass* Node = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessVisualizeComplexity(GEngine->QuadComplexityColors, FVisualizeComplexityApplyPS::CS_STAIR,  1.f, false));
			Node->SetInput(ePId_Input0, FRenderingCompositeOutputRef(Context.FinalOutput));
			Context.FinalOutput = FRenderingCompositeOutputRef(Node);
		}

		if(DebugViewShaderMode == DVSM_ShaderComplexity || DebugViewShaderMode == DVSM_ShaderComplexityContainedQuadOverhead || DebugViewShaderMode == DVSM_ShaderComplexityBleedingQuadOverhead)
		{
			// Legend is costly so we don't do it for ES2, ideally we make a shader permutation
			FRenderingCompositePass* Node = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessVisualizeComplexity(GEngine->ShaderComplexityColors, FVisualizeComplexityApplyPS::CS_RAMP,  1.f, false));
			Node->SetInput(ePId_Input0, FRenderingCompositeOutputRef(Context.FinalOutput));
			Context.FinalOutput = FRenderingCompositeOutputRef(Node);
		}

		bool bStereoRenderingAndHMD = View.Family->EngineShowFlags.StereoRendering && View.Family->EngineShowFlags.HMDDistortion;
		if (bStereoRenderingAndHMD)
		{
			FRenderingCompositePass* Node = NULL;
			const EHMDDeviceType::Type DeviceType = GEngine->XRSystem->GetHMDDevice() ? GEngine->XRSystem->GetHMDDevice()->GetHMDDeviceType() : EHMDDeviceType::DT_ES2GenericStereoMesh;
			if (DeviceType == EHMDDeviceType::DT_ES2GenericStereoMesh)
			{
				Node = Context.Graph.RegisterPass(new FRCPassPostProcessHMD());
			}

			if (Node)
			{
				Node->SetInput(ePId_Input0, FRenderingCompositeOutputRef(Context.FinalOutput));
				Context.FinalOutput = FRenderingCompositeOutputRef(Node);
			}
		}

		// The graph setup should be finished before this line ----------------------------------------

		{
			// currently created on the heap each frame but View.Family->RenderTarget could keep this object and all would be cleaner
			TRefCountPtr<IPooledRenderTarget> Temp;
			FSceneRenderTargetItem Item;
			Item.TargetableTexture = (FTextureRHIRef&)View.Family->RenderTarget->GetRenderTargetTexture();
			Item.ShaderResourceTexture = (FTextureRHIRef&)View.Family->RenderTarget->GetRenderTargetTexture();

			FPooledRenderTargetDesc Desc;

			if (View.Family->RenderTarget->GetRenderTargetTexture())
			{
				Desc.Extent.X = View.Family->RenderTarget->GetRenderTargetTexture()->GetSizeX();
				Desc.Extent.Y = View.Family->RenderTarget->GetRenderTargetTexture()->GetSizeY();
			}
			else
			{
				Desc.Extent = View.Family->RenderTarget->GetSizeXY();
			}

			// todo: this should come from View.Family->RenderTarget
			Desc.Format = PF_B8G8R8A8;
			Desc.NumMips = 1;
			Desc.DebugName = TEXT("OverriddenRenderTarget");

			GRenderTargetPool.CreateUntrackedElement(Desc, Temp, Item);

			OverrideRenderTarget(Context.FinalOutput, Temp, Desc);

			CompositeContext.Process(Context.FinalOutput.GetPass(), TEXT("PostProcessingES2"));
		}
	}
}

void FPostProcessing::ProcessPlanarReflection(FRHICommandListImmediate& RHICmdList, FViewInfo& View, TRefCountPtr<IPooledRenderTarget>& VelocityRT, TRefCountPtr<IPooledRenderTarget>& OutFilteredSceneColor)
{
	{
		FMemMark Mark(FMemStack::Get());
		FRenderingCompositePassContext CompositeContext(RHICmdList, View);

		FPostprocessContext Context(RHICmdList, CompositeContext.Graph, View);
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

		FRenderingCompositeOutputRef VelocityInput;
		if(VelocityRT)
		{
			VelocityInput = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessInput(VelocityRT));
		}

		FSceneViewState* ViewState = Context.View.ViewState;
		EAntiAliasingMethod AntiAliasingMethod = Context.View.AntiAliasingMethod;

		if (AntiAliasingMethod == AAM_TemporalAA && ViewState)
		{
			if(VelocityInput.IsValid())
			{
				AddTemporalAA( Context, VelocityInput );
			}
			else
			{
				// black is how we clear the velocity buffer so this means no velocity
				FRenderingCompositePass* NoVelocity = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessInput(GSystemTextures.BlackDummy));
				FRenderingCompositeOutputRef NoVelocityRef(NoVelocity);
				AddTemporalAA( Context, NoVelocityRef );
			}
		}

		CompositeContext.Process(Context.FinalOutput.GetPass(), TEXT("ProcessPlanarReflection"));

		OutFilteredSceneColor = Context.FinalOutput.GetOutput()->PooledRenderTarget;
	}
}

bool FPostProcessing::HasAlphaChannelSupport()
{
	return CVarAlphaChannel.GetValueOnRenderThread() != 0;
}