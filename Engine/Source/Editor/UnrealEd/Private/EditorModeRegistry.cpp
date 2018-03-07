// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "EditorModeRegistry.h"
#include "Modules/ModuleManager.h"
#include "EditorStyleSet.h"
#include "EdMode.h"
#include "EditorModes.h"
#include "EditorModeInterpolation.h"

#include "Editor/PlacementMode/Public/IPlacementModeModule.h"
#include "Editor/LandscapeEditor/Public/LandscapeEditorModule.h"
#include "Editor/BspMode/Public/IBspModeModule.h"
#include "Editor/MeshPaint/Public/MeshPaintModule.h"
#include "Editor/GeometryMode/Public/GeometryEdMode.h"
#include "Editor/ActorPickerMode/Public/ActorPickerMode.h"
#include "Editor/SceneDepthPickerMode/Public/SceneDepthPickerMode.h"
#include "Editor/TextureAlignMode/Public/TextureAlignEdMode.h"
#include "Editor/FoliageEdit/Public/FoliageEditModule.h"

FEditorModeInfo::FEditorModeInfo()
	: ID(NAME_None)
	, bVisible(false)
	, PriorityOrder(MAX_int32)
{
}

FEditorModeInfo::FEditorModeInfo(
	FEditorModeID InID,
	FText InName,
	FSlateIcon InIconBrush,
	bool InIsVisible,
	int32 InPriorityOrder
	)
	: ID(InID)
	, Name(InName)
	, IconBrush(InIconBrush)
	, bVisible(InIsVisible)
	, PriorityOrder(InPriorityOrder)
{
	if (!InIconBrush.IsSet())
	{
		IconBrush = FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.EditorModes");
	}
}

FEditorModeRegistry* GModeRegistry = nullptr;

void FEditorModeRegistry::Initialize()
{
	Get();

	// Add default editor modes
	GModeRegistry->RegisterMode<FEdModeDefault>(FBuiltinEditorModes::EM_Default);
	GModeRegistry->RegisterMode<FEdModeInterpEdit>(FBuiltinEditorModes::EM_InterpEdit);

	// Load editor mode modules that will automatically register their editor modes, and clean themselves up on unload.
	//@TODO: ROCKET: These are probably good plugin candidates, that shouldn't have to be force-loaded here but discovery loaded somehow
	FModuleManager::LoadModuleChecked<IPlacementModeModule>(TEXT("PlacementMode"));
	FModuleManager::LoadModuleChecked<IBspModeModule>(TEXT("BspMode"));
	FModuleManager::LoadModuleChecked<FTextureAlignModeModule>(TEXT("TextureAlignMode"));
	FModuleManager::LoadModuleChecked<FGeometryModeModule>(TEXT("GeometryMode"));
	FModuleManager::LoadModuleChecked<FActorPickerModeModule>(TEXT("ActorPickerMode"));
	FModuleManager::LoadModuleChecked<FSceneDepthPickerModeModule>(TEXT("SceneDepthPickerMode"));
	FModuleManager::LoadModuleChecked<IMeshPaintModule>(TEXT("MeshPaintMode"));
	FModuleManager::LoadModuleChecked<ILandscapeEditorModule>(TEXT("LandscapeEditor"));
	FModuleManager::LoadModuleChecked<IFoliageEditModule>(TEXT("FoliageEdit"));
}

void FEditorModeRegistry::Shutdown()
{
	delete GModeRegistry;
	GModeRegistry = nullptr;
}

FEditorModeRegistry& FEditorModeRegistry::Get()
{
	if (!GModeRegistry)
	{
		GModeRegistry = new FEditorModeRegistry;
	}
	return *GModeRegistry;	
}

TArray<FEditorModeInfo> FEditorModeRegistry::GetSortedModeInfo() const
{
	TArray<FEditorModeInfo> ModeInfoArray;
	
	for (const auto& Pair : ModeFactories)
	{
		ModeInfoArray.Add(Pair.Value->GetModeInfo());
	}

	ModeInfoArray.Sort([](const FEditorModeInfo& A, const FEditorModeInfo& B){
		return A.PriorityOrder < B.PriorityOrder;
	});

	return ModeInfoArray;
}

FEditorModeInfo FEditorModeRegistry::GetModeInfo(FEditorModeID ModeID) const
{
	FEditorModeInfo Result;
	const TSharedRef<IEditorModeFactory>* ModeFactory = ModeFactories.Find(ModeID);
	if (ModeFactory)
	{
		Result = (*ModeFactory)->GetModeInfo();
	}
	
	return Result;
}

TSharedPtr<FEdMode> FEditorModeRegistry::CreateMode(FEditorModeID ModeID, FEditorModeTools& Owner)
{
	const TSharedRef<IEditorModeFactory>* ModeFactory = ModeFactories.Find(ModeID);
	if (ModeFactory)
	{
		TSharedRef<FEdMode> Instance = (*ModeFactory)->CreateMode();

		// Assign the mode info from the factory before we initialize
		Instance->Info = (*ModeFactory)->GetModeInfo();
		Instance->Owner = &Owner;

		// This binding ensures the mode is destroyed if the type is unregistered
		OnModeUnregistered().AddSP(Instance, &FEdMode::OnModeUnregistered);

		Instance->Initialize();

		return Instance;
	}

	return nullptr;
}

void FEditorModeRegistry::RegisterMode(FEditorModeID ModeID, TSharedRef<IEditorModeFactory> Factory)
{
	check(ModeID != FBuiltinEditorModes::EM_None);
	check(!ModeFactories.Contains(ModeID));

	ModeFactories.Add(ModeID, Factory);

	OnModeRegisteredEvent.Broadcast(ModeID);
	RegisteredModesChanged.Broadcast();
}

void FEditorModeRegistry::UnregisterMode(FEditorModeID ModeID)
{
	// First off delete the factory
	ModeFactories.Remove(ModeID);

	OnModeUnregisteredEvent.Broadcast(ModeID);
	RegisteredModesChanged.Broadcast();
}
