/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_CO_DX12_ASYNC_MANAGER_H
#define NV_CO_DX12_ASYNC_MANAGER_H

#include <Nv/Common/NvCoFreeListHeap.h>
#include <Nv/Common/Container/NvCoHandleMap.h>

namespace nvidia {
namespace Common { 

/*! 
\brief Base class for types that will be managed by the Dx12AyncManager. 
Dx12Async derived types are held in singly linked lists (tracked with m_next) inside of the Dx12AsyncManager.
\see Dx12AsyncManager to describe usage. */
struct Dx12Async
{
	enum State
	{
		STATE_NEW,					///< Initial state when created. Will remain in this state until onSync has been added, then moves to PENDING
		STATE_PENDING,				///< State whilst waiting for the GPU  
		STATE_COMPLETED,			///< The GPU has passed the sync point so GPU work has been done. Typically work needs to happen when transitioning from PENDING to COMPLETED
		STATE_UNKNOWN,				///< Mark to indicate it not being managed any longer and it is not in any list.
		STATE_COUNT_OF,
	};

		/// True if completed
	NV_FORCE_INLINE Bool isCompleted() const { return m_state == STATE_COMPLETED; }
	
	UInt8 m_type;					///< The type of async
	UInt8 m_state;					///< The state it is currently in
	UInt32 m_totalSize;				///< Total size of this type
	Int32 m_refCount;				///< The amount of references on this result
	Int32 m_handleIndex;			///< Handle index in the handle map
	Int32 m_uniqueCount;			///< Unique identifying count

	Void* m_owner;					///< The thing this belongs to (this is opaque to the manager can can be used however is required)
	Dx12Async* m_next;				///< Next in singly linked list
	UInt64 m_completedValue;		///< The value that needs to be hit to complete
};

/*! \brief The Dx12AsyncManager provides facilities to help handle results that may be returned by the CPU at some later point.
 
In principal the kind of scenarios it is designed to work with, is when you request something from the GPU, that the GPU 
handling of the transaction is added to a command list and a fence is added at some point post when all the work to get the
data from the GPU has completed. The manager keeps track of types that are pending on the fence completing, once they have completed.

To have a type managed by the Dx12AsyncManager it should derive from the Dx12Async base class. It should also have an enum defined called 
'TYPE' which is a unique integer that identifies the type. For example

\code{.cpp}
enum 
{
	DX12_ASYNC_GET_MATRIX,
	DX12_ASYNC_GET_STATUS,
};

struct Dx12GetMatrixAsync: public Dx12Async
{
	// Identifies the type index used with this type
	enum { TYPE = DX12_ASYNC_GET_MATRIX };

	// Store any data needed here
};
\endcode

If your async type needs a variable amount of data stored with it, you can pass a size to it's creation such that 
data can be held after the type. 

The updateCompleted, addSync work in the same way as other Dx12 functions. Call addSync with a sync value some time after
the command list has been submitted with all the work needed is done. Call updateCompleted regularly with the fence position
for the Manager to handle state changes of the types. updateCompleted actually returns the end of the list of asyncs that 
have transitioned from 'pending' to 'completed'. Thus an app can do the following

\code{.cpp}
Dx12Async* end = m_manager.updateCompleted(completedValue);
Dx12Async* cur = m_manager.getStart(Dx12Async::STATUS_COMPLETED);

for (; cur != end; cur = cur->m_next)
{
	// cur has transitioned from pending to completed -> may be a good time to copy data from readBack memory etc
} 
\endcode

The manager also takes into account ref counting of Async requests, such that if a request that is 'new' matches it 
can returned more than once. In typical client usage would look something like

\code{.cpp}
{
	// You can always test the status of an async with the m_status member
	Dx12Async* asyncIn = ...;  // This identifies the async type you are interested in. Often it's passes around as an opaque 'cookie'.

	// Will test state, and cast
	Dx12GetMatrixAsync* async = m_manager.getCompleted<Dx12GetMatrixAsync>(asyncIn);

	if (async)
	{
		// Do something with the result
		resultOut = async->m_result;

		// Tell the manager a request that used this async has completed, so it can be released
		// Internally the async is ref counted, so if there are multiple requests on this async
		// the release will need to be called multiple times for it to actually release.
		m_manager.releaseCompleted(async);
	}
}
\endcode

The Dx12AsyncManager uses the same syncing idiom (updateCompleted, and addSync) used by Dx12CircularResourceHeap. 
\see  Dx12CircularResourceHeap
*/
class Dx12AsyncManager
{
	NV_CO_DECLARE_CLASS_BASE(Dx12AsyncManager);
	typedef HandleMapHandle Handle;

	typedef Dx12Async::State State;

		// Get a handle 
	Handle getHandle(const Dx12Async* async) const { return m_handleMap.getHandleByIndex(async->m_handleIndex); }
		/// Get by a handle. Returns NV_NULL if not found
	Dx12Async* getByHandle(Handle handle) const { return m_handleMap.get(handle); }

		/// Call with current fence value
		/// Returns the end of the entries newly added to the completed list. 
		/// Iterate between getStart(Async::STATE_COMPLETED) and this to set one off state on completed transition.
	Dx12Async* updateCompleted(UInt64 completedValue);
		/// Add sync with new value that must be hit or surpassed to indicate the pending work is done
	Void addSync(UInt64 signalValue);

		/// Get the start of a list holding state
	NV_FORCE_INLINE Dx12Async* getStart(State state) const { return m_lists[state]; }

		/// Destroys all entries associated with owner, and returns amount of asyncs associated
	IndexT onOwnerDestroyed(Void* owner);

		/// Deletes any asyncs with the instance id
		/// Returns number released
	IndexT onOwnerDestroyed(State state, Void* owner);
		
		/// Detach the async from any list it is currently in
	Void detach(Dx12Async* async);

		/// Releases an async (decreases ref count and frees)
	Void releaseCompleted(Dx12Async* async);
		/// Release (decreases ref count, and destroys if all refs are gone)
	Void release(Dx12Async* async);
		/// Destroys irrespective of ref count
	Void destroy(Dx12Async* async);

		/// True if this is a valid async
	Bool isValid(const Dx12Async* async) const { return m_heap.isValidAllocation(async); }

		/// True if the handle is valid
	Bool isValid(Handle handle) { return m_handleMap.isValid(handle); }

		/// Find an async with the specified type and owner
	Dx12Async* find(State state, Int type, Void* owner, Int32 uniqueCount) const;
	template <typename T>
	NV_FORCE_INLINE T* find(State state, Void* owner, Int32 uniqueCount) const { return static_cast<T*>(find(state, Int(T::TYPE), owner, uniqueCount)); }

		/// Create a new async
	Dx12Async* create(Int type, Void* owner, Int32 uniqueCount, SizeT size);
	template <typename T>
	NV_FORCE_INLINE T* create(Void* owner, Int32 uniqueCount, SizeT size = sizeof(T)) { NV_CORE_ASSERT(sizeof(T) <= size); return static_cast<T*>(create(Int(T::TYPE), owner, uniqueCount, size)); }
	
		/// Gets completed using handle
	NV_FORCE_INLINE Dx12Async* get(Int type, Handle handle);
	template <typename T>
	NV_FORCE_INLINE T* get(Handle handle) { return static_cast<T*>(get(Int(T::TYPE), handle)); }

		/// Find an async with the specified type and owner
	NV_INLINE Dx12Async* findAndRef(State state, Int type, Void* owner, Int32 uniqueCount) const;
	template <typename T>
	NV_FORCE_INLINE T* findAndRef(State state, Void* owner, Int32 uniqueCount) const { return static_cast<T*>(findAndRef(state, Int(T::TYPE), owner, uniqueCount)); }

		/*! Cancel the list of asyncs handled by the manager  
		\param asyncs List of asyncs (or NV_NULL) to be canceled
		\param numHandles The amount of handles in the list 
		\param allReferences If true all references will be lost, otherwise each handle will lose a single reference
		\return Returns the number of _references_ lost */
	IndexT cancel(Dx12Async*const* asyncs, IndexT numHandles, Bool allReferences);
	/*! Cancel the list of asyncs handled by the manager
		\param asyncs List of handles to be canceled
		\param numHandles The amount of handles in the list
		\param allReferences If true all references will be lost, otherwise each handle will lose a single reference
		\return Returns the number of _references_ lost */
	IndexT cancel(const Handle * handles, IndexT numHandles, Bool allReferences);

		/*! Cancel all asyncs referencing the owner. 
		\param allReferences If true all references will be lost, otherwise each handle will lose a single reference
		\return Returns the number of _references_ lost */
	IndexT cancel(Void* owner, Bool allReferences);

		/*! A helper function for handling completion. 
		 Performs the cast to the type, and returns appropriate error taking into account asyncRepeat if there is failure
		\param asyncInOut TThe 
		\return Returns a Result. NV_FAILED() will be true if should exit. 
		*/ 
	template <typename T>
	Result complete(Handle* asyncInOut, Bool asyncRepeat, T** asyncOut) { return complete(Int(T::TYPE), asyncInOut, asyncRepeat, (Dx12Async**)asyncOut); }
	Result complete(Int type, Handle* asyncInOut, Bool asyncRepeat, Dx12Async** asyncOut);

		/// Ctor
	Dx12AsyncManager(); 

	protected:
	IndexT _cancel(Dx12Async::State state, Void* owner, Bool allReferences);
	Void _destroy(Dx12Async* async);

	HandleMap<Dx12Async> m_handleMap;					///< Maps handles to asyncs
	Dx12Async* m_lists[Dx12Async::STATE_COUNT_OF];		/// The lists for different states
	FreeListHeap m_heap;								///< Storage for asyncs
};

// ---------------------------------------------------------------------------
Dx12Async* Dx12AsyncManager::get(Int type, Handle handle)
{
	Dx12Async* async = getByHandle(handle);
	if (async)
	{
		NV_CORE_ASSERT(async->m_type == type);
		return (async->m_type == type) ? async : NV_NULL;
	}
	else
	{
		return NV_NULL;
	}
}
// ---------------------------------------------------------------------------
Dx12Async* Dx12AsyncManager::findAndRef(State state, Int type, Void* owner, Int32 uniqueCount) const
{
	Dx12Async* async = find(state, type, owner, uniqueCount); 
	if (async) 
	{
		async->m_refCount++;
	}
	return async;
}

} // namespace Common
} // namespace nvidia

#endif // NV_CO_DX12_ASYNC_MANAGER_H
