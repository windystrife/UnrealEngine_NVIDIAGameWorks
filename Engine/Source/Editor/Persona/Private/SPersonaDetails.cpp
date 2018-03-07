// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SPersonaDetails.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"

void SPersonaDetails::Construct(const FArguments& InArgs)
{
	FDetailsViewArgs DetailsViewArgs(false, false, true, FDetailsViewArgs::HideNameArea, true);

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

	TSharedRef<SVerticalBox> Content = SNew(SVerticalBox);

	if (InArgs._TopContent.IsValid())
	{
		Content->AddSlot()
		.AutoHeight()
		[
			InArgs._TopContent.ToSharedRef()
		];
	}

	Content->AddSlot()
	.FillHeight(1.0f)
	[
		DetailsView.ToSharedRef()
	];

	if (InArgs._BottomContent.IsValid())
	{
		Content->AddSlot()
		.AutoHeight()
		[
			InArgs._BottomContent.ToSharedRef()
		];
	}

	ChildSlot
	[
		Content
	];
}
