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

#include "StaticMeshComponent.h"
#include "WaveWorksStaticMeshComponent.generated.h"

/**
 * WaveWorksStaticMeshComponent is used to create an instance of a WaveWorks Water Plane
 * A static mesh is a piece of geometry that consists of a static set of polygons.
 */
UCLASS(ClassGroup=(Rendering, Common), hidecategories=(Object,Activation,"Components|Activation"), ShowCategories=(Mobility), editinlinenew, meta=(BlueprintSpawnableComponent))
class ENGINE_API UWaveWorksStaticMeshComponent : public UStaticMeshComponent
{
	GENERATED_UCLASS_BODY()

	/**
	* Set wind direction
	*/
	UFUNCTION(BlueprintCallable, Category = WaveWorks)
	virtual void SetWindVector(const FVector2D& windVector);

	/**
	* Set wind speed (uint in 'm/s')
	*/
	UFUNCTION(BlueprintCallable, Category = WaveWorks)
	virtual void SetWindSpeed(const float& windSpeed);

	/**
	* Get waveworks's displacement
	* @Param InSamplePoints : sample points, the coordinate's unit is 'm'.
	* @Result VectorArrayDelegate : the callback delegate when sample displacement completed in rendering thread.
	*/
	virtual void SampleDisplacements(TArray<FVector> InSamplePoints, FWaveWorksSampleDisplacementsDelegate VectorArrayDelegate);

	/**
	* Get waveworks's raycast intersect point
	* @Param InOriginPoint : origin raycast point, the coordinate's unit is 'm'.
	* @Param InDirection : raycast direction
	* @Result FWaveWorksRaycastResultDelegate : the callback delegate when raycast completed in rendering thread.
	*/
	virtual void GetIntersectPointWithRay(FVector InOriginPoint, FVector InDirection, FWaveWorksRaycastResultDelegate OnRecieveIntersectPointDelegate);

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WaveWorks")
	class UWaveWorks* WaveWorksAsset;

public:
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
};



