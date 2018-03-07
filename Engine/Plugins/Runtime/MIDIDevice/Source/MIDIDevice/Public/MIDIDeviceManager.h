// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MIDIDeviceManager.generated.h"


USTRUCT( BlueprintType )
struct MIDIDEVICE_API FFoundMIDIDevice
{
	GENERATED_BODY()

	/** The unique ID of this MIDI device */
	UPROPERTY( BlueprintReadOnly, Category="MIDI Device Manager")
	int32 DeviceID;

	/** The name of this device.  This name comes from the MIDI hardware, any might not be unique */
	UPROPERTY( BlueprintReadOnly, Category="MIDI Device Manager" )
	FString DeviceName;

	/** True if the device supports sending events to us */
	UPROPERTY( BlueprintReadOnly, Category="MIDI Device Manager" )
	bool bCanReceiveFrom;

	/** True if the device supports receiving events from us */
	UPROPERTY( BlueprintReadOnly, Category="MIDI Device Manager" )
	bool bCanSendTo;

	/** Whether the device is already in use.  You might not want to create a controller for devices that are busy.  Someone else could be using it. */
	UPROPERTY( BlueprintReadOnly, Category="MIDI Device Manager" )
	bool bIsAlreadyInUse;

	/** True if this is the default MIDI device for input on this system */
	UPROPERTY( BlueprintReadOnly, Category="MIDI Device Manager" )
	bool bIsDefaultInputDevice;

	/** True if this is the default MIDI device for output on this system */
	UPROPERTY( BlueprintReadOnly, Category="MIDI Device Manager" )
	bool bIsDefaultOutputDevice;
};



UCLASS()
class MIDIDEVICE_API UMIDIDeviceManager : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/**
	 * Enumerates all of the connected MIDI devices and reports back with the IDs and names of those devices.  This operation is a little expensive
	 * so only do it once at startup, or if you think that a new device may have been connected.
	 *
	 * @param	
	 */
	UFUNCTION( BlueprintCallable, Category="MIDI Device Manager")
	static void FindMIDIDevices( TArray<FFoundMIDIDevice>& OutMIDIDevices );


	/**
	 * Creates an instance of a MIDI device controller that can be used to interact with a connected MIDI device
	 *
	 * @param	DeviceID		The ID of the MIDI device you want to talk to.  Call "Find MIDI Devices" to enumerate the available devices.
	 * @param	MIDIBufferSize	How large the buffer size (in number of MIDI events) should be for incoming MIDI data.  Larger values can incur higher latency costs for incoming events, but don't set it too low or you'll miss events and your stuff will sound bad.
	 *
	 * @return	If everything goes okay, a valid MIDI device controller object will be returned.  If anything goes wrong, a null reference will be returned.
	 */
	UFUNCTION( BlueprintCallable, Category="MIDI Device Manager")
	static class UMIDIDeviceController* CreateMIDIDeviceController( const int32 DeviceID, const int32 MIDIBufferSize = 1024 );


	/** Called from FMIDIDeviceModule to startup the device manager.  Don't call this yourself. */
	static void StartupMIDIDeviceManager();

	/** Called from FMIDIDeviceModule to shutdown the device manager.  Don't call this yourself. */
	static void ShutdownMIDIDeviceManager();

	/** Called every frame to look for any new MIDI events that were received, and routes those events to subscribers.  Don't call this yourself.
	    It will be called by FMIDIDeviceModule::Tick(). */
	static void ProcessIncomingMIDIEvents();


private:

	/** True if everything is initialized OK */
	static bool bIsInitialized;

};
