// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Interface for objects that can be ticked from the high-frequency media thread.
 */
class IMediaTickable
{
public:

	/** Tick the object. */
	virtual void TickTickable() = 0;

public:

	/** Virtual destructor. */
	virtual ~IMediaTickable() { }
};
