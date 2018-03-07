// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
PostProcessFFTBloom.h: Post processing bloom using FFT convolution
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "PostProcess/PostProcessing.h"

// This class internally organizes the butterfly passes required for the FFT.
// Currently this class is a bass class (shared by two derived classes) and is not used directly.
// ePId_Input0: Color input to transform.
// ePId_Input1: optional input to composite. 
class FRCPassFFTBloom : public TRenderingCompositePassBase<2, 1>
{

public:

	/**
	* Used to verify the physical space kernel exits and has fully streamed in.
	*
	* @return true if the Context.View has a valid physical space kernel. 
	*/
	static bool HasValidPhysicalKernel(FPostprocessContext& Context);

public:

	// Constructor with filter kernel width
	FRCPassFFTBloom() {};

	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

	
private:

	// ----------- Primary Functions called by ::Process --------------------------------
 	/**
	* Will convolve the ePId_Input0 buffer with the kernel found in the context producing the output.
	* @return success code.  Will be false if the source image was too large to transform in group shared memory
	*/
	bool ConvolveImageWithKernel(FRenderingCompositePassContext& Context);

	/**
	* Will simply copy the input image to the output.  
	* To be called if we discover during Process() that the input image was too large to transform
	*/
	void PassThroughImage(FRenderingCompositePassContext& Context);
	
	// -----------------------------------------------------------------------------------
	
	
	// -----------Helper Member Functions-------------------------------------------------
	/** 
	* Initialize the domain member data based on the viewport and update
	* the cached pre-transformed kernel if required.
	*
	* @return Pointer to the Pre-Transformed Kernel 
	* will return null if the domain does not fit in group shared memory
	*/
	FSceneRenderTargetItem* InitDomainAndGetKernel(FRenderingCompositePassContext& Context);


	/**
	* Set up the dimensions and render targets.  Note, this is a function of the filter
	* kernel because the size of the kernel determines the amount of padding required
	* This initializes all the member data of this class
	*/
	void InitializeDomainParameters(FRenderingCompositePassContext& Context, const float KernelSupportPercent, const float MaxBufferScale);
	

	/**
	* On return the KernelTargetItem has been replaced with its 2D FFT. 
	* @param Context          - container for RHI and ShaderMap
	* @param KernelTargetItem - In/Out. The correct powers of two Physical Space Kernel / replaced by transform of kernel.
	*
	* @return success code.
	*/
	bool TransformKernelFFT(FRenderingCompositePassContext& Context, FSceneRenderTargetItem& KernelTargetItem);

	/**
	* 2D Convolution of the ImageRect region of the SrcTargetItem with the pre-convolved SpectralKernel
	*
	* @param Context        - container for RHI and ShaderMap
	* @param SpectralKernel - SRV for pre-transformed Kernel.
	* @param Tint           - A color to be applied to the result.  NB: this should be removed
	* @param SrcTexture     - SRV for the input buffer
	* @param ResultUAV      - UAV where the result is written.
	* @param PreFilter      - Defines how to boost pixels in the Src prior to convolution.
	*
	* @return success code.
	*/
	typedef FVector  FPreFilter;
	bool ConvolveWithKernel(FRenderingCompositePassContext& Context, const FTextureRHIRef& SpectralKernel, const FLinearColor& Tint,
		const FTextureRHIRef& SrcTexture, FUnorderedAccessViewRHIRef& ResultUAV,
		const FPreFilter& PreFilter = FPreFilter(TNumericLimits<float>::Max(), TNumericLimits<float>::Lowest(), 0.f));


	/**
	* Is this pass being run in experimental 1/2 resolution mode?
	* Currently the 1/2 resolution mode attempts to conserve 'energy'
	*/
	bool bHalfResolutionFFT() const
	{
		const FRenderingCompositeOutputRef* OutputRef = GetInput(ePId_Input1);
		return (OutputRef->GetPass() != NULL);
	}

private:
	// ----------- Member Data    -------------------------------------------------

	// Convient pointers to Pass input/output: set in Initialize() 
	const FSceneRenderTargetItem* InputTargetItem = NULL;
	FSceneRenderTargetItem* OutputTargetItem = NULL;

	// The size of the input buffer.
	FIntPoint InputBufferSize;

	// The sub-domain of the Input/Output buffers 
	// where the image lives, i.e. the region of interest
	FIntRect  ImageRect;

	// Image space, padded by black for kernel and  rounded up to powers of two
	// this defines the size of the FFT in each direction.
	FIntPoint FrequencySize;


	// The order of the two-dimensional transform.  This implicitly defines
	// the data layout in transform space for both the kernel and image transform
	bool bDoHorizontalFirst = false;

	
	// Flag to verify that Initialize() has been called.
	bool bIsInitialized = false;

};


