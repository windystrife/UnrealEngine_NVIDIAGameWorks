// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "UObject/NameTypes.h"

#include "ImgMediaSource.h"

#include "Customizations/ImgMediaSourceCustomization.h"


/**
 * Implements the ImgMediaEditor module.
 */
class FImgMediaEditorModule
	: public IModuleInterface
{
public:

	//~ IModuleInterface interface

	virtual void StartupModule() override
	{
		RegisterCustomizations();
	}

	virtual void ShutdownModule() override
	{
		UnregisterCustomizations();
	}

protected:

	/** Register details view customizations. */
	void RegisterCustomizations()
	{
		ImgMediaSourceName = UImgMediaSource::StaticClass()->GetFName();

		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		{
			PropertyModule.RegisterCustomClassLayout(ImgMediaSourceName, FOnGetDetailCustomizationInstance::CreateStatic(&FImgMediaSourceCustomization::MakeInstance));
		}
	}

	/** Unregister details view customizations. */
	void UnregisterCustomizations()
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		{
			PropertyModule.UnregisterCustomClassLayout(ImgMediaSourceName);
		}
	}

private:

	/** Class names. */
	FName ImgMediaSourceName;
};


IMPLEMENT_MODULE(FImgMediaEditorModule, ImgMediaEditor);
