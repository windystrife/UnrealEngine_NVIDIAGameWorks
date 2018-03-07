// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
/** 
 * Interface for data deriving backends
 * This API will not be called concurrently, except that Build might be called on different instances if IsBuildThreadsafe.
**/
class FDerivedDataPluginInterface
{
public:
	virtual ~FDerivedDataPluginInterface()
	{
	}
	/** 
	* Get the plugin name, this is used as the first part of the cache key 
	* @return	Name of the plugin
	**/
 	virtual const TCHAR* GetPluginName() const = 0;
	/** 
	* Get the version of the plugin, this is used as part of the cache key. This is supposed to
	* be a guid string ( ex. "69C8C8A6-A9F8-4EFC-875C-CFBB72E66486" )
	* @return	Version string of the plugin
	**/
 	virtual const TCHAR* GetVersionString() const = 0;
	
	/** 
	* Returns the largest and plugin specific part of the cache key. This must be a alphanumeric+underscore
	* @return	Version number of the plugin, for licensees.
	**/
 	virtual FString GetPluginSpecificCacheKeySuffix() const = 0;
	/** 
	* Indicates that this plugin is threadsafe. Note, the system itself will not call it concurrently if this false, however, then you are responsible for not calling the system itself concurrently.
	* @return	true if this plugin is threadsafe
	**/
	virtual bool IsBuildThreadsafe() const = 0;

	/** Indicated that this plugin generates deterministic data. This is used for DDC verification */
	virtual bool IsDeterministic() const { return false; }

	/** Indicated that this plugin generates deterministic data. This is used for DDC verification */
	virtual FString GetDebugContextString() const { return TEXT("Unknown Context"); }

	/** 
	* Does the work of deriving the data. 
	* @param	OutData	Array of bytes to fill in with the result data
	* @return	true if successful, in the event of failure the cache is not updated and failure is propagated to the original caller.
	**/
 	virtual bool Build(TArray<uint8>& OutData) = 0;
};


