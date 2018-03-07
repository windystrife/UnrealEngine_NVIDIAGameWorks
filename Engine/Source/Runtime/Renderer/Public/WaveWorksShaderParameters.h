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

/*=============================================================================
	WaveWorksShaderParameters.h: WaveWorks rendering
=============================================================================*/

#pragma once

#include "WaveWorksResource.h"
#include "Engine/WaveWorks.h"

/** Shader parameters needed for WaveWorks. */
class FWaveWorksShaderParameters
{
public:
	FWaveWorksShaderParameters() 
	:	bIsBound(false)
	{}

	void Bind(const FShaderParameterMap& ParameterMap, EShaderFrequency Frequency, EShaderParameterFlags Flags = SPF_Optional);

	template<typename ShaderRHIParamRef>
	void Set(FRHICommandList& RHICmdList, 
		const FMaterialShader* Shader, 
		const ShaderRHIParamRef ShaderRHI, 
		const FSceneView& View, 
		FWaveWorksResource* WaveWorksResource
		) const
	{
		if (bIsBound)
		{
			SetUniformBufferParameter(RHICmdList, ShaderRHI, Shader->GetUniformBufferParameter<FWaveWorksShorelineUniformParameters>(), WaveWorksResource->GetShorelineUniformBuffer());

			const UWaveWorks* WaveWorks = WaveWorksResource->GetOwnerWaveWorks();
			if (ShorelineDistanceFieldTexture.IsBound() && WaveWorks != nullptr && WaveWorks->ShorelineDistanceFieldTexture != nullptr)
			{
				FSamplerStateRHIParamRef SamplerStateLinear = TStaticSamplerState<SF_Trilinear>::GetRHI();
				FTextureRHIParamRef shorelineDFTex = WaveWorks->ShorelineDistanceFieldTexture->Resource->TextureRHI.GetReference();
				SetTextureParameter(RHICmdList, ShaderRHI, ShorelineDistanceFieldTexture, ShorelineDistanceFieldTextureSampler, SamplerStateLinear, shorelineDFTex);
			}
		}
	}

	bool IsBound() const { return bIsBound; }

	friend FArchive& operator<<(FArchive& Ar, FWaveWorksShaderParameters& Parameters)
	{
		Parameters.Serialize(Ar);
		return Ar;
	}

	void Serialize(FArchive& Ar);

public:

	/** mapping of WaveWorks shader input (see RHIGetWaveWorksShaderInput()) to resource slot */
	TArray<uint32> ShaderInputMappings;

	/** mapping of QuardTree shader input (see RHIGetWaveWorksQuadTreeShaderInput()) to resource slot */
	TArray<uint32> QuadTreeShaderInputMappings;

private:

	/** true if parameters bound */
	bool bIsBound;

	/** shoreline distance field texture */
	FShaderResourceParameter ShorelineDistanceFieldTexture;

	/** shoreline distance field texture sampler */
	FShaderResourceParameter ShorelineDistanceFieldTextureSampler;
};
