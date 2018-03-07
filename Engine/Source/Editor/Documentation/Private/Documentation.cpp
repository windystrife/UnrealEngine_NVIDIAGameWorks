// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Documentation.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SToolTip.h"
#include "Misc/MessageDialog.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Dialogs/Dialogs.h"
#include "SDocumentationAnchor.h"
#include "UDNParser.h"
#include "DocumentationPage.h"
#include "DocumentationLink.h"
#include "SDocumentationToolTip.h"
#include "Interfaces/IAnalyticsProvider.h"
#include "EngineAnalytics.h"

#define LOCTEXT_NAMESPACE "DocumentationActor"

TSharedRef< IDocumentation > FDocumentation::Create() 
{
	return MakeShareable( new FDocumentation() );
}

FDocumentation::FDocumentation() 
{

}

FDocumentation::~FDocumentation() 
{

}

bool FDocumentation::OpenHome(FDocumentationSourceInfo Source) const
{
	return Open( TEXT("%ROOT%"), Source );
}

bool FDocumentation::OpenHome(const FCultureRef& Culture, FDocumentationSourceInfo Source) const
{
	return Open(TEXT("%ROOT%"), Culture, Source);
}

bool FDocumentation::OpenAPIHome(FDocumentationSourceInfo Source) const
{
	FString Url;
	FUnrealEdMisc::Get().GetURL(TEXT("APIDocsURL"), Url, true);

	if (!Url.IsEmpty())
	{
		Url.ReplaceInline(TEXT("/INT/"), *FString::Printf(TEXT("/%s/"), *(FInternationalization::Get().GetCurrentCulture()->GetUnrealLegacyThreeLetterISOLanguageName())));

		FPlatformProcess::LaunchURL(*Url, nullptr, nullptr);

		return true;
	}
	return false;
}

bool FDocumentation::Open(const FString& Link, FDocumentationSourceInfo Source) const
{
	FString DocumentationUrl;

	// Warn the user if they are opening a URL
	if (Link.StartsWith(TEXT("http")) || Link.StartsWith(TEXT("https")))
	{
		FText Message = LOCTEXT("OpeningURLMessage", "You are about to open an external URL. This will open your web browser. Do you want to proceed?");
		FText URLDialog = LOCTEXT("OpeningURLTitle", "Open external link");

		FSuppressableWarningDialog::FSetupInfo Info(Message, URLDialog, "SuppressOpenURLWarning");
		Info.ConfirmText = LOCTEXT("OpenURL_yes", "Yes");
		Info.CancelText = LOCTEXT("OpenURL_no", "No");
		FSuppressableWarningDialog OpenURLWarning(Info);
		if (OpenURLWarning.ShowModal() == FSuppressableWarningDialog::Cancel)
		{
			return false;
		}
		else
		{
			FPlatformProcess::LaunchURL(*Link, nullptr, nullptr);
			return true;
		}
	}

	if (!FParse::Param(FCommandLine::Get(), TEXT("testdocs")))
	{
		FString OnDiskPath = FDocumentationLink::ToFilePath(Link);
		if (IFileManager::Get().FileSize(*OnDiskPath) != INDEX_NONE)
		{
			DocumentationUrl = FDocumentationLink::ToFileUrl(Link, Source);
		}
	}

	
	if (DocumentationUrl.IsEmpty())
	{
		// When opening a doc website we always request the most ideal culture for our documentation.
		// The DNS will redirect us if necessary.
		DocumentationUrl = FDocumentationLink::ToUrl(Link, Source);
	}

	if (!DocumentationUrl.IsEmpty())
	{
		FPlatformProcess::LaunchURL(*DocumentationUrl, NULL, NULL);
	}

	if (!DocumentationUrl.IsEmpty() && FEngineAnalytics::IsAvailable())
	{
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.Documentation"), TEXT("OpenedPage"), Link);
	}

	return !DocumentationUrl.IsEmpty();
}

bool FDocumentation::Open(const FString& Link, const FCultureRef& Culture, FDocumentationSourceInfo Source) const
{
	FString DocumentationUrl;

	if (!FParse::Param(FCommandLine::Get(), TEXT("testdocs")))
	{
		FString OnDiskPath = FDocumentationLink::ToFilePath(Link, Culture);
		if (IFileManager::Get().FileSize(*OnDiskPath) != INDEX_NONE)
		{
			DocumentationUrl = FDocumentationLink::ToFileUrl(Link, Culture, Source);
		}
	}

	if (DocumentationUrl.IsEmpty())
	{
		DocumentationUrl = FDocumentationLink::ToUrl(Link, Culture, Source);
	}

	if (!DocumentationUrl.IsEmpty())
	{
		FPlatformProcess::LaunchURL(*DocumentationUrl, NULL, NULL);
	}

	if (!DocumentationUrl.IsEmpty() && FEngineAnalytics::IsAvailable())
	{
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.Documentation"), TEXT("OpenedPage"), Link);
	}

	return !DocumentationUrl.IsEmpty();
}

TSharedRef< SWidget > FDocumentation::CreateAnchor( const TAttribute<FString>& Link, const FString& PreviewLink, const FString& PreviewExcerptName ) const
{
	return SNew( SDocumentationAnchor )
		.Link(Link)
		.PreviewLink(PreviewLink)
		.PreviewExcerptName(PreviewExcerptName);
}

TSharedRef< IDocumentationPage > FDocumentation::GetPage( const FString& Link, const TSharedPtr< FParserConfiguration >& Config, const FDocumentationStyle& Style )
{
	TSharedPtr< IDocumentationPage > Page;
	const TWeakPtr< IDocumentationPage >* ExistingPagePtr = LoadedPages.Find( Link );

	if ( ExistingPagePtr != NULL )
	{
		const TSharedPtr< IDocumentationPage > ExistingPage = ExistingPagePtr->Pin();
		if ( ExistingPage.IsValid() )
		{
			Page = ExistingPage;
		}
	}

	if ( !Page.IsValid() )
	{
		Page = FDocumentationPage::Create( Link, FUDNParser::Create( Config, Style ) );
		LoadedPages.Add( Link, TWeakPtr< IDocumentationPage >( Page ) );
	}

	return Page.ToSharedRef();
}

bool FDocumentation::PageExists(const FString& Link) const
{
	const TWeakPtr< IDocumentationPage >* ExistingPagePtr = LoadedPages.Find(Link);
	if (ExistingPagePtr != NULL)
	{
		return true;
	}

	const FString SourcePath = FDocumentationLink::ToSourcePath(Link);
	return FPaths::FileExists(SourcePath);
}

bool FDocumentation::PageExists(const FString& Link, const FCultureRef& Culture) const
{
	const TWeakPtr< IDocumentationPage >* ExistingPagePtr = LoadedPages.Find(Link);
	if (ExistingPagePtr != NULL)
	{
		return true;
	}

	const FString SourcePath = FDocumentationLink::ToSourcePath(Link, Culture);
	return FPaths::FileExists(SourcePath);
}

TSharedRef< class SToolTip > FDocumentation::CreateToolTip(const TAttribute<FText>& Text, const TSharedPtr<SWidget>& OverrideContent, const FString& Link, const FString& ExcerptName) const
{
	TSharedPtr< SDocumentationToolTip > DocToolTip;

	if ( !Text.IsBound() && Text.Get().IsEmpty() )
	{
		return SNew( SToolTip );
	}

	if ( OverrideContent.IsValid() )
	{
		SAssignNew( DocToolTip, SDocumentationToolTip )
		.DocumentationLink( Link )
		.ExcerptName( ExcerptName )
		[
			OverrideContent.ToSharedRef()
		];
	}
	else
	{
		SAssignNew( DocToolTip, SDocumentationToolTip )
		.Text( Text )
		.DocumentationLink( Link )
		.ExcerptName( ExcerptName );
	}
	
	return SNew( SToolTip )
		.IsInteractive( DocToolTip.ToSharedRef(), &SDocumentationToolTip::IsInteractive )

		// Emulate text-only tool-tip styling that SToolTip uses when no custom content is supplied.  We want documentation tool-tips to 
		// be styled just like text-only tool-tips
		.BorderImage( FCoreStyle::Get().GetBrush("ToolTip.BrightBackground") )
		.TextMargin(FMargin(11.0f))
		[
			DocToolTip.ToSharedRef()
		];
}

TSharedRef< class SToolTip > FDocumentation::CreateToolTip(const TAttribute<FText>& Text, const TSharedRef<SWidget>& OverrideContent, const TSharedPtr<SVerticalBox>& DocVerticalBox, const FString& Link, const FString& ExcerptName) const
{
	TSharedRef<SDocumentationToolTip> DocToolTip =
		SNew(SDocumentationToolTip)
		.Text(Text)
		.DocumentationLink(Link)
		.ExcerptName(ExcerptName)
		.AddDocumentation(false)
		.DocumentationMargin(7)
		[
			OverrideContent
		];

	if (DocVerticalBox.IsValid())
	{
		DocToolTip->AddDocumentation(DocVerticalBox);
	}

	return SNew(SToolTip)
		.IsInteractive(DocToolTip, &SDocumentationToolTip::IsInteractive)

		// Emulate text-only tool-tip styling that SToolTip uses when no custom content is supplied.  We want documentation tool-tips to 
		// be styled just like text-only tool-tips
		.BorderImage( FCoreStyle::Get().GetBrush("ToolTip.BrightBackground") )
		.TextMargin(FMargin(11.0f))
		[
			DocToolTip
		];
}

#undef LOCTEXT_NAMESPACE
