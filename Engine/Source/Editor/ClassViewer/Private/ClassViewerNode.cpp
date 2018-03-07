// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ClassViewerNode.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "Engine/Brush.h"

#include "ClassViewerFilter.h"
#include "PropertyHandle.h"

FClassViewerNode::FClassViewerNode( const FString& InClassName, const FString& InClassDisplayName )
{
	ClassName = MakeShareable(new FString(InClassName));
	ClassDisplayName = MakeShareable(new FString(InClassDisplayName));
	bPassesFilter = false;
	bIsBPNormalType = false;

	Class = NULL;
	Blueprint = NULL;
}

FClassViewerNode::FClassViewerNode( const FClassViewerNode& InCopyObject)
{
	ClassName = InCopyObject.ClassName;
	ClassDisplayName = InCopyObject.ClassDisplayName;
	bPassesFilter = InCopyObject.bPassesFilter;

	Class = InCopyObject.Class;
	Blueprint = InCopyObject.Blueprint;
	
	UnloadedBlueprintData = InCopyObject.UnloadedBlueprintData;

	GeneratedClassPackage = InCopyObject.GeneratedClassPackage;
	GeneratedClassname = InCopyObject.GeneratedClassname;
	ParentClassname = InCopyObject.ParentClassname;
	ClassName = InCopyObject.ClassName;
	AssetName = InCopyObject.AssetName;
	bIsBPNormalType = InCopyObject.bIsBPNormalType;

	// We do not want to copy the child list, do not add it. It should be the only item missing.
}

/**
 * Adds the specified child to the node.
 *
 * @param	Child							The child to be added to this node for the tree.
 */
void FClassViewerNode::AddChild( TSharedPtr<FClassViewerNode> Child )
{
	check(Child.IsValid());
	ChildrenList.Add(Child);
}

void FClassViewerNode::AddUniqueChild(TSharedPtr<FClassViewerNode> NewChild)
{
	check(NewChild.IsValid());
	const UClass* NewChildClass = NewChild->Class.Get();
	if(NULL != NewChildClass)
	{
		for(int ChildIndex = 0; ChildIndex < ChildrenList.Num(); ++ChildIndex)
		{
			TSharedPtr<FClassViewerNode> OldChild = ChildrenList[ChildIndex];
			if(OldChild.IsValid() && OldChild->Class == NewChildClass)
			{
				const bool bNewChildHasMoreInfo = NewChild->UnloadedBlueprintData.IsValid();
				const bool bOldChildHasMoreInfo = OldChild->UnloadedBlueprintData.IsValid();
				if(bNewChildHasMoreInfo && !bOldChildHasMoreInfo)
				{
					// make sure, that new child has all needed children
					for(int OldChildIndex = 0; OldChildIndex < OldChild->ChildrenList.Num(); ++OldChildIndex)
					{
						NewChild->AddUniqueChild( OldChild->ChildrenList[OldChildIndex] );
					}

					// replace child
					ChildrenList[ChildIndex] = NewChild;
				}
				return;
			}
		}
	}

	AddChild(NewChild);
}

bool FClassViewerNode::IsRestricted() const
{
	return PropertyHandle.IsValid() && PropertyHandle->IsRestricted(*ClassName);
}

bool FClassViewerNode::IsClassPlaceable() const
{
	const UClass* LoadedClass = Class.Get();
	if (LoadedClass)
	{
		const bool bPlaceableFlags = !LoadedClass->HasAnyClassFlags(CLASS_Abstract | CLASS_NotPlaceable);
		const bool bBasedOnActor = LoadedClass->IsChildOf(AActor::StaticClass());
		const bool bNotABrush = !LoadedClass->IsChildOf(ABrush::StaticClass());
		return bPlaceableFlags && bBasedOnActor && bNotABrush;
	}

	if (UnloadedBlueprintData.IsValid())
	{
		const bool bPlaceableFlags = !UnloadedBlueprintData->HasAnyClassFlags(CLASS_Abstract | CLASS_NotPlaceable);
		const bool bBasedOnActor = UnloadedBlueprintData->IsChildOf(AActor::StaticClass());
		const bool bNotABrush = !UnloadedBlueprintData->IsChildOf(ABrush::StaticClass());
		return bPlaceableFlags && bBasedOnActor && bNotABrush;
	}

	return false;
}
