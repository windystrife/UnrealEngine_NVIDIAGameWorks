// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 * Base class for tracking transactions for undo/redo.
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Serialization/ArchiveUObject.h"
#include "Misc/ITransaction.h"
#include "Transactor.generated.h"


/*-----------------------------------------------------------------------------
	FUndoSessionContext
-----------------------------------------------------------------------------*/

/**
 * Convenience struct for passing around undo/redo context
 */
struct FUndoSessionContext 
{
	FUndoSessionContext()
		: Title(), Context(TEXT("")), PrimaryObject(nullptr)
	{}
	FUndoSessionContext (const TCHAR* InContext, const FText& InSessionTitle, UObject* InPrimaryObject) 
		: Title(InSessionTitle), Context(InContext), PrimaryObject(InPrimaryObject)
	{}

	/** Descriptive title of the undo/redo session */
	FText Title;
	/** The context that generated the undo/redo session */
	FString Context;
	/** The primary UObject for the context (if any). */
	UObject* PrimaryObject;
};

/*-----------------------------------------------------------------------------
	FTransaction.
-----------------------------------------------------------------------------*/

/**
 * A single transaction, representing a set of serialized, undo-able changes to a set of objects.
 *
 * warning: The undo buffer cannot be made persistent because of its dependence on offsets
 * of arrays from their owning UObjects.
 *
 * warning: UObject::Serialize implicitly assumes that class properties do not change
 * in between transaction resets.
 */
class UNREALED_API FTransaction : public ITransaction
{
protected:
	/** Map type for efficient unique indexing into UObject* arrays */
	typedef	TMap<UObject*,int32> ObjectMapType;
	
	// Record of an object.
	class UNREALED_API FObjectRecord
	{
	public:

		/** 
			This type is necessary because the blueprint system is destroying and creating 
			CDOs at edit time (usually on compile, but also on load), but also stores user 
			entered data in the CDO. We "need"  changes to a CDO to persist across instances 
			because as we undo and redo we  need to apply changes to different instances of 
			the CDO - alternatively we could destroy and create the CDO as part of a transaction 
			(this alternative is the reason for the bunny ears around need).

			DanO: My long term preference is for the editor to use a dynamic, mutable type
			(rather than the CDO) to store editor data. The CDO can then be re-instanced (or not)
			as runtime code requires.
		*/
		struct FPersistentObjectRef
		{
			// This enum represents all of the different special cases
			// we are handling with this type:
			enum class EReferenceType : uint8
			{
				SubObject,
				RootObject,

				Unknown
			};

			EReferenceType ReferenceType;
			UObject* Object;
			TArray<FName> SubObjectHierarchyID;
			FName ComponentName;

			friend FArchive& operator<<(FArchive& Ar, FPersistentObjectRef& ReferencedObject)
			{
				Ar << (uint8&)ReferencedObject.ReferenceType;
				Ar << ReferencedObject.Object;
				Ar << ReferencedObject.SubObjectHierarchyID;
				return Ar;
			}

			FPersistentObjectRef()
				: ReferenceType(EReferenceType::Unknown)
				, Object(nullptr)
			{}

			explicit FPersistentObjectRef(UObject* InObject);

			UObject* Get() const;

			UObject* operator->() const
			{
				UObject* Obj = Get();
				check(Obj);
				return Obj;
			}
		};

		// Variables.
		/** The data stream used to serialize/deserialize record */
		TArray<uint8>		Data;
		/** External objects referenced in the transaction */
		TArray<FPersistentObjectRef>	ReferencedObjects;
		/** FNames referenced in the object record */
		TArray<FName>		ReferencedNames;
		/** The object to track */
		FPersistentObjectRef	Object;
		/** Annotation data for the object stored externally */
		TSharedPtr<ITransactionObjectAnnotation> ObjectAnnotation;
		/** Array: If an array object, reference to script array */
		FScriptArray*		Array;
		/** Array: Offset into the array */
		int32				Index;
		/** Array: How many items to record */
		int32				Count;
		/** @todo document this
		 Array: Operation performed on array: 1 (add/insert), 0 (modify), -1 (remove)? */
		int32				Oper;
		/** Array: Size of each item in the array */
		int32				ElementSize;
		/** Array: DefaultConstructor for each item in the array */
		STRUCT_DC			DefaultConstructor;
		/** Array: Serializer to use for each item in the array */
		STRUCT_AR			Serializer;
		/** Array: Destructor for each item in the array */
		STRUCT_DTOR			Destructor;
		/** True if object  has already been restored from data. False otherwise. */
		bool				bRestored;
		/** True if record should serialize data as binary blob (more compact). False to use tagged serialization (more robust) */
		bool				bWantsBinarySerialization;

		/** Copy of data that will be used when the transaction is flipped */
		TArray<uint8> FlipData;

		/** Copy of ReferencedObjects that will be used when the transaction is flipped */
		TArray<FPersistentObjectRef> FlipReferencedObjects;

		/** Copy of ReferencedNames that will be used when the transaction is flipped */
		TArray<FName> FlipReferencedNames;

		/** Copy of ObjectAnnotation that will be used when the transaction is flipped */
		TSharedPtr<ITransactionObjectAnnotation> FlipObjectAnnotation;

		// Constructors.
		FObjectRecord()
		{}
		FObjectRecord( FTransaction* Owner, UObject* InObject, FScriptArray* InArray, int32 InIndex, int32 InCount, int32 InOper, int32 InElementSize, STRUCT_DC InDefaultConstructor, STRUCT_AR InSerializer, STRUCT_DTOR InDestructor );

		// Functions.
		void SerializeContents( FArchive& Ar, int32 InOper );
		void Restore( FTransaction* Owner );
		void Save( FTransaction* Owner );
		void Load( FTransaction* Owner );

		/** Used by GC to collect referenced objects. */
		void AddReferencedObjects( FReferenceCollector& Collector );

		/** @return True if this record contains a reference to a pie object */
		bool ContainsPieObject() const;

		/** Transfers data from an array. */
		class FReader : public FArchiveUObject
		{
		public:
			FReader(
				FTransaction* InOwner,
				const TArray<uint8>& InData,
				const TArray<FPersistentObjectRef>& InReferencedObjects,
				const TArray<FName>& InReferencedNames,
				bool bWantBinarySerialization
				):
				Owner(InOwner),
				Data(InData),
				ReferencedObjects(InReferencedObjects),
				ReferencedNames(InReferencedNames),
				Offset(0)
			{
				ArWantBinaryPropertySerialization = bWantBinarySerialization;
				ArIsLoading = ArIsTransacting = 1;
			}

			virtual int64 Tell() override {return Offset;}
			virtual void Seek( int64 InPos ) override { Offset = InPos; }

		private:
			void Serialize( void* SerData, int64 Num ) override
			{
				if( Num )
				{
					checkSlow(Offset+Num<=Data.Num());
					FMemory::Memcpy( SerData, &Data[Offset], Num );
					Offset += Num;
				}
			}
			FArchive& operator<<( class FName& N ) override
			{
				int32 NameIndex = 0;
				(FArchive&)*this << NameIndex;
				N = ReferencedNames[NameIndex];
				return *this;
			}
			FArchive& operator<<( class UObject*& Res ) override
			{
				int32 ObjectIndex = 0;
				(FArchive&)*this << ObjectIndex;
				if (ObjectIndex != INDEX_NONE)
				{
					Res = ReferencedObjects[ObjectIndex].Get();
				}
				else
				{
					Res = nullptr;
				}
				return *this;
			}
			void Preload( UObject* InObject ) override
			{
				if( Owner )
				{
					if ( Owner->ObjectMap.Contains( InObject ) )
					{
						for (int32 i = 0; i < Owner->Records.Num(); i++)
						{
							if (Owner->Records[i].Object.Get() == InObject)
							{
								Owner->Records[i].Restore(Owner);
							}
						}
					}
				}
			}
			FTransaction* Owner;
			const TArray<uint8>& Data;
			const TArray<FPersistentObjectRef>& ReferencedObjects;
			const TArray<FName>& ReferencedNames;
			int64 Offset;
		};

		/**
		 * Transfers data to an array.
		 */
		class FWriter : public FArchiveUObject
		{
		public:
			FWriter(
				TArray<uint8>& InData,
				TArray<FPersistentObjectRef>& InReferencedObjects,
				TArray<FName>& InReferencedNames,
				bool bWantBinarySerialization
				):
				Data(InData),
				ReferencedObjects(InReferencedObjects),
				ReferencedNames(InReferencedNames),
				Offset(0)
			{
				for(int32 ObjIndex = 0; ObjIndex < InReferencedObjects.Num(); ++ObjIndex)
				{
					ObjectMap.Add(InReferencedObjects[ObjIndex].Get(), ObjIndex);
				}

				ArWantBinaryPropertySerialization = bWantBinarySerialization;
				ArIsSaving = ArIsTransacting = 1;
			}

			virtual int64 Tell() override {return Offset;}
			virtual void Seek( int64 InPos ) override
			{
				checkSlow(Offset<=Data.Num());
				Offset = InPos; 
			}

		private:
			void Serialize( void* SerData, int64 Num ) override
			{
				if( Num )
				{
					int32 DataIndex = ( Offset == Data.Num() )
						?  Data.AddUninitialized(Num)
						:  Offset;
					
					FMemory::Memcpy( &Data[DataIndex], SerData, Num );
					Offset+= Num;
				}
			}
			FArchive& operator<<( class FName& N ) override
			{
				int32 NameIndex = ReferencedNames.AddUnique(N);
				return (FArchive&)*this << NameIndex;
			}
			FArchive& operator<<( class UObject*& Res ) override
			{
				int32 ObjectIndex = INDEX_NONE;
				int32* ObjIndexPtr = ObjectMap.Find(Res);
				if(ObjIndexPtr)
				{
					ObjectIndex = *ObjIndexPtr;
				}
				else if(Res)
				{
					ObjectIndex = ReferencedObjects.Add(FPersistentObjectRef(Res));
					ObjectMap.Add(Res, ObjectIndex);
				}
				return (FArchive&)*this << ObjectIndex;
			}
			TArray<uint8>& Data;
			ObjectMapType ObjectMap;
			TArray<FPersistentObjectRef>& ReferencedObjects;
			TArray<FName>& ReferencedNames;
			int64 Offset;
		};
	};

	// Transaction variables.
	/** List of object records in this transaction */
	TArray<FObjectRecord>	Records;
	
	/** Description of the transaction. Can be used by UI */
	FText		Title;
	
	/** A text string describing the context for the transaction. Typically the name of the system causing the transaction*/
	FString		Context;

	/** The key object being edited in this transaction. For example the blueprint object. Can be NULL */
	UObject*	PrimaryObject;

	/** Used to prevent objects from being serialized to a transaction more than once. */
	ObjectMapType			ObjectMap;

	/** If true, on apply flip the direction of iteration over object records. The only client for which this is false is FMatineeTransaction */
	bool					bFlip;
	/** Used to track direction to iterate over transaction's object records. Typically -1 for Undo, 1 for Redo */
	int32					Inc;
	/** Count of the number of UModels modified since the last call to FTransaction::Apply */
	int32					NumModelsModified;

	/** Objects that will be changed directly by the transaction, empty when not transacting */
	TMap<UObject*, TSharedPtr<ITransactionObjectAnnotation>> ChangedObjects;
public:
	// Constructor.
	FTransaction(  const TCHAR* InContext=nullptr, const FText& InTitle=FText(), bool bInFlip=false )
		:	Title( InTitle )
		,	Context( InContext )
		,	PrimaryObject(nullptr)
		,	bFlip(bInFlip)
		,	Inc(-1)
	{}

	virtual ~FTransaction()
	{
	}

	// FTransactionBase interface.
	virtual void SaveObject( UObject* Object ) override;
	virtual void SaveArray( UObject* Object, FScriptArray* Array, int32 Index, int32 Count, int32 Oper, int32 ElementSize, STRUCT_DC DefaultConstructor, STRUCT_AR Serializer, STRUCT_DTOR Destructor ) override;
	virtual void SetPrimaryObject(UObject* InObject) override;

	/**
	 * Enacts the transaction.
	 */
	virtual void Apply() override;

	/** Returns a unique string to serve as a type ID for the FTranscationBase-derived type. */
	virtual const TCHAR* GetTransactionType() const
	{
		return TEXT("FTransaction");
	}

	// FTransaction interface.
	SIZE_T DataSize() const;

	/** Returns the descriptive text for the transaction */
	FText GetTitle() const
	{
		return Title;
	}

	/** Gets the full context for the transaction */
	FUndoSessionContext GetContext() const
	{
		return FUndoSessionContext(*Context, Title, PrimaryObject);
	}

	/** Serializes a reference to a transaction in a given archive. */
	friend FArchive& operator<<( FArchive& Ar, FTransaction& T )
	{
		return Ar << T.Records << T.Title << T.ObjectMap << T.Context << T.PrimaryObject;
	}

	/** Serializes a reference to a transaction in a given archive. */
	friend FArchive& operator<<(FArchive& Ar, TSharedRef<FTransaction>& SharedT)
	{
		return Ar << SharedT.Get();
	}

	/** Used by GC to collect referenced objects. */
	void AddReferencedObjects( FReferenceCollector& Collector );

	/** Returns the number of models that were modified by the last call to FTransaction::Apply(). */
	int32 GetNumModelsModified() const
	{
		return NumModelsModified;
	}

	/**
	 * Get all the objects that are part of this transaction.
	 * @param	Objects		[out] Receives the object list.  Previous contents are cleared.
	 */
	void GetTransactionObjects(TArray<UObject*>& Objects) const;
	void RemoveRecords( int32 Count = 1 );
	int32 GetRecordCount() const;

	const UObject* GetPrimaryObject() const { return PrimaryObject; }

	/** @return True if this record contains a reference to a pie object */
	bool ContainsPieObject() const;

	/** Checks if a specific object is in the transaction currently underway */
	bool IsObjectTransacting(const UObject* Object) const;

	/**
	 * Outputs the contents of the ObjectMap to the specified output device.
	 */
	void DumpObjectMap(FOutputDevice& Ar) const;

	// Transaction friends.
	friend FArchive& operator<<( FArchive& Ar, FTransaction::FObjectRecord& R );

	friend class FObjectRecord;
	friend class FObjectRecord::FReader;
	friend class FObjectRecord::FWriter;
};

UCLASS(transient)
class UNREALED_API UTransactor : public UObject
{
    GENERATED_UCLASS_BODY()
	/**
	 * Begins a new undo transaction.  An undo transaction is defined as all actions
	 * which take place when the user selects "undo" a single time.
	 * If there is already an active transaction in progress, increments that transaction's
	 * action counter instead of beginning a new transaction.
	 * 
	 * @param	SessionContext	the context for the undo session; typically the tool/editor that cause the undo operation
	 * @param	Description		the description for the undo session;  this is the text that 
	 *							will appear in the "Edit" menu next to the Undo item
	 *
	 * @return	Number of active actions when Begin() was called;  values greater than
	 *			0 indicate that there was already an existing undo transaction in progress.
	 */
	virtual int32 Begin( const TCHAR* SessionContext, const FText& Description ) PURE_VIRTUAL(UTransactor::Begin,return 0;);

	/**
	 * Attempts to close an undo transaction.  Only successful if the transaction's action
	 * counter is 1.
	 * 
	 * @return	Number of active actions when End() was called; a value of 1 indicates that the
	 *			transaction was successfully closed
	 */
	virtual int32 End() PURE_VIRTUAL(UTransactor::End,return 0;);

	/**
	 * Cancels the current transaction, no longer capture actions to be placed in the undo buffer.
	 *
	 * @param	StartIndex	the value of ActiveIndex when the transaction to be canceled was began. 
	 */
	virtual void Cancel( int32 StartIndex = 0 ) PURE_VIRTUAL(UTransactor::Cancel,);

	/**
	 * Resets the entire undo buffer;  deletes all undo transactions.
	 */
	virtual void Reset( const FText& Reason ) PURE_VIRTUAL(UTransactor::Reset,);

	/**
	 * Returns whether there are any active actions; i.e. whether actions are currently
	 * being captured into the undo buffer.
	 */
	virtual bool IsActive() PURE_VIRTUAL(UTransactor::IsActive,return false;);

	/**
	 * Determines whether the undo option should be selectable.
	 * 
	 * @param	Str		[out] the reason that undo is disabled
	 *
	 * @return	true if the "Undo" option should be selectable.
	 */
	virtual bool CanUndo( FText* Text=nullptr ) PURE_VIRTUAL(UTransactor::CanUndo,return false;);

	/**
	 * Determines whether the redo option should be selectable.
	 * 
	 * @param	Str		[out] the reason that redo is disabled
	 *
	 * @return	true if the "Redo" option should be selectable.
	 */
	virtual bool CanRedo( FText* Text=nullptr ) PURE_VIRTUAL(UTransactor::CanRedo,return false;);

	/**
	 * Gets the current length of the transaction queue.
	 *
	 * @return Queue length.
	 */
	virtual int32 GetQueueLength( ) const PURE_VIRTUAL(UTransactor::GetQueueLength,return 0;);

	/**
	 * Gets the transaction at the specified queue index.
	 *
	 * @param QueueIndex - The index of the transaction in the queue.
	 *
	 * @return A read-only pointer to the transaction, or NULL if it does not exist.
	 */
	virtual const FTransaction* GetTransaction( int32 QueueIndex ) const PURE_VIRTUAL(UTransactor::GetQueueEntry,return nullptr;);

	/**
	 * Returns the description of the undo action that will be performed next.
	 * This is the text that is shown next to the "Undo" item in the menu.
	 * 
	 * @param	bCheckWhetherUndoPossible	Perform test whether undo is possible and return Error if not and option is set
	 *
	 * @return	text describing the next undo transaction
	 */
	virtual FUndoSessionContext GetUndoContext ( bool bCheckWhetherUndoPossible = true ) PURE_VIRTUAL(UTransactor::GetUndoDesc,return FUndoSessionContext(););

	/**
	 * Determines the amount of data currently stored by the transaction buffer.
	 *
	 * @return	number of bytes stored in the undo buffer
	 */
	virtual SIZE_T GetUndoSize() const PURE_VIRTUAL(UTransactor::GetUndoSize,return 0;);

	/**
	 * Gets the number of transactions that were undone and can be redone.
	 *
	 * @return Undo count.
	 */
	virtual int32 GetUndoCount( ) const PURE_VIRTUAL(UTransactor::GetUndoCount,return 0;);

	/**
	 * Returns the description of the redo action that will be performed next.
	 * This is the text that is shown next to the "Redo" item in the menu.
	 * 
	 * @return	text describing the next redo transaction
	 */
	virtual FUndoSessionContext GetRedoContext () PURE_VIRTUAL(UTransactor::GetRedoDesc,return FUndoSessionContext(););

	/**
	 * Sets an undo barrier at the current point in the transaction buffer.
	 * Undoing beyond this point will not be allowed until the barrier is removed.
	 */
	virtual void SetUndoBarrier() PURE_VIRTUAL(UTransactor::SetUndoBarrier, );

	/**
	 * Removes the last set undo barrier from the transaction buffer.
	 */
	virtual void RemoveUndoBarrier() PURE_VIRTUAL(UTransactor::RemoveUndoBarrier, );

	/**
	 * Clears all undo barriers.
	 */
	virtual void ClearUndoBarriers() PURE_VIRTUAL(UTransactor::ClearUndoBarriers, );

	/**
	 * Executes an undo transaction, undoing all actions contained by that transaction.
	 * 
	 * @param	bCanRedo	If false indicates that the undone transaction (and any transactions that came after it) cannot be redone
	 * 
	 * @return				true if the transaction was successfully undone
	 */
	virtual bool Undo(bool bCanRedo = true) PURE_VIRTUAL(UTransactor::Undo,return false;);

	/**
	 * Executes an redo transaction, redoing all actions contained by that transaction.
	 * 
	 * @return				true if the transaction was successfully redone
	 */
	virtual bool Redo() PURE_VIRTUAL(UTransactor::Redo,return false;);

	/**
	 * Enables the transaction buffer to serialize the set of objects it references.
	 *
	 * @return	true if the transaction buffer is able to serialize object references.
	 */
	virtual bool EnableObjectSerialization() { return false; }

	/**
	 * Disables the transaction buffer from serializing the set of objects it references.
	 *
	 * @return	true if the transaction buffer is able to serialize object references.
	 */
	virtual bool DisableObjectSerialization() { return false; }

	/**
	 * Wrapper for checking if the transaction buffer is allowed to serialize object references.
	 */
	virtual bool IsObjectSerializationEnabled() { return false; }

	/** 
	 * Set passed object as the primary context object for transactions
	 */
	virtual void SetPrimaryUndoObject( UObject* Object ) PURE_VIRTUAL(UTransactor::SetPrimaryUndoObject,);

	/** Checks if a specific object is referenced by the transaction buffer */
	virtual bool IsObjectInTransationBuffer( const UObject* Object ) const { return false; }

	/** Checks if a specific object is in the transaction currently underway */
	virtual bool IsObjectTransacting(const UObject* Object) const PURE_VIRTUAL(UTransactor::IsObjectTransacting, return false;);

	/** @return True if this record contains a reference to a pie object */
	virtual bool ContainsPieObject() const { return false; }
};
