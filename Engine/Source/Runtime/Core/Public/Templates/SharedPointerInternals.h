// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/EnableIf.h"
#include "Misc/AssertionMacros.h"
#include "HAL/UnrealMemory.h"
#include "Templates/AndOrNot.h"
#include "Templates/RemoveReference.h"
#include "Templates/TypeCompatibleBytes.h"


/** Default behavior. */
#define	FORCE_THREADSAFE_SHAREDPTRS PLATFORM_WEAKLY_CONSISTENT_MEMORY
#define THREAD_SANITISE_UNSAFEPTR 0

#if THREAD_SANITISE_UNSAFEPTR
	#define TSAN_SAFE_UNSAFEPTR
#else
	#define TSAN_SAFE_UNSAFEPTR TSAN_SAFE
#endif

/**
 * ESPMode is used select between either 'fast' or 'thread safe' shared pointer types.
 * This is only used by templates at compile time to generate one code path or another.
 */
enum class ESPMode
{
	/** Forced to be not thread-safe. */
	NotThreadSafe = 0,

	/**
		*	Fast, doesn't ever use atomic interlocks.
		*	Some code requires that all shared pointers are thread-safe.
		*	It's better to change it here, instead of replacing ESPMode::Fast to ESPMode::ThreadSafe throughout the code.
		*/
	Fast = FORCE_THREADSAFE_SHAREDPTRS ? 1 : 0,

	/** Conditionally thread-safe, never spin locks, but slower */
	ThreadSafe = 1
};


// Forward declarations.  Note that in the interest of fast performance, thread safety
// features are mostly turned off (Mode = ESPMode::Fast).  If you need to access your
// object on multiple threads, you should use ESPMode::ThreadSafe!
template< class ObjectType, ESPMode Mode = ESPMode::Fast > class TSharedRef;
template< class ObjectType, ESPMode Mode = ESPMode::Fast > class TSharedPtr;
template< class ObjectType, ESPMode Mode = ESPMode::Fast > class TWeakPtr;
template< class ObjectType, ESPMode Mode = ESPMode::Fast > class TSharedFromThis;


/**
 * SharedPointerInternals contains internal workings of shared and weak pointers.  You should
 * hopefully never have to use anything inside this namespace directly.
 */
namespace SharedPointerInternals
{
	// Forward declarations
	template< ESPMode Mode > class FWeakReferencer;

	/** Dummy structures used internally as template arguments for typecasts */
	struct FStaticCastTag {};
	struct FConstCastTag {};

	// NOTE: The following is an Unreal extension to standard shared_ptr behavior
	struct FNullTag {};


	class FReferenceControllerBase
	{
	public:
		/** Constructor */
		FORCEINLINE explicit FReferenceControllerBase()
			: SharedReferenceCount(1)
			, WeakReferenceCount(1)
		{
		}

		// NOTE: The primary reason these reference counters are 32-bit values (and not 16-bit to save
		//       memory), is that atomic operations require at least 32-bit objects.

		/** Number of shared references to this object.  When this count reaches zero, the associated object
		    will be destroyed (even if there are still weak references!) */
		int32 SharedReferenceCount;

		/** Number of weak references to this object.  If there are any shared references, that counts as one
		   weak reference too. */
		int32 WeakReferenceCount;

		/** Destroys the object associated with this reference counter.  */
		virtual void DestroyObject() = 0;

		virtual ~FReferenceControllerBase()
		{
		}

	private:
		FReferenceControllerBase( FReferenceControllerBase const& );
		FReferenceControllerBase& operator=( FReferenceControllerBase const& );
	};

	template <typename ObjectType, typename DeleterType>
	class TReferenceControllerWithDeleter : private DeleterType, public FReferenceControllerBase
	{
	public:
		explicit TReferenceControllerWithDeleter(ObjectType* InObject, DeleterType&& Deleter)
			: DeleterType(MoveTemp(Deleter))
			, Object(InObject)
		{
		}

		virtual void DestroyObject() override
		{
			(*static_cast<DeleterType*>(this))(Object);
		}

		// Non-copyable
		TReferenceControllerWithDeleter(const TReferenceControllerWithDeleter&) = delete;
		TReferenceControllerWithDeleter& operator=(const TReferenceControllerWithDeleter&) = delete;

	private:
		/** The object associated with this reference counter.  */
		ObjectType* Object;
	};

	template <typename ObjectType>
	class TIntrusiveReferenceController : public FReferenceControllerBase
	{
	public:
		template <typename... ArgTypes>
		explicit TIntrusiveReferenceController(ArgTypes&&... Args)
		{
			new ((void*)&ObjectStorage) ObjectType(Forward<ArgTypes>(Args)...);
		}

		ObjectType* GetObjectPtr() const
		{
			return (ObjectType*)&ObjectStorage;
		}

		virtual void DestroyObject() override
		{
			DestructItem((ObjectType*)&ObjectStorage);
		}

		// Non-copyable
		TIntrusiveReferenceController(const TIntrusiveReferenceController&) = delete;
		TIntrusiveReferenceController& operator=(const TIntrusiveReferenceController&) = delete;

	private:
		/** The object associated with this reference counter.  */
		mutable TTypeCompatibleBytes<ObjectType> ObjectStorage;
	};


	/** Deletes an object via the standard delete operator */
	template <typename Type>
	struct DefaultDeleter
	{
		FORCEINLINE void operator()(Type* Object) const
		{
			delete Object;
		}
	};

	/** Creates a reference controller which just calls delete */
	template <typename ObjectType>
	inline FReferenceControllerBase* NewDefaultReferenceController(ObjectType* Object)
	{
		return new TReferenceControllerWithDeleter<ObjectType, DefaultDeleter<ObjectType>>(Object, DefaultDeleter<ObjectType>());
	}

	/** Creates a custom reference controller with a specified deleter */
	template <typename ObjectType, typename DeleterType>
	inline FReferenceControllerBase* NewCustomReferenceController(ObjectType* Object, DeleterType&& Deleter)
	{
		return new TReferenceControllerWithDeleter<ObjectType, typename TRemoveReference<DeleterType>::Type>(Object, Forward<DeleterType>(Deleter));
	}

	/** Creates an intrusive reference controller */
	template <typename ObjectType, typename... ArgTypes>
	inline TIntrusiveReferenceController<ObjectType>* NewIntrusiveReferenceController(ArgTypes&&... Args)
	{
		return new TIntrusiveReferenceController<ObjectType>(Forward<ArgTypes>(Args)...);
	}


	/** Proxy structure for implicitly converting raw pointers to shared/weak pointers */
	// NOTE: The following is an Unreal extension to standard shared_ptr behavior
	template< class ObjectType >
	struct FRawPtrProxy
	{
		/** The object pointer */
		ObjectType* Object;

		/** Reference controller used to destroy the object */
		FReferenceControllerBase* ReferenceController;

		/** Construct implicitly from an object */
		FORCEINLINE FRawPtrProxy( ObjectType* InObject )
			: Object             ( InObject )
			, ReferenceController( NewDefaultReferenceController( InObject ) )
		{
		}

		/** Construct implicitly from an object and a custom deleter */
		template< class Deleter >
		FORCEINLINE FRawPtrProxy( ObjectType* InObject, Deleter&& InDeleter )
			: Object             ( InObject )
			, ReferenceController( NewCustomReferenceController( InObject, Forward< Deleter >( InDeleter ) ) )
		{
		}
	};


	/**
	 * FReferenceController is a standalone heap-allocated object that tracks the number of references
	 * to an object referenced by TSharedRef, TSharedPtr or TWeakPtr objects.
	 *
	 * It is specialized for different threading modes.
	 */
	template< ESPMode Mode >
	struct FReferenceControllerOps;

	template<>
	struct FReferenceControllerOps<ESPMode::ThreadSafe>
	{
		/** Returns the shared reference count */
		static FORCEINLINE const int32 GetSharedReferenceCount(const FReferenceControllerBase* ReferenceController)
		{
			// This reference count may be accessed by multiple threads
			return static_cast< int32 const volatile& >( ReferenceController->SharedReferenceCount );
		}

		/** Adds a shared reference to this counter */
		static FORCEINLINE void AddSharedReference(FReferenceControllerBase* ReferenceController)
		{
			FPlatformAtomics::InterlockedIncrement( &ReferenceController->SharedReferenceCount );
		}

		/**
		 * Adds a shared reference to this counter ONLY if there is already at least one reference
		 *
		 * @return  True if the shared reference was added successfully
		 */
		static bool ConditionallyAddSharedReference(FReferenceControllerBase* ReferenceController)
		{
			for( ; ; )
			{
				// Peek at the current shared reference count.  Remember, this value may be updated by
				// multiple threads.
				const int32 OriginalCount = static_cast< int32 const volatile& >( ReferenceController->SharedReferenceCount );
				if( OriginalCount == 0 )
				{
					// Never add a shared reference if the pointer has already expired
					return false;
				}

				// Attempt to increment the reference count.
				const int32 ActualOriginalCount = FPlatformAtomics::InterlockedCompareExchange( &ReferenceController->SharedReferenceCount, OriginalCount + 1, OriginalCount );

				// We need to make sure that we never revive a counter that has already expired, so if the
				// actual value what we expected (because it was touched by another thread), then we'll try
				// again.  Note that only in very unusual cases will this actually have to loop.
				if( ActualOriginalCount == OriginalCount )
				{
					return true;
				}
			}
		}

		/** Releases a shared reference to this counter */
		static FORCEINLINE void ReleaseSharedReference(FReferenceControllerBase* ReferenceController)
		{
			checkSlow( ReferenceController->SharedReferenceCount > 0 );

			if( FPlatformAtomics::InterlockedDecrement( &ReferenceController->SharedReferenceCount ) == 0 )
			{
				// Last shared reference was released!  Destroy the referenced object.
				ReferenceController->DestroyObject();

				// No more shared referencers, so decrement the weak reference count by one.  When the weak
				// reference count reaches zero, this object will be deleted.
				ReleaseWeakReference(ReferenceController);
			}
		}


		/** Adds a weak reference to this counter */
		static FORCEINLINE void AddWeakReference(FReferenceControllerBase* ReferenceController)
		{
			FPlatformAtomics::InterlockedIncrement( &ReferenceController->WeakReferenceCount );
		}

		/** Releases a weak reference to this counter */
		static void ReleaseWeakReference(FReferenceControllerBase* ReferenceController)
		{
			checkSlow( ReferenceController->WeakReferenceCount > 0 );

			if( FPlatformAtomics::InterlockedDecrement( &ReferenceController->WeakReferenceCount ) == 0 )
			{
				// No more references to this reference count.  Destroy it!
				delete ReferenceController;
			}
		}
	};


	template<>
	struct FReferenceControllerOps<ESPMode::NotThreadSafe>
	{
		/** Returns the shared reference count */
		static FORCEINLINE const int32 GetSharedReferenceCount(const FReferenceControllerBase* ReferenceController) TSAN_SAFE_UNSAFEPTR
		{
			return ReferenceController->SharedReferenceCount;
		}

		/** Adds a shared reference to this counter */
		static FORCEINLINE void AddSharedReference(FReferenceControllerBase* ReferenceController) TSAN_SAFE_UNSAFEPTR
		{
			++ReferenceController->SharedReferenceCount;
		}

		/**
		 * Adds a shared reference to this counter ONLY if there is already at least one reference
		 *
		 * @return  True if the shared reference was added successfully
		 */
		static bool ConditionallyAddSharedReference(FReferenceControllerBase* ReferenceController) TSAN_SAFE_UNSAFEPTR
		{
			if( ReferenceController->SharedReferenceCount == 0 )
			{
				// Never add a shared reference if the pointer has already expired
				return false;
			}

			++ReferenceController->SharedReferenceCount;
			return true;
		}

		/** Releases a shared reference to this counter */
		static FORCEINLINE void ReleaseSharedReference(FReferenceControllerBase* ReferenceController) TSAN_SAFE_UNSAFEPTR
		{
			checkSlow( ReferenceController->SharedReferenceCount > 0 );

			if( --ReferenceController->SharedReferenceCount == 0 )
			{
				// Last shared reference was released!  Destroy the referenced object.
				ReferenceController->DestroyObject();

				// No more shared referencers, so decrement the weak reference count by one.  When the weak
				// reference count reaches zero, this object will be deleted.
				ReleaseWeakReference(ReferenceController);
			}
		}

		/** Adds a weak reference to this counter */
		static FORCEINLINE void AddWeakReference(FReferenceControllerBase* ReferenceController) TSAN_SAFE_UNSAFEPTR
		{
			++ReferenceController->WeakReferenceCount;
		}

		/** Releases a weak reference to this counter */
		static void ReleaseWeakReference(FReferenceControllerBase* ReferenceController) TSAN_SAFE_UNSAFEPTR
		{
			checkSlow( ReferenceController->WeakReferenceCount > 0 );

			if( --ReferenceController->WeakReferenceCount == 0 )
			{
				// No more references to this reference count.  Destroy it!
				delete ReferenceController;
			}
		}
	};


	/**
	 * FSharedReferencer is a wrapper around a pointer to a reference controller that is used by either a
	 * TSharedRef or a TSharedPtr to keep track of a referenced object's lifetime
	 */
	template< ESPMode Mode >
	class FSharedReferencer
	{
		typedef FReferenceControllerOps<Mode> TOps;

	public:

		/** Constructor for an empty shared referencer object */
		FORCEINLINE FSharedReferencer()
			: ReferenceController( nullptr )
		{ }

		/** Constructor that counts a single reference to the specified object */
		inline explicit FSharedReferencer( FReferenceControllerBase* InReferenceController )
			: ReferenceController( InReferenceController )
		{ }
		
		/** Copy constructor creates a new reference to the existing object */
		FORCEINLINE FSharedReferencer( FSharedReferencer const& InSharedReference )
			: ReferenceController( InSharedReference.ReferenceController )
		{
			// If the incoming reference had an object associated with it, then go ahead and increment the
			// shared reference count
			if( ReferenceController != nullptr )
			{
				TOps::AddSharedReference(ReferenceController);
			}
		}

		/** Move constructor creates no new references */
		FORCEINLINE FSharedReferencer( FSharedReferencer&& InSharedReference )
			: ReferenceController( InSharedReference.ReferenceController )
		{
			InSharedReference.ReferenceController = nullptr;
		}

		/** Creates a shared referencer object from a weak referencer object.  This will only result
		    in a valid object reference if the object already has at least one other shared referencer. */
		FSharedReferencer( FWeakReferencer< Mode > const& InWeakReference )
			: ReferenceController( InWeakReference.ReferenceController )
		{
			// If the incoming reference had an object associated with it, then go ahead and increment the
			// shared reference count
			if( ReferenceController != nullptr )
			{
				// Attempt to elevate a weak reference to a shared one.  For this to work, the object this
				// weak counter is associated with must already have at least one shared reference.  We'll
				// never revive a pointer that has already expired!
				if( !TOps::ConditionallyAddSharedReference(ReferenceController) )
				{
					ReferenceController = nullptr;
				}
			}
		}

		/** Destructor. */
		FORCEINLINE ~FSharedReferencer()
		{
			if( ReferenceController != nullptr )
			{
				// Tell the reference counter object that we're no longer referencing the object with
				// this shared pointer
				TOps::ReleaseSharedReference(ReferenceController);
			}
		}

		/** Assignment operator adds a reference to the assigned object.  If this counter was previously
		    referencing an object, that reference will be released. */
		inline FSharedReferencer& operator=( FSharedReferencer const& InSharedReference )
		{
			// Make sure we're not be reassigned to ourself!
			auto NewReferenceController = InSharedReference.ReferenceController;
			if( NewReferenceController != ReferenceController )
			{
				// First, add a shared reference to the new object
				if( NewReferenceController != nullptr )
				{
					TOps::AddSharedReference(NewReferenceController);
				}

				// Release shared reference to the old object
				if( ReferenceController != nullptr )
				{
					TOps::ReleaseSharedReference(ReferenceController);
				}

				// Assume ownership of the assigned reference counter
				ReferenceController = NewReferenceController;
			}

			return *this;
		}

		/** Move assignment operator adds no references to the assigned object.  If this counter was previously
		    referencing an object, that reference will be released. */
		inline FSharedReferencer& operator=( FSharedReferencer&& InSharedReference )
		{
			// Make sure we're not be reassigned to ourself!
			auto NewReferenceController = InSharedReference.ReferenceController;
			auto OldReferenceController = ReferenceController;
			if( NewReferenceController != OldReferenceController )
			{
				// Assume ownership of the assigned reference counter
				InSharedReference.ReferenceController = nullptr;
				ReferenceController                   = NewReferenceController;

				// Release shared reference to the old object
				if( OldReferenceController != nullptr )
				{
					TOps::ReleaseSharedReference(OldReferenceController);
				}
			}

			return *this;
		}

		/**
		 * Tests to see whether or not this shared counter contains a valid reference
		 *
		 * @return  True if reference is valid
		 */
		FORCEINLINE const bool IsValid() const
		{
			return ReferenceController != nullptr;
		}

		/**
		 * Returns the number of shared references to this object (including this reference.)
		 *
		 * @return  Number of shared references to the object (including this reference.)
		 */
		FORCEINLINE const int32 GetSharedReferenceCount() const
		{
			return ReferenceController != nullptr ? TOps::GetSharedReferenceCount(ReferenceController) : 0;
		}

		/**
		 * Returns true if this is the only shared reference to this object.  Note that there may be
		 * outstanding weak references left.
		 *
		 * @return  True if there is only one shared reference to the object, and this is it!
		 */
		FORCEINLINE const bool IsUnique() const
		{
			return GetSharedReferenceCount() == 1;
		}

	private:

 		// Expose access to ReferenceController to FWeakReferencer
		template< ESPMode OtherMode > friend class FWeakReferencer;

	private:

		/** Pointer to the reference controller for the object a shared reference/pointer is referencing */
		FReferenceControllerBase* ReferenceController;
	};


	/**
	 * FWeakReferencer is a wrapper around a pointer to a reference controller that is used
	 * by a TWeakPtr to keep track of a referenced object's lifetime.
	 */
	template< ESPMode Mode >
	class FWeakReferencer
	{
		typedef FReferenceControllerOps<Mode> TOps;

	public:

		/** Default constructor with empty counter */
		FORCEINLINE FWeakReferencer()
			: ReferenceController( nullptr )
		{ }

		/** Construct a weak referencer object from another weak referencer */
		FORCEINLINE FWeakReferencer( FWeakReferencer const& InWeakRefCountPointer )
			: ReferenceController( InWeakRefCountPointer.ReferenceController )
		{
			// If the weak referencer has a valid controller, then go ahead and add a weak reference to it!
			if( ReferenceController != nullptr )
			{
				TOps::AddWeakReference(ReferenceController);
			}
		}

		/** Construct a weak referencer object from an rvalue weak referencer */
		FORCEINLINE FWeakReferencer( FWeakReferencer&& InWeakRefCountPointer )
			: ReferenceController( InWeakRefCountPointer.ReferenceController )
		{
			InWeakRefCountPointer.ReferenceController = nullptr;
		}

		/** Construct a weak referencer object from a shared referencer object */
		FORCEINLINE FWeakReferencer( FSharedReferencer< Mode > const& InSharedRefCountPointer )
			: ReferenceController( InSharedRefCountPointer.ReferenceController )
		{
			// If the shared referencer had a valid controller, then go ahead and add a weak reference to it!
			if( ReferenceController != nullptr )
			{
				TOps::AddWeakReference(ReferenceController);
			}
		}

		/** Destructor. */
		FORCEINLINE ~FWeakReferencer()
		{
			if( ReferenceController != nullptr )
			{
				// Tell the reference counter object that we're no longer referencing the object with
				// this weak pointer
				TOps::ReleaseWeakReference(ReferenceController);
			}
		}
		
		/** Assignment operator from a weak referencer object.  If this counter was previously referencing an
		    object, that reference will be released. */
		FORCEINLINE FWeakReferencer& operator=( FWeakReferencer const& InWeakReference )
		{
			AssignReferenceController( InWeakReference.ReferenceController );

			return *this;
		}

		/** Assignment operator from an rvalue weak referencer object.  If this counter was previously referencing an
		    object, that reference will be released. */
		FORCEINLINE FWeakReferencer& operator=( FWeakReferencer&& InWeakReference )
		{
			auto OldReferenceController         = ReferenceController;
			ReferenceController                 = InWeakReference.ReferenceController;
			InWeakReference.ReferenceController = nullptr;
			if( OldReferenceController != nullptr )
			{
				TOps::ReleaseWeakReference(OldReferenceController);
			}

			return *this;
		}

		/** Assignment operator from a shared reference counter.  If this counter was previously referencing an
		   object, that reference will be released. */
		FORCEINLINE FWeakReferencer& operator=( FSharedReferencer< Mode > const& InSharedReference )
		{
			AssignReferenceController( InSharedReference.ReferenceController );

			return *this;
		}

		/**
		 * Tests to see whether or not this weak counter contains a valid reference
		 *
		 * @return  True if reference is valid
		 */
		FORCEINLINE const bool IsValid() const
		{
			return ReferenceController != nullptr && TOps::GetSharedReferenceCount(ReferenceController) > 0;
		}

	private:

		/** Assigns a new reference controller to this counter object, first adding a reference to it, then
		    releasing the previous object. */
		inline void AssignReferenceController( FReferenceControllerBase* NewReferenceController )
		{
			// Only proceed if the new reference counter is different than our current
			if( NewReferenceController != ReferenceController )
			{
				// First, add a weak reference to the new object
				if( NewReferenceController != nullptr )
				{
					TOps::AddWeakReference(NewReferenceController);
				}

				// Release weak reference to the old object
				if( ReferenceController != nullptr )
				{
					TOps::ReleaseWeakReference(ReferenceController);
				}

				// Assume ownership of the assigned reference counter
				ReferenceController = NewReferenceController;
			}
		}

	private:

 		/** Expose access to ReferenceController to FSharedReferencer. */
		template< ESPMode OtherMode > friend class FSharedReferencer;

	private:

		/** Pointer to the reference controller for the object a TWeakPtr is referencing */
		FReferenceControllerBase* ReferenceController;
	};


	/** Templated helper function (const) that creates a shared pointer from an object instance */
	template< class SharedPtrType, class ObjectType, class OtherType, ESPMode Mode >
	FORCEINLINE void EnableSharedFromThis( TSharedPtr< SharedPtrType, Mode > const* InSharedPtr, ObjectType const* InObject, TSharedFromThis< OtherType, Mode > const* InShareable )
	{
		if( InShareable != nullptr )
		{
			InShareable->UpdateWeakReferenceInternal( InSharedPtr, const_cast< ObjectType* >( InObject ) );
		}
	}


	/** Templated helper function that creates a shared pointer from an object instance */
	template< class SharedPtrType, class ObjectType, class OtherType, ESPMode Mode >
	FORCEINLINE void EnableSharedFromThis( TSharedPtr< SharedPtrType, Mode >* InSharedPtr, ObjectType const* InObject, TSharedFromThis< OtherType, Mode > const* InShareable )
	{
		if( InShareable != nullptr )
		{
			InShareable->UpdateWeakReferenceInternal( InSharedPtr, const_cast< ObjectType* >( InObject ) );
		}
	}


	/** Templated helper function (const) that creates a shared reference from an object instance */
	template< class SharedRefType, class ObjectType, class OtherType, ESPMode Mode >
	FORCEINLINE void EnableSharedFromThis( TSharedRef< SharedRefType, Mode > const* InSharedRef, ObjectType const* InObject, TSharedFromThis< OtherType, Mode > const* InShareable )
	{
		if( InShareable != nullptr )
		{
			InShareable->UpdateWeakReferenceInternal( InSharedRef, const_cast< ObjectType* >( InObject ) );
		}
	}


	/** Templated helper function that creates a shared reference from an object instance */
	template< class SharedRefType, class ObjectType, class OtherType, ESPMode Mode >
	FORCEINLINE void EnableSharedFromThis( TSharedRef< SharedRefType, Mode >* InSharedRef, ObjectType const* InObject, TSharedFromThis< OtherType, Mode > const* InShareable )
	{
		if( InShareable != nullptr )
		{
			InShareable->UpdateWeakReferenceInternal( InSharedRef, const_cast< ObjectType* >( InObject ) );
		}
	}


	/** Templated helper catch-all function, accomplice to the above helper functions */
	FORCEINLINE void EnableSharedFromThis( ... ) { }
}
