// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_SoundSourceBus.h"
#include "Misc/PackageName.h"
#include "AssetData.h"
#include "EditorStyleSet.h"
#include "Factories/SoundSourceBusFactory.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "PropertyCustomizationHelpers.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "AudioEditorModule.h"


UClass* FAssetTypeActions_SoundSourceBus::GetSupportedClass() const
{
	return USoundSourceBus::StaticClass();
}


