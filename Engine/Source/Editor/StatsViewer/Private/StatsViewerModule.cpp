// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "StatsViewerModule.h"
#include "Modules/ModuleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "SStatsViewer.h"
#include "StatsPageManager.h"
#include "StatsPages/CookerStatsPage.h"
#include "StatsPages/LightingBuildInfoStatsPage.h"
#include "StatsPages/PrimitiveStatsPage.h"
#include "StatsPages/StaticMeshLightingInfoStatsPage.h"
#include "StatsPages/TextureStatsPage.h"
#include "ObjectHyperlinkColumn.h"


#define LOCTEXT_NAMESPACE "Editor.StatsViewer"

IMPLEMENT_MODULE( FStatsViewerModule, StatsViewer );

const FName StatsViewerApp = FName("StatsViewerApp");

static const FName CookerStatsPage = FName("CookerStats");
static const FName LightingBuildInfoPage = FName("LightingBuildInfo");
static const FName PrimitiveStatsPage = FName("PrimitiveStats");
static const FName StaticMeshLightingInfoPage = FName("StaticMeshLightingInfo");
static const FName TextureStatsPage = FName("TextureStats");

void FStatsViewerModule::StartupModule()
{
	FStatsPageManager::Get().RegisterPage( MakeShareable(&FCookerStatsPage::Get()) );
	FStatsPageManager::Get().RegisterPage( MakeShareable(&FLightingBuildInfoStatsPage::Get()) );
	FStatsPageManager::Get().RegisterPage( MakeShareable(&FPrimitiveStatsPage::Get()) );
	FStatsPageManager::Get().RegisterPage( MakeShareable(&FStaticMeshLightingInfoStatsPage::Get()) );
	FStatsPageManager::Get().RegisterPage( MakeShareable(&FTextureStatsPage::Get()) );
}

void FStatsViewerModule::ShutdownModule()
{
	FStatsPageManager::Get().UnregisterAllPages();
}

TSharedRef< IStatsViewer > FStatsViewerModule::CreateStatsViewer() const
{
	return SNew( SStatsViewer )
		.IsEnabled( FSlateApplication::Get().GetNormalExecutionAttribute() );
}

TSharedRef< class IPropertyTableCustomColumn > FStatsViewerModule::CreateObjectCustomColumn(const FObjectHyperlinkColumnInitializationOptions& InOptions) const
{
	return MakeShareable(new FObjectHyperlinkColumn(InOptions));
}

void FStatsViewerModule::RegisterPage( TSharedRef< IStatsPage > InPage )
{
	FStatsPageManager::Get().RegisterPage(InPage);
}

void FStatsViewerModule::UnregisterPage( TSharedRef< IStatsPage > InPage )
{
	FStatsPageManager::Get().UnregisterPage(InPage);
}

TSharedPtr< IStatsPage > FStatsViewerModule::GetPage( EStatsPage::Type InType )
{
	switch( InType )
	{
	case EStatsPage::CookerStats:
		return GetPage(CookerStatsPage);
	case EStatsPage::LightingBuildInfo:
		return GetPage(LightingBuildInfoPage);
	case EStatsPage::PrimitiveStats:
		return GetPage(PrimitiveStatsPage);
	case EStatsPage::StaticMeshLightingInfo:
		return GetPage(StaticMeshLightingInfoPage);
	case EStatsPage::TextureStats:
		return GetPage(TextureStatsPage);
	default:
		return NULL;
	}
}

TSharedPtr< IStatsPage > FStatsViewerModule::GetPage( const FName& InPageName )
{
	return FStatsPageManager::Get().GetPage(InPageName);
}

void FStatsViewerModule::Clear()
{
	for( int32 PageIndex = 0; PageIndex < FStatsPageManager::Get().NumPages(); PageIndex++ )
	{
		FStatsPageManager::Get().GetPage(PageIndex)->Clear();
	}
}

#undef LOCTEXT_NAMESPACE
