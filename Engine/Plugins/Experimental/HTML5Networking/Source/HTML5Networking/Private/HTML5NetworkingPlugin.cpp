// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "HTML5NetworkingPrivate.h"


class FHTML5NetworkingPlugin : public IModuleInterface
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE(FHTML5NetworkingPlugin, HTML5Networking)

void FHTML5NetworkingPlugin::StartupModule()
{
}


void FHTML5NetworkingPlugin::ShutdownModule()
{
}

DEFINE_LOG_CATEGORY(LogHTML5Networking);


