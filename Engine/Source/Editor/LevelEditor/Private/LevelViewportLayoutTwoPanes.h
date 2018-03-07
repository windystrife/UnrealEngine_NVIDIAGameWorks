// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "LevelViewportLayout.h"
#include "Widgets/Layout/SSplitter.h"
#include "LevelViewportActions.h"

template <EOrientation TOrientation>
class TLevelViewportLayoutTwoPanes : public FLevelViewportLayout
{
public:
	/**
	 * Saves viewport layout information between editor sessions
	 */
	virtual void SaveLayoutString(const FString& LayoutString) const override;
protected:
	/**
	 * Creates the viewports and splitter for the two panes vertical layout                   
	 */
	virtual TSharedRef<SWidget> MakeViewportLayout(const FString& LayoutString) override;

	/** Overridden from FLevelViewportLayout */
	virtual void ReplaceWidget( TSharedRef< SWidget > Source, TSharedRef< SWidget > Replacement ) override;


private:
	/** The splitter widget */
	TSharedPtr< class SSplitter > SplitterWidget;
};


// FLevelViewportLayoutTwoPanesVert /////////////////////////////

class FLevelViewportLayoutTwoPanesVert : public TLevelViewportLayoutTwoPanes<EOrientation::Orient_Vertical>
{
public:
	virtual const FName& GetLayoutTypeName() const override { return LevelViewportConfigurationNames::TwoPanesVert; }
};


// FLevelViewportLayoutTwoPanesHoriz /////////////////////////////

class FLevelViewportLayoutTwoPanesHoriz : public TLevelViewportLayoutTwoPanes<EOrientation::Orient_Horizontal>
{
public:
	virtual const FName& GetLayoutTypeName() const override { return LevelViewportConfigurationNames::TwoPanesHoriz; }
};
