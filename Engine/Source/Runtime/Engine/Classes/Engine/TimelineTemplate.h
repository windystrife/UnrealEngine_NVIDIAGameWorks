// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Misc/Guid.h"
#include "Components/TimelineComponent.h"
#include "Engine/Blueprint.h"
#include "TimelineTemplate.generated.h"

USTRUCT()
struct FTTTrackBase
{
	GENERATED_USTRUCT_BODY()

	/** Name of this track */
	UPROPERTY()
	FName TrackName;

	/** Flag to identify internal/external curve*/
	UPROPERTY()
	bool bIsExternalCurve;

	/** Determine if Tracks are the same */
	ENGINE_API bool operator == (const FTTTrackBase& T2) const;

		FTTTrackBase()
		: TrackName(NAME_None), bIsExternalCurve(false)
		{}
	
};

/** Structure storing information about one event track */
USTRUCT()
struct FTTEventTrack : public FTTTrackBase
{
	GENERATED_USTRUCT_BODY()

	/** Curve object used to store keys */
	UPROPERTY()
	class UCurveFloat* CurveKeys;

	/** Determine if Tracks are the same */
	ENGINE_API bool operator == (const FTTEventTrack& T2) const;

		FTTEventTrack()
		: FTTTrackBase()
		, CurveKeys(NULL)
		{}
	
};

/** Structure storing information about one float interpolation track */
USTRUCT()
struct FTTFloatTrack : public FTTTrackBase
{
	GENERATED_USTRUCT_BODY()

	/** Curve object used to define float value over time */
	UPROPERTY()
	class UCurveFloat* CurveFloat;

	/** Determine if Tracks are the same */
	ENGINE_API bool operator == (const FTTFloatTrack& T2) const;

		FTTFloatTrack()
		: FTTTrackBase()
		, CurveFloat(NULL)
		{}
	
};

/** Structure storing information about one vector interpolation track */
USTRUCT()
struct FTTVectorTrack : public FTTTrackBase
{
	GENERATED_USTRUCT_BODY()

	/** Curve object used to define vector value over time */
	UPROPERTY()
	class UCurveVector* CurveVector;

	/** Determine if Tracks are the same */
	ENGINE_API bool operator == (const FTTVectorTrack& T2) const;

		FTTVectorTrack()
		: FTTTrackBase()
		, CurveVector(NULL)
		{}
	
};

/** Structure storing information about one color interpolation track */
USTRUCT()
struct FTTLinearColorTrack : public FTTTrackBase
{
	GENERATED_USTRUCT_BODY()

	/** Curve object used to define color value over time */
	UPROPERTY()
	class UCurveLinearColor* CurveLinearColor;

	/** Determine if Tracks are the same */
	ENGINE_API bool operator == (const FTTLinearColorTrack& T2) const;

	FTTLinearColorTrack()
	: FTTTrackBase()
	, CurveLinearColor(NULL)
	{}
	
};

UCLASS(MinimalAPI)
class UTimelineTemplate : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Length of this timeline */
	UPROPERTY(EditAnywhere, Category=TimelineTemplate)
	float TimelineLength;

	/** How we want the timeline to determine its own length (e.g. specified length, last keyframe) */
	UPROPERTY(EditAnywhere, Category=TimelineTemplate)
	TEnumAsByte<ETimelineLengthMode> LengthMode;

	/** If we want the timeline to auto-play */
	UPROPERTY(EditAnywhere, Category=TimelineTemplate)
	uint32 bAutoPlay:1;

	/** If we want the timeline to loop */
	UPROPERTY(EditAnywhere, Category=TimelineTemplate)
	uint32 bLoop:1;

	/** If we want the timeline to loop */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=TimelineTemplate)
	uint32 bReplicated:1;

	/** Compiler Validated As Wired up */
	UPROPERTY()
	uint32 bValidatedAsWired:1;

	/** If we want the timeline to ignore global time dilation */
	UPROPERTY(EditAnywhere, Category = TimelineTemplate)
	uint32 bIgnoreTimeDilation : 1;

	/** Set of event tracks */
	UPROPERTY()
	TArray<struct FTTEventTrack> EventTracks;

	/** Set of float interpolation tracks */
	UPROPERTY()
	TArray<struct FTTFloatTrack> FloatTracks;

	/** Set of vector interpolation tracks */
	UPROPERTY()
	TArray<struct FTTVectorTrack> VectorTracks;

	/** Set of linear color interpolation tracks */
	UPROPERTY()
	TArray<struct FTTLinearColorTrack> LinearColorTracks;

	/** Metadata information for this timeline */
	UPROPERTY(EditAnywhere, Category=BPVariableDescription)
	TArray<struct FBPVariableMetaDataEntry> MetaDataArray;

	UPROPERTY(duplicatetransient)
	FGuid	TimelineGuid;

	/** Find the index of a float track */
	int32 FindFloatTrackIndex(const FName& FloatTrackName);
	
	/** Find the index of a vector track */
	int32 FindVectorTrackIndex(const FName& VectorTrackName);
	
	/** Find the index of an event track */
	int32 FindEventTrackIndex(const FName& EventTrackName);

	/** Find the index of a linear color track */
	int32 FindLinearColorTrackIndex(const FName& ColorTrackName);

	/** @return true if a name is valid for a new track (it isn't already in use) */
	ENGINE_API bool IsNewTrackNameValid(const FName& NewTrackName);
	
	/** Get the name of the function we expect to find in the owning actor that we will bind the update event to */
	ENGINE_API FName GetUpdateFunctionName() const;
	
	/** Get the name of the function we expect to find in the owning actor that we will bind the finished event to */
	ENGINE_API FName GetFinishedFunctionName() const;

	/** Get the name of the funcerion we expect to find in the owning actor that we will bind event track with index EvenTrackIndex to */
	ENGINE_API FName GetEventTrackFunctionName(int32 EventTrackIndex) const;

	/** Set a metadata value on the timeline */
	ENGINE_API void SetMetaData(const FName& Key, const FString& Value);

	/** Gets a metadata value on the timeline; asserts if the value isn't present.  Check for validiy using FindMetaDataEntryIndexForKey. */
	ENGINE_API FString GetMetaData(const FName& Key);

	/** Clear metadata value on the timeline */
	ENGINE_API void RemoveMetaData(const FName& Key);

	/** Find the index in the array of a timeline entry */
	ENGINE_API int32 FindMetaDataEntryIndexForKey(const FName& Key);

	/** Returns the property name for the timeline's direction pin */
	ENGINE_API FName GetDirectionPropertyName() const;

	/** Returns the property name for a given timeline track */
	ENGINE_API FName GetTrackPropertyName(const FName TrackName) const;

	/* Create a new unique name for a curve */
	ENGINE_API static FString MakeUniqueCurveName(UObject* Obj, UObject* InOuter);

	ENGINE_API static FString TimelineTemplateNameToVariableName(FName Name);
	ENGINE_API static FString TimelineVariableNameToTemplateName(FName Name);

	ENGINE_API void GetAllCurves(TSet<class UCurveBase*>& InOutCurves) const;

	/* Perform deep copy of curves */
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
};



