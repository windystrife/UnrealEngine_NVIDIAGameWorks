// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "Sound/SoundNodeQualityLevel.h"
#include "EngineGlobals.h"
#include "ActiveSound.h"
#include "Sound/AudioSettings.h"
#include "Sound/SoundCue.h"
#include "GameFramework/GameUserSettings.h"
#include "Engine/Engine.h"

#if WITH_EDITORONLY_DATA
#include "Settings/LevelEditorPlaySettings.h"
#include "Editor.h"
#endif

#if WITH_EDITOR
void USoundNodeQualityLevel::PostLoad()
{
	Super::PostLoad();

	ReconcileNode(false);
}

void USoundNodeQualityLevel::ReconcileNode(bool bReconstructNode)
{
	while (ChildNodes.Num() > GetMinChildNodes())
	{
		RemoveChildNode(ChildNodes.Num()-1);
	}
	while (ChildNodes.Num() < GetMinChildNodes())
	{
		InsertChildNode(ChildNodes.Num());
	}
#if WITH_EDITORONLY_DATA
	if (GIsEditor && bReconstructNode && GraphNode)
	{
		GraphNode->ReconstructNode();
		GraphNode->GetGraph()->NotifyGraphChanged();
	}
#endif
}

FText USoundNodeQualityLevel::GetInputPinName(int32 PinIndex) const
{
	return GetDefault<UAudioSettings>()->GetQualityLevelSettings(PinIndex).DisplayName;
}
#endif

int32 USoundNodeQualityLevel::GetMaxChildNodes() const
{
	return GetDefault<UAudioSettings>()->QualityLevels.Num();
}

int32 USoundNodeQualityLevel::GetMinChildNodes() const
{
	return GetDefault<UAudioSettings>()->QualityLevels.Num();
}

void USoundNodeQualityLevel::ParseNodes( FAudioDevice* AudioDevice, const UPTRINT NodeWaveInstanceHash, FActiveSound& ActiveSound, const FSoundParseParameters& ParseParams, TArray<FWaveInstance*>& WaveInstances )
{
#if WITH_EDITOR
	int32 QualityLevel = 0;

	if (GIsEditor)
	{
		RETRIEVE_SOUNDNODE_PAYLOAD( sizeof( int32 ) );
		DECLARE_SOUNDNODE_ELEMENT( int32, CachedQualityLevel );

		if (*RequiresInitialization)
		{
			const bool bIsPIESound = ((GEditor->bIsSimulatingInEditor || GEditor->PlayWorld != nullptr) && ActiveSound.GetWorldID() > 0);
			if (bIsPIESound)
			{
				CachedQualityLevel = GetDefault<ULevelEditorPlaySettings>()->PlayInEditorSoundQualityLevel;
			}
		}

		QualityLevel = CachedQualityLevel;
	}
	else
	{
		QualityLevel = USoundCue::GetCachedQualityLevel();
	}
#else
	const int32 QualityLevel = USoundCue::GetCachedQualityLevel();
#endif

	if (ChildNodes.IsValidIndex(QualityLevel) && ChildNodes[QualityLevel])
	{
		ChildNodes[QualityLevel]->ParseNodes( AudioDevice, GetNodeWaveInstanceHash(NodeWaveInstanceHash, ChildNodes[QualityLevel], QualityLevel), ActiveSound, ParseParams, WaveInstances );
	}
}
