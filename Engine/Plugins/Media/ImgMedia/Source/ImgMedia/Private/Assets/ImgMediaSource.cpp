// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "ImgMediaSource.h"
#include "ImgMediaPrivate.h"

#include "HAL/FileManager.h"
#include "Misc/Paths.h"


/* UImgMediaSource structors
 *****************************************************************************/

UImgMediaSource::UImgMediaSource()
	: FramesPerSecondOverride(0.0f)
{ }


/* UImgMediaSource interface
 *****************************************************************************/

void UImgMediaSource::GetProxies(TArray<FString>& OutProxies) const
{
	IFileManager::Get().FindFiles(OutProxies, *FPaths::Combine(GetFullPath(), TEXT("*")), false, true);
}


void UImgMediaSource::SetSequencePath(const FString& Path)
{
	const FString SanitizedPath = FPaths::GetPath(Path);

	if (SanitizedPath.IsEmpty() || SanitizedPath.StartsWith(TEXT(".")))
	{
		SequencePath.Path = SanitizedPath;
	}
	else
	{
		FString FullPath = FPaths::ConvertRelativePathToFull(SanitizedPath);
		const FString FullGameContentDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());

		if (FullPath.StartsWith(FullGameContentDir))
		{
			FPaths::MakePathRelativeTo(FullPath, *FullGameContentDir);
			FullPath = FString(TEXT("./")) + FullPath;
		}

		SequencePath.Path = FullPath;
	}
}


/* IMediaOptions interface
 *****************************************************************************/

double UImgMediaSource::GetMediaOption(const FName& Key, const double DefaultValue) const
{
	if (Key == ImgMedia::FramesPerSecondOverrideOption)
	{
		return FramesPerSecondOverride;
	}

	return Super::GetMediaOption(Key, DefaultValue);
}


FString UImgMediaSource::GetMediaOption(const FName& Key, const FString& DefaultValue) const
{
	if (Key == ImgMedia::ProxyOverrideOption)
	{
		return ProxyOverride;
	}

	return Super::GetMediaOption(Key, DefaultValue);
}


bool UImgMediaSource::HasMediaOption(const FName& Key) const
{
	if ((Key == ImgMedia::FramesPerSecondOverrideOption) ||
		(Key == ImgMedia::ProxyOverrideOption))
	{
		return true;
	}

	return Super::HasMediaOption(Key);
}


/* UMediaSource interface
 *****************************************************************************/

FString UImgMediaSource::GetUrl() const
{
	return FString(TEXT("img://")) + GetFullPath();
}


bool UImgMediaSource::Validate() const
{
	return FPaths::DirectoryExists(GetFullPath());
}


/* UFileMediaSource implementation
 *****************************************************************************/

FString UImgMediaSource::GetFullPath() const
{
	if (!FPaths::IsRelative(SequencePath.Path))
	{
		return SequencePath.Path;
	}

	if (SequencePath.Path.StartsWith(TEXT("./")))
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir(), SequencePath.Path.RightChop(2));
	}

	return FPaths::ConvertRelativePathToFull(SequencePath.Path);
}
