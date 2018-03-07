// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneSequencePlaybackSettingsCustomization.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "PropertyHandle.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "MovieSceneSequencePlayer.h"
#include "IDetailChildrenBuilder.h"


#define LOCTEXT_NAMESPACE "MovieSceneSequencePlaybackSettingsCustomization"


TSharedRef<IPropertyTypeCustomization> FMovieSceneSequencePlaybackSettingsCustomization::MakeInstance()
{
	return MakeShareable(new FMovieSceneSequencePlaybackSettingsCustomization);
}


void FMovieSceneSequencePlaybackSettingsCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	// do nothing
}


void FMovieSceneSequencePlaybackSettingsCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> InPropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	StructPropertyHandle = InPropertyHandle;
	ChildBuilder.AddProperty(StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMovieSceneSequencePlaybackSettings, PlayRate)).ToSharedRef());
	LoopCountProperty = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMovieSceneSequencePlaybackSettings, LoopCount));

	// Set up the initial environment
	{
		LoopModes.Add(MakeShareable( new FLoopMode{ LOCTEXT("DontLoop", "Don't Loop"), 0 } ));
		LoopModes.Add(MakeShareable( new FLoopMode{ LOCTEXT("Indefinitely", "Loop Indefinitely"), -1 }) );
		LoopModes.Add(MakeShareable( new FLoopMode{ LOCTEXT("Exactly", "Loop Exactly..."), 1 }) );

		int32 CurrentValue = -1;
		LoopCountProperty->GetValue(CurrentValue);

		TSharedPtr<FLoopMode>* FoundMode = LoopModes.FindByPredicate([&](const TSharedPtr<FLoopMode>& In){ return In->Value == CurrentValue; });
		if (FoundMode)
		{
			CurrentMode = *FoundMode;
		}
		else
		{
			CurrentMode = LoopModes.Last();
		}
	}

	ChildBuilder.AddCustomRow(LOCTEXT("LoopTitle", "Loop"))
		.NameContent()
		[
			LoopCountProperty->CreatePropertyNameWidget()
		]
		.ValueContent()
		.HAlign(HAlign_Fill)
		.MaxDesiredWidth(200)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SComboBox<TSharedPtr<FLoopMode>>)
						.OptionsSource(&LoopModes)
						.OnSelectionChanged_Lambda([&](TSharedPtr<FLoopMode> Mode, ESelectInfo::Type){
							CurrentMode = Mode;
							UpdateProperty();
						})
						.OnGenerateWidget_Lambda([&](TSharedPtr<FLoopMode> InMode){
							return SNew(STextBlock)
								.Font(IDetailLayoutBuilder::GetDetailFont())
								.Text(InMode->DisplayName);
						})
						.InitiallySelectedItem(CurrentMode)
						[
							SAssignNew(CurrentText, STextBlock)
								.Font(IDetailLayoutBuilder::GetDetailFont())
								.Text(CurrentMode->DisplayName)
						]
				]

			+ SHorizontalBox::Slot()
				.Padding(FMargin(4.f,0))
				.VAlign(VAlign_Center)
				[
					SAssignNew(LoopEntry, SHorizontalBox)
						.Visibility(CurrentMode == LoopModes.Last() ? EVisibility::Visible : EVisibility::Collapsed)

					+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.Padding(FMargin(0, 0, 4.f, 0))
						[
							LoopCountProperty->CreatePropertyValueWidget()
						]

					+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.AutoWidth()
						[
							SNew(STextBlock)
								.Font(IDetailLayoutBuilder::GetDetailFont())
								.Text_Raw(this, &FMovieSceneSequencePlaybackSettingsCustomization::GetCustomSuffix)
						]
				]

			+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SButton)
					.OnClicked(this, &FMovieSceneSequencePlaybackSettingsCustomization::OnLoopResetClicked)
					.Visibility(this, &FMovieSceneSequencePlaybackSettingsCustomization::GetLoopResetVisibility)
					.ContentPadding(FMargin(5.f, 0.f))
					.ToolTipText(LOCTEXT("ResetToDefaultToolTip", "Reset to Default"))
					.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
					.Content()
					[
						SNew(SImage)
						.Image( FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault") )
					]
				]
		];

	ChildBuilder.AddProperty(StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMovieSceneSequencePlaybackSettings, bRandomStartTime)).ToSharedRef());
	ChildBuilder.AddProperty(StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMovieSceneSequencePlaybackSettings, StartTime)).ToSharedRef());
	ChildBuilder.AddProperty(StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMovieSceneSequencePlaybackSettings, bRestoreState)).ToSharedRef());
	ChildBuilder.AddProperty(StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMovieSceneSequencePlaybackSettings, bDisableMovementInput)).ToSharedRef());
	ChildBuilder.AddProperty(StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMovieSceneSequencePlaybackSettings, bDisableLookAtInput)).ToSharedRef());
	ChildBuilder.AddProperty(StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMovieSceneSequencePlaybackSettings, bHidePlayer)).ToSharedRef());
	ChildBuilder.AddProperty(StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMovieSceneSequencePlaybackSettings, bHideHud)).ToSharedRef());
}


FText FMovieSceneSequencePlaybackSettingsCustomization::GetCustomSuffix() const
{
	int32 NumLoops = 0;
	LoopCountProperty->GetValue(NumLoops);

	return NumLoops == 1
		? LOCTEXT("Time", "time")
		: LOCTEXT("Times", "times");
}

EVisibility FMovieSceneSequencePlaybackSettingsCustomization::GetLoopResetVisibility() const
{
	return CurrentMode == LoopModes[0] ? EVisibility::Hidden : EVisibility::Visible;
}

FReply FMovieSceneSequencePlaybackSettingsCustomization::OnLoopResetClicked()
{
	CurrentMode = LoopModes[0];

	UpdateProperty();

	return FReply::Handled();
}

void FMovieSceneSequencePlaybackSettingsCustomization::UpdateProperty()
{
	if (CurrentMode == LoopModes.Last())
	{
		// Show custom loop entry
		LoopEntry->SetVisibility(EVisibility::Visible);
	}
	else
	{
		// Hide custom loop entry
		LoopEntry->SetVisibility(EVisibility::Collapsed);
	}

	LoopCountProperty->SetValue(CurrentMode->Value);
	CurrentText->SetText(CurrentMode->DisplayName);
}


#undef LOCTEXT_NAMESPACE
