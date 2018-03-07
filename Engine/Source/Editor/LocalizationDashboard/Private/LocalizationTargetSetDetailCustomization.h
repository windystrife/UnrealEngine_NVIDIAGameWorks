// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "IDetailCustomization.h"

class IDetailLayoutBuilder;
class IPropertyHandle;
class ULocalizationTarget;
class ULocalizationTargetSet;

class FLocalizationTargetSetDetailCustomization : public IDetailCustomization
{
public:
	FLocalizationTargetSetDetailCustomization();
	void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	void BuildTargetsList();
	void RebuildTargetsList();

	bool CanGatherTextAllTargets() const;
	void GatherTextAllTargets();

	bool CanImportTextAllTargets() const;
	void ImportTextAllTargets();

	bool CanExportTextAllTargets() const;
	void ExportTextAllTargets();

	bool CanImportDialogueScriptAllTargets() const;
	void ImportDialogueScriptAllTargets();

	bool CanExportDialogueScriptAllTargets() const;
	void ExportDialogueScriptAllTargets();

	bool CanImportDialogueAllTargets() const;
	void ImportDialogueAllTargets();

	bool CanCountWordsForAllTargets() const;
	void CountWordsForAllTargets();

	bool CanCompileTextAllTargets() const;
	void CompileTextAllTargets();

	void UpdateTargetFromReports(ULocalizationTarget* const LocalizationTarget);

	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<IPropertyHandle> TargetObjectPropertyHandle, const TSharedRef<STableViewBase>& Table);
	FReply OnNewTargetButtonClicked();

private:
	IDetailLayoutBuilder* DetailLayoutBuilder;

	TWeakObjectPtr<ULocalizationTargetSet> TargetSet;

	TSharedPtr<IPropertyHandle> TargetObjectsPropertyHandle;
	FSimpleDelegate TargetsArrayPropertyHandle_OnNumElementsChanged;
	TArray< TSharedPtr<IPropertyHandle> > TargetsList;
	TSharedPtr< SListView< TSharedPtr<IPropertyHandle> > > TargetsListView;

	/* If set, the entry at the index specified needs to be initialized as soon as possible. */
	int32 NewEntryIndexToBeInitialized;
};
