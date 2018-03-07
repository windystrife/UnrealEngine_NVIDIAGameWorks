// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Curves/KeyHandle.h"
#include "Curves/KeyFrameManipulator.h"

/**
 * Proxy type used to reference a key's time and value
 */
template<typename KeyValueType, typename TimeType>
struct TKeyFrameProxy
{
	TKeyFrameProxy(TimeType InTime, KeyValueType& InValue)
		: Time(InTime)
		, Value(InValue)
	{}

	TKeyFrameProxy* operator->() { return this; }
	const TKeyFrameProxy* operator->() const { return this; }

	/** The key time */
	TimeType Time;

	/** (potentially const) Reference to the key's value */
	KeyValueType& Value;
};

template<typename KeyValueType, typename TimeType> struct TKeyIterator;

/**
 * Templated interface to externally owned curve data data
 */
template<typename KeyValueType, typename TimeType>
class TCurveInterface : public TKeyFrameManipulator<TimeType>
{
public:
	// Pass by value/ref parameter types
	typedef typename TCallTraits<TimeType>::ParamType TimeTypeRef;
	typedef typename TCallTraits<KeyValueType>::ParamType KeyValueTypeRef;

	/**
	 * Construction from externally owned curve data
	 *
	 * @param KeyTimesParam			Ptr to array of key times
	 * @param KeyTimesParam			Ptr to array of key values
	 * @param ExternalKeyHandleLUT	Optional external look-up-table for key handles. If not supplied, an internal temporary LUT will be used.
	 */
	TCurveInterface(TArray<TimeType>* KeyTimesParam, TArray<KeyValueType>* KeyValuesParam, FKeyHandleLookupTable* ExternalKeyHandleLUT = nullptr)
		: TKeyFrameManipulator<TimeType>(KeyTimesParam, ExternalKeyHandleLUT)
		, KeyValues(KeyValuesParam)
	{
	}

	TCurveInterface(const TCurveInterface&) = default;
	TCurveInterface& operator=(const TCurveInterface&) = default;
	virtual ~TCurveInterface() {}

	/**
	  * Add a new key to the curve with the supplied Time and Value. Returns the handle of the new key.
	  * 
	  * @param InTime				The time at which to add the key.
	  * @param InValue				The value of the key.
	  * @return A unique identifier for the newly added key
	  */
	FKeyHandle AddKeyValue(TimeTypeRef InTime, KeyValueTypeRef InValue)
	{
		int32 InsertIndex = this->ComputeInsertIndex(InTime);
		FKeyHandle NewHandle = this->InsertKeyImpl(InTime, InsertIndex);

		KeyValues->Insert(InValue, InsertIndex);
		return NewHandle;
	}

	/**
	 * Attempt to retrieve a key from its handle
	 *
	 * @param 						KeyHandle Handle to the key to get.
	 * @return A proxy to the key, or empty container if it was not found
	 */
	TOptional<TKeyFrameProxy<const KeyValueType, TimeType>> GetKey(FKeyHandle KeyHandle) const
	{
		int32 Index = this->GetIndex(KeyHandle);
		if (Index != INDEX_NONE)
		{
			return TKeyFrameProxy<const KeyValueType, TimeType>(this->GetKeyTimeChecked(Index), (*KeyValues)[Index]);
		}
		return TOptional<TKeyFrameProxy<const KeyValueType, TimeType>>();
	}

	/**
	 * Attempt to retrieve a key from its handle
	 *
	 * @param 						KeyHandle Handle to the key to get.
	 * @return A proxy to the key, or empty container if it was not found
	 */
	TOptional<TKeyFrameProxy<KeyValueType, TimeType>> GetKey(FKeyHandle KeyHandle)
	{
		int32 Index = this->GetIndex(KeyHandle);
		if (Index != INDEX_NONE)
		{
			return TKeyFrameProxy<KeyValueType, TimeType>(this->GetKeyTimeChecked(Index), (*KeyValues)[Index]);
		}
		return TOptional<TKeyFrameProxy<KeyValueType, TimeType>>();
	}

	/**
	 * Update the key at the specified time with a new value, or add one if none is present
	 *
	 * @param InTime				The time at which to update/add the value
	 * @param Value					The new key value
	 * @param KeyTimeTolerance		Tolerance to use when looking for an existing key
	 * @return The key's handle
	 */
	FKeyHandle UpdateOrAddKey(TimeTypeRef InTime, KeyValueTypeRef Value, TimeTypeRef KeyTimeTolerance)
	{
		TOptional<FKeyHandle> Handle = this->FindKey(
			[=](TimeType ExistingTime)
			{
				return FMath::IsNearlyEqual(InTime, ExistingTime, KeyTimeTolerance);
			});
		
		if (!Handle.IsSet())
		{
			return AddKeyValue(InTime, Value);
		}
		else
		{
			TOptional<TKeyFrameProxy<KeyValueType, TimeType>> Key = GetKey(Handle.GetValue());

			if (ensure(Key.IsSet()))
			{
				Key->Value = Value;
			}

			return Handle.GetValue();
		}
	}

public:

	/**
	 * Iterate this curve's keys
	 */
	TKeyIterator<KeyValueType, TimeType> IterateKeysAndValues()
	{
		return TKeyIterator<KeyValueType, TimeType>(*this);
	}

	/**
	 * Iterate this curve's keys
	 */
	TKeyIterator<const KeyValueType, TimeType> IterateKeysAndValues() const
	{
		return TKeyIterator<const KeyValueType, TimeType>(*this);
	}

protected:

	virtual void OnKeyAdded(int32 Index) override
	{
		KeyValueType NewValue;
		KeyValues->Insert(MoveTemp(NewValue), Index);
	}

	virtual void OnKeyRelocated(int32 OldIndex, int32 NewIndex) override
	{
		KeyValueType Value = (*KeyValues)[OldIndex];

		KeyValues->RemoveAtSwap(OldIndex, 1, false);
		KeyValues->Insert(MoveTemp(Value), NewIndex);
	}

	virtual void OnKeyRemoved(int32 Index) override
	{
		KeyValues->RemoveAtSwap(Index, 1, false);
	}

	virtual void OnReset() override
	{
		KeyValues->Empty();
	}

private:

	friend TKeyIterator<KeyValueType, TimeType>;
	friend TKeyIterator<const KeyValueType, TimeType>;
	//using TKeyFrameManipulator<TimeType>::GetKeyHandleFromIndex;
	//using TKeyFrameManipulator<TimeType>::GetKeyTimeChecked;

	/** Array of associated key data */
	TArray<KeyValueType>* KeyValues;
};

namespace Impl
{
	template<typename TSrc, typename TDest>
	struct TMatchConst
	{
		typedef TDest Type;
	};

	template<typename TSrc, typename TDest>
	struct TMatchConst<const TSrc, TDest>
	{
		typedef const TDest Type;
	};
}

/**
 * Key time iterator for TKeyFrameManipulator
 */
template<typename KeyValueType, typename TimeType>
struct TKeyIterator
{
	typedef typename Impl::TMatchConst<KeyValueType, TCurveInterface<typename TRemoveConst<KeyValueType>::Type, TimeType>>::Type CurveInterfaceType;

	TKeyIterator(CurveInterfaceType& InCurveInterface)
		: CurveInterface(InCurveInterface)
		, Index(0)
	{}

	FORCEINLINE TKeyIterator& operator++()
	{
		++Index;
		return *this;
	}

	/** conversion to "bool" returning true if the iterator is valid. */
	FORCEINLINE explicit operator bool() const
	{ 
		return Index < CurveInterface.KeyValues->Num();
	}
	/** inverse of the "bool" operator */
	FORCEINLINE bool operator !() const 
	{
		return !(bool)*this;
	}

	FORCEINLINE friend bool operator==(const TKeyIterator& LHS, const TKeyIterator& RHS)
	{
		return &LHS.CurveInterface == &RHS.CurveInterface && LHS.Index == RHS.Index;
	}

	FORCEINLINE friend bool operator!=(const TKeyIterator& LHS, const TKeyIterator& RHS)
	{
		return !(LHS == RHS);
	}

	FORCEINLINE TKeyFrameProxy<KeyValueType, TimeType> operator*() const
	{
		return TKeyFrameProxy<KeyValueType, TimeType>(CurveInterface.GetKeyTimeChecked(Index), (*CurveInterface.KeyValues)[Index]);
	}

	FORCEINLINE TKeyFrameProxy<KeyValueType, TimeType> operator->() const
	{
		return TKeyFrameProxy<KeyValueType, TimeType>(CurveInterface.GetKeyTimeChecked(Index), (*CurveInterface.KeyValues)[Index]);
	}

	FKeyHandle GetKeyHandle()
	{
		return CurveInterface.GetKeyHandleFromIndex(Index);
	}

	int32 GetStartIndex() const
	{
		return 0;
	}

	int32 GetEndIndex() const
	{
		return CurveInterface.KeyValues->Num();
	}

	FORCEINLINE friend TKeyIterator<KeyValueType, TimeType> begin(const TKeyIterator<KeyValueType, TimeType>& Iter)
	{
		TKeyIterator<KeyValueType, TimeType> NewIter = Iter;
		NewIter.Index = Iter.GetStartIndex();
		return NewIter;
	}
	FORCEINLINE friend TKeyIterator<KeyValueType, TimeType> end(const TKeyIterator<KeyValueType, TimeType>& Iter)
	{
		TKeyIterator<KeyValueType, TimeType> NewIter = Iter;
		NewIter.Index = NewIter.GetEndIndex();
		return NewIter;
	}

private:
	CurveInterfaceType& CurveInterface;
	int32 Index;
};
