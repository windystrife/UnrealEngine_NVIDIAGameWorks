// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	CookCommandlet.cpp: Commandlet for cooking content
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "UObject/WeakObjectPtr.h"
#include "Templates/ScopedPointer.h"
#include "Misc/PackageName.h"
#include "Commandlets/Commandlet.h"
#include "UniquePtr.h"
#include "IPlatformFileSandboxWrapper.h"
#include "CookCommandlet.generated.h"

class FSandboxPlatformFile;
class ITargetPlatform;

UCLASS(config=Editor)
class UCookCommandlet
	: public UCommandlet
{
	GENERATED_UCLASS_BODY()

	/** List of asset types that will force GC after loading them during cook */
	UPROPERTY(config)
	TArray<FString> FullGCAssetClassNames;

	/** If true, iterative cooking is being done */
	bool bIterativeCooking;
	/** Prototype cook-on-the-fly server */
	bool bCookOnTheFly; 
	/** Cook everything */
	bool bCookAll;
	/** Skip saving any packages in Engine/COntent/Editor* UNLESS TARGET HAS EDITORONLY DATA (in which case it will save those anyway) */
	bool bSkipEditorContent;
	/** Test for UObject leaks */
	bool bLeakTest;  
	/** Save all cooked packages without versions. These are then assumed to be current version on load. This is dangerous but results in smaller patch sizes. */
	bool bUnversioned;
	/** Generate manifests for building streaming install packages */
	bool bGenerateStreamingInstallManifests;
	/** Error if we access engine content (useful for dlc) */
	bool bErrorOnEngineContentUse;
	/** Use historical serialization system for generating package dependencies (use for historical reasons only this method has been depricated, only affects cooked manifests) */
	bool bUseSerializationForGeneratingPackageDependencies;
	/** Only cook packages specified on commandline options (for debugging)*/
	bool bCookSinglePackage;
	/** Should we output additional verbose cooking warnings */
	bool bVerboseCookerWarnings;
	/** only clean up objects which are not in use by the cooker when we gc (false will enable full gc) */
	bool bPartialGC;
	/** All commandline tokens */
	TArray<FString> Tokens;
	/** All commandline switches */
	TArray<FString> Switches;
	/** All commandline params */
	FString Params;

	/**
	 * Cook on the fly routing for the commandlet
	 *
	 * @param  BindAnyPort					Whether to bind on any port or the default port.
	 * @param  Timeout						Length of time to wait for connections before attempting to close
	 * @param  bForceClose					Whether or not the server should always shutdown after a timeout or after a user disconnects
	 *
	 * @return true on success, false otherwise.
	 */
	bool CookOnTheFly( FGuid InstanceId, int32 Timeout = 180, bool bForceClose = false );

	/** Cooks specified list of files */
	bool CookByTheBook(const TArray<ITargetPlatform*>& Platforms, TArray<FString>& FilesInPath);

	/** See if the cooker has exceeded max memory allowance in this case the cooker should force a garbage collection */
	bool HasExceededMaxMemory(uint64 MaxMemoryAllowance) const;

	/**	Process deferred commands */
	void ProcessDeferredCommands();

public:

	//~ Begin UCommandlet Interface

	virtual int32 Main(const FString& CmdLineParams) override;
	
	//~ End UCommandlet Interface

};
