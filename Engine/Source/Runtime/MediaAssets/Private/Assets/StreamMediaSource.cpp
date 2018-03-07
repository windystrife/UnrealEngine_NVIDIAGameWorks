// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "StreamMediaSource.h"


/* UMediaSource overrides
 *****************************************************************************/

FString UStreamMediaSource::GetUrl() const
{
	return StreamUrl;
}


bool UStreamMediaSource::Validate() const
{
	return StreamUrl.Contains(TEXT("://"));
}
