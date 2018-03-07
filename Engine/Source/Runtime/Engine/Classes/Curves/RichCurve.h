// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"
#include "Curves/KeyHandle.h"
#include "Curves/IndexedCurve.h"
#include "RichCurve.generated.h"

/** Method of interpolation between this key and the next. */
UENUM()
enum ERichCurveInterpMode
{
	RCIM_Linear UMETA(DisplayName="Linear"),
	RCIM_Constant UMETA(DisplayName="Constant"),
	RCIM_Cubic UMETA(DisplayName="Cubic"),
	RCIM_None UMETA(DisplayName="None")
};


/** If using RCIM_Cubic, this enum describes how the tangents should be controlled in editor. */
UENUM()
enum ERichCurveTangentMode
{
	RCTM_Auto UMETA(DisplayName="Auto"),
	RCTM_User UMETA(DisplayName="User"),
	RCTM_Break UMETA(DisplayName="Break"),
	RCTM_None UMETA(DisplayName="None")
};


/** Enumerates tangent weight modes. */
UENUM()
enum ERichCurveTangentWeightMode
{
	RCTWM_WeightedNone UMETA(DisplayName="None"),
	RCTWM_WeightedArrive UMETA(DisplayName="Arrive"),
	RCTWM_WeightedLeave UMETA(DisplayName="Leave"),
	RCTWM_WeightedBoth UMETA(DisplayName="Both")
};


/** Enumerates extrapolation options. */
UENUM()
enum ERichCurveExtrapolation
{
	RCCE_Cycle UMETA(DisplayName="Cycle"),
	RCCE_CycleWithOffset UMETA(DisplayName="CycleWithOffset"),
	RCCE_Oscillate UMETA(DisplayName="Oscillate"),
	RCCE_Linear UMETA(DisplayName="Linear"),
	RCCE_Constant UMETA(DisplayName="Constant"),
	RCCE_None UMETA(DisplayName="None")
};


/** One key in a rich, editable float curve */
USTRUCT()
struct ENGINE_API FRichCurveKey
{
	GENERATED_USTRUCT_BODY()

	/** Interpolation mode between this key and the next */
	UPROPERTY()
	TEnumAsByte<ERichCurveInterpMode> InterpMode;

	/** Mode for tangents at this key */
	UPROPERTY()
	TEnumAsByte<ERichCurveTangentMode> TangentMode;

	/** If either tangent at this key is 'weighted' */
	UPROPERTY()
	TEnumAsByte<ERichCurveTangentWeightMode> TangentWeightMode;

	/** Time at this key */
	UPROPERTY(EditAnywhere, Category="Key")
	float Time;

	/** Value at this key */
	UPROPERTY(EditAnywhere, Category="Key")
	float Value;

	/** If RCIM_Cubic, the arriving tangent at this key */
	UPROPERTY()
	float ArriveTangent;

	/** If RCTWM_WeightedArrive or RCTWM_WeightedBoth, the weight of the left tangent */
	UPROPERTY()
	float ArriveTangentWeight;

	/** If RCIM_Cubic, the leaving tangent at this key */
	UPROPERTY()
	float LeaveTangent;

	/** If RCTWM_WeightedLeave or RCTWM_WeightedBoth, the weight of the right tangent */
	UPROPERTY()
	float LeaveTangentWeight;

	FRichCurveKey()
		: InterpMode(RCIM_Linear)
		, TangentMode(RCTM_Auto)
		, TangentWeightMode(RCTWM_WeightedNone)
		, Time(0.f)
		, Value(0.f)
		, ArriveTangent(0.f)
		, ArriveTangentWeight(0.f)
		, LeaveTangent(0.f)
		, LeaveTangentWeight(0.f)
	{ }

	FRichCurveKey(float InTime, float InValue)
		: InterpMode(RCIM_Linear)
		, TangentMode(RCTM_Auto)
		, TangentWeightMode(RCTWM_WeightedNone)
		, Time(InTime)
		, Value(InValue)
		, ArriveTangent(0.f)
		, ArriveTangentWeight(0.f)
		, LeaveTangent(0.f)
		, LeaveTangentWeight(0.f)
	{ }

	FRichCurveKey(float InTime, float InValue, float InArriveTangent, const float InLeaveTangent, ERichCurveInterpMode InInterpMode)
		: InterpMode(InInterpMode)
		, TangentMode(RCTM_Auto)
		, TangentWeightMode(RCTWM_WeightedNone)
		, Time(InTime)
		, Value(InValue)
		, ArriveTangent(InArriveTangent)
		, ArriveTangentWeight(0.f)
		, LeaveTangent(InLeaveTangent)
		, LeaveTangentWeight(0.f)
	{ }

	/** Conversion constructor */
	FRichCurveKey(const FInterpCurvePoint<float>& InPoint);
	FRichCurveKey(const FInterpCurvePoint<FVector>& InPoint, int32 ComponentIndex);

	/** ICPPStructOps interface */
	bool Serialize(FArchive& Ar);
	bool operator==(const FRichCurveKey& Other) const;
	bool operator!=(const FRichCurveKey& Other) const;

	friend FArchive& operator<<(FArchive& Ar, FRichCurveKey& P)
	{
		P.Serialize(Ar);
		return Ar;
	}
};


template<>
struct TIsPODType<FRichCurveKey>
{
	enum { Value = true };
};


template<>
struct TStructOpsTypeTraits<FRichCurveKey>
	: public TStructOpsTypeTraitsBase2<FRichCurveKey>
{
	enum
	{
		WithSerializer = true,
		WithCopy = false,
		WithIdenticalViaEquality = true,
	};
};


/** A rich, editable float curve */
USTRUCT()
struct ENGINE_API FRichCurve
	: public FIndexedCurve
{
	GENERATED_USTRUCT_BODY()

	FRichCurve() 
		: FIndexedCurve()
		, PreInfinityExtrap(RCCE_Constant)
		, PostInfinityExtrap(RCCE_Constant)
		, DefaultValue(MAX_flt)
	{ }

	/** Virtual destructor. */
	virtual ~FRichCurve() { }

public:

	/**
	 * Check whether this curve has any data or not
	 */
	bool HasAnyData() const
	{
		return DefaultValue != MAX_flt || Keys.Num();
	}

	/** Gets a copy of the keys, so indices and handles can't be meddled with */
	TArray<FRichCurveKey> GetCopyOfKeys() const;

	/** Gets a const reference of the keys, so indices and handles can't be meddled with */
	const TArray<FRichCurveKey>& GetConstRefOfKeys() const;

	/** Const iterator for the keys, so the indices and handles stay valid */
	TArray<FRichCurveKey>::TConstIterator GetKeyIterator() const;
	
	/** Functions for getting keys based on handles */
	FRichCurveKey& GetKey(FKeyHandle KeyHandle);
	FRichCurveKey GetKey(FKeyHandle KeyHandle) const;
	
	/** Quick accessors for the first and last keys */
	FRichCurveKey GetFirstKey() const;
	FRichCurveKey GetLastKey() const;

	/** Get the first key that matches any of the given key handles. */
	FRichCurveKey* GetFirstMatchingKey(const TArray<FKeyHandle>& KeyHandles);

	/** Get the next or previous key given the key handle */
	FKeyHandle GetNextKey(FKeyHandle KeyHandle) const;
	FKeyHandle GetPreviousKey(FKeyHandle KeyHandle) const;

	/**
	  * Add a new key to the curve with the supplied Time and Value. Returns the handle of the new key.
	  * 
	  * @param	bUnwindRotation		When true, the value will be treated like a rotation value in degrees, and will automatically be unwound to prevent flipping 360 degrees from the previous key 
	  * @param  KeyHandle			Optionally can specify what handle this new key should have, otherwise, it'll make a new one
	  */
	FKeyHandle AddKey(float InTime, float InValue, const bool bUnwindRotation = false, FKeyHandle KeyHandle = FKeyHandle());

	/**
	 * Sets the keys with the keys.
	 *
	 * Expects that the keys are already sorted.
	 *
	 * @see AddKey, DeleteKey
	 */
	void SetKeys(const TArray<FRichCurveKey>& InKeys);

	/**
	 *  Remove the specified key from the curve.
	 *
	 * @param KeyHandle The handle of the key to remove.
	 * @see AddKey, SetKeys
	 */
	void DeleteKey(FKeyHandle KeyHandle);

	/** Finds the key at InTime, and updates its value. If it can't find the key within the KeyTimeTolerance, it adds one at that time */
	FKeyHandle UpdateOrAddKey(float InTime, float InValue, const bool bUnwindRotation = false, float KeyTimeTolerance = KINDA_SMALL_NUMBER);

	/** Move a key to a new time. This may change the index of the key, so the new key index is returned. */
	FKeyHandle SetKeyTime(FKeyHandle KeyHandle, float NewTime);

	/** Get the time for the Key with the specified index. */
	float GetKeyTime(FKeyHandle KeyHandle) const;

	/** Finds a key a the specified time */
	FKeyHandle FindKey(float KeyTime, float KeyTimeTolerance = KINDA_SMALL_NUMBER) const;

	/** Set the value of the specified key */
	void SetKeyValue(FKeyHandle KeyHandle, float NewValue, bool bAutoSetTangents=true);

	/** Returns the value of the specified key */
	float GetKeyValue(FKeyHandle KeyHandle) const;

	/** Set the default value of the curve */
	void SetDefaultValue(float InDefaultValue) { DefaultValue = InDefaultValue; }

	/** Get the default value for the curve */
	float GetDefaultValue() const { return DefaultValue; }

	/** Removes the default value for this curve. */
	void ClearDefaultValue() { DefaultValue = MAX_flt; }

	/** Shifts all keys forwards or backwards in time by an even amount, preserving order */
	void ShiftCurve(float DeltaTime);
	void ShiftCurve(float DeltaTime, TSet<FKeyHandle>& KeyHandles);
	
	/** Scales all keys about an origin, preserving order */
	void ScaleCurve(float ScaleOrigin, float ScaleFactor);
	void ScaleCurve(float ScaleOrigin, float ScaleFactor, TSet<FKeyHandle>& KeyHandles);

	/** Set the interp mode of the specified key */
	void SetKeyInterpMode(FKeyHandle KeyHandle, ERichCurveInterpMode NewInterpMode);

	/** Set the tangent mode of the specified key */
	void SetKeyTangentMode(FKeyHandle KeyHandle, ERichCurveTangentMode NewTangentMode);

	/** Set the tangent weight mode of the specified key */
	void SetKeyTangentWeightMode(FKeyHandle KeyHandle, ERichCurveTangentWeightMode NewTangentWeightMode);

	/** Get the interp mode of the specified key */
	ERichCurveInterpMode GetKeyInterpMode(FKeyHandle KeyHandle) const;

	/** Get the tangent mode of the specified key */
	ERichCurveTangentMode GetKeyTangentMode(FKeyHandle KeyHandle) const;

	/** Get range of input time values. Outside this region curve continues constantly the start/end values. */
	void GetTimeRange(float& MinTime, float& MaxTime) const;

	/** Get range of output values. */
	void GetValueRange(float& MinValue, float& MaxValue) const;

	/** Clear all keys. */
	void Reset();

	/** Remap InTime based on pre and post infinity extrapolation values */
	void RemapTimeValue(float& InTime, float& CycleValueOffset) const;

	/** Evaluate this rich curve at the specified time */
	float Eval(float InTime, float InDefaultValue = 0.0f) const;

	/** Auto set tangents for any 'auto' keys in curve */
	void AutoSetTangents(float Tension = 0.f);

	/** Resize curve length to the [MinTimeRange, MaxTimeRange] */
	void ReadjustTimeRange(float NewMinTimeRange, float NewMaxTimeRange, bool bInsert/* whether insert or remove*/, float OldStartTime, float OldEndTime);

	/** Determine if two RichCurves are the same */
	bool operator == (const FRichCurve& Curve) const;

	/** Bake curve given the sample rate */
	void BakeCurve(float SampleRate);
	void BakeCurve(float SampleRate, float FirstKeyTime, float LastKeyTime);

	/** Remove redundant keys, comparing against Tolerance */
	void RemoveRedundantKeys(float Tolerance);
	void RemoveRedundantKeys(float Tolerance, float FirstKeyTime, float LastKeyTime);

public:

	// FIndexedCurve interface

	virtual int32 GetNumKeys() const override;
	virtual bool IsKeyHandleValid(FKeyHandle KeyHandle) const override;

public:

	/** Pre-infinity extrapolation state */
	UPROPERTY()
	TEnumAsByte<ERichCurveExtrapolation> PreInfinityExtrap;

	/** Post-infinity extrapolation state */
	UPROPERTY()
	TEnumAsByte<ERichCurveExtrapolation> PostInfinityExtrap;

	/** Default value */
	UPROPERTY(EditAnywhere, Category="Curve")
	float DefaultValue;

	/** Sorted array of keys */
	UPROPERTY(EditAnywhere, EditFixedSize, Category="Curve", meta=(EditFixedOrder))
	TArray<FRichCurveKey> Keys;
};


/**
 * Info about a curve to be edited.
 */
template<class T>
struct FRichCurveEditInfoTemplate
{
	/** Name of curve, used when displaying in editor. Can include comma's to allow tree expansion in editor */
	FName CurveName;

	/** Pointer to curves to be edited */
	T CurveToEdit;

	FRichCurveEditInfoTemplate()
		: CurveName(NAME_None)
		, CurveToEdit(nullptr)
	{ }

	FRichCurveEditInfoTemplate(T InCurveToEdit)
		: CurveName(NAME_None)
		, CurveToEdit(InCurveToEdit)
	{ }

	FRichCurveEditInfoTemplate(T InCurveToEdit, FName InCurveName)
		: CurveName(InCurveName)
		, CurveToEdit(InCurveToEdit)
	{ }

	inline bool operator==(const FRichCurveEditInfoTemplate<T>& Other) const
	{
		return Other.CurveName.IsEqual(CurveName) && Other.CurveToEdit == CurveToEdit;
	}

	uint32 GetTypeHash() const
	{
		return HashCombine(::GetTypeHash(CurveName), PointerHash(CurveToEdit));
	}
};


template<class T>
inline uint32 GetTypeHash(const FRichCurveEditInfoTemplate<T>& RichCurveEditInfo)
{
	return RichCurveEditInfo.GetTypeHash();
}


typedef FRichCurveEditInfoTemplate<FRichCurve*>			FRichCurveEditInfo;
typedef FRichCurveEditInfoTemplate<const FRichCurve*>	FRichCurveEditInfoConst;
