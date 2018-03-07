// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "CoreFwd.h"

/**
* Platform File chain manager.
**/
class CORE_API FPlatformFileManager
{
	/** Currently used platform file. */
	class IPlatformFile* TopmostPlatformFile;

public:

	/** Constructor. */
	FPlatformFileManager( );

	/**
	 * Gets the currently used platform file.
	 *
	 * @return Reference to the currently used platform file.
	 */
	IPlatformFile& GetPlatformFile( );

	/**
	 * Sets the current platform file.
	 *
	 * @param NewTopmostPlatformFile Platform file to be used.
	 */
	void SetPlatformFile( IPlatformFile& NewTopmostPlatformFile );

	/**
	 * Finds a platform file in the chain of active platform files.
	 *
	 * @param Name of the platform file.
	 * @return Pointer to the active platform file or nullptr if the platform file was not found.
	 */
	IPlatformFile* FindPlatformFile( const TCHAR* Name );

	/**
	 * Creates a new platform file instance.
	 *
	 * @param Name of the platform file to create.
	 * @return Platform file instance of the platform file type was found, nullptr otherwise.
	 */
	IPlatformFile* GetPlatformFile( const TCHAR* Name );

	/**
	 * calls Tick on the platform files in the TopmostPlatformFile chain
	 */
	void TickActivePlatformFile();

	/**
	* Permorms additional initialization when the new async IO is enabled.
	*/
	void InitializeNewAsyncIO();

	/**
	 * Gets FPlatformFileManager Singleton.
	 */
	static FPlatformFileManager& Get( );

private:

	/**
	* Removes the specified file wrapper from the platform file wrapper chain.
	*
	* THIS IS EXTREMELY DANGEROUS AFTER THE ENGINE HAS BEEN INITIALIZED AS WE MAY BE MODIFYING
	* THE WRAPPER CHAIN WHILE THINGS ARE BEING LOADED
	*
	* @param Name of the platform file to create.
	* @return Platform file instance of the platform file type was found, nullptr otherwise.
	*/
	void RemovePlatformFile(IPlatformFile* PlatformFileToRemove);

};
