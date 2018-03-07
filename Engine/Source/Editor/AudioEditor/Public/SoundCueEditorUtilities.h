// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class ISoundCueEditor;
class UEdGraph;
class USoundCue;
struct Rect;

//////////////////////////////////////////////////////////////////////////
// FSoundCueEditorUtilities

class AUDIOEDITOR_API FSoundCueEditorUtilities
{
public:

	/** Can we paste to this graph? */
	static bool CanPasteNodes(const class UEdGraph* Graph);

	/** Perform paste on graph, at location */
	static void  PasteNodesHere(class UEdGraph* Graph, const FVector2D& Location);

	/** Adds SoundNodeWavePlayer nodes based on selected objects
	 *
	 * @param SelectedWaves List of selected SoundWaves to create player nodes for
	 * @param SoundCue The SoundCue that the nodes will be part of
	 * @param OutPlayers Stores all created nodes
	 * @param Location Position of first created node
	 */
	static void CreateWaveContainers(TArray<class USoundWave*>& SelectedWaves, class USoundCue* SoundCue, TArray<class USoundNode*>& OutPlayers, FVector2D Location);

	/** Adds USoundNodeDialoguePlayer nodes based on selected objects
	 *
	 * @param SelectedDialogues List of selected DialogueWaves to create player nodes for
	 * @param SoundCue The SoundCue that the nodes will be part of
	 * @param OutPlayers Stores all created nodes
	 * @param Location Position of first created node
	 */
	static void CreateDialogueContainers(TArray<class UDialogueWave*>& SelectedDialogues, USoundCue* SoundCue, TArray<class USoundNode*>& OutPlayers, FVector2D Location);


	/** Get the bounding area for the currently selected nodes
	 *
	 * @param Graph The Graph we are finding bounds for
	 * @param Rect Final output bounding area, including padding
	 * @param Padding An amount of padding to add to all sides of the bounds
	 *
	 * @return false if nothing is selected*/
	static bool GetBoundsForSelectedNodes(const UEdGraph* Graph, class FSlateRect& Rect, float Padding = 0.0f);

	/** Gets the number of nodes that are currently selected */
	static int32 GetNumberOfSelectedNodes(const UEdGraph* Graph);

	/** Get the currently selected set of nodes */
	static TSet<UObject*> GetSelectedNodes(const UEdGraph* Graph);

private:
	/** Get ISoundCueEditor for given object, if it exists */
	static TSharedPtr<class ISoundCueEditor> GetISoundCueEditorForObject(const UObject* ObjectToFocusOn);

	FSoundCueEditorUtilities() {}
};
