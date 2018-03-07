// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

namespace Audio {

	namespace EAudioMixerPlatformApi
	{
		enum Type
		{
			XAudio2, 	// Windows, XBoxOne
			AudioOut, 	// PS4
			CoreAudio, 	// Mac
			AudioUnit, 	// iOS
			SDL2,		// Used on Linux, Html5
			OpenSLES, 	// Android
			Switch, 	// Switch
			Null		// Unknown/not Supported
		};
	}

	namespace EAudioMixerStreamDataFormat
	{
		enum Type
		{
			Unknown,
			Float,
			Int16,
			Unsupported
		};
	}

	/**
	 * EAudioOutputStreamState
	 * Specifies the state of the output audio stream.
	 */
	namespace EAudioOutputStreamState
	{
		enum Type
		{
			/* The audio stream is shutdown or not uninitialized. */
			Closed,
		
			/* The audio stream is open but not running. */
			Open,

			/** The audio stream is open but stopped. */
			Stopped,
		
			/** The audio output stream is stopping. */
			Stopping,

			/** The audio output stream is open and running. */
			Running,
		};
	}

}
