// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMediaTimeSource.h"
#include "Misc/App.h"
#include "Misc/Timespan.h"


/**
 * Implements the a media time source that derives its time from the application's global time.
 */
class FAppMediaTimeSource
	: public IMediaTimeSource
{
public:

	/** Virtual destructor. */
	virtual ~FAppMediaTimeSource() { }

public:

	//~ IMediaTimeSource interface

	virtual FTimespan GetTimecode() override
	{
		return FTimespan::FromSeconds(FApp::GetCurrentTime());
	}
};
