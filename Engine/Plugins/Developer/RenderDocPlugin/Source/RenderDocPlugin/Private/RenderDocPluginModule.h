// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IRenderDocPlugin.h"

#include "RenderDocPluginLoader.h"
#include "RenderDocPluginSettings.h"

#if WITH_EDITOR
#include "Editor/LevelEditor/Public/LevelEditor.h"
#include "MultiBoxExtender.h"
#include "RenderDocPluginStyle.h"
#include "RenderDocPluginCommands.h"
#include "SRenderDocPluginEditorExtension.h"
#endif

#include "SharedPointer.h"

DECLARE_LOG_CATEGORY_EXTERN(RenderDocPlugin, Log, All);

class FRenderDocPluginModule : public IRenderDocPlugin
{
public:	
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	void Tick(float DeltaTime);
	void CaptureFrame();
	void StartRenderDoc(FString FrameCaptureBaseDirectory);
	FString GetNewestCapture(FString BaseDirectory);

private:
	virtual TSharedPtr<class IInputDevice> CreateInputDevice(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler) override;

	void BeginCapture();
	void EndCapture();
	void CaptureCurrentViewport();	
	void CaptureEntireFrame();

	/** Injects a debug key bind into the local player so that the hot key works the same in game */
	void InjectDebugExecKeybind();

private:
	FRenderDocPluginLoader Loader;
	FRenderDocPluginLoader::RENDERDOC_API_CONTEXT* RenderDocAPI;
	uint32 TickNumber; // Tracks the frame count (tick number) for a full frame capture	

#if WITH_EDITOR
	FRenderDocPluginEditorExtension* EditorExtensions;
#endif
};
