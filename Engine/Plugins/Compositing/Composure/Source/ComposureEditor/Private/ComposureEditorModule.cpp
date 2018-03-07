// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ComposureEditorModule.h"
#include "Modules/ModuleManager.h"
#include "ISequencerModule.h"
#include "Sequencer/ComposurePostMoveSettingsPropertyTrackEditor.h"

DEFINE_LOG_CATEGORY(LogComposureEditor);

class FComposureEditorModule : public IModuleInterface
{
	virtual void StartupModule() override
	{
		ISequencerModule& SequencerModule = FModuleManager::LoadModuleChecked<ISequencerModule>("Sequencer");
		CreatePostMoveSettingsPropertyTrackEditorHandle = SequencerModule.RegisterPropertyTrackEditor<FComposurePostMoveSettingsPropertyTrackEditor>();
	}

	virtual void ShutdownModule() override
	{
		ISequencerModule* SequencerModule = FModuleManager::GetModulePtr<ISequencerModule>("Sequencer");
		if (SequencerModule != nullptr)
		{
			SequencerModule->UnRegisterTrackEditor(CreatePostMoveSettingsPropertyTrackEditorHandle);
		}
	}

private:

	FDelegateHandle CreatePostMoveSettingsPropertyTrackEditorHandle;
};

IMPLEMENT_MODULE(FComposureEditorModule, ComposureEditor )