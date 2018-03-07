// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Delegates/IDelegateInstance.h"
#include "ISequencerModule.h"
#include "Modules/ModuleManager.h"

#include "MediaCompositingEditorStyle.h"
#include "MediaTrackEditor.h"


#define LOCTEXT_NAMESPACE "MediaCompositingEditorModule"


/**
 * Implements the MediaCompositing module.
 */
class FMediaCompositingEditorModule
	: public IModuleInterface
{
public:

	virtual void StartupModule() override
	{
		FMediaCompositingEditorStyle::Get();

		ISequencerModule& SequencerModule = FModuleManager::Get().LoadModuleChecked<ISequencerModule>("Sequencer");
		TrackEditorBindingHandle = SequencerModule.RegisterPropertyTrackEditor<FMediaTrackEditor>();
	}
	
	virtual void ShutdownModule() override
	{
		FMediaCompositingEditorStyle::Destroy();

		ISequencerModule* SequencerModulePtr = FModuleManager::Get().GetModulePtr<ISequencerModule>("Sequencer");	
		
		if (SequencerModulePtr)
		{
			SequencerModulePtr->UnRegisterTrackEditor(TrackEditorBindingHandle);
		}
	}

	FDelegateHandle TrackEditorBindingHandle;
};


IMPLEMENT_MODULE(FMediaCompositingEditorModule, MediaCompositingEditor);


#undef LOCTEXT_NAMESPACE
