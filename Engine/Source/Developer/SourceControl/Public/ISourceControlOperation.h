// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class ISourceControlOperation : public TSharedFromThis<ISourceControlOperation, ESPMode::ThreadSafe>
{
public:
	/**
	 * Virtual destructor
	 */
	virtual ~ISourceControlOperation() {}

	/** Get the name of this operation, used as a unique identifier */
	virtual FName GetName() const = 0;

	/** Get the string to display when this operation is in progress */
	virtual FText GetInProgressString() const 
	{ 
		return FText(); 
	}

	/** Factory method for easier operation creation */
	template<typename Type>
	static TSharedRef<Type, ESPMode::ThreadSafe> Create()
	{
		return MakeShareable( new Type() );
	}
};

typedef TSharedRef<class ISourceControlOperation, ESPMode::ThreadSafe> FSourceControlOperationRef;
