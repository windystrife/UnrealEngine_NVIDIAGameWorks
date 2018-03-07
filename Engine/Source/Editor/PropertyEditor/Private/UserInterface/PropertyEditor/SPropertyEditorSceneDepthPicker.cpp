// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#include "UserInterface/PropertyEditor/SPropertyEditorSceneDepthPicker.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Images/SImage.h"
#include "EditorStyleSet.h"
#include "ActorPickerMode.h"

#define LOCTEXT_NAMESPACE "SceneDepthPicker"

SPropertyEditorSceneDepthPicker::~SPropertyEditorSceneDepthPicker()
{
	FActorPickerModeModule& ActorPickerMode = FModuleManager::Get().GetModuleChecked<FActorPickerModeModule>("ActorPickerMode");

	// make sure we are unregistered when this widget goes away
	ActorPickerMode.EndActorPickingMode();
}

void SPropertyEditorSceneDepthPicker::Construct(const FArguments& InArgs)
{
	OnSceneDepthLocationSelected = InArgs._OnSceneDepthLocationSelected;

	SButton::Construct(
		SButton::FArguments()
		.ButtonStyle( FEditorStyle::Get(), "HoverHintOnly" )
		.OnClicked(this, &SPropertyEditorSceneDepthPicker::OnClicked)
		.ContentPadding(4.0f)
		.ForegroundColor( FSlateColor::UseForeground() )
		.IsFocusable(false)
		[ 
			SNew( SImage )
			.Image( FEditorStyle::GetBrush("PropertyWindow.Button_PickActorInteractive") )
			.ColorAndOpacity( FSlateColor::UseForeground() )
		]
	);
}

FReply SPropertyEditorSceneDepthPicker::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if(InKeyEvent.GetKey() == EKeys::Escape)
	{
		FSceneDepthPickerModeModule& SceneDepthPickerMode = FModuleManager::Get().GetModuleChecked<FSceneDepthPickerModeModule>("SceneDepthPickerMode");

		if (SceneDepthPickerMode.IsInSceneDepthPickingMode())
		{
			SceneDepthPickerMode.EndSceneDepthPickingMode();
			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

bool SPropertyEditorSceneDepthPicker::SupportsKeyboardFocus() const
{
	return true;
}

FReply SPropertyEditorSceneDepthPicker::OnClicked()
{
	FSceneDepthPickerModeModule& SceneDepthPickerMode = FModuleManager::Get().GetModuleChecked<FSceneDepthPickerModeModule>("SceneDepthPickerMode");

	if (SceneDepthPickerMode.IsInSceneDepthPickingMode())
	{
		SceneDepthPickerMode.EndSceneDepthPickingMode();
	}
	else
	{
		SceneDepthPickerMode.BeginSceneDepthPickingMode(OnSceneDepthLocationSelected);
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
