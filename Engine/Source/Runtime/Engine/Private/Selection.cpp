// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "Engine/Selection.h"
#include "UObject/Package.h"
#include "UObject/UObjectAnnotation.h"

DEFINE_LOG_CATEGORY_STATIC(LogSelection, Log, All);

USelection::FOnSelectionChanged	USelection::SelectionChangedEvent;
USelection::FOnSelectionChanged	USelection::SelectObjectEvent;								
FSimpleMulticastDelegate		USelection::SelectNoneEvent;


USelection::USelection(const FObjectInitializer& ObjectInitializer)
:	UObject(ObjectInitializer)
,	SelectionMutex( 0 )
,	bIsBatchDirty(false)
,	SelectionAnnotation(nullptr)
,	bOwnsSelectionAnnotation(false)
{

}


void USelection::Initialize(FUObjectAnnotationSparseBool* InSelectionAnnotation)
{
	if (InSelectionAnnotation)
	{
		SelectionAnnotation = InSelectionAnnotation;
		bOwnsSelectionAnnotation = false;
	}
	else
	{
		SelectionAnnotation = new FUObjectAnnotationSparseBool;
		bOwnsSelectionAnnotation = true;
	}
}

void USelection::Select(UObject* InObject)
{
	check( InObject );

	const bool bSelectionChanged = !SelectionAnnotation->Get(InObject);
	SelectionAnnotation->Set(InObject);

	if(bSelectionChanged)
	{
		// Add to selected lists.
		SelectedObjects.Add(InObject);
		MarkBatchDirty();
	}

	FSelectedClassInfo* SelectedClassInfo = SelectedClasses.Find(InObject->GetClass());
	if( SelectedClassInfo )
	{
		++SelectedClassInfo->SelectionCount;
	}
	else
	{
		// 1 Object with a new class type has been selected
		FSelectedClassInfo NewInfo(InObject->GetClass(), 1);
		SelectedClasses.Add(NewInfo);
	}

	if( !IsBatchSelecting() )
	{
		// Call this after the item has been added from the selection set.
		USelection::SelectObjectEvent.Broadcast( InObject );
	}
}


void USelection::Deselect(UObject* InObject)
{
	check( InObject );

	const bool bSelectionChanged = InObject->IsSelected();
	SelectionAnnotation->Clear(InObject);

	// Remove from selected list.
	SelectedObjects.Remove( InObject );

	if( !IsBatchSelecting() )
	{
		// Call this after the item has been removed from the selection set.
		USelection::SelectObjectEvent.Broadcast( InObject );
	}

	FSetElementId Id = SelectedClasses.FindId(InObject->GetClass());
	if(Id.IsValidId())
	{
		FSelectedClassInfo& ClassInfo = SelectedClasses[Id];
		// One less object of this class is selected;
		--ClassInfo.SelectionCount;
		// If no more objects of the selected class exists, remove it
		if(ClassInfo.SelectionCount == 0)
		{
			SelectedClasses.Remove(Id);
		}
	}


	if ( bSelectionChanged )
	{
		MarkBatchDirty();
	}
}


void USelection::Select(UObject* InObject, bool bSelect)
{
	if( bSelect )
	{
		Select( InObject );
	}
	else
	{
		Deselect( InObject );
	}
}


void USelection::ToggleSelect( UObject* InObject )
{
	check( InObject );

	Select( InObject, !InObject->IsSelected() );
}

void USelection::DeselectAll( UClass* InClass )
{
	// Fast path for deselecting all UObjects with any flags
	if ( InClass == UObject::StaticClass() )
	{
		InClass = nullptr;
	}

	bool bSelectionChanged = false;

	TSet<FSelectedClassInfo> RemovedClasses;
	// Remove from the end to minimize memmoves.
	for ( int32 i = SelectedObjects.Num()-1 ; i >= 0 ; --i )
	{
		UObject* Object = GetSelectedObject(i);

		if ( !Object )
		{		
			// Remove NULLs from SelectedObjects array.
			SelectedObjects.RemoveAt( i );
		}
		else if( !InClass || Object->IsA( InClass ) )
		{
			// if the object is of type InClass then all objects of that same type will be removed
			RemovedClasses.Add(FSelectedClassInfo(Object->GetClass()));

			SelectionAnnotation->Clear(Object);
			SelectedObjects.RemoveAt( i );

			// Call this after the item has been removed from the selection set.
			USelection::SelectObjectEvent.Broadcast( Object );

			bSelectionChanged = true;
		}
	}

	if( InClass == nullptr )
	{
		SelectedClasses.Empty();
	}
	else
	{
		// Remove the passed in class and all child classes that were removed
		// from the list of currently selected classes
		RemovedClasses.Add(InClass);
		SelectedClasses = SelectedClasses.Difference(RemovedClasses);
	}

	if ( bSelectionChanged )
	{
		MarkBatchDirty();
		if ( !IsBatchSelecting() )
		{
			USelection::SelectionChangedEvent.Broadcast(this);
		}
	}
}

void USelection::MarkBatchDirty()
{
	if ( IsBatchSelecting() )
	{
		bIsBatchDirty = true;
	}
}



bool USelection::IsSelected(const UObject* InObject) const
{
	if ( InObject )
	{
		return SelectedObjects.Contains(InObject);
	}

	return false;
}


void USelection::Serialize(FArchive& Ar)
{
	Super::Serialize( Ar );
	Ar << SelectedObjects;

	if(Ar.IsLoading())
	{
		// The set of selected objects may have changed, so make sure our annotations exactly match the list, otherwise
		// UObject::IsSelected() could return a result that was different from the list of objects returned by GetSelectedObjects()
		// This needs to happen in serialize because other code may check the selection state in PostEditUndo and the order of PostEditUndo is indeterminate.
		SelectionAnnotation->ClearAll();

		for(TWeakObjectPtr<UObject>& ObjectPtr : SelectedObjects)
		{
			if (UObject* Object = ObjectPtr.Get(true))
			{
				SelectionAnnotation->Set(Object);
			}
		}
	}
}

bool USelection::Modify(bool bAlwaysMarkDirty/* =true */)
{
	// If the selection currently contains any PIE objects we should not be including it in the transaction buffer
	for (TWeakObjectPtr<UObject>& ObjectPtr : SelectedObjects)
	{
		UObject* Object = ObjectPtr.Get();
		if (Object && Object->GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor | PKG_ContainsScript | PKG_CompiledIn))
		{
			return false;
		}
	}

	return Super::Modify(bAlwaysMarkDirty);
}

void USelection::BeginDestroy()
{
	Super::BeginDestroy();

	if (bOwnsSelectionAnnotation)
	{
		delete SelectionAnnotation;
		SelectionAnnotation = nullptr;
	}
}

