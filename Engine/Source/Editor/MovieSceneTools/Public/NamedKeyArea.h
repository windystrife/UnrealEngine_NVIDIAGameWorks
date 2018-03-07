// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IKeyArea.h"

/**
 * Abstract base class for named key areas.
 */
class MOVIESCENETOOLS_API FNamedKeyArea
	: public IKeyArea
{
public:

	/** Set the name of this key area */
	virtual void SetName(FName Name) override
	{
		KeyAreaName = Name;
	}

	/** Get the name of this key area */
	virtual FName GetName() const override
	{
		return KeyAreaName;
	}

protected:

	/** The name of this key area */
	FName KeyAreaName;
};
