// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameFramework/Actor.h"
#include "Input/DragAndDrop.h"
#include "Editor.h"
#include "UObject/UObjectIterator.h"
#include "UObject/PropertyPortFlags.h"
#include "Exporters/Exporter.h"
#include "UnrealExporter.h"

class FSelectedActorExportObjectInnerContext;

class FSelectedActorExportObjectInnerContext : public FExportObjectInnerContext
{
public:
	FSelectedActorExportObjectInnerContext()
		//call the empty version of the base class
		: FExportObjectInnerContext(false)
	{
		// For each object . . .
		for ( TObjectIterator<UObject> It ; It ; ++It )
		{
			UObject* InnerObj = *It;
			UObject* OuterObj = InnerObj->GetOuter();

			//assume this is not part of a selected actor
			bool bIsChildOfSelectedActor = false;

			UObject* TestParent = OuterObj;
			while (TestParent)
			{
				AActor* TestParentAsActor = Cast<AActor>(TestParent);
				if ( TestParentAsActor && TestParentAsActor->IsSelected())
				{
					bIsChildOfSelectedActor = true;
					break;
				}
				TestParent = TestParent->GetOuter();
			}

			if (bIsChildOfSelectedActor)
			{
				InnerList* Inners = ObjectToInnerMap.Find( OuterObj );
				if ( Inners )
				{
					// Add object to existing inner list.
					Inners->Add( InnerObj );
				}
				else
				{
					// Create a new inner list for the outer object.
					InnerList& InnersForOuterObject = ObjectToInnerMap.Add( OuterObj, InnerList() );
					InnersForOuterObject.Add( InnerObj );
				}
			}
		}
	}
};

class FExportTextDragDropOp : public FDragDropOperation
{
public:
	DRAG_DROP_OPERATOR_TYPE(FExportTextDragDropOp, FDragDropOperation)

	FString ActorExportText;
	int32 NumActors;

	/** The widget decorator to use */
	//virtual TSharedPtr<SWidget> GetDefaultDecorator() const override
	//{
		
	//}

	static TSharedRef<FExportTextDragDropOp> New(const TArray<AActor*>& InActors)
	{
		TSharedRef<FExportTextDragDropOp> Operation = MakeShareable(new FExportTextDragDropOp);

		for(int32 i=0; i<InActors.Num(); i++)
		{
			AActor* Actor = InActors[i];

			GEditor->SelectActor(Actor, true, true);
		}

		FStringOutputDevice Ar;
		const FSelectedActorExportObjectInnerContext Context;
		UExporter::ExportToOutputDevice( &Context, GWorld, NULL, Ar, TEXT("copy"), 0, PPF_DeepCompareInstances | PPF_ExportsNotFullyQualified);
		Operation->ActorExportText = Ar;
		Operation->NumActors = InActors.Num();
		
		Operation->Construct();

		return Operation;
	}
};

