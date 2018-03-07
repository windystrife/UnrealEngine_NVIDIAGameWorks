// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Classes.h"
#include "UnrealHeaderTool.h"
#include "UObject/ErrorException.h"
#include "UObject/Package.h"
#include "Templates/Casts.h"
#include "UObject/ObjectRedirector.h"
#include "StringUtils.h"

namespace
{
	/** 
	 * Returns True if the given class name includes a valid Unreal prefix and matches based on the given class.
	 *
	 * @param InNameToCheck - Name w/ potential prefix to check
	 * @param OriginalClass - Class to check against
	 */
	bool ClassNameHasValidPrefix(const FString InNameToCheck, const FClass* OriginalClass)
	{
		bool bIsLabledDeprecated;
		GetClassPrefix( InNameToCheck, bIsLabledDeprecated );

		// If the class is labeled deprecated, don't try to resolve it during header generation, valid results can't be guaranteed.
		if (bIsLabledDeprecated)
		{
			return true;
		}

		const FString OriginalClassName = OriginalClass->GetNameWithPrefix();

		bool bNamesMatch = (InNameToCheck == OriginalClassName);

		if (!bNamesMatch)
		{
			//@TODO: UCREMOVAL: I/U interface hack - Ignoring prefixing for this call
			if (OriginalClass->HasAnyClassFlags(CLASS_Interface))
			{
				bNamesMatch = InNameToCheck.Mid(1) == OriginalClassName.Mid(1);
			}
		}

		return bNamesMatch;
	}
}

FClasses::FClasses(UPackage* InPackage)
	: UObjectClass((FClass*)UObject::StaticClass())
	, ClassTree   (UObjectClass)
{
	for (UClass* Class : TObjectRange<UClass>())
	{
		if (Class->IsIn(InPackage))
		{
			ClassTree.AddClass(Class);
		}
	}
}

FClass* FClasses::GetRootClass() const
{
	return UObjectClass;
}

bool FClasses::IsDependentOn(const FClass* Suspect, const FClass* Source) const
{
	check(Suspect != Source);
	TSet<const FClass*> VisitedDpendencies;
	return IsDependentOn(Suspect, Source, VisitedDpendencies);
}

bool FClasses::IsDependentOn(const FClass* Suspect, const FClass* Source, TSet<const FClass*>& VisitedDpendencies) const
{
	// Children are all implicitly dependent on their parent, that is, children require their parent
	// to be compiled first therefore if the source is a parent of the suspect, the suspect is
	// dependent on the source.
	if (Suspect->IsChildOf(Source))
	{
		return true;
	}

	// Prevent circular #includes from causing inifinite recursion
	// Note that although it may mean there's a circular dependency somewhere, it does not
	// necessarily mean it's the one we're looking for
	if (VisitedDpendencies.Contains(Suspect))
	{
		return false;
	}
	else
	{
		VisitedDpendencies.Add(Suspect);
	}

	return false;
}

FClass* FClasses::FindClass(const TCHAR* ClassName) const
{
	check(ClassName);

	UObject* ClassPackage = ANY_PACKAGE;

	if (UClass* Result = FindObject<UClass>(ClassPackage, ClassName))
		return (FClass*)Result;

	if (UObjectRedirector* RenamedClassRedirector = FindObject<UObjectRedirector>(ClassPackage, ClassName))
		return (FClass*)CastChecked<UClass>(RenamedClassRedirector->DestinationObject);

	return nullptr;
}

TArray<FClass*> FClasses::GetDerivedClasses(FClass* Parent) const
{
	const FClassTree* ClassLeaf = ClassTree.FindNode(Parent);
	TArray<const FClassTree*> ChildLeaves;
	ClassLeaf->GetChildClasses(ChildLeaves);

	TArray<FClass*> Result;
	for (auto Node : ChildLeaves)
	{
		Result.Add((FClass*)Node->GetClass());
	}

	return Result;
}

FClass* FClasses::FindAnyClass(const TCHAR* ClassName) const
{
	check(ClassName);

	return (FClass*)FindObject<UClass>(ANY_PACKAGE, ClassName);
}

FClass* FClasses::FindScriptClass(const FString& InClassName) const
{
	FString ErrorMsg;
	return FindScriptClass(InClassName, ErrorMsg);
}

FClass* FClasses::FindScriptClassOrThrow(const FString& InClassName) const
{
	FString ErrorMsg;
	if (FClass* Result = FindScriptClass(InClassName, ErrorMsg))
	{
		return Result;
	}

	FError::Throwf(*ErrorMsg);

	// Unreachable, but compiler will warn otherwise because FError::Throwf isn't declared noreturn
	return 0;
}

FClass* FClasses::FindScriptClass(const FString& InClassName, FString& OutErrorMsg) const
{
	// Strip the class name of its prefix and then do a search for the class
	FString ClassNameStripped = GetClassNameWithPrefixRemoved(InClassName);
	if (FClass* FoundClass = FindClass(*ClassNameStripped))
	{
		// If the class was found with the stripped class name, verify that the correct prefix was used and throw an error otherwise
		if (!ClassNameHasValidPrefix(InClassName, FoundClass))
		{
			OutErrorMsg = FString::Printf(TEXT("Class '%s' has an incorrect prefix, expecting '%s'"), *InClassName, *FoundClass->GetNameWithPrefix());
			return NULL;
		}

		return (FClass*)FoundClass;
	}

	// Couldn't find the class with a class name stripped of prefix (or a prefix was not found)
	// See if the prefix was forgotten by trying to find the class with the given identifier
	if (FClass* FoundClass = FindClass(*InClassName))
	{
		// If the class was found with the given identifier, the user forgot to use the correct Unreal prefix	
		OutErrorMsg = FString::Printf(TEXT("Class '%s' is missing a prefix, expecting '%s'"), *InClassName, *FoundClass->GetNameWithPrefix());
	}
	else
	{
		// If the class was still not found, it wasn't a valid identifier
		OutErrorMsg = FString::Printf(TEXT("Class '%s' not found."), *InClassName );
	}

	return NULL;
}

TArray<FClass*> FClasses::GetClassesInPackage(const UPackage* InPackage) const
{
	TArray<FClass*> Result;
	Result.Add(UObjectClass);

	// This cast is evil, but it'll work until we get TArray covariance. ;-)
	ClassTree.GetChildClasses((TArray<UClass*>&)Result, [=](const UClass* Class) { return InPackage == ANY_PACKAGE || Class->GetOuter() == InPackage; }, true);

	return Result;
}

#if WIP_UHT_REFACTOR

void FClasses::ChangeParentClass(FClass* Class)
{
	ClassTree.ChangeParentClass(Class);
}

bool FClasses::ContainsClass(const FClass* Class) const
{
	return !!ClassTree.FindNode(const_cast<FClass*>(Class));
}

void FClasses::Validate()
{
	ClassTree.Validate();
}

#endif
