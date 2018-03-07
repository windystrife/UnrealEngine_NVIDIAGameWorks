// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SSourceControlPicker.h"
#if SOURCE_CONTROL_WITH_SLATE
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SComboButton.h"
#endif
#include "ISourceControlModule.h"
#include "SourceControlModule.h"

#if SOURCE_CONTROL_WITH_SLATE

#include "SSourceControlLogin.h"
#include "EditorStyleSet.h"

#define LOCTEXT_NAMESPACE "SSourceControlPicker"

void SSourceControlPicker::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew( SBorder )
		.BorderImage( FEditorStyle::GetBrush("DetailsView.CategoryTop") )
		.Padding( FMargin( 0.0f, 3.0f, 1.0f, 0.0f ) )
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.FillWidth(1.0f)
			.Padding(2.0f)
			[
				SNew( STextBlock )
				.Text( LOCTEXT("ProviderLabel", "Provider") )
				.Font( FEditorStyle::GetFontStyle(TEXT("SourceControl.LoginWindow.Font")) )
			]
			+SHorizontalBox::Slot()
			.FillWidth(2.0f)
			[
				SNew(SComboButton)
				.OnGetMenuContent(this, &SSourceControlPicker::OnGetMenuContent)
				.ContentPadding(1)
				.ToolTipText( LOCTEXT("ChooseProvider", "Choose the source control provider you want to use before you edit login settings.") )
				.ButtonContent()
				[
					SNew( STextBlock )
					.Text( this, &SSourceControlPicker::OnGetButtonText )
					.Font( FEditorStyle::GetFontStyle(TEXT("SourceControl.LoginWindow.Font")) )
				]
			]
		]
	];
}

void SSourceControlPicker::ChangeSourceControlProvider(int32 ProviderIndex) const
{
	FSourceControlModule& SourceControlModule = FSourceControlModule::Get();
	SourceControlModule.SetCurrentSourceControlProvider(ProviderIndex);

	if(SourceControlModule.GetLoginWidget().IsValid())
	{
		SourceControlModule.GetLoginWidget()->RefreshSettings();
	}
}

TSharedRef<SWidget> SSourceControlPicker::OnGetMenuContent() const
{
	FSourceControlModule& SourceControlModule = FSourceControlModule::Get();

	FMenuBuilder MenuBuilder(true, NULL);

	// Get the provider names first so that we can sort them for the UI
	TArray< TPair<FName, int32> > SortedProviderNames;
	const int NumProviders = SourceControlModule.GetNumSourceControlProviders();
	SortedProviderNames.Reserve(NumProviders);
	for(int ProviderIndex = 0; ProviderIndex < NumProviders; ++ProviderIndex)
	{
		const FName ProviderName = SourceControlModule.GetSourceControlProviderName(ProviderIndex);
		int32 ProviderSortKey = ProviderName == FName("None") ? -1 * ProviderIndex : ProviderIndex;
		SortedProviderNames.Emplace(ProviderName, ProviderSortKey);
	}

	// Sort based on the provider index
	SortedProviderNames.Sort([](const TPair<FName, int32>& One, const TPair<FName, int32>& Two)
	{
		return One.Value < Two.Value;
	});

	for(auto SortedProviderNameIter = SortedProviderNames.CreateConstIterator(); SortedProviderNameIter; ++SortedProviderNameIter)
	{
		const FText ProviderText = GetProviderText(SortedProviderNameIter->Key);
		const int32  ProviderIndex = SortedProviderNameIter->Value;

		FFormatNamedArguments Arguments;
		Arguments.Add( TEXT("ProviderName"), ProviderText );
		MenuBuilder.AddMenuEntry(
			ProviderText,
			FText::Format(LOCTEXT("SourceControlProvider_Tooltip", "Use {ProviderName} as source control provider"), Arguments),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP( this, &SSourceControlPicker::ChangeSourceControlProvider, FMath::Abs(ProviderIndex) ),
				FCanExecuteAction()
				)
			);
	}

	return MenuBuilder.MakeWidget();
}

FText SSourceControlPicker::OnGetButtonText() const
{
	return GetProviderText( ISourceControlModule::Get().GetProvider().GetName() );
}

FText SSourceControlPicker::GetProviderText(const FName& InName) const
{
	if(InName == "None")
	{
		return LOCTEXT("NoProviderDescription", "None  (source control disabled)");
	}

	// @todo: Remove this block after the Git plugin has been exhaustively tested (also remember to change the Git plugin's "IsBetaVersion" setting to false.)
	if(InName == "Git" )
	{
		return LOCTEXT( "GitBetaProviderName", "Git  (beta version)" );
	}

	return FText::FromName(InName);
}
#undef LOCTEXT_NAMESPACE

#endif // SOURCE_CONTROL_WITH_SLATE
