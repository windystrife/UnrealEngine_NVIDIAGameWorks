// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Models/TextureEditorViewportClient.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Interfaces/ITextureEditorToolkit.h"

class FSceneViewport;
class SScrollBar;
class SViewport;

/**
 * Implements the texture editor's view port.
 */
class STextureEditorViewport
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(STextureEditorViewport) { }
	SLATE_END_ARGS()

public:

	/**
	 */
	void AddReferencedObjects( FReferenceCollector& Collector );

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The construction arguments.
	 */
	void Construct( const FArguments& InArgs, const TSharedRef<ITextureEditorToolkit>& InToolkit );
	
	/**
	 * Modifies the checkerboard texture's data.
	 */
	void ModifyCheckerboardTextureColors( );

	/**
	 * Gets the exposure bias.
	 *
	 * @return Exposure bias value.
	 */
	int32 GetExposureBias( ) const
	{
		return ExposureBias;
	}

	/** Enable viewport rendering */
	void EnableRendering();

	/** Disable viewport rendering */
	void DisableRendering();

	TSharedPtr<FSceneViewport> GetViewport( ) const;
	TSharedPtr<SViewport> GetViewportWidget( ) const;
	TSharedPtr<SScrollBar> GetVerticalScrollBar( ) const;
	TSharedPtr<SScrollBar> GetHorizontalScrollBar( ) const;

public:

	// SWidget overrides

	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;

protected:

	/**
	 * Gets the displayed textures resolution as a string.
	 *
	 * @return Texture resolution string.
	 */
	FText GetDisplayedResolution() const;

private:

	// Callback for getting the visibility of the exposure bias widget.
	EVisibility HandleExposureBiasWidgetVisibility( ) const;

	// Callback for getting the exposure bias.
	TOptional<int32> HandleExposureBiasBoxValue( ) const;

	// Callback for changing the exposure bias.
	void HandleExposureBiasBoxValueChanged( int32 NewExposure );

	// Callback for the horizontal scroll bar.
	void HandleHorizontalScrollBarScrolled( float InScrollOffsetFraction );

	// Callback for getting the visibility of the horizontal scroll bar.
	EVisibility HandleHorizontalScrollBarVisibility( ) const;

	// Callback for the vertical scroll bar.
	void HandleVerticalScrollBarScrolled( float InScrollOffsetFraction );

	// Callback for getting the visibility of the horizontal scroll bar.
	EVisibility HandleVerticalScrollBarVisibility( ) const;

	// Callback for clicking an item in the 'Zoom' menu.
	void HandleZoomMenuEntryClicked( double ZoomValue );

	// Callback for clicking the 'Fit' item in the 'Zoom' menu.
	void HandleZoomMenuFitClicked();

	// Callback for setting the checked state of the 'Fit' item in the 'Zoom' menu.
	bool IsZoomMenuFitChecked() const;

	// Callback for getting the zoom percentage text.
	FText HandleZoomPercentageText( ) const;

	// Callback for changes in the zoom slider.
	void HandleZoomSliderChanged( float NewValue );

	// Callback for getting the zoom slider's value.
	float HandleZoomSliderValue( ) const;

	// Checks if the texture being edited has a valid texture resource
	bool HasValidTextureResource( ) const;

private:

	// Which exposure level should be used, in FStop e.g. 0:original, -1:half as bright, 1:2x as bright, 2:4x as bright.
	int32 ExposureBias;

	// Pointer back to the Texture editor tool that owns us.
	TWeakPtr<ITextureEditorToolkit> ToolkitPtr;
	
	// Level viewport client.
	TSharedPtr<class FTextureEditorViewportClient> ViewportClient;

	// Slate viewport for rendering and IO.
	TSharedPtr<FSceneViewport> Viewport;

	// Viewport widget.
	TSharedPtr<SViewport> ViewportWidget;

	// Vertical scrollbar.
	TSharedPtr<SScrollBar> TextureViewportVerticalScrollBar;

	// Horizontal scrollbar.
	TSharedPtr<SScrollBar> TextureViewportHorizontalScrollBar;

	// Is rendering currently enabled? (disabled when reimporting a texture)
	bool bIsRenderingEnabled;
};
