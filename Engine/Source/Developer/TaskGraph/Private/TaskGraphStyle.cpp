// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "TaskGraphStyle.h"
#include "Styling/SlateStyleRegistry.h"

#define IMAGE_BRUSH( RelativePath, ... ) FSlateImageBrush( RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define BOX_BRUSH( RelativePath, ... ) FSlateBoxBrush( RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define BORDER_BRUSH( RelativePath, ... ) FSlateBorderBrush( RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )

TSharedPtr< FTaskGraphStyle::FStyle > FTaskGraphStyle::StyleInstance;

FTaskGraphStyle::FStyle::FStyle()
	: FSlateStyleSet("TaskGraphStyle")
{

}

void FTaskGraphStyle::FStyle::Initialize()
{
#if WITH_EDITORONLY_DATA
	StyleInstance->SetContentRoot( FPaths::EngineContentDir() / TEXT("Editor/Slate") );
	StyleInstance->SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

	StyleInstance->Set( "StatsHeader", new BOX_BRUSH( "Common/TableViewHeader", 4.f/32.f ) );
	StyleInstance->Set( "TaskGraph.ContentAreaBrush", new BOX_BRUSH( "/Docking/TabContentArea", FMargin(4/16.0f) ) );
	StyleInstance->Set( "TaskGraph.Background", new BOX_BRUSH( "Common/ProgressBar_Background", FMargin(5.f/12.f) ) );
	StyleInstance->Set( "TaskGraph.Normal", new BOX_BRUSH( "Common/TaskGraph_Normal", FMargin(5.f/12.f) ) );
	StyleInstance->Set( "TaskGraph.Selected", new BOX_BRUSH( "Common/TaskGraph_Selected", FMargin(5.f/12.f) ) );
	StyleInstance->Set( "TaskGraph.Mono", new BOX_BRUSH( "Common/TaskGraph_Mono", FMargin(5.f/12.f) ) );
	StyleInstance->Set( "TaskGraph.BorderPadding", FVector2D(1,0) );
	StyleInstance->Set( "TaskGraph.SortUp", new IMAGE_BRUSH( "Common/SortUpArrow", FVector2D(8,4) ) );
	StyleInstance->Set( "TaskGraph.SortDown", new IMAGE_BRUSH( "Common/SortDownArrow", FVector2D(8,4) ) );
	StyleInstance->Set( "TaskGraph.Home", new IMAGE_BRUSH( "Icons/Home16x16", FVector2D(16,16) ) );
	StyleInstance->Set( "TaskGraph.ToParent", new IMAGE_BRUSH( "Icons/ToParent", FVector2D(16,16) ) );
	StyleInstance->Set( "TaskGraph.ProgressBar.BorderPadding", FVector2D(1,0) );
	StyleInstance->Set( "TaskGraph.MenuDropdown", new IMAGE_BRUSH( "Common/ComboArrow", FVector2D(8,8) ) );

	FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance.Get());
#endif
}


void FTaskGraphStyle::Initialize() 
{
	StyleInstance = MakeShareable( new FTaskGraphStyle::FStyle );
	StyleInstance->Initialize();
}

void FTaskGraphStyle::Shutdown()
{
	if (StyleInstance.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance.Get());
		ensure(StyleInstance.IsUnique());
		StyleInstance.Reset();
	}
}

TSharedPtr< ISlateStyle > FTaskGraphStyle::Get()
{
	return StyleInstance;
}
