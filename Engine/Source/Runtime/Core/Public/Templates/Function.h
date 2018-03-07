// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "HAL/UnrealMemory.h"
#include "Templates/AreTypesEqual.h"
#include "Templates/UnrealTypeTraits.h"
#include "Templates/RemoveReference.h"
#include "Templates/Decay.h"
#include "Templates/Invoke.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "Math/UnrealMathUtility.h"
#include <new>


// Disable visualization hack for shipping or test builds.
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	#define ENABLE_TFUNCTIONREF_VISUALIZATION 1
#else
	#define ENABLE_TFUNCTIONREF_VISUALIZATION 0
#endif

template <typename FuncType> class TFunction;
template <typename FuncType> class TFunctionRef;

/**
 * TFunction<FuncType>
 *
 * See the class definition for intended usage.
 */
template <typename FuncType>
class TFunction;

/**
 * TFunctionRef<FuncType>
 *
 * See the class definition for intended usage.
 */
template <typename FuncType>
class TFunctionRef;

/**
 * Traits class which checks if T is a TFunction<> type.
 */
template <typename T> struct TIsATFunction               { enum { Value = false }; };
template <typename T> struct TIsATFunction<TFunction<T>> { enum { Value = true  }; };

/**
 * Traits class which checks if T is a TFunction<> type.
 */
template <typename T> struct TIsATFunctionRef                  { enum { Value = false }; };
template <typename T> struct TIsATFunctionRef<TFunctionRef<T>> { enum { Value = true  }; };

/**
 * Private implementation details of TFunction and TFunctionRef.
 */
namespace UE4Function_Private
{
	struct FFunctionStorage;

	/**
	 * Common interface to a callable object owned by TFunction.
	 */
	struct IFunction_OwnedObject
	{
		/**
		 * Creates a copy of the object in the allocator and returns a pointer to it.
		 */
		virtual IFunction_OwnedObject* CopyToEmptyStorage(FFunctionStorage& Storage) const = 0;

		/**
		 * Returns the address of the object.
		 */
		virtual void* GetAddress() = 0;

		/**
		 * Destructor.
		 */
		virtual ~IFunction_OwnedObject() = 0;
	};

	/**
	 * Destructor.
	 */
	inline IFunction_OwnedObject::~IFunction_OwnedObject()
	{
	}

	#if !defined(_WIN32) || defined(_WIN64)
		// Let TFunction store up to 32 bytes which are 16-byte aligned before we heap allocate
		typedef TAlignedBytes<16, 16> AlignedInlineFunctionType;
		typedef TInlineAllocator<2> FunctionAllocatorType;
	#else
		// ... except on Win32, because we can't pass 16-byte aligned types by value, as some TFunctions are.
		// So we'll just keep it heap-allocated, which is always sufficiently aligned.
		typedef TAlignedBytes<16, 8> AlignedInlineFunctionType;
		typedef FHeapAllocator FunctionAllocatorType;
	#endif

	struct FFunctionStorage
	{
		FFunctionStorage()
			: AllocatedSize(0)
		{
		}

		FFunctionStorage(FFunctionStorage&& Other)
			: AllocatedSize(0)
		{
			Allocator.MoveToEmpty(Other.Allocator);
			AllocatedSize       = Other.AllocatedSize;
			Other.AllocatedSize = 0;
		}

		void Empty()
		{
			Allocator.ResizeAllocation(0, 0, sizeof(UE4Function_Private::AlignedInlineFunctionType));
			AllocatedSize = 0;
		}

		typedef FunctionAllocatorType::ForElementType<AlignedInlineFunctionType> AllocatorType;

		IFunction_OwnedObject* GetBoundObject() const
		{
			return AllocatedSize ? (IFunction_OwnedObject*)Allocator.GetAllocation() : nullptr;
		}

		AllocatorType Allocator;
		int32         AllocatedSize;
	};
}

inline void* operator new(size_t Size, UE4Function_Private::FFunctionStorage& Storage)
{
	if (UE4Function_Private::IFunction_OwnedObject* Obj = Storage.GetBoundObject())
	{
		Obj->~IFunction_OwnedObject();
	}

	int32 NewSize = FMath::DivideAndRoundUp(Size, sizeof(UE4Function_Private::AlignedInlineFunctionType));
	if (Storage.AllocatedSize != NewSize)
	{
		Storage.Allocator.ResizeAllocation(0, NewSize, sizeof(UE4Function_Private::AlignedInlineFunctionType));
		Storage.AllocatedSize = NewSize;
	}

	return Storage.Allocator.GetAllocation();
}

namespace UE4Function_Private
{
	/**
	 * Implementation of IFunction_OwnedObject for a given T.
	 */
	template <typename T>
	struct TFunction_OwnedObject : public IFunction_OwnedObject
	{
		/**
		 * Constructor which creates its T by copying.
		 */
		explicit TFunction_OwnedObject(const T& InObj)
			: Obj(InObj)
		{
		}

		/**
		 * Constructor which creates its T by moving.
		 */
		explicit TFunction_OwnedObject(T&& InObj)
			: Obj(MoveTemp(InObj))
		{
		}

		IFunction_OwnedObject* CopyToEmptyStorage(FFunctionStorage& Storage) const override
		{
			return new (Storage) TFunction_OwnedObject(Obj);
		}

		virtual void* GetAddress() override
		{
			return &Obj;
		}

		T Obj;
	};

	/**
	 * A class which is used to instantiate the code needed to call a bound function.
	 */
	template <typename Functor, typename FuncType>
	struct TFunctionRefCaller;

	/**
	 * A class which is used to instantiate the code needed to assert when called - used for unset bindings.
	 */
	template <typename FuncType>
	struct TFunctionRefAsserter;

	/**
	 * A class which defines an operator() which will invoke the TFunctionRefCaller::Call function.
	 */
	template <typename DerivedType, typename FuncType>
	struct TFunctionRefBase;

	#if ENABLE_TFUNCTIONREF_VISUALIZATION
		/**
		 * Helper classes to help debugger visualization.
		 */
		struct IDebugHelper
		{
			virtual ~IDebugHelper() = 0;
		};

		inline IDebugHelper::~IDebugHelper()
		{
		}

		template <typename T>
		struct TDebugHelper : IDebugHelper
		{
			T* Ptr;
		};
	#endif

	/**
	 * Function which invokes the passed callable when invoked.
	 */
	template <typename Functor, typename Ret, typename... ParamTypes>
	struct TFunctionRefCaller<Functor, Ret (ParamTypes...)>
	{
		static Ret Call(void* Obj, ParamTypes&... Params)
		{
			return Invoke(*(Functor*)Obj, Forward<ParamTypes>(Params)...);
		}
	};

	template <typename Functor, typename... ParamTypes>
	struct TFunctionRefCaller<Functor, void (ParamTypes...)>
	{
		static void Call(void* Obj, ParamTypes&... Params)
		{
			Invoke(*(Functor*)Obj, Forward<ParamTypes>(Params)...);
		}
	};

	/**
	 * Function which asserts when invoked.
	 */
	template <typename Ret, typename... ParamTypes>
	struct TFunctionRefAsserter<Ret (ParamTypes...)>
	{
		static Ret Call(void* Obj, ParamTypes&...)
		{
			checkf(false, TEXT("Attempting to call an unbound TFunction!"));

			// This doesn't need to be valid, because it'll never be reached, but it does at least need to compile.
			return Forward<Ret&&>(*(typename TRemoveReference<Ret>::Type*)Obj);
		}
	};

	template <typename... ParamTypes>
	struct TFunctionRefAsserter<void (ParamTypes...)>
	{
		static void Call(void*, ParamTypes&...)
		{
			checkf(false, TEXT("Attempting to call an unbound TFunction!"));
		}
	};

	template <typename DerivedType, typename Ret, typename... ParamTypes>
	struct TFunctionRefBase<DerivedType, Ret (ParamTypes...)>
	{
		explicit TFunctionRefBase(ENoInit)
		{
			// Not really designed to be initialized directly, but we want to be explicit about that.
		}

		Ret operator()(ParamTypes... Params) const
		{
			const DerivedType* Derived = static_cast<const DerivedType*>(this);
			return Callable(Derived->GetPtr(), Params...);
		}

	protected:
		template <typename FunctorType>
		void Set(FunctorType* Functor)
		{
			Callable = &UE4Function_Private::TFunctionRefCaller<FunctorType, Ret (ParamTypes...)>::Call;

			#if ENABLE_TFUNCTIONREF_VISUALIZATION
				// We placement new over the top of the same object each time.  This is illegal,
				// but it ensures that the vptr is set correctly for the bound type, and so is
				// visualizable.  We never depend on the state of this object at runtime, so it's
				// ok.
				new ((void*)&DebugPtrStorage) UE4Function_Private::TDebugHelper<FunctorType>;
				DebugPtrStorage.Ptr = (void*)Functor;
			#endif
		}

		void CopyAndReseat(const TFunctionRefBase& Other, void* Functor)
		{
			Callable = Other.Callable;

			#if ENABLE_TFUNCTIONREF_VISUALIZATION
				// Use Memcpy to copy the other DebugPtrStorage, including vptr (because we don't know the bound type
				// here), and then reseat the underlying pointer.  Possibly even more evil than the Set code.
				FMemory::Memcpy(&DebugPtrStorage, &Other.DebugPtrStorage, sizeof(DebugPtrStorage));
				DebugPtrStorage.Ptr = Functor;
			#endif
		}

		void Unset()
		{
			Callable = &UE4Function_Private::TFunctionRefAsserter<Ret (ParamTypes...)>::Call;
		}

	private:
		// A pointer to a function which invokes the call operator on the callable object
		Ret (*Callable)(void*, ParamTypes&...);

		#if ENABLE_TFUNCTIONREF_VISUALIZATION
			// To help debug visualizers
			UE4Function_Private::TDebugHelper<void> DebugPtrStorage;
		#endif
	};
}

/**
 * TFunctionRef<FuncType>
 *
 * A class which represents a reference to something callable.  The important part here is *reference* - if
 * you bind it to a lambda and the lambda goes out of scope, you will be left with an invalid reference.
 *
 * FuncType represents a function type and so TFunctionRef should be defined as follows:
 *
 * // A function taking a string and float and returning int32.  Parameter names are optional.
 * TFunctionRef<int32 (const FString& Name, float Scale)>
 *
 * If you also want to take ownership of the callable thing, e.g. you want to return a lambda from a
 * function, you should use TFunction.  TFunctionRef does not concern itself with ownership because it's
 * intended to be FAST.
 *
 * TFunctionRef is most useful when you want to parameterize a function with some caller-defined code
 * without making it a template.
 *
 * Example:
 *
 * // Something.h
 * void DoSomethingWithConvertingStringsToInts(TFunctionRef<int32 (const FString& Str)> Convert);
 *
 * // Something.cpp
 * void DoSomethingWithConvertingStringsToInts(TFunctionRef<int32 (const FString& Str)> Convert)
 * {
 *     for (const FString& Str : SomeBunchOfStrings)
 *     {
 *         int32 Int = Convert(Str);
 *         DoSomething(Int);
 *     }
 * }
 *
 * // SomewhereElse.cpp
 * #include "Something.h"
 *
 * void Func()
 * {
 *     // First do something using string length
 *     DoSomethingWithConvertingStringsToInts([](const FString& Str) {
 *         return Str.Len();
 *     });
 *
 *     // Then do something using string conversion
 *     DoSomethingWithConvertingStringsToInts([](const FString& Str) {
 *         int32 Result;
 *         TTypeFromString<int32>::FromString(Result, *Str);
 *         return Result;
 *     });
 * }
 */
template <typename FuncType>
class TFunctionRef : public UE4Function_Private::TFunctionRefBase<TFunctionRef<FuncType>, FuncType>
{
	friend struct UE4Function_Private::TFunctionRefBase<TFunctionRef<FuncType>, FuncType>;

	typedef UE4Function_Private::TFunctionRefBase<TFunctionRef<FuncType>, FuncType> Super;

public:
	/**
	 * Constructor which binds a TFunctionRef to a non-const lvalue function object.
	 */
	template <typename FunctorType, typename = typename TEnableIf<!TIsFunction<FunctorType>::Value && !TAreTypesEqual<TFunctionRef, FunctorType>::Value>::Type>
	TFunctionRef(FunctorType& Functor)
		: Super(NoInit)
	{
		// This constructor is disabled for function types because we want it to call the function pointer overload.
		// It is also disabled for TFunctionRef types because VC is incorrectly treating it as a copy constructor.

		Set(&Functor);
	}

	/**
	 * Constructor which binds a TFunctionRef to an rvalue or const lvalue function object.
	 */
	template <typename FunctorType, typename = typename TEnableIf<!TIsFunction<FunctorType>::Value && !TAreTypesEqual<TFunctionRef, FunctorType>::Value>::Type>
	TFunctionRef(const FunctorType& Functor)
		: Super(NoInit)
	{
		// This constructor is disabled for function types because we want it to call the function pointer overload.
		// It is also disabled for TFunctionRef types because VC is incorrectly treating it as a copy constructor.

		Set(&Functor);
	}

	/**
	 * Constructor which binds a TFunctionRef to a function pointer.
	 */
	template <typename FunctionType, typename = typename TEnableIf<TIsFunction<FunctionType>::Value>::Type>
	TFunctionRef(FunctionType* Function)
		: Super(NoInit)
	{
		// This constructor is enabled only for function types because we don't want weird errors from it being called with arbitrary pointers.

		Set(Function);
	}

	#if ENABLE_TFUNCTIONREF_VISUALIZATION

		/**
		 * Copy constructor.
		 */
		TFunctionRef(const TFunctionRef& Other)
			: Super(NoInit)
		{
			// If visualization is enabled, then we need to do an explicit copy
			// to ensure that our hacky DebugPtrStorage's vptr is copied.
			CopyAndReseat(Other, Other.Ptr);
		}

	#endif

	// We delete the assignment operators because we don't want it to be confused with being related to
	// regular C++ reference assignment - i.e. calling the assignment operator of whatever the reference
	// is bound to - because that's not what TFunctionRef does, nor is it even capable of doing that.
	#if PLATFORM_COMPILER_HAS_DEFAULTED_FUNCTIONS

		#if !ENABLE_TFUNCTIONREF_VISUALIZATION
			TFunctionRef(const TFunctionRef&) = default;
		#endif
		TFunctionRef& operator=(const TFunctionRef&) const = delete;
		~TFunctionRef()                                    = default;

	#else

		// We mark copy assignment as private.  We don't define the others because it's mostly likely
		// that this code path is only for VC and the compiler will still generate the others fine anyway,
		// despite such behavior being deprecated.
		private:
			TFunctionRef& operator=(const TFunctionRef&) const;
		public:

	#endif

private:
	/**
	 * Sets the state of the TFunctionRef given a pointer to a callable thing.
	 */
	template <typename FunctorType>
	void Set(FunctorType* Functor)
	{
		// We force a void* cast here because if FunctorType is an actual function then
		// this won't compile.  We convert it back again before we use it anyway.

		Ptr  = (void*)Functor;
		Super::Set(Functor);
	}

	/**
	 * Copies another TFunctionRef and rebinds it to another object of the same type which was originally bound.
	 * Only intended to be used by TFunction's copy constructor/assignment operator.
	 */
	void CopyAndReseat(const TFunctionRef& Other, void* Functor)
	{
		Ptr = Functor;
		Super::CopyAndReseat(Other, Functor);
	}

	/**
	 * Returns a pointer to the callable object - needed by TFunctionRefBase.
	 */
	void* GetPtr() const
	{
		return Ptr;
	}

	// A pointer to the callable object
	void* Ptr;
};

/**
 * TFunction<FuncType>
 *
 * A class which represents a copy of something callable.  FuncType represents a function type and so
 * TFunction should be defined as follows:
 *
 * // A function taking a string and float and returning int32.  Parameter names are optional.
 * TFunction<int32 (const FString& Name, float Scale)>
 *
 * Unlike TFunctionRef, this object is intended to be used like a UE4 version of std::function.  That is,
 * it takes a copy of whatever is bound to it, meaning you can return it from functions and store them in
 * objects without caring about the lifetime of the original object being bound.
 *
 * Example:
 *
 * // Something.h
 * TFunction<FString (int32)> GetTransform();
 *
 * // Something.cpp
 * TFunction<FString (int32)> GetTransform(const FString& Prefix)
 * {
 *     // Squares number and returns it as a string with the specified prefix
 *     return [=](int32 Num) {
 *         return Prefix + TEXT(": ") + TTypeToString<int32>::ToString(Num * Num);
 *     };
 * }
 *
 * // SomewhereElse.cpp
 * #include "Something.h"
 *
 * void Func()
 * {
 *     TFunction<FString (int32)> Transform = GetTransform(TEXT("Hello"));
 *
 *     FString Result = Transform(5); // "Hello: 25"
 * }
 */
template <typename FuncType>
class TFunction : public UE4Function_Private::TFunctionRefBase<TFunction<FuncType>, FuncType>
{
	friend struct UE4Function_Private::TFunctionRefBase<TFunction<FuncType>, FuncType>;

	typedef UE4Function_Private::TFunctionRefBase<TFunction<FuncType>, FuncType> Super;

public:
	/**
	 * Default constructor.
	 */
	TFunction(TYPE_OF_NULLPTR = nullptr)
		: Super(NoInit)
	{
		Super::Unset();
	}

	/**
	 * Constructor which binds a TFunction to any function object.
	 */
	template <typename FunctorType, typename = typename TEnableIf<!TAreTypesEqual<TFunction, typename TDecay<FunctorType>::Type>::Value>::Type>
	TFunction(FunctorType&& InFunc)
		: Super(NoInit)
	{
		// This constructor is disabled for TFunction types because VC is incorrectly treating it as copy/move constructors.

		typedef typename TDecay<FunctorType>::Type                             DecayedFunctorType;
		typedef UE4Function_Private::TFunction_OwnedObject<DecayedFunctorType> OwnedType;

		// This is probably a mistake if you expect TFunction to take a copy of what
		// TFunctionRef is bound to, because that's not possible.
		//
		// If you really intended to bind a TFunction to a TFunctionRef, you can just
		// wrap it in a lambda (and thus it's clear you're just binding to a call to another
		// reference):
		//
		// TFunction<int32(float)> MyFunction = [=](float F) -> int32 { return MyFunctionRef(F); };
		static_assert(!TIsATFunctionRef<DecayedFunctorType>::Value, "Cannot construct a TFunction from a TFunctionRef");

		OwnedType* NewObj = new (Storage) OwnedType(Forward<FunctorType>(InFunc));
		Super::Set(&NewObj->Obj);
	}

	/**
	 * Copy constructor.
	 */
	TFunction(const TFunction& Other)
		: Super(NoInit)
	{
		if (UE4Function_Private::IFunction_OwnedObject* OtherFunc = Other.Storage.GetBoundObject())
		{
			UE4Function_Private::IFunction_OwnedObject* ThisFunc = OtherFunc->CopyToEmptyStorage(Storage);
			Super::CopyAndReseat(Other, ThisFunc->GetAddress());
		}
		else
		{
			Super::Unset();
		}
	}

	/**
	 * Move constructor.
	 */
	TFunction(TFunction&& Other)
		: Super  (NoInit)
		, Storage(MoveTemp(Other.Storage))
	{
		if (UE4Function_Private::IFunction_OwnedObject* Func = Storage.GetBoundObject())
		{
			Super::CopyAndReseat(Other, Func->GetAddress());
		}
		else
		{
			Super::Unset();
		}

		Other.Unset();
	}

	/**
	 * Copy/move assignment operator.
	 */
	TFunction& operator=(TFunction Other)
	{
		FMemory::Memswap(&Other, this, sizeof(TFunction));
		return *this;
	}

	/**
	 * Nullptr assignment operator - unbinds any bound function.
	 */
	TFunction& operator=(TYPE_OF_NULLPTR)
	{
		if (UE4Function_Private::IFunction_OwnedObject* Obj = Storage.GetBoundObject())
		{
			Obj->~IFunction_OwnedObject();
		}
		Storage.Empty();
		Super::Unset();

		return *this;
	}

	/**
	 * Destructor.
	 */
	~TFunction()
	{
		if (UE4Function_Private::IFunction_OwnedObject* Obj = Storage.GetBoundObject())
		{
			Obj->~IFunction_OwnedObject();
		}
	}

	/**
	 * Tests if the TFunction is callable.
	 */
	FORCEINLINE explicit operator bool() const
	{
		return !!Storage.GetBoundObject();
	}

private:
	/**
	 * Returns a pointer to the callable object - needed by TFunctionRefBase.
	 */
	void* GetPtr() const
	{
		UE4Function_Private::IFunction_OwnedObject* Ptr = Storage.GetBoundObject();
		return Ptr ? Ptr->GetAddress() : nullptr;
	}

	UE4Function_Private::FFunctionStorage Storage;
};

/**
 * Nullptr equality operator.
 */
template <typename FuncType>
FORCEINLINE bool operator==(TYPE_OF_NULLPTR, const TFunction<FuncType>& Func)
{
	return !Func;
}

/**
 * Nullptr equality operator.
 */
template <typename FuncType>
FORCEINLINE bool operator==(const TFunction<FuncType>& Func, TYPE_OF_NULLPTR)
{
	return !Func;
}

/**
 * Nullptr inequality operator.
 */
template <typename FuncType>
FORCEINLINE bool operator!=(TYPE_OF_NULLPTR, const TFunction<FuncType>& Func)
{
	return (bool)Func;
}

/**
 * Nullptr inequality operator.
 */
template <typename FuncType>
FORCEINLINE bool operator!=(const TFunction<FuncType>& Func, TYPE_OF_NULLPTR)
{
	return (bool)Func;
}
