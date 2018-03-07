// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Slate/SWorldWidgetScreenLayer.h"
#include "Widgets/Layout/SBox.h"
#include "Components/WidgetComponent.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/SViewport.h"
#include "Slate/SGameLayerManager.h"

void SWorldWidgetScreenLayer::Construct(const FArguments& InArgs, const FLocalPlayerContext& InPlayerContext)
{
	PlayerContext = InPlayerContext;

	bCanSupportFocus = false;
	DrawSize = FVector2D(0, 0);
	Pivot = FVector2D(0.5f, 0.5f);

	ChildSlot
	[
		SAssignNew(Canvas, SConstraintCanvas)
	];
}

void SWorldWidgetScreenLayer::SetWidgetDrawSize(FVector2D InDrawSize)
{
	DrawSize = InDrawSize;
}

void SWorldWidgetScreenLayer::SetWidgetPivot(FVector2D InPivot)
{
	Pivot = InPivot;
}

void SWorldWidgetScreenLayer::AddComponent(USceneComponent* Component, TSharedPtr<SWidget> Widget)
{
	if ( Component && Widget.IsValid() )
	{
		FComponentEntry& Entry = ComponentMap.FindOrAdd(Component);
		Entry.Component = Component;
		Entry.WidgetComponent = Cast<UWidgetComponent>(Component);
		Entry.Widget = Widget;

		Canvas->AddSlot()
		.Expose(Entry.Slot)
		[
			SAssignNew(Entry.ContainerWidget, SBox)
			[
				Widget.ToSharedRef()
			]
		];
	}
}

void SWorldWidgetScreenLayer::RemoveComponent(USceneComponent* Component)
{
	if ( Component )
	{
		if ( FComponentEntry* Entry = ComponentMap.Find(Component) )
		{
			if ( Entry->Widget.IsValid() )
			{
				Canvas->RemoveSlot(Entry->ContainerWidget.ToSharedRef());
			}

			ComponentMap.Remove(Component);
		}
	}
}

void SWorldWidgetScreenLayer::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	TArray<USceneComponent*, TInlineAllocator<1>> DeadComponents;

	if ( APlayerController* PlayerController = PlayerContext.GetPlayerController() )
	{
		if ( UGameViewportClient* ViewportClient = PlayerController->GetWorld()->GetGameViewport() )
		{
			const FGeometry& ViewportGeometry = ViewportClient->GetGameLayerManager()->GetViewportWidgetHostGeometry();

			for ( auto It = ComponentMap.CreateIterator(); It; ++It )
			{
				FComponentEntry& Entry = It.Value();

				if ( USceneComponent* SceneComponent = Entry.Component.Get() )
				{
					FVector WorldLocation = SceneComponent->GetComponentLocation();

					FVector ViewportPosition;
					const bool bProjected = UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPositionWithDistance(PlayerController, WorldLocation, ViewportPosition);

					if ( bProjected )
					{
						Entry.ContainerWidget->SetVisibility(EVisibility::SelfHitTestInvisible);

						if ( SConstraintCanvas::FSlot* CanvasSlot = Entry.Slot )
						{
							FVector2D AbsoluteProjectedLocation = ViewportGeometry.LocalToAbsolute(FVector2D(ViewportPosition.X, ViewportPosition.Y));
							FVector2D LocalPosition = AllottedGeometry.AbsoluteToLocal(AbsoluteProjectedLocation);

							if ( Entry.WidgetComponent )
							{
								FVector2D ComponentDrawSize = Entry.WidgetComponent->GetDrawSize();
								FVector2D ComponentPivot = Entry.WidgetComponent->GetPivot();
								
								CanvasSlot->AutoSize(ComponentDrawSize.IsZero() || Entry.WidgetComponent->GetDrawAtDesiredSize());
								CanvasSlot->Offset(FMargin(LocalPosition.X, LocalPosition.Y, ComponentDrawSize.X, ComponentDrawSize.Y));
								CanvasSlot->Anchors(FAnchors(0, 0, 0, 0));
								CanvasSlot->Alignment(ComponentPivot);
								// @todo: - DO NOT copy this change to UE4/Main
								//CanvasSlot->ZOrder(-ViewportPosition.Z);
								// @todo: - DO NOT copy this change to UE4/Main
							}
							else
							{
								CanvasSlot->AutoSize(DrawSize.IsZero());
								CanvasSlot->Offset(FMargin(LocalPosition.X, LocalPosition.Y, DrawSize.X, DrawSize.Y));
								CanvasSlot->Anchors(FAnchors(0, 0, 0, 0));
								CanvasSlot->Alignment(Pivot);
								// @todo: - DO NOT copy this change to UE4/Main
								//CanvasSlot->ZOrder(-ViewportPosition.Z);
								// @todo: - DO NOT copy this change to UE4/Main
							}
						}
					}
					else
					{
						Entry.ContainerWidget->SetVisibility(EVisibility::Collapsed);
					}
				}
				else
				{
					DeadComponents.Add(SceneComponent);
				}
			}
		}
	}

	// Normally components should be removed by someone calling remove component, but just in case it was 
	// deleted in a way where they didn't happen, this is our backup solution to ensure we remove stale widgets.
	for ( int32 Index = 0; Index < DeadComponents.Num(); Index++ )
	{
		RemoveComponent(DeadComponents[Index]);
	}
}
