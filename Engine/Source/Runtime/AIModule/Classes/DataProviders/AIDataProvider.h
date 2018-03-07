// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Templates/SubclassOf.h"
#include "UObject/UnrealType.h"
#include "AIDataProvider.generated.h"

class UAIDataProvider;

/**
 * AIDataProvider is an object that can provide collection of properties
 * associated with bound pawn owner or request Id.
 *
 * Editable properties are used to set up provider instance,
 * creating additional filters or ways of accessing data (e.g. gameplay tag of ability)
 *
 * Non editable properties are holding data
 */

USTRUCT()
struct AIMODULE_API FAIDataProviderValue
{
	GENERATED_USTRUCT_BODY()

private:
	/** cached uproperty of provider */
	UPROPERTY(transient)
	mutable UProperty* CachedProperty;

public:
	/** (optional) provider for dynamic data binding */
	UPROPERTY(EditAnywhere, Instanced, Category = Value)
	UAIDataProvider* DataBinding;

	/** name of provider's value property */
	UPROPERTY(EditAnywhere, Category = Value)
	FName DataField;

	/** describe default data */
	virtual FString ValueToString() const;
	FString ToString() const;

	/** filter for provider's properties */
	virtual bool IsMatchingType(UProperty* PropType) const;

	/** find all properties of provider that are matching filter */
	void GetMatchingProperties(TArray<FName>& MatchingProperties) const;

	/** return raw data from provider's property */
	template<typename T>
	T* GetRawValuePtr() const;

	/** bind data in provider and cache property for faster access */
	void BindData(const UObject* Owner, int32 RequestId) const;

	FORCEINLINE bool IsDynamic() const { return DataBinding != nullptr; }

	FAIDataProviderValue() :
		CachedProperty(nullptr),
		DataBinding(nullptr)
	{
	}

	virtual ~FAIDataProviderValue() {};
};

USTRUCT()
struct AIMODULE_API FAIDataProviderTypedValue : public FAIDataProviderValue
{
	GENERATED_USTRUCT_BODY()

	/** type of value */
	UPROPERTY()
	TSubclassOf<UProperty> PropertyType;

	/** filter for provider's properties */
	virtual bool IsMatchingType(UProperty* PropType) const override;
};

USTRUCT()
struct AIMODULE_API FAIDataProviderStructValue : public FAIDataProviderValue
{
	GENERATED_USTRUCT_BODY()

	/** name of UStruct type */
	FString StructName;

	virtual bool IsMatchingType(UProperty* PropType) const override;
};

USTRUCT()
struct AIMODULE_API FAIDataProviderIntValue : public FAIDataProviderTypedValue
{
	GENERATED_USTRUCT_BODY()
	FAIDataProviderIntValue();

	UPROPERTY(EditAnywhere, Category = Value)
	int32 DefaultValue;

	int32 GetValue() const;
	virtual FString ValueToString() const override;
};

USTRUCT()
struct AIMODULE_API FAIDataProviderFloatValue : public FAIDataProviderTypedValue
{
	GENERATED_USTRUCT_BODY()
	FAIDataProviderFloatValue();

	UPROPERTY(EditAnywhere, Category = Value)
	float DefaultValue;

	float GetValue() const;
	virtual FString ValueToString() const override;
};

USTRUCT()
struct AIMODULE_API FAIDataProviderBoolValue : public FAIDataProviderTypedValue
{
	GENERATED_USTRUCT_BODY()
	FAIDataProviderBoolValue();

	UPROPERTY(EditAnywhere, Category = Value)
	bool DefaultValue;

	bool GetValue() const;
	virtual FString ValueToString() const override;
};

UCLASS(EditInlineNew, Abstract, CollapseCategories, AutoExpandCategories=(Provider))
class AIMODULE_API UAIDataProvider : public UObject
{
	GENERATED_UCLASS_BODY()

	virtual void BindData(const UObject& Owner, int32 RequestId);
	virtual FString ToString(FName PropName) const;
};
