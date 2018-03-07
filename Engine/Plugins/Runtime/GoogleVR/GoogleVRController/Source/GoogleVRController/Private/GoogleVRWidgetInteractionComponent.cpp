// Copyright 2017 Google Inc.


#include "Classes/GoogleVRWidgetInteractionComponent.h"
#include "GoogleVRController.h"
#include "Components/WidgetInteractionComponent.h"
#include "Components/WidgetComponent.h"

UGoogleVRWidgetInteractionComponent::UGoogleVRWidgetInteractionComponent(const FObjectInitializer& ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UGoogleVRWidgetInteractionComponent::UpdateState(const FHitResult& HitResult)
{
	// Make sure that the interaction source is custom.
	InteractionSource = EWidgetInteractionSource::Custom;

	// Set the custom hit result.
	SetCustomHitResult(HitResult);

	// Simulate the pointer movement.
	SimulatePointerMovement();
}

FWidgetPath UGoogleVRWidgetInteractionComponent::FindHoveredWidgetPath(const FWidgetTraceResult& TraceResult) const
{
	if (TraceResult.HitWidgetComponent != nullptr)
	{
		// This does not need to match the radius of the pointer.
		// LastHitResult.ImpactPoint already represents the location that was hit on the widget
		// based upon the actual radius of the pointer.
		// However, when that impact point is at the very edge the widget, GetHitWidgetPath
		// sometimes fails to find a valid path when the radius is zero.
		// To fix this, we set the radius to a small non-zero value.
		const float WidgetCursorRadius = 5.0f;
		return FWidgetPath(TraceResult.HitWidgetComponent->GetHitWidgetPath(TraceResult.LocalHitLocation, /*bIgnoreEnabledStatus*/ false, WidgetCursorRadius));
	}
	else
	{
		return FWidgetPath();
	}
}