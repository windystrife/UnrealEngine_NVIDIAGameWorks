// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#include "UserInterface/PropertyEditor/SPropertyEditorInteractiveActorPicker.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Images/SImage.h"
#include "EditorStyleSet.h"

#define LOCTEXT_NAMESPACE "PropertyPicker"

SPropertyEditorInteractiveActorPicker::~SPropertyEditorInteractiveActorPicker()
{
	FActorPickerModeModule& ActorPickerMode = FModuleManager::Get().GetModuleChecked<FActorPickerModeModule>("ActorPickerMode");

	// make sure we are unregistered when this widget goes away
	ActorPickerMode.EndActorPickingMode();
}

void SPropertyEditorInteractiveActorPicker::Construct( const FArguments& InArgs )
{
	OnActorSelected = InArgs._OnActorSelected;
	OnGetAllowedClasses = InArgs._OnGetAllowedClasses;
	OnShouldFilterActor = InArgs._OnShouldFilterActor;

	SButton::Construct(
		SButton::FArguments()
		.ButtonStyle( FEditorStyle::Get(), "HoverHintOnly" )
		.OnClicked( this, &SPropertyEditorInteractiveActorPicker::OnClicked )
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

FReply SPropertyEditorInteractiveActorPicker::OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	if(InKeyEvent.GetKey() == EKeys::Escape)
	{
		FActorPickerModeModule& ActorPickerMode = FModuleManager::Get().GetModuleChecked<FActorPickerModeModule>("ActorPickerMode");

		if(ActorPickerMode.IsInActorPickingMode())
		{
			ActorPickerMode.EndActorPickingMode();
			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

bool SPropertyEditorInteractiveActorPicker::SupportsKeyboardFocus() const
{
	return true;
}

FReply SPropertyEditorInteractiveActorPicker::OnClicked()
{
	FActorPickerModeModule& ActorPickerMode = FModuleManager::Get().GetModuleChecked<FActorPickerModeModule>("ActorPickerMode");

	if(ActorPickerMode.IsInActorPickingMode())
	{
		ActorPickerMode.EndActorPickingMode();
	}
	else
	{
		ActorPickerMode.BeginActorPickingMode(OnGetAllowedClasses, OnShouldFilterActor, OnActorSelected);
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
