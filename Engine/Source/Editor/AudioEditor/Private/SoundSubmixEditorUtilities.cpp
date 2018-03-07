// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SoundSubmixEditorUtilities.h"
#include "AudioEditorModule.h"
#include "SoundSubmixEditorUtilities.h"
#include "Toolkits/ToolkitManager.h"
#include "Classes/SoundSubmixGraph/SoundSubmixGraph.h"
#include "Sound/SoundSubmix.h"

void FSoundSubmixEditorUtilities::CreateSoundSubmix(const UEdGraph* Graph, UEdGraphPin* FromPin, const FVector2D Location, const FString& Name)
{
	check(Graph);

	// Cast outer to SoundSubmix
	USoundSubmix* SoundSubmix = CastChecked<USoundSubmix>(Graph->GetOuter());

	if (SoundSubmix != nullptr)
	{
		TSharedPtr<ISoundSubmixEditor> SoundSubmixEditor;
		TSharedPtr< IToolkit > FoundAssetEditor = FToolkitManager::Get().FindEditorForAsset(SoundSubmix);
		if (FoundAssetEditor.IsValid())
		{
			SoundSubmixEditor = StaticCastSharedPtr<ISoundSubmixEditor>(FoundAssetEditor);
			SoundSubmixEditor->CreateSoundSubmix(FromPin, Location, Name);
		}
	}
}