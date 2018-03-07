// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SProjectLauncherProjectPage.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#include "Widgets/Project/SProjectLauncherProjectPicker.h"


#define LOCTEXT_NAMESPACE "SProjectLauncherProjectPage"


/* SProjectLauncherProjectPage interface
 *****************************************************************************/

void SProjectLauncherProjectPage::Construct(const FArguments& InArgs, const TSharedRef<FProjectLauncherModel>& InModel, bool InShowConfig)
{
	Model = InModel;

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.0)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("WhichProjectToUseText", "Which project would you like to use?"))
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0, 8.0, 0.0, 0.0)
		[
			// project loading area
			SNew(SProjectLauncherProjectPicker, InModel)
			.LaunchProfile(InArgs._LaunchProfile)
		]
	];
}


#undef LOCTEXT_NAMESPACE
