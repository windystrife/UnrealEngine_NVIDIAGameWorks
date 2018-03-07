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
WaveWorksComponent.cpp: UWaveWorksComponent implementation.
=============================================================================*/

#include "Components/WaveWorksComponent.h"
#include "GFSDK_WaveWorks.h"
#include "Engine/WaveWorks.h"
#include "WaveWorksRender.h"
#include "WaveWorksResource.h"
#include "Engine/WaveWorksShorelineCapture.h"

/*-----------------------------------------------------------------------------
UWaveWorksComponent
-----------------------------------------------------------------------------*/

UWaveWorksComponent::UWaveWorksComponent(const class FObjectInitializer& PCIP)
	: Super(PCIP)	
	, MeshDim(128)
	, MinPatchLength(128.0f)
	, AutoRootLOD(20)
	, UpperGridCoverage(64.0f)
	, SeaLevel(0.0f)
	, TessellationLOD(100.0f)
	, bUsesGlobalDistanceField(false)
	, WaveWorksAsset(nullptr)
{
	PrimaryComponentTick.bCanEverTick = true;
}

FPrimitiveSceneProxy* UWaveWorksComponent::CreateSceneProxy()
{
	FWaveWorksSceneProxy* WaveWorksSceneProxy = nullptr;

	if (WaveWorksMaterial != nullptr && WaveWorksAsset != nullptr)
	{		
		WaveWorksSceneProxy = new FWaveWorksSceneProxy(this, WaveWorksAsset);
		if (nullptr == WaveWorksSceneProxy->GetQuadTreeHandle())
		{
			WaveWorksSceneProxy->AttemptCreateQuadTree();
		}
	}

	return WaveWorksSceneProxy;
}

void UWaveWorksComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UWaveWorksComponent::BeginDestroy()
{
	Super::BeginDestroy();
}

FBoxSphereBounds UWaveWorksComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FWaveWorksResource* waveWorksResource = (WaveWorksAsset != nullptr) ? WaveWorksAsset->GetWaveWorksResource() : nullptr;
	float zExtent = (waveWorksResource) ? waveWorksResource->GetGerstnerAmplitude() : 100.0f;

	FBoxSphereBounds NewBounds;
	NewBounds.Origin = FVector::ZeroVector;
	NewBounds.BoxExtent = FVector(HALF_WORLD_MAX, HALF_WORLD_MAX, zExtent);
	NewBounds.SphereRadius = FMath::Sqrt(3.0f * FMath::Square(HALF_WORLD_MAX));
	return NewBounds;
}

void UWaveWorksComponent::SetWindVector( const FVector2D& windVector )
{
	if( !WaveWorksAsset )	
		return;

	WaveWorksAsset->WindDirection = windVector;
}

void UWaveWorksComponent::SetWindSpeed( const float& windSpeed )
{
	if( !WaveWorksAsset )	
		return;

	WaveWorksAsset->WindSpeed = windSpeed;
}

void UWaveWorksComponent::SampleDisplacements(TArray<FVector> InSamplePoints, FWaveWorksSampleDisplacementsDelegate VectorArrayDelegate)
{
	if ( !SceneProxy )
		return;
	
	FWaveWorksSceneProxy* WaveWorksSceneProxy = static_cast<FWaveWorksSceneProxy*>(SceneProxy);
	WaveWorksSceneProxy->SampleDisplacements_GameThread(InSamplePoints, VectorArrayDelegate);
}

void UWaveWorksComponent::GetIntersectPointWithRay(
	FVector InOriginPoint,
	FVector InDirection, 
	FWaveWorksRaycastResultDelegate OnRecieveIntersectPointDelegate)
{
	if ( !SceneProxy )
		return;

	FWaveWorksSceneProxy* WaveWorksSceneProxy = static_cast<FWaveWorksSceneProxy*>(SceneProxy);
	WaveWorksSceneProxy->GetIntersectPointWithRay_GameThread(InOriginPoint, InDirection, SeaLevel, OnRecieveIntersectPointDelegate);
}

void UWaveWorksComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	if ( SceneProxy )
	{
		FWaveWorksSceneProxy* WaveWorksSceneProxy = static_cast<FWaveWorksSceneProxy*>(SceneProxy);
		if (nullptr == WaveWorksSceneProxy->GetQuadTreeHandle())
		{
			WaveWorksSceneProxy->AttemptCreateQuadTree();
		}		
	}
}