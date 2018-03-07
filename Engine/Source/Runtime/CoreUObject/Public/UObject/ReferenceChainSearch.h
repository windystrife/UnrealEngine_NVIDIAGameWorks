// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Class.h"

class FReferenceChainSearch
{
public:

	/** Describes how an object is referenced */
	struct EReferenceType
	{
		enum Type
		{
			/** Invalid */
			Invalid,
			/** Object is referenced through a property */
			Property,
			/** Object is referenced by an array */
			ArrayProperty,
			/** Object is added to the references list by a StructAddReferencedObject call */
			StructARO,
			/** Object is added to the references list by an AddReferencedObject call */
			ARO,
			/** Object is referenced by a map */
			MapProperty,
			/** Object is referenced by a set */
			SetProperty,
		};
	};

	/** Entry in the reference chain */
	struct FReferenceChainLink
	{
		/** Index of ReferencedObj token in ReferencedBy->ReferenceTokenStream **/
		int32 ReferencedObjectIndex;
		/** Describes in which way an object is referenced */
		EReferenceType::Type ReferenceType;
		/** The object that is referencing */
		UObject* ReferencedBy;
		/** Pointer to the object/function that the object is referenced through. Depending on the ReferenceType
			this can be a UProperty or a memberfunction pointer to an ARO or StructARO function. */
		void* ReferencedThrough;
		/** The object that is referenced by this link */
		UObject* ReferencedObj;
		/** Array Index of the references. Only valid if reference is through an array. */
		int32 ArrayIndex;

		FReferenceChainLink()
			:ReferencedObjectIndex(INDEX_NONE), ReferenceType(EReferenceType::Invalid), ReferencedBy(NULL), ReferencedThrough(NULL), ArrayIndex(INDEX_NONE) {}

		FReferenceChainLink(int32 InReferencedObjectIndex, EReferenceType::Type RefType, UObject* InReferencedBy, void* InReferencedThrough, UObject* InReferencedObj, int32 InArrayIndex = INDEX_NONE)
			:ReferencedObjectIndex(InReferencedObjectIndex), ReferenceType(RefType), ReferencedBy(InReferencedBy), ReferencedThrough(InReferencedThrough), ReferencedObj(InReferencedObj), ArrayIndex(InArrayIndex)
		{ }
#if !(UE_BUILD_TEST || UE_BUILD_SHIPPING)
		FORCEINLINE FString GetReferencedByName() const
		{
			return ReferencedBy->GetClass()->DebugTokenMap.GetTokenInfo(ReferencedObjectIndex).Name.ToString();
		}
#endif // !(UE_BUILD_TEST || UE_BUILD_SHIPPING)

		/** Returns whether this link is a property reference or not */
		FORCEINLINE bool IsProperty() const { return ReferenceType == EReferenceType::Property || ReferenceType == EReferenceType::ArrayProperty || ReferenceType == EReferenceType::MapProperty; }

		FORCEINLINE bool operator == (const FReferenceChainLink& Rhs) const { return ReferencedBy == Rhs.ReferencedBy && ReferencedObj == Rhs.ReferencedObj; }

		FString ToString() const;
	};

	/** Stores the entire reference chain from the topmost object up to the object which referencers are searched */
	struct FReferenceChain
	{
		/** Array of reference links */
		TArray<FReferenceChainLink> RefChain;

		bool Contains(const FReferenceChain& Other);
	};

	/** Reference Collector to hijack AddReferencedObjects-Calls */
	class FFindReferencerCollector : public FReferenceCollector
	{
		/** Backpointer to the search-instance */
		FReferenceChainSearch* RefSearchArc;
		/** Reference type of this collector */
		EReferenceType::Type RefType;
		/** ARO function pointer */
		void* AROFuncPtr;
		/** Referencing object being AROed*/
		UObject* ReferencingObject;
#if !(UE_BUILD_TEST || UE_BUILD_SHIPPING)
		/** Finds index of ReferencedObject in ReferencedBy.ReferenceTokenStream */
		int32 FindReferencedObjectIndex(const UObject& ReferencedBy, const UObject& ReferencedObject);
#endif // !(UE_BUILD_TEST || UE_BUILD_SHIPPING)
	public:

		FFindReferencerCollector( FReferenceChainSearch* InRefSearchArc, EReferenceType::Type InRefType, void* InAROFuncPtr, UObject* InReferencingObject = NULL)
			: RefSearchArc(InRefSearchArc), RefType(InRefType), AROFuncPtr(InAROFuncPtr), ReferencingObject(InReferencingObject)
		{
		}

		virtual void HandleObjectReference( UObject*& InObject, const UObject* InReferencingObject, const UProperty* ReferencingProperty ) override;

		virtual bool IsIgnoringArchetypeRef() const override	{ return false; }
		virtual bool IsIgnoringTransient() const override		{ return false; }

		TArray<FReferenceChainLink> References;
	};


	struct ESearchMode
	{
		enum Type
		{
			// Returns all reference chains found
			Default			= 0,
			// Returns only reference chains from external objects
			ExternalOnly	= 1<<0,
			// Returns only the shortest reference chain
			Shortest		= 1<<1,
			// Returns only the longest reference chain
			Longest			= 1<<2,
			// Returns only the direct referencers
			Direct			= 1<<3,
			
			// Print results
			PrintResults	= 1<<16,
		};
	};

	/** 
	 *	Constructs the reference chain search and start it.
	 *
	 *	@param		ObjectToFind		The Object we want the referencers to be found for
	 **/
	FReferenceChainSearch(UObject* ObjectToFind, uint32 Mode = ESearchMode::PrintResults);

	/** 
	 *	Returns the array of ReferenceChains found, which lead to the Object in question
	 *
	 *	@return		Array of reference chains
	 **/
	const TArray<FReferenceChain>& GetReferenceChains() const { return Referencers; }

	/** Dumps results to log */
	void PrintResults();

private:
	/** Object we want to find all the referencers to */
	UObject* ObjectToFind;
	/** Array of all found reference chains leading to the object in question. */
	TArray<FReferenceChain> Referencers;
	/** Cache to store all the references of objects. */
	TMap<UObject*, TArray<FReferenceChainLink> > ReferenceMap;
	/** Search mode */
	uint32 SearchMode;
	
	/** Prints the passed in reference chain to the log. */
	void PrintReferencers(FReferenceChain& Referencer);
	/** Performs the referencer search */
	void PerformSearch();
	/** Processes an object and returns whether it is part of the reference chain or not */
	void ProcessObject(UObject* CurrentObject);
	/** Inserts the reference chain into the result set and tries to merge it into an already
		present one, if possible */
	void InsertReferenceChain(FReferenceChain& Referencer);
	/** Generates internal reference graph representation */
	void BuildRefGraph();
	/** Recursively creates reference chains from the internal reference graph relationship */
	void CreateReferenceChain(struct FRefGraphItem* Node, FReferenceChain& ThisChain, TArray<FReferenceChain>& ChainArray, UObject* ObjectToFind, int32 Levels);
	/** Returns true if the search should log anything */
	bool ShouldOutputToLog() const
	{
		return !!(SearchMode & ESearchMode::PrintResults);
	}
};
