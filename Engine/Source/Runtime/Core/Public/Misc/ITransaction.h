// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "UObject/UObjectHierarchyFwd.h"

// Class for handling undo/redo transactions among objects.
typedef void(*STRUCT_DC)( void* TPtr );						// default construct
typedef void(*STRUCT_AR)( class FArchive& Ar, void* TPtr );	// serialize
typedef void(*STRUCT_DTOR)( void* TPtr );					// destruct


/**
 * Interface for transaction object annotations.
 *
 * Transaction object annotations are used for attaching additional user defined data to a transaction.
 * This is sometimes useful, because the transaction system only remembers changes that are serializable
 * on the UObject that a modification was performed on, but it does not see other changes that may have
 * to be remembered in order to properly restore the object internals.
 */
class ITransactionObjectAnnotation 
{ 
public:
	virtual ~ITransactionObjectAnnotation() {}
	virtual void AddReferencedObjects(class FReferenceCollector& Collector) = 0;
};


/**
 * Interface for transactions.
 *
 * Transactions are created each time an UObject is modified, for example in the Editor.
 * The data stored inside a transaction object can then be used to provide undo/redo functionality.
 */
class ITransaction
{
public:

	/** Applies the transaction. */
	virtual void Apply( ) = 0;

	/**
	 * Saves an array to the transaction.
	 *
	 * @param Object The object that owns the array.
	 * @param Array The array to save.
	 * @param Index 
	 * @param Count 
	 * @param Oper
	 * @param ElementSize
	 * @param Serializer
	 * @param Destructor
	 * @see SaveObject
	 */
	virtual void SaveArray( UObject* Object, class FScriptArray* Array, int32 Index, int32 Count, int32 Oper, int32 ElementSize, STRUCT_DC DefaultConstructor, STRUCT_AR Serializer, STRUCT_DTOR Destructor ) = 0;

	/**
	 * Saves an UObject to the transaction.
	 *
	 * @param Object The object to save.
	 * @see SaveArray
	 */
	virtual void SaveObject( UObject* Object ) = 0;

	/**
	 * Sets the transaction's primary object.
	 *
	 * @param Object The primary object to set.
	 */
	virtual void SetPrimaryObject( UObject* Object ) = 0;
};
