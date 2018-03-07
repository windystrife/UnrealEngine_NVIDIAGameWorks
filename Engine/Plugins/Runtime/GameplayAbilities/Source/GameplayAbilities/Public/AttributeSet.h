// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/Class.h"
#include "Templates/SubclassOf.h"
#include "UObject/UnrealType.h"
#include "GameFramework/Actor.h"
#include "Engine/CurveTable.h"
#include "Engine/DataTable.h"
#include "AttributeSet.generated.h"

class UAbilitySystemComponent;
class UAttributeSet;
struct FGameplayAbilityActorInfo;

USTRUCT(BlueprintType)
struct GAMEPLAYABILITIES_API FGameplayAttributeData
{
	GENERATED_BODY()
	FGameplayAttributeData()
		: BaseValue(0.f)
		, CurrentValue(0.f)
	{}

	FGameplayAttributeData(float DefaultValue)
		: BaseValue(DefaultValue)
		, CurrentValue(DefaultValue)
	{}

	virtual ~FGameplayAttributeData()
	{}

	float GetCurrentValue() const;
	virtual void SetCurrentValue(float NewValue);

	float GetBaseValue() const;
	virtual void SetBaseValue(float NewValue);

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Attribute")
	float BaseValue;

	UPROPERTY(BlueprintReadOnly, Category = "Attribute")
	float CurrentValue;
};

USTRUCT(BlueprintType)
struct GAMEPLAYABILITIES_API FGameplayAttribute
{
	GENERATED_USTRUCT_BODY()

	FGameplayAttribute()
		: Attribute(nullptr)
		, AttributeOwner(nullptr)
	{
	}

	FGameplayAttribute(UProperty *NewProperty);

	bool IsValid() const
	{
		return Attribute != nullptr;
	}

	void SetUProperty(UProperty *NewProperty)
	{
		Attribute = NewProperty;
		if (NewProperty)
		{
			AttributeOwner = Attribute->GetOwnerStruct();
			Attribute->GetName(AttributeName);
		}
		else
		{
			AttributeOwner = nullptr;
			AttributeName.Empty();
		}
	}

	UProperty* GetUProperty() const
	{
		return Attribute;
	}

	UClass* GetAttributeSetClass() const
	{
		check(Attribute);
		return CastChecked<UClass>(Attribute->GetOuter());
	}

	bool IsSystemAttribute() const;

	/** Returns true if the variable associated with Property is of type FGameplayAttributeData or one of its subclasses */
	static bool IsGameplayAttributeDataProperty(const UProperty* Property);

	void SetNumericValueChecked(float& NewValue, class UAttributeSet* Dest) const;

	float GetNumericValue(const UAttributeSet* Src) const;
	float GetNumericValueChecked(const UAttributeSet* Src) const;

	FGameplayAttributeData* GetGameplayAttributeData(UAttributeSet* Src) const;
	FGameplayAttributeData* GetGameplayAttributeDataChecked(UAttributeSet* Src) const;
	
	/** Equality/Inequality operators */
	bool operator==(const FGameplayAttribute& Other) const;
	bool operator!=(const FGameplayAttribute& Other) const;

	friend uint32 GetTypeHash( const FGameplayAttribute& InAttribute )
	{
		// FIXME: Use ObjectID or something to get a better, less collision prone hash
		return PointerHash(InAttribute.Attribute);
	}

	FString GetName() const
	{
		return AttributeName.IsEmpty() ? *GetNameSafe(Attribute) : AttributeName;
	}

	void PostSerialize(const FArchive& Ar);

	UPROPERTY(Category = GameplayAttribute, VisibleAnywhere, BlueprintReadOnly)
	FString AttributeName;

	// In editor, this will filter out properties with meta tag "HideInDetailsView" or equal to FilterMetaStr. In non editor, it returns all properties.
	static void GetAllAttributeProperties(TArray<UProperty*>& OutProperties, FString FilterMetaStr=FString(), bool UseEditorOnlyData=true);

private:
	friend class FAttributePropertyDetails;

	UPROPERTY(Category=GameplayAttribute, EditAnywhere)
	UProperty*	Attribute;

	UPROPERTY(Category = GameplayAttribute, VisibleAnywhere)
	UStruct* AttributeOwner;
};

template<>
struct TStructOpsTypeTraits< FGameplayAttribute > : public TStructOpsTypeTraitsBase2< FGameplayAttribute >
{
	enum
	{
		WithPostSerialize = true,
	};
};

UCLASS(DefaultToInstanced, Blueprintable)
class GAMEPLAYABILITIES_API UAttributeSet : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	bool IsNameStableForNetworking() const override;

	bool IsSupportedForNetworking() const override
	{
		return true;
	}

	virtual bool ShouldInitProperty(bool FirstInit, UProperty* PropertyToInit) const { return true; }

	/**
	 *	Called just before modifying the value of an attribute. AttributeSet can make additional modifications here. Return true to continue, or false to throw out the modification.
	 *	Note this is only called during an 'execute'. E.g., a modification to the 'base value' of an attribute. It is not called during an application of a GameplayEffect, such as a 5 ssecond +10 movement speed buff.
	 */	
	virtual bool PreGameplayEffectExecute(struct FGameplayEffectModCallbackData &Data) { return true; }
	
	
	/**
	 *	Called just before a GameplayEffect is executed to modify the base value of an attribute. No more changes can be made.
	 *	Note this is only called during an 'execute'. E.g., a modification to the 'base value' of an attribute. It is not called during an application of a GameplayEffect, such as a 5 ssecond +10 movement speed buff.
	 */
	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData &Data) { }

	/**
	 *	An "On Aggregator Change" type of event could go here, and that could be called when active gameplay effects are added or removed to an attribute aggregator.
	 *	It is difficult to give all the information in these cases though - aggregators can change for many reasons: being added, being removed, being modified, having a modifier change, immunity, stacking rules, etc.
	 */


	/**
	 *	Called just before any modification happens to an attribute. This is lower level than PreAttributeModify/PostAttribute modify.
	 *	There is no additional context provided here since anything can trigger this. Executed effects, duration based effects, effects being removed, immunity being applied, stacking rules changing, etc.
	 *	This function is meant to enforce things like "Health = Clamp(Health, 0, MaxHealth)" and NOT things like "trigger this extra thing if damage is applied, etc".
	 *	
	 *	NewValue is a mutable reference so you are able to clamp the newly applied value as well.
	 */
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) { }

	/**
	 *	This is called just before any modification happens to an attribute's base value when an attribute aggregator exists.
	 *	This function should enforce clamping (presuming you wish to clamp the base value along with the final value in PreAttributeChange)
	 *	This function should NOT invoke gameplay related events or callbacks. Do those in PreAttributeChange() which will be called prior to the
	 *	final value of the attribute actually changing.
	 */
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const { }

	/** This signifies the attribute set can be ID'd by name over the network. */
	void SetNetAddressable();

	void InitFromMetaDataTable(const UDataTable* DataTable);

	FORCEINLINE AActor* GetOwningActor() const { return CastChecked<AActor>(GetOuter()); }
	UAbilitySystemComponent* GetOwningAbilitySystemComponent() const;
	FGameplayAbilityActorInfo* GetActorInfo() const;

	virtual void PrintDebug();

	virtual void PreNetReceive() override;
	
	virtual void PostNetReceive() override;

protected:
	/** Is this attribute set safe to ID over the network by name?  */
	uint32 bNetAddressable : 1;
};

USTRUCT()
struct GAMEPLAYABILITIES_API FGlobalCurveDataOverride
{
	GENERATED_USTRUCT_BODY()

	TArray<UCurveTable*>	Overrides;
};


/**
 *	Generic numerical value in the form Coeffecient * Curve[Level] 
 */
USTRUCT(BlueprintType)
struct GAMEPLAYABILITIES_API FScalableFloat
{
	GENERATED_USTRUCT_BODY()

	FScalableFloat()
		: Value(0.f)
		, FinalCurve(nullptr)
		, LocalCachedCurveID(INDEX_NONE)
	{
	}

	FScalableFloat(float InInitialValue)
		: Value(InInitialValue)
		, FinalCurve(nullptr)
		, LocalCachedCurveID(INDEX_NONE)
	{
	}

	~FScalableFloat()
	{
	}

public:

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category=ScalableFloat)
	float	Value;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category=ScalableFloat)
	FCurveTableRowHandle	Curve;

	float GetValueAtLevel(float Level, const FString* ContextString = nullptr) const;

	bool IsStatic() const
	{
		return Curve.RowName.IsNone();
	}

	void SetValue(float NewValue);

	void SetScalingValue(float InCoeffecient, FName InRowName, UCurveTable * InTable);

	void LockValueAtLevel(float Level, FGlobalCurveDataOverride *GlobalOverrides, const FString* ContextString = nullptr)
	{
		SetValue(GetValueAtLevel(Level, ContextString));
	}

	float GetValueChecked() const
	{
		check(IsStatic());
		return Value;
	}

	FString ToSimpleString() const
	{
		if (Curve.RowName != NAME_None)
		{
			return FString::Printf(TEXT("%.2f - %s@%s"), Value, *Curve.RowName.ToString(), Curve.CurveTable ? *Curve.CurveTable->GetName() : TEXT("None"));
		}
		return FString::Printf(TEXT("%.2f"), Value);
	}

	bool IsValid() const
	{
		// Error checking: checks if we have a curve table specified but no valid curve entry
		static const FString ContextString = TEXT("FScalableFloat::IsValid");
		GetValueAtLevel(1.f, &ContextString);
		bool bInvalid = (Curve.CurveTable != nullptr || Curve.RowName != NAME_None ) && (FinalCurve == nullptr);
		return !bInvalid;
	}

	/** Equality/Inequality operators */
	bool operator==(const FScalableFloat& Other) const;
	bool operator!=(const FScalableFloat& Other) const;

	//copy operator to prevent duplicate handles
	void operator=(const FScalableFloat& Src);

	/* Used to upgrade a float or int8/int16/int32 property into an FScalableFloat */
	bool SerializeFromMismatchedTag(const FPropertyTag& Tag, FArchive& Ar);

	static void InvalidateAllCachedCurves();

private:

	// Cached direct pointer to RichCurve we should evaluate
	mutable FRichCurve* FinalCurve;
	mutable int32 LocalCachedCurveID;

	static int32 GlobalCachedCurveID;
};

template<>
struct TStructOpsTypeTraits<FScalableFloat>
	: public TStructOpsTypeTraitsBase2<FScalableFloat>
{
	enum
	{
		WithSerializeFromMismatchedTag = true,
	};
};


/**
 *	DataTable that allows us to define meta data about attributes. Still a work in progress.
 */
USTRUCT(BlueprintType)
struct GAMEPLAYABILITIES_API FAttributeMetaData : public FTableRowBase
{
	GENERATED_USTRUCT_BODY()

public:

	FAttributeMetaData();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gameplay Attribute")
	float		BaseValue;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gameplay Attribute")
	float		MinValue;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gameplay Attribute")
	float		MaxValue;

	UPROPERTY()
	FString		DerivedAttributeInfo;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gameplay Attribute")
	bool		bCanStack;
};

/**
 *	Helper struct that facilitates initializing attribute set default values from spread sheets (UCurveTable).
 *	Projects are free to initialize their attribute sets however they want. This is just want example that is 
 *	useful in some cases.
 *	
 *	Basic idea is to have a spreadsheet in this form: 
 *	
 *									1	2	3	4	5	6	7	8	9	10	11	12	13	14	15	16	17	18	19	20
 *
 *	Default.Health.MaxHealth		100	200	300	400	500	600	700	800	900	999	999	999	999	999	999	999	999	999	999	999
 *	Default.Health.HealthRegenRate	1	1	1	1	1	1	1	1	1	1	1	1	1	1	1	1	1	1	1	1
 *	Default.Health.AttackRating		10	10	10	10	10	10	10	10	10	10	10	10	10	10	10	10	10	10	10	10
 *	Default.Move.MaxMoveSpeed		500	500	500	500	500	500	500	500	500	500	500	500	500	500	500	500	500	500	500	500
 *	Hero1.Health.MaxHealth			100	100	100	100	100	100	100	100	100	100	100	100	100	100	100	100	100	100	100	100
 *	Hero1.Health.HealthRegenRate	1	1	1	1	1	1	1	1	1	1	1	1	1	1	1	1 	1	1	1	1
 *	Hero1.Health.AttackRating		10	10	10	10	10	10	10	10	10	10	10	10	10	10	10	10	10	10	10	10
 *	Hero1.Move.MaxMoveSpeed			500	500	500	500	500	500	500	500	500	500	500	500	500	500	500	500	500	500	500	500
 *	
 *	
 *	Where rows are in the form: [GroupName].[AttributeSetName].[Attribute]
 *	GroupName			- arbitrary name to identify the "group"
 *	AttributeSetName	- what UAttributeSet the attributes belong to. (Note that this is a simple partial match on the UClass name. "Health" matches "UMyGameHealthSet").
 *	Attribute			- the name of the actual attribute property (matches full name).
 *		
 *	Columns represent "Level". 
 *	
 *	FAttributeSetInitter::PreloadAttributeSetData(UCurveTable*)
 *	This transforms the CurveTable into a more efficient format to read in at run time. Should be called from UAbilitySystemGlobals for example.
 *
 *	FAttributeSetInitter::InitAttributeSetDefaults(UAbilitySystemComponent* AbilitySystemComponent, FName GroupName, int32 Level) const;
 *	This initializes the given AbilitySystemComponent's attribute sets with the specified GroupName and Level. Game code would be expected to call
 *	this when spawning a new Actor, or leveling up an actor, etc.
 *	
 *	Example Game code usage:
 *	
 *	IGameplayAbilitiesModule::Get().GetAbilitySystemGlobals()->GetAttributeSetInitter()->InitAttributeSetDefaults(MyCharacter->AbilitySystemComponent, "Hero1", MyLevel);
 *	
 *	Notes:
 *	-This lets system designers specify arbitrary values for attributes. They can be based on any formula they want.
 *	-Projects with very large level caps may wish to take a simpler "Attributes gained per level" approach.
 *	-Anything initialized in this method should not be directly modified by gameplay effects. E.g., if MaxMoveSpeed scales with level, anything else that 
 *		modifies MaxMoveSpeed should do so with a non-instant GameplayEffect.
 *	-"Default" is currently the hardcoded, fallback GroupName. If InitAttributeSetDefaults is called without a valid GroupName, we will fallback to default.
 *
 */
struct GAMEPLAYABILITIES_API FAttributeSetInitter
{
	virtual ~FAttributeSetInitter() {}

	virtual void PreloadAttributeSetData(const TArray<UCurveTable*>& CurveData) = 0;
	virtual void InitAttributeSetDefaults(UAbilitySystemComponent* AbilitySystemComponent, FName GroupName, int32 Level, bool bInitialInit) const = 0;
	virtual void ApplyAttributeDefault(UAbilitySystemComponent* AbilitySystemComponent, FGameplayAttribute& InAttribute, FName GroupName, int32 Level) const = 0;
	virtual TArray<float> GetAttributeSetValues(UClass* AttributeSetClass, UProperty* AttributeProperty, FName GroupName) const { return TArray<float>(); }
};

/** Explicit implementation of attribute set initter, relying on the existence and usage of discrete levels for data look-up (that is, CurveTable->Eval is not possible) */
struct GAMEPLAYABILITIES_API FAttributeSetInitterDiscreteLevels : public FAttributeSetInitter
{
	virtual void PreloadAttributeSetData(const TArray<UCurveTable*>& CurveData) override;

	virtual void InitAttributeSetDefaults(UAbilitySystemComponent* AbilitySystemComponent, FName GroupName, int32 Level, bool bInitialInit) const override;
	virtual void ApplyAttributeDefault(UAbilitySystemComponent* AbilitySystemComponent, FGameplayAttribute& InAttribute, FName GroupName, int32 Level) const override;

	virtual TArray<float> GetAttributeSetValues(UClass* AttributeSetClass, UProperty* AttributeProperty, FName GroupName) const override;
private:

	bool IsSupportedProperty(UProperty* Property) const;

	struct FAttributeDefaultValueList
	{
		void AddPair(UProperty* InProperty, float InValue)
		{
			List.Add(FOffsetValuePair(InProperty, InValue));
		}

		struct FOffsetValuePair
		{
			FOffsetValuePair(UProperty* InProperty, float InValue)
			: Property(InProperty), Value(InValue) { }

			UProperty*	Property;
			float		Value;
		};

		TArray<FOffsetValuePair>	List;
	};

	struct FAttributeSetDefaults
	{
		TMap<TSubclassOf<UAttributeSet>, FAttributeDefaultValueList> DataMap;
	};

	struct FAttributeSetDefaultsCollection
	{
		TArray<FAttributeSetDefaults>		LevelData;
	};

	TMap<FName, FAttributeSetDefaultsCollection>	Defaults;
};

/**
 *	This is a helper macro that can be used in RepNotify functions to handle attributes that will be predictively modified by clients.
 *	
 *	void UMyHealthSet::OnRep_Health()
 *	{
 *		GAMEPLAYATTRIBUTE_REPNOTIFY(UMyHealthSet, Health);
 *	}
 */

#define GAMEPLAYATTRIBUTE_REPNOTIFY(C, P) \
{ \
	static UProperty* ThisProperty = FindFieldChecked<UProperty>(C::StaticClass(), GET_MEMBER_NAME_CHECKED(C, P)); \
	GetOwningAbilitySystemComponent()->SetBaseAttributeValueFromReplication(P, FGameplayAttribute(ThisProperty)); \
}
