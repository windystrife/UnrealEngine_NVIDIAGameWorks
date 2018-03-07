// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

class FClassViewerInitializationOptions;

/** Interface class for creating filters for the Class Viewer. */
class IClassViewerFilter
{
public:
	virtual ~IClassViewerFilter() {}

	/**
	 * Checks if a class is allowed by this filter.
	 *
	 * @param InInitOptions				The Class Viewer/Picker options.
	 * @param InClass					The class to be tested.
	 * @param InFilterFuncs				Useful functions for filtering.
	 */
	virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< class FClassViewerFilterFuncs > InFilterFuncs ) = 0;

	/**
	 * Checks if a class is allowed by this filter.
	 *
	 * @param InInitOptions				The Class Viewer/Picker options.
	 * @param InUnloadedClassData		The information for the unloaded class to be tested.
	 * @param InFilterFuncs				Useful functions for filtering.
	 */
	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const class IUnloadedBlueprintData > InUnloadedClassData, TSharedRef< class FClassViewerFilterFuncs > InFilterFuncs) = 0;
};

namespace EFilterReturn
{
	enum Type{ Failed = 0, Passed, NoItems };
}
class CLASSVIEWER_API FClassViewerFilterFuncs
{
public:

	virtual ~FClassViewerFilterFuncs() {}

	/** 
	 * Checks if the given Class is a child-of any of the classes in a set.
	 *
	 * @param InSet				The set to test against.
	 * @param InClass			The class to test against.
	 *
	 * @return					EFilterReturn::Passed if it is a child-of a class in the set, EFilterReturn::Failed if it is not, EFilterReturn::NoItems if the set is empty.
	 */
	virtual EFilterReturn::Type IfInChildOfClassesSet(TSet< const UClass* >& InSet, const UClass* InClass);

	/** 
	 * Checks if the given Class is a child-of any of the classes in a set.
	 *
	 * @param InSet				The set to test against.
	 * @param InClass			The class to test against.
	 *
	 * @return					EFilterReturn::Passed if it is a child-of a class in the set, EFilterReturn::Failed if it is not, EFilterReturn::NoItems if the set is empty.
	 */
	virtual EFilterReturn::Type IfInChildOfClassesSet(TSet< const UClass* >& InSet, const TSharedPtr< const class IUnloadedBlueprintData > InClass);

	/** 
	 * Checks if the given Class is a child-of ALL of the classes in a set.
	 *
	 * @param InSet				The set to test against.
	 * @param InClass			The class to test against.
	 *
	 * @return					EFilterReturn::Passed if it is a child-of ALL the classes in the set, EFilterReturn::Failed if it is not, EFilterReturn::NoItems if the set is empty.
	 */
	virtual EFilterReturn::Type IfMatchesAllInChildOfClassesSet(TSet< const UClass* >& InSet, const UClass* InClass);

	/** 
	 * Checks if the given Class is a child-of ALL of the classes in a set.
	 *
	 * @param InSet				The set to test against.
	 * @param InClass			The class to test against.
	 *
	 * @return					EFilterReturn::Passed if it is a child-of ALL the classes in the set, EFilterReturn::Failed if it is not, EFilterReturn::NoItems if the set is empty.
	 */
	virtual EFilterReturn::Type IfMatchesAllInChildOfClassesSet(TSet< const UClass* >& InSet, const TSharedPtr< const class IUnloadedBlueprintData > InClass);

	/** 
	 * Checks if ALL the Objects has a Is-A relationship with the passed in class.
	 *
	 * @param InSet				The set to test against.
	 * @param InClass			The class to test against.
	 *
	 * @return					EFilterReturn::Passed if ALL the Objects set IsA the passed class, EFilterReturn::Failed if it is not, EFilterReturn::NoItems if the set is empty.
	 */
	virtual EFilterReturn::Type IfMatchesAll_ObjectsSetIsAClass(TSet< const UObject* >& InSet, const UClass* InClass);

	/** 
	 * Checks if ALL the Objects has a Is-A relationship with the passed in class.
	 *
	 * @param InSet				The set to test against.
	 * @param InClass			The class to test against.
	 *
	 * @return					EFilterReturn::Passed if ALL the Objects set IsA the passed class, EFilterReturn::Failed if it is not, EFilterReturn::NoItems if the set is empty.
	 */
	virtual EFilterReturn::Type IfMatchesAll_ObjectsSetIsAClass(TSet< const UObject* >& InSet, const TSharedPtr< const class IUnloadedBlueprintData > InClass);

	/** 
	 * Checks if ALL the Classes set has a Is-A relationship with the passed in class.
	 *
	 * @param InSet				The set to test against.
	 * @param InClass			The class to test against.
	 *
	 * @return					EFilterReturn::Passed if ALL the Classes set IsA the passed class, EFilterReturn::Failed if it is not, EFilterReturn::NoItems if the set is empty.
	 */
	virtual EFilterReturn::Type IfMatchesAll_ClassesSetIsAClass(TSet< const UClass* >& InSet, const UClass* InClass);

	/** 
	 * Checks if ALL the Classes set has a Is-A relationship with the passed in class.
	 *
	 * @param InSet				The set to test against.
	 * @param InClass			The class to test against.
	 *
	 * @return					EFilterReturn::Passed if ALL the Classes set IsA the passed class, EFilterReturn::Failed if it is not, EFilterReturn::NoItems if the set is empty.
	 */
	virtual EFilterReturn::Type IfMatchesAll_ClassesSetIsAClass(TSet< const UClass* >& InSet, const TSharedPtr< const class IUnloadedBlueprintData > InClass);

	/** 
	 * Checks if any in the Classes set has a Is-A relationship with the passed in class.
	 *
	 * @param InSet				The set to test against.
	 * @param InClass			The class to test against.
	 *
	 * @return					EFilterReturn::Passed if the Classes set IsA the passed class, EFilterReturn::Failed if it is not, EFilterReturn::NoItems if the set is empty.
	 */
	virtual EFilterReturn::Type IfMatches_ClassesSetIsAClass(TSet< const UClass* >& InSet, const UClass* InClass);

	/** 
	 * Checks if any in the Classes set has a Is-A relationship with the passed in class.
	 *
	 * @param InSet				The set to test against.
	 * @param InClass			The class to test against.
	 *
	 * @return					EFilterReturn::Passed if the Classes set IsA the passed class, EFilterReturn::Failed if it is not, EFilterReturn::NoItems if the set is empty.
	 */
	virtual EFilterReturn::Type IfMatches_ClassesSetIsAClass(TSet< const UClass* >& InSet, const TSharedPtr< const class IUnloadedBlueprintData > InClass);

	/** 
	 * Checks if the Class is in the Classes set.
	 *
	 * @param InSet				The set to test against.
	 * @param InClass			The class to test against.
	 *
	 * @return					EFilterReturn::Passed if the Class is in the Classes set, EFilterReturn::Failed if it is not, EFilterReturn::NoItems if the set is empty.
	 */
	virtual EFilterReturn::Type IfInClassesSet(TSet< const UClass* >& InSet, const UClass* InClass);

	/** 
	 * Checks if the Class is in the Classes set.
	 *
	 * @param InSet				The set to test against.
	 * @param InClass			The class to test against.
	 *
	 * @return					EFilterReturn::Passed if the Class is in the Classes set, EFilterReturn::Failed if it is not, EFilterReturn::NoItems if the set is empty.
	 */
	virtual EFilterReturn::Type IfInClassesSet(TSet< const UClass* >& InSet, const TSharedPtr< const class IUnloadedBlueprintData > InClass);
};

class IUnloadedBlueprintData
{
public:
	/**
	 * Used to safely check whether the passed in flag is set.
	 *
	 * @param	InFlagsToCheck		Class flag to check for
	 *
	 * @return	true if the passed in flag is set, false otherwise
	 *			(including no flag passed in, unless the FlagsToCheck is CLASS_AllFlags)
	 */
	virtual bool HasAnyClassFlags( uint32 InFlagsToCheck ) const = 0;

	/**
	 * Used to safely check whether all of the passed in flags are set.
	 *
	 * @param InFlagsToCheck	Class flags to check for
	 * @return true if all of the passed in flags are set (including no flags passed in), false otherwise
	 */
	virtual bool HasAllClassFlags( uint32 InFlagsToCheck ) const = 0;

	/** 
	 * Sets the flags for this class.
	 *
	 * @param InFlags		The flags to be set to.
	 */
	virtual void SetClassFlags(uint32 InFlags) = 0;

	/** 
	 * This will return whether or not this class implements the passed in class / interface 
	 *
	 * @param InInterface - the interface to check and see if this class implements it
	 **/
	virtual bool ImplementsInterface(const UClass* InInterface) const = 0;

	/**
	 * Checks whether or not the class is a child-of the passed in class.
	 *
	 * @param InClass		The class to check against.
	 *
	 * @return				true if it is a child-of the passed in class.
	 */
	virtual bool IsChildOf(const UClass* InClass) const = 0;

	/** 
	 * Checks whether or not the class has an Is-A relationship with the passed in class.
	 *
	 * @param InClass		The class to check against.
	 *
	 * @return				true if it has an Is-A relationship to the passed in class.
	 */
	virtual bool IsA(const UClass* InClass) const = 0;

	/**
	 * Attempts to get the ClassWithin property for this class.
	 *
	 * @return ClassWithin of the child most Native class in the hierarchy.
	 */
	virtual const UClass* GetClassWithin() const = 0;
};
