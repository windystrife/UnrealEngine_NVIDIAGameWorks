// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PLUGIN_NAMEEdMode.h"
#include "PLUGIN_NAMEEdModeToolkit.h"
#include "Toolkits/ToolkitManager.h"
#include "EditorModeManager.h"

const FEditorModeID FPLUGIN_NAMEEdMode::EM_PLUGIN_NAMEEdModeId = TEXT("EM_PLUGIN_NAMEEdMode");

FPLUGIN_NAMEEdMode::FPLUGIN_NAMEEdMode()
{

}

FPLUGIN_NAMEEdMode::~FPLUGIN_NAMEEdMode()
{

}

void FPLUGIN_NAMEEdMode::Enter()
{
	FEdMode::Enter();

	if (!Toolkit.IsValid() && UsesToolkits())
	{
		Toolkit = MakeShareable(new FPLUGIN_NAMEEdModeToolkit);
		Toolkit->Init(Owner->GetToolkitHost());
	}
}

void FPLUGIN_NAMEEdMode::Exit()
{
	if (Toolkit.IsValid())
	{
		FToolkitManager::Get().CloseToolkit(Toolkit.ToSharedRef());
		Toolkit.Reset();
	}

	// Call base Exit method to ensure proper cleanup
	FEdMode::Exit();
}

bool FPLUGIN_NAMEEdMode::UsesToolkits() const
{
	return true;
}




