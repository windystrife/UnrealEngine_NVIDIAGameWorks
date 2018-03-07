// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SCrashReportClient.h"

#if !CRASH_REPORT_UNATTENDED_ONLY

#include "CrashReportClientStyle.h"
#include "SlateStyle.h"
#include "SThrobber.h"
#include "CrashDescription.h"
#include "Framework/Text/SlateHyperlinkRun.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SSpacer.h"
#include "Framework/Application/SlateApplication.h"

#define LOCTEXT_NAMESPACE "CrashReportClient"

static void OnBrowserLinkClicked(const FSlateHyperlinkRun::FMetadata& Metadata, TSharedRef<SWidget> ParentWidget)
{
	const FString* UrlPtr = Metadata.Find(TEXT("href"));
	if(UrlPtr)
	{
		FPlatformProcess::LaunchURL(**UrlPtr, nullptr, nullptr);
	}
}

static void OnViewCrashDirectory( const FSlateHyperlinkRun::FMetadata& Metadata, TSharedRef<SWidget> ParentWidget )
{
	const FString* UrlPtr = Metadata.Find( TEXT( "href" ) );
	if (UrlPtr)
	{
		FPlatformProcess::ExploreFolder( **UrlPtr );
	}
}

void SCrashReportClient::Construct(const FArguments& InArgs, TSharedRef<FCrashReportClient> Client)
{
	CrashReportClient = Client;
	bHasUserCommentErrors = false;

	auto CrashedAppName = FPrimaryCrashProperties::Get()->IsValid() ? FPrimaryCrashProperties::Get()->GameName : TEXT("");
	FText CrashDetailedMessage = LOCTEXT("CrashDetailed", "We are very sorry that this crash occurred. Our goal is to prevent crashes like this from occurring in the future. Please help us track down and fix this crash by providing detailed information about what you were doing so that we may reproduce the crash and fix it quickly. You can also log a Bug Report with us at <a id=\"browser\" href=\"https://answers.unrealengine.com\" style=\"Hyperlink\">AnswerHub</> and work directly with support staff to report this issue.\n\nThanks for your help in improving the Unreal Engine.");
	if (FPrimaryCrashProperties::Get()->IsValid())
	{
		FString CrashDetailedMessageString = FPrimaryCrashProperties::Get()->CrashReporterMessage.AsString();
		if (!CrashDetailedMessageString.IsEmpty())
		{
			CrashDetailedMessage = FText::FromString(CrashDetailedMessageString);
		}
	}

	// Set the text displaying the name of the crashed app, if available
	const FText CrashedAppText = CrashedAppName.IsEmpty() ?
		LOCTEXT( "CrashedAppNotFound", "An unknown process has crashed" ) :
		LOCTEXT( "CrashedAppUnreal", "An Unreal process has crashed: " );

	const FText CrashReportDataText = FText::Format( LOCTEXT(
		"CrashReportData",
		"Crash reports comprise diagnostics files (<a id=\"browser\" href=\"{0}\" style=\"Richtext.Hyperlink\">click here to view directory</>) and the following summary information: " ),
		FText::FromString( CrashReportClient->GetCrashDirectory()) );

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SVerticalBox)

			// Stuff anchored to the top
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.TextStyle(FCrashReportClientStyle::Get(), "Title")
					.Text(CrashedAppText)
				]

				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.TextStyle(FCrashReportClientStyle::Get(), "Title")
					.Text(FText::FromString(CrashedAppName))
				]
			]

			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding( FMargin( 4, 10 ) )
			[
				SNew( SRichTextBlock )
				.Text(CrashDetailedMessage)
				.AutoWrapText(true)
				.DecoratorStyleSet( &FCoreStyle::Get() )
				+ SRichTextBlock::HyperlinkDecorator( TEXT("browser"), FSlateHyperlinkRun::FOnClick::CreateStatic( &OnBrowserLinkClicked, AsShared() ) )
			]

			+SVerticalBox::Slot()
			.Padding(FMargin(4, 10, 4, 4))
			[
				SNew(SSplitter)
				.Orientation(Orient_Vertical)
				+SSplitter::Slot()
				.Value(0.3f)
				[
					SNew( SOverlay )
					+ SOverlay::Slot()
					[
						SAssignNew( CrashDetailsInformation, SMultiLineEditableTextBox )
						.Style( &FCrashReportClientStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>( "NormalEditableTextBox" ) )
						.OnTextCommitted( CrashReportClient.ToSharedRef(), &FCrashReportClient::UserCommentChanged )
						.OnTextChanged( this, &SCrashReportClient::OnUserCommentTextChanged)
						.Font( FSlateFontInfo( FPaths::EngineContentDir() / TEXT( "Slate/Fonts/Roboto-Regular.ttf" ), 9 ) )
						.AutoWrapText( true )
						.BackgroundColor( FSlateColor( FLinearColor::Black ) )
						.ForegroundColor( FSlateColor( FLinearColor::White * 0.8f ) )
					]

					// HintText is not implemented in SMultiLineEditableTextBox, so this is a workaround.
					+ SOverlay::Slot()
					[
						SNew(STextBlock)
						.Margin( FMargin(4,2,0,0) )
						.Font( FSlateFontInfo( FPaths::EngineContentDir() / TEXT( "Slate/Testing/Fonts/Roboto-Italic.ttf" ), 9 ) )
						.ColorAndOpacity( FSlateColor( FLinearColor::White * 0.5f ) )
						.Text( LOCTEXT( "CrashProvide", "Please provide detailed information about what you were doing when the crash occurred." ) )
						.Visibility( this, &SCrashReportClient::IsHintTextVisible )
					]	
				]

				+SSplitter::Slot()
				.Value(0.7f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SOverlay)

						+ SOverlay::Slot()			
						[
							SNew(SColorBlock)
							.Color(FLinearColor::Black)
						]

						+ SOverlay::Slot()
						[
							SNew( SRichTextBlock )
							.Margin( FMargin( 4, 2, 0, 8 ) )
							.TextStyle( &FCrashReportClientStyle::Get().GetWidgetStyle<FTextBlockStyle>( "CrashReportDataStyle" ) )
							.Text( CrashReportDataText )
							.AutoWrapText( true )
							.DecoratorStyleSet( &FCrashReportClientStyle::Get() )
							+ SRichTextBlock::HyperlinkDecorator( TEXT( "browser" ), FSlateHyperlinkRun::FOnClick::CreateStatic( &OnViewCrashDirectory, AsShared() ) )
						]
					]

					+ SVerticalBox::Slot()
					.FillHeight(0.7f)
					[
						SNew(SOverlay)
		
						+ SOverlay::Slot()
						[
							SNew( SMultiLineEditableTextBox )
							.Style( &FCrashReportClientStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>( "NormalEditableTextBox" ) )
							.Font( FSlateFontInfo( FPaths::EngineContentDir() / TEXT( "Slate/Fonts/Roboto-Regular.ttf" ), 8 ) )
							.AutoWrapText( false )
							.IsReadOnly( true )
							.ReadOnlyForegroundColor( FSlateColor( FLinearColor::White * 0.8f) )
							.BackgroundColor( FSlateColor( FLinearColor::Black ) )
							.ForegroundColor( FSlateColor( FLinearColor::White * 0.8f ) )
							.Text( Client, &FCrashReportClient::GetDiagnosticText )
						]


						+ SOverlay::Slot()
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SNew(SThrobber)
							.Visibility(CrashReportClient.ToSharedRef(), &FCrashReportClient::IsThrobberVisible)
							.NumPieces(5)
						]
					]
				]
			]

			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding( FMargin( 4, 12, 4, 4 ) )
			[
				SNew( SHorizontalBox )
				.Visibility( FCrashReportClientConfig::Get().GetHideLogFilesOption() ? EVisibility::Collapsed : EVisibility::Visible )
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign( VAlign_Center )
				[
					SNew( SCheckBox )
					.IsChecked( FCrashReportClientConfig::Get().GetSendLogFile() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked )
					.OnCheckStateChanged( CrashReportClient.ToSharedRef(), &FCrashReportClient::SendLogFile_OnCheckStateChanged )
				]

				+ SHorizontalBox::Slot()
				.FillWidth( 1.0f )
				.VAlign( VAlign_Center )
				[
					SNew( STextBlock )
					.AutoWrapText( true )
					.Text( LOCTEXT( "IncludeLogs", "Include log files with submission. I understand that logs contain some personal information such as my system and user name." ) )
				]
			]

			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding( FMargin( 4, 4 ) )
			[
				SNew(SHorizontalBox)

				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SCheckBox)
					.IsChecked( FCrashReportClientConfig::Get().GetAllowToBeContacted() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked )
					.OnCheckStateChanged(CrashReportClient.ToSharedRef(), &FCrashReportClient::AllowToBeContacted_OnCheckStateChanged)
				]

				
				+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Text(LOCTEXT("IAgree", "I agree to be contacted by Epic Games via email if additional information about this crash would help fix it."))
				]
			]

			// Stuff anchored to the bottom
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding( FMargin(4, 4+16, 4, 4) )
			[
				SNew(SHorizontalBox)
	
				+ SHorizontalBox::Slot()
				.HAlign( HAlign_Center )
				.VAlign( VAlign_Center )
				.AutoWidth()
				.Padding( FMargin( 0 ) )
				[
					SNew( SButton )
					.ContentPadding( FMargin( 8, 2 ) )
					.Text( LOCTEXT( "CloseWithoutSending", "Close Without Sending" ) )
					.OnClicked( Client, &FCrashReportClient::CloseWithoutSending )
					.Visibility(FCrashReportClientConfig::Get().IsAllowedToCloseWithoutSending() ? EVisibility::Visible : EVisibility::Hidden)
				]

				+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.Padding(0)
				[			
					SNew(SSpacer)
				]

				+SHorizontalBox::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.AutoWidth()
				.Padding( FMargin(6) )
				[
					SNew(SButton)
					.ContentPadding( FMargin(8,2) )
					.Text(LOCTEXT("Send", "Send and Close"))
					.OnClicked(Client, &FCrashReportClient::Submit)
					.IsEnabled(this, &SCrashReportClient::IsSendEnabled)
				]

				+SHorizontalBox::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.AutoWidth()
				.Padding( FMargin(0) )
				[
					SNew(SButton)
					.ContentPadding( FMargin(8,2) )
					.Text(LOCTEXT("SendAndRestartEditor", "Send and Restart"))
					.OnClicked(Client, &FCrashReportClient::SubmitAndRestart)
					.IsEnabled(this, &SCrashReportClient::IsSendEnabled)
				]			
			]
		]
	];

	FSlateApplication::Get().SetUnhandledKeyDownEventHandler(FOnKeyEvent::CreateSP(this, &SCrashReportClient::OnUnhandledKeyDown));
}

FReply SCrashReportClient::OnUnhandledKeyDown(const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Enter)
	{
		CrashReportClient->Submit();
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SCrashReportClient::OnUserCommentTextChanged(const FText& NewText)
{
	FText ErrorMessage = FText::GetEmpty();
	bHasUserCommentErrors = false;

	int SizeLimit = FCrashReportClientConfig::Get().GetUserCommentSizeLimit();
	int Size = NewText.ToString().Len();
	if (Size > SizeLimit)
	{
		bHasUserCommentErrors = true;
		ErrorMessage = FText::Format(LOCTEXT("UserCommentTooLongError", "Description may only be a maximum of {0} characters (currently {1})"), SizeLimit, Size);
	}

	CrashDetailsInformation->SetError(ErrorMessage);
}

EVisibility SCrashReportClient::IsHintTextVisible() const
{
	return CrashDetailsInformation->GetText().IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Hidden;
}

bool SCrashReportClient::IsSendEnabled() const
{
	bool bValidAppName = FPrimaryCrashProperties::Get()->IsValid() && !FPrimaryCrashProperties::Get()->GameName.IsEmpty();

	return bValidAppName && !bHasUserCommentErrors;
}

#undef LOCTEXT_NAMESPACE

#endif // !CRASH_REPORT_UNATTENDED_ONLY
