// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Margin.h"
#include "Fonts/SlateFontInfo.h"
#include "Misc/Paths.h"
#include "Brushes/SlateBorderBrush.h"
#include "Brushes/SlateBoxBrush.h"
#include "Brushes/SlateImageBrush.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateStyle.h"

#define IMAGE_BRUSH(RelativePath, ...) FSlateImageBrush(RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define BOX_BRUSH(RelativePath, ...) FSlateBoxBrush(RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define BORDER_BRUSH(RelativePath, ...) FSlateBorderBrush(RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define TTF_FONT(RelativePath, ...) FSlateFontInfo(RootToContentDir(RelativePath, TEXT(".ttf")), __VA_ARGS__)
#define OTF_FONT(RelativePath, ...) FSlateFontInfo(RootToContentDir(RelativePath, TEXT(".otf")), __VA_ARGS__)


/**
 * Implements the visual style of the messaging debugger UI.
 */
class FLevelSequenceEditorStyle
	: public FSlateStyleSet
{
public:

	/** Default constructor. */
	 FLevelSequenceEditorStyle()
		 : FSlateStyleSet("LevelSequenceEditorStyle")
	 {
		const FVector2D Icon16x16(16.0f, 16.0f);
		const FVector2D Icon20x20(20.0f, 20.0f);
		const FVector2D Icon40x40(40.0f, 40.0f);

		SetContentRoot(FPaths::EnginePluginsDir() / TEXT("MovieScene/LevelSequenceEditor/Content"));

		// tab icons
		Set("LevelSequenceEditor.Tabs.Sequencer", new IMAGE_BRUSH("icon_tab_sequencer_16x", Icon16x16));
		Set("LevelSequenceEditor.PossessNewActor", new IMAGE_BRUSH("Icon_Actor_To_Sequencer_16x", Icon16x16));
		Set("LevelSequenceEditor.PossessNewActor.Small", new IMAGE_BRUSH("Icon_Actor_To_Sequencer_16x", Icon16x16));
		Set("LevelSequenceEditor.CreateNewLevelSequenceInLevel", new IMAGE_BRUSH("CreateNewLevelSequenceInLevel_16x", Icon16x16));
		Set("LevelSequenceEditor.CreateNewLevelSequenceInLevel.Small", new IMAGE_BRUSH("CreateNewLevelSequenceInLevel_16x", Icon16x16));
		Set("LevelSequenceEditor.CreateNewMasterSequenceInLevel", new IMAGE_BRUSH("CreateNewMasterSequenceInLevel_16x", Icon16x16));
		Set("LevelSequenceEditor.CreateNewMasterSequenceInLevel.Small", new IMAGE_BRUSH("CreateNewMasterSequenceInLevel_16x", Icon16x16));
		
		Set("LevelSequenceEditor.CinematicViewportPlayMarker", new IMAGE_BRUSH("CinematicViewportPlayMarker", FVector2D(11, 6)));
		Set("LevelSequenceEditor.CinematicViewportRangeStart", new BORDER_BRUSH("CinematicViewportRangeStart", FMargin(1.f,.3f,0.f,.6f)));
		Set("LevelSequenceEditor.CinematicViewportRangeEnd", new BORDER_BRUSH("CinematicViewportRangeEnd", FMargin(0.f,.3f,1.f,.6f)));

		Set("LevelSequenceEditor.CinematicViewportTransportRangeKey", new IMAGE_BRUSH("CinematicViewportTransportRangeKey", FVector2D(7.f, 7.f)));

		Set("FilmOverlay.DefaultThumbnail", new IMAGE_BRUSH("DefaultFilmOverlayThumbnail", FVector2D(36, 24)));

		Set("FilmOverlay.Disabled", new IMAGE_BRUSH("FilmOverlay.Disabled", FVector2D(36, 24)));
		Set("FilmOverlay.2x2Grid", new IMAGE_BRUSH("FilmOverlay.2x2Grid", FVector2D(36, 24)));
		Set("FilmOverlay.3x3Grid", new IMAGE_BRUSH("FilmOverlay.3x3Grid", FVector2D(36, 24)));
		Set("FilmOverlay.Crosshair", new IMAGE_BRUSH("FilmOverlay.Crosshair", FVector2D(36, 24)));
		Set("FilmOverlay.Rabatment", new IMAGE_BRUSH("FilmOverlay.Rabatment", FVector2D(36, 24)));

		FSlateStyleRegistry::RegisterSlateStyle(*this);
	 }

	 /** Virtual destructor. */
	 virtual ~FLevelSequenceEditorStyle()
	 {
		FSlateStyleRegistry::UnRegisterSlateStyle(*this);
	 }

	static TSharedRef<FLevelSequenceEditorStyle> Get()
	{
		if (!Singleton.IsValid())
		{
			Singleton = MakeShareable(new FLevelSequenceEditorStyle);
		}
		return Singleton.ToSharedRef();
	}

private:
	static TSharedPtr<FLevelSequenceEditorStyle> Singleton;
};


#undef IMAGE_BRUSH
#undef BOX_BRUSH
#undef BORDER_BRUSH
#undef TTF_FONT
#undef OTF_FONT
