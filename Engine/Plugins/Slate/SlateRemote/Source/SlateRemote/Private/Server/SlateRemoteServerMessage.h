// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// magic number that must match UDKRemote
#define SLATE_REMOTE_SERVER_MESSAGE_MAGIC_ID 0xAB


/**
 * Enumerates available Slate Remote message data types.
 */
enum EDataType
{
	DT_TouchBegan = 0,
	DT_TouchMoved = 1,
	DT_TouchEnded = 2,
	DT_Tilt = 3,
	DT_Motion = 4,
	DT_Gyro = 5,
	DT_Ping = 6,
};


/**
 * Enumerates supported Slate Remote device orientations.
 */
enum EDeviceOrientation
{
	DO_Unknown,
	DO_Portrait,
	DO_PortraitUpsideDown,
	DO_LandscapeLeft,
	DO_LandscapeRight,
	DO_FaceUp,
	DO_FaceDown,
};


/**
 * Structure for Slate Remote event messages.
 */
struct FSlateRemoteServerMessage
{
	/** A byte that must match to what we expect */
	uint8 MagicTag;

	/** What version of message this is from UDK Remote */
	uint8 MessageVersion;

	/** Unique Id for the message, used for detecting lost/duplicate packets, etc (only duplicate currently handled) */
	uint16 MessageID;

	/** What type of message is this? */
	uint8 DataType;

	/** Unique identifier for the touch/finger */
	uint8 Handle;

	/** The current orientation of the device */
	uint8 DeviceOrientation;

	/** The current orientation of the UI */
	uint8 UIOrientation;

	/** X/Y or Pitch/Yaw data or CoreMotion data */
	float Data[12];
};
