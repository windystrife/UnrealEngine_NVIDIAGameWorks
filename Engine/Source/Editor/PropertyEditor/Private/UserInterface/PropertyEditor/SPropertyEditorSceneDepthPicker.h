// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "SceneDepthPickerMode.h"
#include "Widgets/Input/SButton.h"

class SPropertyEditorSceneDepthPicker : public SButton
{
public:
	SLATE_BEGIN_ARGS(SPropertyEditorSceneDepthPicker) {}

	SLATE_EVENT(FOnSceneDepthLocationSelected, OnSceneDepthLocationSelected)

	SLATE_END_ARGS()

	~SPropertyEditorSceneDepthPicker();

	void Construct( const FArguments& InArgs );

	/** Begin SWidget interface */
	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override;
	virtual bool SupportsKeyboardFocus() const override;
	/** End SWidget interface */

private:
	/** Delegate for when the button is clicked */
	FReply OnClicked();

	/** Called when scene depth location is picked */
	FOnSceneDepthLocationSelected OnSceneDepthLocationSelected;
};
