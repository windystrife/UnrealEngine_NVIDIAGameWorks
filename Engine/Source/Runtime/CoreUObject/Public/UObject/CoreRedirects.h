// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	CoreRedirects.h: Object/Class/Field redirects read from ini files or registered at startup
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Misc/EnumClassFlags.h"

/** 
 * Flags describing the type and properties of this redirect
 */
enum class ECoreRedirectFlags : int32
{
	None = 0,

	// Core type of the thing being redirected, multiple can be set
	Type_Object =			0x00000001, // UObject
	Type_Class =			0x00000002, // UClass
	Type_Struct =			0x00000004, // UStruct
	Type_Enum =				0x00000008, // UEnum
	Type_Function =			0x00000010, // UFunction
	Type_Property =			0x00000020, // UProperty
	Type_Package =			0x00000040, // UPackage

	// Option flags, specify rules for this redirect
	Option_InstanceOnly =	0x00010000, // Only redirect instances of this type, not the type itself
	Option_Removed =		0x00020000, // This type was explicitly removed, new name isn't valid
	Option_MatchSubstring = 0x00040000, // Does a slow substring match
};
ENUM_CLASS_FLAGS(ECoreRedirectFlags);


/**
 * An object path extracted into component names for matching. TODO merge with FSoftObjectPath?
 */
struct COREUOBJECT_API FCoreRedirectObjectName
{
	/** Raw name of object */
	FName ObjectName;

	/** String of outer chain, may be empty */
	FName OuterName;

	/** Package this was in before, may be extracted out of OldName */
	FName PackageName;

	/** Default to invalid names */
	FCoreRedirectObjectName() {}

	/** Copy constructor */
	FCoreRedirectObjectName(const FCoreRedirectObjectName& Other)
		: ObjectName(Other.ObjectName), OuterName(Other.OuterName), PackageName(Other.PackageName)
	{
	}

	/** Construct from FNames that are already expanded */
	FCoreRedirectObjectName(FName InObjectName, FName InOuterName, FName InPackageName)
		: ObjectName(InObjectName), OuterName(InOuterName), PackageName(InPackageName)
	{

	}

	/** Construct from a path string, this handles full paths with packages, or partial paths without */
	FCoreRedirectObjectName(const FString& InString);

	/** Construct from object in memory */
	FCoreRedirectObjectName(const class UObject* Object);

	/** Creates FString version */
	FString ToString() const;

	/** Sets back to invalid state */
	void Reset();

	/** Checks for exact equality */
	bool operator==(const FCoreRedirectObjectName& Other) const
	{
		return ObjectName == Other.ObjectName && OuterName == Other.OuterName && PackageName == Other.PackageName;
	}

	bool operator!=(const FCoreRedirectObjectName& Other) const
	{
		return !(*this == Other);
	}

	/** Returns true if the passed in name matches requirements, will ignore names that are none */
	bool Matches(const FCoreRedirectObjectName& Other, bool bCheckSubstring = false) const;

	/** Returns integer of degree of match. 0 if doesn't match at all, higher integer for better matches */
	int32 MatchScore(const FCoreRedirectObjectName& Other) const;

	/** Returns the name used as the key into the acceleration map */
	FName GetSearchKey(ECoreRedirectFlags Type) const
	{
		if ((Type & ECoreRedirectFlags::Option_MatchSubstring) == ECoreRedirectFlags::Option_MatchSubstring)
		{
			static FName SubstringName = FName(TEXT("*SUBSTRING*"));

			// All substring matches pass initial test as they need to be manually checked
			return SubstringName;
		}

		if ((Type & ECoreRedirectFlags::Type_Package) == ECoreRedirectFlags::Type_Package)
		{
			return PackageName;
		}

		return ObjectName;
	}

	/** Returns true if this refers to an actual object */
	bool IsValid() const
	{
		return ObjectName != NAME_None || PackageName != NAME_None;
	}

	/** Returns true if all names have valid characters */
	bool HasValidCharacters() const;

	/** Expand OldName/NewName as needed */
	static bool ExpandNames(const FString& FullString, FName& OutName, FName& OutOuter, FName &OutPackage);

	/** Turn it back into an FString */
	static FString CombineNames(FName NewName, FName NewOuter, FName NewPackage);
};

/** 
 * A single redirection from an old name to a new name, parsed out of an ini file
 */
struct COREUOBJECT_API FCoreRedirect
{
	/** Flags of this redirect */
	ECoreRedirectFlags RedirectFlags;

	/** Name of object to look for */
	FCoreRedirectObjectName OldName;

	/** Name to replace with */
	FCoreRedirectObjectName NewName;

	/** Change the class of this object when doing a redirect */
	FCoreRedirectObjectName OverrideClassName;

	/** Map of value changes, from old value to new value */
	TMap<FString, FString> ValueChanges;

	/** Construct from name strings, which may get parsed out */
	FCoreRedirect(const FCoreRedirect& Other)
		: RedirectFlags(Other.RedirectFlags), OldName(Other.OldName), NewName(Other.NewName), OverrideClassName(Other.OverrideClassName), ValueChanges(Other.ValueChanges)
	{
	}

	/** Construct from name strings, which may get parsed out */
	FCoreRedirect(ECoreRedirectFlags InRedirectFlags, FString InOldName, FString InNewName)
		: RedirectFlags(InRedirectFlags), OldName(InOldName), NewName(InNewName)
	{
		NormalizeNewName();
	}
	
	/** Construct parsed out object names */
	FCoreRedirect(ECoreRedirectFlags InRedirectFlags, const FCoreRedirectObjectName& InOldName, const FCoreRedirectObjectName& InNewName)
		: RedirectFlags(InRedirectFlags), OldName(InOldName), NewName(InNewName)
	{
		NormalizeNewName();
	}

	/** Normalizes NewName with data from OldName */
	void NormalizeNewName();

	/** Parses a char buffer into the ValueChanges map */
	const TCHAR* ParseValueChanges(const TCHAR* Buffer);

	/** Returns true if the passed in name matches requirements */
	bool Matches(ECoreRedirectFlags InFlags, const FCoreRedirectObjectName& InName) const;

	/** Returns true if this has value redirects */
	bool HasValueChanges() const;

	/** Returns true if this is a substring match */
	bool IsSubstringMatch() const;

	/** Convert to new names based on mapping */
	FCoreRedirectObjectName RedirectName(const FCoreRedirectObjectName& OldObjectName) const;

	/** See if search criteria is identical */
	bool IdenticalMatchRules(const FCoreRedirect& Other) const
	{
		return RedirectFlags == Other.RedirectFlags && OldName == Other.OldName;
	}

	/** Returns the name used as the key into the acceleration map */
	FName GetSearchKey() const
	{
		return OldName.GetSearchKey(RedirectFlags);
	}
};

/**
 * A container for all of the registered core-level redirects 
 */
struct COREUOBJECT_API FCoreRedirects
{
	/** Returns a redirected version of the object name. If there are no valid redirects, it will return the original name */
	static FCoreRedirectObjectName GetRedirectedName(ECoreRedirectFlags Type, const FCoreRedirectObjectName& OldObjectName);

	/** Returns map of String->String value redirects for the object name, or nullptr if none found */
	static const TMap<FString, FString>* GetValueRedirects(ECoreRedirectFlags Type, const FCoreRedirectObjectName& OldObjectName);

	/** Performs both a name redirect and gets a value redirect struct if it exists. Returns true if either redirect found */
	static bool RedirectNameAndValues(ECoreRedirectFlags Type, const FCoreRedirectObjectName& OldObjectName, FCoreRedirectObjectName& NewObjectName, const FCoreRedirect** FoundValueRedirect);

	/** Returns true if this name has been registered as explicitly missing */
	static bool IsKnownMissing(ECoreRedirectFlags Type, const FCoreRedirectObjectName& ObjectName);

	/** Adds this as a missing name */
	static bool AddKnownMissing(ECoreRedirectFlags Type, const FCoreRedirectObjectName& ObjectName);

	/** Removes this as a missing name */
	static bool RemoveKnownMissing(ECoreRedirectFlags Type, const FCoreRedirectObjectName& ObjectName);

	/** Returns list of names it may have been before */
	static bool FindPreviousNames(ECoreRedirectFlags Type, const FCoreRedirectObjectName& NewObjectName, TArray<FCoreRedirectObjectName>& PreviousNames);

	/** Returns list of all core redirects that match requirements */
	static bool GetMatchingRedirects(ECoreRedirectFlags Type, const FCoreRedirectObjectName& OldObjectName, TArray<const FCoreRedirect*>& FoundRedirects);

	/** Parse all redirects out of a given ini file */
	static bool ReadRedirectsFromIni(const FString& IniName);

	/** Adds an array of redirects to global list */
	static bool AddRedirectList(const TArray<FCoreRedirect>& Redirects, const FString& SourceString);

	/** Returns true if this has ever been initialized from ini */
	static bool IsInitialized() { return bInitialized; }

	/** Gets map from config key -> Flags */
	static const TMap<FName, ECoreRedirectFlags>& GetConfigKeyMap() { return ConfigKeyMap; }

	/** Goes from the containing package and name of the type to the type flag */
	static ECoreRedirectFlags GetFlagsForTypeName(FName PackageName, FName TypeName);

	/** Goes from UClass Type to the type flag */
	static ECoreRedirectFlags GetFlagsForTypeClass(UClass *TypeClass);

	/** Runs set of redirector tests, returns false on failure */
	static bool RunTests();

private:
	/** Static only class, never constructed */
	FCoreRedirects();

	/** Add a single redirect to a type map */
	static bool AddSingleRedirect(const FCoreRedirect& NewRedirect, const FString& SourceString);

	/** Removes an array of redirects from global list */
	static bool RemoveRedirectList(const TArray<FCoreRedirect>& Redirects, const FString& SourceString);

	/** Remove a single redirect from a type map */
	static bool RemoveSingleRedirect(const FCoreRedirect& OldRedirect, const FString& SourceString);

	/** Add native redirects, called before ini is parsed for the first time */
	static void RegisterNativeRedirects();

	/** There is one of these for each registered set of redirect flags */
	struct FRedirectNameMap
	{
		/** Map from name of thing being mapped to full list. List must be filtered further */
		TMap<FName, TArray<FCoreRedirect> > RedirectMap;
	};

	/** Rather this has been initialized at least once */
	static bool bInitialized;

	/** Map from config name to flag */
	static TMap<FName, ECoreRedirectFlags> ConfigKeyMap;

	/** Map from name of thing being mapped to full list. List must be filtered further */
	static TMap<ECoreRedirectFlags, FRedirectNameMap> RedirectTypeMap;
};
