// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * IShaderFormatArchives, pre-compiled shader archive abstraction
 */
class IShaderFormatArchive
{
public:
	
	/**
	 * @returns The format name for the shaders in the archive.
	 */
	virtual FName GetFormat( void ) const = 0;
	
	/**
	 * @param Frequency The shader frequency.
	 * @param Hash The SHA shader hash
	 * @param Code The input shader code - the archive may strip data so that the code is only stored in the archive.
	 * @returns The identifier for the shader entry - this is not necessarily an index.
	 */
	virtual bool AddShader( uint8 Frequency, const FSHAHash& Hash, TArray<uint8>& Code ) = 0;
	
	/**
	 * @param The output directory for the archive.
	 * @param The directory for the debug data previously generated.
	 * @param Optional pointer to a TArray that on success will be filled with a list of the written file paths.
	 * @returns True if the archive was finalized successfully or false on error.
	 */
	virtual bool Finalize( FString OutputDir, FString DebugOutputDir, TArray<FString>* OutputFiles ) = 0;
	
public:

	/** Virtual destructor. */
	virtual ~IShaderFormatArchive() { }
};
