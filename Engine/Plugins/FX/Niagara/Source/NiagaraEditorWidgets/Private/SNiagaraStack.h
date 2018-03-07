// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorUndoClient.h"
#include "SCompoundWidget.h"
#include "DeclarativeSyntaxSupport.h"
#include "STableRow.h"
#include "STableViewBase.h"
#include "STreeView.h"
#include "ViewModels/Stack/NiagaraStackEntry.h"
#include "EdGraph/EdGraphSchema.h"
#include "SNiagaraStack.generated.h"

class UNiagaraStackViewModel;
class SNiagaraStackTableRow;

class SNiagaraStack : public SCompoundWidget, public FEditorUndoClient
{
public:
	SLATE_BEGIN_ARGS(SNiagaraStack)
	{}

	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, UNiagaraStackViewModel* InStackViewModel);

private:
	void PrimeTreeExpansion();

	TSharedRef<ITableRow> OnGenerateRowForStackItem(UNiagaraStackEntry* Item, const TSharedRef<STableViewBase>& OwnerTable);

	TSharedRef<SNiagaraStackTableRow> ConstructDefaultRow(UNiagaraStackEntry* Item);

	TSharedRef<SNiagaraStackTableRow> ConstructContainerForItem(UNiagaraStackEntry* Item);

	TSharedRef<SWidget> ConstructNameWidgetForItem(UNiagaraStackEntry* Item, TSharedRef<SNiagaraStackTableRow> Container);

	TSharedPtr<SWidget> ConstructValueWidgetForItem(UNiagaraStackEntry* Item, TSharedRef<SNiagaraStackTableRow> Container);
	
	void OnGetChildren(UNiagaraStackEntry* Item, TArray<UNiagaraStackEntry*>& Children);

	float GetNameColumnWidth() const;
	float GetContentColumnWidth() const;

	void OnNameColumnWidthChanged(float Width);
	void OnContentColumnWidthChanged(float Width);
	void StackStructureChanged();

	EVisibility GetVisibilityForItem(UNiagaraStackEntry* Item) const;

private:
	UNiagaraStackViewModel* StackViewModel;

	TSharedPtr<STreeView<UNiagaraStackEntry*>> StackTree;

	float NameColumnWidth;

	float ContentColumnWidth;
};

USTRUCT()
struct FNiagaraStackGraphSchemaAction : public FEdGraphSchemaAction
{
	GENERATED_USTRUCT_BODY();

	DECLARE_DELEGATE(FOnPerformStackAction);

	FNiagaraStackGraphSchemaAction()
	{
	}

	FNiagaraStackGraphSchemaAction(FText InNodeCategory, FText InMenuDesc, FText InToolTip, const int32 InGrouping, FText InKeywords, FOnPerformStackAction InAction)
		: FEdGraphSchemaAction(MoveTemp(InNodeCategory), MoveTemp(InMenuDesc), MoveTemp(InToolTip), InGrouping, MoveTemp(InKeywords))
		, Action(InAction)
	{}

	//~ Begin FEdGraphSchemaAction Interface
	UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override
	{
		Action.ExecuteIfBound();
		return nullptr;
	}
	//~ End FEdGraphSchemaAction Interface

	FOnPerformStackAction Action;
};