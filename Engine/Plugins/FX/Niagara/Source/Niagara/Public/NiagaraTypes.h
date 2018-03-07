// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/UnrealType.h"
#include "Engine/UserDefinedStruct.h"
#include "NiagaraTypes.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogNiagara, Log, Verbose);

// basic type struct definitions

USTRUCT(meta = (DisplayName = "float"))
struct FNiagaraFloat
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=Parameters)//Parameters? These are used for attrs too.
	float Value;
};

USTRUCT(meta = (DisplayName = "int32"))
struct FNiagaraInt32
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = Parameters)//Parameters? These are used for attrs too.
	int32 Value;
};

USTRUCT(meta=(DisplayName="bool"))
struct FNiagaraBool
{
	GENERATED_USTRUCT_BODY()

		// The Niagara VM expects this bitmask for its compare and select operators for false.
	enum BoolValues { 
		True = INDEX_NONE,
		False = 0
	}; 

	

	void SetValue(bool bValue) { Value = bValue ? True : False; }
	bool GetValue() const { return Value != False; }

	bool IsValid() const { return Value == True || Value == False; }
	
	FNiagaraBool():Value(False) {}
	FNiagaraBool(bool bInValue) : Value(bInValue ? True : False) {}
	FORCEINLINE operator bool() { return GetValue(); }

	UPROPERTY(EditAnywhere, Category = Parameters)//Parameters? These are used for attrs too. Must be either FNiagaraBool::True or FNiagaraBool::False.
	int32 Value;
};

USTRUCT()
struct FNiagaraNumeric
{
	GENERATED_USTRUCT_BODY()
};


USTRUCT()
struct FNiagaraParameterMap
{
	GENERATED_USTRUCT_BODY()
};

USTRUCT()
struct FNiagaraTestStructInner
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = TestStruct)
	FVector InnerVector1;

	UPROPERTY(EditAnywhere, Category = TestStruct)
	FVector InnerVector2;
};

USTRUCT()
struct FNiagaraTestStruct
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = TestStruct)
	FVector Vector1;

	UPROPERTY(EditAnywhere, Category = TestStruct)
	FVector Vector2;

	UPROPERTY(EditAnywhere, Category = TestStruct)
	FNiagaraTestStructInner InnerStruct1;

	UPROPERTY(EditAnywhere, Category = TestStruct)
	FNiagaraTestStructInner InnerStruct2;
};

USTRUCT(meta = (DisplayName = "Matrix"))
struct FNiagaraMatrix
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=NiagaraMatrix)
	FVector4 Row0;

	UPROPERTY(EditAnywhere, Category = NiagaraMatrix)
	FVector4 Row1;

	UPROPERTY(EditAnywhere, Category = NiagaraMatrix)
	FVector4 Row2;

	UPROPERTY(EditAnywhere, Category = NiagaraMatrix)
	FVector4 Row3;
};

/** Data controlling the spawning of particles */
USTRUCT(meta = (DisplayName = "Spawn Info"))
struct FNiagaraSpawnInfo
{
	GENERATED_USTRUCT_BODY();
	
	FNiagaraSpawnInfo()
		: Count(0)
		, InterpStartDt(0.0f)
		, IntervalDt(1.0f)
	{}

	/** How many particles to spawn. */
	UPROPERTY(EditAnywhere, Category = SpawnInfo)
	int32 Count;
	/** The sub frame delta time at which to spawn the first particle. */
	UPROPERTY(EditAnywhere, Category = SpawnInfo)
	float InterpStartDt;
	/** The sub frame delta time between each particle. */
	UPROPERTY(EditAnywhere, Category = SpawnInfo)
	float IntervalDt;
};

/** Information about how this type should be laid out in an FNiagaraDataSet */
struct FNiagaraTypeLayoutInfo
{
	FNiagaraTypeLayoutInfo()
	{}

	/** Byte offset of each float component in a structured layout. */
	TArray<uint32> FloatComponentByteOffsets;
	/** Offset into register table for each float component. */
	TArray<uint32> FloatComponentRegisterOffsets;

	/** Byte offset of each int32 component in a structured layout. */
	TArray<uint32> Int32ComponentByteOffsets;
	/** Offset into register table for each int32 component. */
	TArray<uint32> Int32ComponentRegisterOffsets;

	FORCEINLINE uint32 GetNumComponents()const { return FloatComponentByteOffsets.Num() + Int32ComponentByteOffsets.Num(); }

	static void GenerateLayoutInfo(FNiagaraTypeLayoutInfo& Layout, const UScriptStruct* Struct)
	{
		Layout.FloatComponentByteOffsets.Empty();
		Layout.FloatComponentRegisterOffsets.Empty();
		Layout.Int32ComponentByteOffsets.Empty();
		Layout.Int32ComponentRegisterOffsets.Empty();
		GenerateLayoutInfoInternal(Layout, Struct);
	}

private:
	static void GenerateLayoutInfoInternal(FNiagaraTypeLayoutInfo& Layout, const UScriptStruct* Struct)
	{
		for (TFieldIterator<UProperty> PropertyIt(Struct, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
		{
			UProperty* Property = *PropertyIt;
			if (Property->IsA(UFloatProperty::StaticClass()))
			{
				Layout.FloatComponentRegisterOffsets.Add(Layout.GetNumComponents());
				Layout.FloatComponentByteOffsets.Add(Property->GetOffset_ForInternal());
			}
			else if (Property->IsA(UIntProperty::StaticClass()) || Property->IsA(UBoolProperty::StaticClass()))
			{
				Layout.Int32ComponentRegisterOffsets.Add(Layout.GetNumComponents());
				Layout.Int32ComponentByteOffsets.Add(Property->GetOffset_ForInternal());
			}
			//Should be able to support double easily enough
			else if (UStructProperty* StructProp = CastChecked<UStructProperty>(Property))
			{
				GenerateLayoutInfoInternal(Layout, StructProp->Struct);
			}
			else
			{
				check(false);
			}
		}
	}
};

/*
*  Can convert a UStruct with fields of base types only (float, int... - will likely add native vector types here as well)
*	to an FNiagaraTypeDefinition (internal representation)
*/
class NIAGARA_API FNiagaraTypeHelper
{
public:
	static FString ToString(const uint8* ValueData, const UScriptStruct* Struct);
};

/** Defines different modes for selecting the output numeric type of a function or operation based on the types of the inputs. */
UENUM()
enum class ENiagaraNumericOutputTypeSelectionMode : uint8
{
	/** Output type selection not supported. */
	None UMETA(Hidden),
	/** Select the largest of the numeric inputs. */
	Largest,
	/** Select the smallest of the numeric inputs. */
	Smallest,
	/** Selects the base scalar type for this numeric inputs. */
	Scalar,
};

UENUM()
enum class ENiagaraExecutionState : uint32
{
	/**  Run all scripts. Allow spawning.*/
	Active,
	/** Run all scripts but suppress any new spawning.*/
	Inactive,
	/** Scripts still run but all ticking of emitters is halted.*/
	Paused,
	/** Kill immediately.*/
	Kill,
	/** Dead*/
	Dead,
	/** Internal use only*/
	Disabled UMETA(Hidden),
};

USTRUCT()
struct NIAGARA_API FNiagaraTypeDefinition
{
	GENERATED_USTRUCT_BODY()
public:

	// Construct blank raw type definition 
	FNiagaraTypeDefinition(const UClass *ClassDef)
		: Struct(ClassDef), Enum(nullptr)
	{
		check(Struct != nullptr);
	}

	FNiagaraTypeDefinition(const UEnum *EnumDef)
		: Struct(IntStruct), Enum(EnumDef)
	{
		check(Struct != nullptr);
	}

	FNiagaraTypeDefinition(const UScriptStruct *StructDef)
		: Struct(StructDef), Enum(nullptr)
	{
		check(Struct != nullptr);
	}

	FNiagaraTypeDefinition(const FNiagaraTypeDefinition &Other)
		: Struct(Other.Struct), Enum(Other.Enum)
	{
	}

	// Construct a blank raw type definition
	FNiagaraTypeDefinition()
		: Struct(nullptr), Enum(nullptr)
	{}

	bool operator !=(const FNiagaraTypeDefinition &Other) const
	{
		return !(*this == Other);
	}

	bool operator == (const FNiagaraTypeDefinition &Other) const
	{
		return Struct == Other.Struct && Enum == Other.Enum;
	}

	FText GetNameText()const
	{
		checkf(IsValid(), TEXT("Type definition is not valid."));
		if (Enum != nullptr)
		{
			return  FText::FromString(Enum->GetName());
		}

#if WITH_EDITOR
		return GetStruct()->GetDisplayNameText();
#else
		return FText::FromString( GetStruct()->GetName() );
#endif
	}

	FString GetName()const
	{
		checkf(IsValid(), TEXT("Type definition is not valid."));
		if (Enum != nullptr)
		{
			return  Enum->GetName();
		}
		return GetStruct()->GetName();
	}

	const UStruct* GetStruct()const
	{
		return Struct;
	}

	const UScriptStruct* GetScriptStruct()const
	{
		return Cast<UScriptStruct>(Struct);
	}
		
	/** Gets the class ptr for this type if it is a class. */
	const UClass* GetClass()const
	{
		return Cast<UClass>(Struct);
	}

	const UEnum* GetEnum() const
	{
		return Enum;
	}

	bool IsDataInterface()const;

	bool IsEnum() const { return Enum != nullptr; }
	
	int32 GetSize()const
	{
		checkf(IsValid(), TEXT("Type definition is not valid."));
		if (GetClass())
		{
			return 0;//TODO: sizeof(void*);//If we're a class then we allocate space for the user to instantiate it. This and stopping it being GCd is up to the user.
		}
		else
		{
			return CastChecked<UScriptStruct>(Struct)->GetStructureSize();	// TODO: cache this here?
		}
	}

	int32 GetAlignment()const
	{
		checkf(IsValid(), TEXT("Type definition is not valid."));
		if (GetClass())
		{
			return 0;//TODO: sizeof(void*);//If we're a class then we allocate space for the user to instantiate it. This and stopping it being GCd is up to the user.
		}
		else
		{
			return CastChecked<UScriptStruct>(Struct)->GetMinAlignment();
		}
	}

	bool IsValid() const 
	{ 
		return Struct != nullptr;
	}

	/**
	UStruct specifying the type for this variable.
	For most types this will be a UScriptStruct pointing to a something like the struct for an FVector etc.
	In occasional situations this may be a UClass when we're dealing with DataInterface etc.
	*/
	UPROPERTY()
	const UStruct *Struct;

	UPROPERTY()
	const UEnum* Enum;

	static void Init();
	static void RecreateUserDefinedTypeRegistry();
	static const FNiagaraTypeDefinition& GetFloatDef() { return FloatDef; }
	static const FNiagaraTypeDefinition& GetBoolDef() { return BoolDef; }
	static const FNiagaraTypeDefinition& GetIntDef() { return IntDef; }
	static const FNiagaraTypeDefinition& GetVec2Def() { return Vec2Def; }
	static const FNiagaraTypeDefinition& GetVec3Def() { return Vec3Def; }
	static const FNiagaraTypeDefinition& GetVec4Def() { return Vec4Def; }
	static const FNiagaraTypeDefinition& GetColorDef() { return ColorDef; }
	static const FNiagaraTypeDefinition& GetMatrix4Def() { return Matrix4Def; }
	static const FNiagaraTypeDefinition& GetGenericNumericDef() { return NumericDef; }
	static const FNiagaraTypeDefinition& GetParameterMapDef() { return ParameterMapDef; }

	static const UScriptStruct* GetFloatStruct() { return FloatStruct; }
	static const UScriptStruct* GetBoolStruct() { return BoolStruct; }
	static const UScriptStruct* GetIntStruct() { return IntStruct; }
	static const UScriptStruct* GetVec2Struct() { return Vec2Struct; }
	static const UScriptStruct* GetVec3Struct() { return Vec3Struct; }
	static const UScriptStruct* GetVec4Struct() { return Vec4Struct; }
	static const UScriptStruct* GetColorStruct() { return ColorStruct; }
	static const UScriptStruct* GetMatrix4Struct() { return Matrix4Struct; }
	static const UScriptStruct* GetGenericNumericStruct() { return NumericStruct; }
	static const UScriptStruct* GetParameterMapStruct() { return ParameterMapStruct; }

	static const UEnum* GetExecutionStateEnum() { return ExecutionStateEnum; }

	static const FNiagaraTypeDefinition& GetCollisionEventDef() { return CollisionEventDef; }

	static bool IsScalarDefinition(const FNiagaraTypeDefinition& Type);

	FString ToString(const uint8* ValueData)const
	{
		checkf(IsValid(), TEXT("Type definition is not valid."));
		return FNiagaraTypeHelper::ToString(ValueData, CastChecked<UScriptStruct>(Struct));
	}

	static bool TypesAreAssignable(const FNiagaraTypeDefinition& TypeA, const FNiagaraTypeDefinition& TypeB);
	static bool IsLossyConversion(const FNiagaraTypeDefinition& TypeA, const FNiagaraTypeDefinition& TypeB);
	static FNiagaraTypeDefinition GetNumericOutputType(const TArray<FNiagaraTypeDefinition> TypeDefinintions, ENiagaraNumericOutputTypeSelectionMode SelectionMode);

	static const TArray<FNiagaraTypeDefinition>& GetNumericTypes() { return OrderedNumericTypes; }
	static bool IsValidNumericInput(const FNiagaraTypeDefinition& TypeDef);
private:

	static FNiagaraTypeDefinition FloatDef;
	static FNiagaraTypeDefinition BoolDef;
	static FNiagaraTypeDefinition IntDef;
	static FNiagaraTypeDefinition Vec2Def;
	static FNiagaraTypeDefinition Vec3Def;
	static FNiagaraTypeDefinition Vec4Def;
	static FNiagaraTypeDefinition ColorDef;
	static FNiagaraTypeDefinition Matrix4Def;
	static FNiagaraTypeDefinition NumericDef;
	static FNiagaraTypeDefinition ParameterMapDef;

	static const UScriptStruct* FloatStruct;
	static const UScriptStruct* BoolStruct;
	static const UScriptStruct* IntStruct;
	static const UScriptStruct* Vec2Struct;
	static const UScriptStruct* Vec3Struct;
	static const UScriptStruct* Vec4Struct;
	static const UScriptStruct* ColorStruct;
	static const UScriptStruct* Matrix4Struct;
	static const UScriptStruct* NumericStruct;

	static const UEnum* ExecutionStateEnum;

	static const UScriptStruct* ParameterMapStruct;

	static TSet<const UScriptStruct*> NumericStructs;
	static TArray<FNiagaraTypeDefinition> OrderedNumericTypes;

	static TSet<const UScriptStruct*> ScalarStructs;

	static TSet<const UStruct*> FloatStructs;
	static TSet<const UStruct*> IntStructs;
	static TSet<const UStruct*> BoolStructs;

	static FNiagaraTypeDefinition CollisionEventDef;

};

/* Contains all types currently available for use in Niagara
* Used by UI to provide selection; new uniforms and variables
* may be instanced using the types provided here
*/
class NIAGARA_API FNiagaraTypeRegistry
{
public:
	static const TArray<FNiagaraTypeDefinition> &GetRegisteredTypes()
	{
		return RegisteredTypes;
	}

	static const TArray<FNiagaraTypeDefinition> &GetRegisteredParameterTypes()
	{
		return RegisteredParamTypes;
	}

	static const TArray<FNiagaraTypeDefinition> &GetRegisteredPayloadTypes()
	{
		return RegisteredPayloadTypes;
	}

	static const TArray<FNiagaraTypeDefinition>& GetUserDefinedTypes()
	{
		return RegisteredUserDefinedTypes;
	}

	static const TArray<FNiagaraTypeDefinition>& GetNumericTypes()
	{ 
		return RegisteredNumericTypes;
	}

	static void ClearUserDefinedRegistry()
	{
		for (const FNiagaraTypeDefinition& Def : RegisteredUserDefinedTypes)
		{
			RegisteredTypes.Remove(Def);
			RegisteredPayloadTypes.Remove(Def);
			RegisteredParamTypes.Remove(Def);
		}

		RegisteredNumericTypes.Empty();
		RegisteredUserDefinedTypes.Empty();
	}

	static void Register(const FNiagaraTypeDefinition &NewType, bool bCanBeParameter, bool bCanBePayload, bool bIsUserDefined)
	{
		//TODO: Make this a map of type to a more verbose set of metadata? Such as the hlsl defs, offset table for conversions etc.
		RegisteredTypes.AddUnique(NewType);

		if (bCanBeParameter)
		{
			RegisteredParamTypes.AddUnique(NewType);
		}

		if (bCanBePayload)
		{
			RegisteredPayloadTypes.AddUnique(NewType);
		}

		if (bIsUserDefined)
		{
			RegisteredUserDefinedTypes.AddUnique(NewType);
		}

		if (FNiagaraTypeDefinition::IsValidNumericInput(NewType))
		{
			RegisteredNumericTypes.AddUnique(NewType);
		}
	}

private:
	static TArray<FNiagaraTypeDefinition> RegisteredTypes;
	static TArray<FNiagaraTypeDefinition> RegisteredParamTypes;
	static TArray<FNiagaraTypeDefinition> RegisteredPayloadTypes;
	static TArray<FNiagaraTypeDefinition> RegisteredUserDefinedTypes;
	static TArray<FNiagaraTypeDefinition> RegisteredNumericTypes;
};

FORCEINLINE uint32 GetTypeHash(const FNiagaraTypeDefinition& Type)
{
	return HashCombine(GetTypeHash(Type.GetStruct()), GetTypeHash(Type.GetEnum()));
}

//////////////////////////////////////////////////////////////////////////

USTRUCT()
struct FNiagaraVariable
{
	GENERATED_USTRUCT_BODY()

	FNiagaraVariable()
		: Name(NAME_None)
		, TypeDef(FNiagaraTypeDefinition::GetVec4Def())
	{
	}

	FNiagaraVariable(const FNiagaraVariable &Other)
		: Name(Other.Name)
		, TypeDef(Other.TypeDef)
	{
		if (Other.IsDataAllocated())
		{
			SetData(Other.GetData());
		}
	}

	FNiagaraVariable(FNiagaraTypeDefinition InType, FName InName)
		: Name(InName)
		, TypeDef(InType)
	{
	}
	
	bool operator==(const FNiagaraVariable& Other)const
	{
		return Name == Other.Name && TypeDef == Other.TypeDef;
	}

	bool operator!=(const FNiagaraVariable& Other)const
	{
		return !(*this == Other);
	}

	/** Variables are the same name but if types are auto-assignable, allow them to match. */
	bool IsEquivalent(const FNiagaraVariable& Other, bool bAllowAssignableTypes = true)const
	{
		return Name == Other.Name && (TypeDef == Other.TypeDef || (bAllowAssignableTypes && FNiagaraTypeDefinition::TypesAreAssignable(TypeDef, Other.TypeDef)));
	}
	
	void SetName(FName InName) { Name = InName; }
	FName GetName()const { return Name; }

	void SetType(const FNiagaraTypeDefinition& InTypeDef) { TypeDef = InTypeDef; }
	const FNiagaraTypeDefinition& GetType()const { return TypeDef; }

	FORCEINLINE bool IsDataInterface()const { return GetType().IsDataInterface(); }

	void AllocateData()
	{
		if (VarData.Num() != TypeDef.GetSize())
		{
			VarData.SetNumZeroed(TypeDef.GetSize());
		}
	}

	bool IsDataAllocated()const { return VarData.Num() > 0 && VarData.Num() == TypeDef.GetSize(); }

	void CopyTo(uint8* Dest) const
	{
		check(TypeDef.GetSize() == VarData.Num());
		check(IsDataAllocated());
		FMemory::Memcpy(Dest, VarData.GetData(), VarData.Num());
	}
		
	template<typename T>
	void SetValue(const T& Data)
	{
		check(sizeof(T) == TypeDef.GetSize());
		AllocateData();
		FMemory::Memcpy(VarData.GetData(), &Data, VarData.Num());
	}

	template<typename T>
	T* GetValue()
	{
		check(sizeof(T) == TypeDef.GetSize());
		check(IsDataAllocated());
		return (T*)GetData();
	}

	template<typename T>
	const T* GetValue() const
	{
		check(sizeof(T) == TypeDef.GetSize());
		check(IsDataAllocated());
		return (T*)GetData();
	}

	void SetData(const uint8* Data)
	{
		check(Data);
		AllocateData();
		FMemory::Memcpy(VarData.GetData(), Data, VarData.Num());
	}

	const uint8* GetData() const
	{
		return VarData.GetData();
	}

	uint8* GetData()
	{
		return VarData.GetData();
	}

	int32 GetSizeInBytes() const
	{
		return TypeDef.GetSize();
	}

	int32 GetAlignment()const
	{
		return TypeDef.GetAlignment();
	}

	int32 GetAllocatedSizeInBytes() const
	{
		return VarData.Num();
	}

	FString ToString()const
	{
		FString Ret = Name.ToString() + TEXT("(");
		Ret += TypeDef.ToString(VarData.GetData());
		Ret += TEXT(")");
		return Ret;
	}

	bool IsNameValid() const
	{
		return Name != NAME_None;
	}

	FORCEINLINE bool IsInNameSpace(FString Namespace) 
	{
		return Name.ToString().StartsWith(Namespace + TEXT("."));
	}

private:
	UPROPERTY(EditAnywhere, Category = "Variable")
	FName Name;
	UPROPERTY(EditAnywhere, Category = "Variable")
	FNiagaraTypeDefinition TypeDef;
	//This gets serialized but do we need to worry about endianness doing things like this? If not, where does that get handled?
	//TODO: Remove storage here entirely and move everything to an FNiagaraParameterStore.
	UPROPERTY()
	TArray<uint8> VarData;
};

FORCEINLINE uint32 GetTypeHash(const FNiagaraVariable& Var)
{
	return HashCombine(GetTypeHash(Var.GetType()), GetTypeHash(Var.GetName()));
}
