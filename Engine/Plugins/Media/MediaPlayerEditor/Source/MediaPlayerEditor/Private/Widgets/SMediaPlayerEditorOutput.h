// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class UMaterial;
class UMaterialExpressionTextureSample;
class UMediaPlayer;
class UMediaSoundComponent;
class UMediaTexture;

struct FSlateBrush;

enum class EMediaEvent;


/**
 * Handles content output in the viewer tab in the UMediaPlayer asset editor.
 */
class SMediaPlayerEditorOutput
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SMediaPlayerEditorOutput) { }
	SLATE_END_ARGS()

public:

	/** Default constructor. */
	SMediaPlayerEditorOutput();

	/** Destructor. */
	~SMediaPlayerEditorOutput();

public:

	/**
	 * Construct this widget
	 *
	 * @param InArgs The declaration data for this widget.
	 * @param InMediaPlayer The UMediaPlayer asset to show the details for.
	 */
	void Construct(const FArguments& InArgs, UMediaPlayer& InMediaPlayer);

public:

	//~ SWidget interface

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:

	/** Callback for media events from the media player. */
	void HandleMediaPlayerMediaEvent(EMediaEvent Event);

private:

	/** The material that wraps the video texture for display in an SImage. */
	UMaterial* Material;

	/** The Slate brush that renders the material. */
	TSharedPtr<FSlateBrush> MaterialBrush;

	/** The media player whose video texture is shown in this widget. */
	TWeakObjectPtr<UMediaPlayer> MediaPlayer;

	/** The media texture to render the media player's video output. */
	UMediaTexture* MediaTexture;

	/** The sound component to play the media player's audio output. */
	UMediaSoundComponent* SoundComponent;

	/** The video texture sampler in the wrapper material. */
	UMaterialExpressionTextureSample* TextureSampler;
};
