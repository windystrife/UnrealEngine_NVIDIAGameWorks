// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AsyncPackage.h: Unreal async loading definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/UObjectGlobals.h"
#include "Misc/Guid.h"
#include "UniquePtr.h"

struct FAsyncPackageDesc
{
	/** Handle for the caller */
	int32 RequestID;
	/** Name of the UPackage to create. */
	FName Name;
	/** Name of the package to load. */
	FName NameToLoad;
	/** GUID of the package to load, or the zeroed invalid GUID for "don't care" */
	FGuid Guid;
	/** Delegate called on completion of loading. This delegate can only be created and consumed on the game thread */
	TUniquePtr<FLoadPackageAsyncDelegate> PackageLoadedDelegate;
	/** The flags that should be applied to the package */
	EPackageFlags PackageFlags;
	/** Package loading priority. Higher number is higher priority. */
	TAsyncLoadPriority Priority;
	/** PIE instance ID this package belongs to, INDEX_NONE otherwise */
	int32 PIEInstanceID;


	FAsyncPackageDesc(int32 InRequestID, const FName& InName, FName InPackageToLoadFrom = NAME_None, const FGuid& InGuid = FGuid(), TUniquePtr<FLoadPackageAsyncDelegate>&& InCompletionDelegate = TUniquePtr<FLoadPackageAsyncDelegate>(), EPackageFlags InPackageFlags = PKG_None, int32 InPIEInstanceID = INDEX_NONE, TAsyncLoadPriority InPriority = 0)
		: RequestID(InRequestID)
		, Name(InName)
		, NameToLoad(InPackageToLoadFrom)
		, Guid(InGuid)
		, PackageLoadedDelegate(MoveTemp(InCompletionDelegate))
		, PackageFlags(InPackageFlags)
		, Priority(InPriority)
		, PIEInstanceID(InPIEInstanceID)
	{
		if (NameToLoad == NAME_None)
		{
			NameToLoad = Name;
		}
	}

	/** This constructor does not modify the package loaded delegate as this is not safe outside the game thread */
	FAsyncPackageDesc(const FAsyncPackageDesc& OldPackage)
		: RequestID(OldPackage.RequestID)
		, Name(OldPackage.Name)
		, NameToLoad(OldPackage.NameToLoad)
		, Guid(OldPackage.Guid)
		, PackageFlags(OldPackage.PackageFlags)
		, Priority(OldPackage.Priority)
		, PIEInstanceID(OldPackage.PIEInstanceID)
	{
	}

	/** This constructor will explicitly copy the package loaded delegate and invalidate the old one */
	FAsyncPackageDesc(const FAsyncPackageDesc& OldPackage, TUniquePtr<FLoadPackageAsyncDelegate>&& InPackageLoadedDelegate)
		: FAsyncPackageDesc(OldPackage)
	{
		PackageLoadedDelegate = MoveTemp(InPackageLoadedDelegate);
	}

#if DO_GUARD_SLOW
	~FAsyncPackageDesc()
	{
		checkSlow(!PackageLoadedDelegate.IsValid() || IsInGameThread());
	}
#endif
};
