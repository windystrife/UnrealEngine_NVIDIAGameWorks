// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Serialization/TraceReferences.h"
#include "UObject/UnrealType.h"

// This traces referenced/referencer of an object using FArchiveObjectGraph 
FTraceReferences::FTraceReferences( bool bIncludeTransients, EObjectFlags KeepFlags )
: ArchiveObjectGraph ( bIncludeTransients, KeepFlags )
{}

FString FTraceReferences::GetReferencerString( UObject* Object, int32 Depth)
{
	TArray<FObjectGraphNode*> Referencers;
	FString OutString;

	if ( GetReferencer( Object, Referencers, false, Depth ) > 0 )
	{
		int32 CurrentDepth = 0, NumPrinted=0;

		do 
		{
			NumPrinted = 0;
			for (int32 RefId=0; RefId<Referencers.Num(); ++RefId)
			{
				FObjectGraphNode * Node = Referencers[RefId];
				if (CurrentDepth == Node->ReferenceDepth)
				{
					++NumPrinted;
					OutString += FString::Printf(TEXT("(%d) %s%s"), CurrentDepth, *Node->NodeObject->GetPathName(), LINE_TERMINATOR);

					for (int32 Id=0; Id<Node->ReferencerProperties.Num(); ++Id)
					{
						OutString += FString::Printf(TEXT("\t(%d) %s%s"), Id+1, *Node->ReferencerProperties[Id]->GetName(), LINE_TERMINATOR);
					}
				}
			}

			++CurrentDepth;
		} while( NumPrinted > 0 || CurrentDepth == 0);
	}

	return OutString;
}

FString FTraceReferences::GetReferencedString( UObject* Object, int32 Depth)
{
	TArray<FObjectGraphNode*> Referenced;
	FString OutString;

	if ( GetReferenced( Object, Referenced, false, Depth ) > 0 )
	{
		int32 CurrentDepth = 0, NumPrinted=0;

		do 
		{
			NumPrinted = 0;
			for (int32 RefId=0; RefId<Referenced.Num(); ++RefId)
			{
				FObjectGraphNode * Node = Referenced[RefId];
				if (CurrentDepth == Node->ReferenceDepth)
				{
					++NumPrinted;
					OutString += FString::Printf(TEXT("(%d) %s%s"), CurrentDepth, *Node->NodeObject->GetPathName(), LINE_TERMINATOR);

					for (int32 Id=0; Id<Node->ReferencerProperties.Num(); ++Id)
					{
						OutString += FString::Printf(TEXT("\t(%d) %s%s"), Id+1, *Node->ReferencerProperties[Id]->GetName(), LINE_TERMINATOR);
					}
				}
			}

			++CurrentDepth;
		} while( NumPrinted > 0 || CurrentDepth == 0);
	}

	return OutString;
}

int32 FTraceReferences::GetReferencer( UObject* Object, TArray<FObjectGraphNode*> &Referencer, bool bExcludeSelf, int32 Depth)
{
	ArchiveObjectGraph.ClearSearchFlags();
	Referencer.Empty();

	GetReferencerInternal( Object, Referencer, 0, Depth );

	if ( bExcludeSelf )
	{
		// remove head
		Referencer.RemoveAt(0);
	}

	return Referencer.Num();
}

void FTraceReferences::GetReferencerInternal( UObject* CurrentObject, TArray<FObjectGraphNode*> &OutReferencer, int32 CurrentDepth, int32 TargetDepth )
{
	if (TargetDepth >= CurrentDepth)
	{
		FObjectGraphNode * CurrentTarget = ArchiveObjectGraph.ObjectGraph.FindRef(CurrentObject);

		if ( CurrentTarget && !CurrentTarget->Visited && CurrentTarget->ReferencerRecords.Num() > 0 )
		{
			// set Depth and add to outerReferencer
			CurrentTarget->Visited = true;
			CurrentTarget->ReferenceDepth = CurrentDepth;
			OutReferencer.Add(CurrentTarget);

			// go through all referncers and call referencerinternal
			for (TMap<UObject*, FTraceRouteRecord>::TIterator Iter(CurrentTarget->ReferencerRecords); Iter; ++Iter )
			{
				bool HasValidProperty=false;
				FTraceRouteRecord& Referencer = Iter.Value();

				for ( int32 Idx=0; Idx<Referencer.ReferencerProperties.Num(); ++Idx )
				{
					if (Referencer.ReferencerProperties[Idx])
					{
						CurrentTarget->ReferencerProperties.Add(Referencer.ReferencerProperties[Idx]);
						HasValidProperty = true;
					}
				}

				if ( HasValidProperty )
				{
					GetReferencerInternal( Referencer.GraphNode->NodeObject, OutReferencer, CurrentDepth + 1, TargetDepth );
				}
			}
		}
	}
}

int32 FTraceReferences::GetReferenced( UObject* Object, TArray<FObjectGraphNode*> &Referenced, bool bExcludeSelf, int32 Depth)
{
	ArchiveObjectGraph.ClearSearchFlags();
	Referenced.Empty();

	GetReferencedInternal( Object, Referenced, 0, Depth );

	if ( bExcludeSelf )
	{
		// remove head
		Referenced.RemoveAt(0);
	}

	return Referenced.Num();
}

void FTraceReferences::GetReferencedInternal( UObject* CurrentObject, TArray<FObjectGraphNode*> &OutReferenced, int32 CurrentDepth, int32 TargetDepth )
{
	if (TargetDepth >= CurrentDepth)
	{
		FObjectGraphNode * CurrentTarget = ArchiveObjectGraph.ObjectGraph.FindRef(CurrentObject);

		if ( CurrentTarget && !CurrentTarget->Visited && CurrentTarget->ReferencedObjects.Num() > 0 )
		{
			// set Depth and add to outerReferencer
			CurrentTarget->Visited = true;
			CurrentTarget->ReferenceDepth = CurrentDepth;
			OutReferenced.Add(CurrentTarget);

			// go through all refernced and call referencedinternal
			for (TMap<UObject*, FTraceRouteRecord>::TIterator Iter(CurrentTarget->ReferencedObjects); Iter; ++Iter )
			{
				bool HasValidProperty=false;
				FTraceRouteRecord& Referenced = Iter.Value();

				for ( int32 Idx=0; Idx<Referenced.ReferencerProperties.Num(); ++Idx )
				{
					if (Referenced.ReferencerProperties[Idx])
					{
						// For referenced, it makes sense if we add property to the referenced, instead of target
						Referenced.GraphNode->ReferencerProperties.Add(Referenced.ReferencerProperties[Idx]);
						HasValidProperty = true;
						break;
					}
				}

				if ( HasValidProperty )
				{
					// it has property set up, call getreferenced again
					GetReferencedInternal( Referenced.GraphNode->NodeObject, OutReferenced, CurrentDepth + 1, TargetDepth );
				}
			}
		}
	}
}

