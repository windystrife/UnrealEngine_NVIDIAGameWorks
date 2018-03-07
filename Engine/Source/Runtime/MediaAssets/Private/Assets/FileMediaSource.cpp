// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "FileMediaSource.h"
#include "Misc/Paths.h"


namespace FileMediaSource
{
	/** Name of the PrecacheFile media option. */
	static const FName PrecacheFileOption("PrecacheFile");
}


/* UFileMediaSource interface
 *****************************************************************************/

FString UFileMediaSource::GetFullPath() const
{
	if (!FPaths::IsRelative(FilePath))
	{
		return FilePath;
	}

	if (FilePath.StartsWith(TEXT("./")))
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir(), FilePath.RightChop(2));
	}

	return FPaths::ConvertRelativePathToFull(FilePath);
}


void UFileMediaSource::SetFilePath(const FString& Path)
{
	if (Path.IsEmpty() || Path.StartsWith(TEXT("./")))
	{
		FilePath = Path;
	}
	else
	{
		FString FullPath = FPaths::ConvertRelativePathToFull(Path);
		const FString FullGameContentDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());

		if (FullPath.StartsWith(FullGameContentDir))
		{
			FPaths::MakePathRelativeTo(FullPath, *FullGameContentDir);
			FullPath = FString(TEXT("./")) + FullPath;
		}

		FilePath = FullPath;
	}
}


/* IMediaSource overrides
 *****************************************************************************/

bool UFileMediaSource::GetMediaOption(const FName& Key, bool DefaultValue) const
{
	if (Key == FileMediaSource::PrecacheFileOption)
	{
		return PrecacheFile;
	}

	return Super::GetMediaOption(Key, DefaultValue);
}


bool UFileMediaSource::HasMediaOption(const FName& Key) const
{
	if (Key == FileMediaSource::PrecacheFileOption)
	{
		return true;
	}

	return Super::HasMediaOption(Key);
}


/* UMediaSource overrides
 *****************************************************************************/

FString UFileMediaSource::GetUrl() const
{
	return FString(TEXT("file://")) + GetFullPath();
}


bool UFileMediaSource::Validate() const
{
	return FPaths::FileExists(GetFullPath());
}
