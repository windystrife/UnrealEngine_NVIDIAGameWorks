// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/AreTypesEqual.h"
#include "Templates/UnrealTemplate.h"
#include "Templates/Decay.h"
#include "Delegates/IntegerSequence.h"

// VS2015 Update 2 (and seemingly earlier) erroneously complains about multiple versions
// of special member functions, so we disable the use of defaulting in that case.
// 
// http://stackoverflow.com/questions/36657243/c-multiple-versions-of-a-defaulted-special-member-functions-error-in-msvc-2
#if defined(_MSC_VER) && _MSC_FULL_VER <= 190023918
	#define TUPLES_USE_DEFAULTED_FUNCTIONS 0
#else
	#define TUPLES_USE_DEFAULTED_FUNCTIONS PLATFORM_COMPILER_HAS_DEFAULTED_FUNCTIONS
#endif

// Static analysis causes internal compiler errors with auto-deduced return types,
// but some older VC versions still have return type deduction failures inside the delegate code
// when they are enabled.  So we currently only enable them for static analysis builds.
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
	#define USE_TUPLE_AUTO_RETURN_TYPES (PLATFORM_COMPILER_HAS_AUTO_RETURN_TYPES && USING_CODE_ANALYSIS)
#else
	#define USE_TUPLE_AUTO_RETURN_TYPES 1
#endif

class FArchive;

template <typename... Types>
struct TTuple;

namespace UE4Tuple_Private
{
	template <int32 N, typename... Types>
	struct TNthTypeFromParameterPack;

	template <int32 N, typename T, typename... OtherTypes>
	struct TNthTypeFromParameterPack<N, T, OtherTypes...>
	{
		typedef typename TNthTypeFromParameterPack<N - 1, OtherTypes...>::Type Type;
	};

	template <typename T, typename... OtherTypes>
	struct TNthTypeFromParameterPack<0, T, OtherTypes...>
	{
		typedef T Type;
	};

	template <typename T, typename... Types>
	struct TDecayedFrontOfParameterPackIsSameType
	{
		enum { Value = TAreTypesEqual<T, typename TDecay<typename TNthTypeFromParameterPack<0, Types...>::Type>::Type>::Value };
	};

	template <typename T, uint32 Index>
	struct TTupleElement
	{
		template <
			typename... ArgTypes,
			typename = typename TEnableIf<
				TAndValue<
					sizeof...(ArgTypes) != 0,
					TOrValue<
						sizeof...(ArgTypes) != 1,
						TNot<UE4Tuple_Private::TDecayedFrontOfParameterPackIsSameType<TTupleElement, ArgTypes...>>
					>
				>::Value
			>::Type
		>
		explicit TTupleElement(ArgTypes&&... Args)
			: Value(Forward<ArgTypes>(Args)...)
		{
		}

		TTupleElement()
			: Value()
		{
		}

		#if TUPLES_USE_DEFAULTED_FUNCTIONS

			TTupleElement(TTupleElement&&) = default;
			TTupleElement(const TTupleElement&) = default;
			TTupleElement& operator=(TTupleElement&&) = default;
			TTupleElement& operator=(const TTupleElement&) = default;

		#else

			TTupleElement(TTupleElement&& Other)
				: Value(MoveTemp(Other.Value))
			{
			}

			TTupleElement(const TTupleElement& Other)
				: Value(Other.Value)
			{
			}

			void operator=(TTupleElement&& Other)
			{
				Value = MoveTemp(Other.Value);
			}

			void operator=(const TTupleElement& Other)
			{
				Value = Other.Value;
			}

		#endif

		T Value;
	};

	template <uint32 IterIndex, uint32 Index, typename... Types>
	struct TTupleElementHelperImpl;

	template <uint32 IterIndex, uint32 Index, typename ElementType, typename... Types>
	struct TTupleElementHelperImpl<IterIndex, Index, ElementType, Types...> : TTupleElementHelperImpl<IterIndex + 1, Index, Types...>
	{
	};

	template <uint32 Index, typename ElementType, typename... Types>
	struct TTupleElementHelperImpl<Index, Index, ElementType, Types...>
	{
		typedef ElementType Type;

		template <typename TupleType>
		static FORCEINLINE ElementType& Get(TupleType& Tuple)
		{
			return static_cast<TTupleElement<ElementType, Index>&>(Tuple).Value;
		}

		template <typename TupleType>
		static FORCEINLINE const ElementType& Get(const TupleType& Tuple)
		{
			return Get((TupleType&)Tuple);
		}
	};

	template <uint32 WantedIndex, typename... Types>
	struct TTupleElementHelper : TTupleElementHelperImpl<0, WantedIndex, Types...>
	{
	};

	template <uint32 ArgCount, uint32 ArgToCompare>
	struct FEqualityHelper
	{
		template <typename TupleType>
		FORCEINLINE static bool Compare(const TupleType& Lhs, const TupleType& Rhs)
		{
			return Lhs.template Get<ArgToCompare>() == Rhs.template Get<ArgToCompare>() && FEqualityHelper<ArgCount, ArgToCompare + 1>::Compare(Lhs, Rhs);
		}
	};

	template <uint32 ArgCount>
	struct FEqualityHelper<ArgCount, ArgCount>
	{
		template <typename TupleType>
		FORCEINLINE static bool Compare(const TupleType& Lhs, const TupleType& Rhs)
		{
			return true;
		}
	};

	template <uint32 NumArgs, uint32 ArgToCompare = 0, bool Last = ArgToCompare + 1 == NumArgs>
	struct TLessThanHelper
	{
		template <typename TupleType>
		FORCEINLINE static bool Do(const TupleType& Lhs, const TupleType& Rhs)
		{
			return Lhs.template Get<ArgToCompare>() < Rhs.template Get<ArgToCompare>() || (!(Rhs.template Get<ArgToCompare>() < Lhs.template Get<ArgToCompare>()) && TLessThanHelper<NumArgs, ArgToCompare + 1>::Do(Lhs, Rhs));
		}
	};

	template <uint32 NumArgs, uint32 ArgToCompare>
	struct TLessThanHelper<NumArgs, ArgToCompare, true>
	{
		template <typename TupleType>
		FORCEINLINE static bool Do(const TupleType& Lhs, const TupleType& Rhs)
		{
			return Lhs.template Get<ArgToCompare>() < Rhs.template Get<ArgToCompare>();
		}
	};

	template <uint32 NumArgs>
	struct TLessThanHelper<NumArgs, NumArgs, false>
	{
		template <typename TupleType>
		FORCEINLINE static bool Do(const TupleType& Lhs, const TupleType& Rhs)
		{
			return false;
		}
	};

	template <typename Indices, typename... Types>
	struct TTupleStorage;

	template <uint32... Indices, typename... Types>
	struct TTupleStorage<TIntegerSequence<uint32, Indices...>, Types...> : TTupleElement<Types, Indices>...
	{
		template <
			typename... ArgTypes,
			typename = typename TEnableIf<
				TAndValue<
					sizeof...(ArgTypes) == sizeof...(Types) && sizeof...(ArgTypes) != 0,
					TOrValue<
						sizeof...(ArgTypes) != 1,
						TNot<UE4Tuple_Private::TDecayedFrontOfParameterPackIsSameType<TTupleStorage, ArgTypes...>>
					>
				>::Value
			>::Type
		>
		explicit TTupleStorage(ArgTypes&&... Args)
			: TTupleElement<Types, Indices>(Forward<ArgTypes>(Args))...
		{
		}

		#if TUPLES_USE_DEFAULTED_FUNCTIONS

			TTupleStorage() = default;
			TTupleStorage(TTupleStorage&&) = default;
			TTupleStorage(const TTupleStorage&) = default;
			TTupleStorage& operator=(TTupleStorage&&) = default;
			TTupleStorage& operator=(const TTupleStorage&) = default;

		#else

			TTupleStorage()
				: TTupleElement<Types, Indices>()...
			{
			}

			TTupleStorage(TTupleStorage&& Other)
				: TTupleElement<Types, Indices>(MoveTemp(*(TTupleElement<Types, Indices>*)&Other))...
			{
			}

			TTupleStorage(const TTupleStorage& Other)
				: TTupleElement<Types, Indices>(*(const TTupleElement<Types, Indices>*)&Other)...
			{
			}

			void operator=(TTupleStorage&& Other)
			{
				int Temp[] = { 0, (*(TTupleElement<Types, Indices>*)this = MoveTemp(*(TTupleElement<Types, Indices>*)&Other), 0)... };
				(void)Temp;
			}

			void operator=(const TTupleStorage& Other)
			{
				int Temp[] = { 0, (*(TTupleElement<Types, Indices>*)this = *(const TTupleElement<Types, Indices>*)&Other, 0)... };
				(void)Temp;
			}

		#endif

		template <uint32 Index> FORCEINLINE const typename TTupleElementHelper<Index, Types...>::Type& Get() const { return TTupleElementHelper<Index, Types...>::Get(*this); }
		template <uint32 Index> FORCEINLINE       typename TTupleElementHelper<Index, Types...>::Type& Get()       { return TTupleElementHelper<Index, Types...>::Get(*this); }
	};

	// Specialization of 2-TTuple to give it the API of TPair.
	template <typename InKeyType, typename InValueType>
	struct TTupleStorage<TIntegerSequence<uint32, 0, 1>, InKeyType, InValueType>
	{
	private:
		template <uint32 Index, typename Dummy> // Dummy needed for partial template specialization workaround
		struct TGetHelper;

		template <typename Dummy>
		struct TGetHelper<0, Dummy>
		{
			typedef InKeyType ResultType;

			static const InKeyType& Get(const TTupleStorage& Tuple) { return Tuple.Key; }
			static       InKeyType& Get(      TTupleStorage& Tuple) { return Tuple.Key; }
		};

		template <typename Dummy>
		struct TGetHelper<1, Dummy>
		{
			typedef InValueType ResultType;

			static const InValueType& Get(const TTupleStorage& Tuple) { return Tuple.Value; }
			static       InValueType& Get(      TTupleStorage& Tuple) { return Tuple.Value; }
		};

	public:
		typedef InKeyType   KeyType;
		typedef InValueType ValueType;

		template <typename KeyInitType, typename ValueInitType>
		explicit TTupleStorage(KeyInitType&& KeyInit, ValueInitType&& ValueInit)
			: Key  (Forward<KeyInitType  >(KeyInit  ))
			, Value(Forward<ValueInitType>(ValueInit))
		{
		}

		TTupleStorage()
			: Key()
			, Value()
		{
		}

		#if TUPLES_USE_DEFAULTED_FUNCTIONS

			TTupleStorage(TTupleStorage&&) = default;
			TTupleStorage(const TTupleStorage&) = default;
			TTupleStorage& operator=(TTupleStorage&&) = default;
			TTupleStorage& operator=(const TTupleStorage&) = default;

		#else

			TTupleStorage(TTupleStorage&& Other)
				: Key  (MoveTemp(Other.Key))
				, Value(MoveTemp(Other.Value))
			{
			}

			TTupleStorage(const TTupleStorage& Other)
				: Key  (Other.Key)
				, Value(Other.Value)
			{
			}

			void operator=(TTupleStorage&& Other)
			{
				Key   = MoveTemp(Other.Key);
				Value = MoveTemp(Other.Value);
			}

			void operator=(const TTupleStorage& Other)
			{
				Key   = Other.Key;
				Value = Other.Value;
			}

		#endif

		template <uint32 Index> FORCEINLINE const typename TGetHelper<Index, void>::ResultType& Get() const { return TGetHelper<Index, void>::Get(*this); }
		template <uint32 Index> FORCEINLINE       typename TGetHelper<Index, void>::ResultType& Get()       { return TGetHelper<Index, void>::Get(*this); }

		InKeyType   Key;
		InValueType Value;
	};

	template <typename Indices, typename... Types>
	struct TTupleImpl;

	template <uint32... Indices, typename... Types>
	struct TTupleImpl<TIntegerSequence<uint32, Indices...>, Types...> : TTupleStorage<TIntegerSequence<uint32, Indices...>, Types...>
	{
	private:
		typedef TTupleStorage<TIntegerSequence<uint32, Indices...>, Types...> Super;

	public:
		using Super::Get;

		template <
			typename... ArgTypes,
			typename = typename TEnableIf<
				TAndValue<
					sizeof...(ArgTypes) == sizeof...(Types) && sizeof...(ArgTypes) != 0,
					TOrValue<
						sizeof...(ArgTypes) != 1,
						TNot<UE4Tuple_Private::TDecayedFrontOfParameterPackIsSameType<TTupleImpl, ArgTypes...>>
					>
				>::Value
			>::Type
		>
		explicit TTupleImpl(ArgTypes&&... Args)
			: Super(Forward<ArgTypes>(Args)...)
		{
		}

		#if TUPLES_USE_DEFAULTED_FUNCTIONS

			TTupleImpl() = default;
			TTupleImpl(TTupleImpl&& Other) = default;
			TTupleImpl(const TTupleImpl& Other) = default;
			TTupleImpl& operator=(TTupleImpl&& Other) = default;
			TTupleImpl& operator=(const TTupleImpl& Other) = default;

		#else

			TTupleImpl()
				: Super()
			{
			}

			TTupleImpl(TTupleImpl&& Other)
				: Super(MoveTemp(*(Super*)&Other))
			{
			}

			TTupleImpl(const TTupleImpl& Other)
				: Super(*(const Super*)&Other)
			{
			}

			void operator=(TTupleImpl&& Other)
			{
				*(Super*)this = MoveTemp(*(Super*)&Other);
			}

			void operator=(const TTupleImpl& Other)
			{
				*(Super*)this = *(const Super*)&Other;
			}

		#endif

		template <typename FuncType, typename... ArgTypes>
		#if USE_TUPLE_AUTO_RETURN_TYPES
			decltype(auto) ApplyAfter(FuncType&& Func, ArgTypes&&... Args) const
		#else
			auto ApplyAfter(FuncType&& Func, ArgTypes&&... Args) const -> decltype(Func(Forward<ArgTypes>(Args)..., Get<Indices>()...))
		#endif
		{
			return Func(Forward<ArgTypes>(Args)..., this->template Get<Indices>()...);
		}

		template <typename FuncType, typename... ArgTypes>
		#if USE_TUPLE_AUTO_RETURN_TYPES
			decltype(auto) ApplyBefore(FuncType&& Func, ArgTypes&&... Args) const
		#else
			auto ApplyBefore(FuncType&& Func, ArgTypes&&... Args) const -> decltype(Func(Get<Indices>()..., Forward<ArgTypes>(Args)...))
		#endif
		{
			return Func(this->template Get<Indices>()..., Forward<ArgTypes>(Args)...);
		}

		FORCEINLINE friend FArchive& operator<<(FArchive& Ar, TTupleImpl& Tuple)
		{
			// This should be implemented with a fold expression when our compilers support it
			int Temp[] = { 0, (Ar << Tuple.template Get<Indices>(), 0)... };
			(void)Temp;
			return Ar;
		}

		FORCEINLINE friend bool operator==(const TTupleImpl& Lhs, const TTupleImpl& Rhs)
		{
			// This could be implemented with a fold expression when our compilers support it
			return FEqualityHelper<sizeof...(Types), 0>::Compare(Lhs, Rhs);
		}

		FORCEINLINE friend bool operator!=(const TTupleImpl& Lhs, const TTupleImpl& Rhs)
		{
			return !(Lhs == Rhs);
		}

		FORCEINLINE friend bool operator<(const TTupleImpl& Lhs, const TTupleImpl& Rhs)
		{
			return TLessThanHelper<sizeof...(Types)>::Do(Lhs, Rhs);
		}

		FORCEINLINE friend bool operator<=(const TTupleImpl& Lhs, const TTupleImpl& Rhs)
		{
			return !(Rhs < Lhs);
		}

		FORCEINLINE friend bool operator>(const TTupleImpl& Lhs, const TTupleImpl& Rhs)
		{
			return Rhs < Lhs;
		}

		FORCEINLINE friend bool operator>=(const TTupleImpl& Lhs, const TTupleImpl& Rhs)
		{
			return !(Lhs < Rhs);
		}
	};

	#ifdef _MSC_VER

		// Not strictly necessary, but some VC versions give a 'syntax error: <fake-expression>' error
		// for empty tuples.
		template <>
		struct TTupleImpl<TIntegerSequence<uint32>>
		{
			explicit TTupleImpl()
			{
			}

			// Doesn't matter what these return, or even have a function body, but they need to be declared
			template <uint32 Index> FORCEINLINE const int32& Get() const;
			template <uint32 Index> FORCEINLINE       int32& Get();

			template <typename FuncType, typename... ArgTypes>
			#if USE_TUPLE_AUTO_RETURN_TYPES
				decltype(auto) ApplyAfter(FuncType&& Func, ArgTypes&&... Args) const
			#else
				auto ApplyAfter(FuncType&& Func, ArgTypes&&... Args) const -> decltype(Func(Forward<ArgTypes>(Args)...))
			#endif
			{
				return Func(Forward<ArgTypes>(Args)...);
			}

			template <typename FuncType, typename... ArgTypes>
			#if USE_TUPLE_AUTO_RETURN_TYPES
				decltype(auto) ApplyBefore(FuncType&& Func, ArgTypes&&... Args) const
			#else
				auto ApplyBefore(FuncType&& Func, ArgTypes&&... Args) const -> decltype(Func(Forward<ArgTypes>(Args)...))
			#endif
			{
				return Func(Forward<ArgTypes>(Args)...);
			}

			FORCEINLINE friend FArchive& operator<<(FArchive& Ar, TTupleImpl& Tuple)
			{
				return Ar;
			}

			FORCEINLINE friend bool operator==(const TTupleImpl& Lhs, const TTupleImpl& Rhs)
			{
				return true;
			}

			FORCEINLINE friend bool operator!=(const TTupleImpl& Lhs, const TTupleImpl& Rhs)
			{
				return false;
			}

			FORCEINLINE friend bool operator<(const TTupleImpl& Lhs, const TTupleImpl& Rhs)
			{
				return false;
			}

			FORCEINLINE friend bool operator<=(const TTupleImpl& Lhs, const TTupleImpl& Rhs)
			{
				return true;
			}

			FORCEINLINE friend bool operator>(const TTupleImpl& Lhs, const TTupleImpl& Rhs)
			{
				return false;
			}

			FORCEINLINE friend bool operator>=(const TTupleImpl& Lhs, const TTupleImpl& Rhs)
			{
				return true;
			}
		};

	#endif

	template <typename IntegerSequence>
	struct TTransformTuple_Impl;

	template <uint32... Indices>
	struct TTransformTuple_Impl<TIntegerSequence<uint32, Indices...>>
	{
		template <typename TupleType, typename FuncType>
		#if USE_TUPLE_AUTO_RETURN_TYPES
			static decltype(auto) Do(TupleType&& Tuple, FuncType Func)
		#else
			static auto Do(TupleType&& Tuple, FuncType Func) -> decltype(MakeTuple(Func(Forward<TupleType>(Tuple).template Get<Indices>())...))
		#endif
		{
			return MakeTuple(Func(Forward<TupleType>(Tuple).template Get<Indices>())...);
		}
	};

	template <typename IntegerSequence>
	struct TVisitTupleElements_Impl;

	template <uint32... Indices>
	struct TVisitTupleElements_Impl<TIntegerSequence<uint32, Indices...>>
	{
		template <typename TupleType, typename FuncType>
		static void Do(TupleType&& Tuple, FuncType Func)
		{
			// This should be implemented with a fold expression when our compilers support it
			int Temp[] = { 0, (Func(Tuple.template Get<Indices>()), 0)... };
			(void)Temp;
		}
	};


	template <typename TupleType>
	struct TCVTupleArity;

	template <typename... Types>
	struct TCVTupleArity<const volatile TTuple<Types...>>
	{
		enum { Value = sizeof...(Types) };
	};
}

template <typename... Types>
struct TTuple : UE4Tuple_Private::TTupleImpl<TMakeIntegerSequence<uint32, sizeof...(Types)>, Types...>
{
private:
	typedef UE4Tuple_Private::TTupleImpl<TMakeIntegerSequence<uint32, sizeof...(Types)>, Types...> Super;

public:
	template <
		typename... ArgTypes,
		typename = typename TEnableIf<
			TAndValue<
				sizeof...(ArgTypes) == sizeof...(Types) && sizeof...(ArgTypes) != 0,
				TOrValue<
					sizeof...(ArgTypes) != 1,
					TNot<UE4Tuple_Private::TDecayedFrontOfParameterPackIsSameType<TTuple, ArgTypes...>>
				>
			>::Value
		>::Type
	>
	explicit TTuple(ArgTypes&&... Args)
		: Super(Forward<ArgTypes>(Args)...)
	{
		// This constructor is disabled for TTuple and zero parameters because VC is incorrectly instantiating it as a move/copy/default constructor.
	}

	#if TUPLES_USE_DEFAULTED_FUNCTIONS

		TTuple() = default;
		TTuple(TTuple&&) = default;
		TTuple(const TTuple&) = default;
		TTuple& operator=(TTuple&&) = default;
		TTuple& operator=(const TTuple&) = default;

	#else

		TTuple()
		{
		}

		TTuple(TTuple&& Other)
			: Super(MoveTemp(*(Super*)&Other))
		{
		}

		TTuple(const TTuple& Other)
			: Super(*(const Super*)&Other)
		{
		}

		TTuple& operator=(TTuple&& Other)
		{
			*(Super*)this = MoveTemp(*(Super*)&Other);
			return *this;
		}

		TTuple& operator=(const TTuple& Other)
		{
			*(Super*)this = *(const Super*)&Other;
			return *this;
		}

	#endif
};


/**
 * Traits class which calculates the number of elements in a tuple.
 */
template <typename TupleType>
struct TTupleArity : UE4Tuple_Private::TCVTupleArity<const volatile TupleType>
{
};


/**
 * Makes a TTuple from some arguments.  The type of the TTuple elements are the decayed versions of the arguments.
 *
 * @param  Args  The arguments used to construct the tuple.
 * @return A tuple containing a copy of the arguments.
 *
 * Example:
 *
 * void Func(const int32 A, FString&& B)
 * {
 *     // Equivalent to:
 *     // TTuple<int32, const TCHAR*, FString> MyTuple(A, TEXT("Hello"), MoveTemp(B));
 *     auto MyTuple = MakeTuple(A, TEXT("Hello"), MoveTemp(B));
 * }
 */
template <typename... Types>
TTuple<typename TDecay<Types>::Type...> MakeTuple(Types&&... Args)
{
	return TTuple<typename TDecay<Types>::Type...>(Forward<Types>(Args)...);
}


/**
 * Creates a new TTuple by applying a functor to each of the elements.
 *
 * @param  Tuple  The tuple to apply the functor to.
 * @param  Func   The functor to apply.
 *
 * @return A new tuple of the transformed elements.
 *
 * Example:
 *
 * float        Overloaded(int32 Arg);
 * char         Overloaded(const TCHAR* Arg);
 * const TCHAR* Overloaded(const FString& Arg);
 *
 * void Func(const TTuple<int32, const TCHAR*, FString>& MyTuple)
 * {
 *     // Equivalent to:
 *     // TTuple<float, char, const TCHAR*> TransformedTuple(Overloaded(MyTuple.Get<0>()), Overloaded(MyTuple.Get<1>()), Overloaded(MyTuple.Get<2>())));
 *     auto TransformedTuple = TransformTuple(MyTuple, [](const auto& Arg) { return Overloaded(Arg); });
 * }
 */
template <typename FuncType, typename... Types>
#if USE_TUPLE_AUTO_RETURN_TYPES
	FORCEINLINE decltype(auto) TransformTuple(TTuple<Types...>&& Tuple, FuncType Func)
#else
	FORCEINLINE auto TransformTuple(TTuple<Types...>&& Tuple, FuncType Func) -> decltype(UE4Tuple_Private::TTransformTuple_Impl<TMakeIntegerSequence<uint32, sizeof...(Types)>>::Do(MoveTemp(Tuple), MoveTemp(Func)))
#endif
{
	return UE4Tuple_Private::TTransformTuple_Impl<TMakeIntegerSequence<uint32, sizeof...(Types)>>::Do(MoveTemp(Tuple), MoveTemp(Func));
}

template <typename FuncType, typename... Types>
#if USE_TUPLE_AUTO_RETURN_TYPES
	FORCEINLINE decltype(auto) TransformTuple(const TTuple<Types...>& Tuple, FuncType Func)
#else
	FORCEINLINE auto TransformTuple(const TTuple<Types...>& Tuple, FuncType Func) -> decltype(UE4Tuple_Private::TTransformTuple_Impl<TMakeIntegerSequence<uint32, sizeof...(Types)>>::Do(Tuple, MoveTemp(Func)))
#endif
{
	return UE4Tuple_Private::TTransformTuple_Impl<TMakeIntegerSequence<uint32, sizeof...(Types)>>::Do(Tuple, MoveTemp(Func));
}


/**
 * Visits each element in the tuple in turn and applies the supplied functor to it.
 *
 * @param  Tuple  The tuple to apply the functor to.
 * @param  Func   The functor to apply.
 */
template <typename TupleType, typename FuncType>
FORCEINLINE void VisitTupleElements(TupleType& Tuple, FuncType Func)
{
	UE4Tuple_Private::TVisitTupleElements_Impl<TMakeIntegerSequence<uint32, TTupleArity<TupleType>::Value>>::Do(Tuple, MoveTemp(Func));
}
