// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/ICursor.h"
#include "Templates/SharedPointer.h"
#include "GenericPlatform/GenericApplicationMessageHandler.h"
#include "Containers/BitArray.h"
THIRD_PARTY_INCLUDES_START
	#include <SDL.h>
	#include <emscripten/html5.h>
THIRD_PARTY_INCLUDES_END


/**
 * Interface class for HTML5 input devices
 */
class FHTML5InputInterface
{
public:
	/** Initialize the interface */
	static TSharedRef< FHTML5InputInterface > Create(  const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler, const TSharedPtr< ICursor >& InCursor );

public:

	~FHTML5InputInterface() {}

	void SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler);

	/** Tick the interface (i.e check for new controllers) */
	void Tick( float DeltaTime, const SDL_Event& Event,TSharedRef < FGenericWindow>& ApplicationWindow );
	/**
	 * Poll for controller state and send events if needed
	 */
	void SendControllerEvents();


private:

	FHTML5InputInterface( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler, const TSharedPtr< ICursor >& InCursor );


private:

	TSharedRef< FGenericApplicationMessageHandler > MessageHandler;
	const TSharedPtr< ICursor > Cursor;

	TBitArray<FDefaultBitArrayAllocator> KeyStates;

	EmscriptenGamepadEvent PrevGamePadState[5];
	double LastPressedTime[5][15];

};
