// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
PostProcessFFTBlooom.cpp: Post processing blom using an FFT-based convolution.
=============================================================================*/

#include "PostProcess/PostProcessFFTBloom.h"
#include "PostProcess/RenderingCompositionGraph.h"
#include "GPUFastFourierTransform.h"
#include "ScenePrivate.h"
#include "GlobalShader.h"
#include "Shader.h"

#include "RendererModule.h" // for log



class FResizeAndCenterTextureCS : public FGlobalShader
{

public:
	DECLARE_SHADER_TYPE(FResizeAndCenterTextureCS, Global);

	FResizeAndCenterTextureCS() {};

	FResizeAndCenterTextureCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		using GPUFFTComputeShaderUtils::FComputeParameterBinder;

		FComputeParameterBinder Binder(Initializer.ParameterMap);

		Binder(SrcROTexture,         TEXT("SrcTexture"))
			  (SrcSampler,           TEXT("SrcSampler"))
			  (DstRWTexture,         TEXT("DstTexture"))
		      (DstExtent,            TEXT("DstExtent"))
			  (ImageExtent,          TEXT("ImageExtent"))
			  (KernelCenterAndScale, TEXT("KernelCenterAndScale"))
			  (DstBufferExtent,      TEXT("DstBufferExtent"));
	}

	static bool ShouldCache(EShaderPlatform Platform)
	{
		// @todo MetalMRT: Metal MRT can't cope with the threadgroup storage requirements for these shaders right now
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && (!IsMetalPlatform(Platform) || RHIGetShaderLanguageVersion(Platform) >= 2) && (Platform != SP_METAL_MRT);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		
		OutEnvironment.SetDefine(TEXT("INCLUDE_RESIZE_AND_CENTER"), 1);
		OutEnvironment.SetDefine(TEXT("THREADS_PER_GROUP"), FResizeAndCenterTextureCS::NumThreadsPerGroup());
		
	}


	// Determine the number of threads used per scanline when writing the physical space kernel
	static int32 NumThreadsPerGroup() { return 32; }
	// Method for use with the FScopedUAVBind
	FShaderResourceParameter& DestinationResourceParamter() { return DstRWTexture; }

	static const TCHAR* GetSourceFilename() { return TEXT("/Engine/Private/PostProcessFFTBloom.usf"); }
	static const TCHAR* GetFunctionName()   { return TEXT("ResizeAndCenterTextureCS"); }

	void SetCSParamters(FRHICommandList& RHICmdList, const FRenderingCompositePassContext& Context,
		const FIntPoint& DstExtentValue, const FIntPoint& ImageExtentValue, const float ResizeScaleValue,
		const FVector2D& KernelUVCenter, const FTextureRHIRef& SrcTexture, const FIntPoint& DstBufferExtentValue,
		const bool bForceCenterZero)
	{
		float CenterScale = bForceCenterZero ? 0.f : 1.f;
		const FLinearColor KernelCenterAndScaleValue(KernelUVCenter.X, KernelUVCenter.Y, ResizeScaleValue, CenterScale);

		const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		// Set up the input.  We have to do this explicitly because the FFT dispatches multiple compute shaders and manages their input/output.

		GPUFFTComputeShaderUtils::FComputeParamterValueSetter ParamSetter(RHICmdList, ShaderRHI);
		
		ParamSetter.Set<ESamplerFilter::SF_Bilinear, ESamplerAddressMode::AM_Wrap>(SrcROTexture, SrcSampler, SrcTexture);
		
		ParamSetter(DstExtent, DstExtentValue)
			       (ImageExtent, ImageExtentValue)
				   (KernelCenterAndScale, KernelCenterAndScaleValue)
				   (DstBufferExtent, DstBufferExtentValue);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar  << SrcROTexture
			<< SrcSampler
			<< DstRWTexture
			<< DstExtent
			<< ImageExtent
			<< KernelCenterAndScale
			<< DstBufferExtent;
		return bShaderHasOutdatedParameters;
	}

public:

	FShaderResourceParameter SrcROTexture;
	FShaderResourceParameter SrcSampler;
	FShaderResourceParameter DstRWTexture;
	FShaderParameter DstExtent;
	FShaderParameter ImageExtent;
	FShaderParameter KernelCenterAndScale;
	FShaderParameter DstBufferExtent;
};

IMPLEMENT_SHADER_TYPE3(FResizeAndCenterTextureCS, SF_Compute);



class FCaptureKernelWeightsCS : public FGlobalShader
{

public:
	DECLARE_SHADER_TYPE(FCaptureKernelWeightsCS, Global);

	FCaptureKernelWeightsCS() {};

	FCaptureKernelWeightsCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		using GPUFFTComputeShaderUtils::FComputeParameterBinder;
		FComputeParameterBinder Binder(Initializer.ParameterMap);

		Binder(HalfResKernelSrcROTexture, TEXT("HalfResSrcTexture"))
			(PhysicalKernelSrcROTexture,  TEXT("PhysicalSrcTexture"))
			(PhyscalKernelSrcSampler,     TEXT("PhysicalSrcSampler"))
			(DstRWTexture,                TEXT("DstTexture"))
			(HalfResSumLocation,          TEXT("HalfResSumLocation"))
			(UVCenter,                    TEXT("UVCenter"));
	}

	static bool ShouldCache(EShaderPlatform Platform)
	{
		// @todo MetalMRT: Metal MRT can't cope with the threadgroup storage requirements for these shaders right now
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && (!IsMetalPlatform(Platform) || RHIGetShaderLanguageVersion(Platform) >= 2) && (Platform != SP_METAL_MRT);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("INCLUDE_CAPTURE_KERNEL_WEIGHTS"), 1);
	}

	// Method for use with the FScopedUAVBind
	FShaderResourceParameter& DestinationResourceParamter() { return DstRWTexture; }

	static const TCHAR* GetSourceFilename() { return TEXT("/Engine/Private/PostProcessFFTBloom.usf"); }
	static const TCHAR* GetFunctionName()   { return TEXT("CaptureKernelWeightsCS"); }


	void SetCSParamters(FRHICommandList& RHICmdList, const FRenderingCompositePassContext& Context,
		const FTextureRHIRef& HalfResKernelRef,
		const FIntPoint& HalfResSumLocationValue,
		const FTextureRHIRef& PhysicalKernelRef,
		const FVector2D& UVCenterValue)
	{
		const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();



		// Set up the input.  We have to do this explicitly because the FFT dispatches multiple compute shaders and manages their input/output.
		GPUFFTComputeShaderUtils::FComputeParamterValueSetter ParamSetter(RHICmdList, ShaderRHI);
		ParamSetter.Set<ESamplerFilter::SF_Bilinear, ESamplerAddressMode::AM_Wrap>(PhysicalKernelSrcROTexture, PhyscalKernelSrcSampler, PhysicalKernelRef);
		
		ParamSetter(HalfResKernelSrcROTexture, HalfResKernelRef)
			       (HalfResSumLocation, HalfResSumLocationValue)
		           (UVCenter, UVCenterValue);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar  << HalfResKernelSrcROTexture
			<< PhysicalKernelSrcROTexture
			<< PhyscalKernelSrcSampler
			<< DstRWTexture
			<< HalfResSumLocation
			<< UVCenter;

		return bShaderHasOutdatedParameters;
	}

public:

	FShaderResourceParameter HalfResKernelSrcROTexture;
	FShaderResourceParameter PhysicalKernelSrcROTexture;
	FShaderResourceParameter PhyscalKernelSrcSampler;
	FShaderResourceParameter DstRWTexture;
	FShaderParameter HalfResSumLocation;
	FShaderParameter UVCenter;
};

IMPLEMENT_SHADER_TYPE3(FCaptureKernelWeightsCS, SF_Compute);


class FBlendLowResCS : public FGlobalShader
{

public:
	DECLARE_SHADER_TYPE(FBlendLowResCS, Global);

	FBlendLowResCS() {};

	FBlendLowResCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		using GPUFFTComputeShaderUtils::FComputeParameterBinder;

		FComputeParameterBinder Binder(Initializer.ParameterMap);
		Binder(FullResSrcROTexture, TEXT("SrcTexture"))
			  (HalfResSrcROTexture, TEXT("HalfResSrcTexture"))
			  (HalfResSrcSampler,   TEXT("HalfResSrcSampler"))
			  (CenterWeight,        TEXT("CenterWeightTexture"))
			  (DstRWTexture,        TEXT("DstTexture"))
			  (DstRect,             TEXT("DstRect"))
			  (HalfRect,            TEXT("HalfRect"))
			  (HalfBufferSize,      TEXT("HalfBufferSize"));
	}

	static bool ShouldCache(EShaderPlatform Platform)
	{
		// @todo MetalMRT: Metal MRT can't cope with the threadgroup storage requirements for these shaders right now
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && (!IsMetalPlatform(Platform) || RHIGetShaderLanguageVersion(Platform) >= 2) && (Platform != SP_METAL_MRT);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("INCLUDE_BLEND_LOW_RES"), 1);
		OutEnvironment.SetDefine(TEXT("THREADS_PER_GROUP"), FBlendLowResCS::NumThreadsPerGroup());
	}

	// Method for use with the FScopedUAVBind
	FShaderResourceParameter& DestinationResourceParamter() { return DstRWTexture; }

	// Determine the number of threads used per scanline when writing the physical space kernel
	static int32 NumThreadsPerGroup() { return 32; }

	static const TCHAR* GetSourceFilename() { return TEXT("/Engine/Private/PostProcessFFTBloom.usf"); }
	static const TCHAR* GetFunctionName()   { return TEXT("BlendLowResCS"); }


	void SetCSParamters(FRHICommandList& RHICmdList, const FRenderingCompositePassContext& Context,
		const FIntRect& TargetRect, const FIntRect& HalfResRect, const FIntPoint& HalfBufferExent, const FTextureRHIRef& CenterWeightTexutreRef,
		const FTextureRHIRef& FullResTextureRef, const FTextureRHIRef& HalfResTextureRef)
	{

		using GPUFFTComputeShaderUtils::FComputeParamterValueSetter;

		const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();


		// Set up the input.  We have to do this explicitly because the FFT dispatches multiple compute shaders and manages their input/output.
		// We don't need a sampler for this texture ( will use .load)
		SetTextureParameter(RHICmdList, ShaderRHI, CenterWeight, CenterWeightTexutreRef);

		FComputeParamterValueSetter ParamSetter(RHICmdList, ShaderRHI);
		
		ParamSetter.Set<ESamplerFilter::SF_Bilinear, ESamplerAddressMode::AM_Wrap>(HalfResSrcROTexture, HalfResSrcSampler, HalfResTextureRef);
		
		ParamSetter(FullResSrcROTexture, FullResTextureRef)
		           (DstRect,             TargetRect)
		           (HalfRect,            HalfResRect)
		           (HalfBufferSize,      HalfBufferExent);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar  << FullResSrcROTexture
			<< HalfResSrcROTexture
			<< HalfResSrcSampler
			<< CenterWeight
			<< DstRWTexture
			<< DstRect
			<< HalfRect
			<< HalfBufferSize;

		return bShaderHasOutdatedParameters;
	}

public:

	FShaderResourceParameter FullResSrcROTexture;
	FShaderResourceParameter HalfResSrcROTexture;
	FShaderResourceParameter HalfResSrcSampler;
	FShaderResourceParameter CenterWeight;
	FShaderResourceParameter DstRWTexture;
	FShaderParameter DstRect;
	FShaderParameter HalfRect;
	FShaderParameter HalfBufferSize;
};

IMPLEMENT_SHADER_TYPE3(FBlendLowResCS, SF_Compute);


class FPassThroughCS : public FGlobalShader
{

public:
	DECLARE_SHADER_TYPE(FPassThroughCS, Global);

	FPassThroughCS() {};

	FPassThroughCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		using GPUFFTComputeShaderUtils::FComputeParameterBinder;
		FComputeParameterBinder Binder(Initializer.ParameterMap);

		Binder(SrcROTexture, TEXT("SrcTexture"))
		      (DstRWTexture, TEXT("DstTexture"))
		      (DstRect,      TEXT("DstRect"))
		      (SrcRect,      TEXT("SrcRect"));
	}

	static bool ShouldCache(EShaderPlatform Platform)
	{
		// @todo MetalMRT: Metal MRT can't cope with the threadgroup storage requirements for these shaders right now
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && (!IsMetalPlatform(Platform) || RHIGetShaderLanguageVersion(Platform) >= 2) && (Platform != SP_METAL_MRT);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("INCLUDE_PASSTHROUGH"), 1);
		OutEnvironment.SetDefine(TEXT("THREADS_PER_GROUP"), FPassThroughCS::NumThreadsPerGroup());
	}

	// Method for use with the FScopedUAVBind
	FShaderResourceParameter& DestinationResourceParamter() { return DstRWTexture; }

	static int32 NumThreadsPerGroup() { return 32; }

	static const TCHAR* GetSourceFilename() { return TEXT("/Engine/Private/PostProcessFFTBloom.usf"); }
	static const TCHAR* GetFunctionName()   { return TEXT("PassThroughCS"); }

	void SetCSParamters(FRHICommandList& RHICmdList, const FRenderingCompositePassContext& Context,
		const FTextureRHIRef& SrcTexture, const FIntRect& SrcRectValue, const FIntRect& DstRectValue)
	{
		using GPUFFTComputeShaderUtils::FComputeParamterValueSetter;
		const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();



		// Set up the input.  We have to do this explicitly because the FFT dispatches multiple compute shaders and manages their input/output.

		FComputeParamterValueSetter ParamSetter(RHICmdList, ShaderRHI);
		ParamSetter(SrcROTexture, SrcTexture)
				   (DstRect, DstRectValue)
			       (SrcRect, SrcRectValue);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar  << SrcROTexture
			<< DstRWTexture
			<< DstRect
			<< SrcRect;

		return bShaderHasOutdatedParameters;
	}

public:

	FShaderResourceParameter SrcROTexture;
	FShaderResourceParameter DstRWTexture;
	FShaderParameter SrcRect;
	FShaderParameter DstRect;
};

IMPLEMENT_SHADER_TYPE3(FPassThroughCS, SF_Compute);

/**
* Used to resample the physical space kernel into the correct sized buffer with the 
* correct periodicity and center
*
* Resizes the image, moves the center to to 0,0 and applies periodicity 
* across the full TargetSize (periods TargetSize.x & TargetSize.y) 
*
* @param Context       - container for RHI and ShaderMap
* @param SrcTexture    - SRV for the physical space kernel supplied by user
* @param SrcImageSize  - The extent of the src image
* @param SrcImageCenterUV - The location of the center in src image (e.g. where the kernel center really is).
* @param ResizeScale    - Affective size of the physical space kernel in units of the ImageExtent.x 
* @param TargetSize    - Size of the image produced. 
* @param DstUAV        - Holds the result
* @param DstBufferSize - Size of DstBuffer
* @param bForceCenterZero -  is true only for the experimental 1/2 res version, part of conserving energy
*/
void ResizeAndCenterTexture(FRenderingCompositePassContext& Context,
	const FTextureRHIRef& SrcTexture,
	const FIntPoint& SrcImageSize,
	const FVector2D& SrcImageCenterUV,
	const float ResizeScale,
	const FIntPoint& TargetSize,
	FUnorderedAccessViewRHIRef& DstUAV,
	const FIntPoint& DstBufferSize,
	const bool bForceCenterZero)
{
	FRHICommandListImmediate& RHICmdList = Context.RHICmdList;
	SCOPED_DRAW_EVENTF(RHICmdList, FRCPassFFTBloom, TEXT("FFT: Pre-process the space kernel to %d by %d"), TargetSize.X, TargetSize.Y);

	// Clamp the image center

	FVector2D ClampedImageCenterUV = SrcImageCenterUV;
	ClampedImageCenterUV.X = FMath::Clamp(SrcImageCenterUV.X, 0.f, 1.f);
	ClampedImageCenterUV.Y = FMath::Clamp(SrcImageCenterUV.Y, 0.f, 1.f);

	TShaderMap<FGlobalShaderType>& ShaderMap = *Context.GetShaderMap();
	// Get a pointer to the shader
	FResizeAndCenterTextureCS* ComputeShader = ShaderMap.GetShader< FResizeAndCenterTextureCS >();

	//
	SetRenderTarget(RHICmdList, FTextureRHIRef(), FTextureRHIRef());
	RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());

	// set destination
	check(DstUAV);
	RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, DstUAV);

	{
		GPUFFTComputeShaderUtils::FScopedUAVBind ScopedBindOutput = GPUFFTComputeShaderUtils::FScopedUAVBind::BindOutput(RHICmdList, *ComputeShader, DstUAV);


		ComputeShader->SetCSParamters(RHICmdList, Context, TargetSize, SrcImageSize, ResizeScale, ClampedImageCenterUV, SrcTexture, DstBufferSize, bForceCenterZero);

		// Use multiple threads per scan line to insure memory coalescing during the write
		const int32 ThreadsPerGroup = ComputeShader->NumThreadsPerGroup();
		const int32 ThreadsGroupsPerScanLine = (DstBufferSize.X % ThreadsPerGroup == 0) ? DstBufferSize.X / ThreadsPerGroup : DstBufferSize.X / ThreadsPerGroup + 1;

		RHICmdList.DispatchComputeShader(ThreadsGroupsPerScanLine, DstBufferSize.Y, 1);
	}


	
}

/**
* Used by experimental energy conserving 1/2 resolution version of the bloom.
* Captures the sum of the kernel weights represented by the 1/2 res kernel and
* the Center weight from the physical space kernel.
*
* @param Context            - container for RHI and ShaderMap
* @param HalfResKernel      - SRV for the pre-transformed 1/2 res kernel
* @param HalfResSumLocation - The location to sample in the pre-transformed kernel to find the sum of the physical space kernel weights
* @param PhysicalKernel     - SRV for the original physical space kernel
* @param CenterUV           - Where to sample the Physical Kernel for the center weight
* @param CenterWeightRT     - 2x1 float4 buffer that on return will hold result:
*     At  (0,0) the center weight of physical kernel, and (1,0) the sum of the 1/2res kernel weights
*/
void CaptureKernelWeight(FRenderingCompositePassContext& Context,
	const FTextureRHIRef& HalfResKernel,
	const FIntPoint& HalfResSumLocation,
	const FTextureRHIRef& PhysicalKernel,
	const FVector2D& CenterUV,
	TRefCountPtr<IPooledRenderTarget>& CenterWeightRT)
{
	FRHICommandListImmediate& RHICmdList = Context.RHICmdList;

	SCOPED_DRAW_EVENTF(RHICmdList, FRCPassFFTBloom, TEXT("FFT: Capture Kernel Weights"));

	FSceneRenderTargetItem& DstTargetItem = CenterWeightRT->GetRenderTargetItem();

	// Get a pointer to the shader
	TShaderMap<FGlobalShaderType>& ShaderMap = *Context.GetShaderMap();
	// Get a pointer to the shader
	FCaptureKernelWeightsCS* ComputeShader = ShaderMap.GetShader< FCaptureKernelWeightsCS >();


	SetRenderTarget(RHICmdList, FTextureRHIRef(), FTextureRHIRef());
	RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());

	// set destination
	check(DstTargetItem.UAV);
	RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, DstTargetItem.UAV);
	{
		GPUFFTComputeShaderUtils::FScopedUAVBind ScopedBindOutput = GPUFFTComputeShaderUtils::FScopedUAVBind::BindOutput(Context.RHICmdList, *ComputeShader, DstTargetItem.UAV);
		RHICmdList.SetUAVParameter(ComputeShader->GetComputeShader(), ComputeShader->DstRWTexture.GetBaseIndex(), DstTargetItem.UAV);

		ComputeShader->SetCSParamters(RHICmdList, Context, HalfResKernel, HalfResSumLocation, PhysicalKernel, CenterUV);

		RHICmdList.DispatchComputeShader(1, 1, 1);
	}


	RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, DstTargetItem.UAV);

	// Test.
	ensureMsgf(DstTargetItem.TargetableTexture == DstTargetItem.ShaderResourceTexture, TEXT("%s should be resolved to a separate SRV"), *DstTargetItem.TargetableTexture->GetName().ToString());
}

/**
* Used by energy conserving 1/2 resolution version of the bloom.
* This blends the results of the low resolution bloom with the full resolution image 
* in an energy conserving manner.  Assumes the 1/2-res bloom is done with a kernel that
* is missing the center pixel (i.e. the self-gather contribution), and this missing contribution
* is supplied by the full-res image.
*
* @param Context  - container for RHI and ShaderMap
* @param FullResImage - Unbloomed full-resolution source image
* @param FullResImageRect      - Region in FullResImage and DstUAV where the image lives
* @param HaflResConvolvedImage - A 1/2 res image that has been convolved with the bloom kernel minus center.
* @param HalfResRect           - Location of image in the HalfResConvolvedImage buffer
* @param HalfBufferSize        - Full size of the 1/2 Res buffer.
* @param CenterWeightTexture   - Texture that holds the weight between the kernel center and sum of 1/2res kernel weights.
*                                 needed to correctly composite the 1/2 res bloomed result with the full-res image.
* @param DstUAV                - Destination buffer that will hold the result.
*/
void BlendLowRes(FRenderingCompositePassContext& Context,
	const FTextureRHIRef& FullResImage,
	const FIntRect& FullResImageRect,
	const FTextureRHIRef& HalfResConvolvedImage,
	const FIntRect& HalfResRect,
	const FIntPoint& HalfBufferSize,
	const FTextureRHIRef& CenterWeightTexutre,
	FUnorderedAccessViewRHIRef& DstUAV)
{
	FRHICommandListImmediate& RHICmdList = Context.RHICmdList;
	SCOPED_DRAW_EVENTF(RHICmdList, FRCPassFFTBloom, TEXT("FFT: Post-process upres and blend"));


	// Get a pointer to the shader
	// Get a pointer to the shader
	TShaderMap<FGlobalShaderType>& ShaderMap = *Context.GetShaderMap();
	// Get a pointer to the shader
	FBlendLowResCS* ComputeShader = ShaderMap.GetShader< FBlendLowResCS>();


	SetRenderTarget(RHICmdList, FTextureRHIRef(), FTextureRHIRef());
	RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());

	// set destination
	check(DstUAV);
	RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, DstUAV);
	
	{
		GPUFFTComputeShaderUtils::FScopedUAVBind ScopedBindOutput = GPUFFTComputeShaderUtils::FScopedUAVBind::BindOutput(Context.RHICmdList, *ComputeShader, DstUAV);
		ComputeShader->SetCSParamters(RHICmdList, Context, FullResImageRect, HalfResRect, HalfBufferSize, CenterWeightTexutre, FullResImage, HalfResConvolvedImage);

		FIntPoint TargetExtent = FullResImageRect.Size();
		// Use multiple threads per scan line to insure memory coalescing during the write
		const int32 ThreadsPerGroup = ComputeShader->NumThreadsPerGroup();
		const int32 ThreadsGroupsPerScanLine = (TargetExtent.X % ThreadsPerGroup == 0) ? TargetExtent.X / ThreadsPerGroup : TargetExtent.X / ThreadsPerGroup + 1;

		RHICmdList.DispatchComputeShader(ThreadsGroupsPerScanLine, TargetExtent.Y, 1);
	}

	RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, DstUAV);

}

/**
* Used to copy the input image in the event that it is too large to bloom (i.e. doesn't fit in the FFT group shared memory)
*
* @param Context - container for RHI and ShaderMap
* @param SrcTargetItem  - The SrcBuffer to be copied.
* @param SrcRect        - The region in the SrcBuffer to copy
* @param DstUAV         - The target buffer for the copy
* @param DstRect        - The location and region in the target buffer for the copy
*/
void CopyImageRect(FRenderingCompositePassContext& Context, const FSceneRenderTargetItem& SrcTargetItem, const FIntRect& SrcRect, FUnorderedAccessViewRHIRef& DstUAV,  const FIntRect& DstRect)
{

	SCOPED_DRAW_EVENTF(Context.RHICmdList, FRCPassFFTBloom, TEXT("FFT: passthrough "));

	FRHICommandListImmediate& RHICmdList = Context.RHICmdList;


	// Get a pointer to the shader
	// Get a pointer to the shader
		// Get a pointer to the shader
	TShaderMap<FGlobalShaderType>& ShaderMap = *Context.GetShaderMap();
	// Get a pointer to the shader
	FPassThroughCS* ComputeShader = ShaderMap.GetShader< FPassThroughCS >();

	SetRenderTarget(Context.RHICmdList, FTextureRHIRef(), FTextureRHIRef());
	Context.RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());

	// set destination
	check(DstUAV);
	RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, DstUAV);
	{
		GPUFFTComputeShaderUtils::FScopedUAVBind ScopedBindOutput = GPUFFTComputeShaderUtils::FScopedUAVBind::BindOutput(RHICmdList, *ComputeShader, DstUAV);


		ComputeShader->SetCSParamters(RHICmdList, Context, SrcTargetItem.ShaderResourceTexture, SrcRect, DstRect);

		const FIntPoint DstRectSize = DstRect.Size();

		// Use multiple threads per scan line to insure memory coalescing during the write
		const int32 ThreadsPerGroup = ComputeShader->NumThreadsPerGroup();
		const int32 ThreadsGroupsPerScanLine = (DstRectSize.X % ThreadsPerGroup == 0) ? DstRectSize.X / ThreadsPerGroup : DstRectSize.X / ThreadsPerGroup + 1;

		RHICmdList.DispatchComputeShader(ThreadsGroupsPerScanLine, DstRectSize.Y, 1);
	}
}

void FRCPassFFTBloom::InitializeDomainParameters(FRenderingCompositePassContext& Context, const float KernelSupportScale, const float KernelSupportScaleClamp)
{

	// We padd by 1/2 the number of pixels the kernel needs in the x-direction
	// so if the kernel is being applied on the edge of the image it will see padding and not periodicity
	// NB:  If the kernel padding would force a transform buffer that is too big for group shared memory (> 4096)
	//      we clamp it.  This could result in a wrap-around in the bloom (from one side of the screen to the other),
	//      but since the amplitude of the bloom kernel tails is usually very small, this shouldnt be too bad.
	auto KernelRadiusSupportFunctor = [KernelSupportScale, KernelSupportScaleClamp](const FIntPoint& Size) ->int32 {

		float ClampedKernelSupportScale = (KernelSupportScaleClamp > 0) ? FMath::Min(KernelSupportScale, KernelSupportScaleClamp) : KernelSupportScale;
		int32 FilterRadius = FMath::CeilToInt(0.5 * ClampedKernelSupportScale * Size.X);
		const int32 MaxFFTSize = GPUFFT::MaxScanLineLength();
		int32 MaxDim = FMath::Max(Size.X, Size.Y);
		if (MaxDim + FilterRadius > MaxFFTSize && MaxDim < MaxFFTSize) FilterRadius = MaxFFTSize - MaxDim;

		return FilterRadius;
	};

	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	if (!InputDesc)
	{
		// input is not hooked up correctly return dummy kernel
		return;
	}


	const FSceneView& View = Context.View;

	InputBufferSize = InputDesc->Extent;

	// Get the source
	TRefCountPtr<IPooledRenderTarget> Input = Context.Pass->GetInput(EPassInputId(0))->GetOutput()->RequestInput();
	InputTargetItem = &Input->GetRenderTargetItem();
	const FTextureRHIRef& InputTexture = InputTargetItem->ShaderResourceTexture;

	// This will be for the actual output.

	OutputTargetItem = const_cast<FSceneRenderTargetItem*>(&PassOutputs[0].RequestSurface(Context));
	FIntPoint OutputBufferSize = PassOutputs[0].RenderTargetDesc.Extent;



	// Determine the region in the source buffer that we want to copy.
	//
	// e.g. 4 means the input texture is 4x smaller than the buffer size
	const uint32 InputScaleFactor  = FMath::DivideAndRoundUp(FSceneRenderTargets::Get(Context.RHICmdList).GetBufferSizeXY().Y, InputBufferSize.Y);
	const uint32 OutputScaleFactor = FMath::DivideAndRoundUp(FSceneRenderTargets::Get(Context.RHICmdList).GetBufferSizeXY().Y, OutputBufferSize.Y);

	const FIntRect InputRect = View.ViewRect / InputScaleFactor;
	const FIntRect OutputRect = View.ViewRect / OutputScaleFactor;

	// Capture the region of interest
	ImageRect = InputRect;
	const FIntPoint ImageSize = ImageRect.Size();
	
	int32 SpectralPadding = KernelRadiusSupportFunctor(ImageSize);

	// The following are mathematically equivalent
	// 1) Horizontal FFT / Vertical FFT / Filter / Vertical InvFFT / Horizontal InvFFT
	// 2) Vertical FFT / Horizontal FFT / Filter / Horizontal InvFFT / Vertical InvFFT
	// but we choose the one with the smallest intermediate buffer size

	// The size of the input image plus padding that accounts for
	// the width of the kernel.  The ImageRect is virtually padded
	// with black to account for the gather action of the convolution.
	FIntPoint PaddedImageSize = ImageSize + FIntPoint(SpectralPadding, SpectralPadding);
	FrequencySize = FIntPoint(FMath::RoundUpToPowerOfTwo(PaddedImageSize.X), FMath::RoundUpToPowerOfTwo(PaddedImageSize.Y));

	// Chose to do to transform in the direction that results in writting the least amount of data to main memory.

	bDoHorizontalFirst = ((FrequencySize.Y * PaddedImageSize.X) > (FrequencySize.X * PaddedImageSize.Y));

	// 
	bIsInitialized = true;
}



bool  FRCPassFFTBloom::TransformKernelFFT(FRenderingCompositePassContext& Context, FSceneRenderTargetItem& KernelTargetItem)
{
	FRHICommandListImmediate& RHICmdList = Context.RHICmdList;
	GPUFFT::FGPUFFTShaderContext FFTContext(RHICmdList, *Context.GetShaderMap());

	// Create the tmp buffer

	// Our frequency storage layout adds two elements to the first transform direction. 
	const FIntPoint FrequencyPadding = (bDoHorizontalFirst) ? FIntPoint(2, 0) : FIntPoint(0, 2);
	const FIntPoint PaddedFrequencySize = FrequencySize + FrequencyPadding;

	// Should read / write to PF_G16R16F or PF_G32R32F (float2 formats)
	// Need to set the render target description before we "request surface"
	const EPixelFormat PixelFormat = GPUFFT::PixelFormat();
	FPooledRenderTargetDesc Desc = FPooledRenderTargetDesc::Create2DDesc(PaddedFrequencySize, PixelFormat,
		FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable | TexCreate_UAV, false);

	// Temp buffer used at intermediate buffer when transforming the world space kernel 
	TRefCountPtr<IPooledRenderTarget> TmpRT;
	GRenderTargetPool.FindFreeElement(Context.RHICmdList, Desc, TmpRT, TEXT("FFT Tmp Kernel Buffer"));

	FIntRect SrcRect(FIntPoint(0, 0), FrequencySize);
	const FTextureRHIRef& SrcImage = KernelTargetItem.ShaderResourceTexture;
	FSceneRenderTargetItem& ResultBuffer = KernelTargetItem;

	bool SuccessValue = GPUFFT::FFTImage2D(FFTContext, FrequencySize, bDoHorizontalFirst, SrcRect, SrcImage,  ResultBuffer, TmpRT->GetRenderTargetItem());

	// Transition resource
	RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, ResultBuffer.UAV);
	//RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, ResultUAV);
	
	return SuccessValue;
}


bool FRCPassFFTBloom::ConvolveWithKernel(FRenderingCompositePassContext& Context,
	const FTextureRHIRef& SpectralKernelTexture, const FLinearColor& Tint,
	const FTextureRHIRef& SrcTexture, FUnorderedAccessViewRHIRef& ResultUAV,
	const FPreFilter& PreFilter)
{

	if (!bIsInitialized )
	{
		// The dimensions have not be calculated.
		return false;
	}

	FRHICommandListImmediate& RHICmdList = Context.RHICmdList;
	GPUFFT::FGPUFFTShaderContext FFTContext(RHICmdList, *Context.GetShaderMap());

	// Get Tmp buffers required for the Convolution
	
	TRefCountPtr<IPooledRenderTarget> TmpTargets[2];

	const FIntPoint TmpExtent = GPUFFT::Convolution2DBufferSize(FrequencySize, bDoHorizontalFirst, ImageRect.Size());
		//(bDoHorizontalFirst) ? FIntPoint(FrequencySize.X + 2, ImageRect.Size().Y) : FIntPoint(ImageRect.Size().X, FrequencySize.Y + 2);
	FPooledRenderTargetDesc Desc = 
		FPooledRenderTargetDesc::Create2DDesc(TmpExtent, GPUFFT::PixelFormat(), FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable | TexCreate_UAV, false);

	GRenderTargetPool.FindFreeElement(Context.RHICmdList, Desc, TmpTargets[0], TEXT("Tmp FFT Buffer A"));
	GRenderTargetPool.FindFreeElement(Context.RHICmdList, Desc, TmpTargets[1], TEXT("Tmp FFT Buffer B"));
	
	// Get the source
	const FTextureRHIRef& InputTexture = SrcTexture;

	bool SuccessValue =
		GPUFFT::ConvolutionWithTextureImage2D(FFTContext, FrequencySize, bDoHorizontalFirst,
				SpectralKernelTexture,
			    ImageRect/*region of interest*/,
				InputTexture, 
				ResultUAV,
				TmpTargets[0]->GetRenderTargetItem(),
				TmpTargets[1]->GetRenderTargetItem(),
				PreFilter);

	RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToGfx, ResultUAV);

	return SuccessValue;

}

FSceneRenderTargetItem* FRCPassFFTBloom::InitDomainAndGetKernel(FRenderingCompositePassContext& Context)
{

	const FSceneView& View = Context.View;
	FSceneViewState* ViewState = (FSceneViewState*)View.State;

	

	const auto& PPSettings = View.FinalPostProcessSettings;

	// The kernel parameters on the FinalPostProcess.  

	UTexture2D* BloomConvolutionTexture = PPSettings.BloomConvolutionTexture;
	
	if (BloomConvolutionTexture == nullptr)
	{
		BloomConvolutionTexture = GEngine->DefaultBloomKernelTexture;
	}

	const float BloomConvolutionSize    = PPSettings.BloomConvolutionSize;
	const FVector2D CenterUV            = PPSettings.BloomConvolutionCenterUV;
	const float ClampedBloomConvolutionBufferScale = FMath::Clamp(PPSettings.BloomConvolutionBufferScale, 0.f, 1.f);
	

	// The pre-filter boost parameters for bright pixels
	const FVector PreFilter(PPSettings.BloomConvolutionPreFilterMin, PPSettings.BloomConvolutionPreFilterMax, PPSettings.BloomConvolutionPreFilterMult);

	
	// Clip the Kernel support (i.e. bloom size) to 100% the screen width 
	const float MaxBloomSize = 1.f;
	const float ClampedBloomSizeScale = FMath::Clamp(BloomConvolutionSize, 0.f, MaxBloomSize);

	// Set up the buffer sizes
	InitializeDomainParameters(Context, ClampedBloomSizeScale, ClampedBloomConvolutionBufferScale);

	if (bIsInitialized == false) return nullptr;

	// The transform kernel gets cached in the view state.
	if (!ViewState)
	{
		// input is not hooked up correctly, return a null pointer.
		return nullptr;
	}
	// redundant check
	if (!BloomConvolutionTexture || !BloomConvolutionTexture->Resource)  return nullptr;


	// The FFT is much slower if not in group shared memory.
	bool bFitsInGroupSharedMemory = GPUFFT::FitsInGroupSharedMemory(FrequencySize.X) && GPUFFT::FitsInGroupSharedMemory(FrequencySize.Y);
	//if (!bFitsInGroupSharedMemory) return nullptr;



	// Our frequency storage layout adds two elements to the first transform direction. 
	const FIntPoint FrequencyPadding = (bDoHorizontalFirst) ? FIntPoint(2, 0) : FIntPoint(0, 2);
	const FIntPoint PaddedFrequencySize = FrequencySize + FrequencyPadding;

	// Should read / write to PF_G16R16F or PF_G32R32F (float2 formats)
	// Need to set the render target description before we "request surface"
	const EPixelFormat PixelFormat = GPUFFT::PixelFormat();
	const FPooledRenderTargetDesc TransformDesc = FPooledRenderTargetDesc::Create2DDesc(PaddedFrequencySize, PixelFormat,
		FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable | TexCreate_UAV, false);


	auto& FFTKernel = ViewState->BloomFFTKernel;
	// Get the FFT kernel from the view state (note, this has already been transformed).
	TRefCountPtr<IPooledRenderTarget>& TransformedKernelRT = FFTKernel.Spectral;
	const UTexture2D* CachedKernelPhysical                 = FFTKernel.Physical;
	const float       CachedKernelScale                    = FFTKernel.Scale;
	const FVector2D&  CachedKernelCenterUV                 = FFTKernel.CenterUV;
	const FIntPoint&  CachedImageSize                      = FFTKernel.ImageSize;

	const FIntPoint ImageSize = ImageRect.Size();
	// Check if the FFT kernel is dirty 

	bool bCachedKernelIsDirty = true;
	if (TransformedKernelRT)
	{
		FPooledRenderTarget* TransformedTexture = (FPooledRenderTarget*)TransformedKernelRT.GetReference();

		const bool bSameTexture        = (CachedKernelPhysical == static_cast<const UTexture2D*>(BloomConvolutionTexture));
		const bool bSameSpectralBuffer = TransformedTexture->GetDesc().Compare(TransformDesc, true /*exact match*/);
		const bool bSameKernelSize     = FMath::IsNearlyEqual(CachedKernelScale, BloomConvolutionSize, float(1.e-6) /*tol*/);
		const bool bSameImageSize      = (ImageSize == CachedImageSize);
		const bool bSameKernelCenterUV = CachedKernelCenterUV.Equals(CenterUV, float(1.e-6) /*tol*/);
		const bool bSameMipLevel       = bSameTexture && (
			FFTKernel.PhysicalMipLevel == static_cast<FTexture2DResource*>(BloomConvolutionTexture->Resource)->GetCurrentFirstMip());

		if (bSameTexture && bSameSpectralBuffer && bSameKernelSize && bSameImageSize && bSameKernelCenterUV && bSameMipLevel)
		{
			bCachedKernelIsDirty = false;
		}

	}


	const bool bIsHalfResolutionFFT = bHalfResolutionFFT();
	// Re-transform the kernel if needed.
	if (bCachedKernelIsDirty)
	{
		// Resize the buffer to hold the transformed kernel
		GRenderTargetPool.FindFreeElement(Context.RHICmdList, TransformDesc, TransformedKernelRT, TEXT("FFTKernel"));

		// NB: SpectralKernelRTItem is member data
		FSceneRenderTargetItem& SpectralKernelRTItem = TransformedKernelRT->GetRenderTargetItem();
		FUnorderedAccessViewRHIRef SpectralKernelUAV = SpectralKernelRTItem.UAV;

		// Sample the physical space kernel into the resized buffer

		FTextureRHIRef& PhysicalSpaceKernelTextureRef = BloomConvolutionTexture->Resource->TextureRHI;

		// Rescale the physical space kernel ( and omit the center if this is a 1/2 resolution fft, it will be added later)

		ResizeAndCenterTexture(Context, PhysicalSpaceKernelTextureRef, ImageSize, CenterUV, ClampedBloomSizeScale,
			FrequencySize, SpectralKernelRTItem.UAV, PaddedFrequencySize, bIsHalfResolutionFFT);

		Context.RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, SpectralKernelRTItem.UAV);

		// Two Dimensional FFT of the physical space kernel.  
		// Input: SpectralRTItem holds the physical space kernel, on return it will be the spectral space 

		TransformKernelFFT(Context, SpectralKernelRTItem);



		if (bIsHalfResolutionFFT)
		{
			TRefCountPtr<IPooledRenderTarget>& CenterWeightRT = FFTKernel.CenterWeight;

			const FPooledRenderTargetDesc CenterWeightDesc = FPooledRenderTargetDesc::Create2DDesc(FIntPoint(2, 1), PixelFormat,
				FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable | TexCreate_UAV, false);

			// Resize the buffer to hold the transformed kernel
			GRenderTargetPool.FindFreeElement(Context.RHICmdList, CenterWeightDesc, CenterWeightRT, TEXT("FFTKernelCenterWeight"));

			const FTextureRHIRef& HalfResKernelTextureRef = SpectralKernelRTItem.ShaderResourceTexture;

			const FIntPoint& HalfResKernelExtent = PaddedFrequencySize;

			const FIntPoint HalfResSumLocation = (bDoHorizontalFirst) ? FIntPoint(HalfResKernelExtent.X, 0) : FIntPoint(0, HalfResKernelExtent.Y);
			// Capture the missing center weight from the kernel and the sum of the existing weights.
			CaptureKernelWeight(Context, HalfResKernelTextureRef, HalfResKernelExtent, PhysicalSpaceKernelTextureRef, CenterUV, CenterWeightRT);
		}

		// Update the data on the ViewState
		ViewState->BloomFFTKernel.Scale = BloomConvolutionSize;
		ViewState->BloomFFTKernel.ImageSize = ImageSize;
		ViewState->BloomFFTKernel.Physical = BloomConvolutionTexture;
		ViewState->BloomFFTKernel.CenterUV = CenterUV;
		ViewState->BloomFFTKernel.PhysicalMipLevel = static_cast<FTexture2DResource*>(BloomConvolutionTexture->Resource)->GetCurrentFirstMip();
	}

	// Return pointer to the transformed kernel.
	return &(TransformedKernelRT->GetRenderTargetItem());
}


bool FRCPassFFTBloom::ConvolveImageWithKernel(FRenderingCompositePassContext& Context)
{

	// Init the domain data update the cached kernel if needed.
	FSceneRenderTargetItem* SpectralKernelRTItem = InitDomainAndGetKernel(Context);

	// was the domain too large?  did something else fail?
	if (!SpectralKernelRTItem) return false;

	// Do the convolution with the kernel

	const FTextureRHIRef& SpectralKernelTexture = SpectralKernelRTItem->ShaderResourceTexture;

	const bool bIsHalfResolutionFFT = bHalfResolutionFFT();
	

	const FSceneView& View = Context.View;
	const auto& FinalPPSettings = View.FinalPostProcessSettings;
	// The pre-filter boost parameters for bright pixels
	const FVector PreFilter(FinalPPSettings.BloomConvolutionPreFilterMin, FinalPPSettings.BloomConvolutionPreFilterMax, FinalPPSettings.BloomConvolutionPreFilterMult);

	const FLinearColor Tint(1, 1, 1, 1);

	if (bIsHalfResolutionFFT)
	{

		// Get a half-resolution destination buffer.
		TRefCountPtr<IPooledRenderTarget> HalfResConvolutionResult;
		
		const EPixelFormat PixelFormat = GPUFFT::PixelFormat();

		const FPooledRenderTargetDesc HalfResFFTDesc = FPooledRenderTargetDesc::Create2DDesc(InputBufferSize, PixelFormat,
			FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable | TexCreate_UAV, false);

		GRenderTargetPool.FindFreeElement(Context.RHICmdList, HalfResFFTDesc, HalfResConvolutionResult, TEXT("HalfRes FFT Result"));
		FSceneRenderTargetItem& HalfResConvolutionRTItem = HalfResConvolutionResult->GetRenderTargetItem();

		// The FFT result buffer is also half res.
	
		ConvolveWithKernel(Context, SpectralKernelTexture, Tint, InputTargetItem->ShaderResourceTexture , HalfResConvolutionRTItem.UAV, PreFilter);

		// The blend weighting parameters from the View State

		FSceneViewState* ViewState = (FSceneViewState*)View.State;
		auto& FFTKernel = ViewState->BloomFFTKernel;

		const FTextureRHIRef& CenterWeightTexture = FFTKernel.CenterWeight->GetRenderTargetItem().ShaderResourceTexture;

		// The output buffer
		// NB: the Target buffer and source buffer have the same extent.

		FSceneRenderTargetItem& PassOutput = *OutputTargetItem;

		// Get full resolution source

		const TRefCountPtr<IPooledRenderTarget>& FullResRT = Context.Pass->GetInput(ePId_Input1)->GetOutput()->RequestInput();
		const FTextureRHIRef& FullResResourceTexture = FullResRT->GetRenderTargetItem().ShaderResourceTexture;

		// Blend with  alpha * SrcBuffer + betta * BloomedBuffer  where alpha = Weights[0], beta = Weights[1]
		const FIntPoint& HalfResBufferSize = InputBufferSize;
		BlendLowRes(Context, FullResResourceTexture, View.ViewRect, HalfResConvolutionRTItem.ShaderResourceTexture, ImageRect, HalfResBufferSize, CenterWeightTexture, PassOutput.UAV);

	

	}
	else
	{
		// Do Convolution directly into the output buffer
		// NB: In this case there is only one input, and the output has matching resolution

		ConvolveWithKernel(Context, SpectralKernelTexture, Tint, InputTargetItem->ShaderResourceTexture, OutputTargetItem->UAV, PreFilter);

	}

	return true;
}

void FRCPassFFTBloom::PassThroughImage(FRenderingCompositePassContext& Context)
{
	// Copy the Image content and location
	const FIntRect& InputRect  = ImageRect;
	const FIntRect& OutputRect = ImageRect;
	CopyImageRect(Context, *InputTargetItem, InputRect, OutputTargetItem->UAV, OutputRect);
	Context.RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, OutputTargetItem->UAV);
}



void FRCPassFFTBloom::Process(FRenderingCompositePassContext& Context)
{
	bool bSucesss = ConvolveImageWithKernel(Context);

	// Fail gracefully by just copying the input image without convolution.
	// Currently this will happen if the transform lengths are too large
	// for group shared memory or if the Context.View.State is invalid.
	if (!bSucesss)
	{
		PassThroughImage(Context);
	}

}

FPooledRenderTargetDesc FRCPassFFTBloom::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	//  The optional second input will override the output format and size
	const bool bIsHalfResolutionFFT = bHalfResolutionFFT();

	EPassInputId PassInputId = (bIsHalfResolutionFFT) ? ePId_Input1 : ePId_Input0;

	const FPooledRenderTargetDesc& SrcRet = GetInput(PassInputId)->GetOutput()->RenderTargetDesc;

	// NB: this only resets a limited number of parameters 
	//SrcRet.Reset();
	// PF_FloatRGBA
	EPixelFormat Format = SrcRet.Format;

	FIntPoint Extent = SrcRet.Extent;
	FPooledRenderTargetDesc Ret(FPooledRenderTargetDesc::Create2DDesc(Extent, Format, FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable | TexCreate_UAV, false));

	Ret.DebugName = TEXT("FFTBuffer");

	return Ret;
}


bool FRCPassFFTBloom::HasValidPhysicalKernel(FPostprocessContext& Context)
{
	const FViewInfo& View = Context.View;

	UTexture2D* BloomConvolutionTexture = View.FinalPostProcessSettings.BloomConvolutionTexture;

	// Fall back to the default bloom texture if provided.
	if (BloomConvolutionTexture == nullptr)
	{
		BloomConvolutionTexture = GEngine->DefaultBloomKernelTexture;
	}

	bool bValidSetup = (BloomConvolutionTexture != nullptr && BloomConvolutionTexture->Resource != nullptr);

	if (bValidSetup && BloomConvolutionTexture->IsFullyStreamedIn() == false)
	{
		UE_LOG(LogRenderer, Warning, TEXT("The Physical Kernel Texture not fully streamed in."));
	}

	bValidSetup = bValidSetup && (BloomConvolutionTexture->IsFullyStreamedIn() == true);

	if (bValidSetup && BloomConvolutionTexture->bHasStreamingUpdatePending == true)
	{
		UE_LOG(LogRenderer, Warning, TEXT("The Physical Kernel Texture has pending update."));
	}

	bValidSetup = bValidSetup && (BloomConvolutionTexture->bHasStreamingUpdatePending == false);

	return bValidSetup;
}
