// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SWidget.h"
#include "SViewportToolBar.h"
#include "UICommandList.h"

///////////////////////////////////////////////////////////
// SBlastVectorViewportToolBar

class SBlastVectorViewportToolBar : public SViewportToolBar
{

public:
	SLATE_BEGIN_ARGS(SBlastVectorViewportToolBar) {}
		SLATE_ARGUMENT(TSharedPtr<class SEditorViewport>, Viewport)
		SLATE_ARGUMENT(TSharedPtr<FUICommandList>, CommandList)
		SLATE_ARGUMENT(TSharedPtr<FExtender>, Extenders)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	TSharedRef< SWidget > MakeTransformToolBar(const TSharedPtr< FExtender > InExtenders);

private:

	/** The editor viewport that we are in */
	TWeakPtr<SEditorViewport> Viewport;

	/** Command list */
	TSharedPtr<FUICommandList> CommandList;
};



///////////////////////////////////////////////////////////
// SBlastMeshEditorViewportToolbar

class SBlastMeshEditorViewportToolbar : public SViewportToolBar
{
public:
	SLATE_BEGIN_ARGS(SBlastMeshEditorViewportToolbar) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TWeakPtr<class SBlastMeshEditorViewport> InInfoProvider);

	TSharedRef<SWidget> GenerateCameraMenu() const;
	TSharedRef<SWidget> GenerateShowMenu() const;
	TSharedRef<SWidget> GenerateOptionsMenu() const;
	TSharedRef<SWidget> GenerateFOVMenu() const;
	float OnGetFOVValue() const;
	void OnFOVValueChanged(float NewValue);

	FText GetCameraMenuLabel() const;
	const FSlateBrush* GetCameraMenuLabelIcon() const;

private:
	/** The viewport that we are in */
	TWeakPtr<SBlastMeshEditorViewport> Viewport;
};
