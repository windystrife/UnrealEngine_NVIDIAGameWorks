// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Editor/Transactor.h"

/** 
 * Matinee specific transaction record that ignores non-matinee objects.
 */
class FMatineeTransaction : public FTransaction
{
public:

	FMatineeTransaction( const TCHAR* InContext = nullptr, const FText& InTitle=FText(), bool InFlip=0 )
		:	FTransaction(InContext, InTitle, InFlip)
	{ }

public:

	// FTransaction interface

	virtual void SaveObject( UObject* Object ) override;
	virtual void SaveArray( UObject* Object, FScriptArray* Array, int32 Index, int32 Count, int32 Oper, int32 ElementSize, STRUCT_DC DefaultConstructor, STRUCT_AR Serializer, STRUCT_DTOR Destructor ) override;
};

