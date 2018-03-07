// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Framework/MultiBox/MultiBoxCustomization.h"
#include "Misc/ConfigCacheIni.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Framework/Commands/InputBindingManager.h"
#include "Widgets/Layout/SBorder.h"
#include "Misc/RemoteConfigIni.h"
#include "Framework/Commands/UICommandDragDropOp.h"

void SCustomToolbarPreviewWidget::Construct( const FArguments& InArgs )
{
	Content = InArgs._Content.Widget;
}

void SCustomToolbarPreviewWidget::BuildMultiBlockWidget(const ISlateStyle* StyleSet, const FName& StyleName)
{
	ChildSlot
	[
		SNew( SBorder )
		.Padding(0)
		.BorderImage( FCoreStyle::Get().GetBrush("NoBorder") )
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			Content.ToSharedRef()
		]
	];

	// Add this widget to the search list of the multibox and hide it
	if (MultiBlock->GetSearchable())
		OwnerMultiBoxWidget.Pin()->AddSearchElement(this->AsWidget(), FText::GetEmpty());
}

void SMultiBlockDragHandle::Construct( const FArguments& InArgs, TSharedRef<SMultiBoxWidget> InBaseWidget, TSharedRef<const FMultiBlock> InBlock, FName CustomizationName )
{
	BaseWidget = InBaseWidget;
	Block = InBlock;
	MultiBoxCustomizationName = CustomizationName;
}

FReply SMultiBlockDragHandle::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && Block->GetAction().IsValid() )
	{
		return FReply::Handled().DetectDrag( SharedThis(this), MouseEvent.GetEffectingButton() );
	}

	return FReply::Unhandled();
}

void SMultiBlockDragHandle::OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	if ( DragDropEvent.GetOperationAs<FUICommandDragDropOp>().IsValid() )
	{
		BaseWidget.Pin()->OnCustomCommandDragEnter( Block.ToSharedRef(), MyGeometry, DragDropEvent );
	}
}

FReply SMultiBlockDragHandle::OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	if ( DragDropEvent.GetOperationAs<FUICommandDragDropOp>().IsValid() )
	{
		BaseWidget.Pin()->OnCustomCommandDragged( Block.ToSharedRef(), MyGeometry, DragDropEvent );
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply SMultiBlockDragHandle::OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	if ( DragDropEvent.GetOperationAs<FUICommandDragDropOp>().IsValid() )
	{
		BaseWidget.Pin()->OnCustomCommandDropped();
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply SMultiBlockDragHandle::OnDragDetected( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	TSharedRef<FUICommandDragDropOp> NewOp = FUICommandDragDropOp::New(
			Block->GetAction().ToSharedRef(), 
			MultiBoxCustomizationName, 
			Block->MakeWidget( BaseWidget.Pin().ToSharedRef(), EMultiBlockLocation::None, Block->HasIcon() )->AsWidget(),
			MyGeometry.AbsolutePosition-MouseEvent.GetScreenSpacePosition()
		);
			
	NewOp->SetOnDropNotification( FSimpleDelegate::CreateSP( BaseWidget.Pin().ToSharedRef(), &SMultiBoxWidget::OnDropExternal ) );

	TSharedRef<FDragDropOperation> DragDropOp = NewOp;
	return FReply::Handled().BeginDragDrop( DragDropOp );
}

TSharedRef< class IMultiBlockBaseWidget > FDropPreviewBlock::ConstructWidget() const
{
	return
		SNew( SCustomToolbarPreviewWidget )
		.Visibility( EVisibility::Hidden )
		.Content()
		[
			ActualWidget->AsWidget()
		];

}

void FMultiBoxCustomizationData::LoadCustomizedBlocks()
{
	Transactions.Empty();

	FString Content;
	
	GConfig->GetString(*GetConfigSectionName(), *CustomizationName.ToString(), Content, GEditorPerProjectIni);

	TSharedPtr<FJsonObject> SavedData;
	TSharedRef< TJsonReader<> > Reader = TJsonReaderFactory<>::Create( FRemoteConfig::ReplaceIniSpecialCharWithChar(Content).ReplaceEscapedCharWithChar() );
	bool bResult = FJsonSerializer::Deserialize( Reader, SavedData );

	if( bResult && SavedData.IsValid() )
	{
		TArray<TSharedPtr<FJsonValue> > CustomBlockData = SavedData->GetArrayField( TEXT("CustomBlocks") );
		for (TArray<TSharedPtr<FJsonValue> >::TConstIterator It(CustomBlockData); It; ++It)
		{
			TSharedPtr<FJsonObject> SavedCommand = (*It)->AsObject();

			if( SavedCommand.IsValid() )
			{
				FString CommandName = SavedCommand->GetStringField( TEXT("CommandName") );
				FString Context = SavedCommand->GetStringField( TEXT("Context") );
				int32 Index = FMath::TruncToInt( SavedCommand->GetNumberField( TEXT("Index") ) );
				FCustomBlockTransaction::ETransactionType TransType = (FCustomBlockTransaction::ETransactionType)FMath::TruncToInt( SavedCommand->GetNumberField("TransactionType") );

				if( !CommandName.IsEmpty() && !Context.IsEmpty() && Index >= 0 )
				{
					const TSharedPtr< const FUICommandInfo> FoundCommand = FInputBindingManager::Get().FindCommandInContext( FName( *Context ), FName( *CommandName ) );
					if( FoundCommand.IsValid() )
					{
						FCustomBlockTransaction NewTransaction;
						if( TransType == FCustomBlockTransaction::Add )
						{
							NewTransaction = FCustomBlockTransaction::CreateAdd( FoundCommand.ToSharedRef(), Index );
						}
						else
						{
							NewTransaction = FCustomBlockTransaction::CreateRemove( FoundCommand.ToSharedRef(), Index );
						}

						Transactions.Add( NewTransaction );
					}
				}
			}
		}
	}
}

FString FMultiBoxCustomizationData::GetConfigSectionName() const
{
	return TEXT("CustomMultiBoxes1_0");
}

void FMultiBoxCustomizationData::RemoveTransactionAt(int32 RemoveIndex)
{
	// Copy off the transaction we are going to remove.
	FCustomBlockTransaction TransToRemove = Transactions[RemoveIndex];
	const int32 TransToRemoveIndex = TransToRemove.BlockIndex;

	// Remove the transaction
	Transactions.RemoveAt( RemoveIndex );

	// Iterate over all transactions after this one to fix up indices.
	for( int32 StartIndex = RemoveIndex; StartIndex < Transactions.Num(); ++StartIndex )
	{
		int32& CheckIndex = Transactions[StartIndex].BlockIndex;
		
		// If the index is greater than or equal to the index we are removing, this transaction is affected by this removal.
		if( CheckIndex >= TransToRemoveIndex )
		{
			// If the removed transaction was an add, subtract 1 from the index of this transaction. Otherwise, add one.
			if( TransToRemove.TransactionType == FCustomBlockTransaction::Add )
			{
				--CheckIndex;
			}
			else
			{
				++CheckIndex;
			}
		}
	}
}

bool FMultiBoxCustomizationData::RemoveDuplicateTransaction()
{
	if( Transactions.Num() > 0 )
	{
		FCustomBlockTransaction LastTrans = Transactions.Last();

		int32 CheckIndex = LastTrans.BlockIndex;
		
		// Skip the last item
		for( int32 TransIndex = Transactions.Num()-2; TransIndex >= 0; --TransIndex )
		{
			FCustomBlockTransaction CurrentTrans = Transactions[TransIndex];

			//ensure( !(CurrentTrans.TransactionType == LastTrans.TransactionType && CurrentTrans.Command == LastTrans.Command ) );

			if( CurrentTrans.Command == LastTrans.Command &&
				CurrentTrans.BlockIndex == CheckIndex &&
				CurrentTrans.TransactionType != LastTrans.TransactionType )
			{
				RemoveTransactionAt( Transactions.Num()-1 );
				RemoveTransactionAt( TransIndex );

				return true;
			}

			if( CheckIndex >= CurrentTrans.BlockIndex )
			{
				if( CurrentTrans.TransactionType == FCustomBlockTransaction::Add )
				{
					--CheckIndex;
				}
				else
				{
					++CheckIndex;
				}
			}
		}
	}

	return false;
}

bool FMultiBoxCustomizationData::RemoveUnnecessaryTransactions(const TArray< TSharedRef< const FMultiBlock > >& AllBlocks)
{
	// A local struct describing the state of the menu
	struct FCustomizationState
	{
		FCustomizationState(const TArray< TSharedRef< const FMultiBlock > >& InAllBlocks)
		{
			for ( int32 BlockIdx = 0; BlockIdx < InAllBlocks.Num(); ++BlockIdx )
			{
				StateData.Add(InAllBlocks[BlockIdx]->GetAction());
			}
		}

		// Applies the inverse operation of the given transaction to mutate the state
		void ApplyInverseOfTransaction(const FCustomBlockTransaction& Trans)
		{
			if ( Trans.TransactionType == FCustomBlockTransaction::Add )
			{
				// Remove on an add operation
				if ( ensure(StateData.IsValidIndex(Trans.BlockIndex)) )
				{
					ensure(StateData[Trans.BlockIndex] == Trans.Command);

					StateData.RemoveAt(Trans.BlockIndex);
				}
			}
			else if ( Trans.TransactionType == FCustomBlockTransaction::Remove )
			{
				// Add on a remove transaction
				StateData.Insert(Trans.Command, Trans.BlockIndex);
			}
		}

		// Compares two states. If all elements of the states are the same then they are "equal"
		bool operator==(const FCustomizationState& Other) const
		{
			if ( StateData.Num() != Other.StateData.Num() )
			{
				return false;
			}

			const int32 Length = StateData.Num();
			for ( int32 StateIdx = 0; StateIdx < Length; ++StateIdx )
			{
				if ( StateData[StateIdx] != Other.StateData[StateIdx] )
				{
					return false;
				}
			}

			return true;
		}

	private:
		TArray<TWeakPtr<const FUICommandInfo>> StateData;
	};

	if( Transactions.Num() > 0 )
	{
		// Take the current state and start applying the inverse of each transaction to it to determine the states before the current.
		// If we found any state that is identical to the current state, all transactions between that state and the current state are unnecessary, so remove them

		// Record the initial (current) state and initialize the Test state.
		FCustomizationState InitialState(AllBlocks);
		FCustomizationState TestState = InitialState;

		// Walk backwards through all transactions, applying the inverse of each transaction to the test state
		for ( int32 TransIdx = Transactions.Num() - 1; TransIdx >= 0; --TransIdx )
		{
			TestState.ApplyInverseOfTransaction(Transactions[TransIdx]);

			// If the test state is equal to the current state, all transactions between are unnecessary. Remove them.
			if ( TestState == InitialState )
			{
				for ( int32 RemoveIdx = Transactions.Num() - 1; RemoveIdx >= TransIdx; --RemoveIdx )
				{
					RemoveTransactionAt(RemoveIdx);
				}

				// Return true to indicate we actually removed something.
				return true;
			}
		}
	}

	// All transactions were necessary to form the final state. Return false.
	return false;
}

void FMultiBoxCustomizationData::SaveCustomizedBlocks()
{
	FString SaveData;
	TSharedRef< TJsonWriter<> > Writer = TJsonWriterFactory<>::Create( &SaveData );

	Writer->WriteObjectStart();
	Writer->WriteArrayStart(TEXT("CustomBlocks"));
	for (TArray<FCustomBlockTransaction>::TConstIterator It(Transactions); It; ++It)
	{
		const FCustomBlockTransaction& Transaction = *It;

		if( Transaction.Command.IsValid() )
		{
			const FUICommandInfo& Command = *Transaction.Command.Pin();

			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("CommandName"), Command.GetCommandName().ToString() );
			Writer->WriteValue(TEXT("Context"), Command.GetBindingContext().ToString() );
			Writer->WriteValue(TEXT("Index"), (float)Transaction.BlockIndex );
			Writer->WriteValue(TEXT("TransactionType"), (float)Transaction.TransactionType );
			Writer->WriteObjectEnd();
		}
	}

	Writer->WriteArrayEnd();
	Writer->WriteObjectEnd();
	Writer->Close();

	GConfig->SetString( *GetConfigSectionName(), *CustomizationName.ToString(), *FRemoteConfig::ReplaceIniCharWithSpecialChar(SaveData).ReplaceCharWithEscapedChar(), GEditorPerProjectIni );
}

void FMultiBoxCustomizationData::BlockRemoved( TSharedRef< const FMultiBlock> RemovedBlock, int32 Index, const TArray< TSharedRef< const FMultiBlock > >& AllBlocks )
{
	FCustomBlockTransaction Remove = FCustomBlockTransaction::CreateRemove( RemovedBlock->GetAction().ToSharedRef(), Index );

	SaveTransaction( Remove, AllBlocks );

}

void FMultiBoxCustomizationData::BlockAdded( TSharedRef< const FMultiBlock > AddedBlock, int32 Index, const TArray< TSharedRef< const FMultiBlock > >& AllBlocks )
{
	FCustomBlockTransaction Add = FCustomBlockTransaction::CreateAdd( AddedBlock->GetAction().ToSharedRef(), Index );

	SaveTransaction( Add, AllBlocks );
}

#define DEBUG_TRANSACTIONS 0
#if DEBUG_TRANSACTIONS
static void PrintTransactions( const TArray<FCustomBlockTransaction>& Transactions )
{
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("==========BEGIN TRANSACTIONS=======\n"));
	for( int32 i = 0; i < Transactions.Num(); ++i )
	{
		const FCustomBlockTransaction& Trans = Transactions[i];

		FString TransType = Trans.TransactionType == FCustomBlockTransaction::Add ? TEXT("+") : TEXT("-");
		FPlatformMisc::LowLevelOutputDebugStringf( TEXT("%s(%s,%d)\n"), *TransType, *Trans.Command.Pin()->GetCommandName().ToString(), Trans.BlockIndex );
	}
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("============END TRANSACTIONS=======\n"));
}
#endif

void FMultiBoxCustomizationData::SaveTransaction( const struct FCustomBlockTransaction& Transaction, const TArray< TSharedRef< const FMultiBlock > >& AllBlocks )
{
	Transactions.Add( Transaction );

	while( RemoveDuplicateTransaction() );
	while( RemoveUnnecessaryTransactions(AllBlocks) );

#if DEBUG_TRANSACTIONS
	PrintTransactions( Transactions );
#endif

	SaveCustomizedBlocks();
}
