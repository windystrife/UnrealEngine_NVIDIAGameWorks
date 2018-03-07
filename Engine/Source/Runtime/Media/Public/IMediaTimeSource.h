// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Interface for media clock time sources.
 */
class IMediaTimeSource
{
public:

	/**
	 * Get the current time code.
	 *
	 * @return Time code.
	 */
	virtual FTimespan GetTimecode() = 0;

public:

	/** Virtual destructor. */
	virtual ~IMediaTimeSource() { }
};
