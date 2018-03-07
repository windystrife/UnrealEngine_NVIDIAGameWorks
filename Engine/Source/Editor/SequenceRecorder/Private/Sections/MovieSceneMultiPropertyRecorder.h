// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/LazyObjectPtr.h"
#include "IMovieSceneSectionRecorder.h"
#include "IMovieSceneSectionRecorderFactory.h"

class IMovieScenePropertyRecorder;

class FMovieSceneMultiPropertyRecorderFactory : public IMovieSceneSectionRecorderFactory
{
public:
	virtual ~FMovieSceneMultiPropertyRecorderFactory() {}

	virtual TSharedPtr<IMovieSceneSectionRecorder> CreateSectionRecorder(const struct FActorRecordingSettings& InActorRecordingSettings) const override;
	virtual bool CanRecordObject(UObject* InObjectToRecord) const override;
};

class FMovieSceneMultiPropertyRecorder : public IMovieSceneSectionRecorder
{
public:
	FMovieSceneMultiPropertyRecorder();
	virtual ~FMovieSceneMultiPropertyRecorder() { }

	/** IMovieSceneSectionRecorder interface */
	virtual void CreateSection(UObject* InObjectToRecord, class UMovieScene* InMovieScene, const FGuid& Guid, float Time) override;
	virtual void FinalizeSection() override;
	virtual void Record(float CurrentTime) override;
	virtual void InvalidateObjectToRecord() override
	{
		ObjectToRecord = nullptr;
	}
	virtual UObject* GetSourceObject() const override
	{
		return ObjectToRecord.Get();
	}

	/** Check if the property can be recorded */
	static bool CanPropertyBeRecorded(const UProperty& InProperty);

private:
	/** Object to record from */
	TLazyObjectPtr<class UObject> ObjectToRecord;

	/** All of our property recorders */
	TArray<TSharedPtr<class IMovieScenePropertyRecorder>> PropertyRecorders;
};
