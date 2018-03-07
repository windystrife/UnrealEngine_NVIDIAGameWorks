// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/LazyObjectPtr.h"
#include "IMovieSceneSectionRecorder.h"
#include "IMovieSceneSectionRecorderFactory.h"

class UMovieSceneBoolSection;

class FMovieSceneSpawnSectionRecorderFactory : public IMovieSceneSectionRecorderFactory
{
public:
	virtual ~FMovieSceneSpawnSectionRecorderFactory() {}

	virtual TSharedPtr<IMovieSceneSectionRecorder> CreateSectionRecorder(const struct FActorRecordingSettings& InActorRecordingSettings) const override;
	virtual bool CanRecordObject(class UObject* InObjectToRecord) const override;
};

class FMovieSceneSpawnSectionRecorder : public IMovieSceneSectionRecorder
{
public:
	virtual ~FMovieSceneSpawnSectionRecorder() {}

	virtual void CreateSection(UObject* InObjectToRecord, class UMovieScene* MovieScene, const FGuid& Guid, float Time) override;
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

private:
	/** Object to record from */
	TLazyObjectPtr<UObject> ObjectToRecord;

	/** Section to record to */
	TWeakObjectPtr<class UMovieSceneBoolSection> MovieSceneSection;

	bool bWasSpawned;
};
