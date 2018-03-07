// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "InputCoreTypes.h"
#include "Templates/SubclassOf.h"
#include "Engine/EngineBaseTypes.h"
#include "UObject/Interface.h"
#include "Input/Events.h"
#include "Styling/SlateBrush.h"
#include "Components/SlateWrapperTypes.h"
#include "Blueprint/DragDropOperation.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WidgetBlueprintLibrary.generated.h"

class UFont;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USlateBrushAsset;
class UTexture2D;

UCLASS()
class UMG_API UWidgetBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

public:

	/** Creates a widget */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, meta=( WorldContext="WorldContextObject", DisplayName="Create Widget", BlueprintInternalUseOnly="true" ), Category="Widget")
	static class UUserWidget* Create(UObject* WorldContextObject, TSubclassOf<class UUserWidget> WidgetType, APlayerController* OwningPlayer);

	/**
	 * Creates a new drag and drop operation that can be returned from a drag begin to inform the UI what i
	 * being dragged and dropped and what it looks like.
	 */
	UFUNCTION(BlueprintCallable, Category="Widget|Drag and Drop", meta=( BlueprintInternalUseOnly="true" ))
	static UDragDropOperation* CreateDragDropOperation(TSubclassOf<UDragDropOperation> OperationClass);
	
	/** Setup an input mode that allows only the UI to respond to user input. */
	DEPRECATED(4.13, "Locking the mouse to the viewport is now controlled by an enum. Call SetInputMode_UIOnlyEx instead")
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Input", meta = (DeprecatedFunction, DeprecationMessage = "Use the new version of Set Input Mode UI Only instead"), DisplayName = "Set Input Mode UI Only (Deprecated)")
	static void SetInputMode_UIOnly(APlayerController* Target, UWidget* InWidgetToFocus = nullptr, bool bLockMouseToViewport = false);

	/** Setup an input mode that allows only the UI to respond to user input. */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="Input", meta=(DisplayName="Set Input Mode UI Only"))
	static void SetInputMode_UIOnlyEx(APlayerController* Target, UWidget* InWidgetToFocus = nullptr, EMouseLockMode InMouseLockMode = EMouseLockMode::DoNotLock);

	/** Setup an input mode that allows only the UI to respond to user input, and if the UI doesn't handle it player input / player controller gets a chance. */
	DEPRECATED(4.13, "Locking the mouse to the viewport is now controlled by an enum. Call SetInputMode_GameAndUIEx instead")
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Input", meta = (DeprecatedFunction, DeprecationMessage = "Use the new version of Set Input Mode Game And UI instead"), DisplayName = "Set Input Mode Game And UI (Deprecated)")
	static void SetInputMode_GameAndUI(APlayerController* Target, UWidget* InWidgetToFocus = nullptr, bool bLockMouseToViewport = false, bool bHideCursorDuringCapture = true);

	/** Setup an input mode that allows only the UI to respond to user input, and if the UI doesn't handle it player input / player controller gets a chance. */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="Input", meta = (DisplayName = "Set Input Mode Game And UI"))
	static void SetInputMode_GameAndUIEx(APlayerController* Target, UWidget* InWidgetToFocus = nullptr, EMouseLockMode InMouseLockMode = EMouseLockMode::DoNotLock, bool bHideCursorDuringCapture = true);

	/** Setup an input mode that allows only player input / player controller to respond to user input. */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="Input")
	static void SetInputMode_GameOnly(APlayerController* Target);

	/** */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="Focus")
	static void SetFocusToGameViewport();

	/** Draws a box */
	UFUNCTION(BlueprintCallable, Category="Painting")
	static void DrawBox(UPARAM(ref) FPaintContext& Context, FVector2D Position, FVector2D Size, USlateBrushAsset* Brush, FLinearColor Tint = FLinearColor::White);

	/**
	 * Draws a line.
	 *
	 * @param PositionA		Starting position of the line in local space.
	 * @param PositionB		Ending position of the line in local space.
	 * @param Thickness		How many pixels thick this line should be.
	 * @param Tint			Color to render the line.
	 */
	UFUNCTION(BlueprintCallable, meta=( AdvancedDisplay = "4" ), Category="Painting" )
	static void DrawLine(UPARAM(ref) FPaintContext& Context, FVector2D PositionA, FVector2D PositionB, FLinearColor Tint = FLinearColor::White, bool bAntiAlias = true);

	/**
	 * Draws several line segments.
	 *
	 * @param Points		Line pairs, each line needs to be 2 separate points in the array.
	 * @param Thickness		How many pixels thick this line should be.
	 * @param Tint			Color to render the line.
	 */
	UFUNCTION(BlueprintCallable, meta=( AdvancedDisplay = "3" ), Category="Painting" )
	static void DrawLines(UPARAM(ref) FPaintContext& Context, const TArray<FVector2D>& Points, FLinearColor Tint = FLinearColor::White, bool bAntiAlias = true);

	/**
	 * Draws text.
	 *
	 * @param InString		The string to draw.
	 * @param Position		The starting position where the text is drawn in local space.
	 * @param Tint			Color to render the line.
	 */
	UFUNCTION(BlueprintCallable, Category="Painting", meta=( DeprecatedFunction, DeprecationMessage = "Use Draw Text instead", DisplayName="Draw String"))
	static void DrawText(UPARAM(ref) FPaintContext& Context, const FString& InString, FVector2D Position, FLinearColor Tint = FLinearColor::White);

	/**
	 * Draws text.
	 *
	 * @param Text			The string to draw.
	 * @param Position		The starting position where the text is drawn in local space.
	 * @param Tint			Color to render the line.
	 */
	UFUNCTION(BlueprintCallable, Category="Painting", meta=(DisplayName="Draw Text"))
	static void DrawTextFormatted(UPARAM(ref) FPaintContext& Context, const FText& Text, FVector2D Position, UFont* Font, int32 FontSize = 16, FName FontTypeFace = FName(TEXT("Regular")), FLinearColor Tint = FLinearColor::White);

	/**
	 * The event reply to use when you choose to handle an event.  This will prevent the event 
	 * from continuing to bubble up / down the widget hierarchy.
	 */
	UFUNCTION(BlueprintPure, Category="Widget|Event Reply")
	static FEventReply Handled();

	/** The event reply to use when you choose not to handle an event. */
	UFUNCTION(BlueprintPure, Category="Widget|Event Reply")
	static FEventReply Unhandled();

	/**  */
	UFUNCTION(BlueprintPure, meta=( DefaultToSelf="CapturingWidget" ), Category="Widget|Event Reply")
	static FEventReply CaptureMouse(UPARAM(ref) FEventReply& Reply, UWidget* CapturingWidget);

	/**  */
	UFUNCTION(BlueprintPure, Category="Widget|Event Reply")
	static FEventReply ReleaseMouseCapture(UPARAM(ref) FEventReply& Reply);

	UFUNCTION( BlueprintPure, Category = "Widget|Event Reply", meta = ( HidePin = "CapturingWidget", DefaultToSelf = "CapturingWidget" ) )
	static FEventReply LockMouse( UPARAM( ref ) FEventReply& Reply, UWidget* CapturingWidget );

	UFUNCTION( BlueprintPure, Category = "Widget|Event Reply" )
	static FEventReply UnlockMouse( UPARAM( ref ) FEventReply& Reply );

	/**  */
	UFUNCTION(BlueprintPure, meta= (HidePin="CapturingWidget", DefaultToSelf="CapturingWidget"), Category="Widget|Event Reply")
	static FEventReply SetUserFocus(UPARAM(ref) FEventReply& Reply, UWidget* FocusWidget, bool bInAllUsers = false);

	UFUNCTION(BlueprintPure, meta = (DeprecatedFunction, DeprecationMessage = "Use SetUserFocus() instead"), Category = "Widget|Event Reply")
	static FEventReply CaptureJoystick(UPARAM(ref) FEventReply& Reply, UWidget* CapturingWidget, bool bInAllJoysticks = false);

	/**  */
	UFUNCTION(BlueprintPure, meta = (HidePin = "CapturingWidget", DefaultToSelf = "CapturingWidget"), Category = "Widget|Event Reply")
	static FEventReply ClearUserFocus(UPARAM(ref) FEventReply& Reply, bool bInAllUsers = false);

	UFUNCTION(BlueprintPure, meta = (DeprecatedFunction, DeprecationMessage = "Use ClearUserFocus() instead"), Category = "Widget|Event Reply")
	static FEventReply ReleaseJoystickCapture(UPARAM(ref) FEventReply& Reply, bool bInAllJoysticks = false);

	/**  */
	UFUNCTION(BlueprintPure, Category="Widget|Event Reply")
	static FEventReply SetMousePosition(UPARAM(ref) FEventReply& Reply, FVector2D NewMousePosition);

	/**
	 * Ask Slate to detect if a user starts dragging in this widget later.  Slate internally tracks the movement
	 * and if it surpasses the drag threshold, Slate will send an OnDragDetected event to the widget.
	 *
	 * @param WidgetDetectingDrag  Detect dragging in this widget
	 * @param DragKey		       This button should be pressed to detect the drag
	 */
	UFUNCTION(BlueprintPure, meta=( HidePin="WidgetDetectingDrag", DefaultToSelf="WidgetDetectingDrag" ), Category="Widget|Drag and Drop|Event Reply")
	static FEventReply DetectDrag(UPARAM(ref) FEventReply& Reply, UWidget* WidgetDetectingDrag, FKey DragKey);

	/**
	 * Given the pointer event, emit the DetectDrag reply if the provided key was pressed.
	 * If the DragKey is a touch key, that will also automatically work.
	 * @param PointerEvent	The pointer device event coming in.
	 * @param WidgetDetectingDrag  Detect dragging in this widget.
	 * @param DragKey		       This button should be pressed to detect the drag, won't emit the DetectDrag FEventReply unless this is pressed.
	 */
	UFUNCTION(BlueprintCallable, meta=( HidePin="WidgetDetectingDrag", DefaultToSelf="WidgetDetectingDrag" ), Category="Widget|Drag and Drop|Event Reply")
	static FEventReply DetectDragIfPressed(const FPointerEvent& PointerEvent, UWidget* WidgetDetectingDrag, FKey DragKey);

	/**
	 * An event should return FReply::Handled().EndDragDrop() to request that the current drag/drop operation be terminated.
	 */
	UFUNCTION(BlueprintPure, Category="Widget|Drag and Drop|Event Reply")
	static FEventReply EndDragDrop(UPARAM(ref) FEventReply& Reply);

	/**
	 * Returns true if a drag/drop event is occurring that a widget can handle.
	 */
	UFUNCTION(BlueprintPure, BlueprintCosmetic, Category="Widget|Drag and Drop")
	static bool IsDragDropping();

	/**
	 * Returns the drag and drop operation that is currently occurring if any, otherwise nothing.
	 */
	UFUNCTION(BlueprintPure, BlueprintCosmetic, Category="Widget|Drag and Drop")
	static UDragDropOperation* GetDragDroppingContent();

	/**
	 * Cancels any current drag drop operation.
	 */
	UFUNCTION(BlueprintCallable, Category="Widget|Drag and Drop")
	static void CancelDragDrop();

	/**
	 * Creates a Slate Brush from a Slate Brush Asset
	 *
	 * @return A new slate brush using the asset's brush.
	 */
	UFUNCTION(BlueprintPure, Category="Widget|Brush")
	static FSlateBrush MakeBrushFromAsset(USlateBrushAsset* BrushAsset);

	/** 
	 * Creates a Slate Brush from a Texture2D
	 *
	 * @param Width  When less than or equal to zero, the Width of the brush will default to the Width of the Texture
	 * @param Height  When less than or equal to zero, the Height of the brush will default to the Height of the Texture
	 *
	 * @return A new slate brush using the texture.
	 */
	UFUNCTION(BlueprintPure, Category="Widget|Brush")
	static FSlateBrush MakeBrushFromTexture(UTexture2D* Texture, int32 Width = 0, int32 Height = 0);

	/**
	 * Creates a Slate Brush from a Material.  Materials don't have an implicit size, so providing a widget and height
	 * is required to hint slate with how large the image wants to be by default.
	 *
	 * @return A new slate brush using the material.
	 */
	UFUNCTION(BlueprintPure, Category="Widget|Brush")
	static FSlateBrush MakeBrushFromMaterial(UMaterialInterface* Material, int32 Width = 32, int32 Height = 32);

	/**
	 * Gets the resource object on a brush.  This could be a UTexture2D or a UMaterialInterface.
	 */
	UFUNCTION(BlueprintPure, Category="Widget|Brush")
	static UObject* GetBrushResource(UPARAM(ref) FSlateBrush& Brush);

	/**
	 * Gets the brush resource as a texture 2D.
	 */
	UFUNCTION(BlueprintPure, Category="Widget|Brush")
	static UTexture2D* GetBrushResourceAsTexture2D(UPARAM(ref) FSlateBrush& Brush);

	/**
	 * Gets the brush resource as a material.
	 */
	UFUNCTION(BlueprintPure, Category="Widget|Brush")
	static UMaterialInterface* GetBrushResourceAsMaterial(UPARAM(ref) FSlateBrush& Brush);

	/**
	 * Sets the resource on a brush to be a UTexture2D.
	 */
	UFUNCTION(BlueprintCallable, Category="Widget|Brush")
	static void SetBrushResourceToTexture(UPARAM(ref) FSlateBrush& Brush, UTexture2D* Texture);

	/**
	 * Sets the resource on a brush to be a Material.
	 */
	UFUNCTION(BlueprintCallable, Category="Widget|Brush")
	static void SetBrushResourceToMaterial(UPARAM(ref) FSlateBrush& Brush, UMaterialInterface* Material);

	/**
	 * Creates a Slate Brush that wont draw anything, the "Null Brush".
	 *
	 * @return A new slate brush that wont draw anything.
	 */
	UFUNCTION(BlueprintPure, Category="Widget|Brush")
	static FSlateBrush NoResourceBrush();

	/**
	 * Gets the material that allows changes to parameters at runtime.  The brush must already have a material assigned to it, 
	 * if it does it will automatically be converted to a MID.
	 *
	 * @return A material that supports dynamic input from the game.
	 */
	UFUNCTION(BlueprintPure, Category="Widget|Brush")
	static UMaterialInstanceDynamic* GetDynamicMaterial(UPARAM(ref) FSlateBrush& Brush);

	/** Closes any popup menu */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="Widget|Menu")
	static void DismissAllMenus();

	/**
	 * Find all widgets of a certain class.
	 * @param FoundWidgets The widgets that were found matching the filter.
	 * @param WidgetClass The widget class to filter by.
	 * @param TopLevelOnly Only the widgets that are direct children of the viewport will be returned.
	 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="Widget", meta=( WorldContext="WorldContextObject", DeterminesOutputType = "WidgetClass", DynamicOutputParam = "FoundWidgets" ))
	static void GetAllWidgetsOfClass(UObject* WorldContextObject, TArray<UUserWidget*>& FoundWidgets, TSubclassOf<UUserWidget> WidgetClass, bool TopLevelOnly = true);

	/**
	* Find all widgets in the world with the specified interface.
	* This is a slow operation, use with caution e.g. do not use every frame.
	* @param Interface The interface to find. Must be specified or result array will be empty.
	* @param FoundWidgets Output array of widgets that implement the specified interface.
	* @param TopLevelOnly Only the widgets that are direct children of the viewport will be returned.
	*/
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="Widget", meta = (WorldContext = "WorldContextObject", DeterminesOutputType = "WidgetClass", DynamicOutputParam = "FoundWidgets"))
	static void GetAllWidgetsWithInterface(UObject* WorldContextObject, TSubclassOf<UInterface> Interface, TArray<UUserWidget*>& FoundWidgets, bool TopLevelOnly);

	UFUNCTION(BlueprintPure, Category="Widget", meta = (CompactNodeTitle = "->", BlueprintAutocast))
	static FInputEvent GetInputEventFromKeyEvent(const FKeyEvent& Event);

	UFUNCTION(BlueprintPure, Category="Widget", meta = (CompactNodeTitle = "->", BlueprintAutocast))
	static FKeyEvent GetKeyEventFromAnalogInputEvent(const FAnalogInputEvent& Event);

	UFUNCTION(BlueprintPure, Category="Widget", meta = ( CompactNodeTitle = "->", BlueprintAutocast ))
	static FInputEvent GetInputEventFromCharacterEvent(const FCharacterEvent& Event);

	UFUNCTION(BlueprintPure, Category="Widget", meta = ( CompactNodeTitle = "->", BlueprintAutocast ))
	static FInputEvent GetInputEventFromPointerEvent(const FPointerEvent& Event);

	UFUNCTION(BlueprintPure, Category="Widget", meta = ( CompactNodeTitle = "->", BlueprintAutocast ))
	static FInputEvent GetInputEventFromNavigationEvent(const FNavigationEvent& Event);

	/**
	 * Gets the amount of padding that needs to be added when accounting for the safe zone on TVs.
	 */
	UFUNCTION(BlueprintCallable, Category="Widget|Safe Zone", meta=( WorldContext="WorldContextObject" ))
	static void GetSafeZonePadding(UObject* WorldContextObject, FVector2D& SafePadding, FVector2D& SafePaddingScale, FVector2D& SpillOverPadding);

	/**
	 * Loads or sets a hardware cursor from the content directory in the game.
	 */
	UFUNCTION(BlueprintCallable, Category="Widget|Hardware Cursor", meta=( WorldContext="WorldContextObject" ))
	static bool SetHardwareCursor(UObject* WorldContextObject, EMouseCursor::Type CursorShape, FName CursorName, FVector2D HotSpot);
};
