// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"

// Only enable unreal audio on windows or mac
#if PLATFORM_WINDOWS
#define ENABLE_UNREAL_AUDIO 1
#else
#define ENABLE_UNREAL_AUDIO 0
#endif

// Turns on extra checks and asserts for debugging threading code
#define ENABLE_UNREAL_AUDIO_THREAD_DEBUGGING		1
#define ENABLE_UNREAL_AUDIO_EXTRA_DEBUG_CHECKS		1

namespace UAudio
{
	namespace ESystemError
	{
		enum Type
		{
			NONE,
			INVALID_HANDLE,
			COMMAND_QUEUE_FULL,
			ALREADY_INITIALIZED,
			NOT_INITIALIZED,
			INVALID_ARGUMENTS,
		};

		static inline const TCHAR* ToString(ESystemError::Type SystemError)
		{
			switch (SystemError)
			{
				case NONE:						return TEXT("NONE");
				case INVALID_HANDLE:			return TEXT("INVALID_HANDLE");
				case COMMAND_QUEUE_FULL:		return TEXT("COMMAND_QUEUE_FULL");
				case ALREADY_INITIALIZED:		return TEXT("ALREADY_INITIALIZED");
				case NOT_INITIALIZED:			return TEXT("NOT_INITIALIZED");
				case INVALID_ARGUMENTS:			return TEXT("INVALID_ARGUMENTS");
				default:						check(false);
			}
			return TEXT("");
		}
	}

	namespace EDeviceError
	{
		enum Type
		{
			WARNING,
			UNKNOWN,
			INVALID_PARAMETER,
			PLATFORM,
			SYSTEM,
			THREAD,
		};

		static inline const TCHAR* ToString(EDeviceError::Type DeviceError)
		{
			switch (DeviceError)
			{
				case WARNING:			return TEXT("WARNING");
				case UNKNOWN:			return TEXT("UNKNOWN");
				case INVALID_PARAMETER:	return TEXT("INVALID_PARAMETER");
				case PLATFORM:			return TEXT("PLATFORM");
				case SYSTEM:			return TEXT("SYSTEM");
				case THREAD:			return TEXT("THREAD");
				default:				check(false);
			}
			return TEXT("");
		}
	}

	namespace EDeviceApi
	{
		enum Type
		{
			WASAPI,
			XAUDIO2,
			NGS2,
			ALSA,
			CORE_AUDIO,
			OPENAL,
			HTML5,
			DUMMY,
		};

		static inline const TCHAR* ToString(EDeviceApi::Type DeviceApi)
		{
			switch (DeviceApi)
			{
				case WASAPI:		return TEXT("WASAPI");
				case XAUDIO2:		return TEXT("XAUDIO2");
				case NGS2:			return TEXT("NGS2");
				case ALSA:			return TEXT("ALSA");
				case CORE_AUDIO:	return TEXT("CORE_AUDIO");
				case OPENAL:		return TEXT("OPENAL");
				case HTML5:			return TEXT("HTML5");
				case DUMMY:			return TEXT("DUMMY");
				default:			check(false);
			}
			return TEXT("");
		}
	};

	namespace ESpeaker
	{
		/** Values that represent speaker types. */
		enum Type
		{
			FRONT_LEFT,
			FRONT_RIGHT,
			FRONT_CENTER,
			LOW_FREQUENCY,
			BACK_LEFT,
			BACK_RIGHT,
			FRONT_LEFT_OF_CENTER,
			FRONT_RIGHT_OF_CENTER,
			BACK_CENTER,
			SIDE_LEFT,
			SIDE_RIGHT,
			TOP_CENTER,
			TOP_FRONT_LEFT,
			TOP_FRONT_CENTER,
			TOP_FRONT_RIGHT,
			TOP_BACK_LEFT,
			TOP_BACK_CENTER,
			TOP_BACK_RIGHT,
			UNUSED,
			SPEAKER_TYPE_COUNT
		};

		static inline const TCHAR* ToString(ESpeaker::Type Speaker)
		{
			switch (Speaker)
			{
				case FRONT_LEFT:				return TEXT("FRONT_LEFT");
				case FRONT_RIGHT:				return TEXT("FRONT_RIGHT");
				case FRONT_CENTER:				return TEXT("FRONT_CENTER");
				case LOW_FREQUENCY:				return TEXT("LOW_FREQUENCY");
				case BACK_LEFT:					return TEXT("BACK_LEFT");
				case BACK_RIGHT:				return TEXT("BACK_RIGHT");
				case FRONT_LEFT_OF_CENTER:		return TEXT("FRONT_LEFT_OF_CENTER");
				case FRONT_RIGHT_OF_CENTER:		return TEXT("FRONT_RIGHT_OF_CENTER");
				case BACK_CENTER:				return TEXT("BACK_CENTER");
				case SIDE_LEFT:					return TEXT("SIDE_LEFT");
				case SIDE_RIGHT:				return TEXT("SIDE_RIGHT");
				case TOP_CENTER:				return TEXT("TOP_CENTER");
				case TOP_FRONT_LEFT:			return TEXT("TOP_FRONT_LEFT");
				case TOP_FRONT_CENTER:			return TEXT("TOP_FRONT_CENTER");
				case TOP_FRONT_RIGHT:			return TEXT("TOP_FRONT_RIGHT");
				case TOP_BACK_LEFT:				return TEXT("TOP_BACK_LEFT");
				case TOP_BACK_CENTER:			return TEXT("TOP_BACK_CENTER");
				case TOP_BACK_RIGHT:			return TEXT("TOP_BACK_RIGHT");
				case UNUSED:					return TEXT("UNUSED");
				default:						return TEXT("UKNOWN");
			}
			return TEXT("");
		}
	};

}


