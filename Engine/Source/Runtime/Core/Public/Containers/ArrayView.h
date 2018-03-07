// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/PointerIsConvertibleFromTo.h"
#include "Misc/AssertionMacros.h"
#include "Templates/UnrealTypeTraits.h"
#include "Math/NumericLimits.h"
#include "Containers/Array.h"

namespace ArrayViewPrivate
{
	/**
	 * Trait testing whether a type is compatible with the view type
	 */
	template <typename T, typename ElementType>
	struct TIsCompatibleElementType
	{
	public:
		/** NOTE:
		 * The stars in the TPointerIsConvertibleFromTo test are *IMPORTANT*
		 * They prevent TArrayView<Base>(TArray<Derived>&) from compiling!
		 */
		enum { Value = TPointerIsConvertibleFromTo<T*, ElementType* const>::Value };
	};
}

/**
 * Templated fixed-size view of another array
 *
 * A statically sized view of an array of typed elements.  Designed to allow functions to take either a fixed C array
 * or a TArray with an arbitrary allocator as an argument when the function neither adds nor removes elements
 *
 * e.g.:
 * int32 SumAll(TArrayView<const int32> array)
 * {
 *     return Algo::Accumulate(array);
 * }
 * 
 * could be called as:
 *     SumAll(MyTArray);
 *     SumAll(MyCArray);
 *     SumAll({1, 2, 3});
 *     SumAll(MakeArrayView(Ptr, Num));
 * 
 * Note:
 *   View classes are not const-propagating! If you want a view where the elements are const, you need "TArrayView<const T>" not "const TArrayView<T>"!
 *
 * Caution:
 *   Treat a view like a *reference* to the elements in the array. DO NOT free or reallocate the array while the view exists!
 */
template<typename InElementType>
class TArrayView
{
public:
	using ElementType = InElementType;

	/**
	 * Constructor.
	 */
	TArrayView()
		: DataPtr(nullptr)
		, ArrayNum(0)
	{
	}

private:
	template <typename T>
	using TIsCompatibleElementType = ArrayViewPrivate::TIsCompatibleElementType<T, ElementType>;

public:
	/**
	 * Copy constructor from another view
	 *
	 * @param Other The source array view to copy
	 */
	template <typename OtherElementType,
		typename = typename TEnableIf<TIsCompatibleElementType<OtherElementType>::Value>::Type>
	FORCEINLINE TArrayView(const TArrayView<OtherElementType>& Other)
		: DataPtr(Other.GetData())
		, ArrayNum(Other.Num())
	{
	}

	/**
	 * Construct a view of a C array with a compatible element type
	 *
	 * @param Other The source array to view.
	 */
	template <typename OtherElementType, size_t Size,
		typename = typename TEnableIf<TIsCompatibleElementType<OtherElementType>::Value>::Type>
	FORCEINLINE TArrayView(OtherElementType (&Other)[Size])
		: DataPtr(Other)
		, ArrayNum((int32)Size)
	{
		static_assert(Size <= MAX_int32, "Array Size too large! Size is only int32 for compatibility with TArray!");
	}

	/**
	 * Construct a view of a C array with a compatible element type
	 *
	 * @param Other The source array to view.
	 */
	template <typename OtherElementType, size_t Size,
		typename = typename TEnableIf<TIsCompatibleElementType<const OtherElementType>::Value>::Type>
		FORCEINLINE TArrayView(const OtherElementType (&Other)[Size])
		: DataPtr(Other)
		, ArrayNum((int32)Size)
	{
		static_assert(Size <= MAX_int32, "Array Size too large! Size is only int32 for compatibility with TArray!");
	}

	/**
	 * Construct a view of a TArray with a compatible element type
	 *
	 * @param Other The source array to view.
	 */
	template <typename OtherElementType, typename OtherAllocator,
		typename = typename TEnableIf<TIsCompatibleElementType<OtherElementType>::Value>::Type>
	FORCEINLINE TArrayView(TArray<OtherElementType, OtherAllocator>& Other)
		: DataPtr(Other.GetData())
		, ArrayNum(Other.Num())
	{
	}

	template <typename OtherElementType, typename OtherAllocator,
		typename = typename TEnableIf<TIsCompatibleElementType<const OtherElementType>::Value>::Type>
	FORCEINLINE TArrayView(const TArray<OtherElementType, OtherAllocator>& Other)
		: DataPtr(Other.GetData())
		, ArrayNum(Other.Num())
	{
	}

	/**
	 * Construct a view from an initializer list with a compatible element type
	 *
	 * @param List The initializer list to view.
	 */
	template <typename OtherElementType,
		typename = typename TEnableIf<TIsCompatibleElementType<OtherElementType>::Value>::Type>
	FORCEINLINE TArrayView(std::initializer_list<OtherElementType> List)
		: DataPtr(&*List.begin())
		, ArrayNum(List.size())
	{
	}

	/**
	 * Construct a view of an arbitrary pointer
	 *
	 * @param InData	The data to view
	 * @param InCount	The number of elements
	 */
	template <typename OtherElementType,
		typename = typename TEnableIf<TIsCompatibleElementType<OtherElementType>::Value>::Type>
	FORCEINLINE TArrayView(OtherElementType* InData, int32 InCount)
		: DataPtr(InData)
		, ArrayNum(InCount)
	{
		check(ArrayNum >= 0);
	}

	/**
	 * Assignment operator
	 *
	 * @param Other The source array view to assign from
	 */
	template <typename OtherElementType,
		typename = typename TEnableIf<TIsCompatibleElementType<OtherElementType>::Value>::Type>
	FORCEINLINE TArrayView& operator=(const TArrayView<OtherElementType>& Other)
	{
		if (this != &Other)
		{
			DataPtr = Other.DataPtr;
			ArrayNum = Other.ArrayNum;
		}
		return *this;
	}

public:

	/**
	 * Helper function for returning a typed pointer to the first array entry.
	 *
	 * @returns Pointer to first array entry or nullptr if ArrayMax == 0.
	 */
	FORCEINLINE ElementType* GetData() const
	{
		return DataPtr;
	}

	/** 
	 * Helper function returning the size of the inner type.
	 *
	 * @returns Size in bytes of array type.
	 */
	FORCEINLINE size_t GetTypeSize() const
	{
		return sizeof(ElementType);
	}

	/**
	 * Checks array invariants: if array size is greater than zero and less
	 * than maximum.
	 */
	FORCEINLINE void CheckInvariants() const
	{
		checkSlow(ArrayNum >= 0);
	}

	/**
	 * Checks if index is in array range.
	 *
	 * @param Index Index to check.
	 */
	FORCEINLINE void RangeCheck(int32 Index) const
	{
		CheckInvariants();

		checkf((Index >= 0) & (Index < ArrayNum),TEXT("Array index out of bounds: %i from an array of size %i"),Index,ArrayNum); // & for one branch
	}

	/**
	 * Tests if index is valid, i.e. than or equal to zero, and less than the number of elements in the array.
	 *
	 * @param Index Index to test.
	 *
	 * @returns True if index is valid. False otherwise.
	 */
	FORCEINLINE bool IsValidIndex(int32 Index) const
	{
		return (Index >= 0) && (Index < ArrayNum);
	}

	/**
	 * Returns number of elements in array.
	 *
	 * @returns Number of elements in array.
	 */
	FORCEINLINE int32 Num() const
	{
		return ArrayNum;
	}

	/**
	 * Array bracket operator. Returns reference to element at give index.
	 *
	 * @returns Reference to indexed element.
	 */
	FORCEINLINE ElementType& operator[](int32 Index) const
	{
		RangeCheck(Index);
		return GetData()[Index];
	}

	/**
	 * Returns n-th last element from the array.
	 *
	 * @param IndexFromTheEnd (Optional) Index from the end of array.
	 *                        Default is 0.
	 *
	 * @returns Reference to n-th last element from the array.
	 */
	FORCEINLINE ElementType& Last(int32 IndexFromTheEnd = 0) const
	{
		RangeCheck(ArrayNum - IndexFromTheEnd - 1);
		return GetData()[ArrayNum - IndexFromTheEnd - 1];
	}

	/**
	 * Returns a sliced view
	 *
	 * @param Index starting index of the new view
	 * @param InNum number of elements in the new view
	 * @returns Sliced view
	 */
	FORCEINLINE TArrayView Slice(int32 Index, int32 InNum)
	{
		check(InNum > 0);
		check(IsValidIndex(Index));
		check(IsValidIndex(Index + InNum - 1));
		return TArrayView(DataPtr + Index, InNum);
	}

	/**
	 * Finds element within the array.
	 *
	 * @param Item Item to look for.
	 * @param Index Output parameter. Found index.
	 *
	 * @returns True if found. False otherwise.
	 */
	FORCEINLINE bool Find(const ElementType& Item, int32& Index) const
	{
		Index = this->Find(Item);
		return Index != INDEX_NONE;
	}

	/**
	 * Finds element within the array.
	 *
	 * @param Item Item to look for.
	 *
	 * @returns Index of the found element. INDEX_NONE otherwise.
	 */
	int32 Find(const ElementType& Item) const
	{
		const ElementType* RESTRICT Start = GetData();
		for (const ElementType* RESTRICT Data = Start, *RESTRICT DataEnd = Data + ArrayNum; Data != DataEnd; ++Data)
		{
			if (*Data == Item)
			{
				return static_cast<int32>(Data - Start);
			}
		}
		return INDEX_NONE;
	}

	/**
	 * Finds element within the array starting from the end.
	 *
	 * @param Item Item to look for.
	 * @param Index Output parameter. Found index.
	 *
	 * @returns True if found. False otherwise.
	 */
	FORCEINLINE bool FindLast(const ElementType& Item, int32& Index) const
	{
		Index = this->FindLast(Item);
		return Index != INDEX_NONE;
	}

	/**
	 * Finds element within the array starting from StartIndex and going backwards. Uses predicate to match element.
	 *
	 * @param Pred Predicate taking array element and returns true if element matches search criteria, false otherwise.
	 * @param StartIndex Index of element from which to start searching.
	 *
	 * @returns Index of the found element. INDEX_NONE otherwise.
	 */
	template <typename Predicate>
	int32 FindLastByPredicate(Predicate Pred, int32 StartIndex) const
	{
		check(StartIndex >= 0 && StartIndex <= this->Num());
		for (const ElementType* RESTRICT Start = GetData(), *RESTRICT Data = Start + StartIndex; Data != Start; )
		{
			--Data;
			if (Pred(*Data))
			{
				return static_cast<int32>(Data - Start);
			}
		}
		return INDEX_NONE;
	}

	/**
	* Finds element within the array starting from the end. Uses predicate to match element.
	*
	* @param Pred Predicate taking array element and returns true if element matches search criteria, false otherwise.
	*
	* @returns Index of the found element. INDEX_NONE otherwise.
	*/
	template <typename Predicate>
	FORCEINLINE int32 FindLastByPredicate(Predicate Pred) const
	{
		return FindLastByPredicate(Pred, ArrayNum);
	}

	/**
	 * Finds an item by key (assuming the ElementType overloads operator== for
	 * the comparison).
	 *
	 * @param Key The key to search by.
	 *
	 * @returns Index to the first matching element, or INDEX_NONE if none is
	 *          found.
	 */
	template <typename KeyType>
	int32 IndexOfByKey(const KeyType& Key) const
	{
		const ElementType* RESTRICT Start = GetData();
		for (const ElementType* RESTRICT Data = Start, *RESTRICT DataEnd = Start + ArrayNum; Data != DataEnd; ++Data)
		{
			if (*Data == Key)
			{
				return static_cast<int32>(Data - Start);
			}
		}
		return INDEX_NONE;
	}

	/**
	 * Finds an item by predicate.
	 *
	 * @param Pred The predicate to match.
	 *
	 * @returns Index to the first matching element, or INDEX_NONE if none is
	 *          found.
	 */
	template <typename Predicate>
	int32 IndexOfByPredicate(Predicate Pred) const
	{
		const ElementType* RESTRICT Start = GetData();
		for (const ElementType* RESTRICT Data = Start, *RESTRICT DataEnd = Start + ArrayNum; Data != DataEnd; ++Data)
		{
			if (Pred(*Data))
			{
				return static_cast<int32>(Data - Start);
			}
		}
		return INDEX_NONE;
	}

	/**
	 * Finds an item by key (assuming the ElementType overloads operator== for
	 * the comparison).
	 *
	 * @param Key The key to search by.
	 *
	 * @returns Pointer to the first matching element, or nullptr if none is found.
	 */
	template <typename KeyType>
	ElementType* FindByKey(const KeyType& Key) const
	{
		for (ElementType* RESTRICT Data = GetData(), *RESTRICT DataEnd = Data + ArrayNum; Data != DataEnd; ++Data)
		{
			if (*Data == Key)
			{
				return Data;
			}
		}

		return nullptr;
	}

	/**
	 * Finds an element which matches a predicate functor.
	 *
	 * @param Pred The functor to apply to each element.
	 *
	 * @return Pointer to the first element for which the predicate returns
	 *         true, or nullptr if none is found.
	 */
	template <typename Predicate>
	ElementType* FindByPredicate(Predicate Pred) const
	{
		for (ElementType* RESTRICT Data = GetData(), *RESTRICT DataEnd = Data + ArrayNum; Data != DataEnd; ++Data)
		{
			if (Pred(*Data))
			{
				return Data;
			}
		}

		return nullptr;
	}

	/**
	 * Filters the elements in the array based on a predicate functor.
	 *
	 * @param Pred The functor to apply to each element.
	 *
	 * @returns TArray with the same type as this object which contains
	 *          the subset of elements for which the functor returns true.
	 */
	template <typename Predicate>
	TArray<typename TRemoveConst<ElementType>::Type> FilterByPredicate(Predicate Pred) const
	{
		TArray<typename TRemoveConst<ElementType>::Type> FilterResults;
		for (const ElementType* RESTRICT Data = GetData(), *RESTRICT DataEnd = Data + ArrayNum; Data != DataEnd; ++Data)
		{
			if (Pred(*Data))
			{
				FilterResults.Add(*Data);
			}
		}
		return FilterResults;
	}

	/**
	 * Checks if this array contains the element.
	 *
	 * @returns	True if found. False otherwise.
	 */
	template <typename ComparisonType>
	bool Contains(const ComparisonType& Item) const
	{
		for (const ElementType* RESTRICT Data = GetData(), *RESTRICT DataEnd = Data + ArrayNum; Data != DataEnd; ++Data)
		{
			if (*Data == Item)
			{
				return true;
			}
		}
		return false;
	}

	/**
	 * Checks if this array contains element for which the predicate is true.
	 *
	 * @param Predicate to use
	 *
	 * @returns	True if found. False otherwise.
	 */
	template <typename Predicate>
	FORCEINLINE bool ContainsByPredicate(Predicate Pred) const
	{
		return FindByPredicate(Pred) != nullptr;
	}

private:
	/**
	 * DO NOT USE DIRECTLY
	 * STL-like iterators to enable range-based for loop support.
	 */
	FORCEINLINE friend ElementType* begin(const TArrayView& Array) { return Array.GetData(); }
	FORCEINLINE friend ElementType* end  (const TArrayView& Array) { return Array.GetData() + Array.Num(); }

public:
	/**
	 * Sorts the array assuming < operator is defined for the item type.
	 */
	void Sort()
	{
		::Sort(GetData(), Num());
	}

	/**
	 * Sorts the array using user define predicate class.
	 *
	 * @param Predicate Predicate class instance.
	 */
	template <class PREDICATE_CLASS>
	void Sort(const PREDICATE_CLASS& Predicate)
	{
		::Sort(GetData(), Num(), Predicate);
	}

	/**
	 * Stable sorts the array assuming < operator is defined for the item type.
	 *
	 * Stable sort is slower than non-stable algorithm.
	 */
	void StableSort()
	{
		::StableSort(GetData(), Num());
	}

	/**
	 * Stable sorts the array using user defined predicate class.
	 *
	 * Stable sort is slower than non-stable algorithm.
	 *
	 * @param Predicate Predicate class instance
	 */
	template <class PREDICATE_CLASS>
	void StableSort(const PREDICATE_CLASS& Predicate)
	{
		::StableSort(GetData(), Num(), Predicate);
	}

private:
	ElementType* DataPtr;
	int32 ArrayNum;
};

template <typename InElementType>
struct TIsZeroConstructType<TArrayView<InElementType>>
{
	enum { Value = true };
};

template <typename T>
struct TIsContiguousContainer<TArrayView<T>>
{
	enum { Value = true };
};

//////////////////////////////////////////////////////////////////////////

template <typename ElementType, size_t Size>
TArrayView<ElementType> MakeArrayView(ElementType(&Other)[Size])
{
	return TArrayView<ElementType>(Other);
}

template <typename ElementType, size_t Size>
TArrayView<const ElementType> MakeArrayView(const ElementType(&Other)[Size])
{
	return TArrayView<ElementType>(Other);
}

template <typename ElementType, typename Allocator>
TArrayView<ElementType> MakeArrayView(TArray<ElementType, Allocator>& Other)
{
	return TArrayView<ElementType>(Other);
}

template <typename ElementType, typename Allocator>
TArrayView<const ElementType> MakeArrayView(const TArray<ElementType, Allocator>& Other)
{
	return TArrayView<const ElementType>(Other);
}

template<typename ElementType>
TArrayView<ElementType> MakeArrayView(std::initializer_list<ElementType> List)
{
	return TArrayView<ElementType>(List);
}

template<typename ElementType>
TArrayView<ElementType> MakeArrayView(ElementType* Pointer, int32 Size)
{
	return TArrayView<ElementType>(Pointer, Size);
}
