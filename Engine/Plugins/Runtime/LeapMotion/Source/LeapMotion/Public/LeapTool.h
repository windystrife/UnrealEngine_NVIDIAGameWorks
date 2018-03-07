// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LeapPointable.h"
#include "Leap_NoPI.h"
#include "LeapTool.generated.h"

/**
* The Tool class represents a tracked tool.
* Tools are Pointable objects that the Leap Motion software has classified as a tool.
* Get valid Tool objects from a Frame object.
*
* Leap API reference: https://developer.leapmotion.com/documentation/cpp/api/Leap.Tool.html
*/
UCLASS(BlueprintType)
class LEAPMOTION_API ULeapTool : public ULeapPointable
{
	GENERATED_UCLASS_BODY()
public:
	~ULeapTool();

	void SetTool(const class Leap::Tool &Tool);

private:
	class FPrivateTool* Private;
};