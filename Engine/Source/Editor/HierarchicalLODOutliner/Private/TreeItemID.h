// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectKey.h"

namespace HLODOutliner
{
	struct FTreeItemID
	{
	public:
		FTreeItemID() : CachedHash(0) {}

		FTreeItemID(const UObject* InObject)
		{
			ObjectKey = FObjectKey(InObject);
			CachedHash = GetTypeHash(ObjectKey);
		}

		/** Copy construction / assignment */
		FTreeItemID(const FTreeItemID& Other)
		{
			*this = Other;
		}

		FTreeItemID& operator=(const FTreeItemID& Other)
		{
			ObjectKey = Other.ObjectKey;
			CachedHash = Other.CachedHash;
			return *this;
		}

		/** Move construction / assignment */
		FTreeItemID(FTreeItemID&& Other)
		{
			*this = MoveTemp(Other);
		}

		FTreeItemID& operator=(FTreeItemID&& Other)
		{
			FMemory::Memswap(this, &Other, sizeof(FTreeItemID));
			return *this;
		}

		~FTreeItemID()
		{
			CachedHash = 0;
		}

		friend bool operator==(const FTreeItemID& One, const FTreeItemID& Other)
		{
			return One.CachedHash == Other.CachedHash;
		}
		friend bool operator!=(const FTreeItemID& One, const FTreeItemID& Other)
		{
			return One.CachedHash != Other.CachedHash;
		}

		friend uint32 GetTypeHash(const FTreeItemID& ItemID)
		{
			return ItemID.CachedHash;
		}

		void SetCachedHash(const uint32 InHash)
		{
			CachedHash = InHash;
		}
	private:
		FObjectKey ObjectKey;
		uint32 CachedHash;
	};
};
