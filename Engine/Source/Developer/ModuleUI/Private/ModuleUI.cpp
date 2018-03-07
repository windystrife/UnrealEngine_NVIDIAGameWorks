// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "ModuleUIInterface.h"
#include "SModuleUI.h"


/**
 * Implementation of the ModuleUI system
 */
class FModuleUI
	: public IModuleUIInterface
{

public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** IModuleUIInterface implementation */
	virtual TSharedRef< SWidget > GetModuleUIWidget() override;

private:

};


IMPLEMENT_MODULE( FModuleUI, ModuleUI );


void FModuleUI::StartupModule()
{
}



void FModuleUI::ShutdownModule()
{
}


TSharedRef< SWidget > FModuleUI::GetModuleUIWidget()
{
	return SNew( SModuleUI );
}
