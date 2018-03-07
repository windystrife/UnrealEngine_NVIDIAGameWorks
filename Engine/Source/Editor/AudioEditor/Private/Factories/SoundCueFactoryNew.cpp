// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Factories/SoundCueFactoryNew.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "Sound/SoundNodeAttenuation.h"
#include "Sound/SoundNodeModulator.h"
#include "Sound/SoundNodeDialoguePlayer.h"
#include "SoundCueGraph/SoundCueGraphNode.h"
#include "Sound/SoundCue.h"
#include "Sound/DialogueWave.h"

USoundCueFactoryNew::USoundCueFactoryNew(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

	SupportedClass = USoundCue::StaticClass();

	bCreateNew = true;
	bEditorImport = false;
	bEditAfterNew = true;
}

UObject* USoundCueFactoryNew::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	USoundCue* SoundCue = NewObject<USoundCue>(InParent, Name, Flags);

	if (InitialSoundWave)
	{
		USoundNodeWavePlayer* WavePlayer = SoundCue->ConstructSoundNode<USoundNodeWavePlayer>();
		SoundCue->FirstNode = WavePlayer;
		SoundCue->LinkGraphNodesFromSoundNodes();
		WavePlayer->SetSoundWave(InitialSoundWave);
		WavePlayer->GraphNode->NodePosX = -250;
		WavePlayer->GraphNode->NodePosY = -35;
	}
	else if (InitialDialogueWave)
	{
		USoundNodeDialoguePlayer* DialoguePlayer = SoundCue->ConstructSoundNode<USoundNodeDialoguePlayer>();
		SoundCue->FirstNode = DialoguePlayer;
		SoundCue->LinkGraphNodesFromSoundNodes();
		DialoguePlayer->SetDialogueWave(InitialDialogueWave);
		DialoguePlayer->GraphNode->NodePosX = -250 - CastChecked<USoundCueGraphNode>(DialoguePlayer->GetGraphNode())->EstimateNodeWidth();
		DialoguePlayer->GraphNode->NodePosY = -35;

		if (InitialDialogueWave->ContextMappings.Num() == 1)
		{
			DialoguePlayer->DialogueWaveParameter.Context.Speaker = InitialDialogueWave->ContextMappings[0].Context.Speaker;
			DialoguePlayer->DialogueWaveParameter.Context.Targets = InitialDialogueWave->ContextMappings[0].Context.Targets;
		}
	}

	return SoundCue;
}



