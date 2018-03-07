// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UObjectArchetype.cpp: Unreal object archetype relationship management
=============================================================================*/

#include "CoreMinimal.h"
#include "UObject/UObjectHash.h"
#include "UObject/Object.h"
#include "UObject/Class.h"
#include "UObject/Package.h"


UObject* UObject::GetArchetypeFromRequiredInfo(UClass* Class, UObject* Outer, FName Name, EObjectFlags ObjectFlags)
{
	UObject* Result = NULL;
	const bool bIsCDO = !!(ObjectFlags&RF_ClassDefaultObject);
	if (bIsCDO)
	{
		Result = Class->GetArchetypeForCDO();
	}
	else
	{
		if (Outer
			&& Outer->GetClass() != UPackage::StaticClass()) // packages cannot have subobjects
		{
			// Get a lock on the UObject hash tables for the duration of the GetArchetype operation
			void LockUObjectHashTables();
			LockUObjectHashTables();

			UObject* ArchetypeToSearch = Outer->GetArchetype();
			UObject* MyArchetype = static_cast<UObject*>(FindObjectWithOuter(ArchetypeToSearch, Class, Name));
			if (MyArchetype)
			{
				Result = MyArchetype; // found that my outers archetype had a matching component, that must be my archetype
			}
			else if (!!(ObjectFlags&RF_InheritableComponentTemplate) && Outer->IsA<UClass>())
			{
				for (UClass* SuperClassArchetype = static_cast<UClass*>(Outer)->GetSuperClass();
				SuperClassArchetype && SuperClassArchetype->HasAllClassFlags(CLASS_CompiledFromBlueprint);
					SuperClassArchetype = SuperClassArchetype->GetSuperClass())
				{
					if (GEventDrivenLoaderEnabled && EVENT_DRIVEN_ASYNC_LOAD_ACTIVE_AT_RUNTIME)
					{
						if (SuperClassArchetype->HasAnyFlags(RF_NeedLoad))
						{
							UE_LOG(LogClass, Fatal, TEXT("%s had RF_NeedLoad when searching supers for an archetype of %s in %s"), *GetFullNameSafe(ArchetypeToSearch), *GetFullNameSafe(Class), *GetFullNameSafe(Outer));
						}
					}
					Result = static_cast<UObject*>(FindObjectWithOuter(SuperClassArchetype, Class, Name));
					// We can have invalid archetypes halfway through the hierarchy, keep looking if it's pending kill or transient
					if (Result && !Result->IsPendingKill() && !Result->HasAnyFlags(RF_Transient))
					{
						break;
					}
				}
			}
			else
			{
				if (GEventDrivenLoaderEnabled && EVENT_DRIVEN_ASYNC_LOAD_ACTIVE_AT_RUNTIME)
				{
					if (ArchetypeToSearch->HasAnyFlags(RF_NeedLoad))
					{
						UE_LOG(LogClass, Fatal, TEXT("%s had RF_NeedLoad when searching for an archetype of %s in %s"), *GetFullNameSafe(ArchetypeToSearch), *GetFullNameSafe(Class), *GetFullNameSafe(Outer));
					}
				}

				Result = ArchetypeToSearch->GetClass()->FindArchetype(Class, Name);
			}

			void UnlockUObjectHashTables();
			UnlockUObjectHashTables();
		}

		if (!Result)
		{
			// nothing found, I am not a CDO, so this is just the class CDO
			Result = Class->GetDefaultObject();
		}
	}

	if (GEventDrivenLoaderEnabled && EVENT_DRIVEN_ASYNC_LOAD_ACTIVE_AT_RUNTIME)
	{
		if (Result && Result->HasAnyFlags(RF_NeedLoad))
		{
			UE_LOG(LogClass, Fatal, TEXT("%s had RF_NeedLoad when being set up as an archetype of %s in %s"), *GetFullNameSafe(Result), *GetFullNameSafe(Class), *GetFullNameSafe(Outer));
		}
	}

	return Result;
}


