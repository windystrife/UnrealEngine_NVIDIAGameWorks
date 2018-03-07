// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/IsPointer.h"
#include "HAL/UnrealMemory.h"
#include "Templates/EnableIf.h"
#include "Templates/AndOrNot.h"
#include "Templates/AreTypesEqual.h"
#include "Templates/IsArithmetic.h"
#include "Templates/UnrealTypeTraits.h"
#include "Templates/RemoveReference.h"
#include "Templates/TypeCompatibleBytes.h"
#include "Traits/IsContiguousContainer.h"

/*-----------------------------------------------------------------------------
	Standard templates.
-----------------------------------------------------------------------------*/

/**
 * Chooses between the two parameters based on whether the first is nullptr or not.
 * @return If the first parameter provided is non-nullptr, it is returned; otherwise the second parameter is returned.
 */
template<typename ReferencedType>
FORCEINLINE ReferencedType* IfAThenAElseB(ReferencedType* A,ReferencedType* B)
{
	const PTRINT IntA = reinterpret_cast<PTRINT>(A);
	const PTRINT IntB = reinterpret_cast<PTRINT>(B);

	// Compute a mask which has all bits set if IntA is zero, and no bits set if it's non-zero.
	const PTRINT MaskB = -(!IntA);

	return reinterpret_cast<ReferencedType*>(IntA | (MaskB & IntB));
}

/** branchless pointer selection based on predicate
* return PTRINT(Predicate) ? A : B;
**/
template<typename PredicateType,typename ReferencedType>
FORCEINLINE ReferencedType* IfPThenAElseB(PredicateType Predicate,ReferencedType* A,ReferencedType* B)
{
	const PTRINT IntA = reinterpret_cast<PTRINT>(A);
	const PTRINT IntB = reinterpret_cast<PTRINT>(B);

	// Compute a mask which has all bits set if Predicate is zero, and no bits set if it's non-zero.
	const PTRINT MaskB = -(!PTRINT(Predicate));

	return reinterpret_cast<ReferencedType*>((IntA & ~MaskB) | (IntB & MaskB));
}

/** A logical exclusive or function. */
inline bool XOR(bool A, bool B)
{
	return A != B;
}

/** This is used to provide type specific behavior for a copy which cannot change the value of B. */
template<typename T>
FORCEINLINE void Move(T& A,typename TMoveSupportTraits<T>::Copy B)
{
	// Destruct the previous value of A.
	A.~T();

	// Use placement new and a copy constructor so types with const members will work.
	new(&A) T(B);
}

/** This is used to provide type specific behavior for a move which may change the value of B. */
template<typename T>
FORCEINLINE void Move(T& A,typename TMoveSupportTraits<T>::Move B)
{
	// Destruct the previous value of A.
	A.~T();

	// Use placement new and a copy constructor so types with const members will work.
	new(&A) T(MoveTemp(B));
}

/**
 * Generically gets the data pointer of a contiguous container
 */
template<typename T, typename = typename TEnableIf<TIsContiguousContainer<T>::Value>::Type>
auto GetData(T&& Container) -> decltype(Container.GetData())
{
	return Container.GetData();
}

template <typename T, SIZE_T N>
CONSTEXPR T* GetData(T (&Container)[N])
{
	return Container;
}

template <typename T>
CONSTEXPR T* GetData(std::initializer_list<T> List)
{
	return List.begin();
}

/**
* Generically gets the number of items in a contiguous container
*/
template<typename T, typename = typename TEnableIf<TIsContiguousContainer<T>::Value>::Type>
SIZE_T GetNum(T&& Container)
{
	return (SIZE_T)Container.Num();
}

template <typename T, SIZE_T N>
CONSTEXPR SIZE_T GetNum(T (&Container)[N])
{
	return N;
}

template <typename T>
CONSTEXPR SIZE_T GetNum(std::initializer_list<T> List)
{
	return List.size();
}

/*----------------------------------------------------------------------------
	Standard macros.
----------------------------------------------------------------------------*/

#ifdef __clang__
	template <typename T>
	auto ArrayCountHelper(T& t) -> typename TEnableIf<__is_array(T), char(&)[sizeof(t) / sizeof(t[0]) + 1]>::Type;
#else
	template <typename T, uint32 N>
	char (&ArrayCountHelper(const T (&)[N]))[N + 1];
#endif

// Number of elements in an array.
#define ARRAY_COUNT( array ) (sizeof(ArrayCountHelper(array)) - 1)

// Offset of a struct member.
#ifndef UNREAL_CODE_ANALYZER
// UCA uses clang on Windows. According to C++11 standard, (which in this case clang follows and msvc doesn't)
// forbids using reinterpret_cast in constant expressions. msvc uses reinterpret_cast in offsetof macro,
// while clang uses compiler intrinsic. Calling static_assert(STRUCT_OFFSET(x, y) == SomeValue) causes compiler
// error when using clang on Windows (while including windows headers).
#define STRUCT_OFFSET( struc, member )	offsetof(struc, member)
#else
#define STRUCT_OFFSET( struc, member )	__builtin_offsetof(struc, member)
#endif

#if PLATFORM_VTABLE_AT_END_OF_CLASS
	#error need implementation
#else
	#define VTABLE_OFFSET( Class, MultipleInheritenceParent )	( ((PTRINT) static_cast<MultipleInheritenceParent*>((Class*)1)) - 1)
#endif


/**
 * works just like std::min_element.
 */
template<class ForwardIt> inline
ForwardIt MinElement(ForwardIt First, ForwardIt Last)
{
	ForwardIt Result = First;
	for (; ++First != Last; )
	{
		if (*First < *Result) 
		{
			Result = First;
		}
	}
	return Result;
}

/**
 * works just like std::min_element.
 */
template<class ForwardIt, class PredicateType> inline
ForwardIt MinElement(ForwardIt First, ForwardIt Last, PredicateType Predicate)
{
	ForwardIt Result = First;
	for (; ++First != Last; )
	{
		if (Predicate(*First,*Result))
		{
			Result = First;
		}
	}
	return Result;
}

/**
* works just like std::max_element.
*/
template<class ForwardIt> inline
ForwardIt MaxElement(ForwardIt First, ForwardIt Last)
{
	ForwardIt Result = First;
	for (; ++First != Last; )
	{
		if (*Result < *First) 
		{
			Result = First;
		}
	}
	return Result;
}

/**
* works just like std::max_element.
*/
template<class ForwardIt, class PredicateType> inline
ForwardIt MaxElement(ForwardIt First, ForwardIt Last, PredicateType Predicate)
{
	ForwardIt Result = First;
	for (; ++First != Last; )
	{
		if (Predicate(*Result,*First))
		{
			Result = First;
		}
	}
	return Result;
}

/**
 * utility template for a class that should not be copyable.
 * Derive from this class to make your class non-copyable
 */
class FNoncopyable
{
protected:
	// ensure the class cannot be constructed directly
	FNoncopyable() {}
	// the class should not be used polymorphically
	~FNoncopyable() {}
private:
	FNoncopyable(const FNoncopyable&);
	FNoncopyable& operator=(const FNoncopyable&);
};


/** 
 * exception-safe guard around saving/restoring a value.
 * Commonly used to make sure a value is restored 
 * even if the code early outs in the future.
 * Usage:
 *  	TGuardValue<bool> GuardSomeBool(bSomeBool, false); // Sets bSomeBool to false, and restores it in dtor.
 */
template <typename Type>
struct TGuardValue : private FNoncopyable
{
	TGuardValue(Type& ReferenceValue, const Type& NewValue)
	: RefValue(ReferenceValue), OldValue(ReferenceValue)
	{
		RefValue = NewValue;
	}
	~TGuardValue()
	{
		RefValue = OldValue;
	}

	/**
	 * Overloaded dereference operator.
	 * Provides read-only access to the original value of the data being tracked by this struct
	 *
	 * @return	a const reference to the original data value
	 */
	FORCEINLINE const Type& operator*() const
	{
		return OldValue;
	}

private:
	Type& RefValue;
	Type OldValue;
};


/** 
 * Commonly used to make sure a value is incremented, and then decremented anyway the function can terminate.
 * Usage:
 *  	TScopeCounter<int32> BeginProcessing(ProcessingCount); // increments ProcessingCount, and decrements it in the dtor
 */
template <typename Type>
struct TScopeCounter : private FNoncopyable
{
	TScopeCounter(Type& ReferenceValue)
		: RefValue(ReferenceValue)
	{
		++RefValue;
	}
	~TScopeCounter()
	{
		--RefValue;
	}

private:
	Type& RefValue;
};


/**
 * Helper class to make it easy to use key/value pairs with a container.
 */
template <typename KeyType, typename ValueType>
struct TKeyValuePair
{
	TKeyValuePair( const KeyType& InKey, const ValueType& InValue )
	:	Key(InKey), Value(InValue)
	{
	}
	TKeyValuePair( const KeyType& InKey )
	:	Key(InKey)
	{
	}
	TKeyValuePair()
	{
	}
	bool operator==( const TKeyValuePair& Other ) const
	{
		return Key == Other.Key;
	}
	bool operator!=( const TKeyValuePair& Other ) const
	{
		return Key != Other.Key;
	}
	bool operator<( const TKeyValuePair& Other ) const
	{
		return Key < Other.Key;
	}
	FORCEINLINE bool operator()( const TKeyValuePair& A, const TKeyValuePair& B ) const
	{
		return A.Key < B.Key;
	}
	KeyType		Key;
	ValueType	Value;
};

//
// Macros that can be used to specify multiple template parameters in a macro parameter.
// This is necessary to prevent the macro parsing from interpreting the template parameter
// delimiting comma as a macro parameter delimiter.
// 

#define TEMPLATE_PARAMETERS2(X,Y) X,Y


/**
 * Removes one level of pointer from a type, e.g.:
 *
 * TRemovePointer<      int32  >::Type == int32
 * TRemovePointer<      int32* >::Type == int32
 * TRemovePointer<      int32**>::Type == int32*
 * TRemovePointer<const int32* >::Type == const int32
 */
template <typename T> struct TRemovePointer     { typedef T Type; };
template <typename T> struct TRemovePointer<T*> { typedef T Type; };

/**
 * MoveTemp will cast a reference to an rvalue reference.
 * This is UE's equivalent of std::move except that it will not compile when passed an rvalue or
 * const object, because we would prefer to be informed when MoveTemp will have no effect.
 */
template <typename T>
FORCEINLINE typename TRemoveReference<T>::Type&& MoveTemp(T&& Obj)
{
	typedef typename TRemoveReference<T>::Type CastType;

	// Validate that we're not being passed an rvalue or a const object - the former is redundant, the latter is almost certainly a mistake
	static_assert(TIsLValueReferenceType<T>::Value, "MoveTemp called on an rvalue");
	static_assert(!TAreTypesEqual<CastType&, const CastType&>::Value, "MoveTemp called on a const object");

	return (CastType&&)Obj;
}

/**
 * MoveTemp will cast a reference to an rvalue reference.
 * This is UE's equivalent of std::move.  It doesn't static assert like MoveTemp, because it is useful in
 * templates or macros where it's not obvious what the argument is, but you want to take advantage of move semantics
 * where you can but not stop compilation.
 */
template <typename T>
FORCEINLINE typename TRemoveReference<T>::Type&& MoveTempIfPossible(T&& Obj)
{
	typedef typename TRemoveReference<T>::Type CastType;
	return (CastType&&)Obj;
}

/**
 * CopyTemp will enforce the creation of an rvalue which can bind to rvalue reference parameters.
 * Unlike MoveTemp, the source object will never be modifed. (i.e. a copy will be made)
 * There is no std:: equivalent.
 */
template <typename T>
FORCEINLINE T CopyTemp(T& Val)
{
	return const_cast<const T&>(Val);
}

template <typename T>
FORCEINLINE T CopyTemp(const T& Val)
{
	return Val;
}

template <typename T>
FORCEINLINE T&& CopyTemp(T&& Val)
{
	// If we already have an rvalue, just return it unchanged, rather than needlessly creating yet another rvalue from it.
	return MoveTemp(Val);
}

/**
 * Forward will cast a reference to an rvalue reference.
 * This is UE's equivalent of std::forward.
 */
template <typename T>
FORCEINLINE T&& Forward(typename TRemoveReference<T>::Type& Obj)
{
	return (T&&)Obj;
}

template <typename T>
FORCEINLINE T&& Forward(typename TRemoveReference<T>::Type&& Obj)
{
	return (T&&)Obj;
}

/**
 * A traits class which specifies whether a Swap of a given type should swap the bits or use a traditional value-based swap.
 */
template <typename T>
struct TUseBitwiseSwap
{
	// We don't use bitwise swapping for 'register' types because this will force them into memory and be slower.
	enum { Value = !TOrValue<__is_enum(T), TIsPointer<T>, TIsArithmetic<T>>::Value };
};


/**
 * Swap two values.  Assumes the types are trivially relocatable.
 */
template <typename T>
inline typename TEnableIf<TUseBitwiseSwap<T>::Value>::Type Swap(T& A, T& B)
{
	if (LIKELY(&A != &B))
	{
		TTypeCompatibleBytes<T> Temp;
		FMemory::Memcpy(&Temp, &A, sizeof(T));
		FMemory::Memcpy(&A, &B, sizeof(T));
		FMemory::Memcpy(&B, &Temp, sizeof(T));
	}
}

template <typename T>
inline typename TEnableIf<!TUseBitwiseSwap<T>::Value>::Type Swap(T& A, T& B)
{
	T Temp = MoveTemp(A);
	A = MoveTemp(B);
	B = MoveTemp(Temp);
}

template <typename T>
inline void Exchange(T& A, T& B)
{
	Swap(A, B);
}

/**
 * This exists to avoid a Visual Studio bug where using a cast to forward an rvalue reference array argument
 * to a pointer parameter will cause bad code generation.  Wrapping the cast in a function causes the correct
 * code to be generated.
 */
template <typename T, typename ArgType>
FORCEINLINE T StaticCast(ArgType&& Arg)
{
	return static_cast<T>(Arg);
}

/**
 * TRValueToLValueReference converts any rvalue reference type into the equivalent lvalue reference, otherwise returns the same type.
 */
template <typename T> struct TRValueToLValueReference      { typedef T  Type; };
template <typename T> struct TRValueToLValueReference<T&&> { typedef T& Type; };

/**
 * Reverses the order of the bits of a value.
 * This is an TEnableIf'd template to ensure that no undesirable conversions occur.  Overloads for other types can be added in the same way.
 *
 * @param Bits - The value to bit-swap.
 * @return The bit-swapped value.
 */
template <typename T>
FORCEINLINE typename TEnableIf<TAreTypesEqual<T, uint32>::Value, T>::Type ReverseBits( T Bits )
{
	Bits = ( Bits << 16) | ( Bits >> 16);
	Bits = ( (Bits & 0x00ff00ff) << 8 ) | ( (Bits & 0xff00ff00) >> 8 );
	Bits = ( (Bits & 0x0f0f0f0f) << 4 ) | ( (Bits & 0xf0f0f0f0) >> 4 );
	Bits = ( (Bits & 0x33333333) << 2 ) | ( (Bits & 0xcccccccc) >> 2 );
	Bits = ( (Bits & 0x55555555) << 1 ) | ( (Bits & 0xaaaaaaaa) >> 1 );
	return Bits;
}

/** Template for initializing a singleton at the boot. */
template< class T >
struct TForceInitAtBoot
{
	TForceInitAtBoot()
	{
		T::Get();
	}
};

/** Used to avoid cluttering code with ifdefs. */
struct FNoopStruct
{
	FNoopStruct()
	{}

	~FNoopStruct()
	{}
};

/**
 * Copies the cv-qualifiers from one type to another, e.g.:
 *
 * TCopyQualifiersFromTo<const    T1,       T2>::Type == const T2
 * TCopyQualifiersFromTo<volatile T1, const T2>::Type == const volatile T2
 */
template <typename From, typename To> struct TCopyQualifiersFromTo                          { typedef                To Type; };
template <typename From, typename To> struct TCopyQualifiersFromTo<const          From, To> { typedef const          To Type; };
template <typename From, typename To> struct TCopyQualifiersFromTo<      volatile From, To> { typedef       volatile To Type; };
template <typename From, typename To> struct TCopyQualifiersFromTo<const volatile From, To> { typedef const volatile To Type; };

/**
 * Tests if qualifiers are lost between one type and another, e.g.:
 *
 * TCopyQualifiersFromTo<const    T1,                T2>::Value == true
 * TCopyQualifiersFromTo<volatile T1, const volatile T2>::Value == false
 */
template <typename From, typename To>
struct TLosesQualifiersFromTo
{
	enum { Value = !TAreTypesEqual<typename TCopyQualifiersFromTo<From, To>::Type, To>::Value };
};

/**
 * Returns the same type passed to it.  This is useful in a few cases, but mainly for inhibiting template argument deduction in function arguments, e.g.:
 *
 * template <typename T>
 * void Func1(T Val); // Can be called like Func(123) or Func<int>(123);
 *
 * template <typename T>
 * void Func2(typename TIdentity<T>::Type Val); // Must be called like Func<int>(123)
 */
template <typename T>
struct TIdentity
{
	typedef T Type;
};

/**
 * Equivalent to std::declval.  
 *
 * Note that this function is unimplemented, and is only intended to be used in unevaluated contexts, like sizeof and trait expressions.
 */
template <typename T>
T&& DeclVal();
