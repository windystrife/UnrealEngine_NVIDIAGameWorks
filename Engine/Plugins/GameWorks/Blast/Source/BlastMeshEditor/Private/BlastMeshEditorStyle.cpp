// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BlastMeshEditorStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateTypes.h"
#include "EditorStyleSet.h"
#include "Interfaces/IPluginManager.h"
#include "SlateOptMacros.h"


#define IMAGE_PLUGIN_BRUSH( RelativePath, ... ) FSlateImageBrush( FBlastMeshEditorStyle::InContent( RelativePath, ".png" ), __VA_ARGS__ )
#define IMAGE_BRUSH(RelativePath, ...) FSlateImageBrush(StyleSet->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define BOX_BRUSH(RelativePath, ...) FSlateBoxBrush(StyleSet->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define TTF_FONT(RelativePath, ...) FSlateFontInfo(StyleSet->RootToContentDir(RelativePath, TEXT(".ttf")), __VA_ARGS__)
#define TTF_CORE_FONT(RelativePath, ...) FSlateFontInfo(StyleSet->RootToCoreContentDir(RelativePath, TEXT(".ttf") ), __VA_ARGS__)

FString FBlastMeshEditorStyle::InContent(const FString& RelativePath, const ANSICHAR* Extension)
{
	static FString ContentDir = IPluginManager::Get().FindPlugin(TEXT("Blast"))->GetContentDir();
	return (ContentDir / RelativePath) + Extension;
}

TSharedPtr< FSlateStyleSet > FBlastMeshEditorStyle::StyleSet = nullptr;
TSharedPtr< class ISlateStyle > FBlastMeshEditorStyle::Get() { return StyleSet; }

FName FBlastMeshEditorStyle::GetStyleSetName()
{
	static FName StyleName(TEXT("BlastMeshEditorStyle"));
	return StyleName;
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void FBlastMeshEditorStyle::Initialize()
{
	// Const icon sizes
	const FVector2D Icon12x12(12.0f, 12.0f);
	const FVector2D Icon16x16(16.0f, 16.0f);
	const FVector2D Icon20x20(20.0f, 20.0f);
	const FVector2D Icon40x40(40.0f, 40.0f);

	// Only register once
	if (StyleSet.IsValid())
	{
		return;
	}

	StyleSet = MakeShareable(new FSlateStyleSet(GetStyleSetName()));
	StyleSet->SetContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate"));
	StyleSet->SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

	StyleSet->Set("BlastMeshEditor.Fracture", new IMAGE_PLUGIN_BRUSH(TEXT("Icons/icon_BlastMeshEd_Fracture_40x"), Icon40x40));
	StyleSet->Set("BlastMeshEditor.Fracture.Small", new IMAGE_PLUGIN_BRUSH(TEXT("Icons/icon_BlastMeshEd_Fracture_20x"), Icon20x20));
	StyleSet->Set("BlastMeshEditor.Reset", new IMAGE_PLUGIN_BRUSH(TEXT("Icons/icon_BlastMeshEd_Reset_40x"), Icon40x40));
	StyleSet->Set("BlastMeshEditor.Reset.Small", new IMAGE_PLUGIN_BRUSH(TEXT("Icons/icon_BlastMeshEd_Reset_20x"), Icon20x20));
	StyleSet->Set("BlastMeshEditor.FixChunkHierarchy", new IMAGE_PLUGIN_BRUSH(TEXT("Icons/icon_BlastMeshEd_FixChunkHierarchy_40x"), Icon40x40));
	StyleSet->Set("BlastMeshEditor.FixChunkHierarchy.Small", new IMAGE_PLUGIN_BRUSH(TEXT("Icons/icon_BlastMeshEd_FixChunkHierarchy_20x"), Icon20x20));
	StyleSet->Set("BlastMeshEditor.ImportRootFromStaticMesh", new IMAGE_PLUGIN_BRUSH(TEXT("Icons/icon_BlastMeshEd_ImportFromStatic_40x"), Icon40x40));
	StyleSet->Set("BlastMeshEditor.ImportRootFromStaticMesh.Small", new IMAGE_PLUGIN_BRUSH(TEXT("Icons/icon_BlastMeshEd_ImportFromStatic_20x"), Icon20x20));
	StyleSet->Set("BlastMeshEditor.FitUvCoordinates", new IMAGE_PLUGIN_BRUSH(TEXT("Icons/icon_BlastMeshEd_FitUV_40x"), Icon40x40));
	StyleSet->Set("BlastMeshEditor.FitUvCoordinates.Small", new IMAGE_PLUGIN_BRUSH(TEXT("Icons/icon_BlastMeshEd_FitUV_20x"), Icon20x20));
	StyleSet->Set("BlastMeshEditor.RebuildCollisionMesh", new IMAGE_PLUGIN_BRUSH(TEXT("Icons/icon_BlastMeshEd_RebuildCollisionMesh_40x"), Icon40x40));
	StyleSet->Set("BlastMeshEditor.RebuildCollisionMesh.Small", new IMAGE_PLUGIN_BRUSH(TEXT("Icons/icon_BlastMeshEd_RebuildCollisionMesh_20x"), Icon20x20));
	StyleSet->Set("BlastMeshEditor.ToggleCollisionMeshView", new IMAGE_PLUGIN_BRUSH(TEXT("Icons/icon_BlastMeshEd_RebuildCollisionMesh_40x"), Icon40x40));
	StyleSet->Set("BlastMeshEditor.ToggleCollisionMeshView.Small", new IMAGE_PLUGIN_BRUSH(TEXT("Icons/icon_BlastMeshEd_RebuildCollisionMesh_20x"), Icon20x20));
	StyleSet->Set("BlastMeshEditor.ToggleVoronoiSitesView", new IMAGE_PLUGIN_BRUSH(TEXT("Icons/icon_BlastMeshEd_Point_20x"), Icon40x40));
	StyleSet->Set("BlastMeshEditor.ToggleVoronoiSitesView.Small", new IMAGE_PLUGIN_BRUSH(TEXT("Icons/icon_BlastMeshEd_Point_20x"), Icon20x20));
	StyleSet->Set("BlastMeshEditor.Chunk", new IMAGE_PLUGIN_BRUSH(TEXT("Icons/icon_BlastMeshEd_Chunk_16x"), Icon16x16));
	StyleSet->Set("BlastMeshEditor.SupportChunk", new IMAGE_PLUGIN_BRUSH(TEXT("Icons/icon_BlastMeshEd_SupportChunk_16x"), Icon16x16));
	StyleSet->Set("BlastMeshEditor.StaticChunk", new IMAGE_PLUGIN_BRUSH(TEXT("Icons/icon_BlastMeshEd_StaticChunk_16x"), Icon16x16));
	StyleSet->Set("BlastMeshEditor.SupportStaticChunk", new IMAGE_PLUGIN_BRUSH(TEXT("Icons/icon_BlastMeshEd_SupportStaticChunk_16x"), Icon16x16));

	StyleSet->Set("BlastMeshEditor.Adjust", new IMAGE_PLUGIN_BRUSH(TEXT("Icons/icon_BlastMeshEd_Adjust_16x"), Icon16x16));
	StyleSet->Set("BlastMeshEditor.BlastVectorExit", new IMAGE_PLUGIN_BRUSH(TEXT("Icons/icon_BlastMeshEd_Cross_12x"), Icon12x12));
	StyleSet->Set("BlastMeshEditor.BlastVectorNormal", new IMAGE_PLUGIN_BRUSH(TEXT("Icons/icon_BlastMeshEd_Normal_20x"), Icon20x20));
	StyleSet->Set("BlastMeshEditor.BlastVectorPoint", new IMAGE_PLUGIN_BRUSH(TEXT("Icons/icon_BlastMeshEd_Point_20x"), Icon20x20));
	StyleSet->Set("BlastMeshEditor.BlastVectorTwoPoint", new IMAGE_PLUGIN_BRUSH(TEXT("Icons/icon_BlastMeshEd_TwoPoint_20x"), Icon20x20));
	StyleSet->Set("BlastMeshEditor.BlastVectorThreePoint", new IMAGE_PLUGIN_BRUSH(TEXT("Icons/icon_BlastMeshEd_ThreePoint_20x"), Icon20x20));


	//StyleSet->Set("BlastMeshEditor.ImportRootFromStaticMesh.Small", new IMAGE_PLUGIN_BRUSH(TEXT("Icons/icon_BlastMeshEd_ImportFBX_20x"), Icon20x20));

	StyleSet->Set("BlastMeshEditor.ExpandArrow", new IMAGE_BRUSH(TEXT("Icons/toolbar_expand_16x"), Icon16x16));

	//const FTextBlockStyle& NormalText = FEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText");

	//TODO move here all styles used in BME


	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet.Get());
};

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

#undef IMAGE_PLUGIN_BRUSH
#undef IMAGE_BRUSH
#undef BOX_BRUSH
#undef TTF_FONT
#undef TTF_CORE_FONT

void FBlastMeshEditorStyle::Shutdown()
{
	if (StyleSet.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet.Get());
		ensure(StyleSet.IsUnique());
		StyleSet.Reset();
	}
}
