// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformFilemanager.h"

namespace BuildPatchServices
{
	struct FFileAttributes
	{
		bool bReadOnly;
		bool bCompressed;
		bool bUnixExecutable;
		TSet<FString> InstallTags;
		FFileAttributes();
	};

	class FFileAttributesParser
	{
	public:
		/**
		 * Loads in the file attributes meta file and populates the given map.
		 * @param   MetaFilename        The amount of data to attempt to retrieve.
		 * @param   FileAttributes      The map to populate with the attributes
		 * @return  True if the file existed and parsed successfully
		 */
		virtual bool ParseFileAttributes(const FString& MetaFilename, TMap<FString, FFileAttributes>& FileAttributes) = 0;
	};

	typedef TSharedRef<FFileAttributesParser, ESPMode::ThreadSafe> FFileAttributesParserRef;
	typedef TSharedPtr<FFileAttributesParser, ESPMode::ThreadSafe> FFileAttributesParserPtr;

	class FFileAttributesParserFactory
	{
	public:
		static FFileAttributesParserRef Create(IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile());
	};
}
