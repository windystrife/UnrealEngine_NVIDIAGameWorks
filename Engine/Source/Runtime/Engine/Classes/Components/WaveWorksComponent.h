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
WaveWorksComponent.h: WaveWorks related classes.
=============================================================================*/

/**
* WaveWorks Component for QuadTree drawing
*/

#pragma once

#include "WaveWorksComponent.generated.h"

UCLASS(ClassGroup = Rendering, meta = (BlueprintSpawnableComponent), HideCategories = (Transform, Collision, Base, Object, PhysicsVolume, Lighting, Mobile, Physix))
class ENGINE_API UWaveWorksComponent : public UMeshComponent
{
	GENERATED_BODY()

public:
	/* Default constructor */
	UWaveWorksComponent(const class FObjectInitializer& ObjectInitializer);
	
	/* Begin UPrimitiveComponent interface */
	virtual void BeginPlay() override;
	virtual void BeginDestroy() override;
	virtual class FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	/* End UPrimitiveComponent interface */

	/**
	* Set wind direction
	*/
	UFUNCTION( BlueprintCallable, Category = WaveWorks )
	virtual void SetWindVector( const FVector2D& windVector );

	/**
	* Set wind speed (uint in 'm/s')
	*/
	UFUNCTION( BlueprintCallable, Category = WaveWorks )
	virtual void SetWindSpeed( const float& windSpeed );

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

	/** The waveworks asset */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = WaveWorks)
	class UWaveWorks* WaveWorksAsset;

	/** The matarial used to rendering waveworks */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = WaveWorks)
	class UMaterialInterface* WaveWorksMaterial;

	/** Dimension of a single square patch, default to 128x128 grids */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = WaveWorks)
	int32 MeshDim;

	/** The size of the smallest permissible leaf quad in world space (i.e. the size of a lod zero patch) (in m). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = WaveWorks)
	float MinPatchLength;

	/** The lod of the root patch used for frustum culling and mesh lodding. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = WaveWorks)
	int32 AutoRootLOD;

	/** The upper limit of the pixel number a grid can cover in screen space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = WaveWorks)
	float UpperGridCoverage;

	/** The offset required to place the surface at sea level */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = WaveWorks)
	float SeaLevel;

	/** The tessellation LOD scale parameter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = WaveWorks)
	float TessellationLOD;

	/** The tessellation LOD scale parameter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = WaveWorks)
	bool bUsesGlobalDistanceField;
};