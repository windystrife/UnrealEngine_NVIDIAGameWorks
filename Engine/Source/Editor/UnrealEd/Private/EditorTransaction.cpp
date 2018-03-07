// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "CoreMinimal.h"
#include "Misc/MemStack.h"
#include "UObject/Object.h"
#include "UObject/Package.h"
#include "Engine/Level.h"
#include "Components/ActorComponent.h"
#include "Model.h"
#include "Editor/Transactor.h"
#include "Editor/TransBuffer.h"
#include "Components/ModelComponent.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "BSPOps.h"

DEFINE_LOG_CATEGORY_STATIC(LogEditorTransaction, Log, All);

inline UObject* BuildSubobjectKey(UObject* InObj, TArray<FName>& OutHierarchyNames)
{
	auto UseOuter = [](const UObject* Obj)
	{
		if (Obj == nullptr)
		{
			return false;
		}

		const bool bIsCDO = Obj->HasAllFlags(RF_ClassDefaultObject);
		const UObject* CDO = bIsCDO ? Obj : nullptr;
		const bool bIsClassCDO = (CDO != nullptr) ? (CDO->GetClass()->ClassDefaultObject == CDO) : false;
		if(!bIsClassCDO && CDO)
		{
			// Likely a trashed CDO, try to recover. Only known cause of this is
			// ambiguous use of DSOs:
			CDO = CDO->GetClass()->ClassDefaultObject;
		}
		const UActorComponent* AsComponent = Cast<UActorComponent>(Obj);
		const bool bIsDSO = Obj->HasAnyFlags(RF_DefaultSubObject);
		const bool bIsSCSComponent = AsComponent && AsComponent->IsCreatedByConstructionScript();
		return (bIsCDO && bIsClassCDO) || bIsDSO || bIsSCSComponent;
	};
	
	UObject* Outermost = nullptr;

	UObject* Iter = InObj;
	while (UseOuter(Iter))
	{
		OutHierarchyNames.Add(Iter->GetFName());
		Iter = Iter->GetOuter();
		Outermost = Iter;
	}

	return Outermost;
}

/*-----------------------------------------------------------------------------
	A single transaction.
-----------------------------------------------------------------------------*/

FTransaction::FObjectRecord::FObjectRecord(FTransaction* Owner, UObject* InObject, FScriptArray* InArray, int32 InIndex, int32 InCount, int32 InOper, int32 InElementSize, STRUCT_DC InDefaultConstructor, STRUCT_AR InSerializer, STRUCT_DTOR InDestructor)
	:	Object				( InObject )
	,	Array				( InArray )
	,	Index				( InIndex )
	,	Count				( InCount )
	,	Oper				( InOper )
	,	ElementSize			( InElementSize )
	,	DefaultConstructor	( InDefaultConstructor )
	,	Serializer			( InSerializer )
	,	Destructor			( InDestructor )
	,	bRestored			( false )
	,	bWantsBinarySerialization ( true )
{
	// Blueprint compile-in-place can alter class layout so use tagged serialization for objects relying on a UBlueprint's Class
	if (UBlueprintGeneratedClass* Class = Cast<UBlueprintGeneratedClass>(InObject->GetClass()))
	{
		bWantsBinarySerialization = false; 
	}
	ObjectAnnotation = Object->GetTransactionAnnotation();
	FWriter Writer( Data, ReferencedObjects, ReferencedNames, bWantsBinarySerialization );
	SerializeContents( Writer, Oper );
}

void FTransaction::FObjectRecord::SerializeContents( FArchive& Ar, int32 InOper )
{
	// Cache to restore at the end
	bool bWasArIgnoreOuterRef = Ar.ArIgnoreOuterRef;

	if (Object.SubObjectHierarchyID.Num() != 0)
	{
		Ar.ArIgnoreOuterRef = true;
	}

	if( Array )
	{
		//UE_LOG( LogEditorTransaction, Log, TEXT("Array %s %i*%i: %i"), Object ? *Object->GetFullName() : TEXT("Invalid Object"), Index, ElementSize, InOper);

		check((SIZE_T)Array >= (SIZE_T)Object.Get() + sizeof(UObject));
		check((SIZE_T)Array + sizeof(FScriptArray) <= (SIZE_T)Object.Get() + Object->GetClass()->GetPropertiesSize());
		check(ElementSize!=0);
		check(DefaultConstructor!=NULL);
		check(Serializer!=NULL);
		check(Index>=0);
		check(Count>=0);
		if( InOper==1 )
		{
			// "Saving add order" or "Undoing add order" or "Redoing remove order".
			if( Ar.IsLoading() )
			{
				checkSlow(Index+Count<=Array->Num());
				for( int32 i=Index; i<Index+Count; i++ )
				{
					Destructor( (uint8*)Array->GetData() + i*ElementSize );
				}
				Array->Remove( Index, Count, ElementSize );
			}
		}
		else
		{
			// "Undo/Redo Modify" or "Saving remove order" or "Undoing remove order" or "Redoing add order".
			if( InOper==-1 && Ar.IsLoading() )
			{
				Array->InsertZeroed( Index, Count, ElementSize );
				for( int32 i=Index; i<Index+Count; i++ )
				{
					DefaultConstructor( (uint8*)Array->GetData() + i*ElementSize );
				}
			}

			// Serialize changed items.
			check(Index+Count<=Array->Num());
			for( int32 i=Index; i<Index+Count; i++ )
			{
				Serializer( Ar, (uint8*)Array->GetData() + i*ElementSize );
			}
		}
	}
	else
	{
		//UE_LOG(LogEditorTransaction, Log,  TEXT("Object %s"), *Object->GetFullName());
		check(Index==0);
		check(ElementSize==0);
		check(DefaultConstructor==NULL);
		check(Serializer==NULL);
		// Once UE-46691 this should probably become an ensure
		if (UObject* Obj = Object.Get())
		{
			Obj->Serialize(Ar);
		}
	}
	Ar.ArIgnoreOuterRef = bWasArIgnoreOuterRef;
}

void FTransaction::FObjectRecord::Restore( FTransaction* Owner )
{
	// only used by FMatineeTransaction:
	if( !bRestored )
	{
		bRestored = true;
		check(!Owner->bFlip);
		FTransaction::FObjectRecord::FReader Reader( Owner, Data, ReferencedObjects, ReferencedNames, bWantsBinarySerialization );
		SerializeContents( Reader, Oper );
	}
}

void FTransaction::FObjectRecord::Save(FTransaction* Owner)
{
	// common undo/redo path, before applying undo/redo buffer we save current state:
	check(Owner->bFlip);
	if (!bRestored)
	{
		FlipData.Empty();
		FlipReferencedObjects.Empty();
		FlipReferencedNames.Empty();
		FlipObjectAnnotation = TSharedPtr<ITransactionObjectAnnotation>();
		// Once UE-46691 this should probably become an ensure
		if (UObject* Obj = Object.Get())
		{
			FlipObjectAnnotation = Obj->GetTransactionAnnotation();
		}
		FWriter Writer(FlipData, FlipReferencedObjects, FlipReferencedNames, bWantsBinarySerialization);
		SerializeContents(Writer, -Oper);
	}
}

void FTransaction::FObjectRecord::Load(FTransaction* Owner)
{
	// common undo/redo path, we apply the saved state and then swap it for the state we cached in ::Save above
	check(Owner->bFlip);
	if (!bRestored)
	{
		bRestored = true;
		FTransaction::FObjectRecord::FReader Reader(Owner, Data, ReferencedObjects, ReferencedNames, bWantsBinarySerialization);
		SerializeContents(Reader, Oper);
		Exchange(ObjectAnnotation, FlipObjectAnnotation);
		Exchange(Data, FlipData);
		Exchange(ReferencedObjects, FlipReferencedObjects);
		Exchange(ReferencedNames, FlipReferencedNames);
		Oper *= -1;
	}
}

int32 FTransaction::GetRecordCount() const
{
	return Records.Num();
}

bool FTransaction::ContainsPieObject() const
{
	for( const FObjectRecord& Record : Records )
	{
		if( Record.ContainsPieObject() )
		{
			return true;
		}
	}

	return false;
}

bool FTransaction::IsObjectTransacting(const UObject* Object) const
{
	// This function is meaningless when called outside of a transaction context. Without this
	// ensure clients will commonly introduced bugs by having some logic that runs during
	// the transacting and some logic that does not, yielding assymetrical results.
	ensure(GIsTransacting);
	ensure(ChangedObjects.Num() != 0);
	return ChangedObjects.Contains(Object);
}

void FTransaction::RemoveRecords( int32 Count /* = 1  */ )
{
	if ( Count > 0 && Records.Num() >= Count )
	{
		// Remove anything from the ObjectMap which is about to be removed from the Records array
		for (int32 Index = 0; Index < Count; Index++)
		{
			ObjectMap.Remove( Records[Records.Num() - Count + Index].Object.Get() );
		}

		Records.RemoveAt( Records.Num() - Count, Count );
	}
}

/**
 * Outputs the contents of the ObjectMap to the specified output device.
 */
void FTransaction::DumpObjectMap(FOutputDevice& Ar) const
{
	Ar.Logf( TEXT("===== DumpObjectMap %s ==== "), *Title.ToString() );
	for ( ObjectMapType::TConstIterator It(ObjectMap) ; It ; ++It )
	{
		const UObject* CurrentObject	= It.Key();
		const int32 SaveCount				= It.Value();
		Ar.Logf( TEXT("%i\t: %s"), SaveCount, *CurrentObject->GetPathName() );
	}
	Ar.Logf( TEXT("=== EndDumpObjectMap %s === "), *Title.ToString() );
}

FArchive& operator<<( FArchive& Ar, FTransaction::FObjectRecord& R )
{
	FMemMark Mark(FMemStack::Get());
	Ar << R.Object;
	Ar << R.Data;
	Ar << R.ReferencedObjects;
	Ar << R.ReferencedNames;
	Mark.Pop();
	return Ar;
}

FTransaction::FObjectRecord::FPersistentObjectRef::FPersistentObjectRef(UObject* InObject)
	: ReferenceType(EReferenceType::Unknown)
	, Object(nullptr)
{
	check(InObject);
	UObject* Outermost = BuildSubobjectKey(InObject, SubObjectHierarchyID);

	if (SubObjectHierarchyID.Num()>0)
	{
		check(Outermost);
		//check(Outermost != GetTransientPackage());
		ReferenceType = EReferenceType::SubObject;
		Object = Outermost;
	}
	else
	{
		SubObjectHierarchyID.Empty();
		ReferenceType = EReferenceType::RootObject;
		Object = InObject;
	}

	// Make sure that when we look up the object we find the same thing:
	checkSlow(Get() == InObject);
}

UObject* FTransaction::FObjectRecord::FPersistentObjectRef::Get() const
{
	if (ReferenceType == EReferenceType::SubObject)
	{
		check (SubObjectHierarchyID.Num() > 0)
		// find the subobject:
		UObject* CurrentObject = Object;
		bool bFoundTargetSubObject = (SubObjectHierarchyID.Num() == 0);
		if (!bFoundTargetSubObject)
		{
			// Current increasing depth into sub-objects, starts at 1 to avoid the sub-object found and placed in NextObject.
			int SubObjectDepth = SubObjectHierarchyID.Num() - 1;
			UObject* NextObject = CurrentObject;
			while (NextObject != nullptr && !bFoundTargetSubObject)
			{
				// Look for any UObject with the CurrentObject's outer to find the next sub-object:
				NextObject = StaticFindObjectFast(UObject::StaticClass(), CurrentObject, SubObjectHierarchyID[SubObjectDepth]);
				bFoundTargetSubObject = SubObjectDepth == 0;
				--SubObjectDepth;
				CurrentObject = NextObject;
			}
		}

		return bFoundTargetSubObject ? CurrentObject : nullptr;
	}

	return Object;
}

void FTransaction::FObjectRecord::AddReferencedObjects( FReferenceCollector& Collector )
{
	UObject* Obj = Object.Object;
	Collector.AddReferencedObject(Obj);
	Object.Object = Obj;

	for (FPersistentObjectRef& ReferencedObject : ReferencedObjects)
	{
		UObject* RefObj = ReferencedObject.Object;
		Collector.AddReferencedObject(RefObj);
		ReferencedObject.Object = RefObj;
	}

	if (ObjectAnnotation.IsValid())
	{
		ObjectAnnotation->AddReferencedObjects(Collector);
	}
}

bool FTransaction::FObjectRecord::ContainsPieObject() const
{
	{
		UObject* Obj = Object.Object;

		if(Obj && Obj->GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor))
		{
			return true;
		}
	}

	for (const FPersistentObjectRef& ReferencedObject : ReferencedObjects)
	{
		const UObject* Obj = ReferencedObject.Object;
		if( Obj && Obj->GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor))
		{
			return true;
		}
	}

	return false;
}

void FTransaction::AddReferencedObjects( FReferenceCollector& Collector )
{
	for( FObjectRecord& ObjectRecord : Records )
	{
		ObjectRecord.AddReferencedObjects( Collector );
	}
	Collector.AddReferencedObjects(ObjectMap);
}

// FTransactionBase interface.
void FTransaction::SaveObject( UObject* Object )
{
	check(Object);
	Object->CheckDefaultSubobjects();

	int32* SaveCount = ObjectMap.Find(Object);
	if ( !SaveCount )
	{
		ObjectMap.Add(Object,1);
		// Save the object.
		new( Records )FObjectRecord( this, Object, NULL, 0, 0, 0, 0, NULL, NULL, NULL );
	}
	else
	{
		++(*SaveCount);
	}
}

void FTransaction::SaveArray( UObject* Object, FScriptArray* Array, int32 Index, int32 Count, int32 Oper, int32 ElementSize, STRUCT_DC DefaultConstructor, STRUCT_AR Serializer, STRUCT_DTOR Destructor )
{
	check(Object);
	check(Array);
	check(ElementSize);
	check(DefaultConstructor);
	check(Serializer);
	check(Object->IsValidLowLevel());
	check((SIZE_T)Array>=(SIZE_T)Object);
	check((SIZE_T)Array+sizeof(FScriptArray)<=(SIZE_T)Object+Object->GetClass()->PropertiesSize);
	check(Index>=0);
	check(Count>=0);
	check(Index+Count<=Array->Num());

	// don't serialize the array if the object is contained within a PIE package
	if( Object->HasAnyFlags(RF_Transactional) && !Object->GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor))
	{
		// Save the array.
		new( Records )FObjectRecord( this, Object, Array, Index, Count, Oper, ElementSize, DefaultConstructor, Serializer, Destructor );
	}
}

void FTransaction::SetPrimaryObject(UObject* InObject)
{
	if (PrimaryObject == NULL)
	{
		PrimaryObject = InObject;
	}
}

/**
 * Enacts the transaction.
 */
void FTransaction::Apply()
{
	checkSlow(Inc==1||Inc==-1);

	// Figure out direction.
	const int32 Start = Inc==1 ? 0             : Records.Num()-1;
	const int32 End   = Inc==1 ? Records.Num() :              -1;

	// Init objects.
	for( int32 i=Start; i!=End; i+=Inc )
	{
		FObjectRecord& Record = Records[i];
		Record.bRestored = false;

		UObject* Object = Record.Object.Get();
		if (Object)
		{
			if (!ChangedObjects.Contains(Object))
			{
				Object->CheckDefaultSubobjects();
				Object->PreEditUndo();
			}

			ChangedObjects.Add(Object, Record.ObjectAnnotation);
		}
	}

	if (bFlip)
	{
		for (int32 i = Start; i != End; i += Inc)
		{
			Records[i].Save(this);
		}
		for (int32 i = Start; i != End; i += Inc)
		{
			Records[i].Load(this);
		}
	}
	else
	{
		for (int32 i = Start; i != End; i += Inc)
		{
			Records[i].Restore(this);
		}
	}

	// An Actor's components must always get its PostEditUndo before the owning Actor so do a quick sort
	ChangedObjects.KeySort([](UObject& A, UObject& B)
	{
		UActorComponent* BAsComponent = Cast<UActorComponent>(&B);
		return (BAsComponent ? (BAsComponent->GetOwner() != &A) : true);
	});

	TArray<ULevel*> LevelsToCommitModelSurface;
	NumModelsModified = 0;		// Count the number of UModels that were changed.
	for (auto ChangedObjectIt : ChangedObjects)
	{
		UObject* ChangedObject = ChangedObjectIt.Key;
		UModel* Model = Cast<UModel>(ChangedObject);
		if (Model && Model->Nodes.Num())
		{
			FBSPOps::bspBuildBounds(Model);
			++NumModelsModified;
		}
		
		if (UModelComponent* ModelComponent = Cast<UModelComponent>(ChangedObject))
		{
			ULevel* Level = ModelComponent->GetTypedOuter<ULevel>();
			check(Level);
			LevelsToCommitModelSurface.AddUnique(Level);
		}

		TSharedPtr<ITransactionObjectAnnotation> ChangedObjectTransactionAnnotation = ChangedObjectIt.Value;
		if (ChangedObjectTransactionAnnotation.IsValid())
		{
			ChangedObject->PostEditUndo(ChangedObjectTransactionAnnotation);
		}
		else
		{
			ChangedObject->PostEditUndo();
		}
	}

	// Commit model surfaces for unique levels within the transaction
	for (ULevel* Level : LevelsToCommitModelSurface)
	{
		Level->CommitModelSurfaces();
	}

	// Flip it.
	if (bFlip)
	{
		Inc *= -1;
	}
	for (auto ChangedObjectIt : ChangedObjects)
	{
		UObject* ChangedObject = ChangedObjectIt.Key;
		ChangedObject->CheckDefaultSubobjects();
	}

	ChangedObjects.Empty();
}

SIZE_T FTransaction::DataSize() const
{
	SIZE_T Result=0;
	for( int32 i=0; i<Records.Num(); i++ )
	{
		Result += Records[i].Data.Num();
	}
	return Result;
}

/**
 * Get all the objects that are part of this transaction.
 * @param	Objects		[out] Receives the object list.  Previous contents are cleared.
 */
void FTransaction::GetTransactionObjects(TArray<UObject*>& Objects) const
{
	Objects.Empty(); // Just in case.

	for(int32 i=0; i<Records.Num(); i++)
	{
		UObject* Obj = Records[i].Object.Get();
		if (Obj)
		{
			Objects.AddUnique(Obj);
		}
	}
}


/*-----------------------------------------------------------------------------
	Transaction tracking system.
-----------------------------------------------------------------------------*/
UTransactor::UTransactor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UTransBuffer::Initialize(SIZE_T InMaxMemory)
{
	MaxMemory = InMaxMemory;
	// Reset.
	Reset( NSLOCTEXT("UnrealEd", "Startup", "Startup") );
	CheckState();

	UE_LOG(LogInit, Log, TEXT("Transaction tracking system initialized") );
}

// UObject interface.
void UTransBuffer::Serialize( FArchive& Ar )
{
	check( !Ar.IsPersistent() );

	CheckState();

	Super::Serialize( Ar );

	if ( IsObjectSerializationEnabled() || !Ar.IsObjectReferenceCollector() )
	{
		Ar << UndoBuffer;
	}
	Ar << ResetReason << UndoCount << ActiveCount << ActiveRecordCounts;

	CheckState();
}

void UTransBuffer::FinishDestroy()
{
	if ( !HasAnyFlags(RF_ClassDefaultObject) )
	{
		CheckState();
		UE_LOG(LogExit, Log, TEXT("Transaction tracking system shut down") );
	}
	Super::FinishDestroy();
}

void UTransBuffer::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{	
	UTransBuffer* This = CastChecked<UTransBuffer>(InThis);
	This->CheckState();

	if ( This->IsObjectSerializationEnabled() )
	{
		// We cannot support undoing across GC if we allow it to eliminate references so we need
		// to suppress it.
		Collector.AllowEliminatingReferences(false);
		for (const TSharedRef<FTransaction>& SharedTrans : This->UndoBuffer)
		{
			SharedTrans->AddReferencedObjects( Collector );
		}
		for (const TSharedRef<FTransaction>& SharedTrans : This->RemovedTransactions)
		{
			SharedTrans->AddReferencedObjects(Collector);
		}
		Collector.AllowEliminatingReferences(true);
	}

	This->CheckState();

	Super::AddReferencedObjects( This, Collector );
}

int32 UTransBuffer::Begin( const TCHAR* SessionContext, const FText& Description )
{
	return BeginInternal<FTransaction>(SessionContext, Description);
}


int32 UTransBuffer::End()
{
	CheckState();
	const int32 Result = ActiveCount;
	// Don't assert as we now purge the buffer when resetting.
	// So, the active count could be 0, but the code path may still call end.
	if (ActiveCount >= 1)
	{
		if( --ActiveCount==0 )
		{
#if 0 // @todo DB: please don't remove this code -- thanks! :)
			// End the current transaction.
			if ( GUndo && GLog )
			{
				// @todo DB: Fix this potentially unsafe downcast.
				static_cast<FTransaction*>(GUndo)->DumpObjectMap( *GLog );
			}
#endif
			GUndo = nullptr;
			PreviousUndoCount = INDEX_NONE;
			RemovedTransactions.Reset();
		}
		ActiveRecordCounts.Pop();
		CheckState();
	}
	return Result;
}


void UTransBuffer::Reset( const FText& Reason )
{
	if (ensure(!GIsTransacting))
	{
		CheckState();

		if (ActiveCount != 0)
		{
			FString ErrorMessage = TEXT("");
			ErrorMessage += FString::Printf(TEXT("Non zero active count in UTransBuffer::Reset") LINE_TERMINATOR);
			ErrorMessage += FString::Printf(TEXT("ActiveCount : %d") LINE_TERMINATOR, ActiveCount);
			ErrorMessage += FString::Printf(TEXT("SessionName : %s") LINE_TERMINATOR, *GetUndoContext(false).Context);
			ErrorMessage += FString::Printf(TEXT("Reason      : %s") LINE_TERMINATOR, *Reason.ToString());

			ErrorMessage += FString::Printf(LINE_TERMINATOR);
			ErrorMessage += FString::Printf(TEXT("Purging the undo buffer...") LINE_TERMINATOR);

			UE_LOG(LogEditorTransaction, Log, TEXT("%s"), *ErrorMessage);


			// Clear out the transaction buffer...
			Cancel(0);
		}

		// Reset all transactions.
		UndoBuffer.Empty();
		UndoCount = 0;
		ResetReason = Reason;
		ActiveCount = 0;
		ActiveRecordCounts.Empty();

		CheckState();
	}
}


void UTransBuffer::Cancel( int32 StartIndex /*=0*/ )
{
	CheckState();

	// if we don't have any active actions, we shouldn't have an active transaction at all
	if ( ActiveCount > 0 )
	{
		if ( StartIndex == 0 )
		{
			// clear the global pointer to the soon-to-be-deleted transaction
			GUndo = nullptr;
			
			// remove the currently active transaction from the buffer
			UndoBuffer.Pop(false);

			// replace the removed transactions
			UndoBuffer.Reserve(UndoBuffer.Num() + RemovedTransactions.Num());
			for (TSharedRef<FTransaction>& Transaction : RemovedTransactions)
			{
				UndoBuffer.Add(Transaction);
			}
			RemovedTransactions.Reset();

			UndoCount = PreviousUndoCount;
			PreviousUndoCount = INDEX_NONE;
		}
		else
		{
			int32 RecordsToKeep = 0;
			for (int32 ActiveIndex = 0; ActiveIndex <= StartIndex; ++ActiveIndex)
			{
				RecordsToKeep += ActiveRecordCounts[ActiveIndex];
			}

			FTransaction& Transaction = UndoBuffer.Last().Get();
			Transaction.RemoveRecords(Transaction.GetRecordCount() - RecordsToKeep);
		}

		// reset the active count
		ActiveCount = StartIndex;
		ActiveRecordCounts.SetNum(StartIndex);
	}

	CheckState();
}


bool UTransBuffer::CanUndo( FText* Text )
{
	CheckState();
	if( ActiveCount )
	{
		if( Text )
		{
			*Text = NSLOCTEXT("TransactionSystem", "CantUndoDuringTransaction", "(Can't undo while action is in progress)");
		}
		return false;
	}
	
	if (UndoBarrierStack.Num())
	{
		const int32 UndoBarrier = UndoBarrierStack.Last();
		if (UndoBuffer.Num() - UndoCount <= UndoBarrier)
		{
			if (Text)
			{
				*Text = NSLOCTEXT("TransactionSystem", "HitUndoBarrier", "(Hit Undo barrier; can't undo any further)");
			}
			return false;
		}
	}

	if( UndoBuffer.Num()==UndoCount )
	{
		if( Text )
		{
			*Text = FText::Format( NSLOCTEXT("TransactionSystem", "CantUndoAfter", "(Can't undo after: {0})"), ResetReason );
		}
		return false;
	}
	return true;
}


bool UTransBuffer::CanRedo( FText* Text )
{
	CheckState();
	if( ActiveCount )
	{
		if( Text )
		{
			*Text = NSLOCTEXT("TransactionSystem", "CantRedoDuringTransaction", "(Can't redo while action is in progress)");
		}
		return 0;
	}
	if( UndoCount==0 )
	{
		if( Text )
		{
			*Text = NSLOCTEXT("TransactionSystem", "NothingToRedo", "(Nothing to redo)");
		}
		return 0;
	}
	return 1;
}


const FTransaction* UTransBuffer::GetTransaction( int32 QueueIndex ) const
{
	if (UndoBuffer.Num() > QueueIndex && QueueIndex != INDEX_NONE)
	{
		return &UndoBuffer[QueueIndex].Get();
	}

	return NULL;
}


FUndoSessionContext UTransBuffer::GetUndoContext( bool bCheckWhetherUndoPossible )
{
	FUndoSessionContext Context;
	FText Title;
	if( bCheckWhetherUndoPossible && !CanUndo( &Title ) )
	{
		Context.Title = Title;
		return Context;
	}

	TSharedRef<FTransaction>& Transaction = UndoBuffer[ UndoBuffer.Num() - (UndoCount + 1) ];
	return Transaction->GetContext();
}


FUndoSessionContext UTransBuffer::GetRedoContext()
{
	FUndoSessionContext Context;
	FText Title;
	if( !CanRedo( &Title ) )
	{
		Context.Title = Title;
		return Context;
	}

	TSharedRef<FTransaction>& Transaction = UndoBuffer[ UndoBuffer.Num() - UndoCount ];
	return Transaction->GetContext();
}


void UTransBuffer::SetUndoBarrier()
{
	UndoBarrierStack.Push(UndoBuffer.Num() - UndoCount);
}


void UTransBuffer::RemoveUndoBarrier()
{
	if (UndoBarrierStack.Num() > 0)
	{
		UndoBarrierStack.Pop();
	}
}


void UTransBuffer::ClearUndoBarriers()
{
	UndoBarrierStack.Empty();
}


bool UTransBuffer::Undo(bool bCanRedo)
{
	CheckState();

	if (!CanUndo())
	{
		UndoDelegate.Broadcast(FUndoSessionContext(), false);

		return false;
	}

	// Apply the undo changes.
	GIsTransacting = true;
	{
		FTransaction& Transaction = UndoBuffer[ UndoBuffer.Num() - ++UndoCount ].Get();
		UE_LOG(LogEditorTransaction, Log,  TEXT("Undo %s"), *Transaction.GetTitle().ToString() );
		CurrentTransaction = &Transaction;

		BeforeRedoUndoDelegate.Broadcast(Transaction.GetContext());
		Transaction.Apply();
		UndoDelegate.Broadcast(Transaction.GetContext(), true);

		if (!bCanRedo)
		{
			UndoBuffer.RemoveAt(UndoBuffer.Num() - UndoCount, UndoCount);
			UndoCount = 0;
		}

		CurrentTransaction = nullptr;
	}
	GIsTransacting = false;

	CheckState();

	return true;
}

bool UTransBuffer::Redo()
{
	CheckState();

	if (!CanRedo())
	{
		RedoDelegate.Broadcast(FUndoSessionContext(), false);

		return false;
	}

	// Apply the redo changes.
	GIsTransacting = true;
	{
		FTransaction& Transaction = UndoBuffer[ UndoBuffer.Num() - UndoCount-- ].Get();
		UE_LOG(LogEditorTransaction, Log,  TEXT("Redo %s"), *Transaction.GetTitle().ToString() );
		CurrentTransaction = &Transaction;

		BeforeRedoUndoDelegate.Broadcast(Transaction.GetContext());
		Transaction.Apply();
		RedoDelegate.Broadcast(Transaction.GetContext(), true);

		CurrentTransaction = nullptr;
	}
	GIsTransacting = false;

	CheckState();

	return true;
}

bool UTransBuffer::EnableObjectSerialization()
{
	return --DisallowObjectSerialization == 0;
}

bool UTransBuffer::DisableObjectSerialization()
{
	return ++DisallowObjectSerialization == 0;
}


SIZE_T UTransBuffer::GetUndoSize() const
{
	SIZE_T Result=0;
	for( int32 i=0; i<UndoBuffer.Num(); i++ )
	{
		Result += UndoBuffer[i]->DataSize();
	}
	return Result;
}


void UTransBuffer::CheckState() const
{
	// Validate the internal state.
	check(UndoBuffer.Num()>=UndoCount);
	check(ActiveCount>=0);
	check(ActiveRecordCounts.Num() == ActiveCount);
}


void UTransBuffer::SetPrimaryUndoObject(UObject* PrimaryObject)
{
	// Only record the primary object if its transactional, not in any of the temporary packages and theres an active transaction
	if ( PrimaryObject && PrimaryObject->HasAnyFlags( RF_Transactional ) &&
		(PrimaryObject->GetOutermost()->HasAnyPackageFlags( PKG_PlayInEditor|PKG_ContainsScript|PKG_CompiledIn ) == false) )
	{
		const int32 NumTransactions = UndoBuffer.Num();
		const int32 CurrentTransactionIdx = NumTransactions - (UndoCount + 1);

		if ( CurrentTransactionIdx >= 0 )
		{
			TSharedRef<FTransaction>& Transaction = UndoBuffer[ CurrentTransactionIdx ];
			Transaction->SetPrimaryObject(PrimaryObject);
		}
	}
}

bool UTransBuffer::IsObjectInTransationBuffer( const UObject* Object ) const
{
	TArray<UObject*> TransactionObjects;
	for( const TSharedRef<FTransaction>& Transaction : UndoBuffer )
	{
		Transaction->GetTransactionObjects(TransactionObjects);

		if( TransactionObjects.Contains(Object) )
		{
			return true;
		}
		
		TransactionObjects.Reset();
	}

	return false;
}

bool UTransBuffer::IsObjectTransacting(const UObject* Object) const
{
	// We can't provide a truly meaningful answer to this question when not transacting:
	if (ensure(CurrentTransaction))
	{
		return CurrentTransaction->IsObjectTransacting(Object);
	}

	return false;
}

bool UTransBuffer::ContainsPieObject() const
{
	for( const TSharedRef<FTransaction>& Transaction : UndoBuffer )
	{
		if( Transaction->ContainsPieObject() )
		{
			return true;
		}
	}

	return false;
}
