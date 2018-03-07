// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Textures/SlateIcon.h"
#include "SViewportToolBar.h"
#include "Settings/LevelEditorViewportSettings.h"

class FExtender;
class FUICommandList;
class SEditorViewport;
class SSlider;
enum class ECheckBoxState : uint8;

/**
 * Viewport toolbar containing transform, grid snapping, local to world and camera speed controls.
 */
class UNREALED_API STransformViewportToolBar  : public SViewportToolBar
{

public:
	SLATE_BEGIN_ARGS( STransformViewportToolBar ){}
		SLATE_ARGUMENT( TSharedPtr<class SEditorViewport>, Viewport )
		SLATE_ARGUMENT( TSharedPtr<FUICommandList>, CommandList )
		SLATE_ARGUMENT( TSharedPtr<FExtender>, Extenders )
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs );
	/**
	 * Static: Creates a widget for the main tool bar
	 *
	 * @return	New widget
	 */
	TSharedRef< SWidget > MakeTransformToolBar( const TSharedPtr< FExtender > InExtenders );

private:
	FSlateIcon GetLocalToWorldIcon() const;

	/** Camera speed menu construction callback */
	TSharedRef<SWidget> FillCameraSpeedMenu();

	/** Camera speed Label callback */
	FText GetCameraSpeedLabel() const;

	/** Returns the current camera speed setting */
	float GetCamSpeedSliderPosition() const;

	/** 
	 * Sets new camera speed 
	 *
	 * @Param	NewValue	Value to set camera speed too
	 */
	void OnSetCamSpeed(float NewValue);

	/** Grid snap label callbacks */
	FText GetLocationGridLabel() const;
	FText GetRotationGridLabel() const;
	FText GetLayer2DLabel() const;
	FText GetScaleGridLabel() const;

	/** GridSnap menu construction callbacks */
	TSharedRef<SWidget> FillLocationGridSnapMenu();
	TSharedRef<SWidget> FillRotationGridSnapMenu();
	TSharedRef<SWidget> FillLayer2DSnapMenu();
	TSharedRef<SWidget> FillScaleGridSnapMenu();

	/** Grid snap setting callbacks */
	static void SetGridSize( int32 InIndex );
	static void SetRotationGridSize( int32 InIndex, ERotationGridMode InGridMode );
	static void SetScaleGridSize( int32 InIndex );
	static void SetLayer2D(int32 Layer2DIndex);

	/** Grid snap is checked callbacks for the menu values */
	static bool IsGridSizeChecked( int32 GridSizeIndex );
	static bool IsRotationGridSizeChecked( int32 GridSizeIndex, ERotationGridMode GridMode );
	static bool IsScaleGridSizeChecked( int32 GridSizeIndex );
	static bool IsLayer2DSelected(int32 Later2DIndex);

	/** Callbacks for preserving non-uniform scaling when snapping */
	static void TogglePreserveNonUniformScale();
	static bool IsPreserveNonUniformScaleChecked();
	
	/** Methods to build more complex duel lists */
	TSharedRef<SWidget> BuildLocationGridCheckBoxList(FName InExtentionHook, const FText& InHeading, const TArray<float>& InGridSizes) const;
	TSharedRef<SWidget> BuildRotationGridCheckBoxList(FName InExtentionHook, const FText& InHeading, const TArray<float>& InGridSizes, ERotationGridMode InGridMode) const;

	/** Make the surface snapping toolbar checkbox button */
	TSharedRef< SWidget > MakeSurfaceSnappingButton( FName ToolBarStyle );

	/** Grid Snap checked state callbacks */
	ECheckBoxState IsLocationGridSnapChecked() const;
	ECheckBoxState IsRotationGridSnapChecked() const;
	ECheckBoxState IsLayer2DSnapChecked() const;
	ECheckBoxState IsScaleGridSnapChecked() const;

	EVisibility IsLayer2DSnapVisible() const;

	/** Grid snap toggle handlers */
	void HandleToggleLocationGridSnap(ECheckBoxState InState);
	void HandleToggleRotationGridSnap(ECheckBoxState InState);
	void HandleToggleLayer2DSnap(ECheckBoxState InState);
	void HandleToggleScaleGridSnap(ECheckBoxState InState);

private:

	/** Reference to the camera slider used to display current camera speed */
	mutable TSharedPtr< SSlider > CamSpeedSlider;

	/** The editor viewport that we are in */
	TWeakPtr<class SEditorViewport> Viewport;

	/** Command list */
	TSharedPtr<FUICommandList> CommandList;
};
