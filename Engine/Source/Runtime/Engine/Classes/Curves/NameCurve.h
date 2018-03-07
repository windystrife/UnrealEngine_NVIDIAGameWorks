// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"
#include "Curves/KeyHandle.h"
#include "Curves/IndexedCurve.h"
#include "NameCurve.generated.h"

/**
 * One key in a curve of FNames.
 */
USTRUCT()
struct ENGINE_API FNameCurveKey
{
	GENERATED_USTRUCT_BODY()

	/** Time at this key */
	UPROPERTY(EditAnywhere, Category="Key")
	float Time;

	/** Value at this key */
	UPROPERTY(EditAnywhere, Category="Key")
	FName Value;

	/** Default constructor. */
	FNameCurveKey()
		: Time(0.0f)
		, Value(NAME_None)
	{ }

	/** Creates and initializes a new instance. */
	FNameCurveKey(float InTime, const FName& InValue)
		: Time(InTime)
		, Value(InValue)
	{ }

public:

	// TStructOpsTypeTraits interface

	bool operator==(const FNameCurveKey& Other) const;
	bool operator!=(const FNameCurveKey& Other) const;
	bool Serialize(FArchive& Ar);

	/** Serializes a name curve key from or into an archive. */
	friend FArchive& operator<<(FArchive& Ar, FNameCurveKey& Key)
	{
		Key.Serialize(Ar);
		return Ar;
	}
};


template<>
struct TIsPODType<FNameCurveKey>
{
	enum { Value = true };
};


template<>
struct TStructOpsTypeTraits<FNameCurveKey>
	: public TStructOpsTypeTraitsBase2<FNameCurveKey>
{
	enum
	{
		WithSerializer = true,
		WithCopy = false,
		WithIdenticalViaEquality = true,
	};
};


/**
 * Implements a curve of FNames.
 */
USTRUCT()
struct ENGINE_API FNameCurve
	: public FIndexedCurve
{
	GENERATED_USTRUCT_BODY()

	/** Virtual destructor. */
	virtual ~FNameCurve() { }

public:

	/**
	  * Add a new key to the curve with the supplied Time and Value. Returns the handle of the new key.
	  * 
	  * @param InTime The time at which to add the key.
	  * @param InValue The value of the key.
	  * @param KeyHandle Optional handle for the new key.
	  */
	FKeyHandle AddKey(float InTime, const FName& InValue, FKeyHandle KeyHandle = FKeyHandle());

	/**
	 * Remove the specified key from the curve.
	 *
	 * @param KeyHandle Handle to the key to remove.
	 */
	void DeleteKey(FKeyHandle KeyHandle);

	/**
	 * Finds a key a the specified time.
	 *
	 * @param KeyTime The time at which to find the key.
	 * @param KeyTimeTolerance The key time tolerance to use for equality.
	 * @return A handle to the key, or invalid key handle if not found.
	 */
	FKeyHandle FindKey(float KeyTime, float KeyTimeTolerance = KINDA_SMALL_NUMBER) const;

	/**
	 * Get a key.
	 *
	 * @param KeyHandle The handle of the key to get.
	 * @return The key.
	 */
	FNameCurveKey& GetKey(FKeyHandle KeyHandle);
	FNameCurveKey GetKey(FKeyHandle KeyHandle) const;

	/**
	 * Read-only access to the key collection.
	 *
	 * @return Collection of keys.
	 */
	const TArray<FNameCurveKey>& GetKeys() const
	{
		return Keys;
	}

	/**
	 * Get the time for the Key with the specified index.
	 *
	 * @param KeyHandle Handle to the key whose time to get.
	 * @return The key's time.
	 */
	float GetKeyTime(FKeyHandle KeyHandle) const;

	/**
	 * Move a key to a new time.
	 *
	 * This may change the index of the key, so the new key index is returned.
	 *
	 * @param KeyHandle The handle of the key to change.
	 * @param NewTime The new time to set on the key.
	 */
	FKeyHandle SetKeyTime(FKeyHandle KeyHandle, float NewTime);

	/**
	 * Finds the key at InTime, and updates its value. If it can't find the key within the KeyTimeTolerance, it adds one at that time.
	 *
	 * @param InTime The time at which the key should be added or updated.
	 * @param InValue The value of the key.
	 * @param KeyTimeTolerance The tolerance used for key time equality.
	 */
	FKeyHandle UpdateOrAddKey(float InTime, const FName& InValue, float KeyTimeTolerance = KINDA_SMALL_NUMBER);

public:

	/** Shifts all keys forwards or backwards in time by an even amount, preserving order. */
	void ShiftCurve(float DeltaTime);
	void ShiftCurve(float DeltaTime, TSet<FKeyHandle>& KeyHandles);
	
	/** Scales all keys about an origin, preserving order. */
	void ScaleCurve(float ScaleOrigin, float ScaleFactor);
	void ScaleCurve(float ScaleOrigin, float ScaleFactor, TSet<FKeyHandle>& KeyHandles);

public:

	// FIndexedCurve interface

	virtual int32 GetNumKeys() const override;
	virtual bool IsKeyHandleValid(FKeyHandle KeyHandle) const override;

public:

	/** Sorted array of keys */
	UPROPERTY(EditAnywhere, EditFixedSize, Category="Curve")
	TArray<FNameCurveKey> Keys;
};
