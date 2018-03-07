// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "LevelViewportLayout.h"
#include "LevelViewportActions.h"

class SHorizontalBox;

class FLevelViewportLayoutOnePane : public FLevelViewportLayout
{
public:
	/**
	* Saves viewport layout information between editor sessions
	*/
	virtual void SaveLayoutString(const FString& LayoutString) const override;

	virtual const FName& GetLayoutTypeName() const override{ return LevelViewportConfigurationNames::OnePane; }
protected:
	/**
	* Creates the viewport for the single pane
	*/
	virtual TSharedRef<SWidget> MakeViewportLayout(const FString& LayoutString) override;

	/** Overridden from FLevelViewportLayout */
	virtual void ReplaceWidget(TSharedRef< SWidget > Source, TSharedRef< SWidget > Replacement) override;

protected:
	/** The viewport widget parent box */
	TSharedPtr< SHorizontalBox > ViewportBox;
};
