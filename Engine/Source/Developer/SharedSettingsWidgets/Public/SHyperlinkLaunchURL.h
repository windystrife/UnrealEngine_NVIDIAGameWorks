// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

/////////////////////////////////////////////////////
// SHyperlinkLaunchURL

// This widget is a hyperlink that launches an external URL when clicked

class SHAREDSETTINGSWIDGETS_API SHyperlinkLaunchURL : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SHyperlinkLaunchURL)
		{}

		/** If set, this text will be used for the display string of the hyperlink */
		SLATE_ATTRIBUTE(FText, Text)

	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, const FString& InDestinationURL);

protected:
	void OnNavigate();

protected:
	FString DestinationURL;
};
