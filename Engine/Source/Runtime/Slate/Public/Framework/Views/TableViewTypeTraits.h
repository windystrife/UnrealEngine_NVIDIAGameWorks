// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/UObjectBase.h"
#include "UObject/GCObject.h"

/**
 * Lists/Trees only work with shared pointer types, and UObjbectBase*.
 * Type traits to ensure that the user does not accidentally make a List/Tree of value types.
 */
template <typename T, typename Enable = void>
struct TIsValidListItem
{
	enum
	{
		Value = false
	};
};
template <typename T>
struct TIsValidListItem<TSharedRef<T, ESPMode::NotThreadSafe>>
{
	enum
	{
		Value = true
	};
};
template <typename T>
struct TIsValidListItem<TSharedRef<T, ESPMode::ThreadSafe>>
{
	enum
	{
		Value = true
	};
};
template <typename T>
struct TIsValidListItem<TSharedPtr<T, ESPMode::NotThreadSafe>>
{
	enum
	{
		Value = true
	};
};
template <typename T>
struct TIsValidListItem<TSharedPtr<T, ESPMode::ThreadSafe>>
{
	enum
	{
		Value = true
	};
};
template <typename T>
struct TIsValidListItem<T*, typename TEnableIf<TPointerIsConvertibleFromTo<T, UObjectBase>::Value>::Type>
{
	enum
	{
		Value = true
	};
};

template <typename T>
struct TIsValidListItem<const T*, typename TEnableIf<TPointerIsConvertibleFromTo<T, UObjectBase>::Value>::Type>
{
	enum
	{
		Value = true
	};
};
template <typename T>
struct TIsValidListItem<TWeakObjectPtr<T>>
{
	enum
	{
		Value = true
	};
};

/**
 * Furthermore, ListViews of TSharedPtr<> work differently from lists of UObject*.
 * ListTypeTraits provide the specialized functionality such as pointer testing, resetting,
 * and optional serialization for UObject garbage collection.
 */
template <typename T, typename Enable=void> struct TListTypeTraits
{
	static_assert(TIsValidListItem<T>::Value, "Item type T must be a UObjectBase pointer, TSharedRef, or TSharedPtr.");
};


/**
 * Pointer-related functionality (e.g. setting to null, testing for null) specialized for SharedPointers.
 */
template <typename T> struct TListTypeTraits< TSharedPtr<T, ESPMode::NotThreadSafe> >
{
public:
	typedef TSharedPtr<T> NullableType;

	static void AddReferencedObjects( FReferenceCollector&, TArray< TSharedPtr<T> >&, TSet< TSharedPtr<T> >&  )
	{
	}

	static bool IsPtrValid( const TSharedPtr<T>& InPtr )
	{
		return InPtr.IsValid();
	}

	static void ResetPtr( TSharedPtr<T>& InPtr )
	{
		InPtr.Reset();
	}

	static TSharedPtr<T> MakeNullPtr()
	{
		return TSharedPtr<T>(NULL);
	}

	static TSharedPtr<T> NullableItemTypeConvertToItemType( const TSharedPtr<T>& InPtr )
	{
		return InPtr;
	}

	class SerializerType{};
};


template <typename T> struct TListTypeTraits< TSharedPtr<T, ESPMode::ThreadSafe> >
{
public:
	typedef TSharedPtr<T, ESPMode::ThreadSafe> NullableType;

	static void AddReferencedObjects( FReferenceCollector&, TArray< TSharedPtr<T, ESPMode::ThreadSafe> >&, TSet< TSharedPtr<T, ESPMode::ThreadSafe> >&  )
	{
	}

	static bool IsPtrValid( const TSharedPtr<T, ESPMode::ThreadSafe>& InPtr )
	{
		return InPtr.IsValid();
	}

	static void ResetPtr( TSharedPtr<T, ESPMode::ThreadSafe>& InPtr )
	{
		InPtr.Reset();
	}

	static TSharedPtr<T, ESPMode::ThreadSafe> MakeNullPtr()
	{
		return TSharedPtr<T, ESPMode::ThreadSafe>(NULL);
	}

	static TSharedPtr<T, ESPMode::ThreadSafe> NullableItemTypeConvertToItemType( const TSharedPtr<T, ESPMode::ThreadSafe>& InPtr )
	{
		return InPtr;
	}

	class SerializerType{};
};


template <typename T> struct TListTypeTraits< TSharedRef<T, ESPMode::NotThreadSafe> >
{
public:
	typedef TSharedPtr<T> NullableType;

	static void AddReferencedObjects( FReferenceCollector&, TArray< TSharedRef<T> >&, TSet< TSharedRef<T> >&  )
	{
	}

	static bool IsPtrValid( const TSharedPtr<T>& InPtr )
	{
		return InPtr.IsValid();
	}

	static void ResetPtr( TSharedPtr<T>& InPtr )
	{
		InPtr.Reset();
	}

	static TSharedPtr<T> MakeNullPtr()
	{
		return TSharedPtr<T>(NULL);
	}

	static TSharedRef<T> NullableItemTypeConvertToItemType( const TSharedPtr<T>& InPtr )
	{
		return InPtr.ToSharedRef();
	}

	class SerializerType{};
};


template <typename T> struct TListTypeTraits< TSharedRef<T, ESPMode::ThreadSafe> >
{
public:
	typedef TSharedPtr<T, ESPMode::ThreadSafe> NullableType;

	static void AddReferencedObjects( FReferenceCollector&, TArray< TSharedRef<T, ESPMode::ThreadSafe> >&, TSet< TSharedRef<T, ESPMode::ThreadSafe> >&  )
	{
	}

	static bool IsPtrValid( const TSharedPtr<T, ESPMode::ThreadSafe>& InPtr )
	{
		return InPtr.IsValid();
	}

	static void ResetPtr( TSharedPtr<T, ESPMode::ThreadSafe>& InPtr )
	{
		InPtr.Reset();
	}

	static TSharedPtr<T, ESPMode::ThreadSafe> MakeNullPtr()
	{
		return TSharedPtr<T, ESPMode::ThreadSafe>(NULL);
	}

	static TSharedRef<T, ESPMode::ThreadSafe> NullableItemTypeConvertToItemType( const TSharedPtr<T, ESPMode::ThreadSafe>& InPtr )
	{
		return InPtr.ToSharedRef();
	}

	class SerializerType{};
};


/**
 * Pointer-related functionality (e.g. setting to null, testing for null) specialized for SharedPointers.
 */
template <typename T> struct TListTypeTraits< TWeakObjectPtr<T> >
{
public:
	typedef TWeakObjectPtr<T> NullableType;

	static void AddReferencedObjects( FReferenceCollector&, TArray< TWeakObjectPtr<T> >&, TSet< TWeakObjectPtr<T> >&  )
	{
	}

	static bool IsPtrValid( const TWeakObjectPtr<T>& InPtr )
	{
		return InPtr.IsValid();
	}

	static void ResetPtr( TWeakObjectPtr<T>& InPtr )
	{
		InPtr.Reset();
	}

	static TWeakObjectPtr<T> MakeNullPtr()
	{
		return TWeakObjectPtr<T>(NULL);
	}

	static TWeakObjectPtr<T> NullableItemTypeConvertToItemType( const TWeakObjectPtr<T>& InPtr )
	{
		return InPtr;
	}

	class SerializerType{};
};


/**
 * Lists of pointer types only work if the pointers are deriving from UObject*.
 * In addition to testing and setting the pointers to null, Lists of UObjects
 * will serialize the objects they are holding onto.
 */
template <typename T>
struct TListTypeTraits<T*, typename TEnableIf<TPointerIsConvertibleFromTo<T, UObjectBase>::Value>::Type>
{
public:
	typedef T* NullableType;

	static void AddReferencedObjects( FReferenceCollector& Collector, TArray<T*>& ItemsWithGeneratedWidgets, TSet<T*>& SelectedItems )
	{
		// Serialize generated items
		Collector.AddReferencedObjects(ItemsWithGeneratedWidgets);

		// Serialize the selected items
		Collector.AddReferencedObjects(SelectedItems);
	}

	static bool IsPtrValid( T* InPtr ) { return InPtr != NULL; }

	static void ResetPtr( T*& InPtr ) { InPtr = NULL; }

	static T* MakeNullPtr() { return NULL; }

	static T* NullableItemTypeConvertToItemType( T* InPtr ) { return InPtr; }

	typedef FGCObject SerializerType;
};

template <typename T>
struct TListTypeTraits<const T*, typename TEnableIf<TPointerIsConvertibleFromTo<T, UObjectBase>::Value>::Type>
{
public:
	typedef const T* NullableType;

	static void AddReferencedObjects( FReferenceCollector& Collector, TArray<const T*>& ItemsWithGeneratedWidgets, TSet<const T*>& SelectedItems )
	{
		// Serialize generated items
		Collector.AddReferencedObjects(ItemsWithGeneratedWidgets);

		// Serialize the selected items
		Collector.AddReferencedObjects(SelectedItems);
	}

	static bool IsPtrValid( const T* InPtr ) { return InPtr != NULL; }

	static void ResetPtr( const T*& InPtr ) { InPtr = NULL; }

	static const T* MakeNullPtr() { return NULL; }

	static const T* NullableItemTypeConvertToItemType( const T* InPtr ) { return InPtr; }

	typedef FGCObject SerializerType;
};
