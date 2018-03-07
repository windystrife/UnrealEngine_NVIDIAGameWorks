// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "UObject/Package.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Fonts/SlateFontInfo.h"
#include "Engine/Font.h"
#include "Brushes/SlateNoResource.h"
#include "Rendering/DrawElements.h"
#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Slate/UMGDragDropOp.h"
#include "Slate/SlateBrushAsset.h"
#include "EngineGlobals.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Engine/Engine.h"

//For PIE error messages

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UWidgetBlueprintLibrary

UWidgetBlueprintLibrary::UWidgetBlueprintLibrary(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

UUserWidget* UWidgetBlueprintLibrary::Create(UObject* WorldContextObject, TSubclassOf<UUserWidget> WidgetType, APlayerController* OwningPlayer)
{
	if ( WidgetType == nullptr || WidgetType->HasAnyClassFlags(CLASS_Abstract) )
	{
		return nullptr;
	}

	UUserWidget* UserWidget = nullptr;
	if ( OwningPlayer == nullptr )
	{
		UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
		UserWidget = CreateWidget<UUserWidget>(World, WidgetType);
	}
	else
	{
		UserWidget = CreateWidget<UUserWidget>(OwningPlayer, WidgetType);
	}

	if (UserWidget)
	{
		UserWidget->SetFlags(RF_StrongRefOnFrame);
	}
	return UserWidget;
}

UDragDropOperation* UWidgetBlueprintLibrary::CreateDragDropOperation(TSubclassOf<UDragDropOperation> Operation)
{
	UDragDropOperation* DragDropOperation = nullptr;
	if ( Operation )
	{
		DragDropOperation = NewObject<UDragDropOperation>(GetTransientPackage(), Operation);
	}
	else
	{
		DragDropOperation = NewObject<UDragDropOperation>();
	}

	if (DragDropOperation)
	{
		DragDropOperation->SetFlags(RF_StrongRefOnFrame);
	}
	return DragDropOperation;
}

void UWidgetBlueprintLibrary::SetInputMode_UIOnly(APlayerController* Target, UWidget* InWidgetToFocus, bool bLockMouseToViewport)
{
	SetInputMode_UIOnlyEx(Target, InWidgetToFocus, bLockMouseToViewport ? EMouseLockMode::LockOnCapture : EMouseLockMode::DoNotLock);
}

void UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(APlayerController* Target, UWidget* InWidgetToFocus, EMouseLockMode InMouseLockMode)
{
	if (Target != nullptr)
	{
		FInputModeUIOnly InputMode;
		InputMode.SetLockMouseToViewportBehavior(InMouseLockMode);

		if (InWidgetToFocus != nullptr)
		{
			InputMode.SetWidgetToFocus(InWidgetToFocus->TakeWidget());
		}
		Target->SetInputMode(InputMode);
	}
	#if WITH_EDITOR 
	else
	{ 
		FMessageLog("PIE").Error(LOCTEXT("UMG WidgetBlueprint Library: SetInputMode_UIOnly", "SetInputMode_UIOnly expects a valid player controller as target"));
	}
	#endif // WITH_EDITOR
}

void UWidgetBlueprintLibrary::SetInputMode_GameAndUI(APlayerController* Target, UWidget* InWidgetToFocus, bool bLockMouseToViewport, bool bHideCursorDuringCapture)
{
	SetInputMode_GameAndUIEx(Target, InWidgetToFocus, bLockMouseToViewport ? EMouseLockMode::LockOnCapture : EMouseLockMode::DoNotLock, bHideCursorDuringCapture);
}

void UWidgetBlueprintLibrary::SetInputMode_GameAndUIEx(APlayerController* Target, UWidget* InWidgetToFocus, EMouseLockMode InMouseLockMode, bool bHideCursorDuringCapture)
{
	if (Target != nullptr)
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(InMouseLockMode);
		InputMode.SetHideCursorDuringCapture(bHideCursorDuringCapture);

		if (InWidgetToFocus != nullptr)
		{
			InputMode.SetWidgetToFocus(InWidgetToFocus->TakeWidget());
		}
		Target->SetInputMode(InputMode);
	}
	#if WITH_EDITOR 
	else
	{
		FMessageLog("PIE").Error(LOCTEXT("UMG WidgetBlueprint Library: SetInputMode_GameAndUI", "SetInputMode_GameAndUI expects a valid player controller as target"));	
	}
	#endif // WITH_EDITOR
	
}

void UWidgetBlueprintLibrary::SetInputMode_GameOnly(APlayerController* Target)
{
	if (Target != nullptr)
	{
		FInputModeGameOnly InputMode;
		Target->SetInputMode(InputMode);
	}
	#if WITH_EDITOR 
	else
	{
		FMessageLog("PIE").Error(LOCTEXT("UMG WidgetBlueprint Library: SetInputMode_GameOnly", "SetInputMode_GameOnly expects a valid player controller as target"));
	}
	#endif // WITH_EDITOR
}

void UWidgetBlueprintLibrary::SetFocusToGameViewport()
{
	FSlateApplication::Get().SetAllUserFocusToGameViewport();
}

void UWidgetBlueprintLibrary::DrawBox(UPARAM(ref) FPaintContext& Context, FVector2D Position, FVector2D Size, USlateBrushAsset* Brush, FLinearColor Tint)
{
	Context.MaxLayer++;

	if ( Brush )
	{
		FSlateDrawElement::MakeBox(
			Context.OutDrawElements,
			Context.MaxLayer,
			Context.AllottedGeometry.ToPaintGeometry(Position, Size),
			&Brush->Brush,
			ESlateDrawEffect::None,
			Tint);
	}
}

void UWidgetBlueprintLibrary::DrawLine(UPARAM(ref) FPaintContext& Context, FVector2D PositionA, FVector2D PositionB, FLinearColor Tint, bool bAntiAlias)
{
	Context.MaxLayer++;

	TArray<FVector2D> Points;
	Points.Add(PositionA);
	Points.Add(PositionB);

	FSlateDrawElement::MakeLines(
		Context.OutDrawElements,
		Context.MaxLayer,
		Context.AllottedGeometry.ToPaintGeometry(),
		Points,
		ESlateDrawEffect::None,
		Tint,
		bAntiAlias);
}

void UWidgetBlueprintLibrary::DrawLines(UPARAM(ref) FPaintContext& Context, const TArray<FVector2D>& Points, FLinearColor Tint, bool bAntiAlias)
{
	Context.MaxLayer++;

	FSlateDrawElement::MakeLines(
		Context.OutDrawElements,
		Context.MaxLayer,
		Context.AllottedGeometry.ToPaintGeometry(),
		Points,
		ESlateDrawEffect::None,
		Tint,
		bAntiAlias);
}

void UWidgetBlueprintLibrary::DrawText(UPARAM(ref) FPaintContext& Context, const FString& InString, FVector2D Position, FLinearColor Tint)
{
	Context.MaxLayer++;

	//TODO UMG Create a font asset usable as a UFont or as a slate font asset.
	FSlateFontInfo FontInfo = FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText").Font;
	
	FSlateDrawElement::MakeText(
		Context.OutDrawElements,
		Context.MaxLayer,
		Context.AllottedGeometry.ToOffsetPaintGeometry(Position),
		InString,
		FontInfo,
		ESlateDrawEffect::None,
		Tint);
}

void UWidgetBlueprintLibrary::DrawTextFormatted(UPARAM(ref) FPaintContext& Context, const FText& Text, FVector2D Position, UFont* Font, int32 FontSize, FName FontTypeFace, FLinearColor Tint)
{
	if ( Font )
	{
		Context.MaxLayer++;

		//TODO UMG Create a font asset usable as a UFont or as a slate font asset.
		FSlateFontInfo FontInfo(Font, FontSize, FontTypeFace);

		FSlateDrawElement::MakeText(
			Context.OutDrawElements,
			Context.MaxLayer,
			Context.AllottedGeometry.ToOffsetPaintGeometry(Position),
			Text,
			FontInfo,
			ESlateDrawEffect::None,
			Tint);
	}
}

FEventReply UWidgetBlueprintLibrary::Handled()
{
	FEventReply Reply;
	Reply.NativeReply = FReply::Handled();

	return Reply;
}

FEventReply UWidgetBlueprintLibrary::Unhandled()
{
	FEventReply Reply;
	Reply.NativeReply = FReply::Unhandled();

	return Reply;
}

FEventReply UWidgetBlueprintLibrary::CaptureMouse(UPARAM(ref) FEventReply& Reply, UWidget* CapturingWidget)
{
	if ( CapturingWidget )
	{
		TSharedPtr<SWidget> CapturingSlateWidget = CapturingWidget->GetCachedWidget();
		if ( CapturingSlateWidget.IsValid() )
		{
			Reply.NativeReply = Reply.NativeReply.CaptureMouse(CapturingSlateWidget.ToSharedRef());
		}
	}

	return Reply;
}

FEventReply UWidgetBlueprintLibrary::ReleaseMouseCapture(UPARAM(ref) FEventReply& Reply)
{
	Reply.NativeReply = Reply.NativeReply.ReleaseMouseCapture();

	return Reply;
}

FEventReply UWidgetBlueprintLibrary::LockMouse( UPARAM( ref ) FEventReply& Reply, UWidget* CapturingWidget )
{
	if ( CapturingWidget )
	{
		TSharedPtr< SWidget > SlateWidget = CapturingWidget->GetCachedWidget();
		if ( SlateWidget.IsValid() )
		{
			Reply.NativeReply = Reply.NativeReply.LockMouseToWidget( SlateWidget.ToSharedRef() );
		}
	}

	return Reply;
}

FEventReply UWidgetBlueprintLibrary::UnlockMouse( UPARAM( ref ) FEventReply& Reply )
{
	Reply.NativeReply = Reply.NativeReply.ReleaseMouseLock();

	return Reply;
}

FEventReply UWidgetBlueprintLibrary::SetUserFocus( UPARAM( ref ) FEventReply& Reply, UWidget* FocusWidget, bool bInAllUsers/* = false*/ )
{
	if (FocusWidget)
	{
		TSharedPtr<SWidget> CapturingSlateWidget = FocusWidget->GetCachedWidget();
		if (CapturingSlateWidget.IsValid())
		{
			Reply.NativeReply = Reply.NativeReply.SetUserFocus(CapturingSlateWidget.ToSharedRef(), EFocusCause::SetDirectly, bInAllUsers);
		}
	}

	return Reply;
}

FEventReply UWidgetBlueprintLibrary::CaptureJoystick(UPARAM(ref) FEventReply& Reply, UWidget* CapturingWidget, bool bInAllJoysticks/* = false*/)
{
	return SetUserFocus(Reply, CapturingWidget, bInAllJoysticks);
}

FEventReply UWidgetBlueprintLibrary::ClearUserFocus(UPARAM(ref) FEventReply& Reply, bool bInAllUsers /*= false*/)
{
	Reply.NativeReply = Reply.NativeReply.ClearUserFocus(bInAllUsers);

	return Reply;
}

FEventReply UWidgetBlueprintLibrary::ReleaseJoystickCapture(UPARAM(ref) FEventReply& Reply, bool bInAllJoysticks /*= false*/)
{
	return ClearUserFocus(Reply, bInAllJoysticks);
}

FEventReply UWidgetBlueprintLibrary::SetMousePosition(UPARAM(ref) FEventReply& Reply, FVector2D NewMousePosition)
{
	FIntPoint NewPoint((int32)NewMousePosition.X, (int32)NewMousePosition.Y);
	Reply.NativeReply = Reply.NativeReply.SetMousePos(NewPoint);

	return Reply;
}

FEventReply UWidgetBlueprintLibrary::DetectDrag(UPARAM(ref) FEventReply& Reply, UWidget* WidgetDetectingDrag, FKey DragKey)
{
	if ( WidgetDetectingDrag )
	{
		TSharedPtr<SWidget> SlateWidgetDetectingDrag = WidgetDetectingDrag->GetCachedWidget();
		if ( SlateWidgetDetectingDrag.IsValid() )
		{
			Reply.NativeReply = Reply.NativeReply.DetectDrag(SlateWidgetDetectingDrag.ToSharedRef(), DragKey);
		}
	}

	return Reply;
}

FEventReply UWidgetBlueprintLibrary::DetectDragIfPressed(const FPointerEvent& PointerEvent, UWidget* WidgetDetectingDrag, FKey DragKey)
{
	if ( PointerEvent.GetEffectingButton() == DragKey || PointerEvent.IsTouchEvent() )
	{
		FEventReply Reply = UWidgetBlueprintLibrary::Handled();
		return UWidgetBlueprintLibrary::DetectDrag(Reply, WidgetDetectingDrag, DragKey);
	}

	return UWidgetBlueprintLibrary::Unhandled();
}

FEventReply UWidgetBlueprintLibrary::EndDragDrop(UPARAM(ref) FEventReply& Reply)
{
	Reply.NativeReply = Reply.NativeReply.EndDragDrop();

	return Reply;
}

bool UWidgetBlueprintLibrary::IsDragDropping()
{
	if ( FSlateApplication::Get().IsDragDropping() )
	{
		TSharedPtr<FDragDropOperation> SlateDragOp = FSlateApplication::Get().GetDragDroppingContent();
		if ( SlateDragOp.IsValid() && SlateDragOp->IsOfType<FUMGDragDropOp>() )
		{
			return true;
		}
	}

	return false;
}

UDragDropOperation* UWidgetBlueprintLibrary::GetDragDroppingContent()
{
	TSharedPtr<FDragDropOperation> SlateDragOp = FSlateApplication::Get().GetDragDroppingContent();
	if ( SlateDragOp.IsValid() && SlateDragOp->IsOfType<FUMGDragDropOp>() )
	{
		TSharedPtr<FUMGDragDropOp> UMGDragDropOp = StaticCastSharedPtr<FUMGDragDropOp>(SlateDragOp);
		return UMGDragDropOp->GetOperation();
	}

	return nullptr;
}

void UWidgetBlueprintLibrary::CancelDragDrop()
{
	FSlateApplication::Get().CancelDragDrop();
}

FSlateBrush UWidgetBlueprintLibrary::MakeBrushFromAsset(USlateBrushAsset* BrushAsset)
{
	if ( BrushAsset )
	{
		return BrushAsset->Brush;
	}

	return FSlateNoResource();
}

FSlateBrush UWidgetBlueprintLibrary::MakeBrushFromTexture(UTexture2D* Texture, int32 Width, int32 Height)
{
	if ( Texture )
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(Texture);
		Width = (Width > 0) ? Width : Texture->GetSizeX();
		Height = (Height > 0) ? Height : Texture->GetSizeY();
		Brush.ImageSize = FVector2D(Width, Height);
		return Brush;
	}
	
	return FSlateNoResource();
}

FSlateBrush UWidgetBlueprintLibrary::MakeBrushFromMaterial(UMaterialInterface* Material, int32 Width, int32 Height)
{
	if ( Material )
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(Material);
		Brush.ImageSize = FVector2D(Width, Height);

		return Brush;
	}

	return FSlateNoResource();
}

UObject* UWidgetBlueprintLibrary::GetBrushResource(FSlateBrush& Brush)
{
	return Brush.GetResourceObject();
}

UTexture2D* UWidgetBlueprintLibrary::GetBrushResourceAsTexture2D(UPARAM(ref) FSlateBrush& Brush)
{
	return Cast<UTexture2D>(Brush.GetResourceObject());
}

UMaterialInterface* UWidgetBlueprintLibrary::GetBrushResourceAsMaterial(UPARAM(ref) FSlateBrush& Brush)
{
	return Cast<UMaterialInterface>(Brush.GetResourceObject());
}

void UWidgetBlueprintLibrary::SetBrushResourceToTexture(FSlateBrush& Brush, UTexture2D* Texture)
{
	Brush.SetResourceObject(Texture);
}

void UWidgetBlueprintLibrary::SetBrushResourceToMaterial(FSlateBrush& Brush, UMaterialInterface* Material)
{
	Brush.SetResourceObject(Material);
}

FSlateBrush UWidgetBlueprintLibrary::NoResourceBrush()
{
	return FSlateNoResource();
}

UMaterialInstanceDynamic* UWidgetBlueprintLibrary::GetDynamicMaterial(FSlateBrush& Brush)
{
	UObject* Resource = Brush.GetResourceObject();

	// If we already have a dynamic material, return it.
	if ( UMaterialInstanceDynamic* DynamicMaterial = Cast<UMaterialInstanceDynamic>(Resource) )
	{
		return DynamicMaterial;
	}
	// If the resource has a material interface we'll just update the brush to have a dynamic material.
	else if ( UMaterialInterface* Material = Cast<UMaterialInterface>(Resource) )
	{
		DynamicMaterial = UMaterialInstanceDynamic::Create(Material, nullptr);
		Brush.SetResourceObject(DynamicMaterial);

		return DynamicMaterial;
	}

	//TODO UMG can we do something for textures?  General purpose dynamic material for them?

	return nullptr;
}

void UWidgetBlueprintLibrary::DismissAllMenus()
{
	FSlateApplication::Get().DismissAllMenus();
}

void UWidgetBlueprintLibrary::GetAllWidgetsOfClass(UObject* WorldContextObject, TArray<UUserWidget*>& FoundWidgets, TSubclassOf<UUserWidget> WidgetClass, bool TopLevelOnly)
{
	//Prevent possibility of an ever-growing array if user uses this in a loop
	FoundWidgets.Empty();
	
	if ( !WidgetClass || !WorldContextObject )
	{
		return;
	}
	 
	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if ( !World )
	{
		return;
	}

	for ( TObjectIterator<UUserWidget> Itr; Itr; ++Itr )
	{
		UUserWidget* LiveWidget = *Itr;

		// Skip any widget that's not in the current world context.
		if ( LiveWidget->GetWorld() != World )
		{
			continue;
		}

		// Skip any widget that is not a child of the class specified.
		if ( !LiveWidget->GetClass()->IsChildOf(WidgetClass) )
		{
			continue;
		}
		
		if ( TopLevelOnly )
		{
			if ( LiveWidget->IsInViewport() )
			{
				FoundWidgets.Add(LiveWidget);
			}
		}
		else
		{
			FoundWidgets.Add(LiveWidget);
		}
	}
}

void UWidgetBlueprintLibrary::GetAllWidgetsWithInterface(UObject* WorldContextObject, TSubclassOf<UInterface> Interface, TArray<UUserWidget*>& FoundWidgets, bool TopLevelOnly)
{
	FoundWidgets.Empty();

	if (!Interface || !WorldContextObject)
	{
		return;
	}

	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		return;
	}

	for (TObjectIterator<UUserWidget> Itr; Itr; ++Itr)
	{
		UUserWidget* LiveWidget = *Itr;

		if (LiveWidget->GetWorld() != World)
		{
			continue;
		}

		if (TopLevelOnly)
		{
			if (LiveWidget->IsInViewport() && LiveWidget->GetClass()->ImplementsInterface(Interface))
			{
				FoundWidgets.Add(LiveWidget);
			}
		}
		else if (LiveWidget->GetClass()->ImplementsInterface(Interface))
		{
			FoundWidgets.Add(LiveWidget);
		}
	}
}

FInputEvent UWidgetBlueprintLibrary::GetInputEventFromKeyEvent(const FKeyEvent& Event)
{
	return Event;
}

FKeyEvent UWidgetBlueprintLibrary::GetKeyEventFromAnalogInputEvent(const FAnalogInputEvent& Event)
{
	return Event;
}

FInputEvent UWidgetBlueprintLibrary::GetInputEventFromCharacterEvent(const FCharacterEvent& Event)
{
	return Event;
}

FInputEvent UWidgetBlueprintLibrary::GetInputEventFromPointerEvent(const FPointerEvent& Event)
{
	return Event;
}

FInputEvent UWidgetBlueprintLibrary::GetInputEventFromNavigationEvent(const FNavigationEvent& Event)
{
	return Event;
}

void UWidgetBlueprintLibrary::GetSafeZonePadding(UObject* WorldContextObject, FVector2D& SafePadding, FVector2D& SafePaddingScale, FVector2D& SpillOverPadding)
{
	FVector2D ViewportSize = UWidgetLayoutLibrary::GetViewportSize(WorldContextObject);

	FDisplayMetrics Metrics;
	FSlateApplication::Get().GetDisplayMetrics(Metrics);

	SafePadding = Metrics.TitleSafePaddingSize;
	SafePaddingScale = SafePadding / ViewportSize;
	SpillOverPadding = Metrics.ActionSafePaddingSize;
}

bool UWidgetBlueprintLibrary::SetHardwareCursor(UObject* WorldContextObject, EMouseCursor::Type CursorShape, FName CursorName, FVector2D HotSpot)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if ( World && World->IsGameWorld() )
	{
		if ( UGameViewportClient* ViewportClient = World->GetGameViewport() )
		{
			return ViewportClient->SetHardwareCursor(CursorShape, CursorName, HotSpot);
		}
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
