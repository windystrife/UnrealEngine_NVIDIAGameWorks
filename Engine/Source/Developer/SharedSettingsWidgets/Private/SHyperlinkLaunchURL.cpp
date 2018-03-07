// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SHyperlinkLaunchURL.h"
#include "HAL/PlatformProcess.h"
#include "EditorStyleSet.h"
#include "Widgets/Input/SHyperlink.h"

#define LOCTEXT_NAMESPACE "SHyperlinkLaunchURL"

/////////////////////////////////////////////////////
// SHyperlinkLaunchURL

void SHyperlinkLaunchURL::Construct(const FArguments& InArgs, const FString& InDestinationURL)
{
	DestinationURL = InDestinationURL;

	TAttribute<FText> DisplayText;
	if (InArgs._Text.IsBound() || !InArgs._Text.Get().IsEmpty())
	{
		DisplayText = InArgs._Text;
	}
	else
	{
		DisplayText = FText::FromString(DestinationURL);
	}

	ChildSlot
	[
		SNew(SHyperlink)
		.Style(FEditorStyle::Get(), TEXT("NavigationHyperlink"))
		.Text(DisplayText)
		.ToolTipText(InArgs._ToolTipText)
		.OnNavigate(this, &SHyperlinkLaunchURL::OnNavigate)
	];
}

void SHyperlinkLaunchURL::OnNavigate()
{
	FPlatformProcess::LaunchURL(*DestinationURL, NULL, NULL);
}

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
