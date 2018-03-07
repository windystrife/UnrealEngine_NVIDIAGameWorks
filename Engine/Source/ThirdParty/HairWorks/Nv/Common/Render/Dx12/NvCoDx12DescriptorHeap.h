/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_CO_DX12_DESCRIPTOR_HEAP_H
#define NV_CO_DX12_DESCRIPTOR_HEAP_H

#include <Nv/Common/NvCoCommon.h>
#include <Nv/Common/NvCoTypeMacros.h>
#include <Nv/Common/NvCoComPtr.h>

#include <dxgi.h>
#include <d3d12.h>

namespace nvidia {
namespace Common {

/*! \brief A simple class to manage an underlying Dx12 Descriptor Heap. Allocations are made linearly in order. It is not possible to free
individual allocations, but all allocations can be deallocated with 'deallocateAll'. */
class Dx12DescriptorHeap
{
	NV_CO_DECLARE_CLASS_BASE(Dx12DescriptorHeap);

		/// Initialize
	Result init(ID3D12Device* device, Int size, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags);
		/// Initialize with an array of handles copying over the representation
	Result init(ID3D12Device* device, const D3D12_CPU_DESCRIPTOR_HANDLE* handles, Int numHandles, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags);

		/// Get the total amount of descriptors possible on the heap
	NV_FORCE_INLINE Int getSize() const { return m_size; }
		/// Allocate a descriptor. Returns the index, or -1 if none left.
	NV_FORCE_INLINE Int allocate();
		/// Allocate a number of descriptors. Returns the start index (or -1 if not possible)
	NV_FORCE_INLINE Int allocate(Int numDescriptors);

		/// Deallocates all allocations, and starts allocation from the start of the underlying heap again
	NV_FORCE_INLINE Void deallocateAll() { m_currentIndex = 0; }

		/// Get the size of each 
	NV_FORCE_INLINE Int getDescriptorSize() const { return m_descriptorSize; }

		/// Get the GPU heap start
	NV_FORCE_INLINE D3D12_GPU_DESCRIPTOR_HANDLE getGpuStart() const { return m_heap->GetGPUDescriptorHandleForHeapStart(); }
		/// Get the CPU heap start
	NV_FORCE_INLINE D3D12_CPU_DESCRIPTOR_HANDLE getCpuStart() const { return m_heap->GetCPUDescriptorHandleForHeapStart(); }

		/// Get the GPU handle at the specified index
	NV_FORCE_INLINE D3D12_GPU_DESCRIPTOR_HANDLE getGpuHandle(Int index) const;
		/// Get the CPU handle at the specified index
	NV_FORCE_INLINE D3D12_CPU_DESCRIPTOR_HANDLE getCpuHandle(Int index) const;

		/// Get the underlying heap
	NV_FORCE_INLINE ID3D12DescriptorHeap* getHeap() const { return m_heap; }

		/// Ctor
	Dx12DescriptorHeap();

protected:
	ComPtr<ID3D12DescriptorHeap> m_heap;	///< The underlying heap being allocated from
	Int m_size;								///< Total amount of allocations available on the heap
	Int m_currentIndex;						///< The current descriptor
	Int m_descriptorSize;					///< The size of each descriptor
};

// ---------------------------------------------------------------------------
Int Dx12DescriptorHeap::allocate()
{
	NV_CORE_ASSERT(m_currentIndex < m_size);
	if (m_currentIndex < m_size)
	{
		return m_currentIndex++;	
	}
	return -1;
}
// ---------------------------------------------------------------------------
Int Dx12DescriptorHeap::allocate(Int numDescriptors)
{
	NV_CORE_ASSERT(m_currentIndex + numDescriptors <= m_size);
	if (m_currentIndex + numDescriptors <= m_size)
	{
		const Int index = m_currentIndex;
		m_currentIndex += numDescriptors;
		return index;
	}
	return -1;
}
// ---------------------------------------------------------------------------
NV_FORCE_INLINE D3D12_CPU_DESCRIPTOR_HANDLE Dx12DescriptorHeap::getCpuHandle(Int index) const
{ 
	NV_CORE_ASSERT(index >= 0 && index < m_size);  
	D3D12_CPU_DESCRIPTOR_HANDLE start = m_heap->GetCPUDescriptorHandleForHeapStart();
	D3D12_CPU_DESCRIPTOR_HANDLE dst;
	dst.ptr = start.ptr + m_descriptorSize * index;
	return dst;
}	
// ---------------------------------------------------------------------------
NV_FORCE_INLINE D3D12_GPU_DESCRIPTOR_HANDLE Dx12DescriptorHeap::getGpuHandle(Int index) const
{ 
	NV_CORE_ASSERT(index >= 0 && index < m_size);  
	D3D12_GPU_DESCRIPTOR_HANDLE start = m_heap->GetGPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE dst;
	dst.ptr = start.ptr + m_descriptorSize * index;
	return dst;
}

} // namespace Common
} // namespace nvidia


#endif // NV_CO_DX12_DESCRIPTOR_HEAP_H
