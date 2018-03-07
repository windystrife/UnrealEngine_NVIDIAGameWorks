// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LandscapeComponent.h"
#include "LandscapeLayerInfoObject.h"
#include "Materials/MaterialInstance.h"
#include "LandscapeEdit.h"
#include "LandscapeRender.h"

FName FWeightmapLayerAllocationInfo::GetLayerName() const
{
	if (LayerInfo)
	{
		return LayerInfo->LayerName;
	}
	return NAME_None;
}

#if WITH_EDITOR

void FLandscapeEditToolRenderData::UpdateDebugColorMaterial(const ULandscapeComponent* const Component)
{
	Component->GetLayerDebugColorKey(DebugChannelR, DebugChannelG, DebugChannelB);
}

void FLandscapeEditToolRenderData::UpdateSelectionMaterial(int32 InSelectedType, const ULandscapeComponent* const Component)
{
	// Check selection
	if (SelectedType != InSelectedType && (SelectedType & ST_REGION) && !(InSelectedType & ST_REGION))
	{
		// Clear Select textures...
		if (DataTexture)
		{
			FLandscapeEditDataInterface LandscapeEdit(Component->GetLandscapeInfo());
			LandscapeEdit.ZeroTexture(DataTexture);
		}
	}

	SelectedType = InSelectedType;
}

void ULandscapeComponent::UpdateEditToolRenderData()
{
	FLandscapeComponentSceneProxy* LandscapeSceneProxy = (FLandscapeComponentSceneProxy*)SceneProxy;

	if (LandscapeSceneProxy != nullptr)
	{
		TArray<UMaterialInterface*> UsedMaterialsForVerification;
		const bool bGetDebugMaterials = true;
		GetUsedMaterials(UsedMaterialsForVerification, bGetDebugMaterials);

		ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
			UpdateEditToolRenderData,
			FLandscapeEditToolRenderData, LandscapeEditToolRenderData, EditToolRenderData,
			FLandscapeComponentSceneProxy*, LandscapeSceneProxy, LandscapeSceneProxy,
			TArray<UMaterialInterface*>, UsedMaterialsForVerification, UsedMaterialsForVerification,
			{
				LandscapeSceneProxy->EditToolRenderData = LandscapeEditToolRenderData;				
				LandscapeSceneProxy->SetUsedMaterialForVerification(UsedMaterialsForVerification);
			});
	}
}

#endif