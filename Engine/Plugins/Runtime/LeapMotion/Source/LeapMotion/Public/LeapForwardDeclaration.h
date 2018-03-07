// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InputCoreTypes.h"

//Forward declare namespace so public is not exposed to Leap header
namespace Leap
{
	class Arm;
	class Bone;
	class CircleGesture;
	class Controller;
	class Finger;
	class FingerList;
	class Gesture;
	class GestureList;
	class Hand;
	class HandList;
	class InteractionBox;
	class KeyTapGesture;
	class Frame;
	class Image;
	class ImageList;
	class Pointable;
	class PointableList;
	class ScreenTapGesture;
	class SwipeGesture;
	class Tool;
	class ToolList;
}

//Input mapping
struct FKeysLeap
{
	//Left Hand Actions
	static const FKey LeapLeftPinch;
	static const FKey LeapLeftGrab;

	//Left Hand Rotations
	static const FKey LeapLeftPalmPitch;
	static const FKey LeapLeftPalmYaw;
	static const FKey LeapLeftPalmRoll;

	//Right Hand Actions
	static const FKey LeapRightPinch;
	static const FKey LeapRightGrab;

	//Right Hand Rotations
	static const FKey LeapRightPalmPitch;
	static const FKey LeapRightPalmYaw;
	static const FKey LeapRightPalmRoll;
};