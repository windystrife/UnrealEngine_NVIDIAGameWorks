// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"
#include "Curves/KeyHandle.h"
#include "MovieSceneSection.h"
#include "Curves/NameCurve.h"
#include "Curves/CurveInterface.h"
#include "UObject/StructOnScope.h"
#include "Engine/Engine.h"
#include "UObject/SoftObjectPath.h"
#include "MovieSceneEventSection.generated.h"

struct EventData;

USTRUCT()
struct MOVIESCENETRACKS_API FMovieSceneEventParameters
{
	GENERATED_BODY()

	FMovieSceneEventParameters() {}

	/** Construction from a struct type */
	FMovieSceneEventParameters(UStruct& InStruct)
		: StructType(&InStruct)
	{
	}

	FMovieSceneEventParameters(const FMovieSceneEventParameters& RHS) = default;
	FMovieSceneEventParameters& operator=(const FMovieSceneEventParameters& RHS) = default;

	FMovieSceneEventParameters(FMovieSceneEventParameters&&) = default;
	FMovieSceneEventParameters& operator=(FMovieSceneEventParameters&&) = default;

	/**
	 * Access the struct type of this event parameter payload
	 * @return A valid UStruct* or nullptr if the struct is not set, or no longer available
	 */
	UStruct* GetStructType() const
	{
		return Cast<UStruct>(StructType.TryLoad());
	}

	/**
	 * Change the type of this event parameter payload to be the specified struct
	 */
	void Reassign(UStruct* NewStruct)
	{
		StructType = NewStruct;

		if (!NewStruct)
		{
			StructBytes.Reset();
		}
	}

	/**
	 * Retrieve an instance of this payload
	 *
	 * @param OutStruct Structure to receive the instance
	 */
	void GetInstance(FStructOnScope& OutStruct) const;

	/**
	 * Overwrite this payload with another instance of the same type.
	 *
	 * @param InstancePtr A valid pointer to an instance of the type represented by GetStructType
	 */
	void OverwriteWith(uint8* InstancePtr);

	/**
	 * Serialization implementation
	 */
	bool Serialize(FArchive& Ar);

	friend FArchive& operator<<(FArchive& Ar, FMovieSceneEventParameters& Payload)
	{
		Payload.Serialize(Ar);
		return Ar;
	}

private:

	/** Soft object path to the type of this parameter payload */
	FSoftObjectPath StructType;

	/** Serialized bytes that represent the payload. Serialized internally with FEventParameterArchive */
	TArray<uint8> StructBytes;
};

template<>
struct TStructOpsTypeTraits<FMovieSceneEventParameters> : public TStructOpsTypeTraitsBase2<FMovieSceneEventParameters>
{
	enum 
	{
		WithCopy = true,
		WithSerializer = true
	};
};

USTRUCT()
struct FEventPayload
{
	GENERATED_BODY()

	FEventPayload() {}
	FEventPayload(FName InEventName) : EventName(InEventName) {}

	FEventPayload(const FEventPayload&) = default;
	FEventPayload& operator=(const FEventPayload&) = default;

#if PLATFORM_COMPILER_HAS_DEFAULTED_FUNCTIONS
	FEventPayload(FEventPayload&&) = default;
	FEventPayload& operator=(FEventPayload&&) = default;
#else
	FEventPayload(FEventPayload&& RHS)
		: EventName(MoveTemp(RHS.EventName))
		, Parameters(MoveTemp(RHS.Parameters))
	{
	}
	FEventPayload& operator=(FEventPayload&& RHS)
	{
		EventName = MoveTemp(RHS.EventName);
		Parameters = MoveTemp(RHS.Parameters);
		return *this;
	}
#endif

	/** The name of the event to trigger */
	UPROPERTY(EditAnywhere, Category=Event)
	FName EventName;

	/** The event parameters */
	UPROPERTY(EditAnywhere, Category=Event, meta=(ShowOnlyInnerProperties))
	FMovieSceneEventParameters Parameters;
};

/** A curve of events */
USTRUCT()
struct FMovieSceneEventSectionData
{
	GENERATED_BODY()
	
	FMovieSceneEventSectionData() = default;

	FMovieSceneEventSectionData(const FMovieSceneEventSectionData& RHS)
		: KeyTimes(RHS.KeyTimes)
		, KeyValues(RHS.KeyValues)
	{
	}

	FMovieSceneEventSectionData& operator=(const FMovieSceneEventSectionData& RHS)
	{
		KeyTimes = RHS.KeyTimes;
		KeyValues = RHS.KeyValues;
#if WITH_EDITORONLY_DATA
		KeyHandles.Reset();
#endif
		return *this;
	}

	/** Sorted array of key times */
	UPROPERTY()
	TArray<float> KeyTimes;

	/** Array of values that correspond to each key time */
	UPROPERTY()
	TArray<FEventPayload> KeyValues;

#if WITH_EDITORONLY_DATA
	/** Transient key handles */
	FKeyHandleLookupTable KeyHandles;
#endif
};

/**
 * Implements a section in movie scene event tracks.
 */
UCLASS(MinimalAPI)
class UMovieSceneEventSection
	: public UMovieSceneSection
{
	GENERATED_BODY()

	/** Default constructor. */
	UMovieSceneEventSection();

public:
	
	// ~UObject interface
	virtual void PostLoad() override;

	/**
	 * Get the section's event data.
	 *
	 * @return Event data.
	 */
	const FMovieSceneEventSectionData& GetEventData() const { return EventData; }
	
	TCurveInterface<FEventPayload, float> GetCurveInterface() { return CurveInterface.GetValue(); }

public:

	//~ UMovieSceneSection interface

	virtual void DilateSection(float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles) override;
	virtual void GetKeyHandles(TSet<FKeyHandle>& KeyHandles, TRange<float> TimeRange) const override;
	virtual void MoveSection(float DeltaPosition, TSet<FKeyHandle>& KeyHandles) override;
	virtual TOptional<float> GetKeyTime(FKeyHandle KeyHandle) const override;
	virtual void SetKeyTime(FKeyHandle KeyHandle, float Time) override;

private:

	UPROPERTY()
	FNameCurve Events_DEPRECATED;

	UPROPERTY()
	FMovieSceneEventSectionData EventData;

	TOptional<TCurveInterface<FEventPayload, float>> CurveInterface;
};
