// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D12Residency.h: D3D memory residency functions.
=============================================================================*/

#pragma once

#if PLATFORM_XBOXONE
static_assert(ENABLE_RESIDENCY_MANAGEMENT == 0, "Xbox One doesn't need memory residency management. Please disable it.");
namespace D3DX12Residency
{
	class ManagedObject {};
	class ResidencySet {};
	class ResidencyManager {};

	class IDXGIAdapter3;
}
#else
#include "D3D12Util.h"
#include "AllowWindowsPlatformTypes.h"
#include "dxgi1_6.h"
#pragma warning(push)
#pragma warning(disable : 6031)
	#include <D3DX12Residency.h>
#pragma warning(pop)
#include "HideWindowsPlatformTypes.h"
#endif

namespace D3DX12Residency
{
	inline void Initialize(ManagedObject& Object, ID3D12Pageable* pResource, uint64 ObjectSize)
	{
#if ENABLE_RESIDENCY_MANAGEMENT
		Object.Initialize(pResource, ObjectSize);
#endif
	}

	inline bool IsInitialized(ManagedObject& Object)
	{
#if ENABLE_RESIDENCY_MANAGEMENT
		return Object.IsInitialized();
#else
		return false;
#endif
	}

	inline bool IsInitialized(ManagedObject* pObject)
	{
#if ENABLE_RESIDENCY_MANAGEMENT
		return pObject && IsInitialized(*pObject);
#else
		return false;
#endif
	}

	inline void BeginTrackingObject(ResidencyManager& ResidencyManager, ManagedObject& Object)
	{
#if ENABLE_RESIDENCY_MANAGEMENT
		ResidencyManager.BeginTrackingObject(&Object);
#endif
	}

	inline void EndTrackingObject(ResidencyManager& ResidencyManager, ManagedObject& Object)
	{
#if ENABLE_RESIDENCY_MANAGEMENT
		ResidencyManager.EndTrackingObject(&Object);
#endif
	}

	inline void InitializeResidencyManager(ResidencyManager& ResidencyManager, ID3D12Device* Device, uint32 DeviceNodeMask, IDXGIAdapter3* Adapter, uint32 MaxLatency)
	{
#if ENABLE_RESIDENCY_MANAGEMENT
		VERIFYD3D12RESULT(ResidencyManager.Initialize(Device, DeviceNodeMask, Adapter, MaxLatency));
#endif
	}

	inline void DestroyResidencyManager(ResidencyManager& ResidencyManager)
	{
#if ENABLE_RESIDENCY_MANAGEMENT
		ResidencyManager.Destroy();
#endif
	}

	inline ResidencySet* CreateResidencySet(ResidencyManager& ResidencyManager)
	{
#if ENABLE_RESIDENCY_MANAGEMENT
		return ResidencyManager.CreateResidencySet();
#else
		return nullptr;
#endif
	}

	inline void DestroyResidencySet(ResidencyManager& ResidencyManager, ResidencySet* pSet)
	{
#if ENABLE_RESIDENCY_MANAGEMENT
		if (pSet)
		{
			ResidencyManager.DestroyResidencySet(pSet);
		}
#endif
	}

	inline void Open(ResidencySet* pSet)
	{
#if ENABLE_RESIDENCY_MANAGEMENT
		if (pSet)
		{
			VERIFYD3D12RESULT(pSet->Open());
		}
#endif
	}

	inline void Close(ResidencySet* pSet)
	{
#if ENABLE_RESIDENCY_MANAGEMENT
		if (pSet)
		{
			VERIFYD3D12RESULT(pSet->Close());
		}
#endif
	}

	inline void Insert(ResidencySet& Set, ManagedObject& Object)
	{
#if ENABLE_RESIDENCY_MANAGEMENT
		check(Object.IsInitialized());
		Set.Insert(&Object);
#endif
	}

	inline void Insert(ResidencySet& Set, ManagedObject* pObject)
	{
#if ENABLE_RESIDENCY_MANAGEMENT
		check(pObject && pObject->IsInitialized());
		Set.Insert(pObject);
#endif
	}
}

typedef D3DX12Residency::ManagedObject FD3D12ResidencyHandle;
typedef D3DX12Residency::ResidencySet FD3D12ResidencySet;
typedef D3DX12Residency::ResidencyManager FD3D12ResidencyManager;