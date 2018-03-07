// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ConstructorHelpers.h: Constructor helper templates
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "UObject/Package.h"

namespace ConstructorHelpersInternal
{
	template<typename T>
	inline T* FindOrLoadObject( FString& PathName )
	{
		// If there is no dot, add a dot and repeat the object name.
		int32 PackageDelimPos = INDEX_NONE;
		PathName.FindChar( TCHAR('.'), PackageDelimPos );
		if( PackageDelimPos == INDEX_NONE )
		{
			int32 ObjectNameStart = INDEX_NONE;
			PathName.FindLastChar( TCHAR('/'), ObjectNameStart );
			if( ObjectNameStart != INDEX_NONE )
			{
				const FString ObjectName = PathName.Mid( ObjectNameStart+1 );
				PathName += TCHAR('.');
				PathName += ObjectName;
			}
		}

		UClass* Class = T::StaticClass();
		Class->GetDefaultObject(); // force the CDO to be created if it hasn't already
		T* ObjectPtr = LoadObject<T>(NULL, *PathName);
		if (ObjectPtr)
		{
			ObjectPtr->AddToRoot();
		}
		return ObjectPtr;
	}

	template<>
	inline UPackage* FindOrLoadObject<UPackage>( FString& PathName )
	{
		// If there is a dot, remove it.
		int32 PackageDelimPos = INDEX_NONE;
		PathName.FindChar( TCHAR('.'), PackageDelimPos );
		if( PackageDelimPos != INDEX_NONE )
		{
			PathName.RemoveAt(PackageDelimPos,1,false);
		}

		// Find the package in memory. 
		UPackage* PackagePtr = FindPackage( nullptr, *PathName );
		if( !PackagePtr )
		{
			// If it is not in memory, try to load it.
			PackagePtr = LoadPackage( nullptr, *PathName, LOAD_None );
		}
		if (PackagePtr)
		{
			PackagePtr->AddToRoot();
		}
		return PackagePtr;
	}

	inline UClass* FindOrLoadClass(FString& PathName, UClass* BaseClass)
	{
		// If there is no dot, add ".<object_name>_C"
		int32 PackageDelimPos = INDEX_NONE;
		PathName.FindChar(TCHAR('.'), PackageDelimPos);
		if (PackageDelimPos == INDEX_NONE)
		{
			int32 ObjectNameStart = INDEX_NONE;
			PathName.FindLastChar(TCHAR('/'), ObjectNameStart);
			if (ObjectNameStart != INDEX_NONE)
			{
				const FString ObjectName = PathName.Mid(ObjectNameStart + 1);
				PathName += TCHAR('.');
				PathName += ObjectName;
				PathName += TCHAR('_');
				PathName += TCHAR('C');
			}
		}
		UClass* LoadedClass = StaticLoadClass(BaseClass, NULL, *PathName);
		if (LoadedClass)
		{
			LoadedClass->AddToRoot();
		}
		return LoadedClass;
	}
}

struct COREUOBJECT_API ConstructorHelpers
{
public:
	template<class T>
	struct FObjectFinder
	{
		T* Object;
		FObjectFinder(const TCHAR* ObjectToFind)
		{
			CheckIfIsInConstructor(ObjectToFind);
			FString PathName(ObjectToFind);
			StripObjectClass(PathName,true);

			Object = ConstructorHelpersInternal::FindOrLoadObject<T>(PathName);
			ValidateObject( Object, PathName, ObjectToFind );
		}
		bool Succeeded() const
		{
			return !!Object;
		}
	};

	template<class T>
	struct FObjectFinderOptional
	{
	private:
		T* Object;
		const TCHAR* ObjectToFind;
	public:
		FObjectFinderOptional(const TCHAR* InObjectToFind)
			: Object(nullptr)
			, ObjectToFind(InObjectToFind)
		{
		}
		T* Get()
		{
			if (!Object && ObjectToFind)
			{
				CheckIfIsInConstructor(ObjectToFind);
				FString PathName(ObjectToFind);
				StripObjectClass(PathName,true);

				Object = ConstructorHelpersInternal::FindOrLoadObject<T>(PathName);
				ValidateObject( Object, PathName, ObjectToFind );

				ObjectToFind = nullptr; // don't try to look again
			}
			return Object;
		}
		bool Succeeded()
		{
			return !!Get();
		}
	};

	template<class T>
	struct FClassFinder
	{
		TSubclassOf<T> Class;
		FClassFinder(const TCHAR* ClassToFind)
		{
			CheckIfIsInConstructor(ClassToFind);
			FString PathName(ClassToFind);
			StripObjectClass(PathName, true);
			Class = ConstructorHelpersInternal::FindOrLoadClass(PathName, T::StaticClass());
			ValidateObject(*Class, PathName, *PathName);
		}
		bool Succeeded()
		{
			return !!*Class;
		}
	};

public:
	/** If there is an object class, strips it off. */
	static void StripObjectClass( FString& PathName, bool bAssertOnBadPath = false );

private:
	static void ValidateObject(UObject *Object, const FString& PathName, const TCHAR* ObjectToFind)
	{
		if (!Object)
		{
			FailedToFind(ObjectToFind);
		}
#if UE_BUILD_DEBUG
		else 
		{
			CheckFoundViaRedirect(Object, PathName, ObjectToFind);
		}
#endif
	}

	static void FailedToFind(const TCHAR* ObjectToFind);
	static void CheckFoundViaRedirect(UObject *Object, const FString& PathName, const TCHAR* ObjectToFind);
	static void CheckIfIsInConstructor(const TCHAR* ObjectToFind);
};


