// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MaterialEditorActions.h"
#include "Misc/ConfigCacheIni.h"
#include "Materials/MaterialExpression.h"
#include "MaterialGraph/MaterialGraphSchema.h"
#include "Materials/MaterialExpressionComment.h"

#define LOCTEXT_NAMESPACE "MaterialEditorCommands"

void FMaterialEditorCommands::RegisterCommands()
{
	UI_COMMAND( Apply, "Apply", "Apply changes to original material and its use in the world.", EUserInterfaceActionType::Button, FInputChord(EKeys::Enter) );
	UI_COMMAND( Flatten, "Flatten", "Flatten the material to a texture for mobile devices.", EUserInterfaceActionType::Button, FInputChord() );

	UI_COMMAND( ShowAllMaterialParameters, "Params", "Show or Hide all the materials parameters", EUserInterfaceActionType::ToggleButton, FInputChord() );

	UI_COMMAND( SetCylinderPreview, "Cylinder", "Sets the preview mesh to a cylinder primitive.", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( SetSpherePreview, "Sphere", "Sets the preview mesh to a sphere primitive.", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( SetPlanePreview, "Plane", "Sets the preview mesh to a plane primitive.", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( SetCubePreview, "Cube", "Sets the preview mesh to a cube primitive.", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( SetPreviewMeshFromSelection, "Mesh", "Sets the preview mesh based on the current content browser selection.", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( TogglePreviewGrid, "Grid", "Toggles the preview pane's grid.", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( TogglePreviewBackground, "Background", "Toggles the preview pane's background.", EUserInterfaceActionType::ToggleButton, FInputChord() );

	UI_COMMAND( CameraHome, "Home", "Goes home on the canvas.", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( CleanUnusedExpressions, "Clean Up", "Cleans up any unused Expressions.", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( ShowHideConnectors, "Connectors", "Show or Hide Unused Connectors", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND( ToggleLivePreview, "Live Preview", "Toggles real time update of the preview material.", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND( ToggleRealtimeExpressions, "Live Nodes", "Toggles real time update of the graph canvas.", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( AlwaysRefreshAllPreviews, "Live Update", "All nodes are previewed live.", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( ToggleMaterialStats, "Stats", "Toggles displaying of the material's stats.", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND( ToggleMobileStats, "Mobile Stats", "Toggles material stats and compilation errors for mobile.", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( NewComment, "New Comment", "Creates a new comment node.", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( MatertialPasteHere, "Paste Here", "Pastes copied items at this location.", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( UseCurrentTexture, "Use Current Texture", "Uses the current texture selected in the content browser.", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( ConvertObjects, "Convert to Parameter", "Converts the objects to parameters.", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( ConvertToConstant, "Convert to Constant", "Converts the parameters to constants.", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( ConvertToTextureObjects, "Convert to Texture Object", "Converts the objects to texture objects.", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( ConvertToTextureSamples, "Convert to Texture Sample", "Converts the objects to texture samples.", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( StopPreviewNode, "Stop Previewing Node", "Stops the preview viewport from previewing this node", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( StartPreviewNode, "Start Previewing Node", "Makes the preview viewport start previewing this node", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( EnableRealtimePreviewNode, "Enable Realtime Preview", "Enables realtime previewing of this expression node", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( DisableRealtimePreviewNode, "Disable Realtime Preview", "Disables realtime previewing of this expression node", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( BreakAllLinks, "Break All Links", "Breaks all links leading out of this node.", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( DuplicateObjects, "Duplicate Object(s)", "Duplicates the selected objects.", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( DeleteObjects, "Delete Object(s)", "Deletes the selected objects.", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( SelectDownstreamNodes, "Select Downstream Nodes", "Selects all nodes that use this node's outgoing links.", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( SelectUpstreamNodes, "Select Upstream Nodes", "Selects all nodes that feed links into this node.", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( RemoveFromFavorites, "Remove From Favorites", "Removes this expression from your favorites.", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( AddToFavorites, "Add To Favorites", "Adds this expression to your favorites.", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( BreakLink, "Break Link", "Deletes this link.", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( ForceRefreshPreviews, "Force Refresh Previews", "Forces a refresh of all previews", EUserInterfaceActionType::Button, FInputChord(EKeys::SpaceBar) );
	UI_COMMAND( CreateComponentMaskNode, "Create ComponentMask Node", "Creates a ComponentMask node at the current cursor position.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Shift, EKeys::C));
	UI_COMMAND( FindInMaterial, "Search", "Finds expressions and comments in the current Material", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::F));
	UI_COMMAND( PromoteToParameter, "Promote to Parameter", "Promote selected Pin to parameter of pin type", EUserInterfaceActionType::Button, FInputChord());	
}

//////////////////////////////////////////////////////////////////////////
// FExpressionSpawnInfo

TSharedPtr< FEdGraphSchemaAction > FExpressionSpawnInfo::GetAction(UEdGraph* InDestGraph)
{
	if (MaterialExpressionClass == UMaterialExpressionComment::StaticClass())
	{
		return MakeShareable(new FMaterialGraphSchemaAction_NewComment);
	}
	else
	{
		TSharedPtr<FMaterialGraphSchemaAction_NewNode> NewNodeAction(new FMaterialGraphSchemaAction_NewNode);
		NewNodeAction->MaterialExpressionClass = MaterialExpressionClass;
		return NewNodeAction;
	}
}

//////////////////////////////////////////////////////////////////////////
// FMaterialEditorSpawnNodeCommands

void FMaterialEditorSpawnNodeCommands::RegisterCommands()
{
	const FString ConfigSection = TEXT("MaterialEditorSpawnNodes");
	const FString SettingName = TEXT("Node");
	TArray< FString > NodeSpawns;
	GConfig->GetArray(*ConfigSection, *SettingName, NodeSpawns, GEditorPerProjectIni);

	for(int32 x = 0; x < NodeSpawns.Num(); ++x)
	{
		FString ClassName;
		if(!FParse::Value(*NodeSpawns[x], TEXT("Class="), ClassName))
		{
			// Could not find a class name, cannot continue with this line
			continue;
		}

		FString CommandLabel;
		UClass* FoundClass = FindObject<UClass>(ANY_PACKAGE, *ClassName, true);
		TSharedPtr< FExpressionSpawnInfo > InfoPtr;

		if(FoundClass && FoundClass->IsChildOf(UMaterialExpression::StaticClass()))
		{
			// The class name matches that of a UMaterialExpression, so setup a spawn info that can generate one
			CommandLabel = FoundClass->GetName();

			InfoPtr = MakeShareable(new FExpressionSpawnInfo(FoundClass));
		}

		// If spawn info was created, setup a UI Command for keybinding.
		if(InfoPtr.IsValid())
		{
			TSharedPtr< FUICommandInfo > CommandInfo;

			FKey Key;
			bool bShift = false;
			bool bCtrl = false;
			bool bAlt = false;

			// Parse the keybinding information
			FString KeyString;
			if( FParse::Value(*NodeSpawns[x], TEXT("Key="), KeyString) )
			{
				Key = *KeyString;
			}

			if( Key.IsValid() )
			{
				FParse::Bool(*NodeSpawns[x], TEXT("Shift="), bShift);
				FParse::Bool(*NodeSpawns[x], TEXT("Alt="), bAlt);
				FParse::Bool(*NodeSpawns[x], TEXT("Ctrl="), bCtrl);
			}

			FInputChord Chord(Key, EModifierKey::FromBools(bCtrl, bAlt, bShift, false));

			const FText CommandLabelText = FText::FromString( CommandLabel );
			const FText Description = FText::Format( NSLOCTEXT("MaterialEditor", "NodeSpawnDescription", "Hold down the bound keys and left click in the graph panel to spawn a {0} node."), CommandLabelText);

			FUICommandInfo::MakeCommandInfo( this->AsShared(), CommandInfo, FName(*NodeSpawns[x]), CommandLabelText, Description, FSlateIcon(FEditorStyle::GetStyleSetName(), *FString::Printf(TEXT("%s.%s"), *this->GetContextName().ToString(), *NodeSpawns[x])), EUserInterfaceActionType::Button, Chord );

			InfoPtr->CommandInfo = CommandInfo;

			NodeCommands.Add(InfoPtr);
		}
	}
}

TSharedPtr< FEdGraphSchemaAction > FMaterialEditorSpawnNodeCommands::GetGraphActionByChord(FInputChord& InChord, UEdGraph* InDestGraph) const
{
	if(InChord.IsValidChord())
	{
		for(int32 x = 0; x < NodeCommands.Num(); ++x)
		{
			if (NodeCommands[x]->CommandInfo->HasActiveChord(InChord))
			{
				return NodeCommands[x]->GetAction(InDestGraph);
			}
		}
	}

	return TSharedPtr< FEdGraphSchemaAction >();
}

const TSharedPtr<const FInputChord> FMaterialEditorSpawnNodeCommands::GetChordByClass(UClass* MaterialExpressionClass) const
{
	for(int32 Index = 0; Index < NodeCommands.Num(); ++Index)
	{
		if (NodeCommands[Index]->GetClass() == MaterialExpressionClass && NodeCommands[Index]->CommandInfo->GetFirstValidChord()->IsValidChord())
		{
			// Just return the first valid chord
			return NodeCommands[Index]->CommandInfo->GetFirstValidChord();
		}
	}

	return TSharedPtr< const FInputChord >();
}

#undef LOCTEXT_NAMESPACE
