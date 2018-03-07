// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"

class FTransaction;

/**
 * Implements the undo history panel.
 */
class SUndoHistory
	: public SCompoundWidget
{
	// Structure for transaction information.
	struct FTransactionInfo
	{
		// Holds the transactions index in the transaction queue.
		int32 QueueIndex;

		// Holds a pointer to the transaction.
		const FTransaction* Transaction;

		// Creates and initializes a new instance.
		FTransactionInfo( int32 InQueueIndex, const FTransaction* InTransaction )
			: QueueIndex(InQueueIndex)
			, Transaction(InTransaction)
		{ }
	};

public:

	SLATE_BEGIN_ARGS(SUndoHistory) { }
	SLATE_END_ARGS()

public:

	/**
	 * Construct this widget
	 *
	 * @param InArgs The declaration data for this widget.
	 */
	void Construct( const FArguments& InArgs );

public:

	//~ Begin SWidget Interface

	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;

	//~ End SWidget Interface

protected:

	/**
	 * Reloads the list of undo transactions.
	 */
	void ReloadUndoList( );

private:

	// Callback for clicking the 'Discard History' button.
	FReply HandleDiscardHistoryButtonClicked( );

	// Callback for generating a row widget for the message list view.
	TSharedRef<ITableRow> HandleUndoListGenerateRow( TSharedPtr<FTransactionInfo> TransactionInfo, const TSharedRef<STableViewBase>& OwnerTable );

	// Callback for checking whether the specified undo list row transaction is applied.
	bool HandleUndoListRowIsApplied( int32 QueueIndex ) const;

	// Callback for selecting a message in the message list view.
	void HandleUndoListSelectionChanged( TSharedPtr<FTransactionInfo> TransactionInfo, ESelectInfo::Type SelectInfo );

	// Callback for getting the undo size text.
	FText HandleUndoSizeTextBlockText( ) const;

private:

	// Holds the index of the last active transaction.
	int32 LastActiveTransactionIndex;

	// Holds the number of transactions at the last undo list reload.
	int32 LastQueueLength;

	// Holds the number of undo actions at the last undo list reload.
	int32 LastUndoCount;

	// Holds the list of undo transaction indices.
	TArray<TSharedPtr<FTransactionInfo> > UndoList;

	// Holds the undo list view.
	TSharedPtr<SListView<TSharedPtr<FTransactionInfo> > > UndoListView;
};
