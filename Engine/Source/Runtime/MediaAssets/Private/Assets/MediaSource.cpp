// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MediaSource.h"


/* IMediaOptions interface
 *****************************************************************************/

FName UMediaSource::GetDesiredPlayerName() const
{
	return NAME_None;
}


bool UMediaSource::GetMediaOption(const FName& Key, bool DefaultValue) const
{
	return DefaultValue;
}


double UMediaSource::GetMediaOption(const FName& Key, double DefaultValue) const
{
	return DefaultValue;
}


int64 UMediaSource::GetMediaOption(const FName& Key, int64 DefaultValue) const
{
	return DefaultValue;
}


FString UMediaSource::GetMediaOption(const FName& Key, const FString& DefaultValue) const
{
	return DefaultValue;
}


FText UMediaSource::GetMediaOption(const FName& Key, const FText& DefaultValue) const
{
	return DefaultValue;
}


bool UMediaSource::HasMediaOption(const FName& Key) const
{
	return false;
}
