// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AboutScreen.h"
#include "Fonts/SlateFontInfo.h"
#include "Misc/Paths.h"
#include "Misc/EngineVersion.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/SListView.h"
#include "EditorStyleSet.h"
#include "UnrealEdMisc.h"
#include "IDocumentation.h"

#define LOCTEXT_NAMESPACE "AboutScreen"

void SAboutScreen::Construct(const FArguments& InArgs)
{
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4428)	// universal-character-name encountered in source
#endif
	AboutLines.Add(MakeShareable(new FLineDefinition(LOCTEXT("Copyright1", "Copyright 1998-2017 Epic Games, Inc. All rights reserved"), 11, FLinearColor(1.f, 1.f, 1.f), FMargin(0.f) )));
	AboutLines.Add(MakeShareable(new FLineDefinition(LOCTEXT("Copyright2", "Epic, Epic Games, Unreal, and their respective logos are trademarks or registered trademarks of Epic Games, Inc.\nin the United States of America and elsewhere."), 8, FLinearColor(1.f, 1.f, 1.f), FMargin(0.0f,2.0f) )));

#ifdef _MSC_VER
#pragma warning(pop)
#endif

	FText Version = FText::Format( LOCTEXT("VersionLabel", "Version: {0}"), FText::FromString( FEngineVersion::Current().ToString( ) ) );

	ChildSlot
		[
			SNew(SOverlay)
			+SOverlay::Slot()
			[
				SNew(SImage).Image(FEditorStyle::GetBrush(TEXT("AboutScreen.Background")))
			]

			+SOverlay::Slot()
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Top)
					.Padding(FMargin(10.f, 10.f, 0.f, 0.f))
					[
						SAssignNew(UE4Button, SButton)
						.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
						.OnClicked(this, &SAboutScreen::OnUE4ButtonClicked)
						[
							SNew(SImage).Image(this, &SAboutScreen::GetUE4ButtonBrush)
						]
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Top)
					.Padding(FMargin(10.f, 10.f, 0.f, 0.f))
					[
						SAssignNew(EpicGamesButton, SButton)
						.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
						.OnClicked(this, &SAboutScreen::OnEpicGamesButtonClicked)
						[
							SNew(SImage).Image(this, &SAboutScreen::GetEpicGamesButtonBrush)
						]
					]
					+SHorizontalBox::Slot()
					.FillWidth(1.f)
					.HAlign(HAlign_Right)
					.Padding(FMargin(0.f, 52.f, 7.f, 0.f))
					[
						SNew(SEditableText)
						.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f))
						.IsReadOnly(true)
						.Text( Version )
					]
				]
				+SVerticalBox::Slot()
				.Padding(FMargin(5.f, 5.f, 5.f, 5.f))
				.VAlign(VAlign_Top)
				[
					SNew(SListView<TSharedRef<FLineDefinition>>)
					.ListItemsSource(&AboutLines)
					.OnGenerateRow(this, &SAboutScreen::MakeAboutTextItemWidget)
					.SelectionMode( ESelectionMode::None )
				] 
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.HAlign(HAlign_Left)
					.Padding(FMargin(5.f, 0.f, 5.f, 5.f)) 
					[
						SAssignNew(FacebookButton, SButton)
						.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
						.ToolTipText(LOCTEXT("FacebookToolTip", "Unreal Engine on Facebook"))
						.OnClicked(this, &SAboutScreen::OnFacebookButtonClicked)
						[
							SNew(SImage).Image(this, &SAboutScreen::GetFacebookButtonBrush)
						]
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Bottom)
					.Padding(FMargin(5.f, 0.f, 5.f,5.f))
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.Text(LOCTEXT("Close", "Close"))
						.ButtonColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
						.OnClicked(this, &SAboutScreen::OnClose)
					]
				]
			]
		];
}

TSharedRef<ITableRow> SAboutScreen::MakeAboutTextItemWidget(TSharedRef<FLineDefinition> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	if( Item->Text.IsEmpty() )
	{
		return
			SNew(STableRow< TSharedPtr<FString> >, OwnerTable)
			.Padding(6.0f)
			[
				SNew(SSpacer)
			];
	}
	else 
	{
		return
			SNew(STableRow< TSharedPtr<FString> >, OwnerTable)
			.Padding( Item->Margin )
			[
				SNew(STextBlock)
				.ColorAndOpacity( Item->TextColor )
				.Font(FSlateFontInfo( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), Item->FontSize ))
				.Text( Item->Text )
			];
	}
}

const FSlateBrush* SAboutScreen::GetUE4ButtonBrush() const
{
	return FEditorStyle::GetBrush(!UE4Button->IsHovered() ? TEXT("AboutScreen.UE4") : TEXT("AboutScreen.UE4Hovered"));
}

const FSlateBrush* SAboutScreen::GetEpicGamesButtonBrush() const
{
	return FEditorStyle::GetBrush(!EpicGamesButton->IsHovered() ? TEXT("AboutScreen.EpicGames") : TEXT("AboutScreen.EpicGamesHovered"));
}

const FSlateBrush* SAboutScreen::GetFacebookButtonBrush() const
{
	return FEditorStyle::GetBrush(!FacebookButton->IsHovered() ? TEXT("AboutScreen.Facebook") : TEXT("AboutScreen.FacebookHovered"));
}

FReply SAboutScreen::OnUE4ButtonClicked()
{
	IDocumentation::Get()->OpenHome(FDocumentationSourceInfo(TEXT("logo_docs")));
	return FReply::Handled();
}

FReply SAboutScreen::OnEpicGamesButtonClicked()
{
	FString EpicGamesURL;
	if(FUnrealEdMisc::Get().GetURL( TEXT("EpicGamesURL"), EpicGamesURL ))
	{
		FPlatformProcess::LaunchURL( *EpicGamesURL, NULL, NULL );
	}
	return FReply::Handled();
}

FReply SAboutScreen::OnFacebookButtonClicked()
{
	FString FacebookURL;
	if(FUnrealEdMisc::Get().GetURL( TEXT("FacebookURL"), FacebookURL ))
	{
		FPlatformProcess::LaunchURL( *FacebookURL, NULL, NULL );
	}
	return FReply::Handled();
}

FReply SAboutScreen::OnClose()
{
	TSharedRef<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow( AsShared() ).ToSharedRef();
	FSlateApplication::Get().RequestDestroyWindow( ParentWindow );
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
