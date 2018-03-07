// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Widgets/SCompoundWidget.h"
#include "Framework/Commands/UICommandInfo.h"
#include "Framework/MultiBox/MultiBox.h"

class SCustomToolbarPreviewWidget : public SMultiBlockBaseWidget
{
public:
	SLATE_BEGIN_ARGS( SCustomToolbarPreviewWidget ) {}
		SLATE_DEFAULT_SLOT( FArguments, Content )
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs );

	virtual void BuildMultiBlockWidget(const ISlateStyle* StyleSet, const FName& StyleName) override;
private:
	TSharedPtr<SWidget> Content;
};

class SMultiBlockDragHandle : public SCompoundWidget
{
	SLATE_BEGIN_ARGS( SMultiBlockDragHandle ) {}
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, TSharedRef<SMultiBoxWidget> InBaseWidget, TSharedRef<const FMultiBlock> InBlock, FName CustomizationName );

	FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;

	virtual void OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	virtual FReply OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	virtual FReply OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	virtual FReply OnDragDetected( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;

private:
	TSharedPtr<const FMultiBlock> Block;
	TWeakPtr<SMultiBoxWidget> BaseWidget;
	FName MultiBoxCustomizationName;
};


/**
 * Arbitrary Widget MultiBlock
 */
class FDropPreviewBlock
	: public FMultiBlock
{

public:

	FDropPreviewBlock( TSharedRef<const FMultiBlock> InActualBlock, TSharedRef<IMultiBlockBaseWidget> InActualWidget ) 
		: FMultiBlock( NULL, NULL )
		, ActualBlock( InActualBlock )
		, ActualWidget( InActualWidget )
	{
	}

	/** FMultiBlock interface */
	virtual TSharedRef< class IMultiBlockBaseWidget > ConstructWidget() const override;
	virtual bool HasIcon() const override { return GetActualBlock()->HasIcon(); }

	TSharedRef<const FMultiBlock> GetActualBlock() const { return ActualBlock.ToSharedRef(); }

private:
	TSharedPtr<const FMultiBlock> ActualBlock;
	TSharedPtr<IMultiBlockBaseWidget> ActualWidget;

};

struct FCustomBlockTransaction
{
	enum ETransactionType
	{
		Remove,
		Add
	};

	TWeakPtr<const FUICommandInfo> Command;
	int32 BlockIndex;
	ETransactionType TransactionType;

	bool operator==(const FCustomBlockTransaction& Other ) const
	{
		return Command == Other.Command && BlockIndex == Other.BlockIndex && TransactionType == Other.TransactionType;
	}

	static FCustomBlockTransaction CreateRemove( TSharedRef<const FUICommandInfo> InCommand, int32 Index )
	{
		FCustomBlockTransaction Transaction;
		Transaction.Command = InCommand;
		Transaction.BlockIndex = Index;
		Transaction.TransactionType = ETransactionType::Remove;

		return Transaction;
	}

	static FCustomBlockTransaction CreateAdd( TSharedRef<const FUICommandInfo> InCommand, int32 Index )
	{
		FCustomBlockTransaction Transaction;
		Transaction.Command = InCommand;
		Transaction.BlockIndex = Index;
		Transaction.TransactionType = ETransactionType::Add;

		return Transaction;
	}
};

class FMultiBoxCustomizationData
{
public:
	FMultiBoxCustomizationData( FName InCustomizationName )
		: CustomizationName( InCustomizationName )
	{}

	FName GetCustomizationName() const { return CustomizationName; }

	void LoadCustomizedBlocks();

	void SaveCustomizedBlocks();
	void BlockRemoved( TSharedRef< const FMultiBlock> RemovedBlock, int32 Index, const TArray< TSharedRef< const FMultiBlock > >& AllBlocks );
	void BlockAdded( TSharedRef< const FMultiBlock > AddedBlock, int32 Index, const TArray< TSharedRef< const FMultiBlock > >& AllBlocks );
	void SaveTransaction( const struct FCustomBlockTransaction& Transaction, const TArray< TSharedRef< const FMultiBlock > >& AllBlocks );

	uint32 GetNumTransactions() const { return Transactions.Num(); }

	const FCustomBlockTransaction& GetTransaction( uint32 TransactionIndex ) { return Transactions[TransactionIndex]; }
private:
	FString GetConfigSectionName() const;
	void RemoveTransactionAt(int32 TransIndex);
	bool RemoveDuplicateTransaction();
	bool RemoveUnnecessaryTransactions(const TArray< TSharedRef< const FMultiBlock > >& AllBlocks);
private:
	TArray<FCustomBlockTransaction> Transactions;
	FName CustomizationName;
};
