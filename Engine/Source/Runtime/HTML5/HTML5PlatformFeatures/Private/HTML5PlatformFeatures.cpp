// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "PlatformFeatures.h"
#include "HTML5SaveGameSystem.h"

class FHTML5PlatformFeatures : public IPlatformFeaturesModule
{
public:
	virtual class ISaveGameSystem* GetSaveGameSystem() override
	{
		static FHTML5SaveGameSystem HTML5SaveGameSystem;
		return &HTML5SaveGameSystem;
	}
};

IMPLEMENT_MODULE(FHTML5PlatformFeatures, HTML5PlatformFeatures);
