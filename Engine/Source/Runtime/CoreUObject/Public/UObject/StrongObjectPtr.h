// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GCObject.h"
#include "UniquePtr.h"

/**
 * Specific implementation of FGCObject that prevents a single UObject-based pointer from being GC'd while this guard is in scope.
 * @note This is the "full-fat" version of FGCObjectScopeGuard which uses a heap-allocated FGCObject so *can* safely be used with containers that treat types as trivially relocatable.
 */
template <typename ObjectType>
class TStrongObjectPtr
{
public:
	TStrongObjectPtr()
		: ReferenceCollector(MakeUnique<FInternalReferenceCollector>(nullptr))
	{
		static_assert(TPointerIsConvertibleFromTo<ObjectType, const volatile UObject>::Value, "TStrongObjectPtr can only be constructed with UObject types");
	}

	explicit TStrongObjectPtr(ObjectType* InObject)
		: ReferenceCollector(MakeUnique<FInternalReferenceCollector>(InObject))
	{
		static_assert(TPointerIsConvertibleFromTo<ObjectType, const volatile UObject>::Value, "TStrongObjectPtr can only be constructed with UObject types");
	}

	TStrongObjectPtr(const TStrongObjectPtr& InOther)
		: ReferenceCollector(MakeUnique<FInternalReferenceCollector>(InOther.Get()))
	{
	}

	template <typename OtherObjectType>
	TStrongObjectPtr(const TStrongObjectPtr<OtherObjectType>& InOther)
		: ReferenceCollector(MakeUnique<FInternalReferenceCollector>(InOther.Get()))
	{
	}

	FORCEINLINE TStrongObjectPtr& operator=(const TStrongObjectPtr& InOther)
	{
		ReferenceCollector->Set(InOther.Get());
		return *this;
	}

	template <typename OtherObjectType>
	FORCEINLINE TStrongObjectPtr& operator=(const TStrongObjectPtr<OtherObjectType>& InOther)
	{
		ReferenceCollector->Set(InOther.Get());
		return *this;
	}

	FORCEINLINE ObjectType& operator*() const
	{
		check(IsValid());
		return *Get();
	}

	FORCEINLINE ObjectType* operator->() const
	{
		check(IsValid());
		return Get();
	}

	FORCEINLINE bool IsValid() const
	{
		return Get() != nullptr;
	}

	FORCEINLINE explicit operator bool() const
	{
		return Get() != nullptr;
	}

	FORCEINLINE ObjectType* Get() const
	{
		return ReferenceCollector->Get();
	}

	FORCEINLINE void Reset(ObjectType* InNewObject = nullptr)
	{
		ReferenceCollector->Set(InNewObject);
	}

	FORCEINLINE friend uint32 GetTypeHash(const TStrongObjectPtr& InStrongObjectPtr)
	{
		return GetTypeHash(InStrongObjectPtr.Get());
	}

private:
	class FInternalReferenceCollector : public FGCObject
	{
	public:
		FInternalReferenceCollector(ObjectType* InObject)
			: Object(InObject)
		{
		}

		virtual ~FInternalReferenceCollector()
		{
		}

		FORCEINLINE ObjectType* Get() const
		{
			return Object;
		}

		FORCEINLINE void Set(ObjectType* InObject)
		{
			Object = InObject;
		}

		//~ FGCObject interface
		virtual void AddReferencedObjects(FReferenceCollector& Collector) override
		{
			Collector.AddReferencedObject(Object);
		}

	private:
		ObjectType* Object;
	};

	TUniquePtr<FInternalReferenceCollector> ReferenceCollector;
};

template <typename LHSObjectType, typename RHSObjectType>
FORCEINLINE bool operator==(const TStrongObjectPtr<LHSObjectType>& InLHS, const TStrongObjectPtr<RHSObjectType>& InRHS)
{
	return InLHS.Get() == InRHS.Get();
}

template <typename LHSObjectType, typename RHSObjectType>
FORCEINLINE bool operator!=(const TStrongObjectPtr<LHSObjectType>& InLHS, const TStrongObjectPtr<RHSObjectType>& InRHS)
{
	return InLHS.Get() != InRHS.Get();
}
