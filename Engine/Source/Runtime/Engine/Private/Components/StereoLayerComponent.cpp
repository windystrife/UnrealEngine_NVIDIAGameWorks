// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/StereoLayerComponent.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "TextureResource.h"
#include "Engine/Texture.h"
#include "IStereoLayers.h"
#include "StereoRendering.h"

UStereoLayerComponent::UStereoLayerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bLiveTexture(false)
	, bSupportsDepth(false)
	, bNoAlphaChannel(false)
	, Texture(nullptr)
	, LeftTexture(nullptr)
	, bQuadPreserveTextureRatio(false)
	, QuadSize(FVector2D(100.0f, 100.0f))
	, UVRect(FBox2D(FVector2D(0.0f, 0.0f), FVector2D(1.0f, 1.0f)))
	, CylinderRadius(100)
	, CylinderOverlayArc(100)
	, CylinderHeight(50)
	, StereoLayerType(SLT_FaceLocked)
	, StereoLayerShape(SLSH_QuadLayer)
	, Priority(0)
	, bIsDirty(true)
	, bTextureNeedsUpdate(false)
	, LayerId(0)
	, LastTransform(FTransform::Identity)
	, bLastVisible(false)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UStereoLayerComponent::BeginDestroy()
{
	Super::BeginDestroy();

	IStereoLayers* StereoLayers;
	if (LayerId && GEngine->StereoRenderingDevice.IsValid() && (StereoLayers = GEngine->StereoRenderingDevice->GetStereoLayers()) != nullptr)
	{
		StereoLayers->DestroyLayer(LayerId);
		LayerId = 0;
	}
}

void UStereoLayerComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent( DeltaTime, TickType, ThisTickFunction );

	IStereoLayers* StereoLayers;
	if (!GEngine->StereoRenderingDevice.IsValid() || (StereoLayers = GEngine->StereoRenderingDevice->GetStereoLayers()) == nullptr || !Texture)
	{
		return;
	}

	FTransform Transform;
	if (StereoLayerType == SLT_WorldLocked)
	{
		Transform = GetComponentTransform();
	}
	else
	{
		Transform = GetRelativeTransform();
	}
	
	// If the transform changed dirty the layer and push the new transform
	if (!bIsDirty && (bLastVisible != bVisible || FMemory::Memcmp(&LastTransform, &Transform, sizeof(Transform)) != 0))
	{
		bIsDirty = true;
	}

	bool bCurrVisible = bVisible;
	if (!Texture || !Texture->Resource)
	{
		bCurrVisible = false;
	}

	if (bIsDirty)
	{
		if (!bCurrVisible)
		{
			if (LayerId)
			{
				StereoLayers->DestroyLayer(LayerId);
				LayerId = 0;
			}
		}
		else
		{
			IStereoLayers::FLayerDesc LayerDsec;
			LayerDsec.Priority = Priority;
			LayerDsec.QuadSize = QuadSize;
			LayerDsec.UVRect = UVRect;
			LayerDsec.Transform = Transform;
			if (Texture)
			{
				LayerDsec.Texture = Texture->Resource->TextureRHI;
			}
			if (LeftTexture)
			{
				LayerDsec.LeftTexture = LeftTexture->Resource->TextureRHI;
			}
			LayerDsec.CylinderRadius = CylinderRadius; 
			LayerDsec.CylinderOverlayArc = CylinderOverlayArc;
			LayerDsec.CylinderHeight = CylinderHeight;
			
			LayerDsec.Flags |= (bLiveTexture) ? IStereoLayers::LAYER_FLAG_TEX_CONTINUOUS_UPDATE : 0;
			LayerDsec.Flags |= (bNoAlphaChannel) ? IStereoLayers::LAYER_FLAG_TEX_NO_ALPHA_CHANNEL : 0;
			LayerDsec.Flags |= (bQuadPreserveTextureRatio) ? IStereoLayers::LAYER_FLAG_QUAD_PRESERVE_TEX_RATIO : 0;
			LayerDsec.Flags |= (bSupportsDepth) ? IStereoLayers::LAYER_FLAG_SUPPORT_DEPTH : 0;

			switch (StereoLayerType)
			{
			case SLT_WorldLocked:
				LayerDsec.PositionType = IStereoLayers::WorldLocked;
				break;
			case SLT_TrackerLocked:
				LayerDsec.PositionType = IStereoLayers::TrackerLocked;
				break;
			case SLT_FaceLocked:
				LayerDsec.PositionType = IStereoLayers::FaceLocked;
				break;
			}

			switch (StereoLayerShape)
			{
			case SLSH_QuadLayer:
				LayerDsec.ShapeType = IStereoLayers::QuadLayer;
				break;

			case SLSH_CylinderLayer:
				LayerDsec.ShapeType = IStereoLayers::CylinderLayer;
				break;

			case SLSH_CubemapLayer:
				LayerDsec.ShapeType = IStereoLayers::CubemapLayer;
				break;
			default:
				break;
			}

			if (LayerId)
			{
				StereoLayers->SetLayerDesc(LayerId, LayerDsec);
			}
			else
			{
				LayerId = StereoLayers->CreateLayer(LayerDsec);
			}
		}
		LastTransform = Transform;
		bLastVisible = bCurrVisible;
		bIsDirty = false;
	}

	if (bTextureNeedsUpdate && LayerId)
	{
		StereoLayers->MarkTextureForUpdate(LayerId);
		bTextureNeedsUpdate = false;
	}
}

void UStereoLayerComponent::SetTexture(UTexture* InTexture)
{
	if (Texture == InTexture)
	{
		return;
	}

	Texture = InTexture;
	bIsDirty = true;
}

void UStereoLayerComponent::SetQuadSize(FVector2D InQuadSize)
{
	if (QuadSize == InQuadSize)
	{
		return;
	}

	QuadSize = InQuadSize;
	bIsDirty = true;
}

void UStereoLayerComponent::SetUVRect(FBox2D InUVRect)
{
	if (UVRect == InUVRect)
	{
		return;
	}

	UVRect = InUVRect;
	bIsDirty = true;
}

void UStereoLayerComponent::SetPriority(int32 InPriority)
{
	if (Priority == InPriority)
	{
		return;
	}

	Priority = InPriority;
	bIsDirty = true;
}

void UStereoLayerComponent::MarkTextureForUpdate()
{
	bTextureNeedsUpdate = true;
}

