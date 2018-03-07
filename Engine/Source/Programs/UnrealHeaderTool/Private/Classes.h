// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ParserClass.h"
#include "UObject/ClassTree.h"

#define WIP_UHT_REFACTOR 1

class UPackage;
class FString;
class FClass;

class FClasses
{
public:
	explicit FClasses(UPackage* InPackage);

	/**
	 * Returns the root class (i.e. UObject)
	 *
	 * @return The root class.
	 */
	FClass* GetRootClass() const;

	/**
	 * Determines whether the class hierarchy rooted at Suspect is
	 * dependent on the hierarchy rooted at Source.
	 *
	 * @param   Suspect  Root of hierarchy for suspect class
	 * @param   Source   Root of hierarchy for source class
	 * @return           true if the hierarchy rooted at Suspect is dependent on the one rooted at Source, false otherwise
	 */
	bool IsDependentOn(const FClass* Suspect, const FClass* Source) const;

	FClass* FindClass(const TCHAR* ClassName) const;

	TArray<FClass*> GetDerivedClasses(FClass* Parent) const;

	FClass* FindAnyClass(const TCHAR* ClassName) const;

	/** 
	 * Attempts to find a script class based on the given name. Will attempt to strip
	 * the prefix of the given name while searching. Throws script errors when appropriate.
	 *
	 * @param   InClassName  Name w/ Unreal prefix to use when searching for a class
	 * @return               The found class, or NULL if the class was not found.
	 */
	FClass* FindScriptClass(const FString& InClassName) const;

	/** 
	 * Attempts to find a script class based on the given name. Will attempt to strip
	 * the prefix of the given name while searching. Throws an exception with the script error
	 * if the class could not be found.
	 *
	 * @param   InClassName  Name w/ Unreal prefix to use when searching for a class
	 * @return               The found class.
	 */
	FClass* FindScriptClassOrThrow(const FString& InClassName) const;

	/** 
	 * Attempts to find a script class based on the given name. Will attempt to strip
	 * the prefix of the given name while searching. Throws script errors when appropriate.
	 *
	 * @param   InClassName  Name w/ Unreal prefix to use when searching for a class
	 * @param   OutErrorMsg  Error message (if any) giving the caller flexibility in how they present an error
	 * @return               The found class, or NULL if the class was not found.
	 */
	FClass* FindScriptClass(const FString& InClassName, FString& OutErrorMsg) const;

	/**
	 * Returns an array of classes for the given package.
	 *
	 * @param   InPackage  The package to return the classes from.
	 * @return             The classes in the specified package.
	 */
	TArray<FClass*> GetClassesInPackage(const UPackage* InPackage = ANY_PACKAGE) const;

// Anything in here should eventually be removed when this class encapsulates its own data structure, rather than being 'poked' by the outside
#if WIP_UHT_REFACTOR

	/**
	 * Move a class node in the hierarchy tree after a class has changed its SuperClass
	 * 
	 * @param	SearchClass			the class that has changed parents
	 * @param	InNewParentClass	if non-null force reparenting to this instead of the SuperClass
	 *
	 * @return	true if SearchClass was successfully moved to the new location
	 */
	void ChangeParentClass(FClass* Class);

	bool ContainsClass(const FClass* Class) const;

	/**
	 * Validates the state of the tree (shouldn't be needed once this class has well-defined invariants).
	 */
	void Validate();

	FORCEINLINE FClassTree& GetClassTree()
	{
		return ClassTree;
	}

#endif

private:
	FClass*     UObjectClass;
	FClassTree  ClassTree;

	/**
	* Determines whether the class hierarchy rooted at Suspect is dependent on the hierarchy rooted at Source.
	* Used by the public overload of IsDependentOn to recursively track dependencies and handle circular references
	*/
	bool IsDependentOn(const FClass* Suspect, const FClass* Source, TSet<const FClass*>& VisitedDpendencies) const;

	friend auto begin(const FClasses& Classes) -> decltype(begin(TObjectRange<FClass>())) { return begin(TObjectRange<FClass>()); }
	friend auto end  (const FClasses& Classes) -> decltype(end  (TObjectRange<FClass>())) { return end  (TObjectRange<FClass>()); }
};
