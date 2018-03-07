// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "InterpData.generated.h"

/**
 * Interpolation data, containing keyframe tracks, event tracks etc.
 * This does not contain any  AActor  references or state, so can safely be stored in
 * packages, shared between multiple MatineeActors etc.
 */
UCLASS(hidecategories=Object, MinimalAPI)
class UInterpData : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Duration of interpolation sequence - in seconds. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = InterpData)
	float InterpLength;

	/** Position in Interp to move things to for path-building in editor. */
	UPROPERTY()
	float PathBuildTime;

	/** Actual interpolation data. Groups of InterpTracks. */
	UPROPERTY(export, BlueprintReadOnly, Category=InterpData)
	TArray<class UInterpGroup*> InterpGroups;

	/** Used for curve editor to remember curve-editing setup. Only loaded in editor. */
	UPROPERTY(export)
	class UInterpCurveEdSetup* CurveEdSetup;

#if WITH_EDITORONLY_DATA
	/** Used for filtering which tracks are currently visible. */
	UPROPERTY()
	TArray<class UInterpFilter*> InterpFilters;

	/** The currently selected filter. */
	UPROPERTY()
	class UInterpFilter* SelectedFilter;

	/** Array of default filters. */
	UPROPERTY(transient)
	TArray<class UInterpFilter*> DefaultFilters;

#endif // WITH_EDITORONLY_DATA
	/** Used in editor for defining sections to loop, stretch etc. */
	UPROPERTY()
	float EdSectionStart;

	/** Used in editor for defining sections to loop, stretch etc. */
	UPROPERTY()
	float EdSectionEnd;

	/** If true, then the matinee should be baked and pruned at cook time. */
	UPROPERTY(EditAnywhere, Category=InterpData)
	uint32 bShouldBakeAndPrune:1;

	/** Cached version of the director group, if any, for easy access while in game */
	UPROPERTY(transient)
	class UInterpGroupDirector* CachedDirectorGroup;

	/** Unique names of all events contained across all event tracks */
	UPROPERTY()
	TArray<FName> AllEventNames;


	//~ Begin UObject Interface
	virtual void PostLoad(void) override;
	virtual void PostInitProperties() override;
	//~ End UObject Interface

	/** Search through all InterpGroups in this InterpData to find a group whose GroupName matches the given name. Returns INDEX_NONE if group not found. */
	ENGINE_API int32 FindGroupByName( FName GroupName );

	/** Search through all InterpGroups in this InterpData to find a group whose GroupName matches the given name. Returns INDEX_NONE if not group found. */
	ENGINE_API int32 FindGroupByName( const FString& InGroupName );

	/** Search through all groups to find all tracks of the given class. */
	ENGINE_API void FindTracksByClass(UClass* TrackClass, TArray<class UInterpTrack*>& OutputTracks);
	
	/** Find a DirectorGroup in the data. Should only ever be 0 or 1 of these! */
	ENGINE_API class UInterpGroupDirector* FindDirectorGroup();

	/** Checks to see if the event name exists */
	ENGINE_API bool IsEventName(const FName& InEventName) const;

	/** Get the list of all unique event names */
	ENGINE_API void GetAllEventNames(TArray<FName>& OutEventNames) const;

	/** Update the AllEventNames array */
	ENGINE_API void UpdateEventNames();

#if WITH_EDITORONLY_DATA
	void CreateDefaultFilters();
#endif
};



