// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SpotLightComponentVisualizer.h"
#include "SceneManagement.h"
#include "Components/SpotLightComponent.h"



void FSpotLightComponentVisualizer::DrawVisualization( const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI )
{
	if(View->Family->EngineShowFlags.LightRadius)
	{
		const USpotLightComponent* SpotLightComp = Cast<const USpotLightComponent>(Component);
		if(SpotLightComp != NULL)
		{
			FTransform TransformNoScale = SpotLightComp->GetComponentTransform();
			TransformNoScale.RemoveScaling();

			// Draw point light source shape
			DrawWireCapsule(PDI, TransformNoScale.GetTranslation(), TransformNoScale.GetUnitAxis( EAxis::X ), TransformNoScale.GetUnitAxis( EAxis::Y ), TransformNoScale.GetUnitAxis( EAxis::Z ),
							FColor(231, 239, 0, 255), SpotLightComp->SourceRadius, 0.5f * SpotLightComp->SourceLength + SpotLightComp->SourceRadius, 25, SDPG_World);

			// Draw outer light cone
			DrawWireSphereCappedCone(PDI, TransformNoScale, SpotLightComp->AttenuationRadius, SpotLightComp->OuterConeAngle, 32, 8, 10, FColor(200, 255, 255), SDPG_World);

			// Draw inner light cone (if non zero)
			if(SpotLightComp->InnerConeAngle > KINDA_SMALL_NUMBER)
			{
				DrawWireSphereCappedCone(PDI, TransformNoScale, SpotLightComp->AttenuationRadius, SpotLightComp->InnerConeAngle, 32, 8, 10, FColor(150, 200, 255), SDPG_World);
			}
		}
	}
}
