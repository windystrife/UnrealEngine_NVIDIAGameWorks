// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#define MATINEE_MODULE_CONSTANTS	1

/** Constants Used by Matinee*/
namespace MatineeConstants
{
	/**The state of recording new Matinee Tracks*/
	namespace ERecordingState
	{
		enum Type
		{
			RECORDING_GET_READY_PAUSE,
			RECORDING_ACTIVE,
			RECORDING_COMPLETE,

			NUM_RECORDING_STATES
		};
	};

	/**How recording should behave*/
	namespace ERecordMode
	{
		enum Type
		{
			//A new camera actor, camera group, and movement/fovangle tracks will be added for each take
			RECORD_MODE_NEW_CAMERA,
			//A new camera actor, camera group, and movement/fovangle tracks will be added AND rooted to the currently selected object.
			RECORD_MODE_NEW_CAMERA_ATTACHED,
			//Duplicate tracks of each selected track will be made for each take
			RECORD_MODE_DUPLICATE_TRACKS,
			//Empties and re-records over the currently selected tracks
			RECORD_MODE_REPLACE_TRACKS,

			NUM_RECORD_MODES
		};
	};

	/**How the camera should move */
	namespace ECameraScheme
	{
		enum Type
		{
			//camera movement should align with the camera axes
			CAMERA_SCHEME_FREE_CAM,
			//camera movement should planar
			CAMERA_SCHEME_PLANAR_CAM,

			NUM_CAMERA_SCHEMES
		};
	};

	namespace ERecordMenu
	{
		/**Menu Settings for HUD during record mode*/
		enum Type
		{
			//Default menu item to Start Recording
			RECORD_MENU_RECORD_MODE,
			//Allows camera translation speed adjustments
			RECORD_MENU_TRANSLATION_SPEED,
			//Allows camera rotation speed adjustments
			RECORD_MENU_ROTATION_SPEED,
			//Allows camera zoom speed adjustments
			RECORD_MENU_ZOOM_SPEED,
			//Allows for custom offset for pitch
			RECORD_MENU_TRIM,
			//Invert the x axis
			RECORD_MENU_INVERT_X_AXIS,
			//Invert the x axis
			RECORD_MENU_INVERT_Y_AXIS,
			//Setting for what to do about roll wiggle
			RECORD_MENU_ROLL_SMOOTHING,
			//Setting for what to do about pitch wiggle
			RECORD_MENU_PITCH_SMOOTHING,
			//camera movement scheme
			RECORD_MENU_CAMERA_MOVEMENT_SCHEME,
			//Absolute setting for zoom distance
			RECORD_MENU_ZOOM_DISTANCE,

			NUM_RECORD_MENU_ITEMS
		};
	}

	/**Delay between recording setup and the start of recording*/
	const uint32 CountdownDurationInSeconds = 5;

	const uint32 MaxSmoothingSamples = 10;
}
