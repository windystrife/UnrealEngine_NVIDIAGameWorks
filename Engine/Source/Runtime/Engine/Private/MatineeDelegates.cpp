// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MatineeDelegates.h"

FMatineeDelegates& FMatineeDelegates::Get()
{
	// return the singleton object
	static FMatineeDelegates Singleton;
	return Singleton;
}
