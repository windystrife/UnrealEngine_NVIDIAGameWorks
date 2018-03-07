// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SLocalizationTargetEditor.h"
#include "Modules/ModuleManager.h"
#include "IDetailsView.h"
#include "LocalizationTargetTypes.h"
#include "PropertyEditorModule.h"

void SLocalizationTargetEditor::Construct(const FArguments& InArgs, ULocalizationTargetSet* const InProjectSettings, ULocalizationTarget* const InLocalizationTarget, const FIsPropertyEditingEnabled& IsPropertyEditingEnabled)
{
	check(InProjectSettings->TargetObjects.Contains(InLocalizationTarget));

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs(false, false, false, FDetailsViewArgs::ENameAreaSettings::HideNameArea, false, nullptr, false, NAME_None);
	TSharedRef<IDetailsView> DetailsView = PropertyModule.CreateDetailView(DetailsViewArgs);
	DetailsView->SetIsPropertyEditingEnabledDelegate(IsPropertyEditingEnabled);

	ChildSlot
		[
			DetailsView
		];

	DetailsView->SetObject(InLocalizationTarget, true);
}
