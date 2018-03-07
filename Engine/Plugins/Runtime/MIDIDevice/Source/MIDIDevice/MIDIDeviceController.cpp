// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MIDIDeviceController.h"
#include "MIDIDeviceLog.h"
#include "portmidi.h"


#define LOCTEXT_NAMESPACE "MIDIDeviceController"


UMIDIDeviceController::~UMIDIDeviceController()
{
	// Clean everything up before we're garbage collected
	ShutdownDevice();
}


// todo midi: Add SetFilter and SetChannelMask support (better performance)

void UMIDIDeviceController::StartupDevice( const int32 InitDeviceID, const int32 InitMIDIBufferSize, bool& bOutWasSuccessful )
{
	bOutWasSuccessful = false;

	this->DeviceID = InitDeviceID;
	this->PMMIDIInputStream = nullptr;
	this->MIDIBufferSize = 0;

	const PmDeviceID PMDeviceID = DeviceID;

	if( InitMIDIBufferSize > 0 )
	{
		const PmDeviceInfo* PMDeviceInfo = Pm_GetDeviceInfo( PMDeviceID );
		if( PMDeviceInfo != nullptr )
		{
			// Is the device already in use?  If so, spit out a warning
			if( PMDeviceInfo->opened != 0 )
			{
				UE_LOG( LogMIDIDevice, Warning, TEXT( "Warning while creating a MIDI device controller:  PortMidi reports that device ID %i (%s) is already in use." ), PMDeviceID, ANSI_TO_TCHAR( PMDeviceInfo->name ) );
			}

			// Make sure the device is setup for input/output
			if( PMDeviceInfo->input == 0 )
			{
				UE_LOG( LogMIDIDevice, Warning, TEXT( "Warning while creating a MIDI device controller:  PortMidi reports that device ID %i (%S) does is not setup to transmit MIDI data." ), PMDeviceID, ANSI_TO_TCHAR( PMDeviceInfo->name ) );
			}


			// @todo midi: Add MIDI output support

			// @todo midi: Add options for timing/latency (see timeproc, and pm_Synchronize)

			PmError PMError = Pm_OpenInput( &this->PMMIDIInputStream, PMDeviceID, NULL, MIDIBufferSize, NULL, NULL );
			if( PMError == pmNoError )
			{
				check( this->PMMIDIInputStream != nullptr );

				this->DeviceName = ANSI_TO_TCHAR( PMDeviceInfo->name );
				this->MIDIBufferSize = InitMIDIBufferSize;

				// Good to go!
				bOutWasSuccessful = true;
			}
			else
			{
				this->PMMIDIInputStream = nullptr;
				UE_LOG( LogMIDIDevice, Error, TEXT( "Unable to open input connection to MIDI device ID %i (%s) (PortMidi error: %s)." ), PMDeviceID, ANSI_TO_TCHAR( PMDeviceInfo->name ), ANSI_TO_TCHAR( Pm_GetErrorText( PMError ) ) );
			}
		}
		else
		{
			UE_LOG( LogMIDIDevice, Error, TEXT( "Unable to query information about MIDI device (PortMidi device ID: %i)." ), PMDeviceID );
		}
	}
	else
	{
		UE_LOG( LogMIDIDevice, Error, TEXT( "The specified MIDI Buffer Size must be greater than zero." ) );
	}
}


void UMIDIDeviceController::ShutdownDevice()
{
	const PmDeviceID PMDeviceID = this->DeviceID;

	if( this->PMMIDIInputStream != nullptr )
	{
		Pm_Close( this->PMMIDIInputStream );
		this->PMMIDIInputStream = nullptr;
	}
}


void UMIDIDeviceController::ProcessIncomingMIDIEvents()
{
	if( this->PMMIDIInputStream != nullptr )
	{
		const PmDeviceID PMDeviceID = this->DeviceID;

		// Static that we'll copy event data to every time.  This stuff isn't multi-threaded right now, so this is fine.
		static TArray<PmEvent> PMMIDIEvents;
		PMMIDIEvents.SetNum( MIDIBufferSize, false );

		const int32 PMEventCount = Pm_Read( this->PMMIDIInputStream, PMMIDIEvents.GetData(), PMMIDIEvents.Num() );
		for( int32 PMEventIndex = 0; PMEventIndex < PMEventCount; ++PMEventIndex )
		{
			const PmEvent& PMEvent = PMMIDIEvents[ PMEventIndex ];
			const int32 PMTimestamp = PMEvent.timestamp;

			const PmMessage& PMMessage = PMEvent.message;

			const int PMMessageStatus = Pm_MessageStatus( PMMessage );
			const int PMMessageData1 = Pm_MessageData1( PMMessage );
			const int PMMessageData2 = Pm_MessageData2( PMMessage );
			const int32 PMType = ( PMMessageStatus & 0xF0 ) >> 4;
			const int32 PMChannel = ( PMMessageStatus % 16 ) + 1;

			// Send our event
			{
				const int32 Timestamp = PMTimestamp;
				EMIDIEventType EventType = EMIDIEventType::Unknown;

				EMIDIEventType PossibleEventType = (EMIDIEventType)PMType;
				switch( PossibleEventType )
				{
					case EMIDIEventType::NoteOff:
					case EMIDIEventType::NoteOn:
					case EMIDIEventType::NoteAfterTouch:
					case EMIDIEventType::ControlChange:
					case EMIDIEventType::ProgramChange:
					case EMIDIEventType::ChannelAfterTouch:
					case EMIDIEventType::PitchBend:
						EventType = PossibleEventType;	// NOTE: Our values match up, so we can just cast/assign
						break;
				}
				
				const int32 Channel = PMChannel;
				const int32 ControlID = PMMessageData1;
				const int32 Velocity = PMMessageData2;
				const int32 RawEventType = PMType;

				this->OnMIDIEvent.Broadcast( this, Timestamp, EventType, Channel, ControlID, Velocity, RawEventType );
			}
		}
	}
}



#undef LOCTEXT_NAMESPACE
