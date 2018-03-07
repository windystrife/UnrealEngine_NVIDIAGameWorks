// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ComponentVisualizerManager.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "Editor/UnrealEdEngine.h"
#include "UnrealEdGlobals.h"
#include "SEditorViewport.h"


FComponentVisualizerManager::FComponentVisualizerManager()
	: EditedVisualizerViewportClient(nullptr)
{
}

/** Handle a click on the specified editor viewport client */
bool FComponentVisualizerManager::HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click)
{
	bool bHandled = HandleProxyForComponentVis(InViewportClient, HitProxy, Click);
	if (bHandled && Click.GetKey() == EKeys::RightMouseButton)
	{
		TSharedPtr<SWidget> MenuWidget = GenerateContextMenuForComponentVis();
		if (MenuWidget.IsValid())
		{
			TSharedPtr<SEditorViewport> ViewportWidget = InViewportClient->GetEditorViewportWidget();
			if (ViewportWidget.IsValid())
			{
				FSlateApplication::Get().PushMenu(
					ViewportWidget.ToSharedRef(),
					FWidgetPath(),
					MenuWidget.ToSharedRef(),
					FSlateApplication::Get().GetCursorPos(),
					FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));

				return true;
			}
		}
	}

	return false;
}

bool FComponentVisualizerManager::HandleProxyForComponentVis(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click)
{
	if (HitProxy && HitProxy->IsA(HComponentVisProxy::StaticGetType()))
	{
		HComponentVisProxy* VisProxy = (HComponentVisProxy*)HitProxy;
		const UActorComponent* ClickedComponent = VisProxy->Component.Get();
		if (ClickedComponent != NULL)
		{
			TSharedPtr<FComponentVisualizer> Visualizer = GUnrealEd->FindComponentVisualizer(ClickedComponent->GetClass());
			if (Visualizer.IsValid())
			{
				bool bIsActive = Visualizer->VisProxyHandleClick(InViewportClient, VisProxy, Click);
				if (bIsActive)
				{
					// call EndEditing on any currently edited visualizer, if we are going to change it
					TSharedPtr<FComponentVisualizer> EditedVisualizer = EditedVisualizerPtr.Pin();
					if (EditedVisualizer.IsValid() && Visualizer.Get() != EditedVisualizer.Get())
					{
						EditedVisualizer->EndEditing();
					}

					EditedVisualizerPtr = Visualizer;
					EditedVisualizerViewportClient = InViewportClient;
					return true;
				}
			}
		}
	}
	else
	{
		ClearActiveComponentVis();
	}

	return false;
}

void FComponentVisualizerManager::ClearActiveComponentVis()
{
	TSharedPtr<FComponentVisualizer> EditedVisualizer = EditedVisualizerPtr.Pin();

	if (EditedVisualizer.IsValid())
	{
		EditedVisualizer->EndEditing();
	}

	EditedVisualizerPtr.Reset();
	EditedVisualizerViewportClient = nullptr;
}

bool FComponentVisualizerManager::HandleInputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event) const
{
	TSharedPtr<FComponentVisualizer> EditedVisualizer = EditedVisualizerPtr.Pin();

	if (EditedVisualizer.IsValid())
	{
		if(EditedVisualizer->HandleInputKey(ViewportClient, Viewport, Key, Event))
		{
			return true;
		}
	}

	return false;
}

bool FComponentVisualizerManager::HandleInputDelta(FEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale) const
{
	TSharedPtr<FComponentVisualizer> EditedVisualizer = EditedVisualizerPtr.Pin();

	if (EditedVisualizer.IsValid() && EditedVisualizerViewportClient == InViewportClient && InViewportClient->GetCurrentWidgetAxis() != EAxisList::None)
	{
		if (EditedVisualizer->HandleInputDelta(InViewportClient, InViewport, InDrag, InRot, InScale))
		{
			return true;
		}
	}

	return false;
}


bool FComponentVisualizerManager::GetWidgetLocation(const FEditorViewportClient* ViewportClient, FVector& OutLocation) const
{
	TSharedPtr<FComponentVisualizer> EditedVisualizer = EditedVisualizerPtr.Pin();

	if (EditedVisualizer.IsValid())
	{
		return EditedVisualizer->GetWidgetLocation(ViewportClient, OutLocation);
	}

	return false;
}


bool FComponentVisualizerManager::GetCustomInputCoordinateSystem(const FEditorViewportClient* ViewportClient, FMatrix& OutMatrix) const
{
	TSharedPtr<FComponentVisualizer> EditedVisualizer = EditedVisualizerPtr.Pin();

	if (EditedVisualizer.IsValid())
	{
		return EditedVisualizer->GetCustomInputCoordinateSystem(ViewportClient, OutMatrix);
	}

	return false;
}


TSharedPtr<SWidget> FComponentVisualizerManager::GenerateContextMenuForComponentVis() const
{
	TSharedPtr<FComponentVisualizer> EditedVisualizer = EditedVisualizerPtr.Pin();

	if (EditedVisualizer.IsValid())
	{
		return EditedVisualizer->GenerateContextMenu();
	}

	return TSharedPtr<SWidget>();
}


bool FComponentVisualizerManager::IsActive() const
{
	TSharedPtr<FComponentVisualizer> EditedVisualizer = EditedVisualizerPtr.Pin();
	return EditedVisualizer.IsValid();
}


bool FComponentVisualizerManager::IsVisualizingArchetype() const
{
	TSharedPtr<FComponentVisualizer> EditedVisualizer = EditedVisualizerPtr.Pin();
	return EditedVisualizer.IsValid() && EditedVisualizer->IsVisualizingArchetype();
}
