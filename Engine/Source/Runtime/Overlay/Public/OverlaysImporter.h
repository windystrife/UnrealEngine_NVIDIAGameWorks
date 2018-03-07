// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Overlays.h"
#include "Containers/UnrealString.h"

enum class EOverlaysFileType
{
	Unknown,
	SubRipSubtitles,
};

class OVERLAY_API FOverlaysImporter
{
public:
	FOverlaysImporter();
	~FOverlaysImporter();

	/**
	 * Opens the file and preparses it for import
	 *
	 * @param	Filename	The file to open
	 * @return	True if the file was opened successfully, false if the file could not be opened or is not an overlay file.
	 */
	bool OpenFile(const FString& FilePath);

	/**
	 * Parses the supplied import file for basic overlay data
	 *
	 * @param	OutSubtitles	The output array where new overlays are stored. This is emptied when the import begins
	 * @return	True if the file was parsed successfully
	 */
	bool ImportBasic(TArray<FOverlayItem>& OutOverlays) const;

	/**
	 * Resets the importer to a default state
	 */
	void Reset();

private:
	bool ParseSubRipSubtitles(TArray<FOverlayItem>& OutSubtitles) const;

private:
	FString Filename;
	FString FileContents;

	EOverlaysFileType FileType;
};