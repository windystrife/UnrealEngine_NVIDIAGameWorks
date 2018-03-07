// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneMultiPropertyRecorder.h"
#include "UObject/UnrealType.h"
#include "MovieSceneCommonHelpers.h"
#include "Sections/MovieScenePropertyRecorder.h"
#include "SequenceRecorderSettings.h"

TSharedPtr<IMovieSceneSectionRecorder> FMovieSceneMultiPropertyRecorderFactory::CreateSectionRecorder(const FActorRecordingSettings& InActorRecordingSettings) const
{
	return MakeShareable(new FMovieSceneMultiPropertyRecorder());
}

bool FMovieSceneMultiPropertyRecorderFactory::CanRecordObject(UObject* InObjectToRecord) const
{
	const USequenceRecorderSettings* Settings = GetDefault<USequenceRecorderSettings>();

	for (const FPropertiesToRecordForClass& PropertiesToRecordForClass : Settings->ClassesAndPropertiesToRecord)
	{
		if (*PropertiesToRecordForClass.Class == InObjectToRecord->GetClass() && PropertiesToRecordForClass.Properties.Num() > 0)
		{
			return true;
		}
	}

	return false;
}

FMovieSceneMultiPropertyRecorder::FMovieSceneMultiPropertyRecorder()
{
}

void FMovieSceneMultiPropertyRecorder::CreateSection(UObject* InObjectToRecord, class UMovieScene* InMovieScene, const FGuid& Guid, float Time)
{
	ObjectToRecord = InObjectToRecord;

	// collect all properties to record from classes we are recording
	TArray<FName> PropertiesToRecord;

	const FPropertiesToRecordForClass* PropertiesToRecordForClassPtr = nullptr;
	const USequenceRecorderSettings* Settings = GetDefault<USequenceRecorderSettings>();
	for (const FPropertiesToRecordForClass& PropertiesToRecordForClass : Settings->ClassesAndPropertiesToRecord)
	{
		if (*PropertiesToRecordForClass.Class == InObjectToRecord->GetClass() && PropertiesToRecordForClass.Properties.Num() > 0)
		{
			PropertiesToRecord.Append(PropertiesToRecordForClass.Properties);
		}
	}

	// create a recorder for each property name
	for (const FName& PropertyName : PropertiesToRecord)
	{
		FTrackInstancePropertyBindings Binding(PropertyName, PropertyName.ToString());
		UProperty* Property = Binding.GetProperty(*InObjectToRecord);
		if (Property != nullptr)
		{
			if (Property->IsA<UBoolProperty>())
			{
				PropertyRecorders.Add(MakeShareable(new FMovieScenePropertyRecorder<bool>(Binding)));
			}
			else if (Property->IsA<UByteProperty>())
			{
				PropertyRecorders.Add(MakeShareable(new FMovieScenePropertyRecorder<uint8>(Binding)));
			}
			else if (Property->IsA<UEnumProperty>())
			{
				PropertyRecorders.Add(MakeShareable(new FMovieScenePropertyRecorderEnum(Binding)));
			}
			else if (Property->IsA<UFloatProperty>())
			{
				PropertyRecorders.Add(MakeShareable(new FMovieScenePropertyRecorder<float>(Binding)));
			}
			else if (UStructProperty* StructProperty = Cast<UStructProperty>(Property))
			{
				if (StructProperty->Struct->GetFName() == NAME_Vector)
				{
					PropertyRecorders.Add(MakeShareable(new FMovieScenePropertyRecorder<FVector>(Binding)));
				}
				else if (StructProperty->Struct->GetFName() == NAME_Color)
				{
					PropertyRecorders.Add(MakeShareable(new FMovieScenePropertyRecorder<FColor>(Binding)));
				}
			}

			PropertyRecorders.Last()->Create(InObjectToRecord, InMovieScene, Guid, Time);
		}
	}
}

void FMovieSceneMultiPropertyRecorder::FinalizeSection()
{
	for (TSharedPtr<IMovieScenePropertyRecorder> PropertyRecorder : PropertyRecorders)
	{
		PropertyRecorder->Finalize(ObjectToRecord.Get());
	}
}

void FMovieSceneMultiPropertyRecorder::Record(float CurrentTime)
{
	for (TSharedPtr<IMovieScenePropertyRecorder> PropertyRecorder : PropertyRecorders)
	{
		PropertyRecorder->Record(ObjectToRecord.Get(), CurrentTime);
	}
}

bool FMovieSceneMultiPropertyRecorder::CanPropertyBeRecorded(const UProperty& InProperty)
{
	if (InProperty.IsA<UBoolProperty>())
	{
		return true;
	}
	else if (InProperty.IsA<UByteProperty>())
	{
		return true;
	}
	else if (InProperty.IsA<UEnumProperty>())
	{
		return true;
	}
	else if (InProperty.IsA<UFloatProperty>())
	{
		return true;
	}
	else if (const UStructProperty* StructProperty = Cast<const UStructProperty>(&InProperty))
	{
		if (StructProperty->Struct->GetFName() == NAME_Vector)
		{
			return true;
		}
		else if (StructProperty->Struct->GetFName() == NAME_Color)
		{
			return true;
		}
	}

	return false;
}
