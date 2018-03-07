// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma  once 

/*------------------------------------------------------------------------------------
IAndroid_MultiTargetPlatformModule interface 
------------------------------------------------------------------------------------*/

#include "CoreMinimal.h"
#include "Interfaces/ITargetPlatformModule.h"

class IAndroid_MultiTargetPlatformModule
	: public ITargetPlatformModule
{
public:
	//
	// Called by AndroidRuntimeSettings to notify us when the user changes the selected texture formats
	//
	virtual void NotifySelectedFormatsChanged() = 0;
};
