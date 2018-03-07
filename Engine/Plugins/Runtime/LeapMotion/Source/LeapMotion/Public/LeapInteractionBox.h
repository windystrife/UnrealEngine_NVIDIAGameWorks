// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h"
#include "Leap_NoPI.h"
#include "LeapInteractionBox.generated.h"

/**
* The InteractionBox class represents a box-shaped region completely within the 
* field of view of the Leap Motion controller. The interaction box is an axis-aligned 
* rectangular prism and provides normalized coordinates for hands, fingers, and tools 
* within this box.
*
* The InteractionBox class can make it easier to map positions in the Leap Motion coordinate 
* system to 2D or 3D coordinate systems used for application drawing.
*
* Leap API reference: https://developer.leapmotion.com/documentation/cpp/api/Leap.InteractionBox.html
*/
UCLASS(BlueprintType)
class LEAPMOTION_API ULeapInteractionBox : public UObject
{
	GENERATED_UCLASS_BODY()
public:
	~ULeapInteractionBox();
	
	/**
	* The center of the InteractionBox in device coordinates (centimeters).
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Interaction Box")
	FVector Center;

	/**
	* Converts a position defined by normalized InteractionBox coordinates into
	* device coordinates in centimeters.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "DenormalizePoint", CompactNodeTitle = "", Keywords = "normalize point"), Category = "Leap Interaction Box")
	FVector DenormalizePoint(FVector Position) const;

	/**
	* The depth of the InteractionBox in centimeters, measured along the x-axis.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Interaction Box")
	float Depth;

	/**
	* The height of the InteractionBox in centimeters, measured along the z-axis.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Interaction Box")
	float Height;

	/**
	* Reports whether this is a valid InteractionBox object.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Interaction Box")
	bool IsValid;

	/**
	* Normalizes the coordinates of a point using the interaction box.
	*
	* @param	Position	The input position in device coordinates.
	* @param	Clamp		Whether or not to limit the output value to the range [0,1] when the input position is outside the InteractionBox. Defaults to true.
	* @return	The normalized position.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "normalizePoint", CompactNodeTitle = "", Keywords = "normalize point"), Category = "Leap Interaction Box")
	FVector NormalizePoint(FVector Position, bool Clamp=true) const;

	/**
	* The width of the InteractionBox in centimeters, measured along the y-axis.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Interaction Box")
	float Width;

	void SetInteractionBox(const class Leap::InteractionBox &InteractionBox);

private:
	class FPrivateInteractionBox* Private;
};