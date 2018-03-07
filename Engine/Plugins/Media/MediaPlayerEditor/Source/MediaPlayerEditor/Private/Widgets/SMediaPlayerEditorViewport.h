// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/ISlateStyle.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class UMediaPlayer;


class SMediaPlayerEditorViewport
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SMediaPlayerEditorViewport) { }
	SLATE_END_ARGS()

public:

	/** Default constructor. */
	SMediaPlayerEditorViewport();

public:

	/**
	 * Construct this widget
	 *
	 * @param InArgs The declaration data for this widget.
	 * @param InMediaPlayer The UMediaPlayer asset to show the details for.
	 * @param InStyleSet The style set to use.
	 */
	void Construct(const FArguments& InArgs, UMediaPlayer& InMediaPlayer, const TSharedRef<ISlateStyle>& InStyle);

public:

	//~ SWidget interface

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

private:

	/** Callback for getting the text of the player name overlay. */
	FText HandleMediaPlayerNameText() const;

	/** Callback for getting the text of the playback state overlay. */
	FText HandleMediaPlayerStateText() const;

	/** Callback for getting the text of the media source name overlay. */
	FText HandleMediaSourceNameText() const;

	/** Callback for getting the text of the view settings overlay. */
	FText HandleViewSettingsText() const;

private:

	/** Pointer to the media player that is being viewed. */
	UMediaPlayer* MediaPlayer;

	/** The style set to use for this widget. */
	TSharedPtr<ISlateStyle> Style;
};