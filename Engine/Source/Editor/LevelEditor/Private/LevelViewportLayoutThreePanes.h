// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "LevelViewportLayout.h"
#include "Widgets/Layout/SSplitter.h"
#include "LevelViewportActions.h"

class FLevelViewportLayoutThreePanes : public FLevelViewportLayout
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

	virtual TSharedRef<SWidget> MakeThreePanelWidget(
		TMap< FName, TSharedPtr< IViewportLayoutEntity >>& ViewportWidgets,
		const TSharedRef<SWidget>& ViewportKey0, const TSharedRef<SWidget>& ViewportKey1, const TSharedRef<SWidget>& ViewportKey2,
		float PrimarySplitterPercentage, float SecondarySplitterPercentage) = 0;

	/** Overridden from FLevelViewportLayout */
	virtual void ReplaceWidget( TSharedRef< SWidget > Source, TSharedRef< SWidget > Replacement ) override;


protected:
	/** The splitter widgets */
	TSharedPtr< class SSplitter > PrimarySplitterWidget;
	TSharedPtr< class SSplitter > SecondarySplitterWidget;
};

// FLevelViewportLayoutThreePanesLeft /////////////////////////////

class FLevelViewportLayoutThreePanesLeft : public FLevelViewportLayoutThreePanes
{
public:
	virtual const FName& GetLayoutTypeName() const override { return LevelViewportConfigurationNames::ThreePanesLeft; }

	virtual TSharedRef<SWidget> MakeThreePanelWidget(
		TMap< FName, TSharedPtr< IViewportLayoutEntity >>& ViewportWidgets,
		const TSharedRef<SWidget>& ViewportKey0, const TSharedRef<SWidget>& ViewportKey1, const TSharedRef<SWidget>& ViewportKey2,
		float PrimarySplitterPercentage, float SecondarySplitterPercentage) override;
};


// FLevelViewportLayoutThreePanesRight /////////////////////////////

class FLevelViewportLayoutThreePanesRight : public FLevelViewportLayoutThreePanes
{
public:
	virtual const FName& GetLayoutTypeName() const override { return LevelViewportConfigurationNames::ThreePanesRight; }

	virtual TSharedRef<SWidget> MakeThreePanelWidget(
		TMap< FName, TSharedPtr< IViewportLayoutEntity >>& ViewportWidgets,
		const TSharedRef<SWidget>& ViewportKey0, const TSharedRef<SWidget>& ViewportKey1, const TSharedRef<SWidget>& ViewportKey2,
		float PrimarySplitterPercentage, float SecondarySplitterPercentage) override;
};


// FLevelViewportLayoutThreePanesTop /////////////////////////////

class FLevelViewportLayoutThreePanesTop : public FLevelViewportLayoutThreePanes
{
public:
	virtual const FName& GetLayoutTypeName() const override { return LevelViewportConfigurationNames::ThreePanesTop; }

	virtual TSharedRef<SWidget> MakeThreePanelWidget(
		TMap< FName, TSharedPtr< IViewportLayoutEntity >>& ViewportWidgets,
		const TSharedRef<SWidget>& ViewportKey0, const TSharedRef<SWidget>& ViewportKey1, const TSharedRef<SWidget>& ViewportKey2,
		float PrimarySplitterPercentage, float SecondarySplitterPercentage) override;
};


// FLevelViewportLayoutThreePanesBottom /////////////////////////////

class FLevelViewportLayoutThreePanesBottom : public FLevelViewportLayoutThreePanes
{
public:
	virtual const FName& GetLayoutTypeName() const override { return LevelViewportConfigurationNames::ThreePanesBottom; }

	virtual TSharedRef<SWidget> MakeThreePanelWidget(
		TMap< FName, TSharedPtr< IViewportLayoutEntity >>& ViewportWidgets,
		const TSharedRef<SWidget>& ViewportKey0, const TSharedRef<SWidget>& ViewportKey1, const TSharedRef<SWidget>& ViewportKey2,
		float PrimarySplitterPercentage, float SecondarySplitterPercentage) override;
};
