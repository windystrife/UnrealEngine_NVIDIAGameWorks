// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "Sound/SoundNodeSwitch.h"
#include "ActiveSound.h"
#include "Sound/SoundCue.h"

#define LOCTEXT_NAMESPACE "SoundNodeSwitch"

USoundNodeSwitch::USoundNodeSwitch(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USoundNodeSwitch::ParseNodes( FAudioDevice* AudioDevice, const UPTRINT NodeWaveInstanceHash, FActiveSound& ActiveSound, const FSoundParseParameters& ParseParams, TArray<FWaveInstance*>& WaveInstances )
{
	int32 ChildNodeIndex = 0;

	if (ActiveSound.GetIntParameter( IntParameterName, ChildNodeIndex ))
	{
		ChildNodeIndex += 1;
	}
	
	if (ChildNodeIndex < 0 || ChildNodeIndex >= ChildNodes.Num())
	{
		ChildNodeIndex = 0;
	}

	if (ChildNodeIndex < ChildNodes.Num() && ChildNodes[ChildNodeIndex])
	{
		ChildNodes[ChildNodeIndex]->ParseNodes(AudioDevice, GetNodeWaveInstanceHash(NodeWaveInstanceHash, ChildNodes[ChildNodeIndex], ChildNodeIndex), ActiveSound, ParseParams, WaveInstances);
	}
}

void USoundNodeSwitch::CreateStartingConnectors()
{
	InsertChildNode( ChildNodes.Num() );
	InsertChildNode( ChildNodes.Num() );
	InsertChildNode( ChildNodes.Num() );
	InsertChildNode( ChildNodes.Num() );
}

#if WITH_EDITOR
void USoundNodeSwitch::RenamePins()
{
#if WITH_EDITORONLY_DATA
	USoundCue::GetSoundCueAudioEditor()->RenameNodePins(this);
#endif
}

FText USoundNodeSwitch::GetInputPinName(int32 PinIndex) const
{
	if (PinIndex == 0)
	{
		return LOCTEXT("ParamUnset", "Parameter Unset");
	}
	else
	{
		return FText::FromString(FString::Printf(TEXT("%d"), PinIndex - 1));
	}
}

FText USoundNodeSwitch::GetTitle() const
{
	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("Description"), Super::GetTitle());
	Arguments.Add(TEXT("ParameterName"), FText::FromName(IntParameterName));

	return FText::Format(LOCTEXT("Title", "{Description} ({ParameterName})"), Arguments);
}
#endif //WITH_EDITOR

#undef LOCTEXT_NAMESPACE
