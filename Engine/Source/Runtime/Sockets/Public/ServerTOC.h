// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Helper struct to hold and quickly access server TOC.
 */
struct SOCKETS_API FServerTOC
{
	/** List of files in a directory. */
	typedef TMap<FString, FDateTime> FDirectory;

	/** This is the "TOC" of the server */
	TMap<FString, FDirectory*> Directories;

	/** Destructor. Destroys directories. */
	~FServerTOC();

	/**
	 * Adds a file or directory to TOC.
	 *
	 * @param Filename File name or directory name to add.
	 * @param Timestamp File timestamp. Directories should have this set to 0.
	 */
	void AddFileOrDirectory(const FString& Filename, const FDateTime& Timestamp);

	/**
	 * Finds a file in TOC.
	 *
	 * @param Filename File name to find.
	 * @return Pointer to a timestamp if the file was found, NULL otherwise.
	 */
	const FDateTime* FindFile(const FString& Filename) const;

	/**
	 * Finds a directory in TOC.
	 *
	 * @param Directory Directory to find.
	 * @return Pointer to a FDirectory if the directory was found, NULL otherwise.
	 */
	const FDirectory* FindDirectory(const FString& Directory) const;

	/**
	 * Finds a directory in TOC non const version used internally 
	 * see FindDirectory
	 *
	 * @param Directory Directory to find
	 * @return pointer to a FDirectory if the directory was found, null otherwise
	 */
	FDirectory* FindDirectory(const FString& Directory);


	int32 RemoveFileOrDirectory(const FString& Filename);
};
