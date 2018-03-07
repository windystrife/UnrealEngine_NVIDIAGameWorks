// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetEditorModeManager.h"
#include "Engine/Selection.h"
#include "PreviewScene.h"

//////////////////////////////////////////////////////////////////////////
// FAssetEditorModeManager

FAssetEditorModeManager::FAssetEditorModeManager()
	: PreviewScene(nullptr)
{
	ActorSet = NewObject<USelection>();
	ActorSet->SetFlags(RF_Transactional);
	ActorSet->AddToRoot();
	ActorSet->Initialize(nullptr);

	ObjectSet = NewObject<USelection>();
	ObjectSet->SetFlags(RF_Transactional);
	ObjectSet->AddToRoot();
	ObjectSet->Initialize(nullptr);

	ComponentSet = NewObject<USelection>();
	ComponentSet->SetFlags(RF_Transactional);
	ComponentSet->AddToRoot();
	ComponentSet->Initialize(nullptr);
}

FAssetEditorModeManager::~FAssetEditorModeManager()
{
	ActorSet->RemoveFromRoot();
	ActorSet = nullptr;
	ObjectSet->RemoveFromRoot();
	ObjectSet = nullptr;
	ComponentSet->RemoveFromRoot();
	ComponentSet = nullptr;
}

USelection* FAssetEditorModeManager::GetSelectedActors() const
{
	return ActorSet;
}

USelection* FAssetEditorModeManager::GetSelectedObjects() const
{
	return ObjectSet;
}

USelection* FAssetEditorModeManager::GetSelectedComponents() const
{
	return ComponentSet;
}

UWorld* FAssetEditorModeManager::GetWorld() const
{
	return (PreviewScene != nullptr) ? PreviewScene->GetWorld() : GEditor->GetEditorWorldContext().World();
}

void FAssetEditorModeManager::SetPreviewScene(class FPreviewScene* NewPreviewScene)
{
	PreviewScene = NewPreviewScene;
}

FPreviewScene* FAssetEditorModeManager::GetPreviewScene() const
{
	return PreviewScene;
}
