// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SCompoundWidget.h"
#include "DeclarativeSyntaxSupport.h"

class FSequencerSectionKeyAreaNode;
class SOverlay;

class SKeyAreaEditorSwitcher : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SKeyAreaEditorSwitcher){}
	SLATE_END_ARGS()

	/** Construct the widget */
	void Construct(const FArguments& InArgs, TSharedRef<FSequencerSectionKeyAreaNode> InKeyAreaNode);

	/** Rebuild this widget from its cached key area node */
	void Rebuild();

private:

	/** Get the widget's visibility that corresponds to the specified key area index */
	EVisibility GetWidgetVisibility(int32 Index) const;

	/** Tick this widget. Updates the currently visible key editor */
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;

private:

	/** Our overlay widget */
	TSharedPtr<SOverlay> Overlay;
	/** Index of the currently visible key editor */
	int32 VisibleIndex;
	/** The key area to which we relate */
	TWeakPtr<FSequencerSectionKeyAreaNode> WeakKeyAreaNode;
};