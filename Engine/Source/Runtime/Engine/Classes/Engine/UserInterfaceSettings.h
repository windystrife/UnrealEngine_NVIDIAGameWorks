// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/SoftObjectPath.h"
#include "Curves/CurveFloat.h"
#include "Engine/DeveloperSettings.h"
#include "Widgets/SWidget.h"

#include "UserInterfaceSettings.generated.h"

class UDPICustomScalingRule;

/** When to render the Focus Brush for widgets that have user focus. Based on the EFocusCause. */
UENUM()
enum class ERenderFocusRule : uint8
{
	/** Focus Brush will always be rendered for widgets that have user focus. */
	Always,
	/** Focus Brush will be rendered for widgets that have user focus not set based on pointer causes. */
	NonPointer,
	/** Focus Brush will be rendered for widgets that have user focus only if the focus was set by navigation. */
	NavigationOnly,
	/** Focus Brush will not be rendered. */
	Never,
};

/** The Side to use when scaling the UI. */
UENUM()
enum class EUIScalingRule : uint8
{
	/** Evaluates the scale curve based on the shortest side of the viewport. */
	ShortestSide,
	/** Evaluates the scale curve based on the longest side of the viewport. */
	LongestSide,
	/** Evaluates the scale curve based on the X axis of the viewport. */
	Horizontal,
	/** Evaluates the scale curve based on the Y axis of the viewport. */
	Vertical,
	/** Custom - Allows custom rule interpretation. */
	Custom
};

class UWidget;
class UDPICustomScalingRule;

/**
 * 
 */
USTRUCT()
struct FHardwareCursorReference
{
	GENERATED_BODY()

	/**
	 * Specify the partial game content path to the hardware cursor.  For example,
	 *   DO:   Slate/DefaultPointer
	 *   DONT: Slate/DefaultPointer.cur
	 *
	 * NOTE: Having a 'Slate' directory in your game content folder will always be cooked, if
	 *       you're trying to decide where to locate these cursor files.
	 * 
	 * The hardware cursor system will search for platform specific formats first if you want to 
	 * take advantage of those capabilities.
	 *
	 * Windows:
	 *   .ani -> .cur -> .png
	 *
	 * Mac:
	 *   .tiff -> .png
	 *
	 * Linux:
	 *   .png
	 *
	 * Multi-Resolution Png Fallback
	 *  Because there's not a universal multi-resolution format for cursors there's a pattern we look for
	 *  on all platforms where pngs are all that is found instead of cur/ani/tiff.
	 *
	 *    Pointer.png
	 *    Pointer@1.25x.png
	 *    Pointer@1.5x.png
	 *    Pointer@1.75x.png
	 *    Pointer@2x.png
	 *    ...etc
	 */
	UPROPERTY(EditAnywhere, Category="Hardware Cursor")
	FName CursorPath;

	/**
	 * HotSpot needs to be in normalized (0..1) coordinates since it may apply to different resolution images.
	 * NOTE: This HotSpot is only used on formats that do not provide their own hotspot, e.g. Tiff, PNG.
	 */
	UPROPERTY(EditAnywhere, Category="Hardware Cursor", meta=( ClampMin=0, ClampMax=1 ))
	FVector2D HotSpot;
};

/**
 * User Interface settings that control Slate and UMG.
 */
UCLASS(config=Engine, defaultconfig, meta=(DisplayName="User Interface"))
class ENGINE_API UUserInterfaceSettings : public UDeveloperSettings
{
	GENERATED_UCLASS_BODY()

public:

	/**
	 * Rule to determine if we should render the Focus Brush for widgets that have user focus.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Focus")
	ERenderFocusRule RenderFocusRule;

	UPROPERTY(config, EditAnywhere, Category = "Hardware Cursors")
	TMap<TEnumAsByte<EMouseCursor::Type>, FHardwareCursorReference> HardwareCursors;

	UPROPERTY(config, EditAnywhere, Category = "Software Cursors", meta = ( MetaClass = "UserWidget" ))
	TMap<TEnumAsByte<EMouseCursor::Type>, FSoftClassPath> SoftwareCursors;

	// DEPRECATED 4.16
	UPROPERTY(config)
	FSoftClassPath DefaultCursor_DEPRECATED;

	// DEPRECATED 4.16
	UPROPERTY(config)
	FSoftClassPath TextEditBeamCursor_DEPRECATED;

	// DEPRECATED 4.16
	UPROPERTY(config)
	FSoftClassPath CrosshairsCursor_DEPRECATED;

	// DEPRECATED 4.16
	UPROPERTY(config)
	FSoftClassPath HandCursor_DEPRECATED;

	// DEPRECATED 4.16
	UPROPERTY(config)
	FSoftClassPath GrabHandCursor_DEPRECATED;
	
	// DEPRECATED 4.16
	UPROPERTY(config)
	FSoftClassPath GrabHandClosedCursor_DEPRECATED;

	// DEPRECATED 4.16
	UPROPERTY(config)
	FSoftClassPath SlashedCircleCursor_DEPRECATED;

	/**
	 * An optional application scale to apply on top of the custom scaling rules.  So if you want to expose a 
	 * property in your game title, you can modify this underlying value to scale the entire UI.
	 */
	UPROPERTY(config, EditAnywhere, Category="DPI Scaling")
	float ApplicationScale;

	/**
	 * The rule used when trying to decide what scale to apply.
	 */
	UPROPERTY(config, EditAnywhere, Category="DPI Scaling", meta=( DisplayName="DPI Scale Rule" ))
	EUIScalingRule UIScaleRule;

	/**
	 * Set DPI Scale Rule to Custom, and this class will be used instead of any of the built-in rules.
	 */
	UPROPERTY(config, EditAnywhere, Category="DPI Scaling", meta=( MetaClass="DPICustomScalingRule" ))
	FSoftClassPath CustomScalingRuleClass;

	/**
	 * Controls how the UI is scaled at different resolutions based on the DPI Scale Rule
	 */
	UPROPERTY(config, EditAnywhere, Category="DPI Scaling", meta=(
		DisplayName="DPI Curve",
		XAxisName="Resolution",
		YAxisName="Scale"))
	FRuntimeFloatCurve UIScaleCurve;

	/**
	 * If false, widget references will be stripped during cook for server builds and not loaded at runtime.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Widgets")
	bool bLoadWidgetsOnDedicatedServer;

public:

	virtual void PostInitProperties() override;

	void ForceLoadResources();

	/** Gets the current scale of the UI based on the size of a viewport */
	float GetDPIScaleBasedOnSize(FIntPoint Size) const;

private:
	UPROPERTY(Transient)
	TArray<UObject*> CursorClasses;

	UPROPERTY(Transient)
	mutable UClass* CustomScalingRuleClassInstance;

	UPROPERTY(Transient)
	mutable UDPICustomScalingRule* CustomScalingRule;
};
