//
// Copyright (C) Valve Corporation. All rights reserved.
//

#include "PhononSourceComponentVisualizer.h"
#include "PhononSourceComponent.h"
#include "SceneView.h"
#include "SceneManagement.h"

namespace SteamAudio
{
	void FPhononSourceComponentVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
	{
		const UPhononSourceComponent* AttenuatedComponent = Cast<const UPhononSourceComponent>(Component);
		if (AttenuatedComponent != nullptr)
		{
			const FColor OuterRadiusColor(0, 153, 255);
			auto Translation = AttenuatedComponent->GetComponentTransform().GetTranslation();

			DrawWireSphereAutoSides(PDI, Translation, OuterRadiusColor, AttenuatedComponent->BakingRadius, SDPG_World);
		}
	}
}