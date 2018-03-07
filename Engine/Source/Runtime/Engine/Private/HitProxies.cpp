// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HitProxies.cpp: Hit proxy implementation.
=============================================================================*/

#include "HitProxies.h"
#include "Misc/ScopeLock.h"

/// @cond DOXYGEN_WARNINGS

IMPLEMENT_HIT_PROXY_BASE( HHitProxy, NULL );
IMPLEMENT_HIT_PROXY(HObject,HHitProxy);

/// @endcond

const FHitProxyId FHitProxyId::InvisibleHitProxyId( INDEX_NONE - 1 );


/** The global list of allocated hit proxies, indexed by hit proxy ID. */

class FHitProxyArray
{
	TSparseArray<HHitProxy*> HitProxies;
	FCriticalSection Lock;
public:

	static FHitProxyArray& Get()
	{
		static FHitProxyArray Singleton;
		return Singleton;
	}

	void Remove(int32 Index)
	{
		FScopeLock ScopeLock(&Lock);
		HitProxies.RemoveAt(Index);
	}

	int32 Add(HHitProxy* Proxy)
	{
		FScopeLock ScopeLock(&Lock);
		return HitProxies.Add(Proxy);
	}

	HHitProxy* GetHitProxyById(int32 Index)
	{
		FScopeLock ScopeLock(&Lock);
		if(Index >= 0 && Index < HitProxies.GetMaxIndex() && HitProxies.IsAllocated(Index))
		{
			return HitProxies[Index];
		}
		else
		{
			return NULL;
		}
	}
};

static struct FForceInitHitProxyBeforeMain
{
	FForceInitHitProxyBeforeMain()
	{
		// we don't want this to be initialized by two threads at once, so we will set it up before main starts
		FHitProxyArray::Get();
	}
} ForceInitHitProxyBeforeMain;

FHitProxyId::FHitProxyId(FColor Color)
{
	Index = ((int32)Color.R << 16) | ((int32)Color.G << 8) | ((int32)Color.B << 0);
}

FColor FHitProxyId::GetColor() const
{
	return FColor(
		((Index >> 16) & 0xff),
		((Index >> 8) & 0xff),
		((Index >> 0) & 0xff),
		0
		);
}

HHitProxy::HHitProxy(EHitProxyPriority InPriority):
	Priority(InPriority),
	OrthoPriority(InPriority)
{
	InitHitProxy();
}

HHitProxy::HHitProxy(EHitProxyPriority InPriority, EHitProxyPriority InOrthoPriority):
	Priority(InPriority),
	OrthoPriority(InOrthoPriority)
{
	InitHitProxy();
}

HHitProxy::~HHitProxy()
{
	// Remove this hit proxy from the global array.
	FHitProxyArray::Get().Remove(Id.Index);
}

void HHitProxy::InitHitProxy()
{
	// Allocate an entry in the global hit proxy array for this hit proxy, and use the index as the hit proxy's ID.
	Id = FHitProxyId(FHitProxyArray::Get().Add(this));
}

bool HHitProxy::IsA(HHitProxyType* TestType) const
{
	bool bIsInstance = false;
	for(HHitProxyType* Type = GetType();Type;Type = Type->GetParent())
	{
		if(Type == TestType)
		{
			bIsInstance = true;
			break;
		}
	}
	return bIsInstance;
}

HHitProxy* GetHitProxyById(FHitProxyId Id)
{
	return FHitProxyArray::Get().GetHitProxyById(Id.Index);
}
