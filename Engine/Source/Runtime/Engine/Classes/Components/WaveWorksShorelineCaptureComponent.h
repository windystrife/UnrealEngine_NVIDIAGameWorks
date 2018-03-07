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

#include "Components/SceneCaptureComponent2D.h"
#include "WaveWorksShorelineCaptureComponent.generated.h"

/**
 *	Used to capture a 'snapshot' of the scene from a single plane and feed it to a render target.
 */
UCLASS(hidecategories=(ColorGrading, Tonemapper, Lens, RenderingFeatures, PostProcessVolume, LOD, Collision, Object, Physics, SceneComponent), ClassGroup=Rendering, MinimalAPI, editinlinenew, meta=(BlueprintSpawnableComponent))
class UWaveWorksShorelineCaptureComponent : public USceneCaptureComponent2D
{
	GENERATED_UCLASS_BODY()

	/** Max Pixels to shoreline */
	UPROPERTY(interp, Category = SceneCapture, meta = (DisplayName = "MaxPixelsToShoreline", UIMin = "0", ClampMin = "0"))
	int MaxPixelsToShoreline;

	//~ Begin UObject Interface
#if WITH_EDITOR
	virtual bool CanEditChange(const UProperty* InProperty) const override;
#endif // WITH_EDITOR

public:
	ENGINE_API void GenerateShorelineDFTexture();
};
