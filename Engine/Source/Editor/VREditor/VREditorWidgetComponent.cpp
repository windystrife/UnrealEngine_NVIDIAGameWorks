// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "VREditorWidgetComponent.h"

UVREditorWidgetComponent::UVREditorWidgetComponent()
	: Super(),
	DrawingPolicy(EVREditorWidgetDrawingPolicy::Always),
	bIsHovering(false),
	bHasEverDrawn(false)
{
	bSelectable = false;
}

bool UVREditorWidgetComponent::ShouldDrawWidget() const
{
	if ( DrawingPolicy == EVREditorWidgetDrawingPolicy::Always ||
		(DrawingPolicy == EVREditorWidgetDrawingPolicy::Hovering && bIsHovering) ||
		!bHasEverDrawn )
	{
		return Super::ShouldDrawWidget();
	}

	return false;
}

void UVREditorWidgetComponent::DrawWidgetToRenderTarget(float DeltaTime)
{
	bHasEverDrawn = true;

	Super::DrawWidgetToRenderTarget(DeltaTime);
}
