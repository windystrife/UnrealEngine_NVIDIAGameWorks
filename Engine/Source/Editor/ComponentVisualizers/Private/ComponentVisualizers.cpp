// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ComponentVisualizers.h"
#include "Modules/ModuleManager.h"
#include "Components/PrimitiveComponent.h"
#include "Editor/UnrealEdEngine.h"
#include "Components/AudioComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/DecalComponent.h"
#include "PhysicsEngine/RadialForceComponent.h"
#include "UnrealEdGlobals.h"

#include "Perception/PawnSensingComponent.h"
#include "PhysicsEngine/PhysicsSpringComponent.h"

#include "PointLightComponentVisualizer.h"
#include "SpotLightComponentVisualizer.h"
#include "AudioComponentVisualizer.h"
#include "RadialForceComponentVisualizer.h"
#include "ConstraintComponentVisualizer.h"
#include "PhysicalAnimationComponentVisualizer.h"
#include "SpringArmComponentVisualizer.h"
#include "Components/SplineComponent.h"
#include "SplineComponentVisualizer.h"
#include "Components/SplineMeshComponent.h"
#include "SplineMeshComponentVisualizer.h"
#include "DecalComponentVisualizer.h"
#include "SensingComponentVisualizer.h"
#include "SpringComponentVisualizer.h"
#include "StereoLayerComponentVisualizer.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "PhysicsEngine/PhysicalAnimationComponent.h"
#include "Components/StereoLayerComponent.h"
#include "ForceFeedbackComponentVisualizer.h"

// NvFlow begin
#include "GameWorks/RendererHooksNvFlow.h"
// NvFlow end

IMPLEMENT_MODULE( FComponentVisualizersModule, ComponentVisualizers );

void FComponentVisualizersModule::StartupModule()
{
	RegisterComponentVisualizer(UPointLightComponent::StaticClass()->GetFName(), MakeShareable(new FPointLightComponentVisualizer));
	RegisterComponentVisualizer(USpotLightComponent::StaticClass()->GetFName(), MakeShareable(new FSpotLightComponentVisualizer));
	RegisterComponentVisualizer(UAudioComponent::StaticClass()->GetFName(), MakeShareable(new FAudioComponentVisualizer));
	RegisterComponentVisualizer(UForceFeedbackComponent::StaticClass()->GetFName(), MakeShareable(new FForceFeedbackComponentVisualizer));
	RegisterComponentVisualizer(URadialForceComponent::StaticClass()->GetFName(), MakeShareable(new FRadialForceComponentVisualizer));
	RegisterComponentVisualizer(UPhysicsConstraintComponent::StaticClass()->GetFName(), MakeShareable(new FConstraintComponentVisualizer));
	RegisterComponentVisualizer(UPhysicalAnimationComponent::StaticClass()->GetFName(), MakeShareable(new FPhysicsAnimationComponentVisualizer));
	RegisterComponentVisualizer(USpringArmComponent::StaticClass()->GetFName(), MakeShareable(new FSpringArmComponentVisualizer));
	RegisterComponentVisualizer(USplineComponent::StaticClass()->GetFName(), MakeShareable(new FSplineComponentVisualizer));
	RegisterComponentVisualizer(USplineMeshComponent::StaticClass()->GetFName(), MakeShareable(new FSplineMeshComponentVisualizer));
	RegisterComponentVisualizer(UPawnSensingComponent::StaticClass()->GetFName(), MakeShareable(new FSensingComponentVisualizer));
	RegisterComponentVisualizer(UPhysicsSpringComponent::StaticClass()->GetFName(), MakeShareable(new FSpringComponentVisualizer));
	RegisterComponentVisualizer(UDecalComponent::StaticClass()->GetFName(), MakeShareable(new FDecalComponentVisualizer));
	RegisterComponentVisualizer(UStereoLayerComponent::StaticClass()->GetFName(), MakeShareable(new FStereoLayerComponentVisualizer));

	// NvFlow begin
	if (GEditorRendererHooksNvFlow)
	{
		GEditorRendererHooksNvFlow->NvFlowRegisterVisualizer(this);
	}
	// NvFlow end
}

void FComponentVisualizersModule::ShutdownModule()
{
	if(GUnrealEd != NULL)
	{
		// Iterate over all class names we registered for
		for(FName ClassName : RegisteredComponentClassNames)
		{
			GUnrealEd->UnregisterComponentVisualizer(ClassName);
		}
	}
}

void FComponentVisualizersModule::RegisterComponentVisualizer(FName ComponentClassName, TSharedPtr<FComponentVisualizer> Visualizer)
{
	if (GUnrealEd != NULL)
	{
		GUnrealEd->RegisterComponentVisualizer(ComponentClassName, Visualizer);
	}

	RegisteredComponentClassNames.Add(ComponentClassName);

	if (Visualizer.IsValid())
	{
		Visualizer->OnRegister();
	}
}
