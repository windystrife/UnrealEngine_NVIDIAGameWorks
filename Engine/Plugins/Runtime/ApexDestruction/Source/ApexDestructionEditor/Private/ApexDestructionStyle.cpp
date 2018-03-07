// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ApexDestructionStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateTypes.h"
#include "EditorStyleSet.h"
#include "Interfaces/IPluginManager.h"
#include "SlateOptMacros.h"

#define IMAGE_PLUGIN_BRUSH( RelativePath, ... ) FSlateImageBrush( FApexDestructionStyle::InContent( RelativePath, ".png" ), __VA_ARGS__ )
#define IMAGE_BRUSH(RelativePath, ...) FSlateImageBrush(StyleSet->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)

FString FApexDestructionStyle::InContent(const FString& RelativePath, const ANSICHAR* Extension)
{
	static FString ContentDir = IPluginManager::Get().FindPlugin(TEXT("ApexDestruction"))->GetContentDir();
	return (ContentDir / RelativePath) + Extension;
}

TSharedPtr< class FSlateStyleSet > FApexDestructionStyle::StyleSet;

FName FApexDestructionStyle::GetStyleSetName()
{
	static FName PaperStyleName(TEXT("ApexDestructionStyle"));
	return PaperStyleName;
}


void FApexDestructionStyle::Initialize()
{
	// Only register once
	if (StyleSet.IsValid())
	{
		return;
	}

	StyleSet = MakeShareable(new FSlateStyleSet(GetStyleSetName()));
	StyleSet->SetContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate"));
	StyleSet->SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

	// Const icon sizes
	const FVector2D Icon16x16(16.0f, 16.0f);
	const FVector2D Icon20x20(20.0f, 20.0f);
	const FVector2D Icon40x40(40.0f, 40.0f);

	StyleSet->Set("ClassIcon.DestructibleActor", new IMAGE_PLUGIN_BRUSH("Icons/Destructible_16px", Icon16x16));

	StyleSet->Set("ClassIcon.DestructibleComponent", new IMAGE_PLUGIN_BRUSH("Icons/Destructible_16px", Icon16x16));

	StyleSet->Set( "DestructibleMeshEditor.Tabs.Properties", new IMAGE_BRUSH( "/Icons/icon_tab_SelectionDetails_16x", Icon16x16 ) );
	StyleSet->Set( "DestructibleMeshEditor.Tabs.DestructibleSettings", new IMAGE_PLUGIN_BRUSH( "/Icons/icon_DestructibleMeshEd_Destructible_Settings_16x", Icon16x16 ) );
	StyleSet->Set( "DestructibleMeshEditor.Tabs.ChunkParameters", new IMAGE_PLUGIN_BRUSH( "/Icons/icon_DestructibleMeshEd_Chunk_Parameters_16x", Icon16x16 ) );
	StyleSet->Set( "DestructibleMeshEditor.Tabs.FractureSettings", new IMAGE_PLUGIN_BRUSH( "/Icons/icon_DestructibleMeshEd_Fracture_Settings_16x", Icon16x16 ) );

	StyleSet->Set( "DestructibleMeshEditor.Fracture", new IMAGE_PLUGIN_BRUSH( "Icons/icon_DestructibleMeshEd_Fracture_40x", Icon40x40 ) );
	StyleSet->Set( "DestructibleMeshEditor.Fracture.Small", new IMAGE_PLUGIN_BRUSH( "Icons/icon_DestructibleMeshEd_Fracture_20x", Icon20x20 ) );
	StyleSet->Set( "DestructibleMeshEditor.Refresh", new IMAGE_PLUGIN_BRUSH( "Icons/icon_DestructibleMeshEd_Refresh_40x", Icon40x40 ) );
	StyleSet->Set( "DestructibleMeshEditor.Refresh.Small", new IMAGE_PLUGIN_BRUSH( "Icons/icon_DestructibleMeshEd_Refresh_40x", Icon20x20 ) );
	StyleSet->Set( "DestructibleMeshEditor.ImportFBXChunks", new IMAGE_PLUGIN_BRUSH( "Icons/icon_DestructibleMeshEd_ImportFBX_40x", Icon40x40 ) );
	StyleSet->Set( "DestructibleMeshEditor.ImportFBXChunks.Small", new IMAGE_PLUGIN_BRUSH( "Icons/icon_DestructibleMeshEd_ImportFBX_40x", Icon20x20 ) );

	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet.Get());
};

#undef IMAGE_PLUGIN_BRUSH
#undef IMAGE_BRUSH

void FApexDestructionStyle::Shutdown()
{
	if (StyleSet.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet.Get());
		ensure(StyleSet.IsUnique());
		StyleSet.Reset();
	}
}