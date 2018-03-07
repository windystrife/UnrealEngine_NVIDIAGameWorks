//
// Copyright (C) Valve Corporation. All rights reserved.
//

#include "PhononProbeComponentVisualizer.h"
#include "PhononProbeComponent.h"
#include "SceneView.h"
#include "SceneManagement.h"

namespace SteamAudio
{
	void FPhononProbeComponentVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
	{
		const UPhononProbeComponent* ProbeComponent = Cast<const UPhononProbeComponent>(Component);
		if (ProbeComponent)
		{
			const FColor Color(0, 153, 255);
			for (const auto& ProbeLocation : ProbeComponent->ProbeLocations)
			{
				PDI->DrawPoint(ProbeLocation, Color, 5, SDPG_World);
			}
		}
	}
}