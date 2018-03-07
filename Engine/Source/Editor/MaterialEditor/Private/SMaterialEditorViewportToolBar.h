// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "SViewportToolBar.h"
#include "SCommonEditorViewportToolbarBase.h"
#include "SMaterialEditorViewport.h"

///////////////////////////////////////////////////////////
// SMaterialEditorViewportToolBar

// In-viewport toolbar widget used in the material editor
class SMaterialEditorViewportToolBar : public SCommonEditorViewportToolbarBase
{
public:
	SLATE_BEGIN_ARGS(SMaterialEditorViewportToolBar) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<class SMaterialEditor3DPreviewViewport> InViewport);

	// SCommonEditorViewportToolbarBase interface
	virtual TSharedRef<SWidget> GenerateShowMenu() const override;
	// End of SCommonEditorViewportToolbarBase

	virtual bool IsViewModeSupported(EViewModeIndex ViewModeIndex) const override; 
};

///////////////////////////////////////////////////////////
// SMaterialEditorViewportPreviewShapeToolBar

class SMaterialEditorViewportPreviewShapeToolBar : public SViewportToolBar
{
public:
	SLATE_BEGIN_ARGS(SMaterialEditorViewportPreviewShapeToolBar){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<class SMaterialEditor3DPreviewViewport> InViewport);
};
