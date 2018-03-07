// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UObject/ReferenceChainSearch.h"
#include "HAL/PlatformStackWalk.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogReferenceChain, Log, All);

namespace Internal
{
	/** Returns a string with the reference types name */
	const TCHAR* RefereceTypeToString(FReferenceChainSearch::EReferenceType::Type Val)
	{
		switch (Val)
		{
			case FReferenceChainSearch::EReferenceType::Property:	return TEXT("Property");
			case FReferenceChainSearch::EReferenceType::ArrayProperty: return TEXT("Array");
			case FReferenceChainSearch::EReferenceType::StructARO: return TEXT("StructARO");
			case FReferenceChainSearch::EReferenceType::ARO: return TEXT("ARO");
			case FReferenceChainSearch::EReferenceType::MapProperty: return TEXT("Map");
			default: return TEXT("Invalid");
		}
	}

	/** Returns the UProperty instances of a struct based on the properties GC offset */
	UProperty* FindPropertyForOffset( UStruct* Struct, uint32 Offset )
	{
		// get property from token stream
		for (TFieldIterator<UProperty> It(Struct, EFieldIteratorFlags::IncludeSuper); It; ++It)
		{
			if (It->GetOffset_ForGC() == Offset)
			{
				return *It;
			}
		}
		return NULL;
	}
}

bool FReferenceChainSearch::FReferenceChain::Contains( const FReferenceChain& Other )
{
	if (RefChain.Num() <= Other.RefChain.Num()) { return false; }

	int32 StartOffset = -1;

	for (int32 i=0; i < RefChain.Num(); ++i)
	{
		if (RefChain[i] == Other.RefChain[0])
		{
			StartOffset = i;
			break;
		}
	}

	if (StartOffset < 0 || RefChain.Num() - StartOffset < Other.RefChain.Num()) { return false; }

	for (int32 i=0; i < Other.RefChain.Num(); ++i)
	{
		if (!(Other.RefChain[i] == RefChain[i+StartOffset]) ) { return false; }
	}
	return true;
}

#if !(UE_BUILD_TEST || UE_BUILD_SHIPPING)
int32 FReferenceChainSearch::FFindReferencerCollector::FindReferencedObjectIndex(const UObject& ReferencedBy, const UObject& ReferencedObject)
{
	int32 Result = INDEX_NONE;
	auto & TokenMap = ReferencedBy.GetClass()->DebugTokenMap;

	for (int32 Index = 0, End = TokenMap.GetTokenMapSize(); Index < End; ++Index)
	{
		auto TokenName = TokenMap.GetTokenInfo(Index).Name;
		if (ReferencedObject.GetFName() == TokenName)
		{
			Result = Index;
			break;
		}
	}

	return Result;
}
#endif // !(UE_BUILD_TEST || UE_BUILD_SHIPPING)
void FReferenceChainSearch::FFindReferencerCollector::HandleObjectReference(UObject*& InObject, const UObject* RefObject, const UProperty* ReferencingProperty)
{
	UObject* RefSrc = RefObject != NULL ? const_cast<UObject*>(RefObject) : ReferencingObject;
	int32 ReferencedObjectIndex = INDEX_NONE;
#if !(UE_BUILD_TEST || UE_BUILD_SHIPPING)
	if (RefSrc != nullptr && InObject != nullptr)
	{
		ReferencedObjectIndex = FindReferencedObjectIndex(*RefSrc, *InObject);
	}
#endif // !(UE_BUILD_TEST || UE_BUILD_SHIPPING)
	if (ReferencingProperty != NULL)
	{
		FReferenceChainLink RefInfo(ReferencedObjectIndex, EReferenceType::Property, RefSrc, (void*)ReferencingProperty, InObject);
		References.Push(RefInfo);
	}
	else
	{
		FReferenceChainLink RefInfo(ReferencedObjectIndex, RefType, RefSrc, AROFuncPtr, InObject);
		References.Push(RefInfo);
	}
}


void FReferenceChainSearch::PrintReferencers(FReferenceChain& Referencer)
{
	UE_LOG(LogReferenceChain, Log, TEXT("  "));

	UObject* LastReferencedBy = NULL;
	int32 RefLevel = -1;

	for (int32 i=0; i < Referencer.RefChain.Num(); ++i)
	{
		FReferenceChainLink& RefInfo = Referencer.RefChain[i];

		if (RefInfo.ReferencedBy != LastReferencedBy)
		{
			LastReferencedBy = RefInfo.ReferencedBy;
			++RefLevel;
		}

		FString ReferencedThrough = Internal::RefereceTypeToString(RefInfo.ReferenceType);

#if !(UE_BUILD_TEST || UE_BUILD_SHIPPING)
		if (RefInfo.ReferencedThrough != NULL)
		{
			if ( RefInfo.IsProperty() )
			{
				UProperty* Prop = (UProperty*)RefInfo.ReferencedThrough;
				ReferencedThrough = Prop->GetName();
			}
			else
			{
				ANSICHAR Str[4096] = { 0 };
				FPlatformStackWalk::ProgramCounterToHumanReadableString( -1 /* means invalid or unknown */, (uint64)RefInfo.ReferencedThrough, Str, 4096, 0);

				ReferencedThrough = ANSI_TO_TCHAR(Str);
			}
		}
		else if (RefInfo.ReferencedObjectIndex != INDEX_NONE)
		{
			ReferencedThrough = RefInfo.GetReferencedByName();
		}
#endif
		FString ObjectReachability;
		if( RefInfo.ReferencedBy->IsRooted() )
		{
			ObjectReachability += TEXT("(root) ");
		}
		
		CA_SUPPRESS(6011)
		if( RefInfo.ReferencedBy->IsNative() )
		{
			ObjectReachability += TEXT("(native) ");
		}
		
		if( RefInfo.ReferencedBy->IsPendingKill() )
		{
			ObjectReachability += TEXT("(PendingKill) ");
		}

		if( RefInfo.ReferencedBy->HasAnyFlags(RF_Standalone) )
		{
			ObjectReachability += TEXT("(standalone) ");
		}

		if (RefInfo.ReferencedBy->HasAnyInternalFlags(EInternalObjectFlags::Async))
		{
			ObjectReachability += TEXT("(async) ");
		}

		if (RefInfo.ReferencedBy->HasAnyInternalFlags(EInternalObjectFlags::AsyncLoading))
		{
			ObjectReachability += TEXT("(asyncloading) ");
		}

		if (GUObjectArray.IsDisregardForGC(RefInfo.ReferencedBy))
		{
			ObjectReachability += TEXT("(NeverGCed) ");
		}

		FUObjectItem* ReferencedByObjectItem = GUObjectArray.ObjectToObjectItem(RefInfo.ReferencedBy);
		bool bClusterRoot = false;
		if (ReferencedByObjectItem->HasAnyFlags(EInternalObjectFlags::ClusterRoot))
		{
			ObjectReachability += TEXT("(ClusterRoot) ");
			bClusterRoot = true;
		}
		if (ReferencedByObjectItem->GetOwnerIndex() > 0)
		{
			ObjectReachability += TEXT("(Clustered) ");
		}

		FString Indent; Indent = Indent.LeftPad(RefLevel*2);
		UE_LOG(LogReferenceChain, Log, TEXT("%s%s%s->%s"), *Indent, *ObjectReachability, *RefInfo.ReferencedBy->GetFullName(), *ReferencedThrough);

		if (i == Referencer.RefChain.Num()-1)
		{
			Indent += TEXT("  ");
			UE_LOG(LogReferenceChain, Log, TEXT("%s(target) %s"), *Indent, *RefInfo.ReferencedObj->GetFullName());
		}
	}
}

// Internal graph node used to build the internal reference graph represantation
struct FRefGraphItem
{
	FReferenceChainSearch::FReferenceChainLink Link;
	TArray<FRefGraphItem*> Parents;
	TArray<FRefGraphItem*> Children;
};

// searched the list of graph nodes for a node that covers the same reference
static FRefGraphItem* FindNode(TMultiMap<UObject*, FRefGraphItem*>& InputGraphNodeList, UObject* ReferencedBy, UObject* ReferencedObj)
{
	for (auto It = InputGraphNodeList.CreateConstKeyIterator(ReferencedBy); It; ++It)
	{
		if (It.Value()->Link.ReferencedObj == ReferencedObj )
		{
			return It.Value();
		}
	}
	return NULL;
}

// Internal helper function to find all graph nodes that reference the specified object
static int32 FindReferencedGraphNodes(TMultiMap<UObject*, FRefGraphItem*>& InputGraphNodeList, UObject* ReferencedObj, TArray<FRefGraphItem*>& FoundNodes)
{
	InputGraphNodeList.MultiFind(ReferencedObj, FoundNodes);
	return FoundNodes.Num();
}

// Creates child/parent relationship between the nodes
static void LinkToParents(TMultiMap<UObject*, FRefGraphItem*>& InputGraphNodeList, FRefGraphItem* NodeToLink)
{
	for (auto It = InputGraphNodeList.CreateConstIterator(); It; ++It)
	{
		if (It.Value()->Link.ReferencedObj == NodeToLink->Link.ReferencedBy)
		{
			It.Value()->Children.Push(NodeToLink);
			NodeToLink->Parents.Push(It.Value());
		}
	}
}

// Returns true if the object can't be collected by GC
static FORCEINLINE bool IsNonGCObject(UObject* Object)
{
	FUObjectItem* ObjectItem = GUObjectArray.ObjectToObjectItem(Object);
	return (ObjectItem->IsRootSet() ||
		ObjectItem->HasAnyFlags(EInternalObjectFlags::GarbageCollectionKeepFlags) ||
		(GARBAGE_COLLECTION_KEEPFLAGS != RF_NoFlags && Object->HasAnyFlags(GARBAGE_COLLECTION_KEEPFLAGS)));
}

void FReferenceChainSearch::CreateReferenceChain(FRefGraphItem* Node, FReferenceChainSearch::FReferenceChain& ThisChain, TArray<FReferenceChainSearch::FReferenceChain>& ChainArray, UObject* InObjectToFind, int32 Levels)
{
	ThisChain.RefChain.Push(Node->Link);
	if (Levels <= 0)
	{
		UE_CLOG(ShouldOutputToLog(), LogReferenceChain, Log, TEXT("Chain is too long!"));
		for (int32 i=0; i < ThisChain.RefChain.Num(); ++i)
		{
			UE_CLOG(ShouldOutputToLog(), LogReferenceChain, Log, TEXT("%s -> %s"), *ThisChain.RefChain[i].ReferencedBy->GetPathName(), *ThisChain.RefChain[i].ReferencedObj->GetPathName());
		}
	}
	check(Levels > 0);
	Levels--;

	// If we encounter the target object or another root-object, we stop here
	if (Node->Link.ReferencedObj == InObjectToFind)
	{
		ChainArray.Push(ThisChain);
		return;
	}

	if (IsNonGCObject(Node->Link.ReferencedObj))
	{
		return;
	}

	for (int32 i=0; i < Node->Children.Num(); ++i)
	{
		// Skip this child if already in chain
		bool bSkip = false;
		for (int32 j=0; j < ThisChain.RefChain.Num(); ++j)
		{
			if (ThisChain.RefChain[j].ReferencedObj == Node->Children[i]->Link.ReferencedObj)
			{
				bSkip = true;
				break;
			}
		}
		
		if (bSkip) { continue; };

		FReferenceChainSearch::FReferenceChain ChildChain = ThisChain;
		CreateReferenceChain(Node->Children[i], ChildChain, ChainArray, InObjectToFind, Levels);
	}
}

void FReferenceChainSearch::BuildRefGraph()
{
	UE_CLOG(ShouldOutputToLog(), LogReferenceChain, Log, TEXT("Generating reference graph ..."));

	bool bContinue = true;

	TMultiMap<UObject*, FRefGraphItem*> GraphNodes;

	// Create the first graph-nodes referencing the target object
	for (FRawObjectIterator It;It;++It)
	{
		FUObjectItem* ObjItem = *It;
		checkSlow(ObjItem);
		UObject* Object = static_cast<UObject*>(ObjItem->Object);

		TArray<FReferenceChainLink>& RefList = ReferenceMap.FindChecked(Object);

		for (int32 i=0; i < RefList.Num(); ++i)
		{
			if (RefList[i].ReferencedObj == ObjectToFind)
			{
				FRefGraphItem* Node = new FRefGraphItem();
				Node->Link = RefList[i];
				GraphNodes.Add(Node->Link.ReferencedBy, Node);

				RefList[i].ReferenceType = EReferenceType::Invalid;
			}
		}
	}

	int32 Level = 0;
	UE_CLOG(ShouldOutputToLog(), LogReferenceChain, Log, TEXT("Level 0 has %d nodes ..."), GraphNodes.Num());

	while(bContinue)
	{
		int32 AddedNodes = 0;
		TArray<FRefGraphItem*> NewGraphNodes;

		for (FRawObjectIterator It;It;++It)
		{
			FUObjectItem* ObjItem = *It;
			checkSlow(ObjItem->Object);
			UObject* Object = (UObject*)ObjItem->Object;
			TArray<FReferenceChainLink>& RefList = ReferenceMap.FindChecked(Object);

			for (int32 i=0; i < RefList.Num(); ++i)
			{
				if (RefList[i].ReferenceType == EReferenceType::Invalid ||
					IsNonGCObject(RefList[i].ReferencedObj)) // references to rooted objects are not important
				{ 
					continue; 
				}

				TArray<FRefGraphItem*> RefNodes;

				if (FindReferencedGraphNodes(GraphNodes, RefList[i].ReferencedObj, RefNodes) > 0)
				{
					FRefGraphItem* Node = FindNode(GraphNodes, RefList[i].ReferencedBy, RefList[i].ReferencedObj);
					if (Node == NULL)
					{
						Node = new FRefGraphItem();
						Node->Link = RefList[i];

						NewGraphNodes.Push(Node);
					}
				
					for (int32 j=0; j < RefNodes.Num(); ++j)
					{
						Node->Children.Push(RefNodes[j]);
						RefNodes[j]->Parents.Push(Node);
					}

					++AddedNodes;
					
					RefList[i].ReferenceType = EReferenceType::Invalid;
				}
			}
		}
		Level++;
		UE_CLOG(ShouldOutputToLog(), LogReferenceChain, Log, TEXT("Level %d added %d nodes ..."), Level, NewGraphNodes.Num());

		for (int32 i = 0; i < NewGraphNodes.Num(); ++i)
		{
			GraphNodes.Add(NewGraphNodes[i]->Link.ReferencedBy, NewGraphNodes[i]);
		}


		NewGraphNodes.Empty(NewGraphNodes.Num());

		bContinue = AddedNodes > 0;
	}

	TArray<FReferenceChain> Chains;

	UE_CLOG(ShouldOutputToLog(), LogReferenceChain, Log, TEXT("Generating reference chains ..."));
	for (auto It = GraphNodes.CreateConstIterator(); It; ++It)
	{
		FRefGraphItem* Node = It.Value();

		if (IsNonGCObject(Node->Link.ReferencedBy))
		{
			FReferenceChain CurChain;
			CreateReferenceChain(Node, CurChain, Chains, ObjectToFind, Level);
		}
	}

	for (int32 i=0; i < Chains.Num(); ++i)
	{
		InsertReferenceChain(Chains[i]);
	}
}

void FReferenceChainSearch::PerformSearch()
{
	UE_CLOG(ShouldOutputToLog(), LogReferenceChain, Log, TEXT("Searching referencers for %s. This may take several minutes."), *ObjectToFind->GetName());
	
	for (FRawObjectIterator It;It;++It)
	{
		FUObjectItem* CurrentObject = *It;
		UObject* Object = static_cast<UObject*>(CurrentObject->Object);
		ProcessObject(Object);
	}

	BuildRefGraph();
}

/** Helper struct for stack based approach */
struct FStackEntry
{
	/** Current data pointer, incremented by stride */
	uint8* Data;
	/** Current stride */
	int32 Stride;
	/** Current loop count, decremented each iteration */
	int32 Count;
	/** First token index in loop */
	int32 LoopStartIndex;
};

// Local helper function to add a reference chain to the temporary reference chain list
void AddToReferenceList(TArray<FReferenceChainSearch::FReferenceChainLink>& ReferenceList, const FReferenceChainSearch::FReferenceChainLink& RefToAdd)
{
	if (RefToAdd.ReferencedObj == NULL || RefToAdd.ReferencedBy == RefToAdd.ReferencedObj)
	{
		return;
	}

	bool bAdded = false;

	for (FReferenceChainSearch::FReferenceChainLink& Link : ReferenceList)
	{
		if (Link.ReferencedObj == RefToAdd.ReferencedObj)
		{
			bAdded = true;
			if (RefToAdd.IsProperty() && RefToAdd.ReferencedThrough != NULL)
			{
				Link = RefToAdd;
			}
			break;
		}
	}

	if (!bAdded)
	{
		ReferenceList.Add(RefToAdd);
	}
}

void FReferenceChainSearch::ProcessObject( UObject* CurrentObject )
{
	UClass* ObjectClass = CurrentObject->GetClass();

	// Make sure that token stream has been assembled at this point as the below code relies on it.
	if ( !ObjectClass->HasAnyClassFlags(CLASS_TokenStreamAssembled) )
	{
		ObjectClass->AssembleReferenceTokenStream();
		check(ObjectClass->HasAnyClassFlags(CLASS_TokenStreamAssembled));
	}

	// Get pointer to token stream and jump to the start.
	FGCReferenceTokenStream* RESTRICT TokenStream = &ObjectClass->ReferenceTokenStream;
	uint32 TokenStreamIndex			= 0;
	// Keep track of index to reference info. Used to avoid LHSs.
	uint32 ReferenceTokenStreamIndex	= 0;

	TArray<FStackEntry> Stack;
	Stack.AddUninitialized( 128 );

	// Create stack entry and initialize sane values.
	FStackEntry* RESTRICT StackEntry = Stack.GetData();
	uint8* StackEntryData		= (uint8*) CurrentObject;
	StackEntry->Data			= StackEntryData;
	StackEntry->Stride			= 0;
	StackEntry->Count			= -1;
	StackEntry->LoopStartIndex	= -1;

	// Keep track of token return count in separate integer as arrays need to fiddle with it.
	int32 TokenReturnCount = 0;

	UProperty* InArrayProp = NULL;

	TArray<FReferenceChainLink>& ReferenceList = ReferenceMap.Emplace(CurrentObject);

	// Parse the token stream.
	while( true )
	{
		// Cache current token index as it is the one pointing to the reference info.
		ReferenceTokenStreamIndex = TokenStreamIndex;

		// Handle returning from an array of structs, array of structs of arrays of ... (yadda yadda)
		for( int32 ReturnCount=0; ReturnCount<TokenReturnCount; ReturnCount++ )
		{
			// Make sure there's no stack underflow.
			check( StackEntry->Count != -1 );

			// We pre-decrement as we're already through the loop once at this point.
			if( --StackEntry->Count > 0 )
			{
				// Point data to next entry.
				StackEntryData	 = StackEntry->Data + StackEntry->Stride;
				StackEntry->Data = StackEntryData;

				// Jump back to the beginning of the loop.
				TokenStreamIndex = StackEntry->LoopStartIndex;
				ReferenceTokenStreamIndex = StackEntry->LoopStartIndex;
				// We're not done with this token loop so we need to early out instead of backing out further.
				break;
			}
			else
			{
				StackEntry--;
				StackEntryData = StackEntry->Data;

				InArrayProp = NULL;
			}
		}

		// Instead of reading information about reference from stream and caching it like below we access
		// the same memory address over and over and over again to avoid a nasty LHS penalty. Not reading 
		// the reference info means we need to manually increment the token index to skip to the next one.
		TokenStreamIndex++;
		// Helper to make code more readable and hide the ugliness that is avoiding LHSs from caching.
		FGCReferenceInfo ReferenceInfo = TokenStream->AccessReferenceInfo( ReferenceTokenStreamIndex );

		if( ReferenceInfo.Type == GCRT_EndOfStream )
		{
			check(StackEntry == Stack.GetData());
			return;
		}

		if (ReferenceInfo.Type == GCRT_EndOfPointer)
		{
			TokenReturnCount = ReferenceInfo.ReturnCount;
			continue;
		}

		uint32 Offset = ReferenceInfo.Offset;
		
		void* StackEntryPtr = StackEntryData + Offset;
		
		// Get the property from token stream
		UProperty* Prop = Internal::FindPropertyForOffset(CurrentObject->GetClass(), Offset);

		if (InArrayProp != NULL && Prop == NULL)
		{
			Prop = InArrayProp;
		}

		switch (ReferenceInfo.Type)
		{
			case GCRT_Object:
			case GCRT_PersistentObject:
			{
				// We're dealing with an object reference.
				UObject* Object = *(UObject**)StackEntryPtr;
				TokenReturnCount = ReferenceInfo.ReturnCount;

				FReferenceChainLink TopRef(ReferenceTokenStreamIndex, InArrayProp != NULL ? EReferenceType::ArrayProperty : EReferenceType::Property, CurrentObject, Prop, Object);
				AddToReferenceList(ReferenceList, TopRef);
			}
			break;

			case GCRT_ArrayObject:
			{
				// We're dealing with an array of object references.
				TArray<UObject*>& ObjectArray = *(TArray<UObject*>*)StackEntryPtr;
				TokenReturnCount = ReferenceInfo.ReturnCount;

				for( int32 ObjectIndex=0; ObjectIndex<ObjectArray.Num(); ObjectIndex++ )
				{
					UObject*& Object = ObjectArray[ObjectIndex];
				
					FReferenceChainLink TopRef(ReferenceTokenStreamIndex, EReferenceType::ArrayProperty, CurrentObject, Prop, Object, ObjectIndex);
					AddToReferenceList(ReferenceList, TopRef);
				}
			}
			break;

			case GCRT_ArrayStruct:
			{
				InArrayProp = Prop;

				// We're dealing with a dynamic array of structs.
				const FScriptArray& Array = *(FScriptArray*)StackEntryPtr;
				StackEntry++;
				StackEntryData				= (uint8*) Array.GetData();
				StackEntry->Data			= StackEntryData;
				StackEntry->Stride			= TokenStream->ReadStride( TokenStreamIndex );
				StackEntry->Count			= Array.Num();

				const FGCSkipInfo SkipInfo	= TokenStream->ReadSkipInfo( TokenStreamIndex );
				StackEntry->LoopStartIndex	= TokenStreamIndex;

				if( StackEntry->Count == 0 )
				{
					// Skip empty array by jumping to skip index and set return count to the one about to be read in.
					TokenStreamIndex		= SkipInfo.SkipIndex;
					TokenReturnCount		= TokenStream->GetSkipReturnCount( SkipInfo );
				}
				else
				{	
					// Loop again.
					check( StackEntry->Data );
					TokenReturnCount		= 0;
				}
			}
			break;

			case GCRT_FixedArray:
			{
				InArrayProp = Prop;
			
				// We're dealing with a fixed size array
				uint8* PreviousData	= StackEntryData;
				StackEntry++;
				StackEntryData				= PreviousData;
				StackEntry->Data			= PreviousData;
				StackEntry->Stride			= TokenStream->ReadStride( TokenStreamIndex );
				StackEntry->Count			= TokenStream->ReadCount( TokenStreamIndex );
				StackEntry->LoopStartIndex	= TokenStreamIndex;
				TokenReturnCount			= 0;
			}
			break;

			case GCRT_AddStructReferencedObjects:
			{
				// We're dealing with a function call
				TokenReturnCount		= ReferenceInfo.ReturnCount;
				UScriptStruct::ICppStructOps::TPointerToAddStructReferencedObjects Func = (UScriptStruct::ICppStructOps::TPointerToAddStructReferencedObjects) TokenStream->ReadPointer( TokenStreamIndex );

				FFindReferencerCollector ReferenceCollector(this, EReferenceType::StructARO, (void*)Func, CurrentObject);
				Func(StackEntryPtr, ReferenceCollector);

				for (int32 i=0; i < ReferenceCollector.References.Num(); ++i)
				{
					AddToReferenceList(ReferenceList, ReferenceCollector.References[i]);
				}
			}
			break;

			case GCRT_AddReferencedObjects:
			{
				// Static AddReferencedObjects function call.
				void (*AddReferencedObjects)(UObject*, FReferenceCollector&) = (void(*)(UObject*, FReferenceCollector&))TokenStream->ReadPointer( TokenStreamIndex );
				TokenReturnCount = ReferenceInfo.ReturnCount;

				FFindReferencerCollector ReferenceCollector(this, EReferenceType::ARO, (void*)AddReferencedObjects, CurrentObject);
				AddReferencedObjects(CurrentObject, ReferenceCollector);

				for (int32 i=0; i < ReferenceCollector.References.Num(); ++i)
				{
					AddToReferenceList(ReferenceList, ReferenceCollector.References[i]);
				}
			}
			break;

			case GCRT_AddTMapReferencedObjects:
			{
				UMapProperty* MapProperty = (UMapProperty*)TokenStream->ReadPointer( TokenStreamIndex );
				TokenReturnCount = ReferenceInfo.ReturnCount;
				FFindReferencerCollector ReferenceCollector(this, EReferenceType::MapProperty, (void*)MapProperty, CurrentObject);
				MapProperty->SerializeItem(ReferenceCollector.GetVerySlowReferenceCollectorArchive(), StackEntryPtr, nullptr);

				for (const FReferenceChainLink& Ref : ReferenceCollector.References)
				{
					AddToReferenceList(ReferenceList, Ref);
				}
			}
			break;

			case GCRT_AddTSetReferencedObjects:
			{
				USetProperty* SetProperty = (USetProperty*)TokenStream->ReadPointer( TokenStreamIndex );
				TokenReturnCount = ReferenceInfo.ReturnCount;
				FFindReferencerCollector ReferenceCollector(this, EReferenceType::SetProperty, (void*)SetProperty, CurrentObject);
				SetProperty->SerializeItem(ReferenceCollector.GetVerySlowReferenceCollectorArchive(), StackEntryPtr, nullptr);

				for (const FReferenceChainLink& Ref : ReferenceCollector.References)
				{
					AddToReferenceList(ReferenceList, Ref);
				}
			}
			break;

			default:
			{
				UE_CLOG(ShouldOutputToLog(), LogReferenceChain, Fatal,TEXT("Unknown token"));
			}
			break;
		}
	}
}

FReferenceChainSearch::FReferenceChainSearch( UObject* InObjectToFind, uint32 Mode ) 
	:ObjectToFind(InObjectToFind), SearchMode(Mode)
{
	if (ObjectToFind == NULL) 
	{ 
		return; 
	}

	PerformSearch();
	
	if (ShouldOutputToLog())
	{
		PrintResults();
	}
}

void FReferenceChainSearch::PrintResults()
{
	bool bIsFirst = true;

	for (int32 i = 0; i < Referencers.Num(); ++i)
	{
		UObject* Obj = Referencers[i].RefChain[0].ReferencedBy;

		if (!Obj->IsIn(ObjectToFind) && Obj != ObjectToFind)
		{
			if (bIsFirst)
			{
				UE_LOG(LogReferenceChain, Log, TEXT("  "));
				UE_LOG(LogReferenceChain, Log, TEXT("External Referencers:"));
				bIsFirst = false;
			}

			PrintReferencers(Referencers[i]);
		}
	}

	bIsFirst = true;

	for (int32 i = 0; i < Referencers.Num(); ++i)
	{
		UObject* Obj = Referencers[i].RefChain[0].ReferencedBy;

		CA_SUPPRESS(6011)
		if (Obj->IsIn(ObjectToFind) || Obj == ObjectToFind)
		{
			if (bIsFirst)
			{
				UE_LOG(LogReferenceChain, Log, TEXT("  "));
				UE_LOG(LogReferenceChain, Log, TEXT("Internal Referencers:"));
				bIsFirst = false;
			}

			PrintReferencers(Referencers[i]);
		}
	}
}

void FReferenceChainSearch::InsertReferenceChain( FReferenceChain& Referencer )
{
	UObject* RootRef = Referencer.RefChain[0].ReferencedBy;

	if ( !!(SearchMode&ESearchMode::ExternalOnly) && RootRef->IsIn(ObjectToFind))
	{
		return;
	}

	if ( !!(SearchMode&ESearchMode::Direct))
	{
		for (int32 i=Referencer.RefChain.Num()-1; i >= 0; --i)
		{
			if (Referencer.RefChain[i].ReferencedObj == ObjectToFind)
			{
				if ( i > 0 )
				{
					Referencer.RefChain.RemoveAt(0, i);
				}
				RootRef = Referencer.RefChain[0].ReferencedBy;
				break;
			}
		}
	}

	bool bInserted = false;

	for (int32 i=0; i < Referencers.Num(); ++i)
	{
		if (Referencers[i].RefChain.Num() > Referencer.RefChain.Num())
		{
			if (Referencers[i].Contains(Referencer))
			{
				bInserted = true;
				break;
			}
		}
		else if(Referencer.RefChain.Num() > Referencers[i].RefChain.Num())
		{
			if (Referencer.Contains(Referencers[i]))
			{
				Referencers[i] = Referencer;
				bInserted = true;
			}
		}
	}

	if (!bInserted)
	{
		Referencers.Push(Referencer);
	}

	if ( !!(SearchMode&(ESearchMode::Longest|ESearchMode::Shortest)) )
	{
		int32 Index = -1;
		int32 ChainLen = !!(SearchMode&ESearchMode::Longest) ? 0 : 999999;

		for (int32 i=0; i < Referencers.Num(); ++i)
		{
			FReferenceChain& RefChain = Referencers[i];

			if (RefChain.RefChain[0].ReferencedBy == RootRef)
			{
				const int32 Len = RefChain.RefChain.Num();

				if ( (!!(SearchMode&ESearchMode::Longest) && Len > ChainLen) ||
					 (!!(SearchMode&ESearchMode::Shortest) && Len < ChainLen) )
				{
					ChainLen = Len;
					Index = i;
				}
			}
		}

		for (int32 i=0; i < Referencers.Num(); ++i)
		{
			if (i == Index) { continue; }
			
			FReferenceChain& RefChain = Referencers[i];

			if (RefChain.RefChain[0].ReferencedBy == RootRef)
			{
				Referencers.RemoveAt(i--);
			}
		}
	}
}

FString FReferenceChainSearch::FReferenceChainLink::ToString() const
{
	FString ReferencedThroughStr = Internal::RefereceTypeToString(ReferenceType);

	if (ReferencedThrough != NULL)
	{
		if ( IsProperty() )
		{
			UProperty* Prop = (UProperty*)ReferencedThrough;
			ReferencedThroughStr = Prop->GetName();
		}
		else
		{
			ANSICHAR Str[4096] = { 0 };
			FPlatformStackWalk::ProgramCounterToHumanReadableString( -1 /* means invalid or unknown */, (uint64)ReferencedThrough, Str, 4096, 0);

			ReferencedThroughStr = ANSI_TO_TCHAR(Str);
		}
	}

	FString ObjectReachability;
	if( ReferencedBy->IsRooted() )
	{
		ObjectReachability += TEXT("(root) ");
	}
		
	if( ReferencedBy->IsNative() )
	{
		ObjectReachability += TEXT("(native) ");
	}

	if( ReferencedBy->IsPendingKill() )
	{
		ObjectReachability += TEXT("(PendingKill) ");
	}
		
	if( ReferencedBy->HasAnyFlags(RF_Standalone) )
	{
		ObjectReachability += TEXT("(standalone) ");
	}

	if (GUObjectArray.IsDisregardForGC(ReferencedBy))
	{
		ObjectReachability += TEXT("(NeverGCed) ");
	}

	return FString::Printf(TEXT("%s%s->%s >> %s"), *ObjectReachability, *ReferencedBy->GetFullName(), *ReferencedThroughStr, *ReferencedObj->GetFullName());
}
