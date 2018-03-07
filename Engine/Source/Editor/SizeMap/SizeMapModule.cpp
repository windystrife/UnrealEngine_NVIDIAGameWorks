// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SizeMapModule.h"
#include "UObject/NameTypes.h"
#include "Templates/SharedPointer.h"
#include "Internationalization/Internationalization.h"
#include "Modules/ModuleManager.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Framework/Docking/TabManager.h"
#include "ISizeMapModule.h"
#include "SSizeMap.h"
#include "Widgets/Docking/SDockTab.h"
#include "SlateApplication.h"

#define LOCTEXT_NAMESPACE "SizeMap"

class FSizeMapModule : public ISizeMapModule
{
public:
	FSizeMapModule()
		: SizeMapTabId("SizeMap")
	{
	}

	virtual void StartupModule() override
	{
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(SizeMapTabId, FOnSpawnTab::CreateRaw( this, &FSizeMapModule::SpawnSizeMapTab ) )
			.SetDisplayName( LOCTEXT("SizeMapTitle", "Size Map") )
			.SetMenuType( ETabSpawnerMenuType::Hidden );
	}

	virtual void ShutdownModule() override
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(SizeMapTabId);
	}

	virtual void InvokeSizeMapTab(const TArray<FName>& AssetPackageNames) override
	{
		TSharedRef<SDockTab> NewTab = FGlobalTabmanager::Get()->InvokeTab( SizeMapTabId );
		TSharedRef<SSizeMap> SizeMap = StaticCastSharedRef<SSizeMap>( NewTab->GetContent() );
		SizeMap->SetRootAssetPackageNames( AssetPackageNames );
	}

	virtual void InvokeSizeMapModalDialog(const TArray<FName>& AssetPackageNames, TSharedPtr<SWindow> ParentWindow) override
	{
		TSharedRef<SWindow> Window = SNew(SWindow)
			.Title(NSLOCTEXT("UnrealEd", "SizeMapTitle", "Size Map"))
			.SizingRule(ESizingRule::UserSized)
			.ClientSize(FVector2D(800,600))
			.AutoCenter(EAutoCenter::PreferredWorkArea);

		TSharedPtr<SSizeMap> SizeMap = SNew(SSizeMap)
			.SelectAssetOnDoubleClick(false);

		Window->SetContent(SizeMap.ToSharedRef());
		SizeMap->SetRootAssetPackageNames(AssetPackageNames);

		FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);
	}

private:
	TSharedRef<SDockTab> SpawnSizeMapTab( const FSpawnTabArgs& SpawnTabArgs )
	{
		TSharedRef<SDockTab> NewTab = SNew(SDockTab)
			.TabRole(ETabRole::NomadTab);

		NewTab->SetContent( SNew(SSizeMap) );

		return NewTab;
	}

private:
	FName SizeMapTabId;
};


IMPLEMENT_MODULE( FSizeMapModule, SizeMap )


#undef LOCTEXT_NAMESPACE
