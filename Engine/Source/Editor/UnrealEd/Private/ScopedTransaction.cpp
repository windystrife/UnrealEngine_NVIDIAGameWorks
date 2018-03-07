// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "ScopedTransaction.h"
#include "Framework/Application/SlateApplication.h"
#include "Editor.h"

FScopedTransaction::FScopedTransaction(const FText& SessionName, const bool bShouldActuallyTransact )
{
	Construct( TEXT(""), SessionName, NULL, bShouldActuallyTransact);
}

FScopedTransaction::FScopedTransaction(const TCHAR* TransactionContext, const FText& SessionName, UObject* PrimaryObject, const bool bShouldActuallyTransact)
{
	Construct( TransactionContext, SessionName, PrimaryObject, bShouldActuallyTransact);
}

void FScopedTransaction::Construct (const TCHAR* TransactionContext, const FText& SessionName, UObject* PrimaryObject, const bool bShouldActuallyTransact)
{
	if( bShouldActuallyTransact && GEditor && GEditor->Trans && !GEditor->bIsSimulatingInEditor && ensure(!GIsTransacting))
	{
		FSlateApplication::Get().OnLogSlateEvent(EEventLog::BeginTransaction, SessionName );
		Index = GEditor->BeginTransaction( TransactionContext, SessionName, PrimaryObject );
		check( IsOutstanding() );
	}
	else
	{
		Index = -1;
	}
}

FScopedTransaction::~FScopedTransaction()
{
	if ( IsOutstanding() )
	{
		FSlateApplication::Get().OnLogSlateEvent(EEventLog::EndTransaction);
		GEditor->EndTransaction();
	}
}

/**
 * Cancels the transaction.  Reentrant.
 */
void FScopedTransaction::Cancel()
{
	if ( IsOutstanding() )
	{
		GEditor->CancelTransaction( Index );
		Index = -1;
	}
}

/**
 * @return	true if the transaction is still outstanding (that is, has not been cancelled).
 */
bool FScopedTransaction::IsOutstanding() const
{
	return Index >= 0;
}
