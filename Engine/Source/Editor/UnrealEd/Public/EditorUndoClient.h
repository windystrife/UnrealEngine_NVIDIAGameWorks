// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	EditorUndoClient.h: Declares the FEditorUndoClient interface.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"

/**
 * Interface for tools wanting to handle undo/redo operations
 */
class UNREALED_API FEditorUndoClient
{
public:
	/** Always unregister for undo on destruction, just in case. */
	virtual ~FEditorUndoClient();

	/**
	 * Called to see if the context of the current undo/redo operation is a match for the client
	 * Default state matching old context-less undo is Context="" and PrimaryObject=NULL
	 *
	 * @param InContext		A text string providing context for the undo operation; can be the empty string
	 * @param PrimaryObject	The object marked as the primary object for the undo operation; can be NULL
	 * 
	 * @return	True if client wishes to handle the undo/redo operation for this context. False otherwise
	 */
	virtual bool MatchesContext( const FString& InContext, UObject* PrimaryObject ) const { return true; }

	/** 
	 * Signal that client should run any PostUndo code
	 * @param bSuccess	If true than undo succeeded, false if undo failed
	 */
	virtual void PostUndo( bool bSuccess ) {};
	
	/** 
	 * Signal that client should run any PostRedo code
	 * @param bSuccess	If true than redo succeeded, false if redo failed
	 */
	virtual void PostRedo( bool bSuccess )	{};

	/** Return the transaction context for this client */
	virtual FString GetTransactionContext() const { return FString(); }
};
