// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateColor.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "ISequencerSection.h"

class FSequencerSectionPainter;
class ISectionLayoutBuilder;
class UMovieSceneLevelVisibilitySection;

/**
 * A seqeuencer section for displaying and interacting with level visibility movie scene sections. 
 */
class FLevelVisibilitySection
	: public ISequencerSection
	, public TSharedFromThis<FLevelVisibilitySection>
{
public:

	FLevelVisibilitySection( UMovieSceneLevelVisibilitySection& InSectionObject );

public:

	// ISequencerSectionInterface
	virtual UMovieSceneSection* GetSectionObject() override;
	virtual TSharedRef<SWidget> GenerateSectionWidget() override;
	virtual int32 OnPaintSection( FSequencerSectionPainter& InPainter ) const override;
	virtual void GenerateSectionLayout( ISectionLayoutBuilder& LayoutBuilder ) const override { }

private:

	FSlateColor GetBackgroundColor() const;
	FText GetVisibilityText() const;
	FText GetVisibilityToolTip() const;

	bool OnAllowDrop( TSharedPtr<FDragDropOperation> DragDropOperation );
	FReply OnDrop( TSharedPtr<FDragDropOperation> DragDropOperation );

private:

	FText VisibleText;
	FText HiddenText;

	UMovieSceneLevelVisibilitySection& SectionObject;

	FText DisplayName;
};
