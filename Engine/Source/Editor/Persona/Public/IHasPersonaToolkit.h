// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Interface implemented by objects that hold a persona toolkit */
class IHasPersonaToolkit
{
public:
	/** Get the toolkit held by this object */
	virtual TSharedRef<class IPersonaToolkit> GetPersonaToolkit() const = 0;
};
