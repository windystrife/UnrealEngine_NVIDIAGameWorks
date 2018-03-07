// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
D3D12RHICommon.h: Common D3D12 RHI definitions for Windows.
=============================================================================*/

#pragma once

DECLARE_STATS_GROUP(TEXT("D3D12RHI"), STATGROUP_D3D12RHI, STATCAT_Advanced);

#include "WindowsHWrapper.h"

class FD3D12Adapter;
class FD3D12Device;

// MGPU defines.
#define MAX_NUM_LDA_NODES 4
static const bool GEnableMGPU = false;
typedef uint32 GPUNodeMask;
static const GPUNodeMask GDefaultGPUMask = 1;
static const GPUNodeMask GAllGPUsMask = MAXUINT32;

class FD3D12AdapterChild
{
protected:
	FD3D12Adapter* ParentAdapter;

public:
	FD3D12AdapterChild(FD3D12Adapter* InParent = nullptr) : ParentAdapter(InParent) {}

	FORCEINLINE FD3D12Adapter* GetParentAdapter() const
	{
		// If this fires an object was likely created with a default constructor i.e in an STL container
		// and is therefore an orphan
		check(ParentAdapter != nullptr);
		return ParentAdapter;
	}

	// To be used with delayed setup
	inline void SetParentAdapter(FD3D12Adapter* InParent)
	{
		check(ParentAdapter == nullptr);
		ParentAdapter = InParent;
	}
};

class FD3D12DeviceChild
{
protected:
	FD3D12Device* Parent;

public:
	FD3D12DeviceChild(FD3D12Device* InParent = nullptr) : Parent(InParent) {}

	FORCEINLINE FD3D12Device* GetParentDevice() const
	{
		// If this fires an object was likely created with a default constructor i.e in an STL container
		// and is therefore an orphan
		check(Parent != nullptr);
		return Parent;
	}

	// To be used with delayed setup
	inline void SetParentDevice(FD3D12Device* InParent)
	{
		check(Parent == nullptr);
		Parent = InParent;
	}
};

class FD3D12GPUObject
{
public:
	FD3D12GPUObject(const GPUNodeMask& NodeMask, const GPUNodeMask& VisibiltyMask)
		: m_NodeMask(NodeMask)
		, m_VisibilityMask(VisibiltyMask)
	{
		check(NodeMask != 0);// GPU Objects must have some kind of affinity to a GPU Node
	}

	FORCEINLINE const GPUNodeMask GetNodeMask() const { return m_NodeMask; }
	FORCEINLINE const GPUNodeMask GetVisibilityMask() const { return m_VisibilityMask; }

protected:
	const GPUNodeMask m_NodeMask;
	// Which GPUs have direct access to this object
	const GPUNodeMask m_VisibilityMask;
};

class FD3D12SingleNodeGPUObject : public FD3D12GPUObject
{
public:
	FD3D12SingleNodeGPUObject(const GPUNodeMask& NodeMask)
		: m_DeviceIndex(DetermineGPUIndex(NodeMask))
		, FD3D12GPUObject(NodeMask, NodeMask)
	{}

	static FORCEINLINE const uint32_t DetermineGPUIndex(const GPUNodeMask& NodeMask)
	{
		check(__popcnt(NodeMask) == 1);// Single Node GPU objects must only have 1 bit set in their Node Mask

		unsigned long ReturnValue = 0;
		BitScanForward(&ReturnValue, NodeMask);
		return ReturnValue;
	}

	FORCEINLINE const uint32_t GetNodeIndex() const
	{
		return m_DeviceIndex;
	}

private:
	const uint32 m_DeviceIndex;
};

class FD3D12MultiNodeGPUObject : public FD3D12GPUObject
{
public:
	FD3D12MultiNodeGPUObject(GPUNodeMask NodeMask, GPUNodeMask VisibiltyMask)
		: FD3D12GPUObject(NodeMask, VisibiltyMask)
	{
		check((NodeMask & VisibiltyMask) != 0);// A GPU objects must be visible on the device it belongs to
	}
};

template<typename ObjectType>
class FD3D12LinkedAdapterObject
{
public:
	FD3D12LinkedAdapterObject() {};

	FORCEINLINE void SetNextObject(ObjectType* Object)
	{
		NextNode = Object;
	}

	FORCEINLINE ObjectType* GetNextObject()
	{
		return NextNode.GetReference();
	}

private:

	TRefCountPtr<ObjectType> NextNode;
};