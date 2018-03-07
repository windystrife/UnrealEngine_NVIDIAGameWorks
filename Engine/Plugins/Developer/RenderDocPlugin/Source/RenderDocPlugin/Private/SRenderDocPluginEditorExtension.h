// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "Editor/UnrealEd/Public/SViewportToolBar.h"

class FRenderDocPluginModule;
class FExtensionBase;
class FExtensibilityManager;
class FExtender;
class FToolBarBuilder;

class FRenderDocPluginEditorExtension
{
public:
  FRenderDocPluginEditorExtension(FRenderDocPluginModule* ThePlugin);
  ~FRenderDocPluginEditorExtension();

private:
  void Initialize(FRenderDocPluginModule* ThePlugin);
  void OnEditorLoaded(SWindow& SlateWindow, void* ViewportRHIPtr);
  void AddToolbarExtension(FToolBarBuilder& ToolbarBuilder, FRenderDocPluginModule* ThePlugin);

  FDelegateHandle LoadedDelegateHandle;
  TSharedPtr<const FExtensionBase> ToolbarExtension;
  TSharedPtr<FExtensibilityManager> ExtensionManager;
  TSharedPtr<FExtender> ToolbarExtender;
  bool IsEditorInitialized;
};

#endif //WITH_EDITOR
