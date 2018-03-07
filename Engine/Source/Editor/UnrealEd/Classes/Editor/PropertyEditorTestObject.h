// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/ScriptInterface.h"
#include "UObject/SoftObjectPath.h"
#include "Engine/BlendableInterface.h"
#include "PropertyEditorTestObject.generated.h"

class AActor;
class IAnimClassInterface;
class UMaterialInterface;
class UPrimitiveComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTexture;

UENUM()
enum PropertEditorTestEnum
{	
	/** This comment should appear above enum 1 */
	PropertyEditorTest_Enum1 UMETA(Hidden),
	/** This comment should appear above enum 2 */
	PropertyEditorTest_Enum2,
	/** This comment should appear above enum 3 */
	PropertyEditorTest_Enum3 UMETA(Hidden),
	/** This comment should appear above enum 4 */
	PropertyEditorTest_Enum4,
	/** This comment should appear above enum 5 */
	PropertyEditorTest_Enum5 UMETA(Hidden),
	/** This comment should appear above enum 6 */
	PropertyEditorTest_Enum6,
	PropertyEditorTest_MAX,
};

UENUM()
enum ArrayLabelEnum
{
	ArrayIndex0,
	ArrayIndex1,
	ArrayIndex2,
	ArrayIndex3,
	ArrayIndex4,
	ArrayIndex5,
	ArrayIndex_MAX,
};


UENUM()
enum class EditColor : uint8
{
	Red,
	Orange,
	Yellow,
	Green,
	Blue,
	Indigo,
	Violet,
	Pink,
	Magenta,
	Cyan
};


USTRUCT()
struct FPropertyEditorTestSubStruct
{
	GENERATED_USTRUCT_BODY()

	FPropertyEditorTestSubStruct()
		: FirstProperty( 7897789 )
		, SecondProperty( 342432432  )
	{
	}

	UPROPERTY(EditAnywhere, Category=PropertyEditorTestSubStruct)
	int32 FirstProperty;

	UPROPERTY(EditAnywhere, Category=PropertyEditorTestSubStruct)
	int32 SecondProperty;

	UPROPERTY(EditAnywhere, Category=PropertyEditorTestSubStruct)
	FLinearColor CustomizedStructInsideUncustomizedStruct;

	UPROPERTY(EditAnywhere, Category=PropertyEditorTestSubStruct)
	FSoftObjectPath CustomizedStructInsideUncustomizedStruct2;
};

/**
 * This structs properties should be pushed out to categories inside its parent category unless it is in an array
 */
USTRUCT()
struct FPropertyEditorTestBasicStruct
{
	GENERATED_USTRUCT_BODY()

	FPropertyEditorTestBasicStruct()
		: IntPropertyInsideAStruct( 0 )
		, FloatPropertyInsideAStruct( 0.0f )
		, ObjectPropertyInsideAStruct( NULL )
	{
	}

	UPROPERTY(EditAnywhere, Category=InnerStructCategoryWithPushedOutProps)
	int32 IntPropertyInsideAStruct;

	UPROPERTY(EditAnywhere, Category=InnerStructCategoryWithPushedOutProps)
	float FloatPropertyInsideAStruct;

	UPROPERTY(EditAnywhere, Category=InnerStructCategoryWithPushedOutProps)
	UObject* ObjectPropertyInsideAStruct;

	UPROPERTY(EditAnywhere, Category=InnerStructCategoryWithPushedOutProps)
	FPropertyEditorTestSubStruct InnerStruct;
};

UCLASS(transient)
class UPropertyEditorTestObject : public UObject
{
    GENERATED_UCLASS_BODY()



	UPROPERTY(EditAnywhere, Category = BasicProperties)
	int8 Int8Property;

	UPROPERTY(EditAnywhere, Category = BasicProperties)
	int16 Int16roperty;

	UPROPERTY(EditAnywhere, Category = BasicProperties)
	int32 Int32Property;

	UPROPERTY(EditAnywhere, Category = BasicProperties)
	int64 Int64Property;

	UPROPERTY(EditAnywhere, Category = BasicProperties)
	uint8 ByteProperty;

	UPROPERTY(EditAnywhere, Category = BasicProperties)
	uint16 UnsignedInt16Property;

	UPROPERTY(EditAnywhere, Category = BasicProperties)
	uint32 UnsignedInt32Property;

	UPROPERTY(EditAnywhere, Category = BasicProperties)
	uint64 UnsignedInt64Property;

	UPROPERTY(EditAnywhere, Category=BasicProperties)
	float FloatProperty;

	UPROPERTY(EditAnywhere, Category=BasicProperties)
	double DoubleProperty;

	UPROPERTY(EditAnywhere, Category=BasicProperties)
	FName NameProperty;

	UPROPERTY(EditAnywhere, Category=BasicProperties)
	bool BoolProperty;

	UPROPERTY(EditAnywhere, Category=BasicProperties)
	FString StringProperty;

	UPROPERTY(EditAnywhere, Category=BasicProperties)
	FText TextProperty;

	UPROPERTY(EditAnywhere, Category=BasicProperties, meta=(AllowPreserveRatio))
	FIntPoint IntPointProperty;

	UPROPERTY(EditAnywhere, Category=BasicProperties, meta=(AllowPreserveRatio))
	FVector Vector3Property;

	UPROPERTY(EditAnywhere, Category=BasicProperties, meta=(AllowPreserveRatio))
	FVector2D Vector2Property;

	UPROPERTY(EditAnywhere, Category=BasicProperties, meta=(AllowPreserveRatio))
	FVector4 Vector4Property;

	UPROPERTY(EditAnywhere, Category=BasicProperties)
	FRotator RotatorProperty;

	UPROPERTY(EditAnywhere, Category=BasicProperties)
	UObject* ObjectProperty;

	UPROPERTY(EditAnywhere, Category=BasicProperties)
	UClass* ClassProperty;

	UPROPERTY(EditAnywhere, Category=BasicProperties)
	FLinearColor LinearColorProperty;

	UPROPERTY(EditAnywhere, Category=BasicProperties)
	FColor ColorProperty;

	UPROPERTY(EditAnywhere, Category=BasicProperties)
	TEnumAsByte<enum PropertEditorTestEnum> EnumProperty;

	UPROPERTY(EditAnywhere, Category = BasicProperties)
	FMatrix MatrixProperty;

	UPROPERTY(EditAnywhere, Category = BasicProperties)
	FTransform TransformProperty;

	// Integer
	UPROPERTY(EditAnywhere, Category=ArraysOfProperties)
	TArray<int32> IntProperty32Array;

	// Byte
	UPROPERTY(EditAnywhere, Category=ArraysOfProperties)
	TArray<uint8> BytePropertyArray;

	UPROPERTY(EditAnywhere, Category=ArraysOfProperties)
	TArray<float> FloatPropertyArray;

	UPROPERTY(EditAnywhere, Category=ArraysOfProperties)
	TArray<FName> NamePropertyArray;

	UPROPERTY(EditAnywhere, Category=ArraysOfProperties)
	TArray<bool> BoolPropertyArray;

	UPROPERTY(EditAnywhere, Category=ArraysOfProperties)
	TArray<FString> StringPropertyArray;

	UPROPERTY(EditAnywhere, Category=ArraysOfProperties)
	TArray<FText> TextPropertyArray;

	UPROPERTY(EditAnywhere, Category=ArraysOfProperties)
	TArray<FVector> Vector3PropertyArray;

	UPROPERTY(EditAnywhere, Category=ArraysOfProperties)
	TArray<FVector2D> Vector2PropertyArray;

	UPROPERTY(EditAnywhere, Category=ArraysOfProperties)
	TArray<FVector4> Vector4PropertyArray;

	UPROPERTY(EditAnywhere, Category=ArraysOfProperties)
	TArray<FRotator> RotatorPropertyArray;

	UPROPERTY(EditAnywhere, Category=ArraysOfProperties)
	TArray<UObject*> ObjectPropertyArray;

	UPROPERTY(EditAnywhere, Category=ArraysOfProperties)
	TArray<AActor*> ActorPropertyArray;

	UPROPERTY(EditAnywhere, Category=ArraysOfProperties)
	TArray<FLinearColor> LinearColorPropertyArray;

	UPROPERTY(EditAnywhere, Category=ArraysOfProperties)
	TArray<FColor> ColorPropertyArray;

	UPROPERTY(EditAnywhere, Category=ArraysOfProperties)
	TArray<TEnumAsByte<enum PropertEditorTestEnum> > EnumPropertyArray;

	UPROPERTY(EditAnywhere, Category=ArraysOfProperties)
	TArray<FPropertyEditorTestBasicStruct> StructPropertyArray;

	UPROPERTY(EditAnywhere, editfixedsize, Category=ArraysOfProperties)
	TArray<int32> FixedArrayOfInts;

	UPROPERTY(EditAnywhere, Category=ArraysOfProperties)
	int32 StaticArrayOfInts[5];

	UPROPERTY(EditAnywhere, Category=ArraysOfProperties)
	int32 StaticArrayOfIntsWithEnumLabels[ArrayIndex_MAX];

	/** This is a float property tooltip that is overridden */
	UPROPERTY(EditAnywhere, Category=AdvancedProperties, meta=(ClampMin = "0.0", ClampMax = "100.0", UIMin = "0.0", UIMax = "50.0", ToolTip = "This is a custom tooltip that should be shown"))
	float FloatPropertyWithClampedRange;

	UPROPERTY(EditAnywhere, Category=AdvancedProperties, meta=(ClampMin = "0", ClampMax = "100", UIMin = "0", UIMax = "50" ))
	int32 IntPropertyWithClampedRange;

	UPROPERTY(VisibleAnywhere, Category=AdvancedProperties)
	int32 IntThatCannotBeChanged;

	UPROPERTY(VisibleAnywhere, Category=AdvancedProperties)
	FString StringThatCannotBeChanged;

	UPROPERTY(VisibleAnywhere, Category=AdvancedProperties)
	UPrimitiveComponent* ObjectThatCannotBeChanged;

	UPROPERTY(EditAnywhere, Category=AdvancedProperties, meta=(PasswordField=true))
	FString StringPasswordProperty;

	UPROPERTY(EditAnywhere, Category=AdvancedProperties, meta=(PasswordField=true))
	FText TextPasswordProperty;

	UPROPERTY(EditAnywhere, Category=SingleStruct, meta=(ShowOnlyInnerProperties))
	FPropertyEditorTestBasicStruct ThisIsBrokenIfItsVisibleInADetailsView;

	UPROPERTY(EditAnywhere, Category=StructTests)
	FPropertyEditorTestBasicStruct StructWithMultipleInstances1;

	UPROPERTY(EditAnywhere, Category = StructTests, meta = (InlineEditConditionToggle))
	bool bEditConditionStructWithMultipleInstances2;

	UPROPERTY(EditAnywhere, Category=StructTests, meta=(editcondition = "bEditConditionStructWithMultipleInstances2"))
	FPropertyEditorTestBasicStruct StructWithMultipleInstances2;

	UPROPERTY(EditAnywhere, Category=StructTests)
	FSoftObjectPath AssetReferenceCustomStruct;

	UPROPERTY(EditAnywhere, Category=StructTests, meta=(DisplayThumbnail = "true"))
	FSoftObjectPath AssetReferenceCustomStructWithThumbnail;

	UPROPERTY(EditAnywhere, Category=StructTests, meta=(InlineEditConditionToggle))
	bool bEditCondition;

	UPROPERTY(EditAnywhere, Category=AdvancedProperties, meta=(editcondition = "bEditCondition"))
	int32 SimplePropertyWithEditCondition;

	UPROPERTY(EditAnywhere, Category = StructTests, meta = (InlineEditConditionToggle))
	bool bEditConditionAssetReferenceCustomStructWithEditCondition;

	UPROPERTY(EditAnywhere, Category=StructTests, meta=(editcondition = "bEditConditionAssetReferenceCustomStructWithEditCondition"))
	FSoftObjectPath AssetReferenceCustomStructWithEditCondition;

	UPROPERTY(EditAnywhere, Category=StructTests)
	TArray<FPropertyEditorTestBasicStruct> ArrayOfStructs;

	UPROPERTY(EditAnywhere, Category=EditInlineProps)
	UStaticMeshComponent* EditInlineNewStaticMeshComponent;

	UPROPERTY(EditAnywhere, Category=EditInlineProps)
	TArray<UStaticMeshComponent*> ArrayOfEditInlineNewSMCs;

	UPROPERTY(EditAnywhere, Category=AssetPropertyTests)
	UTexture* TextureProp;

	UPROPERTY(EditAnywhere, Category=AssetPropertyTests)
	UStaticMesh* StaticMeshProp;

	UPROPERTY(EditAnywhere, Category=AssetPropertyTests)
	UMaterialInterface* AnyMaterialInterface;

	UPROPERTY(EditAnywhere, Category=AssetPropertyTests)
	AActor* OnlyActorsAllowed;

	UPROPERTY(EditAnywhere, Category=TSetTests)
	TSet<int32> Int32Set;

	UPROPERTY(EditAnywhere, Category=TSetTests)
	TSet<float> FloatSet;

	UPROPERTY(EditAnywhere, Category=TSetTests)
	TSet<FString> StringSet;

	UPROPERTY(EditAnywhere, Category=TSetTests)
	TSet<UObject*> ObjectSet;

	UPROPERTY(EditAnywhere, Category=TSetTests)
	TSet<AActor*> ActorSet;

	UPROPERTY(EditAnywhere, Category=TSetTests)
	TSet<EditColor> EditColorSet;

	UPROPERTY(EditAnywhere, Category=TSetTests)
	TSet<FName> NameSet;

	UPROPERTY(EditAnywhere, Category=TMapTests)
	TMap<int32, FString> Int32ToStringMap;

	UPROPERTY(EditAnywhere, Category=TMapTests)
	TMap<FString, FLinearColor> StringToColorMap;

	UPROPERTY(EditAnywhere, Category=TMapTests)
	TMap<int32, FPropertyEditorTestBasicStruct> Int32ToStructMap;

	UPROPERTY(EditAnywhere, Category=TMapTests)
	TMap<FString, float> StringToFloatMap;

	UPROPERTY(EditAnywhere, Category=TMapTests)
	TMap<FString, UObject*> StringToObjectMap;

	UPROPERTY(EditAnywhere, Category=TMapTests)
	TMap<FString, AActor*> StringToActorMap;

	UPROPERTY(EditAnywhere, Category=TMapTests)
	TMap<UObject*, int32> ObjectToInt32Map;

	UPROPERTY(EditAnywhere, Category=TMapTests)
	TMap<UObject*, FLinearColor> ObjectToColorMap;

	UPROPERTY(EditAnywhere, Category=TMapTests)
	TMap<int32, TEnumAsByte<PropertEditorTestEnum> > IntToEnumMap;

	UPROPERTY(EditAnywhere, Category=TMapTests)
	TMap<FName, FName> NameToNameMap;

	UPROPERTY(EditAnywhere, Category=TSetStructTests)
	TSet<FLinearColor> LinearColorSet;

	UPROPERTY(EditAnywhere, Category=TSetStructTests)
	TSet<FVector> VectorSet;

	UPROPERTY(EditAnywhere, Category=TMapStructKeyTests)
	TMap<FLinearColor, FString> LinearColorToStringMap;

	UPROPERTY(EditAnywhere, Category=TMapStructKeyTests)
	TMap<FVector, float> VectorToFloatMap;

	UPROPERTY(EditAnywhere, Category=TMapStructKeyTests)
	TMap<FLinearColor, FVector> LinearColorToVectorMap;

	UPROPERTY(EditAnywhere, Category=ScriptInterfaces)
	TScriptInterface<IBlendableInterface> BlendableInterface;

	UPROPERTY(EditAnywhere, Category=ScriptInterfaces)
	TScriptInterface<IAnimClassInterface> AnimClassInterface;

	// This is an IBlendableInterface that only allows for ULightPropagationVolumeBlendable objects
	UPROPERTY(EditAnywhere, Category=ScriptInterfaces, meta=(AllowedClasses="LightPropagationVolumeBlendable"))
	TScriptInterface<IBlendableInterface> LightPropagationVolumeBlendable;

	// Allows either an object that's derived from UTexture or IBlendableInterface, to ensure that Object Property handles know how to
	// filter for AllowedClasses correctly.
	UPROPERTY(EditAnywhere, Category=ObjectPropertyAllowedClasses, meta=(AllowedClasses="Texture,BlendableInterface"))
	UObject* TextureOrBlendableInterface;
};
