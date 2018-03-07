// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LightPropagationVolumeModule.cpp: Module encapsulates the asset type for blending LightPropagationVolume settings
=============================================================================*/

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "AssetToolsModule.h"
#include "IAssetTypeActions.h"
#include "LightPropagationVolumeBlendable.h"
#include "AssetTypeActions_LightPropagationVolumeBlendable.h"
#include "LightPropagationVolumeBlendableFactory.h"

#define LOCTEXT_NAMESPACE "LightPropagationVolume"

/////////////////////////////////////////////////////
// ULightPropagationVolumeBlendableFactory

ULightPropagationVolumeBlendableFactory::ULightPropagationVolumeBlendableFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = ULightPropagationVolumeBlendable::StaticClass();
}

UObject* ULightPropagationVolumeBlendableFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<ULightPropagationVolumeBlendable>(InParent, Class, Name, Flags);
}

//////////////////////////////////////////////////////////////////////////
// FLightPropagationVolumeModule

class FLightPropagationVolumeModule : public IModuleInterface
{
	virtual void StartupModule() override
	{
		// Register asset types
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

 		EAssetTypeCategories::Type AssetCategoryBit = AssetTools.FindAdvancedAssetCategory(FName(TEXT("Blendables")));
		
		// this category should have been registered already
		check(AssetCategoryBit != EAssetTypeCategories::Misc);

		RegisterAssetTypeAction(AssetTools, MakeShareable(new FAssetTypeActions_LightPropagationVolumeBlendable(AssetCategoryBit)));
	}

	virtual void ShutdownModule() override
	{
		// Unregister all the asset types that we registered
		if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
		{
			IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();

			for (int32 Index = 0; Index < CreatedAssetTypeActions.Num(); ++Index)
			{
				AssetTools.UnregisterAssetTypeActions(CreatedAssetTypeActions[Index].ToSharedRef());
			}
		}
		CreatedAssetTypeActions.Empty();
	}

private:
	
	/** All created asset type actions.  Cached here so that we can unregister them during shutdown. */
	TArray< TSharedPtr<IAssetTypeActions> > CreatedAssetTypeActions;

	void RegisterAssetTypeAction(IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action)
	{
		AssetTools.RegisterAssetTypeActions(Action);
		CreatedAssetTypeActions.Add(Action);
	}
};

//////////////////////////////////////////////////////////////////////////

IMPLEMENT_MODULE(FLightPropagationVolumeModule, LightPropagationVolumeEditor);

//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
