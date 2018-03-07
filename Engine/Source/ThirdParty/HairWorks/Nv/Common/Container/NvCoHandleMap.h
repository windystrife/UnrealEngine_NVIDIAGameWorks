/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_CO_HANDLE_MAP_H
#define NV_CO_HANDLE_MAP_H

#include <Nv/Common/NvCoCommon.h>
#include <Nv/Common/NvCoTypeMacros.h>
#include <Nv/Common/NvCoMemory.h>
#include <Nv/Common/Container/NvCoArray.h>


/** \addtogroup common
@{
*/

namespace nvidia {
namespace Common {

typedef SizeT HandleMapHandle; 

/* Map (and manager) of 'safe' handles to pointers. 

Safe handles are a way of referring to a piece of data safely, such that if that data is removed from the map, it can be 
detected that the handle is no longer valid. 

Pointers are added to the map, and a handle is returned that can be used for subsequent retrieval of the pointer. If the 
handle is removed from the map, if it is referenced again with the old handle NV_NULL will be returned. A handle can be tested 
if it is invalid using isValid. 

The value of 0 is always 'invalid'. The map can only map to non NV_NULL values. NV_NULL in m_data in Entry indicates an unfilled
entry. 

The implementation associates a counter with each slot in the array. When a handle is created (with add), it is made by combining 
the index in the array, and the counter. If the handle is removed the counter is increased, changing the value. If a handles counter
is different from the counter held in the Map, it indicates the Handle is no longer valid.

By design the Map cannot hold NV_NULL, because NV_NULL is used to indicate if an Entry is in use or not.
*/
class HandleMapBase
{
	NV_CO_DECLARE_CLASS_BASE(HandleMapBase);
	typedef HandleMapHandle Handle;

	struct _Iterator;
	typedef _Iterator* Iterator;

#if NV_PTR_IS_32
	enum { COUNT_SHIFT = 24 };
	static const Handle INDEX_MASK = 0x00ffffff;
#else
	enum { COUNT_SHIFT = 32 + 16 };
	static const Handle INDEX_MASK = NV_UINT64(0x0000ffffffffffff);
#endif
	static const Handle COUNT_VALUE = SizeT(1) << COUNT_SHIFT;
	static const Handle COUNT_MASK = ~INDEX_MASK;

		/// Returns a handle for a pointer. ptr cannot be NV_NULL.
	NV_INLINE Handle add(Void* ptr);
		/// Returns the pointer associated with the handle. If invalid handle, returns NV_NULL
	NV_INLINE Void* get(Handle handle) const;

		/// Adds and returns the index 
	NV_INLINE Handle addIndex(Void* ptr);

		/// Clear all of the members. Doesn't clear the memory though, as we want to keep the counts, such that when new handles are 
		/// generated they will be unique.
	Void clear();

		/// Resets to initial state.
	Void reset();

		/// Get the iterator
	Handle getIterator() const;
		/// True if the iterator is valid
	NV_INLINE Bool isValid(Handle handle) const;
		/// Get the next (only works on a valid iterator)
	Handle getNext(Handle handle) const;

		/// Set the value at the iterator. Value cannot be null.
	Bool set(Handle handle, Void* data);
		/// Remove at the iterator index. Returns true if was removed.
	Bool remove(Handle handle);
		/// Remove by index
	Bool removeByIndex(IndexT index);

		/// Get the total amount of entries
	IndexT getSize() const { return m_entries.getSize() - 1 - m_freeIndices.getSize(); }

		/// Returns the handle for an index. Returns 0, if index is invalid.
	NV_FORCE_INLINE Handle getHandleByIndex(IndexT index) const;

		/// Ctor
	HandleMapBase();

		/// Get the index of a handle
	NV_FORCE_INLINE static IndexT getIndex(Handle handle) { return IndexT(handle & INDEX_MASK); }
		/// Get the count of a handle
	NV_FORCE_INLINE static SizeT getCount(Handle handle) { return SizeT(handle >> COUNT_SHIFT); }

		/// Make a handle
	NV_FORCE_INLINE static Handle makeHandle(IndexT index, SizeT count) { return (count << COUNT_SHIFT) | index; }

#if NV_DEBUG
	static Void selfTest();
#endif

	protected:
	struct Entry
	{
			/// Change the count
		NV_FORCE_INLINE Void changeCount() { m_handle += COUNT_VALUE; }
			/// True if the handle matches 
		NV_FORCE_INLINE Bool isMatch(Handle handle) const { return m_handle == handle; }

		Void* m_data;			///< NV_NULL Indicates not used
		Handle m_handle;			
	};

	Array<Entry> m_entries;
	Array<IndexT> m_freeIndices;
};

// ---------------------------------------------------------------------------
NV_INLINE HandleMapHandle HandleMapBase::addIndex(Void* ptr)
{
	NV_CORE_ASSERT(ptr);
	if (m_freeIndices.getSize() > 0)
	{
		const IndexT index = m_freeIndices.back();
		m_freeIndices.popBack();
		Entry& entry = m_entries[index];
		entry.m_data = ptr;
		return index;
	}
	else
	{
		const IndexT index = m_entries.getSize();
		Entry& entry = m_entries.expandOne();
		entry.m_handle = Handle(index);
		entry.m_data = ptr;
		return index;
	}
}
// ---------------------------------------------------------------------------
HandleMapBase::Handle HandleMapBase::add(Void* ptr)
{
	NV_CORE_ASSERT(ptr);
	Entry* entry;
	if (m_freeIndices.getSize() > 0)
	{
		const IndexT index = m_freeIndices.back();
		m_freeIndices.popBack();
		entry = &m_entries[index];
	}
	else
	{
		const IndexT index = m_entries.getSize();
		entry = &m_entries.expandOne();		
		entry->m_handle = Handle(index);
	}
	entry->m_data = ptr;
	return entry->m_handle;
}
// ---------------------------------------------------------------------------
Bool HandleMapBase::isValid(Handle handle) const
{
	IndexT index = getIndex(handle);
	return index < m_entries.getSize() && m_entries[index].m_handle == handle;
}
// ---------------------------------------------------------------------------
Void* HandleMapBase::get(Handle handle) const
{
	IndexT index = getIndex(handle);
	if (index < m_entries.getSize())
	{
		const Entry& entry = m_entries[index];
		return entry.m_handle == handle ? entry.m_data : NV_NULL;
	}
	return NV_NULL;
}
// ---------------------------------------------------------------------------
HandleMapBase::Handle HandleMapBase::getHandleByIndex(IndexT index) const
{
	if (index > 0 && index < m_entries.getSize())
	{
		const Entry& entry = m_entries[index];
		return entry.m_data ? entry.m_handle : 0;
	}
	return 0;
}

/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! HandleMap !!!!!!!!!!!!!!!!!!!!!!!!!!!!! */

template <typename T>
class HandleMap: public HandleMapBase
{
	public:
	typedef HandleMapBase Parent;
	
		/// Add a pointer to the map. Returns the new handle that is associated with the pointer. 
		/// \return Returns a unique handle associated with the pointer.
	NV_FORCE_INLINE Handle add(T* ptr) { return Parent::add((Void*)SizeT(ptr)); }
		/// For a given handle returns the associated pointer, or NV_NULL if the handle isn't in the map.
	NV_FORCE_INLINE T* get(Handle handle) const { return (T*)Parent::get(handle); }
		/// Sets the pointer associated with a given handle. If the handle is invalid, the set will be silently ignored 
		/// \return True if the handle was set (ie the handle must have been valid)
	NV_FORCE_INLINE Bool set(Handle handle, T* ptr) { return Parent::set(handle, (Void*)SizeT(ptr)); }
};

} // namespace Common 
} // namespace nvidia

/** @} */

#endif // NV_CO_HANDLE_MAP_H