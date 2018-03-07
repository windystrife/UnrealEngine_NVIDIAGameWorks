// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectAnnotation.h"
#include "Commandlets/Commandlet.h"
#include "DiffPackagesCommandlet.generated.h"

/**
 * The different types of comparison differences that can exist between packages.
 */
enum EObjectDiff
{
	/** no difference */
	OD_None,

	/** the object exist in the first package only */
	OD_AOnly,

	/** the object exists in the second package only */
	OD_BOnly,

	/** (three-way merges) the value has been changed from the ancestor package, but the new value is identical in the two packages being compared */
	OD_ABSame,

	/** @todo */
	OD_ABConflict,

	/** @todo */
	OD_Invalid,
};

UCLASS()
class UDiffPackagesCommandlet : public UCommandlet
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	UPackage* Packages[3];

	bool bDiffNonEditProps;
	bool bDiffAllProps;
	
	FString PackageFilenames[3];
	int32 NumPackages;

	/** Handled annotation to track which objects we have dealt with **/
	class FUObjectAnnotationSparseBool HandledAnnotation;

	/**
	 * Parses the command-line and loads the packages being compared.
	 *
	 * @param	Parms	the full command-line used to invoke this commandlet
	 *
	 * @return	true if all parameters were parsed successfully; false if any of the specified packages couldn't be loaded
	 *			or the parameters were otherwise invalid.
	 */
	bool Initialize( const TCHAR* Parms );

	/**
	 * Generates object graphs for the specified object and its corresponding objects in all packages being diffed.
	 *
	 * @param	RootObject			the object to generate the object comparison for
	 * @param	out_Comparison		the object graphs for the specified object for each package being diffed
	 * @param	ObjectsToIgnore		if specified, this list will be passed to the FObjectGraphs created for this comparison; this will prevent those object graphs from containing
	 *								these objects.  Useful when generating an object comparison for package-root type objects, such as levels, worlds, etc. to prevent their comparison's
	 *								object graphs from containing all objects in the level/world
	 *
	 * @return	true if RootObject was found in any of the packages being compared.
	 */
	bool GenerateObjectComparison( UObject* RootObject, struct FObjectComparison& out_Comparison, TArray<struct FObjectComparison>* ObjectsToIgnore=NULL );
	
	// @todo document
	bool ProcessDiff(struct FObjectComparison& Diff);

	EObjectDiff DiffObjects(UObject* ObjA, UObject* ObjB, UObject* ObjAncestor, struct FObjectComparison& PropDiffs);

	/**
	 * Copies the raw property values for the natively serialized properties of the specified object into the output var.
	 *
	 * @param	Object	the object to load values for
	 * @param	out_NativePropertyData	receives the raw bytes corresponding to Object's natively serialized property values.
	 */
	static void LoadNativePropertyData( UObject* Object, TArray<uint8>& out_NativePropertyData );

	/**
	 * Compares the natively serialized property values for the specified objects by comparing the non-script serialized portion of each object's data as it
	 * is on disk.  If a different is detected, gives each object the chance to generate a textual representation of its natively serialized property values
	 * that will be displayed to the user in the final comparison report.
	 *
	 * @param	ObjA		the object from the first package being compared.  Can be NULL if both ObjB and ObjAncestor are valid, which indicates that this object
	 *						doesn't exist in the first package.
	 * @param	ObjB		the object from the second package being compared.  Can be NULL if both ObjA and ObjAncestor are valid, which indicates that this object
	 *						doesn't exist in the second package.
	 * @param	ObjAncestor	the object from the optional common base package.  Can only be NULL if both ObjA and ObjB are valid, which indicates that this is either
	 *						a two-way comparison (if NumPackages == 2) or the object was added to both packages (if NumPackages == 3)
	 * @param	PropertyValueComparisons	contains the results for all property values that were different so far; for any native property values which are determined
	 *										to be different, new entries will be added to the ObjectComparison's list of PropDiffs.
	 *
	 * @return	The cumulative comparison result type for a comparison of all natively serialized property values.
	 *
	 */
	EObjectDiff CompareNativePropertyValues( UObject* ObjA, UObject* ObjB, UObject* ObjAncestor, struct FObjectComparison& PropertyValueComparisons );

	//~ Begin UCommandlet Interface
	virtual int32 Main(const FString& Params) override;
	//~ End UCommandlet Interface
};


