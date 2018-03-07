// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/Class.h"

class AActor;
class APawn;
class APlayerController;
class USoftClassProperty;
class UBlueprint;
class ULevel;
class UPrimitiveComponent;
class USceneComponent;
class USkeletalMeshComponent;
class USkinnedMeshComponent;
class UStaticMeshComponent;
template<class TClass> class TSubclassOf;

FUNCTION_NO_RETURN_START
	COREUOBJECT_API void CastLogError(const TCHAR* FromType, const TCHAR* ToType)
FUNCTION_NO_RETURN_END;

/**
 * Metafunction which detects whether or not a class is an IInterface.  Rules:
 *
 * 1. A UObject is not an IInterface.
 * 2. A type without a UClassType typedef member is not an IInterface.
 * 3. A type whose UClassType::StaticClassFlags does not have CLASS_Interface set is not an IInterface.
 *
 * Otherwise, assume it's an IInterface.
 */
template <typename T, bool bIsAUObject_IMPL = TPointerIsConvertibleFromTo<T, const volatile UObject>::Value>
struct TIsIInterface
{
	enum { Value = false };
};

template <typename T>
struct TIsIInterface<T, false>
{
	template <typename U> static char (&Resolve(typename U::UClassType*))[(U::UClassType::StaticClassFlags & CLASS_Interface) ? 2 : 1];
	template <typename U> static char (&Resolve(...))[1];

	enum { Value = sizeof(Resolve<T>(0)) - 1 };
};

template <typename T>
struct TIsCastable
{
	// It's from-castable if it's an interface or a UObject-derived type
	enum { Value = TIsIInterface<T>::Value || TPointerIsConvertibleFromTo<T, const volatile UObject>::Value };
};

template <typename T>
struct TIsCastableToPointer : TOr<TIsVoidType<T>, TIsCastable<T>>
{
	// It's to-pointer-castable if it's from-castable or void
};

template <typename T>
FORCEINLINE typename TEnableIf<TIsIInterface<T>::Value, FString>::Type GetTypeName()
{
	return T::UClassType::StaticClass()->GetName();
}

template <typename T>
FORCEINLINE typename TEnableIf<!TIsIInterface<T>::Value, FString>::Type GetTypeName()
{
	return T::StaticClass()->GetName();
}

enum class ECastType
{
	UObjectToUObject,
	InterfaceToUObject,
	UObjectToInterface,
	InterfaceToInterface,
	FromCastFlags
};

template <typename Type>
struct TCastFlags
{
	static const uint64 Value = CASTCLASS_None;
};

template <typename From, typename To, bool bFromInterface = TIsIInterface<From>::Value, bool bToInterface = TIsIInterface<To>::Value, uint64 CastClass = TCastFlags<To>::Value>
struct TGetCastType
{
#if UCLASS_FAST_ISA_IMPL == UCLASS_ISA_INDEXTREE || UCLASS_FAST_ISA_IMPL == UCLASS_ISA_CLASSARRAY
	static const ECastType Value = ECastType::UObjectToUObject;
#else
	static const ECastType Value = ECastType::FromCastFlags;
#endif
};

template <typename From, typename To                  > struct TGetCastType<From, To, false, false, CASTCLASS_None> { static const ECastType Value = ECastType::UObjectToUObject;     };
template <typename From, typename To                  > struct TGetCastType<From, To, false, true , CASTCLASS_None> { static const ECastType Value = ECastType::UObjectToInterface;   };
template <typename From, typename To, uint64 CastClass> struct TGetCastType<From, To, true,  false, CastClass     > { static const ECastType Value = ECastType::InterfaceToUObject;   };
template <typename From, typename To, uint64 CastClass> struct TGetCastType<From, To, true,  true , CastClass     > { static const ECastType Value = ECastType::InterfaceToInterface; };

template <typename To, typename From>
To* Cast(From* Src);

template <typename From, typename To, ECastType CastType = TGetCastType<From, To>::Value>
struct TCastImpl
{
	// This is the cast flags implementation
	FORCEINLINE static To* DoCast( UObject* Src )
	{
		return Src && Src->GetClass()->HasAnyCastFlag(TCastFlags<To>::Value) ? (To*)Src : nullptr;
	}

	FORCEINLINE static To* DoCastCheckedWithoutTypeCheck( UObject* Src )
	{
		return (To*)Src;
	}
};

template <typename From, typename To>
struct TCastImpl<From, To, ECastType::UObjectToUObject>
{
	FORCEINLINE static To* DoCast( UObject* Src )
	{
		return Src && Src->IsA<To>() ? (To*)Src : nullptr;
	}

	FORCEINLINE static To* DoCastCheckedWithoutTypeCheck( UObject* Src )
	{
		return (To*)Src;
	}
};

template <typename From, typename To>
struct TCastImpl<From, To, ECastType::InterfaceToUObject>
{
	FORCEINLINE static To* DoCast( From* Src )
	{
		To* Result = nullptr;
		if (Src)
		{
			UObject* Obj = Src->_getUObject();
			if (Obj->IsA<To>())
			{
				Result = (To*)Obj;
			}
		}
		return Result;
	}

	FORCEINLINE static To* DoCastCheckedWithoutTypeCheck( From* Src )
	{
		return Src ? (To*)Src->_getUObject() : nullptr;
	}
};

template <typename From, typename To>
struct TCastImpl<From, To, ECastType::UObjectToInterface>
{
	FORCEINLINE static To* DoCast( UObject* Src )
	{
		return Src ? (To*)Src->GetInterfaceAddress(To::UClassType::StaticClass()) : nullptr;
	}

	FORCEINLINE static To* DoCastCheckedWithoutTypeCheck( UObject* Src )
	{
		return Src ? (To*)Src->GetInterfaceAddress(To::UClassType::StaticClass()) : nullptr;
	}
};

template <typename From, typename To>
struct TCastImpl<From, To, ECastType::InterfaceToInterface>
{
	FORCEINLINE static To* DoCast( From* Src )
	{
		return Src ? (To*)Src->_getUObject()->GetInterfaceAddress(To::UClassType::StaticClass()) : nullptr;
	}

	FORCEINLINE static To* DoCastCheckedWithoutTypeCheck( From* Src )
	{
		return Src ? (To*)Src->_getUObject()->GetInterfaceAddress(To::UClassType::StaticClass()) : nullptr;
	}
};

// Dynamically cast an object type-safely.
template <typename To, typename From>
FORCEINLINE To* Cast(From* Src)
{
	return TCastImpl<From, To>::DoCast(Src);
}

template< class T >
FORCEINLINE T* ExactCast( UObject* Src )
{
	return Src && (Src->GetClass() == T::StaticClass()) ? (T*)Src : nullptr;
}

#if DO_CHECK

	template <typename To, typename From>
	FUNCTION_NON_NULL_RETURN_START
		To* CastChecked(From* Src)
	FUNCTION_NON_NULL_RETURN_END
	{
		if (!Src)
		{
			CastLogError(TEXT("nullptr"), *GetTypeName<To>());
		}

		To* Result = Cast<To>(Src);
		if (!Result)
		{
			CastLogError(*Cast<UObject>(Src)->GetFullName(), *GetTypeName<To>());
		}

		return Result;
	}

	template <typename To, typename From>
	To* CastChecked(From* Src, ECastCheckedType::Type CheckType)
	{
		if (Src)
		{
			To* Result = Cast<To>(Src);
			if (!Result)
			{
				CastLogError(*Cast<UObject>(Src)->GetFullName(), *GetTypeName<To>());
			}

			return Result;
		}

		if (CheckType == ECastCheckedType::NullChecked)
		{
			CastLogError(TEXT("nullptr"), *GetTypeName<To>());
		}

		return nullptr;
	}

#else

	template <typename To, typename From>
	FUNCTION_NON_NULL_RETURN_START
		FORCEINLINE To* CastChecked(From* Src)
	FUNCTION_NON_NULL_RETURN_END
	{
		return TCastImpl<From, To>::DoCastCheckedWithoutTypeCheck(Src);
	}

	template <typename To, typename From>
	FORCEINLINE To* CastChecked(From* Src, ECastCheckedType::Type CheckType)
	{
		return TCastImpl<From, To>::DoCastCheckedWithoutTypeCheck(Src);
	}

#endif

// auto weak versions
template< class T, class U > FORCEINLINE T* Cast       ( const TAutoWeakObjectPtr<U>& Src                                                                   ) { return Cast       <T>(Src.Get()); }
template< class T, class U > FORCEINLINE T* ExactCast  ( const TAutoWeakObjectPtr<U>& Src                                                                   ) { return ExactCast  <T>(Src.Get()); }
template< class T, class U > FORCEINLINE T* CastChecked( const TAutoWeakObjectPtr<U>& Src, ECastCheckedType::Type CheckType = ECastCheckedType::NullChecked ) { return CastChecked<T>(Src.Get(), CheckType); }

// FSubobjectPtr versions
template< class T > FORCEINLINE T* Cast       ( const FSubobjectPtr& Src                                                                   ) { return Cast       <T>(Src.Get()); }
template< class T > FORCEINLINE T* ExactCast  ( const FSubobjectPtr& Src                                                                   ) { return ExactCast  <T>(Src.Get()); }
template< class T > FORCEINLINE T* CastChecked( const FSubobjectPtr& Src, ECastCheckedType::Type CheckType = ECastCheckedType::NullChecked ) { return CastChecked<T>(Src.Get(), CheckType); }

// TSubclassOf versions
template< class T, class U > FORCEINLINE T* Cast       ( const TSubclassOf<U>& Src                                                                   ) { return Cast       <T>(*Src); }
template< class T, class U > FORCEINLINE T* CastChecked( const TSubclassOf<U>& Src, ECastCheckedType::Type CheckType = ECastCheckedType::NullChecked ) { return CastChecked<T>(*Src, CheckType); }

// Const versions of the casts
template< class T, class U > FORCEINLINE const T* Cast       ( const U      * Src                                                                   ) { return Cast       <T>(const_cast<U      *>(Src)); }
template< class T          > FORCEINLINE const T* ExactCast  ( const UObject* Src                                                                   ) { return ExactCast  <T>(const_cast<UObject*>(Src)); }
template< class T, class U > FORCEINLINE const T* CastChecked( const U      * Src, ECastCheckedType::Type CheckType = ECastCheckedType::NullChecked ) { return CastChecked<T>(const_cast<U      *>(Src), CheckType); }

#define DECLARE_CAST_BY_FLAG_FWD(ClassName) class ClassName;
#define DECLARE_CAST_BY_FLAG_CAST(ClassName) \
	template <> \
	struct TCastFlags<ClassName> \
	{ \
		static const uint64 Value = CASTCLASS_##ClassName; \
	}; \
	template <> \
	struct TCastFlags<const ClassName> \
	{ \
		static const uint64 Value = CASTCLASS_##ClassName; \
	};

#define DECLARE_CAST_BY_FLAG(ClassName) \
	DECLARE_CAST_BY_FLAG_FWD(ClassName) \
	DECLARE_CAST_BY_FLAG_CAST(ClassName)

#define FINISH_DECLARING_CAST_FLAGS // intentionally defined to do nothing.

// Define a macro that declares all the cast flags.
// This allows us to reuse these declarations elsewhere to define other properties for these classes.
// Note: When adding an item to this list, you must also add a CASTCLASS_ flag in ObjectBase.h and rebuild UnrealHeaderTool.
#define DECLARE_ALL_CAST_FLAGS \
DECLARE_CAST_BY_FLAG(UField)							\
DECLARE_CAST_BY_FLAG(UEnum)								\
DECLARE_CAST_BY_FLAG(UStruct)							\
DECLARE_CAST_BY_FLAG(UScriptStruct)						\
DECLARE_CAST_BY_FLAG(UClass)							\
DECLARE_CAST_BY_FLAG(UProperty)							\
DECLARE_CAST_BY_FLAG(UObjectPropertyBase)				\
DECLARE_CAST_BY_FLAG(UObjectProperty)					\
DECLARE_CAST_BY_FLAG(UWeakObjectProperty)				\
DECLARE_CAST_BY_FLAG(ULazyObjectProperty)				\
DECLARE_CAST_BY_FLAG(USoftObjectProperty)				\
DECLARE_CAST_BY_FLAG(USoftClassProperty)				\
DECLARE_CAST_BY_FLAG(UBoolProperty)						\
DECLARE_CAST_BY_FLAG(UFunction)							\
DECLARE_CAST_BY_FLAG(UStructProperty)					\
DECLARE_CAST_BY_FLAG(UByteProperty)						\
DECLARE_CAST_BY_FLAG(UIntProperty)						\
DECLARE_CAST_BY_FLAG(UFloatProperty)					\
DECLARE_CAST_BY_FLAG(UDoubleProperty)					\
DECLARE_CAST_BY_FLAG(UClassProperty)					\
DECLARE_CAST_BY_FLAG(UInterfaceProperty)				\
DECLARE_CAST_BY_FLAG(UNameProperty)						\
DECLARE_CAST_BY_FLAG(UStrProperty)						\
DECLARE_CAST_BY_FLAG(UTextProperty)						\
DECLARE_CAST_BY_FLAG(UArrayProperty)					\
DECLARE_CAST_BY_FLAG(UDelegateProperty)					\
DECLARE_CAST_BY_FLAG(UMulticastDelegateProperty)		\
DECLARE_CAST_BY_FLAG(UPackage)							\
DECLARE_CAST_BY_FLAG(ULevel)							\
DECLARE_CAST_BY_FLAG(AActor)							\
DECLARE_CAST_BY_FLAG(APlayerController)					\
DECLARE_CAST_BY_FLAG(APawn)								\
DECLARE_CAST_BY_FLAG(USceneComponent)					\
DECLARE_CAST_BY_FLAG(UPrimitiveComponent)				\
DECLARE_CAST_BY_FLAG(USkinnedMeshComponent)				\
DECLARE_CAST_BY_FLAG(USkeletalMeshComponent)			\
DECLARE_CAST_BY_FLAG(UBlueprint)						\
DECLARE_CAST_BY_FLAG(UDelegateFunction)					\
DECLARE_CAST_BY_FLAG(UStaticMeshComponent)				\
DECLARE_CAST_BY_FLAG(UEnumProperty)						\
FINISH_DECLARING_CAST_FLAGS		// This is here to hopefully remind people to include the "\" in all declarations above, especially when copy/pasting the final line.

// Now actually declare the flags
DECLARE_ALL_CAST_FLAGS

#undef DECLARE_CAST_BY_FLAG
#undef DECLARE_CAST_BY_FLAG_CAST
#undef DECLARE_CAST_BY_FLAG_FWD


#if HACK_HEADER_GENERATOR
// Singleton class to get the cast flag for a given class name.
struct COREUOBJECT_API ClassCastFlagMap
{
	static ClassCastFlagMap& Get();

	// Get Mapped name -> cast flag. Returns CASTCLASS_None if name is not found.
	EClassCastFlags GetCastFlag(const FString& ClassName) const;

private:
	ClassCastFlagMap();
	TMap<FString, EClassCastFlags> CastFlagMap;
};
#endif // HACK_HEADER_GENERATOR



namespace UE4Casts_Private
{
	template <typename To, typename From>
	FORCEINLINE typename TEnableIf<TAnd<TIsPointer<To>, TAnd<TIsCastableToPointer<typename TRemovePointer<To>::Type>, TIsCastable<From>>>::Value, To>::Type DynamicCast(From* Arg)
	{
		typedef typename TRemovePointer<To  >::Type ToValueType;
		typedef typename TRemovePointer<From>::Type FromValueType;

		// Casting away const/volatile
		static_assert(!TLosesQualifiersFromTo<FromValueType, ToValueType>::Value, "Conversion loses qualifiers");

		// If we're casting to void, cast to UObject instead and let it implicitly cast to void
		return Cast<typename TChooseClass<TIsVoidType<ToValueType>::Value, UObject, ToValueType>::Result>(Arg);
	}

	template <typename To, typename From>
	FORCEINLINE typename TEnableIf<!TAnd<TIsPointer<To>, TAnd<TIsCastableToPointer<typename TRemovePointer<To>::Type>, TIsCastable<From>>>::Value, To>::Type DynamicCast(From* Arg)
	{
		return dynamic_cast<To>(Arg);
	}

	template <typename To, typename From>
	FORCEINLINE typename TEnableIf<TAnd<TIsCastable<typename TRemoveReference<To>::Type>, TIsCastable<typename TRemoveReference<From>::Type>>::Value, To>::Type DynamicCast(From&& Arg)
	{
		typedef typename TRemoveReference<From>::Type FromValueType;
		typedef typename TRemoveReference<To  >::Type ToValueType;

		// Casting away const/volatile
		static_assert(!TLosesQualifiersFromTo<FromValueType, ToValueType>::Value, "Conversion loses qualifiers");

		// T&& can only be cast to U&&
		// http://en.cppreference.com/w/cpp/language/dynamic_cast
		static_assert(TOr<TIsLValueReferenceType<From>, TIsRValueReferenceType<To>>::Value, "Cannot dynamic_cast from an rvalue to a non-rvalue reference");

		return Forward<To>(*CastChecked<typename TRemoveReference<To>::Type>(&Arg));
	}

	template <typename To, typename From>
	FORCEINLINE typename TEnableIf<!TAnd<TIsCastable<typename TRemoveReference<To>::Type>, TIsCastable<typename TRemoveReference<From>::Type>>::Value, To>::Type DynamicCast(From&& Arg)
	{
		// This may fail when dynamic_casting rvalue references due to patchy compiler support
		return dynamic_cast<To>(Arg);
	}
}

#define dynamic_cast UE4Casts_Private::DynamicCast
