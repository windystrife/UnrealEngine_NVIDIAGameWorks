// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Editor/TransBuffer.h"
#include "MatineeTransBuffer.generated.h"

/**
 * Special transaction buffer for Matinee undo/redo.
 * Will be capped at InMaxMemory.
 */
UCLASS(transient)
class UMatineeTransBuffer : public UTransBuffer
{
public:
	GENERATED_BODY()

	/**  
	 * Begin a Matinee specific transaction
	 * @param	Description		The description for transaction event
	 */
	virtual void BeginSpecial(const FText& Description);

	/** End a Matinee specific transaction */
	virtual void EndSpecial();

public:

	// UTransactor Interface
	virtual int32 Begin( const TCHAR* SessionContext, const FText& Description ) override
	{
		return 0;
	}

	virtual int32 End() override
	{
		return 1;
	}

	virtual void Cancel(int32 StartIndex = 0) override {}
};
