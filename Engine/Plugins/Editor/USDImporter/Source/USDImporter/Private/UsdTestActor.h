// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UsdTestActor.generated.h"

UENUM()
enum class EUsdTestEnum : uint8
{
	EnumVal1,
	EnumVal2,
	EnumVal3,
	EnumVal4,
	EnumVal5,
};

USTRUCT()
struct FUsdTestStruct2
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "UsdTestStruct2")
	float FloatProperty;

	UPROPERTY(EditAnywhere, Category = "UsdTestStruct2")
	FString StringProperty;
};

USTRUCT()
struct FUsdTestStruct
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "UsdTestStruct")
	int32 IntProperty;
	
	UPROPERTY(EditAnywhere, Category = "UsdTestStruct")
	EUsdTestEnum EnumProperty;

	UPROPERTY(EditAnywhere, Category = "UsdTestStruct")
	TArray<FLinearColor> ColorProperty;

	UPROPERTY(EditAnywhere, Category = "UsdTestStruct")
	TMap<FString, FUsdTestStruct2> MapTest;

	UPROPERTY(EditAnywhere, Category = "UsdTestStruct")
	FUsdTestStruct2 InnerStruct;
};

UCLASS()
class UUsdTestComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UUsdTestComponent() {};

	// Basic properties
	UPROPERTY(EditAnywhere, Category = "Component|BasicProperties")
	int8 Int8Property;

	UPROPERTY(EditAnywhere, Category = "Component|BasicProperties")
	int16 Int16Property;

	UPROPERTY(EditAnywhere, Category = "Component|BasicProperties")
	int32 Int32Property;

	UPROPERTY(EditAnywhere, Category = "Component|BasicProperties")
	int64 Int64Property;

	UPROPERTY(EditAnywhere, Category = "Component|BasicProperties")
	uint8 ByteProperty;

	UPROPERTY(EditAnywhere, Category = "Component|BasicProperties")
	uint16 UnsignedInt16Property;

	UPROPERTY(EditAnywhere, Category = "Component|BasicProperties")
	uint32 UnsignedInt32Property;

	UPROPERTY(EditAnywhere, Category = "Component|BasicProperties")
	uint64 UnsignedInt64Property;

	UPROPERTY(EditAnywhere, Category = "Component|BasicProperties")
	float FloatProperty;

	UPROPERTY(EditAnywhere, Category = "Component|BasicProperties")
	double DoubleProperty;

	UPROPERTY(EditAnywhere, Category = "Component|BasicProperties")
	FName NameProperty;

	UPROPERTY(EditAnywhere, Category = "Component|BasicProperties")
	bool BoolProperty;

	UPROPERTY(EditAnywhere, Category = "Component|BasicProperties")
	FString StringProperty;

	UPROPERTY(EditAnywhere, Category = "Component|BasicProperties")
	FText TextProperty;

	UPROPERTY(EditAnywhere, Category = "Component|BasicProperties")
	FVector2D Vector2Property;

	UPROPERTY(EditAnywhere, Category = "Component|BasicProperties")
	FVector Vector3Property;

	UPROPERTY(EditAnywhere, Category = "Component|BasicProperties")
	FVector4 Vector4Property;

	UPROPERTY(EditAnywhere, Category = "Component|BasicProperties")
	FLinearColor LinearColorProperty;

	UPROPERTY(EditAnywhere, Category = "Component|BasicProperties")
	FRotator RotatorProperty;

	UPROPERTY(EditAnywhere, Category = "Component|BasicProperties")
	FColor ColorProperty;

	UPROPERTY(EditAnywhere, Category = "Component|BasicProperties")
	UObject* ObjectProperty;

	UPROPERTY(EditAnywhere, Category = "Component|BasicProperties")
	EUsdTestEnum EnumProperty;

	// Arrays
	UPROPERTY(EditAnywhere, Category = "Component|ArrayProperties")
	TArray<int8> Int8ArrayProperty;

	UPROPERTY(EditAnywhere, Category = "Component|ArrayProperties")
	TArray<int16> Int16ArrayProperty;

	UPROPERTY(EditAnywhere, Category = "Component|ArrayProperties")
	TArray<int32> Int32ArrayProperty;

	UPROPERTY(EditAnywhere, Category = "Component|ArrayProperties")
	TArray<int64> Int64ArrayProperty;

	UPROPERTY(EditAnywhere, Category = "Component|ArrayProperties")
	TArray<uint8> ByteArrayProperty;

	UPROPERTY(EditAnywhere, Category = "Component|ArrayProperties")
	TArray<uint16> UnsignedInt16ArrayProperty;

	UPROPERTY(EditAnywhere, Category = "Component|ArrayProperties")
	TArray<uint32> UnsignedInt32ArrayProperty;

	UPROPERTY(EditAnywhere, Category = "Component|ArrayProperties")
	TArray<uint64> UnsignedInt64ArrayProperty;

	UPROPERTY(EditAnywhere, Category = "Component|ArrayProperties")
	TArray<float> FloatArrayProperty;

	UPROPERTY(EditAnywhere, Category = "Component|ArrayProperties")
	TArray<double> DoubleArrayProperty;

	UPROPERTY(EditAnywhere, Category = "Component|ArrayProperties")
	TArray<FName> NameArrayProperty;

	UPROPERTY(EditAnywhere, Category = "Component|ArrayProperties")
	TArray<bool> BoolArrayProperty;

	UPROPERTY(EditAnywhere, Category = "Component|ArrayProperties")
	TArray<FString> StringArrayProperty;

	UPROPERTY(EditAnywhere, Category = "Component|ArrayProperties")
	TArray<FText> TextArrayProperty;

	UPROPERTY(EditAnywhere, Category = "Component|ArrayProperties")
	TArray<FVector2D> Vector2ArrayProperty;

	UPROPERTY(EditAnywhere, Category = "Component|ArrayProperties")
	TArray<FVector> Vector3ArrayProperty;

	UPROPERTY(EditAnywhere, Category = "Component|ArrayProperties")
	TArray<FVector4> Vector4ArrayProperty;

	UPROPERTY(EditAnywhere, Category = "Component|ArrayProperties")
	TArray<FLinearColor> LinearColorArrayProperty;

	UPROPERTY(EditAnywhere, Category = "Component|ArrayProperties")
	TArray<FRotator> RotatorArrayProperty;

	UPROPERTY(EditAnywhere, Category = "Component|ArrayProperties")
	TArray<FColor> ColorArrayProperty;

	UPROPERTY(EditAnywhere, Category = "Component|ArrayProperties")
	TArray<UObject*> ObjectArrayProperty;

	UPROPERTY(EditAnywhere, Category = "Component|ArrayProperties")
	TArray<EUsdTestEnum> EnumArrayProperty;

};


UCLASS(Blueprintable)
class AUsdTestActor : public AActor
{
	GENERATED_UCLASS_BODY()
	
public:	
	// Basic properties
	UPROPERTY(EditAnywhere, Category = BasicProperties)
	int8 Int8Property;

	UPROPERTY(EditAnywhere, Category = BasicProperties)
	int16 Int16Property;

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

	UPROPERTY(EditAnywhere, Category = BasicProperties)
	float FloatProperty;

	UPROPERTY(EditAnywhere, Category = BasicProperties)
	double DoubleProperty;

	UPROPERTY(EditAnywhere, Category = BasicProperties)
	FName NameProperty;

	UPROPERTY(EditAnywhere, Category = BasicProperties)
	bool BoolProperty;

	UPROPERTY(EditAnywhere, Category = BasicProperties)
	FString StringProperty;

	UPROPERTY(EditAnywhere, Category = BasicProperties)
	FText TextProperty;

	UPROPERTY(EditAnywhere, Category = BasicProperties)
	FVector2D Vector2Property;

	UPROPERTY(EditAnywhere, Category = BasicProperties)
	FVector Vector3Property;

	UPROPERTY(EditAnywhere, Category = BasicProperties)
	FVector4 Vector4Property;

	UPROPERTY(EditAnywhere, Category = BasicProperties)
	FLinearColor LinearColorProperty;

	UPROPERTY(EditAnywhere, Category = BasicProperties)
	FRotator RotatorProperty;

	UPROPERTY(EditAnywhere, Category = BasicProperties)
	FColor ColorProperty;

	UPROPERTY(EditAnywhere, Category = BasicProperties)
	UObject* ObjectProperty;

	UPROPERTY(EditAnywhere, Category = BasicProperties)
	EUsdTestEnum EnumProperty;

	// Arrays
	UPROPERTY(EditAnywhere, Category = ArrayProperties)
	TArray<int8> Int8ArrayProperty;

	UPROPERTY(EditAnywhere, Category = ArrayProperties)
	TArray<int16> Int16ArrayProperty;

	UPROPERTY(EditAnywhere, Category = ArrayProperties)
	TArray<int32> Int32ArrayProperty;

	UPROPERTY(EditAnywhere, Category = ArrayProperties)
	TArray<int64> Int64ArrayProperty;

	UPROPERTY(EditAnywhere, Category = ArrayProperties)
	TArray<uint8> ByteArrayProperty;

	UPROPERTY(EditAnywhere, Category = ArrayProperties)
	TArray<uint16> UnsignedInt16ArrayProperty;

	UPROPERTY(EditAnywhere, Category = ArrayProperties)
	TArray<uint32> UnsignedInt32ArrayProperty;

	UPROPERTY(EditAnywhere, Category = ArrayProperties)
	TArray<uint64> UnsignedInt64ArrayProperty;

	UPROPERTY(EditAnywhere, Category = ArrayProperties)
	TArray<float> FloatArrayProperty;

	UPROPERTY(EditAnywhere, Category = ArrayProperties)
	TArray<double> DoubleArrayProperty;

	UPROPERTY(EditAnywhere, Category = ArrayProperties)
	TArray<FName> NameArrayProperty;

	UPROPERTY(EditAnywhere, Category = ArrayProperties)
	TArray<bool> BoolArrayProperty;

	UPROPERTY(EditAnywhere, Category = ArrayProperties)
	TArray<FString> StringArrayProperty;

	UPROPERTY(EditAnywhere, Category = ArrayProperties)
	TArray<FText> TextArrayProperty;

	UPROPERTY(EditAnywhere, Category = ArrayProperties)
	TArray<FVector2D> Vector2ArrayProperty;

	UPROPERTY(EditAnywhere, Category = ArrayProperties)
	TArray<FVector> Vector3ArrayProperty;

	UPROPERTY(EditAnywhere, Category = ArrayProperties)
	TArray<FVector4> Vector4ArrayProperty;

	UPROPERTY(EditAnywhere, Category = ArrayProperties)
	TArray<FLinearColor> LinearColorArrayProperty;

	UPROPERTY(EditAnywhere, Category = ArrayProperties)
	TArray<FRotator> RotatorArrayProperty;

	UPROPERTY(EditAnywhere, Category = ArrayProperties)
	TArray<FColor> ColorArrayProperty;

	UPROPERTY(EditAnywhere, Category = ArrayProperties)
	TArray<UObject*> ObjectArrayProperty;

	UPROPERTY(EditAnywhere, Category = ArrayProperties)
	TArray<EUsdTestEnum> EnumArrayProperty;

	UPROPERTY(EditAnywhere, Category = StructProperties)
	FUsdTestStruct TestStructProperty;

	UPROPERTY(EditAnywhere, Category = ArrayDeepNesting)
	TArray<FUsdTestStruct> TestStructPropertyArray;

	UPROPERTY(EditAnywhere, Category = ArrayDeepNesting)
	TArray<FUsdTestStruct> TestStructPropertyArray2;

	UPROPERTY(VisibleAnywhere, Category = InnerComponent)
	UUsdTestComponent* TestComponent;
public:	

};
