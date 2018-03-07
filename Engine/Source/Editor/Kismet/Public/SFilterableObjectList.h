// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "Layout/Visibility.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"

//////////////////////////////////////////////////////////////////////////
// SFilterableObjectList

class KISMET_API SFilterableObjectList : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SFilterableObjectList ){}
	SLATE_END_ARGS()
public:
	void Construct(const FArguments& InArgs);

	void InternalConstruct();

protected:
	// Interface for derived classes to implement
	virtual void RebuildObjectList();
	virtual FString GetSearchableText(UObject* Object);
	
	/** Return value for GenerateRowForObject */
	struct FListRow
	{
		FListRow( const TSharedRef<SWidget>& InWidget, const FOnDragDetected& OnDragDetected )
		: Widget(InWidget)
		, OnDragDetected_Handler( OnDragDetected )
		{
		}

		/** The widget to place into the table row. */
		TSharedRef<SWidget> Widget;
		/** The Delegate to invoke when the user started dragging a row. */
		FOnDragDetected OnDragDetected_Handler;
	};

	/** Make a table row for the list of filterable widgets */
	virtual FListRow GenerateRowForObject(UObject* Object);
	

	// End of interface for derived classes to implement
protected:
	void RefilterObjectList();

	TSharedRef<ITableRow> OnGenerateTableRow(UObject* InData, const TSharedRef<STableViewBase>& OwnerTable);

	FReply OnRefreshButtonClicked();

	void OnFilterTextChanged(const FText& InFilterText);

	EVisibility GetFilterStatusVisibility() const;
	FText GetFilterStatusText() const;

	bool IsFilterActive() const;
	void ReapplyFilter();
protected:
	/** Widget containing the object list */
	TSharedPtr< SListView<UObject*> > ObjectListWidget;

	/* Widget containing the filtering text box */
	TSharedPtr< SSearchBox > FilterTextBoxWidget;

	/** List of objects that can be shown */
	TArray<UObject*> LoadedObjectList;

	/** List of objects to show that have passed the keyword filtering */
	TArray<UObject*> FilteredObjectList;
};
