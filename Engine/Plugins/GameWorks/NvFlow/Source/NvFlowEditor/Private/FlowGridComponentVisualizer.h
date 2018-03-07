#pragma once

// NvFlow begin
#include "ComponentVisualizer.h"

class FFlowGridComponentVisualizer : public FComponentVisualizer
{
public:
	// Begin FComponentVisualizer interface
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	// End FComponentVisualizer interface
};
// NvFlow end