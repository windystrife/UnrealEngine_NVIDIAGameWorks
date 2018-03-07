// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PropertyNode.h"

//-----------------------------------------------------------------------------
//	FObjectPropertyNode - Used for the root and various sub-nodes
//-----------------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// Object iteration
typedef TArray< TWeakObjectPtr<UObject> >::TIterator TPropObjectIterator;
typedef TArray< TWeakObjectPtr<UObject> >::TConstIterator TPropObjectConstIterator;

class FObjectPropertyNode : public FComplexPropertyNode
{
public:
	FObjectPropertyNode();
	virtual ~FObjectPropertyNode();

	/** FPropertyNode Interface */
	virtual FObjectPropertyNode* AsObjectNode() override { return this;}
	virtual const FObjectPropertyNode* AsObjectNode() const override { return this; }
	virtual bool GetReadAddressUncached(FPropertyNode& InNode, bool InRequiresSingleSelection, FReadAddressListData* OutAddresses, bool bComparePropertyContents = true, bool bObjectForceCompare = false, bool bArrayPropertiesCanDifferInSize = false) const override;
	virtual bool GetReadAddressUncached(FPropertyNode& InNode, FReadAddressListData& OutAddresses) const override;

	virtual uint8* GetValueBaseAddress( uint8* Base ) override;

	/**
	 * Returns the UObject at index "n" of the Objects Array
	 * @param InIndex - index to read out of the array
	 */
	UObject* GetUObject(int32 InIndex);
	const UObject* GetUObject(int32 InIndex) const;

	/**
	 * Returns the UPackage at index "n" of the Objects Array
	 * @param InIndex - index to read out of the array
	 */
	UPackage* GetUPackage(int32 InIndex);
	const UPackage* GetUPackage(int32 InIndex) const;

	/** Returns the number of objects for which properties are currently being edited. */
	int32 GetNumObjects() const	{ return Objects.Num(); }

	/**
	 * Adds a new object to the list.
	 */
	void AddObject( UObject* InObject );
	/**
	 * Removes an object from the list.
	 */
	void RemoveObject(UObject* InObject);
	/**
	 * Removes all objects from the list.
	 */
	void RemoveAllObjects();

	/** Set overrides that should be used when looking for packages that contain the given object */
	void SetObjectPackageOverrides(const TMap<TWeakObjectPtr<UObject>, TWeakObjectPtr<UPackage>>& InMapping);
	/** Clear overrides that should be used when looking for packages that contain the given object */
	void ClearObjectPackageOverrides();

	/**
	 * Purges any objects marked pending kill from the object list
	 */
	void PurgeKilledObjects();

	// Called when the object list is finalized, Finalize() finishes the property window setup.
	void Finalize();

	/** @return		The base-est baseclass for objects in this list. */
	UClass*			GetObjectBaseClass()       { return BaseClass.IsValid() ? BaseClass.Get() : NULL; }
	/** @return		The base-est baseclass for objects in this list. */
	const UClass*	GetObjectBaseClass() const { return BaseClass.IsValid() ? BaseClass.Get() : NULL; }


	// FComplexPropertyNode implementation
	virtual UStruct* GetBaseStructure() override { return GetObjectBaseClass(); }
	virtual const UStruct* GetBaseStructure() const override{ return GetObjectBaseClass(); }
	virtual int32 GetInstancesNum() const override{ return GetNumObjects(); }
	virtual uint8* GetMemoryOfInstance(int32 Index) override
	{
		return (uint8*)GetUObject(Index);
	}
	virtual TWeakObjectPtr<UObject> GetInstanceAsUObject(int32 Index) override
	{
		check(Objects.IsValidIndex(Index));
		return Objects[Index];
	}
	virtual EPropertyType GetPropertyType() const override { return EPT_Object; }
	virtual void Disconnect() override
	{
		RemoveAllObjects();
	}

	//////////////////////////////////////////////////////////////////////////
	/** @return		The property stored at this node, to be passed to Pre/PostEditChange. */
	virtual UProperty*		GetStoredProperty()		{ return StoredProperty.IsValid() ? StoredProperty.Get() : NULL; }

	TPropObjectIterator			ObjectIterator()			{ return TPropObjectIterator( Objects ); }
	TPropObjectConstIterator	ObjectConstIterator() const	{ return TPropObjectConstIterator( Objects ); }


	/** Generates a single child from the provided property name.  Any existing children are destroyed */
	TSharedPtr<FPropertyNode> GenerateSingleChild( FName ChildPropertyName );

	/**
	 * @return The hidden categories 
	 */
	const TSet<FName>& GetHiddenCategories() const { return HiddenCategories; }

	bool IsRootNode() const { return ParentNode == nullptr; }
protected:
	/** FPropertyNode interface */
	virtual void InitBeforeNodeFlags() override;
	virtual void InitChildNodes() override;
	virtual bool GetQualifiedName( FString& PathPlusIndex, const bool bWithArrayIndex, const FPropertyNode* StopParent = nullptr, bool bIgnoreCategories = false ) const override;

	/**
	 * Looks at the Objects array and creates the best base class.  Called by
	 * Finalize(); that is, when the list of selected objects is being finalized.
	 */
	void SetBestBaseClass();

private:
	/**
	 * Creates child nodes
	 * 
	 * @param SingleChildName	The property name of a single child to create instead of all childen
	 */
	void InternalInitChildNodes( FName SingleChildName = NAME_None );
private:
	/** The list of objects we are editing properties for. */
	TArray< TWeakObjectPtr<UObject> >		Objects;

	/** The lowest level base class for objects in this list. */
	TWeakObjectPtr<UClass>					BaseClass;

	/**
	 * The property passed to Pre/PostEditChange calls.  
	 */
	TWeakObjectPtr<UProperty>				StoredProperty;

	/**
	 * Set of all category names hidden by the objects in this node
	 */
	TSet<FName> HiddenCategories;

	/** Object -> Package re-mapping */
	TMap<TWeakObjectPtr<UObject>, TWeakObjectPtr<UPackage>> ObjectToPackageMapping;
};

