// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "LevelViewportLayout.h"
#include "Widgets/Layout/SSplitter.h"
#include "LevelViewportActions.h"

class FLevelViewportLayoutFourPanes : public FLevelViewportLayout
{
public:
	/**
	 * Saves viewport layout information between editor sessions
	 */
	virtual void SaveLayoutString(const FString& LayoutString) const override;
protected:
	/**
	 * Creates the viewports and splitter for the four-pane layout              
	 */
	virtual TSharedRef<SWidget> MakeViewportLayout(const FString& LayoutString) override;

	virtual TSharedRef<SWidget> MakeFourPanelWidget(
		TMap<FName, TSharedPtr< IViewportLayoutEntity >>& ViewportWidgets,
		TSharedRef<SWidget> Viewport0, TSharedRef<SWidget> Viewport1, TSharedRef<SWidget> Viewport2, TSharedRef<SWidget> Viewport3,
		float PrimarySplitterPercentage, float SecondarySplitterPercentage0, float SecondarySplitterPercentage1) = 0;

	/** Overridden from FLevelViewportLayout */
	virtual void ReplaceWidget( TSharedRef< SWidget > Source, TSharedRef< SWidget > Replacement ) override;


protected:
	/** The splitter widgets */
	TSharedPtr< class SSplitter > PrimarySplitterWidget;
	TSharedPtr< class SSplitter > SecondarySplitterWidget;
};


// FLevelViewportLayoutFourPanesLeft /////////////////////////////

class FLevelViewportLayoutFourPanesLeft : public FLevelViewportLayoutFourPanes
{
public:
	virtual const FName& GetLayoutTypeName() const override { return LevelViewportConfigurationNames::FourPanesLeft; }

	virtual TSharedRef<SWidget> MakeFourPanelWidget(
		TMap<FName, TSharedPtr< IViewportLayoutEntity >>& ViewportWidgets,
		TSharedRef<SWidget> Viewport0, TSharedRef<SWidget> Viewport1, TSharedRef<SWidget> Viewport2, TSharedRef<SWidget> Viewport3,
		float PrimarySplitterPercentage, float SecondarySplitterPercentage0, float SecondarySplitterPercentage1) override;
};


// FLevelViewportLayoutFourPanesRight /////////////////////////////

class FLevelViewportLayoutFourPanesRight : public FLevelViewportLayoutFourPanes
{
public:
	virtual const FName& GetLayoutTypeName() const override { return LevelViewportConfigurationNames::FourPanesRight; }

	virtual TSharedRef<SWidget> MakeFourPanelWidget(
		TMap<FName, TSharedPtr< IViewportLayoutEntity >>& ViewportWidgets,
		TSharedRef<SWidget> Viewport0, TSharedRef<SWidget> Viewport1, TSharedRef<SWidget> Viewport2, TSharedRef<SWidget> Viewport3,
		float PrimarySplitterPercentage, float SecondarySplitterPercentage0, float SecondarySplitterPercentage1) override;
};


// FLevelViewportLayoutFourPanesTop /////////////////////////////

class FLevelViewportLayoutFourPanesTop : public FLevelViewportLayoutFourPanes
{
public:
	virtual const FName& GetLayoutTypeName() const override { return LevelViewportConfigurationNames::FourPanesTop; }

	virtual TSharedRef<SWidget> MakeFourPanelWidget(
		TMap<FName, TSharedPtr< IViewportLayoutEntity >>& ViewportWidgets,
		TSharedRef<SWidget> Viewport0, TSharedRef<SWidget> Viewport1, TSharedRef<SWidget> Viewport2, TSharedRef<SWidget> Viewport3,
		float PrimarySplitterPercentage, float SecondarySplitterPercentage0, float SecondarySplitterPercentage1) override;
};


// FLevelViewportLayoutFourPanesBottom /////////////////////////////

class FLevelViewportLayoutFourPanesBottom : public FLevelViewportLayoutFourPanes
{
public:
	virtual const FName& GetLayoutTypeName() const override { return LevelViewportConfigurationNames::FourPanesBottom; }

	virtual TSharedRef<SWidget> MakeFourPanelWidget(
		TMap<FName, TSharedPtr< IViewportLayoutEntity >>& ViewportWidgets,
		TSharedRef<SWidget> Viewport0, TSharedRef<SWidget> Viewport1, TSharedRef<SWidget> Viewport2, TSharedRef<SWidget> Viewport3,
		float PrimarySplitterPercentage, float SecondarySplitterPercentage0, float SecondarySplitterPercentage1) override;
};
