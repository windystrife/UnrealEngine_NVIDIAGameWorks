// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Kismet2/StructureEditorUtils.h"
#include "DataTableEditorUtils.h"
#include "Misc/NotifyHook.h"
#include "Widgets/Input/SComboBox.h"

class IStructureDetailsView;
class SEditableTextBox;
class FStructOnScope;

DECLARE_DELEGATE_OneParam(FOnRowModified, FName /*Row name*/);
DECLARE_DELEGATE_OneParam(FOnRowSelected, FName /*Row name*/);

class SRowEditor : public SCompoundWidget
	, public FNotifyHook
	, public FStructureEditorUtils::INotifyOnStructChanged
	, public FDataTableEditorUtils::INotifyOnDataTableChanged
{
public:
	SLATE_BEGIN_ARGS(SRowEditor) {}
	SLATE_END_ARGS()

	SRowEditor();
	virtual ~SRowEditor();

	// FNotifyHook
	virtual void NotifyPreChange( UProperty* PropertyAboutToChange ) override;
	virtual void NotifyPostChange( const FPropertyChangedEvent& PropertyChangedEvent, UProperty* PropertyThatChanged ) override;

	// INotifyOnStructChanged
	virtual void PreChange(const class UUserDefinedStruct* Struct, FStructureEditorUtils::EStructureEditorChangeInfo Info) override;
	virtual void PostChange(const class UUserDefinedStruct* Struct, FStructureEditorUtils::EStructureEditorChangeInfo Info) override;

	// INotifyOnDataTableChanged
	virtual void PreChange(const UDataTable* Changed, FDataTableEditorUtils::EDataTableChangeInfo Info) override;
	virtual void PostChange(const UDataTable* Changed, FDataTableEditorUtils::EDataTableChangeInfo Info) override;

	FOnRowSelected RowSelectedCallback;

private:

	TArray<TSharedPtr<FName>> CachedRowNames;
	TSharedPtr<FStructOnScope> CurrentRow;
	TSoftObjectPtr<UDataTable> DataTable; // weak obj ptr couldn't handle reimporting
	TSharedPtr<class IStructureDetailsView> StructureDetailsView;
	TSharedPtr<FName> SelectedName;
	TSharedPtr<SComboBox<TSharedPtr<FName>>> RowComboBox;
	TSharedPtr<SEditableTextBox> RenameTextBox;

	void RefreshNameList();
	void CleanBeforeChange();
	void Restore();

	UScriptStruct* GetScriptStruct() const;

	FName GetCurrentName() const;
	FText GetCurrentNameAsText() const;
	FString GetStructureDisplayName() const;
	TSharedRef<SWidget> OnGenerateWidget(TSharedPtr<FName> InItem);
	void OnSelectionChanged(TSharedPtr<FName> InItem, ESelectInfo::Type InSeletionInfo);

	FReply OnAddClicked();
	FReply OnRemoveClicked();
	FReply OnMoveRowClicked(FDataTableEditorUtils::ERowMoveDirection MoveDirection);
	FReply OnMoveToExtentClicked(FDataTableEditorUtils::ERowMoveDirection MoveDirection);
	void OnRowRenamed(const FText& Text, ETextCommit::Type CommitType);
	FReply OnResetToDefaultClicked();
	EVisibility GetResetToDefaultVisibility()const ;

public:

	void Construct(const FArguments& InArgs, UDataTable* Changed);

	void SelectRow(FName Name);

	void HandleUndoRedo();
};
