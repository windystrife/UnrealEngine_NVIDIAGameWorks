// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PropertyNode.h"
#include "UObject/StructOnScope.h"

//-----------------------------------------------------------------------------
//	FStructPropertyNode - Used for the root and various sub-nodes
//-----------------------------------------------------------------------------

class FStructurePropertyNode : public FComplexPropertyNode
{
public:
	FStructurePropertyNode() : FComplexPropertyNode() {}
	virtual ~FStructurePropertyNode() {}

	virtual FStructurePropertyNode* AsStructureNode() override { return this; }
	virtual const FStructurePropertyNode* AsStructureNode() const override { return this; }

	/** FPropertyNode Interface */
	virtual uint8* GetValueBaseAddress(uint8* Base) override
	{
		ensure(true);
		return HasValidStructData() ? StructData->GetStructMemory() : NULL;
	}

	void SetStructure(TSharedPtr<FStructOnScope> InStructData)
	{
		ClearCachedReadAddresses(true);
		DestroyTree();
		StructData = InStructData;
	}

	bool HasValidStructData() const
	{
		return StructData.IsValid() && StructData->IsValid();
	}

	TSharedPtr<FStructOnScope> GetStructData() const
	{
		return StructData;
	}

	bool GetReadAddressUncached(FPropertyNode& InPropertyNode, FReadAddressListData& OutAddresses) const override
	{
		if (!HasValidStructData())
		{
			return false;
		}

		UProperty* InItemProperty = InPropertyNode.GetProperty();
		if (!InItemProperty)
		{
			return false;
		}

		uint8* ReadAddress = StructData->GetStructMemory();
		check(ReadAddress);
		OutAddresses.Add(NULL, InPropertyNode.GetValueBaseAddress(ReadAddress), true);
		return true;
	}

	bool GetReadAddressUncached(FPropertyNode& InPropertyNode,
		bool InRequiresSingleSelection,
		FReadAddressListData* OutAddresses,
		bool bComparePropertyContents,
		bool bObjectForceCompare,
		bool bArrayPropertiesCanDifferInSize) const override
	{
		if(OutAddresses)
		{
			return GetReadAddressUncached(InPropertyNode, *OutAddresses);
		}
		else
		{
			FReadAddressListData Unused;
			return GetReadAddressUncached(InPropertyNode, Unused);
		}
	}

	UPackage* GetOwnerPackage() const
	{
		return HasValidStructData() ? StructData->GetPackage() : nullptr;
	}

	/** FComplexPropertyNode Interface */
	virtual const UStruct* GetBaseStructure() const override
	{ 
		return HasValidStructData() ? StructData->GetStruct() : NULL;
	}
	virtual UStruct* GetBaseStructure() override
	{
		const UStruct* Struct = HasValidStructData() ? StructData->GetStruct() : NULL;
		return const_cast<UStruct*>(Struct);
	}
	virtual int32 GetInstancesNum() const override
	{ 
		return HasValidStructData() ? 1 : 0;
	}
	virtual uint8* GetMemoryOfInstance(int32 Index) override
	{ 
		check(0 == Index);
		return HasValidStructData() ? StructData->GetStructMemory() : NULL;
	}
	virtual TWeakObjectPtr<UObject> GetInstanceAsUObject(int32 Index) override
	{
		check(0 == Index);
		return NULL;
	}
	virtual EPropertyType GetPropertyType() const override
	{
		return EPT_StandaloneStructure;
	}

	virtual void Disconnect() override
	{
		SetStructure(NULL);
	}

protected:

	/** FPropertyNode interface */
	virtual void InitChildNodes() override;

	virtual bool GetQualifiedName(FString& PathPlusIndex, const bool bWithArrayIndex, const FPropertyNode* StopParent = nullptr, bool bIgnoreCategories = false) const override
	{
		PathPlusIndex += TEXT("Struct");
		return true;
	}

private:
	TSharedPtr<FStructOnScope> StructData;
};

