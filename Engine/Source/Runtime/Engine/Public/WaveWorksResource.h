/*
* This code contains NVIDIA Confidential Information and is disclosed
* under the Mutual Non-Disclosure Agreement.
*
* Notice
* ALL NVIDIA DESIGN SPECIFICATIONS AND CODE ("MATERIALS") ARE PROVIDED "AS IS" NVIDIA MAKES
* NO REPRESENTATIONS, WARRANTIES, EXPRESSED, IMPLIED, STATUTORY, OR OTHERWISE WITH RESPECT TO
* THE MATERIALS, AND EXPRESSLY DISCLAIMS ANY IMPLIED WARRANTIES OF NONINFRINGEMENT,
* MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
*
* NVIDIA Corporation assumes no responsibility for the consequences of use of such
* information or for any infringement of patents or other rights of third parties that may
* result from its use. No license is granted by implication or otherwise under any patent
* or patent rights of NVIDIA Corporation. No third party distribution is allowed unless
* expressly authorized by NVIDIA.  Details are subject to change without notice.
* This code supersedes and replaces all information previously supplied.
* NVIDIA Corporation products are not authorized for use as critical
* components in life support devices or systems without express written approval of
* NVIDIA Corporation.
*
* Copyright ?2008- 2016 NVIDIA Corporation. All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property and proprietary
* rights in and to this software and related documentation and any modifications thereto.
* Any use, reproduction, disclosure or distribution of this software and related
* documentation without an express license agreement from NVIDIA Corporation is
* strictly prohibited.
*/

#pragma once

//#include "GFSDK_WaveWorks.h"
#include "TextureResource.h"
#include "WaveWorksRender.h"

/*=============================================================================
	WaveWorksResource.h: WaveWorks related classes.
=============================================================================*/

/**
 * FWaveWorksResource type for WaveWorks resources.
 */
class FWaveWorksResource : public FRenderResource, public FDeferredUpdateResource
{
public:

	/**
	 * Constructor
	 * @param InOwner - WaveWorks object to create a resource for
	 */
	FWaveWorksResource(const class UWaveWorks* InOwner)
		: Owner( InOwner )
		, bAddedToDeferredUpdateList(false)
	{
	}

	/**
	 * Initializes the dynamic RHI resource and/or RHI render target used by this resource.
	 * Called when the resource is initialized, or when reseting all RHI resources.
	 * Resources that need to initialize after a D3D device reset must implement this function.
	 * This is only called by the rendering thread.
	 */
	virtual void InitDynamicRHI();

	/**
	 * Releases the dynamic RHI resource and/or RHI render target resources used by this resource.
	 * Called when the resource is released, or when reseting all RHI resources.
	 * Resources that need to release before a D3D device reset must implement this function.
	 * This is only called by the rendering thread.
	 */
	virtual void ReleaseDynamicRHI();

	/**
	* @return WaveWorksRHI for rendering
	*/
	FORCEINLINE FWaveWorksRHIRef GetWaveWorksRHI() { return WaveWorksRHI; }

	/**
	* @return Owner WaveWorks
	*/
	FORCEINLINE const class UWaveWorks* GetOwnerWaveWorks() { return Owner; }

	/**
	* @return Shoreline Uniform Buffer
	*/
	FORCEINLINE FWaveWorksShorelineUniformBufferRef GetShorelineUniformBuffer() { return WaveWorksShorelineUniformBuffer; }

	/**
	* set Shoreline Uniform Buffer
	*/
	FORCEINLINE void SetShorelineUniformBuffer(FWaveWorksShorelineUniformBufferRef ShorelineBuffer) { WaveWorksShorelineUniformBuffer = ShorelineBuffer; }

	/**
	 * Generates the next frame of the WaveWorks simuation
	 */
	virtual void UpdateDeferredResource(FRHICommandListImmediate &, bool bClearRenderTarget = true);

	/**
	* The estimate is based on significant wave height with 4x confidence: http://en.wikipedia.org/wiki/Significant_wave_height
	* we take it as our shore wave, this can be used to get gerstner amplitude
	*/
	float GetGerstnerAmplitude();

	void CustomAddToDeferredUpdateList();
	void CustomRemoveFromDeferredUpdateList();

private:
	/** The UWaveWorks which this resource represents. */
	const class UWaveWorks* Owner;

	/** WaveWorks resource used for rendering with and resolving to */
	FWaveWorksRHIRef WaveWorksRHI;	

	/** Whether waveworks resource added to deferred update list */
	bool bAddedToDeferredUpdateList;

	/** The Uniform Buffer which used for gerstner wave. */
	FWaveWorksShorelineUniformBufferRef WaveWorksShorelineUniformBuffer;
};