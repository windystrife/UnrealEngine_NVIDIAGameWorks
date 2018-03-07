// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "ClassIconFinder.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"
#include "AssetData.h"
#include "Engine/Blueprint.h"
#include "Engine/Brush.h"

const FSlateBrush* FClassIconFinder::FindIconForActors(const TArray< TWeakObjectPtr<AActor> >& InActors, UClass*& CommonBaseClass)
{
	// Get the common base class of the selected actors
	FSlateIcon CommonIcon;

	for( int32 ActorIdx = 0; ActorIdx < InActors.Num(); ++ActorIdx )
	{
		TWeakObjectPtr<AActor> Actor = InActors[ActorIdx];
		UClass* ObjClass = Actor->GetClass();
		check(ObjClass);

		if (!CommonBaseClass)
		{
			CommonBaseClass = ObjClass;
		}
		while (!ObjClass->IsChildOf(CommonBaseClass))
		{
			CommonBaseClass = CommonBaseClass->GetSuperClass();
		}

		FSlateIcon ActorIcon = FindSlateIconForActor(Actor);

		if (!CommonIcon.IsSet())
		{
			CommonIcon = ActorIcon;
		}

		if (CommonIcon != ActorIcon)
		{
			CommonIcon = FSlateIconFinder::FindIconForClass(CommonBaseClass);
		}
	}

	return CommonIcon.GetOptionalIcon();
}

FSlateIcon FClassIconFinder::FindSlateIconForActor( const TWeakObjectPtr<AActor>& InActor )
{
	// Actor specific overrides to normal per-class icons
	AActor* Actor = InActor.Get();

	if ( Actor )
	{
		ABrush* Brush = Cast< ABrush >( Actor );
		if ( Brush )
		{
			if (Brush_Add == Brush->BrushType)
			{
				return FSlateIconFinder::FindIcon("ClassIcon.BrushAdditive");
			}
			else if (Brush_Subtract == Brush->BrushType)
			{
				return FSlateIconFinder::FindIcon("ClassIcon.BrushSubtractive");
			}
		}

		// Actor didn't specify an icon - fallback on the class icon
		return FSlateIconFinder::FindIconForClass(Actor->GetClass());
	}
	else
	{
		// If the actor reference is NULL it must have been deleted
		return FSlateIconFinder::FindIcon("ClassIcon.Deleted");
	}
}

const FSlateBrush* FClassIconFinder::FindIconForActor( const TWeakObjectPtr<AActor>& InActor )
{
	return FindSlateIconForActor(InActor).GetOptionalIcon();
}

const UClass* FClassIconFinder::GetIconClassForBlueprint(const UBlueprint* InBlueprint)
{
	if ( !InBlueprint )
	{
		return nullptr;
	}

	// If we're loaded and have a generated class, just use that
	if ( const UClass* GeneratedClass = InBlueprint->GeneratedClass )
	{
		return GeneratedClass;
	}

	// We don't have a generated class, so instead try and use the parent class from our meta-data
	return GetIconClassForAssetData(FAssetData(InBlueprint));
}

const UClass* FClassIconFinder::GetIconClassForAssetData(const FAssetData& InAssetData, bool* bOutIsClassType)
{
	if ( bOutIsClassType )
	{
		*bOutIsClassType = false;
	}

	UClass* AssetClass = FindObject<UClass>(ANY_PACKAGE, *InAssetData.AssetClass.ToString());
	if ( !AssetClass )
	{
		return nullptr;
	}

	if ( AssetClass == UClass::StaticClass() )
	{
		if ( bOutIsClassType )
		{
			*bOutIsClassType = true;
		}

		return FindObject<UClass>(ANY_PACKAGE, *InAssetData.AssetName.ToString());
	}
	
	if ( AssetClass == UBlueprint::StaticClass() )
	{
		static const FName NativeParentClassTag = "NativeParentClass";
		static const FName ParentClassTag = "ParentClass";

		if ( bOutIsClassType )
		{
			*bOutIsClassType = true;
		}

		// We need to use the asset data to get the parent class as the blueprint may not be loaded
		FString ParentClassName;
		if ( !InAssetData.GetTagValue(NativeParentClassTag, ParentClassName) )
		{
			InAssetData.GetTagValue(ParentClassTag, ParentClassName);
		}
		if ( !ParentClassName.IsEmpty() )
		{
			UObject* Outer = nullptr;
			ResolveName(Outer, ParentClassName, false, false);
			return FindObject<UClass>(ANY_PACKAGE, *ParentClassName);
		}
	}

	// Default to using the class for the asset type
	return AssetClass;
}
