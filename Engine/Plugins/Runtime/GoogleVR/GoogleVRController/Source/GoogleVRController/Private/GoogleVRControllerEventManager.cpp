// Copyright 2017 Google Inc.


#include "Classes/GoogleVRControllerEventManager.h"
#include "GoogleVRController.h"
#include "GoogleVRControllerPrivate.h"

static UGoogleVRControllerEventManager* Singleton = nullptr;

UGoogleVRControllerEventManager::UGoogleVRControllerEventManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UGoogleVRControllerEventManager* UGoogleVRControllerEventManager::GetInstance()
{
	if (!Singleton)
	{
		Singleton = NewObject<UGoogleVRControllerEventManager>();
		Singleton->AddToRoot();
	}
	return Singleton;
}