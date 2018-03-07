// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "LeapEnums.generated.h"

UENUM(BlueprintType)
enum LeapFingerType
{
    FINGER_TYPE_THUMB,
    FINGER_TYPE_INDEX,
    FINGER_TYPE_MIDDLE,
    FINGER_TYPE_RING,
    FINGER_TYPE_PINKY
};

UENUM(BlueprintType)
enum LeapBoneType
{
    TYPE_METACARPAL,
    TYPE_PROXIMAL,
    TYPE_INTERMEDIATE,
    TYPE_DISTAL,
    TYPE_ERROR
};


UENUM(BlueprintType)
enum LeapGestureType
{
    GESTURE_TYPE_INVALID,
    GESTURE_TYPE_SWIPE,
    GESTURE_TYPE_CIRCLE,
    GESTURE_TYPE_SCREEN_TAP,
    GESTURE_TYPE_KEY_TAP
};

UENUM(BlueprintType)
enum LeapGestureState
{
    GESTURE_STATE_INVALID,
    GESTURE_STATE_START,
    GESTURE_STATE_UPDATE,
    GESTURE_STATE_STOP,
};

UENUM(BlueprintType)
enum LeapBasicDirection
{
    DIRECTION_NONE,
    DIRECTION_LEFT,
    DIRECTION_RIGHT,
    DIRECTION_UP,
    DIRECTION_DOWN,
    DIRECTION_AWAY,
    DIRECTION_TOWARD,
};

UENUM(BlueprintType)
enum LeapHandType
{
    HAND_UNKNOWN,
    HAND_LEFT,
    HAND_RIGHT
};

UENUM(BlueprintType)
enum LeapZone
{
    ZONE_ERROR,
    ZONE_NONE,
    ZONE_HOVERING,
    ZONE_TOUCHING
};

