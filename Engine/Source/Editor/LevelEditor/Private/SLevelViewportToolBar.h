// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Widgets/SWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "SLevelViewport.h"
#include "Editor/UnrealEd/Public/SViewportToolBar.h"

class ACameraActor;
class FExtender;
class FMenuBuilder;

/**
 * A level viewport toolbar widget that is placed in a viewport
 */
class SLevelViewportToolBar : public SViewportToolBar
{
public:
	SLATE_BEGIN_ARGS( SLevelViewportToolBar ){}
		SLATE_ARGUMENT( TSharedPtr<class SLevelViewport>, Viewport )
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs );

	/** @return Whether the given viewmode is supported. */ 
	virtual bool IsViewModeSupported(EViewModeIndex ViewModeIndex) const;

private:
	/**
	 * Returns the label for the "Camera" tool bar menu, which changes depending on the viewport type
	 *
	 * @return	Label to use for this menu label
	 */
	FText GetCameraMenuLabel() const;

	/**
	 * Returns the label icon for the "Camera" tool bar menu, which changes depending on the viewport type
	 *
	 * @return	Label icon to use for this menu label
	 */
	const FSlateBrush* GetCameraMenuLabelIcon() const;

	/**
	 * Returns the label for the "View" tool bar menu, which changes depending on viewport show flags
	 *
	 * @return	Label to use for this menu label
	 */
	FText GetViewMenuLabel() const;

	/**
	 * Returns the label for the "Device Profile Preview" tool bar menu, which changes depending on the preview type.
	 *
	 * @return	Label to use for this menu label.
	 */
	FText GetDevicePreviewMenuLabel() const;

	/**
	 * Returns the label icon for the "Device Preview" tool bar menu
	 *
	 * @return	Label icon to use for this menu label
	 */
	const FSlateBrush* GetDevicePreviewMenuLabelIcon() const;

	/**
	 * Returns the label icon for the "View" tool bar menu, which changes depending on viewport show flags
	 *
	 * @return	Label icon to use for this menu label
	 */
	const FSlateBrush* GetViewMenuLabelIcon() const;

	/** @return Returns true, only if this tool bar's viewport is the "current" level editing viewport */
	bool IsCurrentLevelViewport() const;

	/** @return Returns true if this viewport is the perspective viewport */
	bool IsPerspectiveViewport() const;

		/**
	 * Generates the toolbar device profile simulation menu content .
	 *
	 * @return The widget containing the options menu content.
	 */
	TSharedRef<SWidget> GenerateDevicePreviewMenu() const;

	/**
	 * Generates the sub  menu for different device profile previews.
	 *
	 * @param MenuBuilder - the parent menu.
	 * @param InDeviceProfiles - The array of device profiles.
	 */
	void MakeDevicePreviewSubMenu( FMenuBuilder& MenuBuilder, TArray< class UDeviceProfile* > Profiles );

	/**
	 * Set the level profile, and save the selection to an .ini file.
	 *
	 * @param DeviceProfileName - The selected device profile
	 */
	void SetLevelProfile( FString DeviceProfileName );

	/**
	 * Generates the toolbar view mode options menu content 
	 *
	 * @return The widget containing the options menu content
	 */
	TSharedRef<SWidget> GenerateOptionsMenu() const;

	/**
	 * Generates the toolbar camera menu content 
	 *
	 * @return The widget containing the view menu content
	 */
	TSharedRef<SWidget> GenerateCameraMenu() const;

	/**
	 * Generates menu entries for placed cameras (e.g CameraActors
	 *
	 * @param Builder	The menu builder to add menu entries to
	 * @param Cameras	The list of cameras to add
	 */
	void GeneratePlacedCameraMenuEntries( FMenuBuilder& Builder, TArray<ACameraActor*> Cameras ) const;

	/**
	 * Generates menu entries for changing the type of the viewport
	 *
	 * @param Builder	The menu builder to add menu entries to
	 */
	void GenerateViewportTypeMenu( FMenuBuilder& Builder ) const;

	/**
	 * Generates the toolbar view menu content 
	 *
	 * @return The widget containing the view menu content
	 */
	TSharedRef<SWidget> GenerateViewMenu() const;

	/**
	 * Generates the toolbar show menu content 
	 *
	 * @return The widget containing the show menu content
	 */
	TSharedRef<SWidget> GenerateShowMenu() const;

	/**
	 * Returns the initial visibility of the view mode options widget 
	 *
	 * @return The visibility value
	 */
	EVisibility GetViewModeOptionsVisibility() const;

	/** Get the name of the viewmode options menu */
	FText GetViewModeOptionsMenuLabel() const;

	/**
	 * Generates the toolbar view param menu content 
	 *
	 * @return The widget containing the show menu content
	 */
	TSharedRef<SWidget> GenerateViewModeOptionsMenu() const;

	/**
	 * @return The widget containing the perspective only FOV window.
	 */
	TSharedRef<SWidget> GenerateFOVMenu() const;

	/** Called by the FOV slider in the perspective viewport to get the FOV value */
	float OnGetFOVValue() const;

	/** Called when the FOV slider is adjusted in the perspective viewport */
	void OnFOVValueChanged( float NewValue );

	/**
	 * @return The widget containing the far view plane slider.
	 */
	TSharedRef<SWidget> GenerateFarViewPlaneMenu() const;

	/** Called by the far view plane slider in the perspective viewport to get the far view plane value */
	float OnGetFarViewPlaneValue() const;

	/** Called when the far view plane slider is adjusted in the perspective viewport */
	void OnFarViewPlaneValueChanged( float NewValue );

	bool IsLandscapeLODSettingChecked(int32 Value) const;
	void OnLandscapeLODChanged(int32 NewValue);

private:
	/**
	 * Generates the toolbar show layers menu content 
	 *
	 * @param MenuBuilder menu builder
	 */
	static void FillShowLayersMenu( class FMenuBuilder& MenuBuilder, TWeakPtr<class SLevelViewport> Viewport );

	/**
	 * Generates 'show foliage types' menu content for a viewport
	 *
	 * @param MenuBuilder	menu builder
	 * @param Viewport		target vieport
	 */
	static void FillShowFoliageTypesMenu(class FMenuBuilder& MenuBuilder, TWeakPtr<class SLevelViewport> Viewport);

	/** Generates the layout sub-menu content */
	void GenerateViewportConfigsMenu(FMenuBuilder& MenuBuilder) const;

	/** Gets the world we are editing */
	TWeakObjectPtr<UWorld> GetWorld() const;

	/** Gets the extender for the view menu */
	TSharedPtr<FExtender> GetViewMenuExtender();

	void CreateViewMenuExtensions(FMenuBuilder& MenuBuilder);
private:
	/** The viewport that we are in */
	TWeakPtr<class SLevelViewport> Viewport;
};

