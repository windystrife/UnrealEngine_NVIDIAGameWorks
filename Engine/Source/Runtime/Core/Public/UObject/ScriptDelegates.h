// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Templates/AreTypesEqual.h"
#include "Templates/UnrealTypeTraits.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "UObject/NameTypes.h"
#include "Templates/SharedPointer.h"

struct FWeakObjectPtr;

/**
 * Script delegate base class.
 */
template <typename TWeakPtr = FWeakObjectPtr>
class TScriptDelegate
{
	// Although templated, the parameter is not intended to be anything other than the default,
	// and is only a template for module organization reasons.
	static_assert(TAreTypesEqual<TWeakPtr, FWeakObjectPtr>::Value, "TWeakPtr should not be overridden");

public:

	/** Default constructor. */
	TScriptDelegate() 
		: Object( nullptr ),
		  FunctionName( NAME_None )
	{ }

private:

	template <class UObjectTemplate>
	inline bool IsBound_Internal() const
	{
		if (FunctionName != NAME_None)
		{
			if (UObject* ObjectPtr = Object.Get())
			{
				return ((UObjectTemplate*)ObjectPtr)->FindFunction(FunctionName) != nullptr;
			}
		}

		return false;
	}

public:

	/**
	 * Binds a UFunction to this delegate.
	 *
	 * @param InObject The object to call the function on.
	 * @param InFunctionName The name of the function to call.
	 */
	void BindUFunction( UObject* InObject, const FName& InFunctionName )
	{
		Object = InObject;
		FunctionName = InFunctionName;
	}

	/** 
	 * Checks to see if the user object bound to this delegate is still valid
	 *
	 * @return  True if the object is still valid and it's safe to execute the function call
	 */
	inline bool IsBound() const
	{
		return IsBound_Internal<UObject>();
	}

	/** 
	 * Checks to see if this delegate is bound to the given user object.
	 *
	 * @return	True if this delegate is bound to InUserObject, false otherwise.
	 */
	inline bool IsBoundToObject(void const* InUserObject) const
	{
		return InUserObject && (InUserObject == GetUObject());
	}

	/** 
	 * Checks to see if this delegate is bound to the given user object, even if the object is unreachable.
	 *
	 * @return	True if this delegate is bound to InUserObject, false otherwise.
	 */
	bool IsBoundToObjectEvenIfUnreachable(void const* InUserObject) const
	{
		return InUserObject && InUserObject == GetUObjectEvenIfUnreachable();
	}

	/** 
	 * Checks to see if the user object bound to this delegate will ever be valid again
	 *
	 * @return  True if the object is still valid and it's safe to execute the function call
	 */
	inline bool IsCompactable() const
	{
		return FunctionName == NAME_None || !Object.Get(true);
	}

	/**
	 * Unbinds this delegate
	 */
	void Unbind()
	{
		Object = nullptr;
		FunctionName = NAME_None;
	}

	/**
	 * Unbinds this delegate (another name to provide a similar interface to TMulticastScriptDelegate)
	 */
	void Clear()
	{
		Unbind();
	}

	/**
	 * Converts this delegate to a string representation
	 *
	 * @return	Delegate in string format
	 */
	template <class UObjectTemplate>
	inline FString ToString() const
	{
		if( IsBound() )
		{
			return ((UObjectTemplate*)GetUObject())->GetPathName() + TEXT(".") + GetFunctionName().ToString();
		}
		return TEXT( "<Unbound>" );
	}

	/** Delegate serialization */
	friend FArchive& operator<<( FArchive& Ar, TScriptDelegate& D )
	{
		Ar << D.Object << D.FunctionName;
		return Ar;
	}

	/** Comparison operators */
	FORCEINLINE bool operator==( const TScriptDelegate& Other ) const
	{
		return Object == Other.Object && FunctionName == Other.FunctionName;
	}

	FORCEINLINE bool operator!=( const TScriptDelegate& Other ) const
	{
		return Object != Other.Object || FunctionName != Other.FunctionName;
	}

	void operator=( const TScriptDelegate& Other )
	{
		Object = Other.Object;
		FunctionName = Other.FunctionName;
	}

	/** 
	 * Gets the object bound to this delegate
	 *
	 * @return	The object
	 */
	UObject* GetUObject()
	{
		// Downcast UObjectBase to UObject
		return static_cast< UObject* >( Object.Get() );
	}

	/**
	 * Gets the object bound to this delegate (const)
	 *
	 * @return	The object
	 */
	const UObject* GetUObject() const
	{
		// Downcast UObjectBase to UObject
		return static_cast< const UObject* >( Object.Get() );
	}

	/** 
	 * Gets the object bound to this delegate, even if the object is unreachable
	 *
	 * @return	The object
	 */
	UObject* GetUObjectEvenIfUnreachable()
	{
		// Downcast UObjectBase to UObject
		return static_cast< UObject* >( Object.GetEvenIfUnreachable() );
	}

	/**
	 * Gets the object bound to this delegate (const), even if the object is unreachable
	 *
	 * @return	The object
	 */
	const UObject* GetUObjectEvenIfUnreachable() const
	{
		// Downcast UObjectBase to UObject
		return static_cast< const UObject* >( Object.GetEvenIfUnreachable() );
	}

	/**
	 * Gets the name of the function to call on the bound object
	 *
	 * @return	Function name
	 */
	FName GetFunctionName() const
	{
		return FunctionName;
	}

	/**
	 * Executes a delegate by calling the named function on the object bound to the delegate.  You should
	 * always first verify that the delegate is safe to execute by calling IsBound() before calling this function.
	 * In general, you should never call this function directly.  Instead, call Execute() on a derived class.
	 *
	 * @param	Parameters		Parameter structure
	 */
	//CORE_API void ProcessDelegate(void* Parameters) const;
	template <class UObjectTemplate>
	void ProcessDelegate( void* Parameters ) const
	{
		checkf( Object.IsValid() != false, TEXT( "ProcessDelegate() called with no object bound to delegate!" ) );
		checkf( FunctionName != NAME_None, TEXT( "ProcessDelegate() called with no function name set!" ) );

		// Object was pending kill, so we cannot execute the delegate.  Note that it's important to assert
		// here and not simply continue execution, as memory may be left uninitialized if the delegate is
		// not able to execute, resulting in much harder-to-detect code errors.  Users should always make
		// sure IsBound() returns true before calling ProcessDelegate()!
		UObjectTemplate* ObjectPtr = static_cast< UObjectTemplate* >( Object.Get() );	// Down-cast
		checkSlow( !ObjectPtr->IsPendingKill() );

		// Object *must* implement the specified function
		UFunction* Function = ObjectPtr->FindFunctionChecked( FunctionName );

		// Execute the delegate!
		ObjectPtr->ProcessEvent(Function, Parameters);
	}

protected:

	/** The object bound to this delegate, or nullptr if no object is bound */
	TWeakPtr Object;

	/** Name of the function to call on the bound object */
	FName FunctionName;

	// 
	friend class FCallDelegateHelper;
};


template<typename TWeakPtr> struct TIsZeroConstructType<TScriptDelegate<TWeakPtr> > { enum { Value = TIsZeroConstructType<TWeakPtr>::Value }; };


/**
 * Script multi-cast delegate base class
 */
template <typename TWeakPtr = FWeakObjectPtr>
class TMulticastScriptDelegate
{
public:

	/**
	 * Default constructor
	 */
	inline TMulticastScriptDelegate() { }

public:

	/**
	 * Checks to see if any functions are bound to this multi-cast delegate
	 *
	 * @return	True if any functions are bound
	 */
	inline bool IsBound() const
	{
		return InvocationList.Num() > 0;
	}

	/**
	 * Checks whether a function delegate is already a member of this multi-cast delegate's invocation list
	 *
	 * @param	InDelegate	Delegate to check
	 * @return	True if the delegate is already in the list.
	 */
	bool Contains( const TScriptDelegate<TWeakPtr>& InDelegate ) const
	{
		return InvocationList.Contains( InDelegate );
	}

	/**
	 * Checks whether a function delegate is already a member of this multi-cast delegate's invocation list
	 *
	 * @param	InObject		Object of the delegate to check
	 * @param	InFunctionName	Function name of the delegate to check
	 * @return	True if the delegate is already in the list.
	 */
	bool Contains( const UObject* InObject, FName InFunctionName ) const
	{
		return InvocationList.ContainsByPredicate( [=]( const TScriptDelegate<TWeakPtr>& Delegate ){
			return Delegate.GetFunctionName() == InFunctionName && Delegate.IsBoundToObjectEvenIfUnreachable(InObject);
		} );
	}

	/**
	 * Adds a function delegate to this multi-cast delegate's invocation list
	 *
	 * @param	InDelegate	Delegate to add
	 */
	void Add( const TScriptDelegate<TWeakPtr>& InDelegate )
	{
		// First check for any objects that may have expired
		CompactInvocationList();

		// Add the delegate
		AddInternal( InDelegate );
	}

	/**
	 * Adds a function delegate to this multi-cast delegate's invocation list if a delegate with the same signature
	 * doesn't already exist in the invocation list
	 *
	 * @param	InDelegate	Delegate to add
	 */
	void AddUnique( const TScriptDelegate<TWeakPtr>& InDelegate )
	{
		// Add the delegate, if possible
		AddUniqueInternal( InDelegate );

		// Then check for any objects that may have expired
		CompactInvocationList();
	}

	/**
	 * Removes a function from this multi-cast delegate's invocation list (performance is O(N)).  Note that the
	 * order of the delegates may not be preserved!
	 *
	 * @param	InDelegate	Delegate to remove
	 */
	void Remove( const TScriptDelegate<TWeakPtr>& InDelegate )
	{
		// Remove the delegate
		RemoveInternal( InDelegate );

		// Check for any delegates that may have expired
		CompactInvocationList();
	}

	/**
	 * Removes a function from this multi-cast delegate's invocation list (performance is O(N)).  Note that the
	 * order of the delegates may not be preserved!
	 *
	 * @param	InObject		Object of the delegate to remove
	 * @param	InFunctionName	Function name of the delegate to remove
	 */
	void Remove( const UObject* InObject, FName InFunctionName )
	{
		// Remove the delegate
		RemoveInternal( InObject, InFunctionName );

		// Check for any delegates that may have expired
		CompactInvocationList();
	}

	/**
	 * Removes all delegate bindings from this multicast delegate's
	 * invocation list that are bound to the specified object.
	 *
	 * This method also compacts the invocation list.
	 *
	 * @param InObject The object to remove bindings for.
	 */
	void RemoveAll( UObject* Object )
	{
		for (int32 BindingIndex = InvocationList.Num() - 1; BindingIndex >= 0; --BindingIndex)
		{
			const TScriptDelegate<TWeakPtr>& Binding = InvocationList[BindingIndex];

			if (Binding.IsBoundToObject(Object) || Binding.IsCompactable())
			{
				InvocationList.RemoveAtSwap(BindingIndex);
			}
		}
	}

	/**
	 * Removes all functions from this delegate's invocation list
	 */
	void Clear()
	{
		InvocationList.Empty();
	}

	/**
	 * Converts this delegate to a string representation
	 *
	 * @return	Delegate in string format
	 */
	template <typename UObjectTemplate>
	inline FString ToString() const
	{
		if( IsBound() )
		{
			FString AllDelegatesString = TEXT( "[" );
			for( typename FInvocationList::TConstIterator CurDelegate( InvocationList ); CurDelegate; ++CurDelegate )
			{
				if( !AllDelegatesString.IsEmpty() )
				{
					AllDelegatesString += TEXT( ", " );
				}
				AllDelegatesString += CurDelegate->template ToString<UObjectTemplate>();
			}
			AllDelegatesString += TEXT( "]" );
			return AllDelegatesString;
		}
		return TEXT( "<Unbound>" );
	}

	/** Multi-cast delegate serialization */
	friend FArchive& operator<<( FArchive& Ar, TMulticastScriptDelegate<TWeakPtr>& D )
	{
		if( Ar.IsSaving() )
		{
			// When saving the delegate, clean up the list to make sure there are no bad object references
			D.CompactInvocationList();
		}

		Ar << D.InvocationList;

		if( Ar.IsLoading() )
		{
			// After loading the delegate, clean up the list to make sure there are no bad object references
			D.CompactInvocationList();
		}

		return Ar;
	}

	/**
	 * Executes a multi-cast delegate by calling all functions on objects bound to the delegate.  Always
	 * safe to call, even if when no objects are bound, or if objects have expired.  In general, you should
	 * never call this function directly.  Instead, call Broadcast() on a derived class.
	 *
	 * @param	Params				Parameter structure
	 */
	template <class UObjectTemplate>
	void ProcessMulticastDelegate(void* Parameters) const
	{
		if( InvocationList.Num() > 0 )
		{
			// Create a copy of the invocation list, just in case the list is modified by one of the callbacks during the broadcast
			typedef TArray< TScriptDelegate<TWeakPtr>, TInlineAllocator< 4 > > FInlineInvocationList;
			FInlineInvocationList InvocationListCopy = FInlineInvocationList(InvocationList);
	
			// Invoke each bound function
			for( typename FInlineInvocationList::TConstIterator FunctionIt( InvocationListCopy ); FunctionIt; ++FunctionIt )
			{
				if( FunctionIt->IsBound() )
				{
					// Invoke this delegate!
					FunctionIt->template ProcessDelegate<UObjectTemplate>(Parameters);
				}
				else if ( FunctionIt->IsCompactable() )
				{
					// Function couldn't be executed, so remove it.  Note that because the original list could have been modified by one of the callbacks, we have to search for the function to remove here.
					RemoveInternal( *FunctionIt );
				}
			}
		}
	}

	/**
	 * Returns all objects associated with this multicast-delegate.  For advanced uses only -- you should never
	 * need call this function in normal circumstances.
 	 * @return	List of objects bound to this delegate
	*/
	TArray< UObject* > GetAllObjects()
	{
		TArray< UObject* > OutputList;
		for( typename FInvocationList::TIterator CurDelegate( InvocationList ); CurDelegate; ++CurDelegate )
		{
			UObject* CurObject = CurDelegate->GetUObject();
			if( CurObject != nullptr )
			{
				OutputList.Add( CurObject );
			}
		}
		return OutputList;
	}

protected:

	/**
	 * Adds a function delegate to this multi-cast delegate's invocation list
	 *
	 * @param	InDelegate	Delegate to add
	*/
	void AddInternal( const TScriptDelegate<TWeakPtr>& InDelegate )
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		// Verify same function isn't already bound
		const int32 NumFunctions = InvocationList.Num();
		for( int32 CurFunctionIndex = 0; CurFunctionIndex < NumFunctions; ++CurFunctionIndex )
		{
			(void)ensure( InvocationList[ CurFunctionIndex ] != InDelegate );
		}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		InvocationList.Add( InDelegate );
	}

	/**
	 * Adds a function delegate to this multi-cast delegate's invocation list, if a delegate with that signature
	 * doesn't already exist
	 *
	 * @param	InDelegate	Delegate to add
	 */
	void AddUniqueInternal( const TScriptDelegate<TWeakPtr>& InDelegate )
	{
		// Add the item to the invocation list only if it is unique
		InvocationList.AddUnique( InDelegate );
	}

	/**
	 * Removes a function from this multi-cast delegate's invocation list (performance is O(N)).  Note that the
	 * order of the delegates may not be preserved!
	 *
	 * @param	InDelegate	Delegate to remove
	*/
	void RemoveInternal( const TScriptDelegate<TWeakPtr>& InDelegate ) const
	{
		InvocationList.RemoveSingleSwap(InDelegate);
	}

	/**
	 * Removes a function from this multi-cast delegate's invocation list (performance is O(N)).  Note that the
	 * order of the delegates may not be preserved!
	 *
	 * @param	InObject		Object of the delegate to remove
	 * @param	InFunctionName	Function name of the delegate to remove
	*/
	void RemoveInternal( const UObject* InObject, FName InFunctionName ) const
	{
		int32 FoundDelegate = InvocationList.IndexOfByPredicate([=](const TScriptDelegate<TWeakPtr>& Delegate) {
			return Delegate.GetFunctionName() == InFunctionName && Delegate.IsBoundToObjectEvenIfUnreachable(InObject);
		});

		if (FoundDelegate != INDEX_NONE)
		{
			InvocationList.RemoveAtSwap(FoundDelegate, 1, false);
		}
	}

	/** Cleans up any delegates in our invocation list that have expired (performance is O(N)) */
	void CompactInvocationList() const
	{
		InvocationList.RemoveAllSwap([](const TScriptDelegate<TWeakPtr>& Delegate){
			return Delegate.IsCompactable();
		});
	}

protected:

	/** Ordered list functions to invoke when the Broadcast function is called */
	typedef TArray< TScriptDelegate<TWeakPtr> > FInvocationList;
	mutable FInvocationList InvocationList;		// Mutable so that we can housekeep list even with 'const' broadcasts

	// Declare ourselves as a friend of UMulticastDelegateProperty so that it can access our function list
	friend class UMulticastDelegateProperty;

	// 
	friend class FCallDelegateHelper;

	friend struct TIsZeroConstructType<TMulticastScriptDelegate<TWeakPtr> >;
};


template<typename TWeakPtr> struct TIsZeroConstructType<TMulticastScriptDelegate<TWeakPtr> > { enum { Value = TIsZeroConstructType<typename TMulticastScriptDelegate<TWeakPtr>::FInvocationList>::Value }; };
