// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovieSceneCommonHelpers.h"
#include "MovieSceneSection.h"
#include "SequenceRecorderSettings.h"


/** Interface for a generic property recorder */
class IMovieScenePropertyRecorder
{
public:
	virtual void Create(UObject* InObjectToRecord, class UMovieScene* InMovieScene, const FGuid& InGuid, float InTime) = 0;

	virtual void Record(UObject* InObjectToRecord, float InCurrentTime) = 0;

	virtual void Finalize(UObject* InObjectToRecord) = 0;
};

/** Helper struct for recording properties */
template <typename PropertyType>
struct FPropertyKey
{
	PropertyType Value;
	float Time;
};

/** Recorder for a simple property of type PropertyType */
template <typename PropertyType>
class FMovieScenePropertyRecorder : public IMovieScenePropertyRecorder
{
public:
	FMovieScenePropertyRecorder(const FTrackInstancePropertyBindings& InBinding)
		: Binding(InBinding)
	{
	}
	virtual ~FMovieScenePropertyRecorder() { }

	virtual void Create(UObject* InObjectToRecord, class UMovieScene* InMovieScene, const FGuid& InGuid, float InTime) override
	{
		if (InObjectToRecord)
		{
			PreviousValue = Binding.GetCurrentValue<PropertyType>(*InObjectToRecord);
		}

		MovieSceneSection = AddSection(InObjectToRecord, InMovieScene, InGuid, InTime);
	}

	virtual void Record(UObject* InObjectToRecord, float InCurrentTime) override
	{
		if (InObjectToRecord != nullptr)
		{
			MovieSceneSection->SetEndTime(InCurrentTime);

			PropertyType NewValue = Binding.GetCurrentValue<PropertyType>(*InObjectToRecord);
			if (ShouldAddNewKey(NewValue))
			{
				FPropertyKey<PropertyType> Key;
				Key.Time = InCurrentTime;
				Key.Value = NewValue;

				Keys.Add(Key);

				PreviousValue = NewValue;
			}
		}
	}

	virtual void Finalize(UObject* InObjectToRecord) override
	{
		for (const FPropertyKey<PropertyType>& Key : Keys)
		{
			AddKeyToSection(MovieSceneSection, Key);
		}

		const USequenceRecorderSettings* Settings = GetDefault<USequenceRecorderSettings>();
		if (Settings->bReduceKeys)
		{
			ReduceKeys(MovieSceneSection);
		}
	}

private:
	/** 
	 * Helper function, specialized by type, used to check if we do capture-time filtering of keys based
	 * on previous values
	 */
	bool ShouldAddNewKey(const PropertyType& InNewValue) const;

	/** Helper function, specialized by type, used to add an appropriate section to the movie scene */
	class UMovieSceneSection* AddSection(UObject* InObjectToRecord, class UMovieScene* InMovieScene, const FGuid& InGuid, float InTime);

	/** Helper function, specialized by type, used to add keys to the movie scene section at Finalize() time */
	void AddKeyToSection(UMovieSceneSection* InSection, const FPropertyKey<PropertyType>& InKey);

	/** Helper function, specialized by type, used to reduce keys */
	void ReduceKeys(UMovieSceneSection* InSection);

private:
	/** Binding for this property */
	FTrackInstancePropertyBindings Binding;

	/** The keys that are being recorded */
	TArray<FPropertyKey<PropertyType>> Keys;

	/** Section we are recording */
	UMovieSceneSection* MovieSceneSection;

	/** Previous value we use to establish whether we should key */
	PropertyType PreviousValue;
};

/** Recorder for a simple property of type enum */
class FMovieScenePropertyRecorderEnum : public IMovieScenePropertyRecorder
{
public:
	FMovieScenePropertyRecorderEnum(const FTrackInstancePropertyBindings& InBinding)
		: Binding(InBinding)
	{
	}
	virtual ~FMovieScenePropertyRecorderEnum() { }

	virtual void Create(UObject* InObjectToRecord, class UMovieScene* InMovieScene, const FGuid& InGuid, float InTime) override
	{
		if (InObjectToRecord)
		{
			PreviousValue = Binding.GetCurrentValueForEnum(*InObjectToRecord);
		}

		MovieSceneSection = AddSection(InObjectToRecord, InMovieScene, InGuid, InTime);
	}

	virtual void Record(UObject* InObjectToRecord, float InCurrentTime) override
	{
		if (InObjectToRecord != nullptr)
		{
			MovieSceneSection->SetEndTime(InCurrentTime);

			int64 NewValue = Binding.GetCurrentValueForEnum(*InObjectToRecord);
			if (ShouldAddNewKey(NewValue))
			{
				FPropertyKey<int64> Key;
				Key.Time = InCurrentTime;
				Key.Value = NewValue;

				Keys.Add(Key);

				PreviousValue = NewValue;
			}
		}
	}

	virtual void Finalize(UObject* InObjectToRecord) override
	{
		for (const FPropertyKey<int64>& Key : Keys)
		{
			AddKeyToSection(MovieSceneSection, Key);
		}

		ReduceKeys(MovieSceneSection);
	}

private:
	/** 
	 * Helper function, specialized by type, used to check if we do capture-time filtering of keys based
	 * on previous values
	 */
	bool ShouldAddNewKey(const int64& InNewValue) const;

	/** Helper function, specialized by type, used to add an appropriate section to the movie scene */
	class UMovieSceneSection* AddSection(UObject* InObjectToRecord, class UMovieScene* InMovieScene, const FGuid& InGuid, float InTime);

	/** Helper function, specialized by type, used to add keys to the movie scene section at Finalize() time */
	void AddKeyToSection(UMovieSceneSection* InSection, const FPropertyKey<int64>& InKey);

	/** Helper function, specialized by type, used to reduce keys */
	void ReduceKeys(UMovieSceneSection* InSection);

private:
	/** Binding for this property */
	FTrackInstancePropertyBindings Binding;

	/** The keys that are being recorded */
	TArray<FPropertyKey<int64>> Keys;

	/** Section we are recording */
	UMovieSceneSection* MovieSceneSection;

	/** Previous value we use to establish whether we should key */
	int64 PreviousValue;
};
