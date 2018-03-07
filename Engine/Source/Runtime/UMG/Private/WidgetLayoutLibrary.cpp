// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Blueprint/WidgetLayoutLibrary.h"
#include "EngineGlobals.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"
#include "Components/Widget.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/UniformGridSlot.h"
#include "Components/GridSlot.h"
#include "Components/OverlaySlot.h"
#include "Components/BorderSlot.h"
#include "Engine/UserInterfaceSettings.h"
#include "Slate/SlateBrushAsset.h"
#include "WidgetLayoutLibrary.h"
#include "Runtime/Engine/Classes/Engine/UserInterfaceSettings.h"
#include "Runtime/Engine/Classes/Engine/RendererSettings.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Slate/SceneViewport.h"
#include "Slate/SGameLayerManager.h"
#include "Framework/Application/SlateApplication.h"
#include "FrameValue.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UWidgetLayoutLibrary

UWidgetLayoutLibrary::UWidgetLayoutLibrary(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

bool UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(APlayerController* PlayerController, FVector WorldLocation, FVector2D& ViewportPosition)
{
	FVector ScreenPosition3D;
	const bool bSuccess = ProjectWorldLocationToWidgetPositionWithDistance(PlayerController, WorldLocation, ScreenPosition3D);
	ViewportPosition = FVector2D(ScreenPosition3D.X, ScreenPosition3D.Y);
	return bSuccess;
}

bool UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPositionWithDistance(APlayerController* PlayerController, FVector WorldLocation, FVector& ViewportPosition)
{	
	if ( PlayerController )
	{
		FVector PixelLocation;
		const bool bPlayerViewportRelative = false;
		const bool bProjected = PlayerController->ProjectWorldLocationToScreenWithDistance(WorldLocation, PixelLocation, bPlayerViewportRelative);

		if ( bProjected )
		{
			// Round the pixel projected value to reduce jittering due to layout rounding,
			// I do this before I remove scaling, because scaling is going to be applied later
			// in the opposite direction, so as long as we round, before inverse scale, scale should
			// result in more or less the same value, especially after slate does layout rounding.
			FVector2D ScreenPosition(FMath::RoundToInt(PixelLocation.X), FMath::RoundToInt(PixelLocation.Y));

			FVector2D ViewportPosition2D;
			USlateBlueprintLibrary::ScreenToViewport(PlayerController, ScreenPosition, ViewportPosition2D);
			ViewportPosition.X = ViewportPosition2D.X;
			ViewportPosition.Y = ViewportPosition2D.Y;
			ViewportPosition.Z = PixelLocation.Z;

			return true;
		}
	}

	ViewportPosition = FVector::ZeroVector;

	return false;
}

float UWidgetLayoutLibrary::GetViewportScale(UObject* WorldContextObject)
{
	static TFrameValue<float> ViewportScaleCache;

	if ( !ViewportScaleCache.IsSet() || WITH_EDITOR )
	{
		float ViewportScale = 1.0f;

		UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
		if ( World && World->IsGameWorld() )
		{
			if ( UGameViewportClient* ViewportClient = World->GetGameViewport() )
			{
				FVector2D ViewportSize;
				ViewportClient->GetViewportSize(ViewportSize);
				ViewportScale = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(FIntPoint(ViewportSize.X, ViewportSize.Y));
			}
		}

		ViewportScaleCache = ViewportScale;
	}

	return ViewportScaleCache.GetValue();
}

float UWidgetLayoutLibrary::GetViewportScale(UGameViewportClient* ViewportClient)
{
	FVector2D ViewportSize;
	ViewportClient->GetViewportSize(ViewportSize);
	
	float UserResolutionScale = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(FIntPoint(ViewportSize.X, ViewportSize.Y));

	// Normally we'd factor in native DPI Scale here too, but because the SGameLayerManager already
	// accounts for the native DPI scale, and extracts it from the calculations, the UMG/Slate portion of the
	// game can more or less assume the platform scale is 1.0 for DPI.
	//
	// This id done because UMG already provides a solution for scaling UI across devices, that if influenced
	// by the platform scale would produce undesirable results.

	return UserResolutionScale;
}

FVector2D UWidgetLayoutLibrary::GetMousePositionOnPlatform()
{
	if ( FSlateApplication::IsInitialized() )
	{
		return FSlateApplication::Get().GetCursorPos();
	}

	return FVector2D(0, 0);
}

FVector2D UWidgetLayoutLibrary::GetMousePositionOnViewport(UObject* WorldContextObject)
{
	if ( FSlateApplication::IsInitialized() )
	{
		FVector2D MousePosition = FSlateApplication::Get().GetCursorPos();
		FGeometry ViewportGeometry = GetViewportWidgetGeometry(WorldContextObject);
		return ViewportGeometry.AbsoluteToLocal(MousePosition);
	}

	return FVector2D(0, 0);
}

bool UWidgetLayoutLibrary::GetMousePositionScaledByDPI(APlayerController* Player, float& LocationX, float& LocationY)
{
	if ( Player && Player->GetMousePosition(LocationX, LocationY) )
	{
		float Scale = UWidgetLayoutLibrary::GetViewportScale(Player);
		LocationX = LocationX / Scale;
		LocationY = LocationY / Scale;

		return true;
	}

	return false;
}

FVector2D UWidgetLayoutLibrary::GetViewportSize(UObject* WorldContextObject)
{
	static TFrameValue<FVector2D> ViewportSizeCache;

	if ( !ViewportSizeCache.IsSet() || WITH_EDITOR )
	{
		FVector2D ViewportSize(1, 1);

		UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
		if ( World && World->IsGameWorld() )
		{
			if ( UGameViewportClient* ViewportClient = World->GetGameViewport() )
			{
				ViewportClient->GetViewportSize(ViewportSize);
			}
		}

		ViewportSizeCache = ViewportSize;
	}

	return ViewportSizeCache.GetValue();
}

FGeometry UWidgetLayoutLibrary::GetViewportWidgetGeometry(UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if ( World && World->IsGameWorld() )
	{
		if ( UGameViewportClient* ViewportClient = World->GetGameViewport() )
		{
			TSharedPtr<IGameLayerManager> LayerManager = ViewportClient->GetGameLayerManager();
			if ( LayerManager.IsValid() )
			{
				return LayerManager->GetViewportWidgetHostGeometry();
			}
		}
	}

	return FGeometry();
}

FGeometry UWidgetLayoutLibrary::GetPlayerScreenWidgetGeometry(APlayerController* PlayerController)
{
	UWorld* World = GEngine->GetWorldFromContextObject(PlayerController, EGetWorldErrorMode::LogAndReturnNull);
	if ( World && World->IsGameWorld() )
	{
		if ( UGameViewportClient* ViewportClient = World->GetGameViewport() )
		{
			TSharedPtr<IGameLayerManager> LayerManager = ViewportClient->GetGameLayerManager();
			if ( LayerManager.IsValid() )
			{
				return LayerManager->GetPlayerWidgetHostGeometry(PlayerController->GetLocalPlayer());
			}
		}
	}

	return FGeometry();
}

UBorderSlot* UWidgetLayoutLibrary::SlotAsBorderSlot(UWidget* Widget)
{
	if (Widget)
	{
		return Cast<UBorderSlot>(Widget->Slot);
	}

	return nullptr;
}

UCanvasPanelSlot* UWidgetLayoutLibrary::SlotAsCanvasSlot(UWidget* Widget)
{
	if (Widget)
	{
		return Cast<UCanvasPanelSlot>(Widget->Slot);
	}

	return nullptr;
}

UGridSlot* UWidgetLayoutLibrary::SlotAsGridSlot(UWidget* Widget)
{
	if (Widget)
	{
		return Cast<UGridSlot>(Widget->Slot);
	}

	return nullptr;
}

UHorizontalBoxSlot* UWidgetLayoutLibrary::SlotAsHorizontalBoxSlot(UWidget* Widget)
{
	if (Widget)
	{
		return Cast<UHorizontalBoxSlot>(Widget->Slot);
	}

	return nullptr;
}

UOverlaySlot* UWidgetLayoutLibrary::SlotAsOverlaySlot(UWidget* Widget)
{
	if (Widget)
	{
		return Cast<UOverlaySlot>(Widget->Slot);
	}

	return nullptr;
}

UUniformGridSlot* UWidgetLayoutLibrary::SlotAsUniformGridSlot(UWidget* Widget)
{
	if (Widget)
	{
		return Cast<UUniformGridSlot>(Widget->Slot);
	}

	return nullptr;
}

UVerticalBoxSlot* UWidgetLayoutLibrary::SlotAsVerticalBoxSlot(UWidget* Widget)
{
	if (Widget)
	{
		return Cast<UVerticalBoxSlot>(Widget->Slot);
	}

	return nullptr;
}

void UWidgetLayoutLibrary::RemoveAllWidgets(UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if ( World && World->IsGameWorld() )
	{
		if ( UGameViewportClient* ViewportClient = World->GetGameViewport() )
		{
			ViewportClient->RemoveAllViewportWidgets();
		}
	}
}

#undef LOCTEXT_NAMESPACE
