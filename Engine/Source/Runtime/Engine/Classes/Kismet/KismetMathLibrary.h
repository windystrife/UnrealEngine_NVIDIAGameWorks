// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Script.h"
#include "UObject/ObjectMacros.h"
#include "Math/RandomStream.h"
#include "Templates/SubclassOf.h"
#include "UObject/UnrealType.h"
#include "UObject/Stack.h"
#include "UObject/ScriptMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "KismetMathLibrary.generated.h"

// Whether to inline functions at all
#define KISMET_MATH_INLINE_ENABLED	(!UE_BUILD_DEBUG)

/** Provides different easing functions that can be used in blueprints */
UENUM(BlueprintType)
namespace EEasingFunc
{
	enum Type
	{
		/** Simple linear interpolation. */
		Linear,

		/** Simple step interpolation. */
		Step,

		/** Sinusoidal in interpolation. */
		SinusoidalIn,

		/** Sinusoidal out interpolation. */
		SinusoidalOut,

		/** Sinusoidal in/out interpolation. */
		SinusoidalInOut,

		/** Smoothly accelerates, but does not decelerate into the target.  Ease amount controlled by BlendExp. */
		EaseIn,

		/** Immediately accelerates, but smoothly decelerates into the target.  Ease amount controlled by BlendExp. */
		EaseOut,

		/** Smoothly accelerates and decelerates.  Ease amount controlled by BlendExp. */
		EaseInOut,

		/** Easing in using an exponential */
		ExpoIn,

		/** Easing out using an exponential */
		ExpoOut,

		/** Easing in/out using an exponential method */
		ExpoInOut,

		/** Easing is based on a half circle. */
		CircularIn,

		/** Easing is based on an inverted half circle. */
		CircularOut,

		/** Easing is based on two half circles. */
		CircularInOut,

	};
}

/** Different methods for interpolating rotation between transforms */
UENUM(BlueprintType)
namespace ELerpInterpolationMode
{
	enum Type
	{
		/** Shortest Path or Quaternion interpolation for the rotation. */
		QuatInterp,

		/** Rotor or Euler Angle interpolation. */
		EulerInterp,

		/** Dual quaternion interpolation, follows helix or screw-motion path between keyframes.   */
		DualQuatInterp
	};
}

USTRUCT(BlueprintType)
struct ENGINE_API FFloatSpringState
{
	GENERATED_BODY()

	float PrevError;
	float Velocity;

	FFloatSpringState()
	: PrevError(0.f)
	, Velocity(0.f)
	{

	}

	void Reset()
	{
		PrevError = Velocity = 0.f;
	}
};

USTRUCT(BlueprintType)
struct ENGINE_API FVectorSpringState
{
	GENERATED_BODY()

	FVector PrevError;
	FVector Velocity;

	FVectorSpringState()
	: PrevError(FVector::ZeroVector)
	, Velocity(FVector::ZeroVector)
	{

	}

	void Reset()
	{
		PrevError = Velocity = FVector::ZeroVector;
	}
};

UCLASS(meta=(BlueprintThreadSafe))
class ENGINE_API UKismetMathLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	//
	// Boolean functions.
	//
	
	/** Returns a uniformly distributed random bool*/
	UFUNCTION(BlueprintPure, Category="Math|Random", meta=(NotBlueprintThreadSafe))
	static bool RandomBool();

	/** 
	 * Get a random chance with the specified weight. Range of weight is 0.0 - 1.0 E.g.,
	 *		Weight = .6 return value = True 60% of the time
	 */
	UFUNCTION(BlueprintPure, Category = "Math|Random", meta=(Weight = "0.5", NotBlueprintThreadSafe))
	static bool RandomBoolWithWeight(float Weight);

	/** 
	 * Get a random chance with the specified weight. Range of weight is 0.0 - 1.0 E.g.,
	*		Weight = .6 return value = True 60% of the time
	*/
	UFUNCTION(BlueprintPure, Category = "Math|Random", meta=(Weight = "0.5"))
	static bool RandomBoolWithWeightFromStream(float Weight, const FRandomStream& RandomStream);

	/** Returns the logical complement of the Boolean value (NOT A) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "NOT Boolean", CompactNodeTitle = "NOT", Keywords = "! not negate"), Category="Math|Boolean")
	static bool Not_PreBool(bool A);

	/** Returns true if the values are equal (A == B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Equal Boolean", CompactNodeTitle = "==", Keywords = "== equal"), Category="Math|Boolean")
	static bool EqualEqual_BoolBool(bool A, bool B);

	/** Returns true if the values are not equal (A != B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "NotEqual Boolean", CompactNodeTitle = "!=", Keywords = "!= not equal"), Category="Math|Boolean")
	static bool NotEqual_BoolBool(bool A, bool B);

	/** Returns the logical AND of two values (A AND B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "AND Boolean", CompactNodeTitle = "AND", Keywords = "& and", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Boolean")
	static bool BooleanAND(bool A, bool B);

	/** Returns the logical NAND of two values (A AND B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "NAND Boolean", CompactNodeTitle = "NAND", Keywords = "!& nand", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Boolean")
	static bool BooleanNAND(bool A, bool B);

	/** Returns the logical OR of two values (A OR B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "OR Boolean", CompactNodeTitle = "OR", Keywords = "| or", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Boolean")
	static bool BooleanOR(bool A, bool B);
		
	/** Returns the logical eXclusive OR of two values (A XOR B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "XOR Boolean", CompactNodeTitle = "XOR", Keywords = "^ xor"), Category="Math|Boolean")
	static bool BooleanXOR(bool A, bool B);

	/** Returns the logical Not OR of two values (A NOR B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "NOR Boolean", CompactNodeTitle = "NOR", Keywords = "!^ nor"), Category="Math|Boolean")
	static bool BooleanNOR(bool A, bool B);

	//
	// Byte functions.
	//

	/** Multiplication (A * B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Byte * Byte", CompactNodeTitle = "*", Keywords = "* multiply", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Byte")
	static uint8 Multiply_ByteByte(uint8 A, uint8 B);

	/** Division (A / B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Byte / Byte", CompactNodeTitle = "/", Keywords = "/ divide division"), Category="Math|Byte")
	static uint8 Divide_ByteByte(uint8 A, uint8 B = 1);

	/** Modulo (A % B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "% (Byte)", CompactNodeTitle = "%", Keywords = "% modulus"), Category="Math|Byte")
	static uint8 Percent_ByteByte(uint8 A, uint8 B = 1);

	/** Addition (A + B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Byte + Byte", CompactNodeTitle = "+", Keywords = "+ add plus", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Byte")
	static uint8 Add_ByteByte(uint8 A, uint8 B = 1);

	/** Subtraction (A - B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Byte - Byte", CompactNodeTitle = "-", Keywords = "- subtract minus"), Category="Math|Byte")
	static uint8 Subtract_ByteByte(uint8 A, uint8 B = 1);

	/** Returns the minimum value of A and B */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Min (Byte)", CompactNodeTitle = "MIN", CommutativeAssociativeBinaryOperator = "true"), Category = "Math|Byte")
	static uint8 BMin(uint8 A, uint8 B);

	/** Returns the maximum value of A and B */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Max (Byte)", CompactNodeTitle = "MAX", CommutativeAssociativeBinaryOperator = "true"), Category = "Math|Byte")
	static uint8 BMax(uint8 A, uint8 B);
	
	/** Returns true if A is less than B (A < B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Byte < Byte", CompactNodeTitle = "<", Keywords = "< less"), Category="Math|Byte")
	static bool Less_ByteByte(uint8 A, uint8 B);

	/** Returns true if A is greater than B (A > B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Byte > Byte", CompactNodeTitle = ">", Keywords = "> greater"), Category="Math|Byte")
	static bool Greater_ByteByte(uint8 A, uint8 B);

	/** Returns true if A is less than or equal to B (A <= B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Byte <= Byte", CompactNodeTitle = "<=", Keywords = "<= less"), Category="Math|Byte")
	static bool LessEqual_ByteByte(uint8 A, uint8 B);

	/** Returns true if A is greater than or equal to B (A >= B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Byte >= Byte", CompactNodeTitle = ">=", Keywords = ">= greater"), Category="Math|Byte")
	static bool GreaterEqual_ByteByte(uint8 A, uint8 B);

	/** Returns true if A is equal to B (A == B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Equal (Byte)", CompactNodeTitle = "==", Keywords = "== equal"), Category="Math|Byte")
	static bool EqualEqual_ByteByte(uint8 A, uint8 B);

	/** Returns true if A is not equal to B (A != B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "NotEqual (Byte)", CompactNodeTitle = "!=", Keywords = "!= not equal"), Category="Math|Byte")
	static bool NotEqual_ByteByte(uint8 A, uint8 B);

	//
	// Integer functions.
	//

	/** Multiplication (A * B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "integer * integer", CompactNodeTitle = "*", Keywords = "* multiply", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Integer")
	static int32 Multiply_IntInt(int32 A, int32 B);

	/** Division (A / B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "integer / integer", CompactNodeTitle = "/", Keywords = "/ divide division"), Category="Math|Integer")
	static int32 Divide_IntInt(int32 A, int32 B = 1);

	/** Modulo (A % B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "% (integer)", CompactNodeTitle = "%", Keywords = "% modulus"), Category="Math|Integer")
	static int32 Percent_IntInt(int32 A, int32 B = 1);

	/** Addition (A + B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "integer + integer", CompactNodeTitle = "+", Keywords = "+ add plus", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Integer")
	static int32 Add_IntInt(int32 A, int32 B = 1);

	/** Subtraction (A - B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "integer - integer", CompactNodeTitle = "-", Keywords = "- subtract minus"), Category="Math|Integer")
	static int32 Subtract_IntInt(int32 A, int32 B = 1);

	/** Returns true if A is less than B (A < B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "integer < integer", CompactNodeTitle = "<", Keywords = "< less"), Category="Math|Integer")
	static bool Less_IntInt(int32 A, int32 B);

	/** Returns true if A is greater than B (A > B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "integer > integer", CompactNodeTitle = ">", Keywords = "> greater"), Category="Math|Integer")
	static bool Greater_IntInt(int32 A, int32 B);

	/** Returns true if A is less than or equal to B (A <= B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "integer <= integer", CompactNodeTitle = "<=", Keywords = "<= less"), Category="Math|Integer")
	static bool LessEqual_IntInt(int32 A, int32 B);

	/** Returns true if A is greater than or equal to B (A >= B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "integer >= integer", CompactNodeTitle = ">=", Keywords = ">= greater"), Category="Math|Integer")
	static bool GreaterEqual_IntInt(int32 A, int32 B);

	/** Returns true if A is equal to B (A == B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Equal (integer)", CompactNodeTitle = "==", Keywords = "== equal"), Category="Math|Integer")
	static bool EqualEqual_IntInt(int32 A, int32 B);

	/** Returns true if A is not equal to B (A != B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "NotEqual (integer)", CompactNodeTitle = "!=", Keywords = "!= not equal"), Category="Math|Integer")
	static bool NotEqual_IntInt(int32 A, int32 B);

	/** Returns true if value is between Min and Max (V >= Min && V <= Max)
	 * If InclusiveMin is true, value needs to be equal or larger than Min, else it needs to be larger
	 * If InclusiveMax is true, value needs to be smaller or equal than Max, else it needs to be smaller
	 */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "InRange (integer)", Min = "0", Max = "10"), Category = "Math|Integer")
	static bool InRange_IntInt(int32 Value, int32 Min, int32 Max, bool InclusiveMin = true, bool InclusiveMax = true);

	/** Bitwise AND (A & B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Bitwise AND", CompactNodeTitle = "&", Keywords = "& and", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Integer")
	static int32 And_IntInt(int32 A, int32 B);

	/** Bitwise XOR (A ^ B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Bitwise XOR", CompactNodeTitle = "^", Keywords = "^ xor"), Category="Math|Integer")
	static int32 Xor_IntInt(int32 A, int32 B);

	/** Bitwise OR (A | B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Bitwise OR", CompactNodeTitle = "|", Keywords = "| or", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Integer")
	static int32 Or_IntInt(int32 A, int32 B);

	/** Bitwise NOT (~A) */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Bitwise NOT", CompactNodeTitle = "~", Keywords = "~ not"), Category = "Math|Integer")
	static int32 Not_Int(int32 A);

	/** Sign (integer, returns -1 if A < 0, 0 if A is zero, and +1 if A > 0) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Sign (int)"), Category="Math|Integer")
	static int32 SignOfInteger(int32 A);

	/** Returns a uniformly distributed random number between 0 and Max - 1 */
	UFUNCTION(BlueprintPure, Category="Math|Random", meta=(NotBlueprintThreadSafe))
	static int32 RandomInteger(int32 Max);

	/** Return a random integer between Min and Max (>= Min and <= Max) */
	UFUNCTION(BlueprintPure, Category="Math|Random", meta = (NotBlueprintThreadSafe))
	static int32 RandomIntegerInRange(int32 Min, int32 Max);

	/** Returns the minimum value of A and B */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Min (int)", CompactNodeTitle = "MIN", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Integer")
	static int32 Min(int32 A, int32 B);

	/** Returns the maximum value of A and B */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Max (int)", CompactNodeTitle = "MAX", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Integer")
	static int32 Max(int32 A, int32 B);

	/** Returns Value clamped to be between A and B (inclusive) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Clamp (int)"), Category="Math|Integer")
	static int32 Clamp(int32 Value, int32 Min, int32 Max);

	/** Returns the absolute (positive) value of A */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Absolute (int)", CompactNodeTitle = "ABS"), Category="Math|Integer")
	static int32 Abs_Int(int32 A);

	//
	// Float functions.
	//

	/** Power (Base to the Exp-th power) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Power" ), Category="Math|Float")
	static float MultiplyMultiply_FloatFloat(float Base, float Exp);

	/** Multiplication (A * B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "float * float", CompactNodeTitle = "*", Keywords = "* multiply", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Float")
	static float Multiply_FloatFloat(float A, float B);

	/** Multiplication (A * B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "int * float", CompactNodeTitle = "*", Keywords = "* multiply"), Category="Math|Float")
	static float Multiply_IntFloat(int32 A, float B);

	/** Division (A / B) */
	UFUNCTION(BlueprintPure, CustomThunk, meta=(DisplayName = "float / float", CompactNodeTitle = "/", Keywords = "/ divide division"), Category="Math|Float")
	static float Divide_FloatFloat(float A, float B = 1.f);
	
	static float GenericDivide_FloatFloat(float A, float B);

	/** Custom thunk to allow script stack trace in case of divide by zero */
	DECLARE_FUNCTION(execDivide_FloatFloat)
	{
		P_GET_PROPERTY(UFloatProperty, A);
		P_GET_PROPERTY(UFloatProperty, B);

		P_FINISH;

		if (B == 0.f)
		{
			FFrame::KismetExecutionMessage(*FString::Printf(TEXT("Divide by zero detected: %f / 0\n%s"), A, *Stack.GetStackTrace()), ELogVerbosity::Warning);
			*(float*)RESULT_PARAM = 0;
			return;
		}

		*(float*)RESULT_PARAM = GenericDivide_FloatFloat(A, B);
	}

	/** Modulo (A % B) */
	UFUNCTION(BlueprintPure, CustomThunk, meta = (DisplayName = "% (float)", CompactNodeTitle = "%", Keywords = "% modulus"), Category = "Math|Float")
	static float Percent_FloatFloat(float A, float B = 1.f);

	static float GenericPercent_FloatFloat(float A, float B);

	/** Custom thunk to allow script stack trace in case of modulo by zero */
	DECLARE_FUNCTION(execPercent_FloatFloat)
	{
		P_GET_PROPERTY(UFloatProperty, A);
		P_GET_PROPERTY(UFloatProperty, B);

		P_FINISH;

		if (B == 0.f)
		{
			FFrame::KismetExecutionMessage(*FString::Printf(TEXT("Modulo by zero detected: %f %% 0\n%s"), A, *Stack.GetStackTrace()), ELogVerbosity::Warning);
			*(float*)RESULT_PARAM = 0;
			return;
		}

		*(float*)RESULT_PARAM = GenericPercent_FloatFloat(A, B);
	}

	/** Returns the fractional part of a float. */
	UFUNCTION(BlueprintPure, Category="Math|Float")
	static float Fraction(float A);

	/** Addition (A + B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "float + float", CompactNodeTitle = "+", Keywords = "+ add plus", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Float")
	static float Add_FloatFloat(float A, float B = 1.f);

	/** Subtraction (A - B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "float - float", CompactNodeTitle = "-", Keywords = "- subtract minus"), Category="Math|Float")
	static float Subtract_FloatFloat(float A, float B = 1.f);

	/** Returns true if A is Less than B (A < B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "float < float", CompactNodeTitle = "<", Keywords = "< less"), Category="Math|Float")
	static bool Less_FloatFloat(float A, float B);

	/** Returns true if A is greater than B (A > B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "float > float", CompactNodeTitle = ">", Keywords = "> greater"), Category="Math|Float")
	static bool Greater_FloatFloat(float A, float B);

	/** Returns true if A is Less than or equal to B (A <= B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "float <= float", CompactNodeTitle = "<=", Keywords = "<= less"), Category="Math|Float")
	static bool LessEqual_FloatFloat(float A, float B);

	/** Returns true if A is greater than or equal to B (A >= B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "float >= float", CompactNodeTitle = ">=", Keywords = ">= greater"), Category="Math|Float")
	static bool GreaterEqual_FloatFloat(float A, float B);

	/** Returns true if A is exactly equal to B (A == B)*/
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Equal (float)", CompactNodeTitle = "==", Keywords = "== equal"), Category="Math|Float")
	static bool EqualEqual_FloatFloat(float A, float B);

	/** Returns true if A is nearly equal to B (|A - B| < ErrorTolerance) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Nearly Equal (float)", Keywords = "== equal"), Category="Math|Float")
	static bool NearlyEqual_FloatFloat(float A, float B, float ErrorTolerance = 1.e-6f);

	/** Returns true if A does not equal B (A != B)*/
	UFUNCTION(BlueprintPure, meta=(DisplayName = "NotEqual (float)", CompactNodeTitle = "!=", Keywords = "!= not equal"), Category="Math|Float")
	static bool NotEqual_FloatFloat(float A, float B);

	/** Returns true if value is between Min and Max (V >= Min && V <= Max)
	 * If InclusiveMin is true, value needs to be equal or larger than Min, else it needs to be larger
	 * If InclusiveMax is true, value needs to be smaller or equal than Max, else it needs to be smaller
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "InRange (float)", Min="0.0", Max="1.0"), Category="Math|Float")
	static bool InRange_FloatFloat(float Value, float Min, float Max, bool InclusiveMin = true, bool InclusiveMax = true);

	/** Returns the hypotenuse of a right-angled triangle given the width and height. */
	UFUNCTION(BlueprintPure, meta=(Keywords = "pythagorean theorem"), Category = "Math|Float")
	static float Hypotenuse(float Width, float Height);
	
	/** Snaps a value to the nearest grid multiple. E.g.,
	 *		Location = 5.1, GridSize = 10.0 : return value = 10.0
	 * If GridSize is 0 Location is returned
	 * if GridSize is very small precision issues may occur.
	 */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Snap to grid (float)"), Category = "Math|Float")
	static float GridSnap_Float(float Location, float GridSize);

	/** Returns the absolute (positive) value of A */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Absolute (float)", CompactNodeTitle = "ABS"), Category="Math|Float")
	static float Abs(float A);

	/** Returns the sine of A (expects Radians)*/
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Sin (Radians)", CompactNodeTitle = "SIN", Keywords = "sine"), Category="Math|Trig")
	static float Sin(float A);

	/** Returns the inverse sine (arcsin) of A (result is in Radians) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Asin (Radians)", CompactNodeTitle = "ASIN", Keywords = "sine"), Category="Math|Trig")
	static float Asin(float A);

	/** Returns the cosine of A (expects Radians)*/
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Cos (Radians)", CompactNodeTitle = "COS"), Category="Math|Trig")
	static float Cos(float A);

	/** Returns the inverse cosine (arccos) of A (result is in Radians) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Acos (Radians)", CompactNodeTitle = "ACOS"), Category="Math|Trig")
	static float Acos(float A);

	/** Returns the tan of A (expects Radians)*/
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Tan (Radians)", CompactNodeTitle = "TAN"), Category="Math|Trig")
	static float Tan(float A);

	/** Returns the inverse tan (atan) (result is in Radians)*/
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Atan (Radians)"), Category="Math|Trig")
	static float Atan(float A);

	/** Returns the inverse tan (atan2) of A/B (result is in Radians)*/
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Atan2 (Radians)"), Category="Math|Trig")
	static float Atan2(float A, float B);

	/** Returns exponential(e) to the power A (e^A)*/
	UFUNCTION(BlueprintPure, Category="Math|Float", meta=(CompactNodeTitle = "e"))
	static float Exp(float A);

	/** Returns log of A base B (if B^R == A, returns R)*/
	UFUNCTION(BlueprintPure, Category = "Math|Float")
	static float Log(float A, float Base = 1.f);

	/** Returns natural log of A (if e^R == A, returns R)*/
	UFUNCTION(BlueprintPure, Category="Math|Float")
	static float Loge(float A);

	/** Returns square root of A*/
	UFUNCTION(BlueprintPure, Category="Math|Float", meta=(Keywords = "square root", CompactNodeTitle = "SQRT"))
	static float Sqrt(float A);

	/** Returns square of A (A*A)*/
	UFUNCTION(BlueprintPure, Category="Math|Float", meta=(CompactNodeTitle = "^2"))
	static float Square(float A);

	/** Returns a random float between 0 and 1 */
	UFUNCTION(BlueprintPure, Category="Math|Random", meta=(NotBlueprintThreadSafe))
	static float RandomFloat();

	/** Generate a random number between Min and Max */
	UFUNCTION(BlueprintPure, Category="Math|Random", meta=(NotBlueprintThreadSafe))
	static float RandomFloatInRange(float Min, float Max);

	/** Returns the value of PI */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get PI", CompactNodeTitle = "PI"), Category="Math|Trig")
	static float GetPI();

	/** Returns the value of TAU (= 2 * PI) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get TAU", CompactNodeTitle = "TAU"), Category="Math|Trig")
	static float GetTAU();

	/** Returns radians value based on the input degrees */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Degrees To Radians", CompactNodeTitle = "D2R"), Category="Math|Trig")
	static float DegreesToRadians(float A);

	/** Returns degrees value based on the input radians */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Radians To Degrees", CompactNodeTitle = "R2D"), Category="Math|Trig")
	static float RadiansToDegrees(float A);

	/** Returns the sin of A (expects Degrees)*/
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Sin (Degrees)", CompactNodeTitle = "SINd", Keywords = "sine"), Category="Math|Trig")
	static float DegSin(float A);

	/** Returns the inverse sin (arcsin) of A (result is in Degrees) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Asin (Degrees)", CompactNodeTitle = "ASINd", Keywords = "sine"), Category="Math|Trig")
	static float DegAsin(float A);

	/** Returns the cos of A (expects Degrees)*/
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Cos (Degrees)", CompactNodeTitle = "COSd"), Category="Math|Trig")
	static float DegCos(float A);

	/** Returns the inverse cos (arccos) of A (result is in Degrees) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Acos (Degrees)", CompactNodeTitle = "ACOSd"), Category="Math|Trig")
	static float DegAcos(float A);

	/** Returns the tan of A (expects Degrees)*/
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Tan (Degrees)", CompactNodeTitle = "TANd"), Category="Math|Trig")
	static float DegTan(float A);

	/** Returns the inverse tan (atan) (result is in Degrees)*/
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Atan (Degrees)"), Category="Math|Trig")
	static float DegAtan(float A);

	/** Returns the inverse tan (atan2) of A/B (result is in Degrees)*/
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Atan2 (Degrees)"), Category="Math|Trig")
	static float DegAtan2(float A, float B);

	/** 
	 * Clamps an arbitrary angle to be between the given angles.  Will clamp to nearest boundary.
	 * 
	 * @param MinAngleDegrees	"from" angle that defines the beginning of the range of valid angles (sweeping clockwise)
	 * @param MaxAngleDegrees	"to" angle that defines the end of the range of valid angles
	 * @return Returns clamped angle in the range -180..180.
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Clamp Angle"), Category="Math|Float")
	static float ClampAngle(float AngleDegrees, float MinAngleDegrees, float MaxAngleDegrees);

	/** Returns the minimum value of A and B */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Min (float)", CompactNodeTitle = "MIN", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Float")
	static float FMin(float A, float B);

	/** Returns the maximum value of A and B */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Max (float)", CompactNodeTitle = "MAX", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Float")
	static float FMax(float A, float B);

	/** Returns Value clamped between A and B (inclusive) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Clamp (float)", Min="0.0", Max="1.0"), Category="Math|Float")
	static float FClamp(float Value, float Min, float Max);

	/** Returns max of all array entries and the index at which it was found. Returns value of 0 and index of -1 if the supplied array is empty. */
	UFUNCTION(BlueprintPure, Category="Math|Integer")
	static void MaxOfIntArray(const TArray<int32>& IntArray, int32& IndexOfMaxValue, int32& MaxValue);

	/** Returns min of all array entries and the index at which it was found. Returns value of 0 and index of -1 if the supplied array is empty. */
	UFUNCTION(BlueprintPure, Category="Math|Integer")
	static void MinOfIntArray(const TArray<int32>& IntArray, int32& IndexOfMinValue, int32& MinValue);

	/** Returns max of all array entries and the index at which it was found. Returns value of 0 and index of -1 if the supplied array is empty. */
	UFUNCTION(BlueprintPure, Category="Math|Float")
	static void MaxOfFloatArray(const TArray<float>& FloatArray, int32& IndexOfMaxValue, float& MaxValue);

	/** Returns min of all array entries and the index at which it was found. Returns value of 0 and index of -1 if the supplied array is empty. */
	UFUNCTION(BlueprintPure, Category="Math|Float")
	static void MinOfFloatArray(const TArray<float>& FloatArray, int32& IndexOfMinValue, float& MinValue);

	/** Returns max of all array entries and the index at which it was found. Returns value of 0 and index of -1 if the supplied array is empty. */
	UFUNCTION(BlueprintPure, Category="Math|Byte")
	static void MaxOfByteArray(const TArray<uint8>& ByteArray, int32& IndexOfMaxValue, uint8& MaxValue);

	/** Returns min of all array entries and the index at which it was found. Returns value of 0 and index of -1 if the supplied array is empty. */
	UFUNCTION(BlueprintPure, Category="Math|Byte")
	static void MinOfByteArray(const TArray<uint8>& ByteArray, int32& IndexOfMinValue, uint8& MinValue);

	/** Linearly interpolates between A and B based on Alpha (100% of A when Alpha=0 and 100% of B when Alpha=1) */
	UFUNCTION(BlueprintPure, Category="Math|Float")
	static float Lerp(float A, float B, float Alpha);
	
	/**
	* Returns the fraction (alpha) of the range B-A that corresponds to Value, e.g.,
	*	inputs A = 0, B = 8, Value = 3 : outputs Return Value = 3/8, indicating Value is 3/8 from A to B 
	*	inputs A = 8, B = 0, Value = 3 : outputs Return Value = 5/8, indicating Value is 5/8 from A to B
	* Named InverseLerp because Lerp( A, B, InverseLerp(A, B, Value) ) == Value
	* @param A The "from" value this float could be, usually but not necessarily a minimum. Returned as 0.
	* @param B The "to" value this float could be, usually but not necessarily a maximum. Returned as 1.
	* @param Value A value intended to be normalized relative to B-A
	* @return A normalized alpha value considering A and B.
	*/
	UFUNCTION(BlueprintPure, meta = (Keywords = "percentage normalize range"), Category = "Math|Float")
	static float InverseLerp(float A, float B, float Value);

	/** Easeing  between A and B using a specified easing function */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Ease", BlueprintInternalUseOnly = "true"), Category = "Math|Interpolation")
	static float Ease(float A, float B, float Alpha, TEnumAsByte<EEasingFunc::Type> EasingFunc, float BlendExp = 2, int32 Steps = 2);

	/** Rounds A to the nearest integer */
	UFUNCTION(BlueprintPure, Category="Math|Float")
	static int32 Round(float A);

	/** Rounds A to the largest previous integer */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Floor"), Category="Math|Float")
	static int32 FFloor(float A);
	
	/** Rounds A to an integer with truncation towards zero.  (e.g. -1.7 truncated to -1, 2.8 truncated to 2) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Truncate", BlueprintAutocast), Category="Math|Float")
	static int32 FTrunc(float A);
	
	/** Rounds A to an integer with truncation towards zero for each element in a vector.  (e.g. -1.7 truncated to -1, 2.8 truncated to 2) */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Truncate Vector", BlueprintAutocast), Category = "Math|Float")
	static FIntVector FTruncVector(const FVector& InVector);

	/** Rounds A to the smallest following integer */
	UFUNCTION(BlueprintPure, Category="Math|Float")
	static int32 FCeil(float A);

	/** Returns the number of times Divisor will go into Dividend (i.e., Dividend divided by Divisor), as well as the remainder */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Division (whole and remainder)"), Category="Math|Float")
	static int32 FMod(float Dividend, float Divisor, float& Remainder);

	/** Sign (float, returns -1 if A < 0, 0 if A is zero, and +1 if A > 0) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Sign (float)"), Category="Math|Float")
	static float SignOfFloat(float A);

	/** Returns Value normalized to the given range.  (e.g. 20 normalized to the range 10->50 would result in 0.25) */
	UFUNCTION(BlueprintPure, Category="Math|Float")
	static float NormalizeToRange(float Value, float RangeMin, float RangeMax);

	/** Returns Value mapped from one range into another.  (e.g. 20 normalized from the range 10->50 to 20->40 would result in 25) */
	UFUNCTION(BlueprintPure, Category="Math|Float", meta=(Keywords = "get mapped value"))
	static float MapRangeUnclamped(float Value, float InRangeA, float InRangeB, float OutRangeA, float OutRangeB);

	/** Returns Value mapped from one range into another where the Value is clamped to the Input Range.  (e.g. 0.5 normalized from the range 0->1 to 0->50 would result in 25) */
	UFUNCTION(BlueprintPure, Category="Math|Float", meta=(Keywords = "get mapped value"))
	static float MapRangeClamped(float Value, float InRangeA, float InRangeB, float OutRangeA, float OutRangeB);
	
	/** Multiplies the input value by pi. */
	UFUNCTION(BlueprintPure, meta=(Keywords = "* multiply"), Category="Math|Float")
	static float MultiplyByPi(float Value);

	/** Interpolate between A and B, applying an ease in/out function.  Exp controls the degree of the curve. */
	UFUNCTION(BlueprintPure, Category = "Math|Float")
	static float FInterpEaseInOut(float A, float B, float Alpha, float Exponent);

	/**
	* Simple function to create a pulsating scalar value
	*
	* @param  InCurrentTime  Current absolute time
	* @param  InPulsesPerSecond  How many full pulses per second?
	* @param  InPhase  Optional phase amount, between 0.0 and 1.0 (to synchronize pulses)
	*
	* @return  Pulsating value (0.0-1.0)
	*/
	UFUNCTION(BlueprintPure, Category = "Math|Float")
	static float MakePulsatingValue(float InCurrentTime, float InPulsesPerSecond = 1.0f, float InPhase = 0.0f);

	/** 
	 * Returns a new rotation component value
	 *
	 * @param InCurrent is the current rotation value
	 * @param InDesired is the desired rotation value
	 * @param  is the rotation amount to apply
	 *
	 * @return a new rotation component value clamped in the range (-360,360)
	 */
	UFUNCTION(BlueprintPure, Category="Math|Float")
	static float FixedTurn(float InCurrent, float InDesired, float InDeltaRate);

	//
	// Vector functions.
	//

	/** Scales Vector A by B */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "vector * float", CompactNodeTitle = "*", Keywords = "* multiply"), Category="Math|Vector")
	static FVector Multiply_VectorFloat(FVector A, float B);
	
	/** Scales Vector A by B */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "vector * int", CompactNodeTitle = "*", Keywords = "* multiply"), Category="Math|Vector")
	static FVector Multiply_VectorInt(FVector A, int32 B);

	/** Element-wise Vector multiplication (Result = {A.x*B.x, A.y*B.y, A.z*B.z}) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "vector * vector", CompactNodeTitle = "*", Keywords = "* multiply", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Vector")
	static FVector Multiply_VectorVector(FVector A, FVector B);

	/** Vector divide by a float */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "vector / float", CompactNodeTitle = "/", Keywords = "/ divide division"), Category="Math|Vector")
	static FVector Divide_VectorFloat(FVector A, float B = 1.f);

	/** Vector divide by an integer */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "vector / int", CompactNodeTitle = "/", Keywords = "/ divide division"), Category="Math|Vector")
	static FVector Divide_VectorInt(FVector A, int32 B = 1);
	
	/** Element-wise Vector division (Result = {A.x/B.x, A.y/B.y, A.z/B.z}) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "vector / vector", CompactNodeTitle = "/", Keywords = "/ divide division"), Category="Math|Vector")
	static FVector Divide_VectorVector(FVector A, FVector B = FVector(1.f,1.f,1.f));

	/** Vector addition */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "vector + vector", CompactNodeTitle = "+", Keywords = "+ add plus", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Vector")
	static FVector Add_VectorVector(FVector A, FVector B);

	/** Adds a float to each component of a vector */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "vector + float", CompactNodeTitle = "+", Keywords = "+ add plus"), Category="Math|Vector")
	static FVector Add_VectorFloat(FVector A, float B);
	
	/** Adds an integer to each component of a vector */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "vector + int", CompactNodeTitle = "+", Keywords = "+ add plus"), Category="Math|Vector")
	static FVector Add_VectorInt(FVector A, int32 B);

	/** Vector subtraction */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "vector - vector", CompactNodeTitle = "-", Keywords = "- subtract minus"), Category="Math|Vector")
	static FVector Subtract_VectorVector(FVector A, FVector B);

	/** Subtracts a float from each component of a vector */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "vector - float", CompactNodeTitle = "-", Keywords = "- subtract minus"), Category="Math|Vector")
	static FVector Subtract_VectorFloat(FVector A, float B);

	/** Subtracts an integer from each component of a vector */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "vector - int", CompactNodeTitle = "-", Keywords = "- subtract minus"), Category="Math|Vector")
	static FVector Subtract_VectorInt(FVector A, int32 B);

	/** Returns result of vector A rotated by the inverse of Rotator B */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "UnrotateVector"), Category="Math|Vector")
	static FVector LessLess_VectorRotator(FVector A, FRotator B);

	/** Returns result of vector A rotated by Rotator B */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "RotateVector"), Category="Math|Vector")
	static FVector GreaterGreater_VectorRotator(FVector A, FRotator B);

	/** Returns result of vector A rotated by AngleDeg around Axis */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "RotateVectorAroundAxis"), Category="Math|Vector")
	static FVector RotateAngleAxis(FVector InVect, float AngleDeg, FVector Axis);

	/** Returns true if vector A is equal to vector B (A == B) within a specified error tolerance */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Equal (vector)", CompactNodeTitle = "==", Keywords = "== equal"), Category="Math|Vector")
	static bool EqualEqual_VectorVector(FVector A, FVector B, float ErrorTolerance = 1.e-4f);

	/** Returns true if vector A is not equal to vector B (A != B) within a specified error tolerance */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Not Equal (vector)", CompactNodeTitle = "!=", Keywords = "!= not equal"), Category="Math|Vector")
	static bool NotEqual_VectorVector(FVector A, FVector B, float ErrorTolerance = 1.e-4f);

	/** Returns the dot product of two 3d vectors */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Dot Product", CompactNodeTitle = "dot"), Category="Math|Vector" )
	static float Dot_VectorVector(FVector A, FVector B);

	/** Returns the cross product of two 3d vectors */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Cross Product", CompactNodeTitle = "cross"), Category="Math|Vector" )
	static FVector Cross_VectorVector(FVector A, FVector B);

	/** Returns the dot product of two 2d vectors */
	UFUNCTION(BlueprintPure, meta=(DisplayName="Dot Product (2D)", CompactNodeTitle="dot"), Category="Math|Vector")
	static float DotProduct2D(FVector2D A, FVector2D B);

	/** Returns the cross product of two 2d vectors */
	UFUNCTION(BlueprintPure, meta=(DisplayName="Cross Product (2D)", CompactNodeTitle="cross"), Category="Math|Vector")
	static float CrossProduct2D(FVector2D A, FVector2D B);

	/** Returns the length of the FVector */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "VectorLength", Keywords="magnitude"), Category="Math|Vector")
	static float VSize(FVector A);

	/** Returns the length of a 2d FVector. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Vector2dLength", Keywords="magnitude"), Category="Math|Vector2D")
	static float VSize2D(FVector2D A);

	/** Returns the squared length of the FVector */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "VectorLengthSquared", Keywords="magnitude"), Category="Math|Vector")
	static float VSizeSquared(FVector A);

	/** Returns the squared length of a 2d FVector. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Vector2dLengthSquared", Keywords="magnitude"), Category="Math|Vector2D")
	static float VSize2DSquared(FVector2D A);

	/** Returns a unit normal version of the FVector A */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Normalize", Keywords="Unit Vector"), Category="Math|Vector")
	static FVector Normal(FVector A);

	/** Returns a unit normal version of the vector2d A */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Normalize2D", Keywords="Unit Vector"), Category="Math|Vector2D")
	static FVector2D Normal2D(FVector2D A);

	/** Linearly interpolates between A and B based on Alpha (100% of A when Alpha=0 and 100% of B when Alpha=1) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Lerp (vector)"), Category="Math|Vector")
	static FVector VLerp(FVector A, FVector B, float Alpha);

	/** Easeing  between A and B using a specified easing function */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Ease (vector)", BlueprintInternalUseOnly = "true"), Category = "Math|Interpolation")
	static FVector VEase(FVector A, FVector B, float Alpha, TEnumAsByte<EEasingFunc::Type> EasingFunc, float BlendExp = 2, int32 Steps = 2);

	/** Returns a random vector with length of 1 */
	UFUNCTION(BlueprintPure, Category="Math|Random", meta=(NotBlueprintThreadSafe))
	static FVector RandomUnitVector();

	/** Returns a random point within the specified bounding box */
	UFUNCTION(BlueprintPure, Category = "Math|Random", meta=(NotBlueprintThreadSafe))
	static FVector RandomPointInBoundingBox(const FVector& Origin, const FVector& BoxExtent);

	/** 
	 * Returns a random vector with length of 1, within the specified cone, with uniform random distribution.
	 * @param ConeDir					The base "center" direction of the cone.
	 * @param ConeHalfAngleInRadians	The half-angle of the cone (from ConeDir to edge), in radians.
	 */
	UFUNCTION(BlueprintPure, Category = "Math|Random", meta=(NotBlueprintThreadSafe))
	static FVector RandomUnitVectorInConeInRadians(FVector ConeDir, float ConeHalfAngleInRadians);

	/** 
	 * Returns a random vector with length of 1, within the specified cone, with uniform random distribution.
	 * @param ConeDir					The base "center" direction of the cone.
	 * @param ConeHalfAngleInDegrees	The half-angle of the cone (from ConeDir to edge), in degrees.
	 */
	UFUNCTION(BlueprintPure, Category = "Math|Random", meta=(NotBlueprintThreadSafe))
	static inline FVector RandomUnitVectorInConeInDegrees(FVector ConeDir, float ConeHalfAngleInDegrees)
	{
		return RandomUnitVectorInConeInRadians(ConeDir, FMath::DegreesToRadians(ConeHalfAngleInDegrees));
	}

	/**
	* Returns a random vector with length of 1, within the specified cone, with uniform random distribution.
	* The shape of the cone can be modified according to the yaw and pitch angles.
	*
	* @param MaxYawInRadians	The yaw angle of the cone (from ConeDir to horizontal edge), in radians.
	* @param MaxPitchInRadians	The pitch angle of the cone (from ConeDir to vertical edge), in radians.	
	*/
	UFUNCTION(BlueprintPure, Category = "Math|Random", meta = (Keywords = "RandomVector Pitch Yaw", NotBlueprintThreadSafe))
	static FVector RandomUnitVectorInEllipticalConeInRadians(FVector ConeDir, float MaxYawInRadians, float MaxPitchInRadians);

	/**
	* Returns a random vector with length of 1, within the specified cone, with uniform random distribution.
	* The shape of the cone can be modified according to the yaw and pitch angles.
	*
	* @param MaxYawInDegrees	The yaw angle of the cone (from ConeDir to horizontal edge), in degrees.
	* @param MaxPitchInDegrees	The pitch angle of the cone (from ConeDir to vertical edge), in degrees.	
	*/
	UFUNCTION(BlueprintPure, Category = "Math|Random", meta = (Keywords = "RandomVector Pitch Yaw", NotBlueprintThreadSafe))
	static inline FVector RandomUnitVectorInEllipticalConeInDegrees(FVector ConeDir, float MaxYawInDegrees, float MaxPitchInDegrees)
	{
		return RandomUnitVectorInEllipticalConeInRadians(ConeDir, FMath::DegreesToRadians(MaxYawInDegrees), FMath::DegreesToRadians(MaxPitchInDegrees));
	}

	/** Mirrors a vector by a normal */
	UFUNCTION(BlueprintPure, Category="Math|Vector")
	static FVector MirrorVectorByNormal(FVector InVect, FVector InNormal);

	/**
	* Projects one vector (V) onto another (Target) and returns the projected vector.
	* If Target is nearly zero in length, returns the zero vector.
	*
	* @param  V Vector to project.
	* @param  Target Vector on which we are projecting.
	* @return V projected on to Target.
	*/
	UFUNCTION(BlueprintPure, Category="Math|Vector", meta=(Keywords = "ProjectOnTo"))
	static FVector ProjectVectorOnToVector(FVector V, FVector Target);

	/** 
	 * Given a direction vector and a surface normal, returns the vector reflected across the surface normal.
	 * Produces a result like shining a laser at a mirror!
	 *
	 * @param Direction Direction vector the ray is coming from.
	 * @param SurfaceNormal A normal of the surface the ray should be reflected on.
	 *
	 * @returns Reflected vector.
	 */
	UFUNCTION(BlueprintPure, Category="Math|Vector", meta=(Keywords = "Reflection"))
	static FVector GetReflectionVector(FVector Direction, FVector SurfaceNormal);

	/**
	 * Find closest points between 2 segments.
	 *
	 * @param	Segment1Start	Start of the 1st segment.
	 * @param	Segment1End		End of the 1st segment.
	 * @param	Segment2Start	Start of the 2nd segment.
	 * @param	Segment2End		End of the 2nd segment.
	 * @param	Segment1Point	Closest point on segment 1 to segment 2.
	 * @param	Segment2Point	Closest point on segment 2 to segment 1.
	*/
	UFUNCTION(BlueprintPure, Category = "Math|Vector")
	static void FindNearestPointsOnLineSegments(FVector Segment1Start, FVector Segment1End, FVector Segment2Start, FVector Segment2End, FVector& Segment1Point, FVector& Segment2Point);
	
	/**
	 * Find the closest point on a segment to a given point.
	 *
	 * @param Point			Point for which we find the closest point on the segment.
	 * @param SegmentStart	Start of the segment.
	 * @param SegmentEnd	End of the segment.
	 * @return The closest point on the segment to the given point.
	 */
	UFUNCTION(BlueprintPure, Category = "Math|Vector")
	static FVector FindClosestPointOnSegment(FVector Point, FVector SegmentStart, FVector SegmentEnd);

	/**
	 * Find the closest point on an infinite line to a given point.
	 *
	 * @param Point			Point for which we find the closest point on the line.
	 * @param LineOrigin	Point of reference on the line.
	 * @param LineDirection Direction of the line. Not required to be normalized.
	 * @return The closest point on the line to the given point.
	 */
	UFUNCTION(BlueprintPure, Category = "Math|Vector")
	static FVector FindClosestPointOnLine(FVector Point, FVector LineOrigin, FVector LineDirection);

	/**
	* Find the distance from a point to the closest point on a segment.
	*
	* @param Point			Point for which we find the distance to the closest point on the segment.
	* @param SegmentStart	Start of the segment.
	* @param SegmentEnd		End of the segment.
	* @return The distance from the given point to the closest point on the segment.
	*/
	UFUNCTION(BlueprintPure, Category = "Math|Vector")
	static float GetPointDistanceToSegment(FVector Point, FVector SegmentStart, FVector SegmentEnd);

	/**
	* Find the distance from a point to the closest point on an infinite line.
	*
	* @param Point			Point for which we find the distance to the closest point on the line.
	* @param LineOrigin		Point of reference on the line.
	* @param LineDirection	Direction of the line. Not required to be normalized.
	* @return The distance from the given point to the closest point on the line.
	*/
	UFUNCTION(BlueprintPure, Category = "Math|Vector")
	static float GetPointDistanceToLine(FVector Point, FVector LineOrigin, FVector LineDirection);

	/**
	 * Projects a point onto a plane defined by a point on the plane and a plane normal.
	 *
	 * @param  Point Point to project onto the plane.
	 * @param  PlaneBase A point on the plane.
	 * @param  PlaneNormal Normal of the plane.
	 * @return Point projected onto the plane.
	 */
	UFUNCTION(BlueprintPure, Category = "Math|Vector", meta=(Keywords = "ProjectOnTo"))
	static FVector ProjectPointOnToPlane(FVector Point, FVector PlaneBase, FVector PlaneNormal);

	/**
	* Projects a vector onto a plane defined by a normalized vector (PlaneNormal).
	*
	* @param  V Vector to project onto the plane.
	* @param  PlaneNormal Normal of the plane.
	* @return Vector projected onto the plane.
	*/
	UFUNCTION(BlueprintPure, Category="Math|Vector", meta=(Keywords = "ProjectOnTo"))
	static FVector ProjectVectorOnToPlane(FVector V, FVector PlaneNormal);

	/** Negate a vector. */
	UFUNCTION(BlueprintPure, Category="Math|Vector")
	static FVector NegateVector(FVector A);

	/** Clamp the vector size between a min and max length */
	UFUNCTION(BlueprintPure, Category="Math|Vector")
	static FVector ClampVectorSize(FVector A, float Min, float Max);

	/** Find the minimum element (X, Y or Z) of a vector */
	UFUNCTION(BlueprintPure, Category="Math|Vector")
	static float GetMinElement(FVector A);

	/** Find the maximum element (X, Y or Z) of a vector */
	UFUNCTION(BlueprintPure, Category="Math|Vector")
	static float GetMaxElement(FVector A);

	/** Find the average of an array of vectors */
	UFUNCTION(BlueprintPure, Category="Math|Vector")
	static FVector GetVectorArrayAverage(const TArray<FVector>& Vectors);

	/** Find the unit direction vector from one position to another. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Unit Direction Vector", Keywords = "Unit Vector"), Category="Math|Vector")
	static FVector GetDirectionUnitVector(FVector From, FVector To);

	//
	// Rotator functions.
	//

	/** Returns true if rotator A is equal to rotator B (A == B) within a specified error tolerance */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Equal (Rotator)", CompactNodeTitle = "==", Keywords = "== equal"), Category="Math|Rotator")
	static bool EqualEqual_RotatorRotator(FRotator A, FRotator B, float ErrorTolerance = 1.e-4f);

	/** Returns true if rotator A is not equal to rotator B (A != B) within a specified error tolerance */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Not Equal (Rotator)", CompactNodeTitle = "!=", Keywords = "!= not equal"), Category="Math|Rotator")
	static bool NotEqual_RotatorRotator(FRotator A, FRotator B, float ErrorTolerance = 1.e-4f);

	/** Returns rotator representing rotator A scaled by B */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "ScaleRotator", CompactNodeTitle = "*", Keywords = "* multiply rotate rotation"), Category="Math|Rotator")
	static FRotator Multiply_RotatorFloat(FRotator A, float B);
	
	/** Returns rotator representing rotator A scaled by B */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "ScaleRotator (int)", CompactNodeTitle = "*", Keywords = "* multiply rotate rotation"), Category="Math|Rotator")
	static FRotator Multiply_RotatorInt(FRotator A, int32 B);

	/** Combine 2 rotations to give you the resulting rotation of first applying A, then B. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "CombineRotators", Keywords="rotate rotation add"), Category="Math|Rotator")
	static FRotator ComposeRotators(FRotator A, FRotator B);

	/** Negate a rotator*/
	UFUNCTION(BlueprintPure, Category="Math|Rotator", meta=(DisplayName="InvertRotator", Keywords="rotate rotation"))
	static FRotator NegateRotator(FRotator A);

	/** Get the reference frame direction vectors (axes) described by this rotation */
	UFUNCTION(BlueprintPure, Category="Math|Rotator", meta=(Keywords="rotate rotation"))
	static void GetAxes(FRotator A, FVector& X, FVector& Y, FVector& Z);

	/** Generates a random rotation, with optional random roll. */
	UFUNCTION(BlueprintPure, Category="Math|Random", meta=(Keywords="rotate rotation", NotBlueprintThreadSafe))
	static FRotator RandomRotator(bool bRoll = false);

	/** Linearly interpolates between A and B based on Alpha (100% of A when Alpha=0 and 100% of B when Alpha=1) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Lerp (Rotator)"), Category="Math|Rotator")
	static FRotator RLerp(FRotator A, FRotator B, float Alpha, bool bShortestPath);

	/** Easeing  between A and B using a specified easing function */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Ease (Rotator)", BlueprintInternalUseOnly = "true"), Category = "Math|Interpolation")
	static FRotator REase(FRotator A, FRotator B, float Alpha, bool bShortestPath, TEnumAsByte<EEasingFunc::Type> EasingFunc, float BlendExp = 2, int32 Steps = 2);

	/** Normalized A-B */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Delta (Rotator)"), Category="Math|Rotator")
	static FRotator NormalizedDeltaRotator(FRotator A, FRotator B);

	/** Create a rotation from an axis and and angle (in degrees) */
	UFUNCTION(BlueprintPure, Category="Math|Rotator", meta=(Keywords="make construct build rotate rotation"))
	static FRotator RotatorFromAxisAndAngle(FVector Axis, float Angle);

	/**
	* Clamps an angle to the range of [0, 360].
	*
	* @param Angle The angle to clamp.
	* @return The clamped angle.
	*/
	UFUNCTION(BlueprintPure, Category = "Math|Rotator")
	static float ClampAxis(float Angle);

	/**
	* Clamps an angle to the range of [-180, 180].
	*
	* @param Angle The Angle to clamp.
	* @return The clamped angle.
	*/
	UFUNCTION(BlueprintPure, Category="Math|Rotator")
	static float NormalizeAxis(float Angle);

	//
	//	LinearColor functions
	//

	/** Linearly interpolates between A and B based on Alpha (100% of A when Alpha=0 and 100% of B when Alpha=1) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Lerp (LinearColor)"), Category="Math|Color")
	static FLinearColor LinearColorLerp(FLinearColor A, FLinearColor B, float Alpha);

	/**
	 * Linearly interpolates between two colors by the specified Alpha amount (100% of A when Alpha=0 and 100% of B when Alpha=1).  The interpolation is performed in HSV color space taking the shortest path to the new color's hue.  This can give better results than a normal lerp, but is much more expensive.  The incoming colors are in RGB space, and the output color will be RGB.  The alpha value will also be interpolated.
	 * 
	 * @param	A		The color and alpha to interpolate from as linear RGBA
	 * @param	B		The color and alpha to interpolate to as linear RGBA
	 * @param	Alpha	Scalar interpolation amount (usually between 0.0 and 1.0 inclusive)
	 * 
	 * @return	The interpolated color in linear RGB space along with the interpolated alpha value
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Lerp Using HSV (LinearColor)"), Category="Math|Color")
	static FLinearColor LinearColorLerpUsingHSV(FLinearColor A, FLinearColor B, float Alpha);

	/** Element-wise multiplication of two linear colors (R*R, G*G, B*B, A*A) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "LinearColor * (LinearColor)", CompactNodeTitle = "*"), Category="Math|Color")
	static FLinearColor Multiply_LinearColorLinearColor(FLinearColor A, FLinearColor B);

	/** Element-wise multiplication of a linear color by a float (F*R, F*G, F*B, F*A) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "LinearColor * Float", CompactNodeTitle = "*", Keywords = "* multiply"), Category="Math|Color")
	static FLinearColor Multiply_LinearColorFloat(FLinearColor A, float B);

	//
	// Plane functions.
	//
	
	/** 
	* Creates a plane with a facing direction of Normal at the given Point
	* 
	* @param Point	A point on the plane
	* @param Normal  The Normal of the plane at Point
	* @return Plane instance
	*/
	UFUNCTION(BlueprintPure, Category = "Math|Plane", meta=(Keywords="make plane"))
	static FPlane MakePlaneFromPointAndNormal(FVector Point, FVector Normal);
	
	//
	// DateTime functions.
	//

	/** Makes a DateTime struct */
	UFUNCTION(BlueprintPure, Category="Math|DateTime", meta=(NativeMakeFunc, AdvancedDisplay = "3"))
	static FDateTime MakeDateTime(int32 Year, int32 Month, int32 Day, int32 Hour = 0, int32 Minute = 0, int32 Second = 0, int32 Millisecond = 0);

	/** Breaks a DateTime into its components */
	UFUNCTION(BlueprintPure, Category="Math|DateTime", meta=(NativeBreakFunc))
	static void BreakDateTime(FDateTime InDateTime, int32& Year, int32& Month, int32& Day, int32& Hour, int32& Minute, int32& Second, int32& Millisecond);

	/** Addition (A + B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName="DateTime + Timespan", CompactNodeTitle="+", Keywords="+ add plus"), Category="Math|DateTime")
	static FDateTime Add_DateTimeTimespan( FDateTime A, FTimespan B );

	/** Subtraction (A - B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName="DateTime - Timespan", CompactNodeTitle="-", Keywords="- subtract minus"), Category="Math|DateTime")
	static FDateTime Subtract_DateTimeTimespan(FDateTime A, FTimespan B);

	/** Subtraction (A - B) */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "DateTime - DateTime", CompactNodeTitle = "-", Keywords = "- subtract minus"), Category = "Math|DateTime")
	static FTimespan Subtract_DateTimeDateTime(FDateTime A, FDateTime B);

	/** Returns true if the values are equal (A == B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName="Equal (DateTime)", CompactNodeTitle="==", Keywords="== equal"), Category="Math|DateTime")
	static bool EqualEqual_DateTimeDateTime( FDateTime A, FDateTime B );

	/** Returns true if the values are not equal (A != B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName="NotEqual (DateTime)", CompactNodeTitle="!=", Keywords="!= not equal"), Category="Math|DateTime")
	static bool NotEqual_DateTimeDateTime( FDateTime A, FDateTime B );

	/** Returns true if A is greater than B (A > B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName="DateTime > DateTime", CompactNodeTitle=">", Keywords="> greater"), Category="Math|DateTime")
	static bool Greater_DateTimeDateTime( FDateTime A, FDateTime B );

	/** Returns true if A is greater than or equal to B (A >= B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName="DateTime >= DateTime", CompactNodeTitle=">=", Keywords=">= greater"), Category="Math|DateTime")
	static bool GreaterEqual_DateTimeDateTime( FDateTime A, FDateTime B );

	/** Returns true if A is less than B (A < B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName="DateTime < DateTime", CompactNodeTitle="<", Keywords="< less"), Category="Math|DateTime")
	static bool Less_DateTimeDateTime( FDateTime A, FDateTime B );

	/** Returns true if A is less than or equal to B (A <= B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName="DateTime <= DateTime", CompactNodeTitle="<=", Keywords="<= less"), Category="Math|DateTime")
	static bool LessEqual_DateTimeDateTime( FDateTime A, FDateTime B );

	/** Returns the date component of A */
	UFUNCTION(BlueprintPure, meta=(DisplayName="GetDate"), Category="Math|DateTime")
	static FDateTime GetDate( FDateTime A );

	/** Returns the day component of A (1 to 31) */
	UFUNCTION(BlueprintPure, meta=(DisplayName="GetDay"), Category="Math|DateTime")
	static int32 GetDay( FDateTime A );

	/** Returns the day of year of A */
	UFUNCTION(BlueprintPure, meta=(DisplayName="GetDayOfYear"), Category="Math|DateTime")
	static int32 GetDayOfYear( FDateTime A );

	/** Returns the hour component of A (24h format) */
	UFUNCTION(BlueprintPure, meta=(DisplayName="GetHour"), Category="Math|DateTime")
	static int32 GetHour( FDateTime A );

	/** Returns the hour component of A (12h format) */
	UFUNCTION(BlueprintPure, meta=(DisplayName="GetHour12"), Category="Math|DateTime")
	static int32 GetHour12( FDateTime A );

	/** Returns the millisecond component of A */
	UFUNCTION(BlueprintPure, meta=(DisplayName="GetMillisecond"), Category="Math|DateTime")
	static int32 GetMillisecond( FDateTime A );

	/** Returns the minute component of A */
	UFUNCTION(BlueprintPure, meta=(DisplayName="GetMinute"), Category="Math|DateTime")
	static int32 GetMinute( FDateTime A );

	/** Returns the month component of A */
	UFUNCTION(BlueprintPure, meta=(DisplayName="GetMonth"), Category="Math|DateTime")
	static int32 GetMonth( FDateTime A );

	/** Returns the second component of A */
	UFUNCTION(BlueprintPure, meta=(DisplayName="GetSecond"), Category="Math|DateTime")
	static int32 GetSecond( FDateTime A );

	/** Returns the time elapsed since midnight of A */
	UFUNCTION(BlueprintPure, meta=(DisplayName="GetTimeOfDay"), Category="Math|DateTime")
	static FTimespan GetTimeOfDay( FDateTime A );

	/** Returns the year component of A */
	UFUNCTION(BlueprintPure, meta=(DisplayName="GetYear"), Category="Math|DateTime")
	static int32 GetYear( FDateTime A );

	/** Returns whether A's time is in the afternoon */
	UFUNCTION(BlueprintPure, meta=(DisplayName="IsAfternoon"), Category="Math|DateTime")
	static bool IsAfternoon( FDateTime A );

	/** Returns whether A's time is in the morning */
	UFUNCTION(BlueprintPure, meta=(DisplayName="IsMorning"), Category="Math|DateTime")
	static bool IsMorning( FDateTime A );

	/** Returns the number of days in the given year and month */
	UFUNCTION(BlueprintPure, meta=(DisplayName="DaysInMonth"), Category="Math|DateTime")
	static int32 DaysInMonth( int32 Year, int32 Month );

	/** Returns the number of days in the given year */
	UFUNCTION(BlueprintPure, meta=(DisplayName="DaysInYear"), Category="Math|DateTime")
	static int32 DaysInYear( int32 Year );

	/** Returns whether given year is a leap year */
	UFUNCTION(BlueprintPure, meta=(DisplayName="IsLeapYear"), Category="Math|DateTime")
	static bool IsLeapYear( int32 Year );

	/** Returns the maximum date and time value */
	UFUNCTION(BlueprintPure, meta=(DisplayName="MaxValue"), Category="Math|DateTime")
	static FDateTime DateTimeMaxValue( );

	/** Returns the minimum date and time value */
	UFUNCTION(BlueprintPure, meta=(DisplayName="MinValue"), Category="Math|DateTime")
	static FDateTime DateTimeMinValue( );

	/** Returns the local date and time on this computer */
	UFUNCTION(BlueprintPure, meta=(DisplayName="Now"), Category="Math|DateTime")
	static FDateTime Now( );

	/** Returns the local date on this computer */
	UFUNCTION(BlueprintPure, meta=(DisplayName="Today"), Category="Math|DateTime")
	static FDateTime Today( );

	/** Returns the UTC date and time on this computer */
	UFUNCTION(BlueprintPure, meta=(DisplayName="UtcNow"), Category="Math|DateTime")
	static FDateTime UtcNow( );

	/** Converts a date string in ISO-8601 format to a DateTime object */
	UFUNCTION(BlueprintPure, Category="Math|DateTime")
	static bool DateTimeFromIsoString(FString IsoString, FDateTime& Result);

	/** Converts a date string to a DateTime object */
	UFUNCTION(BlueprintPure, Category="Math|DateTime")
	static bool DateTimeFromString(FString DateTimeString, FDateTime& Result);

	//
	// Timespan functions.
	//

	/** Makes a Timespan struct */
	UFUNCTION(BlueprintPure, Category="Math|Timespan", meta=(NativeMakeFunc))
	static FTimespan MakeTimespan(int32 Days, int32 Hours, int32 Minutes, int32 Seconds, int32 Milliseconds);

	/** Makes a Timespan struct */
	UFUNCTION(BlueprintPure, Category="Math|Timespan", meta=(NativeMakeFunc))
	static FTimespan MakeTimespan2(int32 Days, int32 Hours, int32 Minutes, int32 Seconds, int32 FractionNano);

	/** Breaks a Timespan into its components */
	UFUNCTION(BlueprintPure, Category="Math|Timespan", meta=(NativeBreakFunc))
	static void BreakTimespan(FTimespan InTimespan, int32& Days, int32& Hours, int32& Minutes, int32& Seconds, int32& Milliseconds);

	/** Breaks a Timespan into its components */
	UFUNCTION(BlueprintPure, Category="Math|Timespan", meta=(NativeBreakFunc))
	static void BreakTimespan2(FTimespan InTimespan, int32& Days, int32& Hours, int32& Minutes, int32& Seconds, int32& FractionNano);

	/** Addition (A + B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName="Timespan + Timespan", CompactNodeTitle="+", Keywords="+ add plus"), Category="Math|Timespan")
	static FTimespan Add_TimespanTimespan( FTimespan A, FTimespan B );

	/** Subtraction (A - B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName="Timespan - Timespan", CompactNodeTitle="-", Keywords="- subtract minus"), Category="Math|Timespan")
	static FTimespan Subtract_TimespanTimespan( FTimespan A, FTimespan B );

	/** Scalar multiplication (A * s) */
	UFUNCTION(BlueprintPure, meta=(DisplayName="Timespan * float", CompactNodeTitle="*", Keywords="* multiply"), Category="Math|Timespan")
	static FTimespan Multiply_TimespanFloat( FTimespan A, float Scalar );

	/** Scalar division (A * s) */
	UFUNCTION(BlueprintPure, meta=(DisplayName="Timespan * float", CompactNodeTitle="/", Keywords="/ divide"), Category="Math|Timespan")
	static FTimespan Divide_TimespanFloat( FTimespan A, float Scalar );

	/** Returns true if the values are equal (A == B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName="Equal (Timespan)", CompactNodeTitle="==", Keywords="== equal"), Category="Math|Timespan")
	static bool EqualEqual_TimespanTimespan( FTimespan A, FTimespan B );

	/** Returns true if the values are not equal (A != B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName="NotEqual (Timespan)", CompactNodeTitle="!=", Keywords="!= not equal"), Category="Math|Timespan")
	static bool NotEqual_TimespanTimespan( FTimespan A, FTimespan B );

	/** Returns true if A is greater than B (A > B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName="Timespan > Timespan", CompactNodeTitle=">", Keywords="> greater"), Category="Math|Timespan")
	static bool Greater_TimespanTimespan( FTimespan A, FTimespan B );

	/** Returns true if A is greater than or equal to B (A >= B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName="Timespan >= Timespan", CompactNodeTitle=">=", Keywords=">= greater"), Category="Math|Timespan")
	static bool GreaterEqual_TimespanTimespan( FTimespan A, FTimespan B );

	/** Returns true if A is less than B (A < B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName="Timespan < Timespan", CompactNodeTitle="<", Keywords="< less"), Category="Math|Timespan")
	static bool Less_TimespanTimespan( FTimespan A, FTimespan B );

	/** Returns true if A is less than or equal to B (A <= B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName="Timespan <= Timespan", CompactNodeTitle="<=", Keywords="<= less"), Category="Math|Timespan")
	static bool LessEqual_TimespanTimespan( FTimespan A, FTimespan B );

	/** Returns the days component of A */
	UFUNCTION(BlueprintPure, meta=(DisplayName="GetDays"), Category="Math|Timespan")
	static int32 GetDays( FTimespan A );

	/** Returns the absolute value of A */
	UFUNCTION(BlueprintPure, meta=(DisplayName="GetDuration"), Category="Math|Timespan")
	static FTimespan GetDuration( FTimespan A );

	/** Returns the hours component of A */
	UFUNCTION(BlueprintPure, meta=(DisplayName="GetHours"), Category="Math|Timespan")
	static int32 GetHours( FTimespan A );

	/** Returns the milliseconds component of A */
	UFUNCTION(BlueprintPure, meta=(DisplayName="GetMilliseconds"), Category="Math|Timespan")
	static int32 GetMilliseconds( FTimespan A );

	/** Returns the minutes component of A */
	UFUNCTION(BlueprintPure, meta=(DisplayName="GetMinutes"), Category="Math|Timespan")
	static int32 GetMinutes( FTimespan A );

	/** Returns the seconds component of A */
	UFUNCTION(BlueprintPure, meta=(DisplayName="GetSeconds"), Category="Math|Timespan")
	static int32 GetSeconds( FTimespan A );

	/** Returns the total number of days in A */
	UFUNCTION(BlueprintPure, meta=(DisplayName="GetTotalDays"), Category="Math|Timespan")
	static float GetTotalDays( FTimespan A );

	/** Returns the total number of hours in A */
	UFUNCTION(BlueprintPure, meta=(DisplayName="GetTotalHours"), Category="Math|Timespan")
	static float GetTotalHours( FTimespan A );

	/** Returns the total number of milliseconds in A */
	UFUNCTION(BlueprintPure, meta=(DisplayName="GetTotalMilliseconds"), Category="Math|Timespan")
	static float GetTotalMilliseconds( FTimespan A );

	/** Returns the total number of minutes in A */
	UFUNCTION(BlueprintPure, meta=(DisplayName="GetTotalMinutes"), Category="Math|Timespan")
	static float GetTotalMinutes( FTimespan A );

	/** Returns the total number of seconds in A */
	UFUNCTION(BlueprintPure, meta=(DisplayName="GetTotalSeconds"), Category="Math|Timespan")
	static float GetTotalSeconds( FTimespan A );

	/** Returns a time span that represents the specified number of days */
	UFUNCTION(BlueprintPure, meta=(DisplayName="FromDays"), Category="Math|Timespan")
	static FTimespan FromDays( float Days );

	/** Returns a time span that represents the specified number of hours */
	UFUNCTION(BlueprintPure, meta=(DisplayName="FromHours"), Category="Math|Timespan")
	static FTimespan FromHours( float Hours );

	/** Returns a time span that represents the specified number of milliseconds */
	UFUNCTION(BlueprintPure, meta=(DisplayName="FromMilliseconds"), Category="Math|Timespan")
	static FTimespan FromMilliseconds( float Milliseconds );

	/** Returns a time span that represents the specified number of minutes */
	UFUNCTION(BlueprintPure, meta=(DisplayName="FromMinutes"), Category="Math|Timespan")
	static FTimespan FromMinutes( float Minutes );

	/** Returns a time span that represents the specified number of seconds */
	UFUNCTION(BlueprintPure, meta=(DisplayName="FromSeconds"), Category="Math|Timespan")
	static FTimespan FromSeconds( float Seconds );

	/** Returns the maximum time span value */
	UFUNCTION(BlueprintPure, meta=(DisplayName="MaxValue"), Category="Math|Timespan")
	static FTimespan TimespanMaxValue( );

	/** Returns the minimum time span value */
	UFUNCTION(BlueprintPure, meta=(DisplayName="MinValue"), Category="Math|Timespan")
	static FTimespan TimespanMinValue( );

	/** Returns the ratio between two time spans (A / B), handles zero values */
	UFUNCTION(BlueprintPure, meta=(DisplayName="TimespanRatio"), Category="Math|Timespan")
	static float TimespanRatio( FTimespan A, FTimespan B );

	/** Returns a zero time span value */
	UFUNCTION(BlueprintPure, meta=(DisplayName="ZeroValue"), Category="Math|Timespan")
	static FTimespan TimespanZeroValue( );

	/** Converts a time span string to a Timespan object */
	UFUNCTION(BlueprintPure, Category="Math|Timespan")
	static bool TimespanFromString(FString TimespanString, FTimespan& Result);


	// -- Begin K2 utilities

	/** Converts a byte to a float */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "ToFloat (byte)", CompactNodeTitle = "->", Keywords="cast convert", BlueprintAutocast), Category="Math|Conversions")
	static float Conv_ByteToFloat(uint8 InByte);

	/** Converts an integer to a float */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "ToFloat (int)", CompactNodeTitle = "->", Keywords="cast convert", BlueprintAutocast), Category="Math|Conversions")
	static float Conv_IntToFloat(int32 InInt);

	/** Converts an integer to a byte (if the integer is too large, returns the low 8 bits) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "ToByte (int)", CompactNodeTitle = "->", Keywords="cast convert", BlueprintAutocast), Category="Math|Conversions")
	static uint8 Conv_IntToByte(int32 InInt);

	/** Converts an integer to an IntVector*/
	UFUNCTION(BlueprintPure, meta = (DisplayName = "ToIntVector (int)", CompactNodeTitle = "->", Keywords = "cast convert", BlueprintAutocast), Category = "Math|Conversions")
	static FIntVector Conv_IntToIntVector(int32 InInt);

	/** Converts a int to a bool*/
	UFUNCTION(BlueprintPure, meta=(DisplayName = "ToBool (int)", CompactNodeTitle = "->", Keywords="cast convert", BlueprintAutocast), Category="Math|Conversions")
	static bool Conv_IntToBool(int32 InInt);

	/** Converts a bool to an int */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "ToInt (bool)", CompactNodeTitle = "->", Keywords="cast convert", BlueprintAutocast), Category="Math|Conversions")
	static int32 Conv_BoolToInt(bool InBool);

	/** Converts a bool to a float (0.0f or 1.0f) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "ToFloat (bool)", CompactNodeTitle = "->", Keywords="cast convert", BlueprintAutocast), Category="Math|Conversions")
	static float Conv_BoolToFloat(bool InBool);

	/** Converts a bool to a byte */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "ToByte (bool)", CompactNodeTitle = "->", Keywords="cast convert", BlueprintAutocast), Category="Math|Conversions")
	static uint8 Conv_BoolToByte(bool InBool);
	
	/** Converts a byte to an integer */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "ToInt (byte)", CompactNodeTitle = "->", Keywords="cast convert", BlueprintAutocast), Category="Math|Conversions")
	static int32 Conv_ByteToInt(uint8 InByte);

	/** Converts a vector to LinearColor */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "ToLinearColor (vector)", CompactNodeTitle = "->", Keywords="cast convert", BlueprintAutocast), Category="Math|Conversions")
	static FLinearColor Conv_VectorToLinearColor(FVector InVec);

	/** Converts a LinearColor to a vector */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "ToVector (linear color)", CompactNodeTitle = "->", Keywords="cast convert", BlueprintAutocast), Category="Math|Conversions")
	static FVector Conv_LinearColorToVector(FLinearColor InLinearColor);

	/** Converts a color to LinearColor */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "ToLinearColor (color)", CompactNodeTitle = "->", Keywords="cast convert", BlueprintAutocast), Category="Math|Conversions")
	static FLinearColor Conv_ColorToLinearColor(FColor InColor);

	/** Converts a LinearColor to a color*/
	UFUNCTION(BlueprintPure, meta=(DisplayName = "ToColor (linear color)", CompactNodeTitle = "->", Keywords="cast convert", BlueprintAutocast), Category="Math|Conversions")
	static FColor Conv_LinearColorToColor(FLinearColor InLinearColor);

	/** Convert a vector to a transform. Uses vector as location */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "ToTransform (vector)", CompactNodeTitle = "->", Keywords="cast convert", BlueprintAutocast), Category="Math|Conversions")
	static FTransform Conv_VectorToTransform(FVector InLocation);
	
	/** Convert a Vector to a Vector2D */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "ToVector2D (Vector)", CompactNodeTitle = "->", Keywords="cast convert", BlueprintAutocast), Category="Math|Conversions")
	static FVector2D Conv_VectorToVector2D(FVector InVector);

	/** Convert a Vector2D to a Vector */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "ToVector (Vector2D)", CompactNodeTitle = "->", Keywords="cast convert", BlueprintAutocast), Category="Math|Conversions")
	static FVector Conv_Vector2DToVector(FVector2D InVector2D, float Z = 0);

	/** Convert an IntVector to a vector */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "ToVector (IntVector)", CompactNodeTitle = "->", Keywords = "cast convert", BlueprintAutocast), Category = "Math|Conversions")
	static FVector Conv_IntVectorToVector(const FIntVector& InIntVector);

	/** Convert a float into a vector, where each element is that float */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "ToVector (float)", CompactNodeTitle = "->", Keywords="cast convert", BlueprintAutocast), Category="Math|Conversions")
	static FVector Conv_FloatToVector(float InFloat);

	/** Convert a float into a LinearColor, where each element is that float */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "ToLinearColor (float)", CompactNodeTitle = "->", Keywords="cast convert", BlueprintAutocast), Category="Math|Conversions")
	static FLinearColor Conv_FloatToLinearColor(float InFloat);

	/** Makes an FBox from Min and Max and sets IsValid to true */
	UFUNCTION(BlueprintPure, Category="Math|Box", meta=(Keywords="construct build", NativeMakeFunc))
	static FBox MakeBox(FVector Min, FVector Max);

	/** Makes an FBox2D from Min and Max and sets IsValid to true */
	UFUNCTION(BlueprintPure, Category = "Math|Box2D", meta = (Keywords = "construct build", NativeMakeFunc))
	static FBox2D MakeBox2D(FVector2D Min, FVector2D Max);

	/** Makes a vector {X, Y, Z} */
	UFUNCTION(BlueprintPure, Category="Math|Vector", meta=(Keywords="construct build", NativeMakeFunc))
	static FVector MakeVector(float X, float Y, float Z);

	/** Breaks a vector apart into X, Y, Z */
	UFUNCTION(BlueprintPure, Category="Math|Vector", meta=(NativeBreakFunc))
	static void BreakVector(FVector InVec, float& X, float& Y, float& Z);

	/** Makes a 2d vector {X, Y} */
	UFUNCTION(BlueprintPure, Category="Math|Vector2D", meta=(Keywords="construct build", NativeMakeFunc))
	static FVector2D MakeVector2D(float X, float Y);

	/** Breaks a 2D vector apart into X, Y. */
	UFUNCTION(BlueprintPure, Category="Math|Vector2D", meta=(NativeBreakFunc))
	static void BreakVector2D(FVector2D InVec, float& X, float& Y);

	/** Rotate the world forward vector by the given rotation */
	UFUNCTION(BlueprintPure, Category="Math|Vector", meta=(Keywords="rotation rotate"))
	static FVector GetForwardVector(FRotator InRot);

	/** Rotate the world right vector by the given rotation */
	UFUNCTION(BlueprintPure, Category="Math|Vector", meta=(Keywords="rotation rotate"))
	static FVector GetRightVector(FRotator InRot);

	/** Rotate the world up vector by the given rotation */
	UFUNCTION(BlueprintPure, Category="Math|Vector", meta=(Keywords="rotation rotate"))
	static FVector GetUpVector(FRotator InRot);

	/** Creates a directional vector from rotation values {Pitch, Yaw} supplied in degrees with specified Length*/	
	UFUNCTION(BlueprintPure, Category = "Math|Vector", meta = (Keywords = "rotation rotate"))
	static FVector CreateVectorFromYawPitch(float Yaw, float Pitch, float Length = 1.0f );

	/** Breaks a vector apart into Yaw, Pitch rotation values given in degrees. (non-clamped) */
	UFUNCTION(BlueprintPure, Category = "Math|Vector2D", meta = (NativeBreakFunc))
	static void GetYawPitchFromVector(FVector InVec, float& Yaw, float& Pitch);

	/** Breaks a direction vector apart into Azimuth (Yaw) and Elevation (Pitch) rotation values given in degrees. (non-clamped)
	 Relative to the provided reference frame (an Actor's WorldTransform for example) */
	UFUNCTION(BlueprintPure, Category = "Math|Vector", meta = (NativeBreakFunc))
	static void GetAzimuthAndElevation(FVector InDirection, const FTransform& ReferenceFrame, float& Azimuth, float& Elevation);

	/** Makes a rotator {Roll, Pitch, Yaw} from rotation values supplied in degrees */
	UFUNCTION(BlueprintPure, Category="Math|Rotator", meta=(Keywords="construct build rotation rotate rotator makerotator", NativeMakeFunc))
	static FRotator MakeRotator(
		UPARAM(DisplayName="X (Roll)") float Roll,	
		UPARAM(DisplayName="Y (Pitch)") float Pitch,
		UPARAM(DisplayName="Z (Yaw)") float Yaw);

	/** Find a rotation for an object at Start location to point at Target location. */
	UFUNCTION(BlueprintPure, Category="Math|Rotator", meta=(Keywords="rotation rotate"))
	static FRotator FindLookAtRotation(const FVector& Start, const FVector& Target);

	/** Builds a rotator given only a XAxis. Y and Z are unspecified but will be orthonormal. XAxis need not be normalized. */
	UFUNCTION(BlueprintPure, Category="Math|Rotator", meta=(Keywords="construct build rotation rotate rotator makerotator"))
	static FRotator MakeRotFromX(const FVector& X);

	/** Builds a rotation matrix given only a YAxis. X and Z are unspecified but will be orthonormal. YAxis need not be normalized. */
	UFUNCTION(BlueprintPure, Category="Math|Rotator", meta=(Keywords="construct build rotation rotate rotator makerotator"))
	static FRotator MakeRotFromY(const FVector& Y);

	/** Builds a rotation matrix given only a ZAxis. X and Y are unspecified but will be orthonormal. ZAxis need not be normalized. */
	UFUNCTION(BlueprintPure, Category="Math|Rotator", meta=(Keywords="construct build rotation rotate rotator makerotator"))
	static FRotator MakeRotFromZ(const FVector& Z);

	/** Builds a matrix with given X and Y axes. X will remain fixed, Y may be changed minimally to enforce orthogonality. Z will be computed. Inputs need not be normalized. */
	UFUNCTION(BlueprintPure, Category="Math|Rotator", meta=(Keywords="construct build rotation rotate rotator makerotator"))
	static FRotator MakeRotFromXY(const FVector& X, const FVector& Y);

	/** Builds a matrix with given X and Z axes. X will remain fixed, Z may be changed minimally to enforce orthogonality. Y will be computed. Inputs need not be normalized. */
	UFUNCTION(BlueprintPure, Category="Math|Rotator", meta=(Keywords="construct build rotation rotate rotator makerotator"))
	static FRotator MakeRotFromXZ(const FVector& X, const FVector& Z);

	/** Builds a matrix with given Y and X axes. Y will remain fixed, X may be changed minimally to enforce orthogonality. Z will be computed. Inputs need not be normalized. */
	UFUNCTION(BlueprintPure, Category="Math|Rotator", meta=(Keywords="construct build rotation rotate rotator makerotator"))
	static FRotator MakeRotFromYX(const FVector& Y, const FVector& X);

	/** Builds a matrix with given Y and Z axes. Y will remain fixed, Z may be changed minimally to enforce orthogonality. X will be computed. Inputs need not be normalized. */
	UFUNCTION(BlueprintPure, Category="Math|Rotator", meta=(Keywords="construct build rotation rotate rotator makerotator"))
	static FRotator MakeRotFromYZ(const FVector& Y, const FVector& Z);

	/** Builds a matrix with given Z and X axes. Z will remain fixed, X may be changed minimally to enforce orthogonality. Y will be computed. Inputs need not be normalized. */
	UFUNCTION(BlueprintPure, Category="Math|Rotator", meta=(Keywords="construct build rotation rotate rotator makerotator"))
	static FRotator MakeRotFromZX(const FVector& Z, const FVector& X);

	/** Builds a matrix with given Z and Y axes. Z will remain fixed, Y may be changed minimally to enforce orthogonality. X will be computed. Inputs need not be normalized. */
	UFUNCTION(BlueprintPure, Category="Math|Rotator", meta=(Keywords="construct build rotation rotate rotator makerotator"))
	static FRotator MakeRotFromZY(const FVector& Z, const FVector& Y);

	/** Breaks apart a rotator into {Roll, Pitch, Yaw} angles in degrees */
	UFUNCTION(BlueprintPure, Category = "Math|Rotator", meta = (Keywords = "rotation rotate rotator breakrotator", NativeBreakFunc))
	static void BreakRotator(
		UPARAM(DisplayName="Rotation") FRotator InRot,
		UPARAM(DisplayName="X (Roll)") float& Roll,
		UPARAM(DisplayName="Y (Pitch)") float& Pitch,
		UPARAM(DisplayName="Z (Yaw)") float& Yaw);

	/** Breaks apart a rotator into its component axes */
	UFUNCTION(BlueprintPure, Category="Math|Rotator", meta=(Keywords="rotation rotate rotator breakrotator"))
	static void BreakRotIntoAxes(const FRotator& InRot, FVector& X, FVector& Y, FVector& Z);

	/** Make a transform from location, rotation and scale */
	UFUNCTION(BlueprintPure, meta=(Scale = "1,1,1", Keywords="construct build", NativeMakeFunc), Category="Math|Transform")
	static FTransform MakeTransform(FVector Location, FRotator Rotation, FVector Scale);

	/** Breaks apart a transform into location, rotation and scale */
	UFUNCTION(BlueprintPure, Category="Math|Transform", meta=(NativeBreakFunc))
	static void BreakTransform(const FTransform& InTransform, FVector& Location, FRotator& Rotation, FVector& Scale);

	/** Makes a SRand-based random number generator */
	UFUNCTION(BlueprintPure, meta = (Keywords = "construct build", NativeMakeFunc), Category = "Math|Random")
		static FRandomStream MakeRandomStream(int32 InitialSeed);

	/** Breaks apart a random number generator */
	UFUNCTION(BlueprintPure, Category = "Math|Random", meta = (NativeBreakFunc))
		static void BreakRandomStream(const FRandomStream& InRandomStream, int32& InitialSeed);

	/** Make a color from individual color components (RGB space) */
	UFUNCTION(BlueprintPure, Category="Math|Color", meta=(Keywords="construct build"))
	static FLinearColor MakeColor(float R, float G, float B, float A = 1.0f);

	/** Breaks apart a color into individual RGB components (as well as alpha) */
	UFUNCTION(BlueprintPure, Category="Math|Color")
	static void BreakColor(const FLinearColor InColor, float& R, float& G, float& B, float& A);

	/** Make a color from individual color components (HSV space) */
	UFUNCTION(BlueprintPure, Category="Math|Color", meta=(DisplayName = "HSV to RGB"))
	static FLinearColor HSVToRGB(float H, float S, float V, float A = 1.0f);

	/** Breaks apart a color into individual HSV components (as well as alpha) */
	UFUNCTION(BlueprintPure, Category="Math|Color", meta=(DisplayName = "RGB to HSV"))
	static void RGBToHSV(const FLinearColor InColor, float& H, float& S, float& V, float& A);

	/** Converts a HSV linear color (where H is in R, S is in G, and V is in B) to RGB */
	UFUNCTION(BlueprintPure, Category="Math|Color", meta=(DisplayName = "HSV to RGB (vector)", Keywords="cast convert"))
	static void HSVToRGB_Vector(const FLinearColor HSV, FLinearColor& RGB);

	/** Converts a RGB linear color to HSV (where H is in R, S is in G, and V is in B) */
	UFUNCTION(BlueprintPure, Category="Math|Color", meta=(DisplayName = "RGB to HSV (vector)", Keywords="cast convert"))
	static void RGBToHSV_Vector(const FLinearColor RGB, FLinearColor& HSV);

	/** If bPickA is true, A is returned, otherwise B is */
	UFUNCTION(BlueprintPure, Category="Utilities|String")
	static FString SelectString(const FString& A, const FString& B, bool bPickA);

	/** If bPickA is true, A is returned, otherwise B is */
	UFUNCTION(BlueprintPure, Category="Math|Integer")
	static int32 SelectInt(int32 A, int32 B, bool bPickA);

	/** If bPickA is true, A is returned, otherwise B is */
	UFUNCTION(BlueprintPure, Category="Math|Float")
	static float SelectFloat(float A, float B, bool bPickA);

	/** If bPickA is true, A is returned, otherwise B is */
	UFUNCTION(BlueprintPure, Category="Math|Vector")
	static FVector SelectVector(FVector A, FVector B, bool bPickA);

	/** If bPickA is true, A is returned, otherwise B is */
	UFUNCTION(BlueprintPure, Category="Math|Rotator", meta=(Keywords="rotation rotate"))
	static FRotator SelectRotator(FRotator A, FRotator B, bool bPickA);

	/** If bPickA is true, A is returned, otherwise B is */
	UFUNCTION(BlueprintPure, Category="Math|Color")
	static FLinearColor SelectColor(FLinearColor A, FLinearColor B, bool bPickA);

	/** If bPickA is true, A is returned, otherwise B is */
	UFUNCTION(BlueprintPure, Category="Math|Transform")
	static FTransform SelectTransform(const FTransform& A, const FTransform& B, bool bPickA);

	/** If bPickA is true, A is returned, otherwise B is */
	UFUNCTION(BlueprintPure, Category="Utilities")
	static UObject* SelectObject(UObject* A, UObject* B, bool bSelectA);

	/** If bPickA is true, A is returned, otherwise B is */
	UFUNCTION(BlueprintPure, Category = "Utilities")
	static UClass* SelectClass(UClass* A, UClass* B, bool bSelectA);

	// Build a reference frame from three axes
	UFUNCTION(BlueprintPure, Category="Math|Rotator", meta=(Keywords="construct build rotation rotate"))
	static FRotator MakeRotationFromAxes(FVector Forward, FVector Right, FVector Up);

	/** Create a rotator which orients X along the supplied direction vector */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "RotationFromXVector", Keywords="rotation rotate cast convert", BlueprintAutocast), Category="Math|Rotator")
	static FRotator Conv_VectorToRotator(FVector InVec);

	/** Get the X direction vector after this rotation */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "GetRotationXVector", Keywords="rotation rotate cast convert", BlueprintAutocast), Category="Math|Rotator")
	static FVector Conv_RotatorToVector(FRotator InRot);

	//
	// Object operators and functions.
	//
	
	/** Returns true if A and B are equal (A == B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Equal (Object)", CompactNodeTitle = "==", Keywords = "== equal"), Category="Utilities")
	static bool EqualEqual_ObjectObject(class UObject* A, class UObject* B);

	/** Returns true if A and B are not equal (A != B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "NotEqual (Object)", CompactNodeTitle = "!=", Keywords = "!= not equal"), Category="Utilities")
	static bool NotEqual_ObjectObject(class UObject* A, class UObject* B);

	//
	// Class operators and functions.
	//

	/** Returns true if A and B are equal (A == B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Equal (Class)", CompactNodeTitle = "==", Keywords = "== equal"), Category="Utilities")
	static bool EqualEqual_ClassClass(class UClass* A, class UClass* B);

	/** Returns true if A and B are not equal (A != B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "NotEqual (Class)", CompactNodeTitle = "!=", Keywords = "!= not equal"), Category="Utilities")
	static bool NotEqual_ClassClass(class UClass* A, class UClass* B);

	/**
	 * Determine if a class is a child of another class.
	 *
	 * @return	true if TestClass == ParentClass, or if TestClass is a child of ParentClass; false otherwise, or if either
	 *			the value for either parameter is 'None'.
	 */
	UFUNCTION(BlueprintPure, Category="Utilities")
	static bool ClassIsChildOf(TSubclassOf<class UObject> TestClass, TSubclassOf<class UObject> ParentClass);

	//
	// Name operators.
	//
	
	/** Returns true if A and B are equal (A == B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Equal (Name)", CompactNodeTitle = "==", Keywords = "== equal"), Category="Utilities|Name")
	static bool EqualEqual_NameName(FName A, FName B);

	/** Returns true if A and B are not equal (A != B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "NotEqual (Name)", CompactNodeTitle = "!=", Keywords = "!= not equal"), Category="Utilities|Name")
	static bool NotEqual_NameName(FName A, FName B);

	//
	// Transform functions
	//
	
	/** 
	 *	Transform a position by the supplied transform.
	 *	For example, if T was an object's transform, this would transform a position from local space to world space.
	 */
	UFUNCTION(BlueprintPure, Category="Math|Transform", meta=(Keywords="location"))
	static FVector TransformLocation(const FTransform& T, FVector Location);

	/** 
	 *	Transform a direction vector by the supplied transform - will not change its length. 
	 *	For example, if T was an object's transform, this would transform a direction from local space to world space.
	 */
	UFUNCTION(BlueprintPure, Category="Math|Transform")
	static FVector TransformDirection(const FTransform& T, FVector Direction);

	/** 
	 *	Transform a rotator by the supplied transform. 
	 *	For example, if T was an object's transform, this would transform a rotation from local space to world space.
	 */
	UFUNCTION(BlueprintPure, Category="Math|Transform")
	static FRotator TransformRotation(const FTransform& T, FRotator Rotation);

	/** 
	 *	Transform a position by the inverse of the supplied transform.
	 *	For example, if T was an object's transform, this would transform a position from world space to local space.
	 */
	UFUNCTION(BlueprintPure, Category="Math|Transform", meta=(Keywords="location"))
	static FVector InverseTransformLocation(const FTransform& T, FVector Location);

	/** 
	 *	Transform a direction vector by the inverse of the supplied transform - will not change its length.
	 *	For example, if T was an object's transform, this would transform a direction from world space to local space.
	 */
	UFUNCTION(BlueprintPure, Category="Math|Transform")
	static FVector InverseTransformDirection(const FTransform& T, FVector Direction);

	/** 
	 *	Transform a rotator by the inverse of the supplied transform. 
	 *	For example, if T was an object's transform, this would transform a rotation from world space to local space.
	 */
	UFUNCTION(BlueprintPure, Category="Math|Transform")
	static FRotator InverseTransformRotation(const FTransform& T, FRotator Rotation);

	/**
	 * Compose two transforms in order: A * B.
	 *
	 * Order matters when composing transforms:
	 * A * B will yield a transform that logically first applies A then B to any subsequent transformation.
	 *
	 * Example: LocalToWorld = ComposeTransforms(DeltaRotation, LocalToWorld) will change rotation in local space by DeltaRotation.
	 * Example: LocalToWorld = ComposeTransforms(LocalToWorld, DeltaRotation) will change rotation in world space by DeltaRotation.
	 *
	 * @return New transform: A * B
	 */
	UFUNCTION(BlueprintPure, meta=(Keywords="multiply *"), Category="Math|Transform")
	static FTransform ComposeTransforms(const FTransform& A, const FTransform& B);

	/** 
	 * Returns the given transform, converted to be relative to the given ParentTransform.
	 *
	 * Example: AToB = ConvertTransformToRelative(AToWorld, BToWorld) to compute A relative to B.
	 *
	 * @param		Transform		The transform you wish to convert
	 * @param		ParentTransform	The transform the conversion is relative to (in the same space as Transform)
	 * @return		The new relative transform
	 */
	UFUNCTION(BlueprintPure, Category="Math|Transform", meta=(Keywords="cast convert"))
	static FTransform ConvertTransformToRelative(const FTransform& Transform, const FTransform& ParentTransform);

	/** 
	 * Returns the inverse of the given transform T.
	 * 
	 * Example: Given a LocalToWorld transform, WorldToLocal will be returned.
	 *
	 * @param	T	The transform you wish to invert
	 * @return	The inverse of T.
	 */
	UFUNCTION(BlueprintPure, Category = "Math|Transform", meta = (Keywords = "inverse"))
	static FTransform InvertTransform(const FTransform& T);

	/** Linearly interpolates between A and B based on Alpha (100% of A when Alpha=0 and 100% of B when Alpha=1). */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Lerp (Transform)", AdvancedDisplay = "3"), Category="Math|Transform")
	static FTransform TLerp(const FTransform& A, const FTransform& B, float Alpha, TEnumAsByte<ELerpInterpolationMode::Type> InterpMode = ELerpInterpolationMode::QuatInterp);

	/** Ease between A and B using a specified easing function. */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Ease (Transform)", BlueprintInternalUseOnly = "true"), Category = "Math|Interpolation")
	static FTransform TEase(const FTransform& A, const FTransform& B, float Alpha, TEnumAsByte<EEasingFunc::Type> EasingFunc, float BlendExp = 2, int32 Steps = 2);

	/** Tries to reach a target transform. */
	UFUNCTION(BlueprintPure, Category="Math|Interpolation")
	static FTransform TInterpTo(const FTransform& Current, const FTransform& Target, float DeltaTime, float InterpSpeed);

	/** Returns true if transform A is equal to transform B */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Equal Transform", CompactNodeTitle = "==", Keywords = "== equal"), Category="Math|Transform")
	static bool EqualEqual_TransformTransform(const FTransform& A, const FTransform& B);

	/** 
	 *	Returns true if transform A is nearly equal to B 
	 *	@param LocationTolerance	How close position of transforms need to be to be considered equal
	 *	@param RotationTolerance	How close rotations of transforms need to be to be considered equal
	 *	@param Scale3DTolerance		How close scale of transforms need to be to be considered equal
	 */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Nearly Equal (transform)", Keywords = "== equal"), Category = "Math|Transform")
	static bool NearlyEqual_TransformTransform(const FTransform& A, const FTransform& B, float LocationTolerance = 1.e-4f, float RotationTolerance = 1.e-4f, float Scale3DTolerance = 1.e-4f);

	//
	// Vector2D functions
	//

	/** Returns addition of Vector A and Vector B (A + B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "vector2d + vector2d", CompactNodeTitle = "+", Keywords = "+ add plus", CommutativeAssociativeBinaryOperator = "true"), Category="Math|Vector2D")
	static FVector2D Add_Vector2DVector2D(FVector2D A, FVector2D B);

	/** Returns subtraction of Vector B from Vector A (A - B) */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "vector2d - vector2d", CompactNodeTitle = "-", Keywords = "- subtract minus"), Category="Math|Vector2D")
	static FVector2D Subtract_Vector2DVector2D(FVector2D A, FVector2D B);

	/** Returns Vector A scaled by B */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "vector2d * float", CompactNodeTitle = "*", Keywords = "* multiply"), Category="Math|Vector2D")
	static FVector2D Multiply_Vector2DFloat(FVector2D A, float B);

	/** Element-wise Vector multiplication (Result = {A.x*B.x, A.y*B.y}) */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "vector2d * vector2d", CompactNodeTitle = "*", Keywords = "* multiply", CommutativeAssociativeBinaryOperator = "true"), Category = "Math|Vector2D")
	static FVector2D Multiply_Vector2DVector2D(FVector2D A, FVector2D B);

	/** Returns Vector A divided by B */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "vector2d / float", CompactNodeTitle = "/", Keywords = "/ divide division"), Category="Math|Vector2D")
	static FVector2D Divide_Vector2DFloat(FVector2D A, float B = 1.f);

	/** Element-wise Vector divide (Result = {A.x/B.x, A.y/B.y}) */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "vector2d / vector2d", CompactNodeTitle = "/", Keywords = "/ divide division"), Category = "Math|Vector2D")
	static FVector2D Divide_Vector2DVector2D(FVector2D A, FVector2D B);

	/** Returns Vector A added by B */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "vector2d + float", CompactNodeTitle = "+", Keywords = "+ add plus"), Category="Math|Vector2D")
	static FVector2D Add_Vector2DFloat(FVector2D A, float B);

	/** Returns Vector A subtracted by B */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "vector2d - float", CompactNodeTitle = "-", Keywords = "- subtract minus"), Category="Math|Vector2D")
	static FVector2D Subtract_Vector2DFloat(FVector2D A, float B);

	/** Returns true if vector2D A is equal to vector2D B (A == B) within a specified error tolerance */
    UFUNCTION(BlueprintPure, meta=(DisplayName = "Equal (vector2D)", CompactNodeTitle = "==", Keywords = "== equal"), Category="Math|Vector2D")
    static bool EqualEqual_Vector2DVector2D(FVector2D A, FVector2D B, float ErrorTolerance = 1.e-4f);

    /** Returns true if vector2D A is not equal to vector2D B (A != B) within a specified error tolerance */
    UFUNCTION(BlueprintPure, meta=(DisplayName = "Not Equal (vector2D)", CompactNodeTitle = "!=", Keywords = "!= not equal"), Category="Math|Vector2D")
    static bool NotEqual_Vector2DVector2D(FVector2D A, FVector2D B, float ErrorTolerance = 1.e-4f);
	
	/**
	 * Tries to reach Target based on distance from Current position, giving a nice smooth feeling when tracking a position.
	 *
	 * @param		Current			Actual position
	 * @param		Target			Target position
	 * @param		DeltaTime		Time since last tick
	 * @param		InterpSpeed		Interpolation speed
	 * @return		New interpolated position
	 */
	UFUNCTION(BlueprintPure, Category="Math|Interpolation")
	static float FInterpTo(float Current, float Target, float DeltaTime, float InterpSpeed);

	/**
	 * Tries to reach Target at a constant rate.
	 *
	 * @param		Current			Actual position
	 * @param		Target			Target position
	 * @param		DeltaTime		Time since last tick
	 * @param		InterpSpeed		Interpolation speed
	 * @return		New interpolated position
	 */
	UFUNCTION(BlueprintPure, Category="Math|Interpolation")
	static float FInterpTo_Constant(float Current, float Target, float DeltaTime, float InterpSpeed);

	/**
	 * Tries to reach Target based on distance from Current position, giving a nice smooth feeling when tracking a position.
	 *
	 * @param		Current			Actual position
	 * @param		Target			Target position
	 * @param		DeltaTime		Time since last tick
	 * @param		InterpSpeed		Interpolation speed
	 * @return		New interpolated position
	 */
	UFUNCTION(BlueprintPure, Category="Math|Interpolation", meta=(Keywords="position"))
	static FVector VInterpTo(FVector Current, FVector Target, float DeltaTime, float InterpSpeed);

	/**
	 * Tries to reach Target at a constant rate.
	 *
	 * @param		Current			Actual position
	 * @param		Target			Target position
	 * @param		DeltaTime		Time since last tick
	 * @param		InterpSpeed		Interpolation speed
	 * @return		New interpolated position
	 */
	UFUNCTION(BlueprintPure, Category = "Math|Interpolation", meta = (Keywords = "position"))
	static FVector VInterpTo_Constant(FVector Current, FVector Target, float DeltaTime, float InterpSpeed);

	/**
	 * Tries to reach Target based on distance from Current position, giving a nice smooth feeling when tracking a position.
	 *
	 * @param		Current			Actual position
	 * @param		Target			Target position
	 * @param		DeltaTime		Time since last tick
	 * @param		InterpSpeed		Interpolation speed
	 * @return		New interpolated position
	 */
	UFUNCTION(BlueprintPure, Category="Math|Interpolation", meta=(Keywords="position"))
	static FVector2D Vector2DInterpTo(FVector2D Current, FVector2D Target, float DeltaTime, float InterpSpeed);
	
	/**
	 * Tries to reach Target at a constant rate.
	 *
	 * @param		Current			Actual position
	 * @param		Target			Target position
	 * @param		DeltaTime		Time since last tick
	 * @param		InterpSpeed		Interpolation speed
	 * @return		New interpolated position
	 */
	UFUNCTION(BlueprintPure, Category="Math|Interpolation", meta=(Keywords="position"))
	static FVector2D Vector2DInterpTo_Constant(FVector2D Current, FVector2D Target, float DeltaTime, float InterpSpeed);
	
	/**
	 * Tries to reach Target rotation based on Current rotation, giving a nice smooth feeling when rotating to Target rotation.
	 *
	 * @param		Current			Actual rotation
	 * @param		Target			Target rotation
	 * @param		DeltaTime		Time since last tick
	 * @param		InterpSpeed		Interpolation speed
	 * @return		New interpolated position
	 */
	UFUNCTION(BlueprintPure, Category="Math|Interpolation", meta=(Keywords="rotation rotate"))
	static FRotator RInterpTo(FRotator Current, FRotator Target, float DeltaTime, float InterpSpeed);

	/**
	 * Tries to reach Target rotation at a constant rate.
	 *
	 * @param		Current			Actual rotation
	 * @param		Target			Target rotation
	 * @param		DeltaTime		Time since last tick
	 * @param		InterpSpeed		Interpolation speed
	 * @return		New interpolated position
	 */
	UFUNCTION(BlueprintPure, Category="Math|Interpolation", meta=(Keywords="rotation rotate"))
	static FRotator RInterpTo_Constant(FRotator Current, FRotator Target, float DeltaTime, float InterpSpeed);

	/**
	 * Interpolates towards a varying target color smoothly.
	 *
	 * @param		Current			Current Color
	 * @param		Target			Target Color
	 * @param		DeltaTime		Time since last tick
	 * @param		InterpSpeed		Interpolation speed
	 * @return		New interpolated Color
	 */
	UFUNCTION(BlueprintPure, Category = "Math|Interpolation", meta = (Keywords = "color"))
	static FLinearColor CInterpTo(FLinearColor Current, FLinearColor Target, float DeltaTime, float InterpSpeed);

	/** 
	 * Uses a simple spring model to interpolate a float from Current to Target.
	 *
	 * @param Current				Current value
	 * @param Target				Target value
	 * @param SpringState			Data related to spring model (velocity, error, etc..) - Create a unique variable per spring
	 * @param Stiffness				How stiff the spring model is (more stiffness means more oscillation around the target value)
	 * @param CriticalDampingFactor	How much damping to apply to the spring (0 means no damping, 1 means critically damped which means no oscillation)
	 * @param Mass					Multiplier that acts like mass on a spring
	 */
	UFUNCTION(BlueprintCallable, Category = Spring)
	static float FloatSpringInterp(float Current, float Target, UPARAM(ref) FFloatSpringState& SpringState, float Stiffness, float CriticalDampingFactor, float DeltaTime, float Mass = 1.f);

	/**
	* Uses a simple spring model to interpolate a vector from Current to Target.
	*
	* @param Current				Current value
	* @param Target					Target value
	* @param SpringState			Data related to spring model (velocity, error, etc..) - Create a unique variable per spring
	* @param Stiffness				How stiff the spring model is (more stiffness means more oscillation around the target value)
	* @param CriticalDampingFactor	How much damping to apply to the spring (0 means no damping, 1 means critically damped which means no oscillation)
	* @param Mass					Multiplier that acts like mass on a spring
	*/
	UFUNCTION(BlueprintCallable, Category = Spring)
	static FVector VectorSpringInterp(FVector Current, FVector Target, UPARAM(ref) FVectorSpringState& SpringState, float Stiffness, float CriticalDampingFactor, float DeltaTime, float Mass = 1.f);

	/** Resets the state of a given spring */
	UFUNCTION(BlueprintCallable, Category = Spring)
	static void ResetFloatSpringState(UPARAM(ref) FFloatSpringState& SpringState);

	/** Resets the state of a given spring */
	UFUNCTION(BlueprintCallable, Category = Spring)
	static void ResetVectorSpringState(UPARAM(ref) FVectorSpringState& SpringState);

	//
	// Random stream functions
	//

	/** Returns a uniformly distributed random number between 0 and Max - 1 */
	UFUNCTION(BlueprintPure, Category="Math|Random")
	static int32 RandomIntegerFromStream(int32 Max, const FRandomStream& Stream);

	/** Return a random integer between Min and Max (>= Min and <= Max) */
	UFUNCTION(BlueprintPure, Category="Math|Random")
	static int32 RandomIntegerInRangeFromStream(int32 Min, int32 Max, const FRandomStream& Stream);

	/** Returns a random bool */
	UFUNCTION(BlueprintPure, Category="Math|Random")
	static bool RandomBoolFromStream(const FRandomStream& Stream);

	/** Returns a random float between 0 and 1 */
	UFUNCTION(BlueprintPure, Category="Math|Random")
	static float RandomFloatFromStream(const FRandomStream& Stream);

	/** Generate a random number between Min and Max */
	UFUNCTION(BlueprintPure, Category="Math|Random")
	static float RandomFloatInRangeFromStream(float Min, float Max, const FRandomStream& Stream);

	/** Returns a random vector with length of 1.0 */
	UFUNCTION(BlueprintPure, Category="Math|Random")
	static FVector RandomUnitVectorFromStream(const FRandomStream& Stream);

	/** Create a random rotation */
	UFUNCTION(BlueprintPure, Category="Math|Random")
	static FRotator RandomRotatorFromStream(bool bRoll, const FRandomStream& Stream);

	/** Reset a random stream */
	UFUNCTION(BlueprintCallable, Category="Math|Random")
	static void ResetRandomStream(const FRandomStream& Stream);

	/** Create a new random seed for a random stream */
	UFUNCTION(BlueprintCallable, Category="Math|Random")
	static void SeedRandomStream(UPARAM(ref) FRandomStream& Stream);

	/** Set the seed of a random stream to a specific number */
	UFUNCTION(BlueprintCallable, Category="Math|Random")
	static void SetRandomStreamSeed(UPARAM(ref) FRandomStream& Stream, int32 NewSeed);

	/** 
	 * Returns a random vector with length of 1, within the specified cone, with uniform random distribution.
	 * @param ConeDir					The base "center" direction of the cone.
	 * @param ConeHalfAngleInRadians	The half-angle of the cone (from ConeDir to edge), in radians.
	 * @param Stream					The random stream from which to obtain the vector.
	 */
	UFUNCTION(BlueprintPure, Category="Math|Random", meta = (Keywords = "RandomVector"))
	static FVector RandomUnitVectorInConeInRadiansFromStream(const FVector& ConeDir, float ConeHalfAngleInRadians, const FRandomStream& Stream);

	/** 
	 * Returns a random vector with length of 1, within the specified cone, with uniform random distribution.
	 * @param ConeDir					The base "center" direction of the cone.
	 * @param ConeHalfAngleInDegrees	The half-angle of the cone (from ConeDir to edge), in degrees.
	 * @param Stream					The random stream from which to obtain the vector.
	 */
	UFUNCTION(BlueprintPure, Category="Math|Random", meta = (Keywords = "RandomVector"))
	static inline FVector RandomUnitVectorInConeInDegreesFromStream(const FVector& ConeDir, float ConeHalfAngleInDegrees, const FRandomStream& Stream)
	{
		return RandomUnitVectorInConeInRadiansFromStream(ConeDir, FMath::DegreesToRadians(ConeHalfAngleInDegrees), Stream);
	}

	/**
	* Returns a random vector with length of 1, within the specified cone, with uniform random distribution.
	* The shape of the cone can be modified according to the yaw and pitch angles.
	*
	* @param MaxYawInRadians	The yaw angle of the cone (from ConeDir to horizontal edge), in radians.
	* @param MaxPitchInRadians	The pitch angle of the cone (from ConeDir to vertical edge), in radians.
	* @param Stream				The random stream from which to obtain the vector.
	*/
	UFUNCTION(BlueprintPure, Category = "Math|Random", meta = (Keywords = "RandomVector"))
	static FVector RandomUnitVectorInEllipticalConeInRadiansFromStream(const FVector& ConeDir, float MaxYawInRadians, float MaxPitchInRadians, const FRandomStream& Stream);

	/**
	* Returns a random vector with length of 1, within the specified cone, with uniform random distribution.
	* The shape of the cone can be modified according to the yaw and pitch angles.
	*
	* @param MaxYawInDegrees	The yaw angle of the cone (from ConeDir to horizontal edge), in degrees.
	* @param MaxPitchInDegrees	The pitch angle of the cone (from ConeDir to vertical edge), in degrees.
	* @param Stream				The random stream from which to obtain the vector.
	*/
	UFUNCTION(BlueprintPure, Category = "Math|Random", meta = (Keywords = "RandomVector"))
	static inline FVector RandomUnitVectorInEllipticalConeInDegreesFromStream(const FVector& ConeDir, float MaxYawInDegrees, float MaxPitchInDegrees, const FRandomStream& Stream)
	{
		return RandomUnitVectorInEllipticalConeInRadiansFromStream(ConeDir, FMath::DegreesToRadians(MaxYawInDegrees), FMath::DegreesToRadians(MaxPitchInDegrees), Stream);
	}

	//
	// Geometry
	//

	/**  
	 * Finds the minimum area rectangle that encloses all of the points in InVerts
	 * Uses algorithm found in http://www.geometrictools.com/Documentation/MinimumAreaRectangle.pdf
	 *	
	 * @param		InVerts	- Points to enclose in the rectangle
	 * @outparam	OutRectCenter - Center of the enclosing rectangle
	 * @outparam	OutRectSideA - Vector oriented and sized to represent one edge of the enclosing rectangle, orthogonal to OutRectSideB
	 * @outparam	OutRectSideB - Vector oriented and sized to represent one edge of the enclosing rectangle, orthogonal to OutRectSideA
	*/
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Math|Geometry", meta=(WorldContext="WorldContextObject", CallableWithoutWorldContext))
	static void MinimumAreaRectangle(UObject* WorldContextObject, const TArray<FVector>& InVerts, const FVector& SampleSurfaceNormal, FVector& OutRectCenter, FRotator& OutRectRotation, float& OutSideLengthX, float& OutSideLengthY, bool bDebugDraw = false);

	/**
	 * Determines whether a given set of points are coplanar, with a tolerance. Any three points or less are always coplanar.
	 *
	 * @param Points - The set of points to determine coplanarity for.
	 * @param Tolerance - Larger numbers means more variance is allowed.
	 *
	 * @return Whether the points are relatively coplanar, based on the tolerance
	 */
	UFUNCTION(BlueprintPure, Category = "Math|Geometry")
	static bool PointsAreCoplanar(const TArray<FVector>& Points, float Tolerance = 0.1f);

	/**
	 * Determines whether the given point is in a box. Includes points on the box.
	 *
	 * @param Point			Point to test
	 * @param BoxOrigin		Origin of the box
	 * @param BoxExtent		Extents of the box (distance in each axis from origin)
	 * @return Whether the point is in the box.
	 */
	UFUNCTION(BlueprintPure, Category = "Math|Geometry")
	static bool IsPointInBox(FVector Point, FVector BoxOrigin, FVector BoxExtent);

	/**
	* Determines whether a given point is in a box with a given transform. Includes points on the box.
	*
	* @param Point				Point to test
	* @param BoxWorldTransform	Component-to-World transform of the box.
	* @param BoxExtent			Extents of the box (distance in each axis from origin), in component space.
	* @return Whether the point is in the box.
	*/
	UFUNCTION(BlueprintPure, Category = "Math|Geometry")
	static bool IsPointInBoxWithTransform(FVector Point, const FTransform& BoxWorldTransform, FVector BoxExtent);

	//
	// Intersection
	//

	/**
	 * Computes the intersection point between a line and a plane.
	 * @param		T - The t of the intersection between the line and the plane
	 * @param		Intersection - The point of intersection between the line and the plane
	 * @return		True if the intersection test was successful.
	 */
	UFUNCTION(BlueprintPure, Category = "Math|Intersection")
	static bool LinePlaneIntersection(const FVector& LineStart, const FVector& LineEnd, const FPlane& APlane, float& T, FVector& Intersection);

	/**
	 * Computes the intersection point between a line and a plane.
	 * @param		T - The t of the intersection between the line and the plane
	 * @param		Intersection - The point of intersection between the line and the plane
	 * @return		True if the intersection test was successful.
	 */
	UFUNCTION(BlueprintPure, Category = "Math|Intersection", meta = (DisplayName = "Line Plane Intersection (Origin & Normal)"))
	static bool LinePlaneIntersection_OriginNormal(const FVector& LineStart, const FVector& LineEnd, FVector PlaneOrigin, FVector PlaneNormal, float& T, FVector& Intersection);


private:

	static void ReportError_Divide_ByteByte();
	static void ReportError_Percent_ByteByte();
	static void ReportError_Divide_IntInt();
	static void ReportError_Percent_IntInt();
	static void ReportError_Sqrt();
	static void ReportError_Divide_VectorFloat();
	static void ReportError_Divide_VectorInt();
	static void ReportError_Divide_VectorVector();
	static void ReportError_ProjectVectorOnToVector();
	static void ReportError_Divide_Vector2DFloat();
	static void ReportError_Divide_Vector2DVector2D();
	static void ReportError_DaysInMonth();
};


// Conditionally inlined
#if KISMET_MATH_INLINE_ENABLED
#include "KismetMathLibrary.inl"
#endif

