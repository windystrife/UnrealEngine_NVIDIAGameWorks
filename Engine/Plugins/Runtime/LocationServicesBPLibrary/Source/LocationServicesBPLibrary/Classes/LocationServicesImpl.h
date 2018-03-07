// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LocationServicesBPLibrary.h"
#include "LocationServicesImpl.generated.h"


UCLASS(abstract)
class ULocationServicesImpl  : public UObject
{
	GENERATED_BODY()

public:
	/**
	* Called to set up the Location Service before use
	*
	* @param Accuracy - as seen in the enum above
	* @param UpdateFrequency - in milliseconds. (Android only)
	* @param MinDistance - minDistance before a location update, in meters. 0 here means "update asap"
	* @return - true if Initialization was succesful
	*/
	virtual bool InitLocationServices(ELocationAccuracy Accuracy, float UpdateFrequency, float MinDistanceFilter) { return false; }

	/**
	* Starts requesting location updates from the appropriate Location Service
	* @return - true if startup successful
	*/
	virtual bool StartLocationService() { return false;	}

	/**
	* Stops the updates of location from the Location Service that was started with StartLocationService
	* @return - true if stop is successful
	*/
	virtual bool StopLocationService() { return false;	}

	/**
	* Returns the last location information returned by the location service. If no location update has been made, will return
	* a default-value-filled struct.
	* @return - the last known location from updates
	*/
	virtual FLocationServicesData GetLastKnownLocation() { return FLocationServicesData(); }
	
	/**
	* Checks if the supplied Accuracy is available on the current device.
	* @param Accuracy - the accuracy to check
	* @return - true if the mobile device can support the Accuracy, false if it will use a different accuracy
	*/
	virtual bool IsLocationAccuracyAvailable(ELocationAccuracy Accuracy) { return false;  }

	/** @return true if the mobile device has enabled the appropriate service for the app */	
	virtual bool IsLocationServiceEnabled() { return false;	};

	UPROPERTY(BlueprintAssignable, Category = "Services|Mobile|Location")
	FLocationServicesData_OnLocationChanged OnLocationChanged;

};
