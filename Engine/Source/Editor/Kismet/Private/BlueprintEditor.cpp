// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BlueprintEditor.h"
#include "Widgets/Text/STextBlock.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/UserDefinedEnum.h"
#include "Engine/UserDefinedStruct.h"
#include "Logging/TokenizedMessage.h"
#include "Misc/PackageName.h"
#include "AssetData.h"
#include "Editor/EditorEngine.h"
#include "Widgets/Layout/SBorder.h"
#include "HAL/FileManager.h"
#include "Misc/FeedbackContext.h"
#include "UObject/MetaData.h"
#include "EdGraph/EdGraph.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "EditorStyleSet.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/SListView.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphNode_Comment.h"
#include "Editor/UnrealEdEngine.h"
#include "Settings/EditorExperimentalSettings.h"
#include "GeneralProjectSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Components/TimelineComponent.h"
#include "Engine/LevelStreamingKismet.h"
#include "Dialogs/Dialogs.h"
#include "UnrealEdGlobals.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "WorkflowOrientedApp/WorkflowUObjectDocuments.h"
#include "EdGraphSchema_K2.h"
#include "K2Node.h"
#include "EdGraphSchema_K2_Actions.h"
#include "K2Node_Event.h"
#include "K2Node_ActorBoundEvent.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Variable.h"
#include "K2Node_CallFunctionOnMember.h"
#include "K2Node_CallParentFunction.h"
#include "K2Node_Tunnel.h"
#include "K2Node_Composite.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_Literal.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_Select.h"
#include "K2Node_Switch.h"
#include "K2Node_SwitchInteger.h"
#include "K2Node_SwitchName.h"
#include "K2Node_Timeline.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_SetFieldsInStruct.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Engine/Breakpoint.h"
#include "ScopedTransaction.h"
#include "Kismet2/KismetDebugUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Editor/KismetCompiler/Public/KismetCompilerModule.h"
#include "EngineUtils.h"
#include "EdGraphToken.h"
#include "Kismet2/CompilerResultsLog.h"
#include "EdGraphUtilities.h"
#include "IMessageLogListing.h"
#include "Developer/MessageLog/Public/MessageLogModule.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"
#include "BlueprintEditorCommands.h"
#include "GraphEditorActions.h"
#include "SNodePanel.h"
#include "Widgets/Docking/SDockTab.h"
#include "EditorClassUtils.h"
#include "IDocumentation.h"
#include "BlueprintNodeBinder.h"
#include "BlueprintFunctionNodeSpawner.h"
#include "SBlueprintEditorToolbar.h"
#include "FindInBlueprints.h"
#include "SGraphTitleBar.h"
#include "Kismet2/Kismet2NameValidators.h"
#include "Kismet2/DebuggerCommands.h"
#include "Editor.h"
#include "IDetailsView.h"
#include "HAL/PlatformApplicationMisc.h"

#include "BlueprintEditorTabs.h"


#include "Interfaces/IProjectManager.h"

// Core kismet tabs
#include "SGraphNode.h"
#include "SSCSEditor.h"
#include "SSCSEditorViewport.h"
#include "SKismetInspector.h"
#include "SBlueprintPalette.h"
#include "SBlueprintActionMenu.h"
#include "SMyBlueprint.h"
#include "SReplaceNodeReferences.h"
// End of core kismet tabs

// Debugging
#include "Debugging/SKismetDebuggingView.h"
#include "Debugging/KismetDebugCommands.h"
// End of debugging

// Misc diagnostics
#include "ObjectTools.h"
// End of misc diagnostics

#include "AssetRegistryModule.h"
#include "BlueprintEditorTabFactories.h"
#include "SPinTypeSelector.h"
#include "Animation/AnimBlueprint.h"
#include "AnimStateConduitNode.h"
#include "AnimationGraphSchema.h"
#include "AnimationGraph.h"
#include "AnimationStateGraph.h"
#include "AnimationStateMachineSchema.h"
#include "AnimationTransitionGraph.h"
#include "BlueprintEditorModes.h"
#include "BlueprintEditorSettings.h"
#include "K2Node_SwitchString.h"

#include "EngineAnalytics.h"
#include "AnalyticsEventAttribute.h"
#include "Interfaces/IAnalyticsProvider.h"
#include "SourceCodeNavigation.h"
#include "IHotReload.h"

#include "AudioDevice.h"

// Blueprint merging
#include "Widgets/Input/SHyperlink.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "NativeCodeGenerationTool.h"

#define LOCTEXT_NAMESPACE "BlueprintEditor"

/////////////////////////////////////////////////////
// FSelectionDetailsSummoner

FSelectionDetailsSummoner::FSelectionDetailsSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp)
	: FWorkflowTabFactory(FBlueprintEditorTabs::DetailsID, InHostingApp)
{
	TabLabel = LOCTEXT("DetailsView_TabTitle", "Details");
	TabIcon = FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Details");

	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("DetailsView_MenuTitle", "Details");
	ViewMenuTooltip = LOCTEXT("DetailsView_ToolTip", "Shows the details view");
}

TSharedRef<SWidget> FSelectionDetailsSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	TSharedPtr<FBlueprintEditor> BlueprintEditorPtr = StaticCastSharedPtr<FBlueprintEditor>(HostingApp.Pin());

	return BlueprintEditorPtr->GetInspector();
}

TSharedRef<SDockTab> FSelectionDetailsSummoner::SpawnTab(const FWorkflowTabSpawnInfo& Info) const
{
	TSharedRef<SDockTab> Tab = FWorkflowTabFactory::SpawnTab(Info);

	TSharedPtr<FBlueprintEditor> BlueprintEditorPtr = StaticCastSharedPtr<FBlueprintEditor>(HostingApp.Pin());
	BlueprintEditorPtr->GetInspector()->SetOwnerTab(Tab);

	BlueprintEditorPtr->GetInspector()->GetPropertyView()->SetHostTabManager(Info.TabManager);

	return Tab;
}

/////////////////////////////////////////////////////
// SChildGraphPicker

class SChildGraphPicker : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SChildGraphPicker) {}
	SLATE_END_ARGS()
public:
	void Construct(const FArguments& InArgs, UEdGraph* ParentGraph)
	{
		this->ChildSlot
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("Menu.Background"))
			.Padding(5)
			.ToolTipText(LOCTEXT("ChildGraphPickerTooltip", "Pick the graph to enter"))
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ChildGraphPickerDesc", "Navigate to graph"))
					.Font( FEditorStyle::GetFontStyle(TEXT("Kismet.GraphPicker.Title.Font")) )
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SListView<UEdGraph*>)
					.ItemHeight(20)
					.ListItemsSource(&(ParentGraph->SubGraphs))
					.OnGenerateRow( this, &SChildGraphPicker::GenerateMenuItemRow )
				]
			]
		];	
	}
private:
	/** Generate a row for the InItem in the combo box's list (passed in as OwnerTable). Do this by calling the user-specified OnGenerateWidget */
	TSharedRef<ITableRow> GenerateMenuItemRow(UEdGraph* InItem, const TSharedRef<STableViewBase>& OwnerTable)
	{
		check(InItem != NULL);

		const FText DisplayName = FLocalKismetCallbacks::GetGraphDisplayName(InItem);

		return SNew( STableRow<UEdGraph*>, OwnerTable )
			[
				SNew(SHyperlink)
					.Text(DisplayName)
					.Style(FEditorStyle::Get(), "HoverOnlyHyperlink")
					.OnNavigate(this, &SChildGraphPicker::NavigateToGraph, InItem)
			]
		;
	}

	void NavigateToGraph(UEdGraph* ChildGraph)
	{
		FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(ChildGraph);

		FSlateApplication::Get().DismissAllMenus();
	}
};

/////////////////////////////////////////////////////
// FBlueprintEditor

namespace BlueprintEditorImpl
{
	static const float InstructionFadeDuration = 0.5f;

	/**
	 * Utility function that will check to see if the specified graph has any 
	 * nodes other than those that come default, pre-placed, in the graph.
	 *
	 * @param  InGraph  The graph to check.
	 * @return True if the graph has any nodes added by the user, otherwise false.
	 */
	static bool GraphHasUserPlacedNodes(UEdGraph const* InGraph);

	/**
	 * Utility function that will check to see if the specified graph has any
	 * nodes that were default, pre-placed, in the graph.
	 *
	 * @param  InGraph  The graph to check.
	 * @return True if the graph has any pre-placed nodes, otherwise false.
	 */
	static bool GraphHasDefaultNode(UEdGraph const* InGraph);

	/**
	 * Utility function that will set the global save-on-compile setting to the
	 * specified value.
	 * 
	 * @param  NewSetting	The new save-on-compile setting that you want applied.
	 */
	static void SetSaveOnCompileSetting(ESaveOnCompile NewSetting);

	/**
	 * Utility function used to determine what save-on-compile setting should be 
	 * presented to the user.
	 * 
	 * @param  Editor	The editor currently querying for the setting value.
	 * @param  Option	The setting to check for.
	 * @return False if the option isn't set, or if the save-on-compile is disabled for the blueprint being edited (otherwise true). 
	 */
	static bool IsSaveOnCompileOptionSet(TWeakPtr<FBlueprintEditor> Editor, ESaveOnCompile Option);

	/**  Flips the value of the editor's "JumpToNodeErrors" setting. */
	static void ToggleJumpToErrorNodeSetting();

	/**
	 * Utility function that will check to see if the "Jump to Error Nodes" 
	 * setting is enabled.
	 * 
	 * @return True if UBlueprintEditorSettings::bJumpToNodeErrors is set, otherwise false.
	 */
	static bool IsJumpToErrorNodeOptionSet();

	/**
	 * Searches through a blueprint, looking for the most severe error'ing node.
	 * 
	 * @param  Blueprint	The blueprint to search through.
	 * @param  Severity		Defines the severity of the error/warning to search for.
	 * @return The first node found with the specified error.
	 */
	static UEdGraphNode* FindNodeWithError(UBlueprint* Blueprint, EMessageSeverity::Type Severity = EMessageSeverity::Error);

	/**
	 * Searches through an error log, looking for the most severe error'ing node.
	 * 
	 * @param  ErrorLog		The error log you want to search through.
	 * @param  Severity		Defines the severity of the error/warning to search for.
	 * @return The first node found with the specified error.
	 */
	static UEdGraphNode* FindNodeWithError(FCompilerResultsLog const& ErrorLog, EMessageSeverity::Type Severity = EMessageSeverity::Error);
}

static bool BlueprintEditorImpl::GraphHasUserPlacedNodes(UEdGraph const* InGraph)
{
	bool bHasUserPlacedNodes = false;

	for (UEdGraphNode const* Node : InGraph->Nodes)
	{
		if (Node == nullptr)
		{
			continue;
		}

		if (!Node->GetOutermost()->GetMetaData()->HasValue(Node, FNodeMetadata::DefaultGraphNode))
		{
			bHasUserPlacedNodes = true;
			break;
		}
	}

	return bHasUserPlacedNodes;
}

static bool BlueprintEditorImpl::GraphHasDefaultNode(UEdGraph const* InGraph)
{
	bool bHasDefaultNodes = false;

	for (UEdGraphNode const* Node : InGraph->Nodes)
	{
		if (Node == nullptr)
		{
			continue;
		}

		if (Node->GetOutermost()->GetMetaData()->HasValue(Node, FNodeMetadata::DefaultGraphNode) && Node->IsNodeEnabled())
		{
			bHasDefaultNodes = true;
			break;
		}
	}

	return bHasDefaultNodes;
}

static void BlueprintEditorImpl::SetSaveOnCompileSetting(ESaveOnCompile NewSetting)
{
	UBlueprintEditorSettings* Settings = GetMutableDefault<UBlueprintEditorSettings>();
	Settings->SaveOnCompile = NewSetting;
	Settings->SaveConfig();
}

static bool BlueprintEditorImpl::IsSaveOnCompileOptionSet(TWeakPtr<FBlueprintEditor> Editor, ESaveOnCompile Option)
{
	const UBlueprintEditorSettings* Settings = GetDefault<UBlueprintEditorSettings>();

	ESaveOnCompile CurrentSetting = Settings->SaveOnCompile;
	if (!Editor.IsValid() || !Editor.Pin()->IsSaveOnCompileEnabled())
	{
		// if save-on-compile is disabled for the blueprint, then we want to 
		// show "Never" as being selected
		// 
		// @TODO: a tooltip explaining why would be nice too
		CurrentSetting = SoC_Never;
	}

	return (CurrentSetting == Option);
}

static void BlueprintEditorImpl::ToggleJumpToErrorNodeSetting()
{
	UBlueprintEditorSettings* Settings = GetMutableDefault<UBlueprintEditorSettings>();
	Settings->bJumpToNodeErrors = !Settings->bJumpToNodeErrors;
	Settings->SaveConfig();
}

static bool BlueprintEditorImpl::IsJumpToErrorNodeOptionSet()
{
	const UBlueprintEditorSettings* Settings = GetDefault<UBlueprintEditorSettings>();
	return Settings->bJumpToNodeErrors;
}

static UEdGraphNode* BlueprintEditorImpl::FindNodeWithError(UBlueprint* Blueprint, EMessageSeverity::Type Severity/* = EMessageSeverity::Error*/)
{
	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);

	UEdGraphNode* ChoiceNode = nullptr;
	for (UEdGraph* Graph : Graphs)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node && Node->bHasCompilerMessage && !Node->ErrorMsg.IsEmpty() && (Node->ErrorType <= Severity))
			{
				if ((ChoiceNode == nullptr) || (ChoiceNode->ErrorType > Node->ErrorType))
				{
					ChoiceNode = Node;
					if (ChoiceNode->ErrorType == 0)
					{
						break;
					}
				}
			}
		}
	}
	return ChoiceNode;
}

static UEdGraphNode* BlueprintEditorImpl::FindNodeWithError(FCompilerResultsLog const& ErrorLog, EMessageSeverity::Type Severity/* = EMessageSeverity::Error*/)
{
	UEdGraphNode* ChoiceNode = nullptr;
	for (TWeakObjectPtr<UEdGraphNode> NodePtr : ErrorLog.AnnotatedNodes)
	{
		UEdGraphNode* Node = NodePtr.Get();
		if ((Node != nullptr) && (Node->ErrorType <= Severity))
		{
			if ((ChoiceNode == nullptr) || (Node->ErrorType < ChoiceNode->ErrorType))
			{
				ChoiceNode = Node;
				if (ChoiceNode->ErrorType == 0)
				{
					break;
				}
			}
		}
	}

	return ChoiceNode;
}


FName FBlueprintEditor::SelectionState_MyBlueprint(TEXT("MyBlueprint"));
FName FBlueprintEditor::SelectionState_Components(TEXT("Components"));
FName FBlueprintEditor::SelectionState_Graph(TEXT("Graph"));
FName FBlueprintEditor::SelectionState_ClassSettings(TEXT("ClassSettings"));
FName FBlueprintEditor::SelectionState_ClassDefaults(TEXT("ClassDefaults"));


bool FBlueprintEditor::IsASubGraph( const UEdGraph* GraphPtr )
{
	if( GraphPtr && GraphPtr->GetOuter() )
	{
		//Check whether the outer is a composite node
		if( GraphPtr->GetOuter()->IsA( UK2Node_Composite::StaticClass() ) )
		{
			return true;
		}
	}
	return false;
}

/** Util for finding a glyph for a graph */
const FSlateBrush* FBlueprintEditor::GetGlyphForGraph(const UEdGraph* Graph, bool bInLargeIcon)
{
	const FSlateBrush* ReturnValue = FEditorStyle::GetBrush( bInLargeIcon ? TEXT("GraphEditor.Function_24x") : TEXT("GraphEditor.Function_16x") );

	check(Graph != NULL);
	const UEdGraphSchema* Schema = Graph->GetSchema();
	if (Schema != NULL)
	{
		const EGraphType GraphType = Schema->GetGraphType(Graph);
		switch (GraphType)
		{
		default:
		case GT_StateMachine:
			{
				ReturnValue = FEditorStyle::GetBrush( bInLargeIcon ? TEXT("GraphEditor.StateMachine_24x") : TEXT("GraphEditor.StateMachine_16x") );
			}
			break;
		case GT_Function:
			{
				if ( Graph->IsA(UAnimationTransitionGraph::StaticClass()) )
				{
					UObject* GraphOuter = Graph->GetOuter();
					if ( GraphOuter != NULL && GraphOuter->IsA(UAnimStateConduitNode::StaticClass()) )
					{
						ReturnValue = FEditorStyle::GetBrush( bInLargeIcon ? TEXT("GraphEditor.Conduit_24x") : TEXT("GraphEditor.Conduit_16x") );
					}
					else
					{
						ReturnValue = FEditorStyle::GetBrush( bInLargeIcon ? TEXT("GraphEditor.Rule_24x") : TEXT("GraphEditor.Rule_16x") );
					}
				}
				else
				{
					//Check for subgraph
					if( IsASubGraph( Graph ) )
					{
						ReturnValue = FEditorStyle::GetBrush( bInLargeIcon ? TEXT("GraphEditor.SubGraph_24x") : TEXT("GraphEditor.SubGraph_16x") );
					}
					else
					{
						ReturnValue = FEditorStyle::GetBrush( bInLargeIcon ? TEXT("GraphEditor.Function_24x") : TEXT("GraphEditor.Function_16x") );
					}
				}
			}
			break;
		case GT_Macro:
			{
				ReturnValue = FEditorStyle::GetBrush( bInLargeIcon ? TEXT("GraphEditor.Macro_24x") : TEXT("GraphEditor.Macro_16x") );
			}
			break;
		case GT_Ubergraph:
			{
				ReturnValue = FEditorStyle::GetBrush( bInLargeIcon ? TEXT("GraphEditor.EventGraph_24x") : TEXT("GraphEditor.EventGraph_16x") );
			}
			break;
		case GT_Animation:
			{
				if ( Graph->IsA(UAnimationStateGraph::StaticClass()) )
				{
					ReturnValue = FEditorStyle::GetBrush( bInLargeIcon ? TEXT("GraphEditor.State_24x") : TEXT("GraphEditor.State_16x") );
				}
				else
				{
					ReturnValue = FEditorStyle::GetBrush( bInLargeIcon ? TEXT("GraphEditor.Animation_24x") : TEXT("GraphEditor.Animation_16x") );
				}
			}
		}
	}

	return ReturnValue;
}

FSlateBrush const* FBlueprintEditor::GetVarIconAndColor(const UStruct* VarScope, FName VarName, FSlateColor& IconColorOut, FSlateBrush const*& SecondaryBrushOut, FSlateColor& SecondaryColorOut)
{
	if (VarScope != NULL)
	{
		UProperty* Property = FindField<UProperty>(VarScope, VarName);
		if (Property != NULL)
		{
			const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

			FEdGraphPinType PinType;
			if (K2Schema->ConvertPropertyToPinType(Property, PinType)) // use schema to get the color
			{
				IconColorOut = K2Schema->GetPinTypeColor(PinType);
				SecondaryBrushOut = FBlueprintEditorUtils::GetSecondaryIconFromPin(PinType);
				SecondaryColorOut = K2Schema->GetSecondaryPinTypeColor(PinType);
				return FBlueprintEditorUtils::GetIconFromPin(PinType);
			}
		}
	}
	return FEditorStyle::GetBrush(TEXT("Kismet.AllClasses.VariableIcon"));
}

bool FBlueprintEditor::IsInAScriptingMode() const
{
	return IsModeCurrent(FBlueprintEditorApplicationModes::StandardBlueprintEditorMode) || IsModeCurrent(FBlueprintEditorApplicationModes::BlueprintMacroMode);
}

bool FBlueprintEditor::OnRequestClose()
{
	// Also close the Find Results tab if we're not in full edit mode and the option to host Global Find Results is enabled.
	TSharedPtr<SDockTab> FindResultsTab = TabManager->FindExistingLiveTab(FBlueprintEditorTabs::FindResultsID);
	if (FindResultsTab.IsValid() && !IsInAScriptingMode() && GetDefault<UBlueprintEditorSettings>()->bHostFindInBlueprintsInGlobalTab)
	{
		FindResultsTab->RequestCloseTab();
	}

	bEditorMarkedAsClosed = true;
	return FWorkflowCentricApplication::OnRequestClose();
}

bool FBlueprintEditor::InEditingMode() const
{
	UBlueprint* Blueprint = GetBlueprintObj();
	return !FSlateApplication::Get().InKismetDebuggingMode() && (!InDebuggingMode() || (Blueprint && Blueprint->CanRecompileWhilePlayingInEditor()));
}

bool FBlueprintEditor::IsCompilingEnabled() const
{
	UBlueprint* Blueprint = GetBlueprintObj();
	return Blueprint && Blueprint->BlueprintType != BPTYPE_MacroLibrary && InEditingMode();
}

bool FBlueprintEditor::InDebuggingMode() const
{
	return GEditor->PlayWorld != NULL;
}

EVisibility FBlueprintEditor::IsDebuggerVisible() const
{
	return InDebuggingMode() ? EVisibility::Visible : EVisibility::Collapsed;
}

int32 FBlueprintEditor::GetNumberOfSelectedNodes() const
{
	return GetSelectedNodes().Num();
}

FGraphPanelSelectionSet FBlueprintEditor::GetSelectedNodes() const
{
	FGraphPanelSelectionSet CurrentSelection;
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (FocusedGraphEd.IsValid())
	{
		CurrentSelection = FocusedGraphEd->GetSelectedNodes();
	}
	return CurrentSelection;
}

UEdGraphNode* FBlueprintEditor::GetSingleSelectedNode() const
{
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	return (SelectedNodes.Num() == 1) ? Cast<UEdGraphNode>(*SelectedNodes.CreateConstIterator()) : nullptr;
}

void FBlueprintEditor::AnalyticsTrackNodeEvent( UBlueprint* Blueprint, UEdGraphNode *GraphNode, bool bNodeDelete ) const
{
	if( Blueprint && GraphNode && FEngineAnalytics::IsAvailable() )
	{
		// we'd like to see if this was happening in normal blueprint editor or persona 
		const FString EditorName = Cast<UAnimBlueprint>(Blueprint) != NULL ? TEXT("Persona") : TEXT("BlueprintEditor");

		// Build Node Details
		const UGeneralProjectSettings& ProjectSettings = *GetDefault<UGeneralProjectSettings>();
		FString ProjectID = ProjectSettings.ProjectID.ToString();
		TArray< FAnalyticsEventAttribute > NodeAttributes;
		NodeAttributes.Add( FAnalyticsEventAttribute( TEXT( "ProjectId" ), ProjectID ) );
		NodeAttributes.Add( FAnalyticsEventAttribute( TEXT( "BlueprintId" ), Blueprint->GetBlueprintGuid().ToString() ) );
		TArray< TKeyValuePair<FString, FString> > Attributes;

		if( UK2Node* K2Node = Cast<UK2Node>( GraphNode ))
		{
			K2Node->GetNodeAttributes( Attributes );
		}
		else if( UEdGraphNode_Comment* CommentNode = Cast<UEdGraphNode_Comment>( GraphNode ))
		{
			Attributes.Add( TKeyValuePair<FString, FString>( TEXT( "Type" ), TEXT( "Comment" ) ));
			Attributes.Add( TKeyValuePair<FString, FString>( TEXT( "Class" ), CommentNode->GetClass()->GetName() ));
			Attributes.Add( TKeyValuePair<FString, FString>( TEXT( "Name" ), CommentNode->GetName() ));
		}
		if( Attributes.Num() > 0 )
		{
			// Build Node Attributes
			for (const TKeyValuePair<FString, FString>& Attribute : Attributes)
			{
				NodeAttributes.Add( FAnalyticsEventAttribute( Attribute.Key, Attribute.Value ));
			}
			// Send Analytics event 
			FString EventType = bNodeDelete ?	FString::Printf(TEXT( "Editor.Usage.%s.NodeDeleted" ), *EditorName) :
												FString::Printf(TEXT( "Editor.Usage.%s.NodeCreated" ), *EditorName);
			FEngineAnalytics::GetProvider().RecordEvent( EventType, NodeAttributes );
		}
	}
}

void FBlueprintEditor::AnalyticsTrackCompileEvent( UBlueprint* Blueprint, int32 NumErrors, int32 NumWarnings ) const
{
	if(Blueprint && FEngineAnalytics::IsAvailable())
	{
		// we'd like to see if this was happening in normal blueprint editor or persona 
		const FString EditorName = Cast<UAnimBlueprint>(Blueprint) != NULL ? TEXT("Persona") : TEXT("BlueprintEditor");

		// Build Node Details
		const UGeneralProjectSettings& ProjectSettings = *GetDefault<UGeneralProjectSettings>();
		FString ProjectID = ProjectSettings.ProjectID.ToString();

		const bool bSuccess = NumErrors == 0;
		TArray< FAnalyticsEventAttribute > Attributes;
		Attributes.Add(FAnalyticsEventAttribute(TEXT("ProjectId"), ProjectID));
		Attributes.Add(FAnalyticsEventAttribute(TEXT("BlueprintId"), Blueprint->GetBlueprintGuid().ToString()));
		Attributes.Add(FAnalyticsEventAttribute(TEXT("Success"), bSuccess? TEXT("True") : TEXT("False") ));
		Attributes.Add(FAnalyticsEventAttribute(TEXT("NumErrors"), FString::FromInt(NumErrors)));
		Attributes.Add(FAnalyticsEventAttribute(TEXT("NumWarnings"), FString::FromInt(NumWarnings)));

		// Send Analytics event 
		FEngineAnalytics::GetProvider().RecordEvent(FString::Printf(TEXT("Editor.Usage.%s.Compile"), *EditorName), Attributes);
	}
}

void FBlueprintEditor::RefreshEditors(ERefreshBlueprintEditorReason::Type Reason)
{
	bool bForceFocusOnSelectedNodes = false;

	if (CurrentUISelection == SelectionState_MyBlueprint)
	{
		// Handled below, here to avoid tripping the ensure
	}
	else if (CurrentUISelection == SelectionState_Components)
	{
		if (SCSEditor.IsValid())
		{
			SCSEditor->RefreshSelectionDetails();
		}
	}
	else if (CurrentUISelection == SelectionState_Graph)
	{
		bForceFocusOnSelectedNodes = true;
	}
	else if (CurrentUISelection == SelectionState_ClassSettings)
	{
		// No need for a refresh, the Blueprint object didn't change
	}
	else if (CurrentUISelection == SelectionState_ClassDefaults)
	{
		StartEditingDefaults(/*bAutoFocus=*/ false, true);
	}

	// Remove any tabs are that are pending kill or otherwise invalid UObject pointers.
	DocumentManager->CleanInvalidTabs();

	//@TODO: Should determine when we need to do the invalid/refresh business and if the graph node selection change
	// under non-compiles is necessary (except when the selection mode is appropriate, as already detected above)
	if (Reason != ERefreshBlueprintEditorReason::BlueprintCompiled)
	{
		DocumentManager->RefreshAllTabs();

		bForceFocusOnSelectedNodes = true;
	}

	if (bForceFocusOnSelectedNodes)
	{
		FocusInspectorOnGraphSelection(GetSelectedNodes(), /*bForceRefresh=*/ true);
	}

	if (ReplaceReferencesWidget.IsValid())
	{
		ReplaceReferencesWidget->Refresh();
	}

	if (MyBlueprintWidget.IsValid())
	{
		MyBlueprintWidget->Refresh();
	}

	if (SCSEditor.IsValid())
	{
		SCSEditor->UpdateTree();
		
		// Note: Don't pass 'true' here because we don't want the preview actor to be reconstructed until after Blueprint modification is complete.
		UpdateSCSPreview();
	}

	// Note: There is an optimization inside of ShowDetailsForSingleObject() that skips the refresh if the object being selected is the same as the previous object.
	// The SKismetInspector class is shared between both Defaults mode and Components mode, but in Defaults mode the object selected is always going to be the CDO. Given
	// that the selection does not really change, we force it to refresh and skip the optimization. Otherwise, some things may not work correctly in Defaults mode. For
	// example, transform details are customized and the rotation value is cached at customization time; if we don't force refresh here, then after an undo of a previous
	// rotation edit, transform details won't be re-customized and thus the cached rotation value will be stale, resulting in an invalid rotation value on the next edit.
	//@TODO: Probably not always necessary
	RefreshStandAloneDefaultsEditor();

	// Update associated controls like the function editor
	BroadcastRefresh();
}

void FBlueprintEditor::SetUISelectionState(FName SelectionOwner)
{
	if ( SelectionOwner != CurrentUISelection )
	{
		ClearSelectionStateFor(CurrentUISelection);

		CurrentUISelection = SelectionOwner;
	}
}

void FBlueprintEditor::AddToSelection(UEdGraphNode* InNode)
{
	FocusedGraphEdPtr.Pin()->SetNodeSelection(InNode, true);
}

void FBlueprintEditor::ClearSelectionStateFor(FName SelectionOwner)
{
	if ( SelectionOwner == SelectionState_Graph )
	{
		TArray< TSharedPtr<SDockTab> > GraphEditorTabs;
		DocumentManager->FindAllTabsForFactory(GraphEditorTabFactoryPtr, /*out*/ GraphEditorTabs);

		for (TSharedPtr<SDockTab>& GraphEditorTab : GraphEditorTabs)
		{
			TSharedRef<SGraphEditor> Editor = StaticCastSharedRef<SGraphEditor>((GraphEditorTab)->GetContent());

			Editor->ClearSelectionSet();
		}
	}
	else if ( SelectionOwner == SelectionState_Components )
	{
		if ( SCSEditor.IsValid() )
		{
			SCSEditor->ClearSelection();
		}
	}
	else if ( SelectionOwner == SelectionState_MyBlueprint )
	{
		if ( MyBlueprintWidget.IsValid() )
		{
			MyBlueprintWidget->ClearGraphActionMenuSelection();
		}
	}
}

void FBlueprintEditor::SummonSearchUI(bool bSetFindWithinBlueprint, FString NewSearchTerms, bool bSelectFirstResult)
{
	TSharedPtr<SFindInBlueprints> FindResultsToUse;

	if (bSetFindWithinBlueprint
		|| !GetDefault<UBlueprintEditorSettings>()->bHostFindInBlueprintsInGlobalTab)
	{
		FindResultsToUse = FindResults;
		TabManager->InvokeTab(FBlueprintEditorTabs::FindResultsID);
	}
	else
	{
		FindResultsToUse = FFindInBlueprintSearchManager::Get().GetGlobalFindResults();
	}

	if (FindResultsToUse.IsValid())
	{
		FindResultsToUse->FocusForUse(bSetFindWithinBlueprint, NewSearchTerms, bSelectFirstResult);
	}
}

void FBlueprintEditor::SummonFindAndReplaceUI()
{
	TabManager->InvokeTab(FBlueprintEditorTabs::ReplaceNodeReferencesID);
}

void FBlueprintEditor::EnableSCSPreview(bool bEnable)
{
	if(SCSViewport.IsValid())
	{
		SCSViewport->EnablePreview(bEnable);
	}
}

void FBlueprintEditor::UpdateSCSPreview(bool bUpdateNow)
{
	// refresh widget
	if(SCSViewport.IsValid())
	{
		TSharedPtr<SDockTab> OwnerTab = Inspector->GetOwnerTab();
		if ( OwnerTab.IsValid() )
		{
			bUpdateNow &= OwnerTab->IsForeground();
		}

		// Only request a refresh immediately if the viewport tab is in the foreground.
		SCSViewport->RequestRefresh(false, bUpdateNow);
	}
}

AActor* FBlueprintEditor::GetSCSEditorActorContext() const
{
	// Return the current CDO that was last generated for the class
	UBlueprint* Blueprint = GetBlueprintObj();
	if(Blueprint != nullptr && Blueprint->GeneratedClass != nullptr)
	{
		return Blueprint->GeneratedClass->GetDefaultObject<AActor>();
	}
	
	return nullptr;
}

void FBlueprintEditor::OnSelectionUpdated(const TArray<FSCSEditorTreeNodePtrType>& SelectedNodes)
{
	if (SCSViewport.IsValid())
	{
		SCSViewport->OnComponentSelectionChanged();
	}

	UBlueprint* Blueprint = GetBlueprintObj();
	check(Blueprint != nullptr && Blueprint->SimpleConstructionScript != nullptr);

	// Update the selection visualization
	AActor* EditorActorInstance = Blueprint->SimpleConstructionScript->GetComponentEditorActorInstance();
	if (EditorActorInstance != nullptr)
	{
		TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents;
		EditorActorInstance->GetComponents(PrimitiveComponents, true);

		for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
		{
			PrimitiveComponent->PushSelectionToProxy();
		}
	}

	if (Inspector.IsValid())
	{
		// Clear the my blueprints selection
		if ( SelectedNodes.Num() > 0 )
		{
			SetUISelectionState(FBlueprintEditor::SelectionState_Components);
		}

		// Convert the selection set to an array of UObject* pointers
		FText InspectorTitle = FText::GetEmpty();
		TArray<UObject*> InspectorObjects;
		bool bShowComponents = true;
		InspectorObjects.Empty(SelectedNodes.Num());
		for (FSCSEditorTreeNodePtrType NodePtr : SelectedNodes)
		{
			if (NodePtr.IsValid())
			{
				if (NodePtr->GetNodeType() == FSCSEditorTreeNode::RootActorNode)
				{
					AActor* DefaultActor = GetSCSEditorActorContext();
					InspectorObjects.Add(DefaultActor);
					
					FString Title; 
					DefaultActor->GetName(Title);
					InspectorTitle = FText::FromString(Title);
					bShowComponents = false;

					TryInvokingDetailsTab();
				}
				else
				{
					UActorComponent* EditableComponent = NodePtr->GetEditableComponentTemplate(GetBlueprintObj());
					if (EditableComponent)
					{
						InspectorTitle = FText::FromString(NodePtr->GetDisplayString());
						InspectorObjects.Add(EditableComponent);
					}

					if ( SCSViewport.IsValid() )
					{
						TSharedPtr<SDockTab> OwnerTab = SCSViewport->GetOwnerTab();
						if ( OwnerTab.IsValid() )
						{
							OwnerTab->FlashTab();
						}
					}
				}
			}
		}

		// Update the details panel
		SKismetInspector::FShowDetailsOptions Options(InspectorTitle, true);
		Options.bShowComponents = bShowComponents;
		Inspector->ShowDetailsForObjects(InspectorObjects, Options);
	}
}

void FBlueprintEditor::OnComponentDoubleClicked(TSharedPtr<class FSCSEditorTreeNode> Node)
{
	TSharedPtr<SDockTab> OwnerTab = Inspector->GetOwnerTab();
	if ( OwnerTab.IsValid() )
	{
		GetTabManager()->InvokeTab(FBlueprintEditorTabs::SCSViewportID);
	}
}

/** Create new tab for the supplied graph - don't call this directly.*/
TSharedRef<SGraphEditor> FBlueprintEditor::CreateGraphEditorWidget(TSharedRef<FTabInfo> InTabInfo, UEdGraph* InGraph)
{
	check((InGraph != NULL) && IsEditingSingleBlueprint());

	// No need to regenerate the commands.
	if(!GraphEditorCommands.IsValid())
	{
		GraphEditorCommands = MakeShareable( new FUICommandList );
		{
			GraphEditorCommands->MapAction(FGraphEditorCommands::Get().PromoteToVariable,
				FExecuteAction::CreateSP( this, &FBlueprintEditor::OnPromoteToVariable, true ),
				FCanExecuteAction::CreateSP( this, &FBlueprintEditor::CanPromoteToVariable, true )
				);

			GraphEditorCommands->MapAction(FGraphEditorCommands::Get().PromoteToLocalVariable,
				FExecuteAction::CreateSP( this, &FBlueprintEditor::OnPromoteToVariable, false ),
				FCanExecuteAction::CreateSP( this, &FBlueprintEditor::CanPromoteToVariable, false )
				);

			GraphEditorCommands->MapAction(FGraphEditorCommands::Get().SplitStructPin,
				FExecuteAction::CreateSP( this, &FBlueprintEditor::OnSplitStructPin ),
				FCanExecuteAction::CreateSP( this, &FBlueprintEditor::CanSplitStructPin )
				);

			GraphEditorCommands->MapAction(FGraphEditorCommands::Get().RecombineStructPin,
				FExecuteAction::CreateSP( this, &FBlueprintEditor::OnRecombineStructPin ),
				FCanExecuteAction::CreateSP( this, &FBlueprintEditor::CanRecombineStructPin )
				);

			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().AddExecutionPin,
				FExecuteAction::CreateSP( this, &FBlueprintEditor::OnAddExecutionPin ),
				FCanExecuteAction::CreateSP( this, &FBlueprintEditor::CanAddExecutionPin )
				);

			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().RemoveExecutionPin,
				FExecuteAction::CreateSP( this, &FBlueprintEditor::OnRemoveExecutionPin ),
				FCanExecuteAction::CreateSP( this, &FBlueprintEditor::CanRemoveExecutionPin )
				);

			GraphEditorCommands->MapAction(FGraphEditorCommands::Get().RemoveThisStructVarPin,
				FExecuteAction::CreateSP(this, &FBlueprintEditor::OnRemoveThisStructVarPin),
				FCanExecuteAction::CreateSP(this, &FBlueprintEditor::CanRemoveThisStructVarPin)
				);

			GraphEditorCommands->MapAction(FGraphEditorCommands::Get().RemoveOtherStructVarPins,
				FExecuteAction::CreateSP(this, &FBlueprintEditor::OnRemoveOtherStructVarPins),
				FCanExecuteAction::CreateSP(this, &FBlueprintEditor::CanRemoveOtherStructVarPins)
				);

			GraphEditorCommands->MapAction(FGraphEditorCommands::Get().RestoreAllStructVarPins,
				FExecuteAction::CreateSP(this, &FBlueprintEditor::OnRestoreAllStructVarPins),
				FCanExecuteAction::CreateSP(this, &FBlueprintEditor::CanRestoreAllStructVarPins)
				);

			GraphEditorCommands->MapAction(FGraphEditorCommands::Get().ResetPinToDefaultValue,
				FExecuteAction::CreateSP(this, &FBlueprintEditor::OnResetPinToDefaultValue),
				FCanExecuteAction::CreateSP(this, &FBlueprintEditor::CanResetPinToDefaultValue)
			);

			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().AddOptionPin,
				FExecuteAction::CreateSP( this, &FBlueprintEditor::OnAddOptionPin ),
				FCanExecuteAction::CreateSP( this, &FBlueprintEditor::CanAddOptionPin )
				);

			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().RemoveOptionPin,
				FExecuteAction::CreateSP( this, &FBlueprintEditor::OnRemoveOptionPin ),
				FCanExecuteAction::CreateSP( this, &FBlueprintEditor::CanRemoveOptionPin )
				);

			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().ChangePinType,
				FExecuteAction::CreateSP( this, &FBlueprintEditor::OnChangePinType ),
				FCanExecuteAction::CreateSP( this, &FBlueprintEditor::CanChangePinType )
				);

			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().AddParentNode,
				FExecuteAction::CreateSP( this, &FBlueprintEditor::OnAddParentNode ),
				FCanExecuteAction::CreateSP( this, &FBlueprintEditor::CanAddParentNode )
				);

			// Debug actions
			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().AddBreakpoint,
				FExecuteAction::CreateSP( this, &FBlueprintEditor::OnAddBreakpoint ),
				FCanExecuteAction::CreateSP( this, &FBlueprintEditor::CanAddBreakpoint ),
				FIsActionChecked(),
				FIsActionButtonVisible::CreateSP( this, &FBlueprintEditor::CanAddBreakpoint )
				);

			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().RemoveBreakpoint,
				FExecuteAction::CreateSP( this, &FBlueprintEditor::OnRemoveBreakpoint ),
				FCanExecuteAction::CreateSP( this, &FBlueprintEditor::CanRemoveBreakpoint ),
				FIsActionChecked(),
				FIsActionButtonVisible::CreateSP( this, &FBlueprintEditor::CanRemoveBreakpoint )
				);

			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().EnableBreakpoint,
				FExecuteAction::CreateSP( this, &FBlueprintEditor::OnEnableBreakpoint ),
				FCanExecuteAction::CreateSP( this, &FBlueprintEditor::CanEnableBreakpoint ),
				FIsActionChecked(),
				FIsActionButtonVisible::CreateSP( this, &FBlueprintEditor::CanEnableBreakpoint )
				);

			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().DisableBreakpoint,
				FExecuteAction::CreateSP( this, &FBlueprintEditor::OnDisableBreakpoint ),
				FCanExecuteAction::CreateSP( this, &FBlueprintEditor::CanDisableBreakpoint ),
				FIsActionChecked(),
				FIsActionButtonVisible::CreateSP( this, &FBlueprintEditor::CanDisableBreakpoint )
				);

			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().ToggleBreakpoint,
				FExecuteAction::CreateSP( this, &FBlueprintEditor::OnToggleBreakpoint ),
				FCanExecuteAction::CreateSP( this, &FBlueprintEditor::CanToggleBreakpoint ),
				FIsActionChecked(),
				FIsActionButtonVisible::CreateSP( this, &FBlueprintEditor::CanToggleBreakpoint )
				);

			// Encapsulation commands
			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().CollapseNodes,
				FExecuteAction::CreateSP( this, &FBlueprintEditor::OnCollapseNodes ),
				FCanExecuteAction::CreateSP( this, &FBlueprintEditor::CanCollapseNodes )
				);

			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().CollapseSelectionToFunction,
				FExecuteAction::CreateSP( this, &FBlueprintEditor::OnCollapseSelectionToFunction ),
				FCanExecuteAction::CreateSP( this, &FBlueprintEditor::CanCollapseSelectionToFunction ),
				FIsActionChecked(),
				FIsActionButtonVisible::CreateSP( this, &FBlueprintEditor::NewDocument_IsVisibleForType, CGT_NewFunctionGraph )
				);

			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().CollapseSelectionToMacro,
				FExecuteAction::CreateSP( this, &FBlueprintEditor::OnCollapseSelectionToMacro ),
				FCanExecuteAction::CreateSP( this, &FBlueprintEditor::CanCollapseSelectionToMacro ),
				FIsActionChecked(),
				FIsActionButtonVisible::CreateSP( this, &FBlueprintEditor::NewDocument_IsVisibleForType, CGT_NewMacroGraph )
				);

			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().PromoteSelectionToFunction,
				FExecuteAction::CreateSP( this, &FBlueprintEditor::OnPromoteSelectionToFunction ),
				FCanExecuteAction::CreateSP( this, &FBlueprintEditor::CanPromoteSelectionToFunction ),
				FIsActionChecked(),
				FIsActionButtonVisible::CreateSP( this, &FBlueprintEditor::NewDocument_IsVisibleForType, CGT_NewFunctionGraph )
				);

			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().PromoteSelectionToMacro,
				FExecuteAction::CreateSP( this, &FBlueprintEditor::OnPromoteSelectionToMacro ),
				FCanExecuteAction::CreateSP( this, &FBlueprintEditor::CanPromoteSelectionToMacro ),
				FIsActionChecked(),
				FIsActionButtonVisible::CreateSP( this, &FBlueprintEditor::NewDocument_IsVisibleForType, CGT_NewMacroGraph )
				);
			
			// Alignment Commands
			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().AlignNodesTop,
				FExecuteAction::CreateSP( this, &FBlueprintEditor::OnAlignTop )
				);

			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().AlignNodesMiddle,
				FExecuteAction::CreateSP( this, &FBlueprintEditor::OnAlignMiddle )
				);

			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().AlignNodesBottom,
				FExecuteAction::CreateSP( this, &FBlueprintEditor::OnAlignBottom )
				);

			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().AlignNodesLeft,
				FExecuteAction::CreateSP( this, &FBlueprintEditor::OnAlignLeft )
				);

			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().AlignNodesCenter,
				FExecuteAction::CreateSP( this, &FBlueprintEditor::OnAlignCenter )
				);

			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().AlignNodesRight,
				FExecuteAction::CreateSP( this, &FBlueprintEditor::OnAlignRight )
				);

			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().StraightenConnections,
				FExecuteAction::CreateSP( this, &FBlueprintEditor::OnStraightenConnections )
				);

			// Distribution Commands
			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().DistributeNodesHorizontally,
				FExecuteAction::CreateSP( this, &FBlueprintEditor::OnDistributeNodesH )
				);

			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().DistributeNodesVertically,
				FExecuteAction::CreateSP( this, &FBlueprintEditor::OnDistributeNodesV )
				);

			GraphEditorCommands->MapAction( FGenericCommands::Get().Rename,
				FExecuteAction::CreateSP( this, &FBlueprintEditor::OnRenameNode ),
				FCanExecuteAction::CreateSP( this, &FBlueprintEditor::CanRenameNodes )
				);

			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().ExpandNodes,
				FExecuteAction::CreateSP( this, &FBlueprintEditor::OnExpandNodes ),
				FCanExecuteAction::CreateSP( this, &FBlueprintEditor::CanExpandNodes ),
				FIsActionChecked(),
				FIsActionButtonVisible::CreateSP( this, &FBlueprintEditor::CanExpandNodes )
				);

			// Editing commands
			GraphEditorCommands->MapAction( FGenericCommands::Get().SelectAll,
				FExecuteAction::CreateSP( this, &FBlueprintEditor::SelectAllNodes ),
				FCanExecuteAction::CreateSP( this, &FBlueprintEditor::CanSelectAllNodes )
				);

			GraphEditorCommands->MapAction( FGenericCommands::Get().Delete,
				FExecuteAction::CreateSP( this, &FBlueprintEditor::DeleteSelectedNodes ),
				FCanExecuteAction::CreateSP( this, &FBlueprintEditor::CanDeleteNodes )
				);

			GraphEditorCommands->MapAction( FGenericCommands::Get().Copy,
				FExecuteAction::CreateSP( this, &FBlueprintEditor::CopySelectedNodes ),
				FCanExecuteAction::CreateSP( this, &FBlueprintEditor::CanCopyNodes )
				);

			GraphEditorCommands->MapAction( FGenericCommands::Get().Cut,
				FExecuteAction::CreateSP( this, &FBlueprintEditor::CutSelectedNodes ),
				FCanExecuteAction::CreateSP( this, &FBlueprintEditor::CanCutNodes )
				);

			GraphEditorCommands->MapAction( FGenericCommands::Get().Paste,
				FExecuteAction::CreateSP( this, &FBlueprintEditor::PasteNodes ),
				FCanExecuteAction::CreateSP( this, &FBlueprintEditor::CanPasteNodes )
				);

			GraphEditorCommands->MapAction( FGenericCommands::Get().Duplicate,
				FExecuteAction::CreateSP( this, &FBlueprintEditor::DuplicateNodes ),
				FCanExecuteAction::CreateSP( this, &FBlueprintEditor::CanDuplicateNodes )
				);

			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().SelectReferenceInLevel,
				FExecuteAction::CreateSP( this, &FBlueprintEditor::OnSelectReferenceInLevel ),
				FCanExecuteAction::CreateSP( this, &FBlueprintEditor::CanSelectReferenceInLevel ),
				FIsActionChecked(),
				FIsActionButtonVisible::CreateSP( this, &FBlueprintEditor::CanSelectReferenceInLevel )
				);

			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().AssignReferencedActor,
				FExecuteAction::CreateSP( this, &FBlueprintEditor::OnAssignReferencedActor ),
				FCanExecuteAction::CreateSP( this, &FBlueprintEditor::CanAssignReferencedActor ) );

			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().StartWatchingPin,
				FExecuteAction::CreateSP( this, &FBlueprintEditor::OnStartWatchingPin ),
				FCanExecuteAction::CreateSP( this, &FBlueprintEditor::CanStartWatchingPin )
				);

			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().StopWatchingPin,
				FExecuteAction::CreateSP( this, &FBlueprintEditor::OnStopWatchingPin ),
				FCanExecuteAction::CreateSP( this, &FBlueprintEditor::CanStopWatchingPin )
				);

			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().SelectBone,
				FExecuteAction::CreateSP( this, &FBlueprintEditor::OnSelectBone ),
				FCanExecuteAction::CreateSP( this, &FBlueprintEditor::CanSelectBone )
				);

			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().AddBlendListPin,
				FExecuteAction::CreateSP( this, &FBlueprintEditor::OnAddPosePin ),
				FCanExecuteAction::CreateSP( this, &FBlueprintEditor::CanAddPosePin )
				);

			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().RemoveBlendListPin,
				FExecuteAction::CreateSP( this, &FBlueprintEditor::OnRemovePosePin ),
				FCanExecuteAction::CreateSP( this, &FBlueprintEditor::CanRemovePosePin )
				);

			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().ConvertToSeqEvaluator,
				FExecuteAction::CreateSP( this, &FBlueprintEditor::OnConvertToSequenceEvaluator )
				);

			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().ConvertToSeqPlayer,
				FExecuteAction::CreateSP( this, &FBlueprintEditor::OnConvertToSequencePlayer )
				);

			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().ConvertToBSEvaluator,
				FExecuteAction::CreateSP( this, &FBlueprintEditor::OnConvertToBlendSpaceEvaluator )
				);

			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().ConvertToBSPlayer,
				FExecuteAction::CreateSP( this, &FBlueprintEditor::OnConvertToBlendSpacePlayer )
				);

			GraphEditorCommands->MapAction(FGraphEditorCommands::Get().ConvertToAimOffsetLookAt,
				FExecuteAction::CreateSP(this, &FBlueprintEditor::OnConvertToAimOffsetLookAt)
			);

			GraphEditorCommands->MapAction(FGraphEditorCommands::Get().ConvertToAimOffsetSimple,
				FExecuteAction::CreateSP(this, &FBlueprintEditor::OnConvertToAimOffsetSimple)
			);

			GraphEditorCommands->MapAction(FGraphEditorCommands::Get().ConvertToPoseBlender,
				FExecuteAction::CreateSP(this, &FBlueprintEditor::OnConvertToPoseBlender)
				);

			GraphEditorCommands->MapAction(FGraphEditorCommands::Get().ConvertToPoseByName,
				FExecuteAction::CreateSP(this, &FBlueprintEditor::OnConvertToPoseByName)
				);

			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().OpenRelatedAsset,
				FExecuteAction::CreateSP( this, &FBlueprintEditor::OnOpenRelatedAsset )
				);

			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().CreateComment,
				FExecuteAction::CreateSP( this, &FBlueprintEditor::OnCreateComment )
				);

			GraphEditorCommands->MapAction(FGraphEditorCommands::Get().ShowAllPins,
				FExecuteAction::CreateSP(this, &FBlueprintEditor::SetPinVisibility, SGraphEditor::Pin_Show)
				);

			GraphEditorCommands->MapAction(FGraphEditorCommands::Get().HideNoConnectionPins,
				FExecuteAction::CreateSP(this, &FBlueprintEditor::SetPinVisibility, SGraphEditor::Pin_HideNoConnection)
				);

			GraphEditorCommands->MapAction(FGraphEditorCommands::Get().HideNoConnectionNoDefaultPins,
				FExecuteAction::CreateSP(this, &FBlueprintEditor::SetPinVisibility, SGraphEditor::Pin_HideNoConnectionNoDefault)
				);

			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().FindReferences,
				FExecuteAction::CreateSP( this, &FBlueprintEditor::OnFindReferences ),
				FCanExecuteAction::CreateSP( this, &FBlueprintEditor::CanFindReferences )
				);

			GraphEditorCommands->MapAction( FGraphEditorCommands::Get().GoToDefinition,
				FExecuteAction::CreateSP( this, &FBlueprintEditor::OnGoToDefinition ),
				FCanExecuteAction::CreateSP( this, &FBlueprintEditor::CanGoToDefinition )
				);

			GraphEditorCommands->MapAction(FGraphEditorCommands::Get().GoToDocumentation,
				FExecuteAction::CreateSP(this, &FBlueprintEditor::OnGoToDocumentation),
				FCanExecuteAction::CreateSP(this, &FBlueprintEditor::CanGoToDocumentation)
				);

			GraphEditorCommands->MapAction(FGraphEditorCommands::Get().EnableNodes,
				FExecuteAction(),
				FCanExecuteAction(),
				FGetActionCheckState::CreateSP(this, &FBlueprintEditor::GetEnabledCheckBoxStateForSelectedNodes)
				);

			GraphEditorCommands->MapAction(FGraphEditorCommands::Get().DisableNodes,
				FExecuteAction::CreateSP(this, &FBlueprintEditor::OnSetEnabledStateForSelectedNodes, ENodeEnabledState::Disabled),
				FCanExecuteAction(),
				FGetActionCheckState::CreateSP(this, &FBlueprintEditor::CheckEnabledStateForSelectedNodes, ENodeEnabledState::Disabled)
				);

			GraphEditorCommands->MapAction(FGraphEditorCommands::Get().EnableNodes_Always,
				FExecuteAction::CreateSP(this, &FBlueprintEditor::OnSetEnabledStateForSelectedNodes, ENodeEnabledState::Enabled),
				FCanExecuteAction(),
				FGetActionCheckState::CreateSP(this, &FBlueprintEditor::CheckEnabledStateForSelectedNodes, ENodeEnabledState::Enabled)
				);

			GraphEditorCommands->MapAction(FGraphEditorCommands::Get().EnableNodes_DevelopmentOnly,
				FExecuteAction::CreateSP(this, &FBlueprintEditor::OnSetEnabledStateForSelectedNodes, ENodeEnabledState::DevelopmentOnly),
				FCanExecuteAction(),
				FGetActionCheckState::CreateSP(this, &FBlueprintEditor::CheckEnabledStateForSelectedNodes, ENodeEnabledState::DevelopmentOnly)
				);

			OnCreateGraphEditorCommands(GraphEditorCommands);
		}
	}

	// Create the title bar widget
	TSharedPtr<SWidget> TitleBarWidget = SNew(SGraphTitleBar)
		.EdGraphObj(InGraph)
		.Kismet2(SharedThis(this))
		.OnDifferentGraphCrumbClicked(this, &FBlueprintEditor::OnChangeBreadCrumbGraph)
		.HistoryNavigationWidget(InTabInfo->CreateHistoryNavigationWidget());

	SGraphEditor::FGraphEditorEvents InEvents;
	InEvents.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateSP( this, &FBlueprintEditor::OnSelectedNodesChanged );
	InEvents.OnDropActor = SGraphEditor::FOnDropActor::CreateSP( this, &FBlueprintEditor::OnGraphEditorDropActor );
	InEvents.OnDropStreamingLevel = SGraphEditor::FOnDropStreamingLevel::CreateSP( this, &FBlueprintEditor::OnGraphEditorDropStreamingLevel );
	InEvents.OnNodeDoubleClicked = FSingleNodeEvent::CreateSP(this, &FBlueprintEditor::OnNodeDoubleClicked);
	InEvents.OnVerifyTextCommit = FOnNodeVerifyTextCommit::CreateSP(this, &FBlueprintEditor::OnNodeVerifyTitleCommit);
	InEvents.OnTextCommitted = FOnNodeTextCommitted::CreateSP(this, &FBlueprintEditor::OnNodeTitleCommitted);
	InEvents.OnSpawnNodeByShortcut = SGraphEditor::FOnSpawnNodeByShortcut::CreateSP(this, &FBlueprintEditor::OnSpawnGraphNodeByShortcut, InGraph);
	InEvents.OnNodeSpawnedByKeymap = SGraphEditor::FOnNodeSpawnedByKeymap::CreateSP(this, &FBlueprintEditor::OnNodeSpawnedByKeymap );
	InEvents.OnDisallowedPinConnection = SGraphEditor::FOnDisallowedPinConnection::CreateSP(this, &FBlueprintEditor::OnDisallowedPinConnection);

	// Custom menu for K2 schemas
	if(InGraph->Schema != NULL && InGraph->Schema->IsChildOf(UEdGraphSchema_K2::StaticClass()))
	{
		InEvents.OnCreateActionMenu = SGraphEditor::FOnCreateActionMenu::CreateSP(this, &FBlueprintEditor::OnCreateGraphActionMenu);
	}

	// Append play world commands
	GraphEditorCommands->Append( FPlayWorldCommands::GlobalPlayWorldActions.ToSharedRef() );

	TSharedRef<SGraphEditor> Editor = SNew(SGraphEditor)
		.AdditionalCommands(GraphEditorCommands)
		.IsEditable(this, &FBlueprintEditor::IsEditable, InGraph)
		.DisplayAsReadOnly(this, &FBlueprintEditor::IsGraphReadOnly, InGraph)
		.TitleBar(TitleBarWidget)
		.Appearance(this, &FBlueprintEditor::GetGraphAppearance, InGraph)
		.GraphToEdit(InGraph)
		.GraphEvents(InEvents)
		.OnNavigateHistoryBack(FSimpleDelegate::CreateSP(this, &FBlueprintEditor::NavigateTab, FDocumentTracker::NavigateBackwards))
		.OnNavigateHistoryForward(FSimpleDelegate::CreateSP(this, &FBlueprintEditor::NavigateTab, FDocumentTracker::NavigateForwards));
		//@TODO: Crashes in command list code during the callback .OnGraphModuleReloaded(FEdGraphEvent::CreateSP(this, &FBlueprintEditor::ChangeOpenGraphInDocumentEditorWidget, WeakParent))
		;

	OnSetPinVisibility.AddSP(&Editor.Get(), &SGraphEditor::SetPinVisibility);

	FVector2D ViewOffset = FVector2D::ZeroVector;
	float ZoomAmount = INDEX_NONE;

	TSharedPtr<SDockTab> ActiveTab = DocumentManager->GetActiveTab();
	if(ActiveTab.IsValid())
	{
		// Check if the graph is already opened in the current tab, if it is we want to start at the same position to stop the graph from jumping around oddly
		TSharedPtr<SGraphEditor> GraphEditor = StaticCastSharedRef<SGraphEditor>(ActiveTab->GetContent());

		if(GraphEditor.IsValid() && GraphEditor->GetCurrentGraph() == InGraph)
		{
			GraphEditor->GetViewLocation(ViewOffset, ZoomAmount);
		}
	}

	Editor->SetViewLocation(ViewOffset, ZoomAmount);

	return Editor;
}

FGraphAppearanceInfo FBlueprintEditor::GetCurrentGraphAppearance() const
{
	return GetGraphAppearance(GetFocusedGraph());
}

FGraphAppearanceInfo FBlueprintEditor::GetGraphAppearance(UEdGraph* InGraph) const
{
	// Create the appearance info
	FGraphAppearanceInfo AppearanceInfo;

	UBlueprint* Blueprint = (InGraph != nullptr) ? FBlueprintEditorUtils::FindBlueprintForGraph(InGraph) : GetBlueprintObj();
	if (Blueprint != NULL)
	{
	switch (Blueprint->BlueprintType)
	{
	case BPTYPE_LevelScript:
		AppearanceInfo.CornerText = LOCTEXT("AppearanceCornerText_LevelScript", "LEVEL BLUEPRINT");
		break;
	case BPTYPE_MacroLibrary:
		AppearanceInfo.CornerText = LOCTEXT("AppearanceCornerText_Macro", "MACRO");
		break;
	case BPTYPE_Interface:
		AppearanceInfo.CornerText = LOCTEXT("AppearanceCornerText_Interface", "INTERFACE");
		break;
	default:
		AppearanceInfo.CornerText = LOCTEXT("AppearanceCornerText_Blueprint", "BLUEPRINT");
		break;
	}
	}

	UEdGraph const* EditingGraph = GetFocusedGraph();
	if (InGraph && BlueprintEditorImpl::GraphHasDefaultNode(InGraph))
	{
		AppearanceInfo.InstructionText = LOCTEXT("AppearanceInstructionText_DefaultGraph", "Drag Off Pins to Create/Connect New Nodes.");
	}
	else // if the graph is empty...
	{
		AppearanceInfo.InstructionText = LOCTEXT("AppearanceInstructionText_EmptyGraph", "Right-Click to Create New Nodes.");
	}
	auto InstructionOpacityDelegate = TAttribute<float>::FGetter::CreateSP(this, &FBlueprintEditor::GetInstructionTextOpacity, InGraph);
	AppearanceInfo.InstructionFade.Bind(InstructionOpacityDelegate);

	AppearanceInfo.PIENotifyText = GetPIEStatus();

	return AppearanceInfo;
}

// Open the editor for a given graph
void FBlueprintEditor::OnChangeBreadCrumbGraph(UEdGraph* InGraph)
{
	if (InGraph && FocusedGraphEdPtr.IsValid())
	{
		OpenDocument(InGraph, FDocumentTracker::NavigatingCurrentDocument);
	}
}

FBlueprintEditor::FBlueprintEditor()
	: bSaveIntermediateBuildProducts(false)
	, bPendingDeferredClose(false)
	, bRequestedSavingOpenDocumentState(false)
	, bBlueprintModifiedOnOpen (false)
	, PinVisibility(SGraphEditor::Pin_Show)
	, bIsActionMenuContextSensitive(true)
	, CurrentUISelection(NAME_None)
	, bEditorMarkedAsClosed(false)
	, HasOpenActionMenu(nullptr)
	, InstructionsFadeCountdown(0.f)
{
	AnalyticsStats.GraphActionMenusNonCtxtSensitiveExecCount = 0;
	AnalyticsStats.GraphActionMenusCtxtSensitiveExecCount = 0;
	AnalyticsStats.GraphActionMenusCancelledCount = 0;
	AnalyticsStats.MyBlueprintNodeDragPlacementCount = 0;
	AnalyticsStats.PaletteNodeDragPlacementCount = 0;
	AnalyticsStats.NodeGraphContextCreateCount = 0;
	AnalyticsStats.NodePinContextCreateCount = 0;
	AnalyticsStats.NodeKeymapCreateCount = 0;
	AnalyticsStats.NodePasteCreateCount = 0;

	UEditorEngine* Editor = (UEditorEngine*)GEngine;
	if (Editor != NULL)
	{
		Editor->RegisterForUndo(this);
	}

	DocumentManager = MakeShareable(new FDocumentTracker);
}

void FBlueprintEditor::EnsureBlueprintIsUpToDate(UBlueprint* BlueprintObj)
{
	// Purge any NULL graphs
	FBlueprintEditorUtils::PurgeNullGraphs(BlueprintObj);

	// Make sure the blueprint is cosmetically up to date
	FKismetEditorUtilities::UpgradeCosmeticallyStaleBlueprint(BlueprintObj);

	if (FBlueprintEditorUtils::SupportsConstructionScript(BlueprintObj))
	{
		// If we don't have an SCS yet, make it
		if(BlueprintObj->SimpleConstructionScript == NULL)
		{
			check(NULL != BlueprintObj->GeneratedClass);
			BlueprintObj->SimpleConstructionScript = NewObject<USimpleConstructionScript>(BlueprintObj->GeneratedClass);
			BlueprintObj->SimpleConstructionScript->SetFlags(RF_Transactional);

			// Recreate (or create) any widgets that depend on the SCS
			CreateSCSEditors();
		}

		// If we should have a UCS but don't yet, make it
		if(!FBlueprintEditorUtils::FindUserConstructionScript(BlueprintObj))
		{
			const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
			UEdGraph* UCSGraph = FBlueprintEditorUtils::CreateNewGraph(BlueprintObj, K2Schema->FN_UserConstructionScript, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
			FBlueprintEditorUtils::AddFunctionGraph(BlueprintObj, UCSGraph, /*bIsUserCreated=*/ false, AActor::StaticClass());
			UCSGraph->bAllowDeletion = false;
		}

		// Check to see if we have gained a component from our parent (that would require us removing our scene root)
		// (or lost one, which requires adding one)
		if (BlueprintObj->SimpleConstructionScript != nullptr)
		{
			BlueprintObj->SimpleConstructionScript->ValidateSceneRootNodes();
		}
	}
	else
	{
		// If we have an SCS but don't support it, then we remove it
		if(BlueprintObj->SimpleConstructionScript)
		{
			// Remove any SCS variable nodes
			for (USCS_Node* SCS_Node : BlueprintObj->SimpleConstructionScript->GetAllNodes())
			{
				if(SCS_Node)
				{
					FBlueprintEditorUtils::RemoveVariableNodes(BlueprintObj, SCS_Node->GetVariableName());
				}
			}
		
			// Remove the SCS object reference
			BlueprintObj->SimpleConstructionScript = NULL;

			// Mark the Blueprint as having been structurally modified
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BlueprintObj);
		}
	}

	// Make sure that this blueprint is up-to-date with regards to its parent functions
	FBlueprintEditorUtils::ConformCallsToParentFunctions(BlueprintObj);

	// Make sure that this blueprint is up-to-date with regards to its implemented events
	FBlueprintEditorUtils::ConformImplementedEvents(BlueprintObj);

	// Make sure that this blueprint is up-to-date with regards to its implemented interfaces
	FBlueprintEditorUtils::ConformImplementedInterfaces(BlueprintObj);

	// Update old composite nodes(can't do this in PostLoad)
	FBlueprintEditorUtils::UpdateOutOfDateCompositeNodes(BlueprintObj);

	// Update any nodes which might have dropped their RF_Transactional flag due to copy-n-paste issues
	FBlueprintEditorUtils::UpdateTransactionalFlags(BlueprintObj);
}

struct FLoadObjectsFromAssetRegistryHelper
{
	template<class TObjectType>
	static void Load(TSet<TWeakObjectPtr<TObjectType>>& Collection)
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

		const double CompileStartTime = FPlatformTime::Seconds();

		TArray<FAssetData> AssetData;
		AssetRegistryModule.Get().GetAssetsByClass(TObjectType::StaticClass()->GetFName(), AssetData);

		for (int32 AssetIndex = 0; AssetIndex < AssetData.Num(); ++AssetIndex)
		{
			if(AssetData[AssetIndex].IsValid())
			{
				FString AssetPath = AssetData[AssetIndex].ObjectPath.ToString();
				TObjectType* Object = LoadObject<TObjectType>(NULL, *AssetPath, NULL, 0, NULL);
				if (Object)
				{
					Collection.Add( TWeakObjectPtr<TObjectType>(Object) );
				}
			}
		}

		const double FinishTime = FPlatformTime::Seconds();

		UE_LOG(LogBlueprint, Log, TEXT("Loading all assets of type: %s took %.2f seconds"), *TObjectType::StaticClass()->GetName(), static_cast<float>(FinishTime - CompileStartTime));
	}
};

void FBlueprintEditor::CommonInitialization(const TArray<UBlueprint*>& InitBlueprints)
{
	TSharedPtr<FBlueprintEditor> ThisPtr(SharedThis(this));

	// @todo TabManagement
	DocumentManager->Initialize(ThisPtr);

	// Register the document factories
	{
		DocumentManager->RegisterDocumentFactory(MakeShareable(new FTimelineEditorSummoner(ThisPtr)));

		TSharedRef<FDocumentTabFactory> GraphEditorFactory = MakeShareable(new FGraphEditorSummoner(ThisPtr,
			FGraphEditorSummoner::FOnCreateGraphEditorWidget::CreateSP(this, &FBlueprintEditor::CreateGraphEditorWidget)
			));

		// Also store off a reference to the grapheditor factory so we can find all the tabs spawned by it later.
		GraphEditorTabFactoryPtr = GraphEditorFactory;
		DocumentManager->RegisterDocumentFactory(GraphEditorFactory);
	}

	// Make sure we know when tabs become active to update details tab
	OnActiveTabChangedDelegateHandle = FGlobalTabmanager::Get()->OnActiveTabChanged_Subscribe( FOnActiveTabChanged::FDelegate::CreateRaw(this, &FBlueprintEditor::OnActiveTabChanged) );

	if (InitBlueprints.Num() == 1)
	{
		// Load blueprint libraries
		LoadLibrariesFromAssetRegistry();

		FLoadObjectsFromAssetRegistryHelper::Load<UUserDefinedEnum>(UserDefinedEnumerators);

		UBlueprint* InitBlueprint = InitBlueprints[0];

		// Update the blueprint if required
		EBlueprintStatus OldStatus = InitBlueprint->Status;
		EnsureBlueprintIsUpToDate(InitBlueprint);
		bBlueprintModifiedOnOpen = (InitBlueprint->Status != OldStatus);

		// Flag the blueprint as having been opened
		InitBlueprint->bIsNewlyCreated = false;

		// When the blueprint that we are observing changes, it will notify this wrapper widget.
		InitBlueprint->OnChanged().AddSP(this, &FBlueprintEditor::OnBlueprintChanged);
		InitBlueprint->OnCompiled().AddSP(this, &FBlueprintEditor::OnBlueprintCompiled);
		InitBlueprint->OnSetObjectBeingDebugged().AddSP(this, &FBlueprintEditor::HandleSetObjectBeingDebugged);
	}

	CreateDefaultCommands();
	CreateDefaultTabContents(InitBlueprints);

	FKismetEditorUtilities::OnBlueprintUnloaded.AddSP(this, &FBlueprintEditor::OnBlueprintUnloaded);
}

void FBlueprintEditor::LoadLibrariesFromAssetRegistry()
{
	if( GetBlueprintObj() )
	{
		FString UserDeveloperPath = FPackageName::FilenameToLongPackageName( FPaths::GameUserDeveloperDir());
		FString DeveloperPath = FPackageName::FilenameToLongPackageName( FPaths::GameDevelopersDir() );

		//Don't allow loading blueprint macros & functions for interface & data-only blueprints
		if( GetBlueprintObj()->BlueprintType != BPTYPE_Interface && GetBlueprintObj()->BlueprintType != BPTYPE_Const)
		{
			// Load the asset registry module
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

			// Collect a full list of assets with the specified class
			TArray<FAssetData> AssetData;
			AssetRegistryModule.Get().GetAssetsByClass(UBlueprint::StaticClass()->GetFName(), AssetData);

			GWarn->BeginSlowTask(LOCTEXT("LoadingBlueprintAssetData", "Loading Blueprint Asset Data"), true );

			const FName BPTypeName( TEXT( "BlueprintType" ) );
			const FString BPMacroTypeStr( TEXT( "BPTYPE_MacroLibrary" ) );
			const FString BPFunctionTypeStr( TEXT( "BPTYPE_FunctionLibrary" ) );

			for (int32 AssetIndex = 0; AssetIndex < AssetData.Num(); ++AssetIndex)
			{
				FString TagValue = AssetData[ AssetIndex ].GetTagValueRef<FString>(BPTypeName);

				//Only check for Blueprint Macros & Functions in the asset data for loading
				if ( TagValue == BPMacroTypeStr || TagValue == BPFunctionTypeStr )
				{
					FString BlueprintPath = AssetData[AssetIndex].ObjectPath.ToString();

					//For blueprints inside developers folder, only allow the ones inside current user's developers folder.
					bool AllowLoadBP = true;
					if( BlueprintPath.StartsWith(DeveloperPath) )
					{
						if(  !BlueprintPath.StartsWith(UserDeveloperPath) )
						{
							AllowLoadBP = false;
						}
					}

					if( AllowLoadBP )
					{
						// Load the blueprint
						UBlueprint* BlueprintLibPtr = LoadObject<UBlueprint>(NULL, *BlueprintPath, NULL, 0, NULL);
						if (BlueprintLibPtr )
						{
							StandardLibraries.AddUnique(BlueprintLibPtr);
						}
					}
				}

			}
			GWarn->EndSlowTask();
		}
	}
}

void FBlueprintEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	//@TODO: Can't we do this sooner?
	DocumentManager->SetTabManager(InTabManager);

	FWorkflowCentricApplication::RegisterTabSpawners(InTabManager);
}

void FBlueprintEditor::SetCurrentMode(FName NewMode)
{
	// Clear the selection state when the mode changes.
	SetUISelectionState(NAME_None);

	OnModeSetData.Broadcast( NewMode );
	FWorkflowCentricApplication::SetCurrentMode(NewMode);
}

void FBlueprintEditor::InitBlueprintEditor(
	const EToolkitMode::Type Mode,
	const TSharedPtr< IToolkitHost >& InitToolkitHost,
	const TArray<UBlueprint*>& InBlueprints,
	bool bShouldOpenInDefaultsMode)
{
	check(InBlueprints.Num() == 1 || bShouldOpenInDefaultsMode);

	// TRUE if a single Blueprint is being opened and is marked as newly created
	bool bNewlyCreated = InBlueprints.Num() == 1 && InBlueprints[0]->bIsNewlyCreated;

	TArray< UObject* > Objects;
	for (UBlueprint* Blueprint : InBlueprints)
	{
		// Flag the blueprint as having been opened
		Blueprint->bIsNewlyCreated = false;

		Objects.Add( Blueprint );
	}
	
	if (!Toolbar.IsValid())
	{
		Toolbar = MakeShareable(new FBlueprintEditorToolbar(SharedThis(this)));
	}

	GetToolkitCommands()->Append(FPlayWorldCommands::GlobalPlayWorldActions.ToSharedRef());

	// Initialize the asset editor and spawn nothing (dummy layout)
	const TSharedRef<FTabManager::FLayout> DummyLayout = FTabManager::NewLayout("NullLayout")->AddArea(FTabManager::NewPrimaryArea());
	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	InitAssetEditor(Mode, InitToolkitHost, BlueprintEditorAppName, DummyLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, Objects);
	
	CommonInitialization(InBlueprints);
	
	TSharedPtr<FExtender> MenuExtender = MakeShareable(new FExtender);
	FKismet2Menu::SetupBlueprintEditorMenu( MenuExtender, *this );
	AddMenuExtender(MenuExtender);

	FBlueprintEditorModule* BlueprintEditorModule = &FModuleManager::LoadModuleChecked<FBlueprintEditorModule>("Kismet");
	TSharedPtr<FExtender> CustomExtenders = BlueprintEditorModule->GetMenuExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects());
	BlueprintEditorModule->OnGatherBlueprintMenuExtensions().Broadcast(CustomExtenders, GetBlueprintObj());

	AddMenuExtender(CustomExtenders);
	AddToolbarExtender(CustomExtenders);
	
	RegenerateMenusAndToolbars();

	RegisterApplicationModes(InBlueprints, bShouldOpenInDefaultsMode, bNewlyCreated);

	// Post-layout initialization
	PostLayoutBlueprintEditorInitialization();

	// Find and set any instances of this blueprint type if any exists and we are not already editing one
	FBlueprintEditorUtils::FindAndSetDebuggableBlueprintInstances();

	if ( bNewlyCreated )
	{
		if ( UBlueprint* Blueprint = GetBlueprintObj() )
		{
			if ( Blueprint->BlueprintType == BPTYPE_MacroLibrary )
			{
				NewDocument_OnClick(CGT_NewMacroGraph);
			}
			else if ( Blueprint->BlueprintType == BPTYPE_Interface )
			{
				NewDocument_OnClick(CGT_NewFunctionGraph);
			}
			else if ( Blueprint->BlueprintType == BPTYPE_FunctionLibrary )
			{
				NewDocument_OnClick(CGT_NewFunctionGraph);
			}
		}
	}

	if ( UBlueprint* Blueprint = GetBlueprintObj() )
	{
		if ( Blueprint->GetClass() == UBlueprint::StaticClass() && Blueprint->BlueprintType == BPTYPE_Normal )
		{
			if ( !bShouldOpenInDefaultsMode )
			{
				GetToolkitCommands()->ExecuteAction(FFullBlueprintEditorCommands::Get().EditClassDefaults.ToSharedRef());
			}
		}

		// There are upgrade notes, open the log and dump the messages to it
		if (Blueprint->UpgradeNotesLog.IsValid())
		{
			DumpMessagesToCompilerLog(Blueprint->UpgradeNotesLog->Messages, true);
		}
	}
}

void FBlueprintEditor::RegisterApplicationModes(const TArray<UBlueprint*>& InBlueprints, bool bShouldOpenInDefaultsMode, bool bNewlyCreated/* = false*/)
{
	// Newly-created Blueprints will open in Components mode rather than Standard mode
	bool bShouldOpenInComponentsMode = !bShouldOpenInDefaultsMode && bNewlyCreated;

	// Create the modes and activate one (which will populate with a real layout)
	if ( UBlueprint* SingleBP = GetBlueprintObj() )
	{
		if ( !bShouldOpenInDefaultsMode && FBlueprintEditorUtils::IsInterfaceBlueprint(SingleBP) )
		{
			// Interfaces are only valid in the Interface mode
			AddApplicationMode(
				FBlueprintEditorApplicationModes::BlueprintInterfaceMode,
				MakeShareable(new FBlueprintInterfaceApplicationMode(SharedThis(this))));
			SetCurrentMode(FBlueprintEditorApplicationModes::BlueprintInterfaceMode);
		}
		else if ( SingleBP->BlueprintType == BPTYPE_MacroLibrary )
		{
			// Macro libraries are only valid in the Macro mode
			AddApplicationMode(
				FBlueprintEditorApplicationModes::BlueprintMacroMode,
				MakeShareable(new FBlueprintMacroApplicationMode(SharedThis(this))));
			SetCurrentMode(FBlueprintEditorApplicationModes::BlueprintMacroMode);
		}
		else if ( SingleBP->BlueprintType == BPTYPE_FunctionLibrary )
		{
			AddApplicationMode(
				FBlueprintEditorApplicationModes::StandardBlueprintEditorMode,
				MakeShareable(new FBlueprintEditorUnifiedMode(SharedThis(this), FBlueprintEditorApplicationModes::StandardBlueprintEditorMode, FBlueprintEditorApplicationModes::GetLocalizedMode, CanAccessComponentsMode())));
			SetCurrentMode(FBlueprintEditorApplicationModes::StandardBlueprintEditorMode);
		}
		else
		{
			if ( bShouldOpenInDefaultsMode )
			{
				// We either have no blueprints or many, open in the defaults mode for multi-editing
				AddApplicationMode(
					FBlueprintEditorApplicationModes::BlueprintDefaultsMode,
					MakeShareable(new FBlueprintDefaultsApplicationMode(SharedThis(this))));
				SetCurrentMode(FBlueprintEditorApplicationModes::BlueprintDefaultsMode);
			}
			else
			{
				AddApplicationMode(
					FBlueprintEditorApplicationModes::StandardBlueprintEditorMode,
					MakeShareable(new FBlueprintEditorUnifiedMode(SharedThis(this), FBlueprintEditorApplicationModes::StandardBlueprintEditorMode, FBlueprintEditorApplicationModes::GetLocalizedMode, CanAccessComponentsMode())));
				SetCurrentMode(FBlueprintEditorApplicationModes::StandardBlueprintEditorMode);

				if ( bShouldOpenInComponentsMode && CanAccessComponentsMode() )
				{
					TabManager->InvokeTab(FBlueprintEditorTabs::SCSViewportID);
				}
			}
		}
	}
	else
	{
		// We either have no blueprints or many, open in the defaults mode for multi-editing
		AddApplicationMode(
			FBlueprintEditorApplicationModes::BlueprintDefaultsMode,
			MakeShareable(new FBlueprintDefaultsApplicationMode(SharedThis(this))));
		SetCurrentMode(FBlueprintEditorApplicationModes::BlueprintDefaultsMode);
	}
}

void FBlueprintEditor::PostRegenerateMenusAndToolbars()
{
	UBlueprint* BluePrint = GetBlueprintObj();

	if ( BluePrint && !FBlueprintEditorUtils::IsLevelScriptBlueprint( BluePrint ) )
	{
		// build and attach the menu overlay
		TSharedRef<SHorizontalBox> MenuOverlayBox = SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.ColorAndOpacity( FSlateColor::UseSubduedForeground() )
				.ShadowOffset( FVector2D::UnitVector )
				.Text(LOCTEXT("BlueprintEditor_ParentClass", "Parent class: "))
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SSpacer)
				.Size(FVector2D(2.0f,1.0f))
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.ShadowOffset(FVector2D::UnitVector)
				.Text(this, &FBlueprintEditor::GetParentClassNameText)
				.TextStyle(FEditorStyle::Get(), "Common.InheritedFromBlueprintTextStyle")
				.ToolTipText(LOCTEXT("ParentClassToolTip", "The class that the current Blueprint is based on. The parent provides the base definition, which the current Blueprint extends."))
				.Visibility(this, &FBlueprintEditor::GetParentClassNameVisibility)
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.VAlign(VAlign_Center)
				.ButtonStyle( FEditorStyle::Get(), "HoverHintOnly" )
				.OnClicked( this, &FBlueprintEditor::OnFindParentClassInContentBrowserClicked )
				.IsEnabled( this, &FBlueprintEditor::IsParentClassABlueprint )
				.Visibility( this, &FBlueprintEditor::ParentClassButtonsVisibility )
				.ToolTipText( LOCTEXT("FindParentInCBToolTip", "Find parent in Content Browser") )
				.ContentPadding(4.0f)
				.ForegroundColor( FSlateColor::UseForeground() )
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("PropertyWindow.Button_Browse"))
				]
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.VAlign(VAlign_Center)
				.ButtonStyle( FEditorStyle::Get(), "HoverHintOnly" )
				.OnClicked( this, &FBlueprintEditor::OnEditParentClassClicked )
				.IsEnabled( this, &FBlueprintEditor::IsParentClassABlueprint )
				.Visibility( this, &FBlueprintEditor::ParentClassButtonsVisibility )
				.ToolTipText( LOCTEXT("EditParentClassToolTip", "Open parent in editor") )
				.ContentPadding(4.0f)
				.ForegroundColor( FSlateColor::UseForeground() )
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("PropertyWindow.Button_Edit"))
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SHyperlink)
				.Style(FEditorStyle::Get(), "Common.GotoNativeCodeHyperlink")
				.IsEnabled(this, &FBlueprintEditor::IsNativeParentClassCodeLinkEnabled)
				.Visibility(this, &FBlueprintEditor::GetNativeParentClassButtonsVisibility)
				.OnNavigate(this, &FBlueprintEditor::OnEditParentClassNativeCodeClicked)
				.Text(this, &FBlueprintEditor::GetTextForNativeParentClassHeaderLink)
				.ToolTipText(FText::Format(LOCTEXT("GoToCode_ToolTip", "Click to open this source file in {0}"), FSourceCodeNavigation::GetSelectedSourceCodeIDE()))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SSpacer)
				.Size(FVector2D(8.0f, 1.0f))
			]
			;
		SetMenuOverlay( MenuOverlayBox );
	}
}

FText FBlueprintEditor::GetParentClassNameText() const
{
	UClass* ParentClass = (GetBlueprintObj() != NULL) ? GetBlueprintObj()->ParentClass : NULL;
	return (ParentClass != NULL) ? ParentClass->GetDisplayNameText() : LOCTEXT("BlueprintEditor_NoParentClass", "None");
}

bool FBlueprintEditor::IsParentClassOfObjectABlueprint( const UBlueprint* Blueprint ) const
{
	if ( Blueprint != NULL )
	{
		UObject* ParentClass = Blueprint->ParentClass;
		if ( ParentClass != NULL )
		{
			if ( ParentClass->IsA( UBlueprintGeneratedClass::StaticClass() ) )
			{
				return true;
			}
		}
	}

	return false;
}

bool FBlueprintEditor::IsParentClassABlueprint() const
{
	const UBlueprint* Blueprint = GetBlueprintObj();

	return IsParentClassOfObjectABlueprint( Blueprint );
}

EVisibility FBlueprintEditor::ParentClassButtonsVisibility() const
{
	return IsParentClassABlueprint() ? EVisibility::Visible : EVisibility::Collapsed;
}

bool FBlueprintEditor::IsParentClassNative() const
{
	const UBlueprint* Blueprint = GetBlueprintObj();
	if (Blueprint != NULL)
	{
		UClass* ParentClass = Blueprint->ParentClass;
		if (ParentClass != NULL)
		{
			if (ParentClass->HasAllClassFlags(CLASS_Native))
			{
				return true;
			}
		}
	}

	return false;
}

bool FBlueprintEditor::IsNativeParentClassCodeLinkEnabled() const
{
	const UBlueprint* Blueprint = GetBlueprintObj();
	return Blueprint && FSourceCodeNavigation::CanNavigateToClass(Blueprint->ParentClass);
}

EVisibility FBlueprintEditor::GetNativeParentClassButtonsVisibility() const
{
	return IsNativeParentClassCodeLinkEnabled() ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility FBlueprintEditor::GetParentClassNameVisibility() const
{
	return !IsNativeParentClassCodeLinkEnabled() ? EVisibility::Visible : EVisibility::Collapsed;
}

void FBlueprintEditor::OnEditParentClassNativeCodeClicked()
{
	const UBlueprint* Blueprint = GetBlueprintObj();
	if (Blueprint)
	{
		FSourceCodeNavigation::NavigateToClass(Blueprint->ParentClass);
	}
}

FText FBlueprintEditor::GetTextForNativeParentClassHeaderLink() const
{
	// it could be done using FSourceCodeNavigation, but it could be slow
	return FText::FromString(*GetParentClassNameText().ToString());
}

FReply FBlueprintEditor::OnFindParentClassInContentBrowserClicked()
{
	UBlueprint* Blueprint = GetBlueprintObj();
	if ( Blueprint != NULL )
	{
		UObject* ParentClass = Blueprint->ParentClass;
		if ( ParentClass != NULL )
		{
			UBlueprintGeneratedClass* ParentBlueprintGeneratedClass = Cast<UBlueprintGeneratedClass>( ParentClass );
			if ( ParentBlueprintGeneratedClass != NULL )
			{
				if ( ParentBlueprintGeneratedClass->ClassGeneratedBy != NULL )
				{
					TArray< UObject* > ParentObjectList;
					ParentObjectList.Add( ParentBlueprintGeneratedClass->ClassGeneratedBy );
					GEditor->SyncBrowserToObjects( ParentObjectList );
				}
			}
		}
	}

	return FReply::Handled();
}

FReply FBlueprintEditor::OnEditParentClassClicked()
{
	UBlueprint* Blueprint = GetBlueprintObj();
	if ( Blueprint != NULL )
	{
		UObject* ParentClass = Blueprint->ParentClass;
		if ( ParentClass != NULL )
		{
			UBlueprintGeneratedClass* ParentBlueprintGeneratedClass = Cast<UBlueprintGeneratedClass>( ParentClass );
			if ( ParentBlueprintGeneratedClass != NULL )
			{
				FAssetEditorManager::Get().OpenEditorForAsset( ParentBlueprintGeneratedClass->ClassGeneratedBy );
			}
		}
	}

	return FReply::Handled();
}

void FBlueprintEditor::PostLayoutBlueprintEditorInitialization()
{
	if (UBlueprint* Blueprint = GetBlueprintObj())
	{
		// Refresh the graphs
		RefreshEditors();
		
		// EnsureBlueprintIsUpToDate may have updated the blueprint so show notifications to user.
		if (bBlueprintModifiedOnOpen)
		{
			bBlueprintModifiedOnOpen = false;

			if (FocusedGraphEdPtr.IsValid())
			{
				FNotificationInfo Info( NSLOCTEXT("Kismet", "Blueprint Modified", "Blueprint requires updating. Please resave.") );
				Info.Image = FEditorStyle::GetBrush(TEXT("Icons.Info"));
				Info.bFireAndForget = true;
				Info.bUseSuccessFailIcons = false;
				Info.ExpireDuration = 5.0f;

				FocusedGraphEdPtr.Pin()->AddNotification(Info, true);
			}

			// Fire log message
			FString BlueprintName;
			Blueprint->GetName(BlueprintName);

			FFormatNamedArguments Args;
			Args.Add( TEXT("BlueprintName"), FText::FromString( BlueprintName ) );
			LogSimpleMessage( FText::Format( LOCTEXT("Blueprint Modified Long", "Blueprint \"{BlueprintName}\" was updated to fix issues detected on load. Please resave."), Args ) );
		}

		// If we have a warning/error, open output log.
		if (!Blueprint->IsUpToDate() || (Blueprint->Status == BS_UpToDateWithWarnings))
		{
			TabManager->InvokeTab(FBlueprintEditorTabs::CompilerResultsID);
		}
	}

	if (!GetDefault<UBlueprintEditorSettings>()->bHostFindInBlueprintsInGlobalTab)
	{
		// Close any docked global FiB tabs that may have been restored with a saved layout.
		FFindInBlueprintSearchManager::Get().CloseOrphanedGlobalFindResultsTabs(TabManager);
	}
}

void FBlueprintEditor::SetupViewForBlueprintEditingMode()
{
	// Make sure the defaults tab is pointing to the defaults
	StartEditingDefaults(/*bAutoFocus=*/ true);

	// Make sure the inspector is always on top
	//@TODO: This is necessary right now because of a bug in restoring layouts not remembering which tab is on top (to get it right initially), but do we want this behavior always?
	TryInvokingDetailsTab();

	UBlueprint* Blueprint = GetBlueprintObj();
	if ((Blueprint != nullptr) && (Blueprint->Status == EBlueprintStatus::BS_Error))
	{
		UBlueprintEditorSettings const* BpEditorSettings = GetDefault<UBlueprintEditorSettings>();
		if (BpEditorSettings->bJumpToNodeErrors)
		{
			if (UEdGraphNode* NodeWithError = BlueprintEditorImpl::FindNodeWithError(Blueprint))
			{
				JumpToNode(NodeWithError, /*bRequestRename =*/false);
			}
		}
	}
}

FBlueprintEditor::~FBlueprintEditor()
{
	// Clean up the preview
	DestroyPreview();

	// NOTE: Any tabs that we still have hanging out when destroyed will be cleaned up by FBaseToolkit's destructor
	UEditorEngine* Editor = (UEditorEngine*)GEngine;
	if (Editor)
	{
		Editor->UnregisterForUndo( this );
	}

	CloseMergeTool();

	if (GetBlueprintObj())
	{
		GetBlueprintObj()->OnChanged().RemoveAll( this );
		GetBlueprintObj()->OnCompiled().RemoveAll( this );
		GetBlueprintObj()->OnSetObjectBeingDebugged().RemoveAll( this );
	}

	FGlobalTabmanager::Get()->OnActiveTabChanged_Unsubscribe( OnActiveTabChangedDelegateHandle );

	if (FEngineAnalytics::IsAvailable())
	{
		const UGeneralProjectSettings& ProjectSettings = *GetDefault<UGeneralProjectSettings>();
		FString ProjectID = ProjectSettings.ProjectID.ToString();

		TArray< FAnalyticsEventAttribute > BPEditorAttribs;
		BPEditorAttribs.Add( FAnalyticsEventAttribute( FString( "GraphActionMenusExecuted.NonContextSensitive" ), AnalyticsStats.GraphActionMenusNonCtxtSensitiveExecCount ));
		BPEditorAttribs.Add( FAnalyticsEventAttribute( FString( "GraphActionMenusExecuted.ContextSensitive" ), AnalyticsStats.GraphActionMenusCtxtSensitiveExecCount ));
		BPEditorAttribs.Add( FAnalyticsEventAttribute( FString( "GraphActionMenusClosed" ), AnalyticsStats.GraphActionMenusCancelledCount ));

		BPEditorAttribs.Add( FAnalyticsEventAttribute( FString( "MyBlueprintDragPlacedNodesCreated" ), AnalyticsStats.MyBlueprintNodeDragPlacementCount ));
		BPEditorAttribs.Add( FAnalyticsEventAttribute( FString( "BlueprintPaletteDragPlacedNodesCreated" ), AnalyticsStats.PaletteNodeDragPlacementCount ));
		BPEditorAttribs.Add( FAnalyticsEventAttribute( FString( "GraphContextNodesCreated" ), AnalyticsStats.NodeGraphContextCreateCount ));
		BPEditorAttribs.Add( FAnalyticsEventAttribute( FString( "GraphPinContextNodesCreated" ), AnalyticsStats.NodePinContextCreateCount ));
		BPEditorAttribs.Add( FAnalyticsEventAttribute( FString( "KeymapNodesCreated" ), AnalyticsStats.NodeKeymapCreateCount ));
		BPEditorAttribs.Add( FAnalyticsEventAttribute( FString( "PastedNodesCreated" ), AnalyticsStats.NodePasteCreateCount ));

		BPEditorAttribs.Add( FAnalyticsEventAttribute( FString( "ProjectId" ), ProjectID ) );
		FEngineAnalytics::GetProvider().RecordEvent( FString( "Editor.Usage.BlueprintEditorSummary" ), BPEditorAttribs );

		for (auto Iter = AnalyticsStats.GraphDisallowedPinConnections.CreateConstIterator(); Iter; ++Iter)
		{
			TArray< FAnalyticsEventAttribute > BPEditorPinConnectAttribs;
			BPEditorPinConnectAttribs.Add( FAnalyticsEventAttribute( FString( "FromPin.Category" ), Iter->PinTypeCategoryA ));
			BPEditorPinConnectAttribs.Add( FAnalyticsEventAttribute( FString( "FromPin.IsArray" ), Iter->bPinIsArrayA ));
			BPEditorPinConnectAttribs.Add( FAnalyticsEventAttribute( FString( "FromPin.IsReference" ), Iter->bPinIsReferenceA ));
			BPEditorPinConnectAttribs.Add( FAnalyticsEventAttribute( FString( "FromPin.IsWeakPointer" ), Iter->bPinIsWeakPointerA ));
			BPEditorPinConnectAttribs.Add( FAnalyticsEventAttribute( FString( "ToPin.Category" ), Iter->PinTypeCategoryB ));
			BPEditorPinConnectAttribs.Add( FAnalyticsEventAttribute( FString( "ToPin.IsArray" ), Iter->bPinIsArrayB ));
			BPEditorPinConnectAttribs.Add( FAnalyticsEventAttribute( FString( "ToPin.IsReference" ), Iter->bPinIsReferenceB ));
			BPEditorPinConnectAttribs.Add( FAnalyticsEventAttribute( FString( "ToPin.IsWeakPointer" ), Iter->bPinIsWeakPointerB ));
			BPEditorPinConnectAttribs.Add( FAnalyticsEventAttribute( FString( "ProjectId" ), ProjectID ) );

			FEngineAnalytics::GetProvider().RecordEvent( FString( "Editor.Usage.BPDisallowedPinConnection" ), BPEditorPinConnectAttribs );
		}
	}
}

void FBlueprintEditor::FocusInspectorOnGraphSelection(const FGraphPanelSelectionSet& NewSelection, bool bForceRefresh)
{
	// If this graph has selected nodes update the details panel to match.
	if ( NewSelection.Num() > 0 || CurrentUISelection == FBlueprintEditor::SelectionState_Graph )
	{
		SetUISelectionState(FBlueprintEditor::SelectionState_Graph);

		SKismetInspector::FShowDetailsOptions ShowDetailsOptions;
		ShowDetailsOptions.bForceRefresh = bForceRefresh;

		Inspector->ShowDetailsForObjects(NewSelection.Array(), ShowDetailsOptions);
	}
}

void FBlueprintEditor::CreateDefaultTabContents(const TArray<UBlueprint*>& InBlueprints)
{
	UBlueprint* InBlueprint = InBlueprints.Num() == 1 ? InBlueprints[0] : NULL;

	// Cache off whether or not this is an interface, since it is used to govern multiple widget's behavior
	const bool bIsInterface = (InBlueprint && InBlueprint->BlueprintType == BPTYPE_Interface);
	const bool bIsMacro = (InBlueprint && InBlueprint->BlueprintType == BPTYPE_MacroLibrary);

	if (InBlueprint)
	{
		this->DebuggingView =
			SNew(SKismetDebuggingView)
			. BlueprintToWatch(InBlueprint)
			. IsEnabled(!bIsInterface && !bIsMacro);

		this->Palette = 
			SNew(SBlueprintPalette, SharedThis(this))
				.IsEnabled(this, &FBlueprintEditor::IsFocusedGraphEditable);
	}

	if (IsEditingSingleBlueprint())
	{
		this->MyBlueprintWidget = SNew(SMyBlueprint, SharedThis(this));
		this->ReplaceReferencesWidget = SNew(SReplaceNodeReferences, SharedThis(this));
	}
	
	CompilerResultsListing = FCompilerResultsLog::GetBlueprintMessageLog(InBlueprint);
	CompilerResultsListing->OnMessageTokenClicked().AddSP(this, &FBlueprintEditor::OnLogTokenClicked);

	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	CompilerResults = MessageLogModule.CreateLogListingWidget( CompilerResultsListing.ToSharedRef() );
	FindResults = SNew(SFindInBlueprints, SharedThis(this));
	
	this->Inspector = 
		SNew(SKismetInspector)
		. HideNameArea(true)
		. ViewIdentifier(FName("BlueprintInspector"))
		. Kismet2(SharedThis(this))
		. OnFinishedChangingProperties( FOnFinishedChangingProperties::FDelegate::CreateSP(this, &FBlueprintEditor::OnFinishedChangingProperties) );

	if ( InBlueprints.Num() > 0 )
	{
		const bool bShowPublicView = true;
		const bool bHideNameArea = false;

		this->DefaultEditor = 
			SNew(SKismetInspector)
			. Kismet2(SharedThis(this))
			. ViewIdentifier(FName("BlueprintDefaults"))
			. IsEnabled(!bIsInterface)
			. ShowPublicViewControl(bShowPublicView)
			. ShowTitleArea(false)
			. HideNameArea(bHideNameArea)
			. OnFinishedChangingProperties( FOnFinishedChangingProperties::FDelegate::CreateSP( this, &FBlueprintEditor::OnFinishedChangingProperties ) );
	}

	if (InBlueprint && 
		InBlueprint->ParentClass &&
		InBlueprint->ParentClass->IsChildOf(AActor::StaticClass()) && 
		InBlueprint->SimpleConstructionScript )
	{
		CreateSCSEditors();
	}
}

void FBlueprintEditor::CreateSCSEditors()
{
	SCSEditor = SAssignNew(SCSEditor, SSCSEditor)
		.ActorContext(this, &FBlueprintEditor::GetSCSEditorActorContext)
		.PreviewActor(this, &FBlueprintEditor::GetPreviewActor)
		.AllowEditing(this, &FBlueprintEditor::InEditingMode)
		.OnSelectionUpdated(this, &FBlueprintEditor::OnSelectionUpdated)
		.OnItemDoubleClicked(this, &FBlueprintEditor::OnComponentDoubleClicked);

	SCSViewport = SAssignNew(SCSViewport, SSCSEditorViewport)
		.BlueprintEditor(SharedThis(this));
}

void FBlueprintEditor::OnLogTokenClicked(const TSharedRef<IMessageToken>& Token)
{
	if (Token->GetType() == EMessageToken::Object)
	{
		const TSharedRef<FUObjectToken> UObjectToken = StaticCastSharedRef<FUObjectToken>(Token);
		if(UObjectToken->GetObject().IsValid())
		{
			JumpToHyperlink(UObjectToken->GetObject().Get());
		}
	}
	else if (Token->GetType() == EMessageToken::EdGraph)
	{
		const TSharedRef<FEdGraphToken> EdGraphToken = StaticCastSharedRef<FEdGraphToken>(Token);
		const UEdGraphPin* PinBeingReferenced = EdGraphToken->GetPin();
		const UObject* ObjectBeingReferenced = EdGraphToken->GetGraphObject();
		if (PinBeingReferenced)
		{
			JumpToPin(PinBeingReferenced);
		}
		else if(ObjectBeingReferenced)
		{
			JumpToHyperlink(ObjectBeingReferenced);
		}
	}
}

/** Create Default Commands **/
void FBlueprintEditor::CreateDefaultCommands()
{
	// Tell Kismet2 how to handle all the UI actions that it can handle
	// @todo: remove this once GraphEditorActions automatically register themselves.

	FGraphEditorCommands::Register();
	FBlueprintEditorCommands::Register();
	FFullBlueprintEditorCommands::Register();
	FMyBlueprintCommands::Register();
	FBlueprintSpawnNodeCommands::Register();

	static const FName BpEditorModuleName("Kismet");
	FBlueprintEditorModule& BlueprintEditorModule = FModuleManager::LoadModuleChecked<FBlueprintEditorModule>(BpEditorModuleName);
	ToolkitCommands->Append(BlueprintEditorModule.GetsSharedBlueprintEditorCommands());

	ToolkitCommands->MapAction(
		FFullBlueprintEditorCommands::Get().Compile,
		FExecuteAction::CreateSP(this, &FBlueprintEditor::Compile),
		FCanExecuteAction::CreateSP(this, &FBlueprintEditor::IsCompilingEnabled));

	TWeakPtr<FBlueprintEditor> WeakThisPtr = SharedThis(this);
	ToolkitCommands->MapAction(
		FFullBlueprintEditorCommands::Get().SaveOnCompile_Never,
		FExecuteAction::CreateStatic(&BlueprintEditorImpl::SetSaveOnCompileSetting, (ESaveOnCompile)SoC_Never),
		FCanExecuteAction::CreateSP(this, &FBlueprintEditor::IsSaveOnCompileEnabled),
		FIsActionChecked::CreateStatic(&BlueprintEditorImpl::IsSaveOnCompileOptionSet, WeakThisPtr, (ESaveOnCompile)SoC_Never)
	);
	ToolkitCommands->MapAction(
		FFullBlueprintEditorCommands::Get().SaveOnCompile_SuccessOnly,
		FExecuteAction::CreateStatic(&BlueprintEditorImpl::SetSaveOnCompileSetting, (ESaveOnCompile)SoC_SuccessOnly),
		FCanExecuteAction::CreateSP(this, &FBlueprintEditor::IsSaveOnCompileEnabled),
		FIsActionChecked::CreateStatic(&BlueprintEditorImpl::IsSaveOnCompileOptionSet, WeakThisPtr, (ESaveOnCompile)SoC_SuccessOnly)
	);
	ToolkitCommands->MapAction(
		FFullBlueprintEditorCommands::Get().SaveOnCompile_Always,
		FExecuteAction::CreateStatic(&BlueprintEditorImpl::SetSaveOnCompileSetting, (ESaveOnCompile)SoC_Always),
		FCanExecuteAction::CreateSP(this, &FBlueprintEditor::IsSaveOnCompileEnabled),
		FIsActionChecked::CreateStatic(&BlueprintEditorImpl::IsSaveOnCompileOptionSet, WeakThisPtr, (ESaveOnCompile)SoC_Always)
	);

	ToolkitCommands->MapAction(
		FFullBlueprintEditorCommands::Get().JumpToErrorNode,
		FExecuteAction::CreateStatic(&BlueprintEditorImpl::ToggleJumpToErrorNodeSetting),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic(&BlueprintEditorImpl::IsJumpToErrorNodeOptionSet)
	);
	
	ToolkitCommands->MapAction(
		FFullBlueprintEditorCommands::Get().SwitchToScriptingMode,
		FExecuteAction::CreateSP(this, &FBlueprintEditor::SetCurrentMode, FBlueprintEditorApplicationModes::StandardBlueprintEditorMode),
		FCanExecuteAction::CreateSP(this, &FBlueprintEditor::IsEditingSingleBlueprint),
		FIsActionChecked::CreateSP(this, &FBlueprintEditor::IsModeCurrent, FBlueprintEditorApplicationModes::StandardBlueprintEditorMode));

	ToolkitCommands->MapAction(
		FFullBlueprintEditorCommands::Get().SwitchToBlueprintDefaultsMode,
		FExecuteAction::CreateSP(this, &FBlueprintEditor::SetCurrentMode, FBlueprintEditorApplicationModes::BlueprintDefaultsMode),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FBlueprintEditor::IsModeCurrent, FBlueprintEditorApplicationModes::BlueprintDefaultsMode));
	
	ToolkitCommands->MapAction(
		FFullBlueprintEditorCommands::Get().SwitchToComponentsMode,
		FExecuteAction::CreateSP(this, &FBlueprintEditor::SetCurrentMode, FBlueprintEditorApplicationModes::BlueprintComponentsMode),
		FCanExecuteAction::CreateSP(this, &FBlueprintEditor::CanAccessComponentsMode),
		FIsActionChecked::CreateSP(this, &FBlueprintEditor::IsModeCurrent, FBlueprintEditorApplicationModes::BlueprintComponentsMode));

	ToolkitCommands->MapAction(
		FFullBlueprintEditorCommands::Get().EditGlobalOptions,
		FExecuteAction::CreateSP(this, &FBlueprintEditor::EditGlobalOptions_Clicked),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FBlueprintEditor::IsDetailsPanelEditingGlobalOptions));

	ToolkitCommands->MapAction(
		FFullBlueprintEditorCommands::Get().EditClassDefaults,
		FExecuteAction::CreateSP(this, &FBlueprintEditor::EditClassDefaults_Clicked),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FBlueprintEditor::IsDetailsPanelEditingClassDefaults));

	// Edit menu actions
	ToolkitCommands->MapAction( FBlueprintEditorCommands::Get().FindInBlueprint,
		FExecuteAction::CreateSP(this, &FBlueprintEditor::FindInBlueprint_Clicked),
		FCanExecuteAction::CreateSP( this, &FBlueprintEditor::IsInAScriptingMode )
		);

	ToolkitCommands->MapAction( FBlueprintEditorCommands::Get().ReparentBlueprint,
		FExecuteAction::CreateSP(this, &FBlueprintEditor::ReparentBlueprint_Clicked),
		FCanExecuteAction(),
		FIsActionChecked(),
		FIsActionButtonVisible::CreateSP(this, &FBlueprintEditor::ReparentBlueprint_IsVisible)
		);

	ToolkitCommands->MapAction( FGenericCommands::Get().Undo,
		FExecuteAction::CreateSP( this, &FBlueprintEditor::UndoGraphAction ),
		FCanExecuteAction::CreateSP( this, &FBlueprintEditor::CanUndoGraphAction )
		);

	ToolkitCommands->MapAction( FGenericCommands::Get().Redo,
		FExecuteAction::CreateSP( this, &FBlueprintEditor::RedoGraphAction ),
		FCanExecuteAction::CreateSP( this, &FBlueprintEditor::CanRedoGraphAction )
		);


	// View commands
	ToolkitCommands->MapAction( FBlueprintEditorCommands::Get().ZoomToWindow,
		FExecuteAction::CreateSP( this, &FBlueprintEditor::ZoomToWindow_Clicked ),
		FCanExecuteAction::CreateSP( this, &FBlueprintEditor::CanZoomToWindow )
		);

	ToolkitCommands->MapAction( FBlueprintEditorCommands::Get().ZoomToSelection,
		FExecuteAction::CreateSP( this, &FBlueprintEditor::ZoomToSelection_Clicked ),
		FCanExecuteAction::CreateSP( this, &FBlueprintEditor::CanZoomToSelection )
		);

	ToolkitCommands->MapAction( FBlueprintEditorCommands::Get().NavigateToParent,
		FExecuteAction::CreateSP( this, &FBlueprintEditor::NavigateToParentGraph_Clicked ),
		FCanExecuteAction::CreateSP( this, &FBlueprintEditor::CanNavigateToParentGraph )
		);

	ToolkitCommands->MapAction( FBlueprintEditorCommands::Get().NavigateToParentBackspace,
		FExecuteAction::CreateSP( this, &FBlueprintEditor::NavigateToParentGraph_Clicked ),
		FCanExecuteAction::CreateSP( this, &FBlueprintEditor::CanNavigateToParentGraph )
		);

	ToolkitCommands->MapAction( FBlueprintEditorCommands::Get().NavigateToChild,
		FExecuteAction::CreateSP( this, &FBlueprintEditor::NavigateToChildGraph_Clicked ),
		FCanExecuteAction::CreateSP( this, &FBlueprintEditor::CanNavigateToChildGraph )
		);

	ToolkitCommands->MapAction(FGraphEditorCommands::Get().ShowAllPins,
		FExecuteAction::CreateSP(this, &FBlueprintEditor::SetPinVisibility, SGraphEditor::Pin_Show),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FBlueprintEditor::GetPinVisibility, SGraphEditor::Pin_Show));

	ToolkitCommands->MapAction(FGraphEditorCommands::Get().HideNoConnectionPins,
		FExecuteAction::CreateSP(this, &FBlueprintEditor::SetPinVisibility, SGraphEditor::Pin_HideNoConnection),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FBlueprintEditor::GetPinVisibility, SGraphEditor::Pin_HideNoConnection));

	ToolkitCommands->MapAction(FGraphEditorCommands::Get().HideNoConnectionNoDefaultPins,
		FExecuteAction::CreateSP(this, &FBlueprintEditor::SetPinVisibility, SGraphEditor::Pin_HideNoConnectionNoDefault),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FBlueprintEditor::GetPinVisibility, SGraphEditor::Pin_HideNoConnectionNoDefault));

	// Compile
	ToolkitCommands->MapAction( FBlueprintEditorCommands::Get().CompileBlueprint,
		FExecuteAction::CreateSP(this, &FBlueprintEditor::Compile),
		FCanExecuteAction::CreateSP(this, &FBlueprintEditor::IsCompilingEnabled)
		);

	ToolkitCommands->MapAction( FBlueprintEditorCommands::Get().RefreshAllNodes,
		FExecuteAction::CreateSP(this, &FBlueprintEditor::RefreshAllNodes_OnClicked),
		FCanExecuteAction::CreateSP( this, &FBlueprintEditor::IsInAScriptingMode )
		);

	ToolkitCommands->MapAction( FBlueprintEditorCommands::Get().DeleteUnusedVariables,
		FExecuteAction::CreateSP(this, &FBlueprintEditor::DeleteUnusedVariables_OnClicked),
		FCanExecuteAction::CreateSP( this, &FBlueprintEditor::IsInAScriptingMode )
		);
	
	ToolkitCommands->MapAction( FBlueprintEditorCommands::Get().FindInBlueprints,
		FExecuteAction::CreateSP(this, &FBlueprintEditor::FindInBlueprints_OnClicked)
		);

	// Debug actions
	ToolkitCommands->MapAction( FBlueprintEditorCommands::Get().ClearAllBreakpoints,
		FExecuteAction::CreateSP(this, &FBlueprintEditor::ClearAllBreakpoints),
		FCanExecuteAction::CreateSP(this, &FBlueprintEditor::HasAnyBreakpoints)
		);

	ToolkitCommands->MapAction( FBlueprintEditorCommands::Get().DisableAllBreakpoints,
		FExecuteAction::CreateSP(this, &FBlueprintEditor::DisableAllBreakpoints),
		FCanExecuteAction::CreateSP(this, &FBlueprintEditor::HasAnyEnabledBreakpoints)
		);

	ToolkitCommands->MapAction( FBlueprintEditorCommands::Get().EnableAllBreakpoints,
		FExecuteAction::CreateSP(this, &FBlueprintEditor::EnableAllBreakpoints),
		FCanExecuteAction::CreateSP(this, &FBlueprintEditor::HasAnyDisabledBreakpoints)
		);

	ToolkitCommands->MapAction( FBlueprintEditorCommands::Get().ClearAllWatches,
		FExecuteAction::CreateSP(this, &FBlueprintEditor::ClearAllWatches),
		FCanExecuteAction::CreateSP(this, &FBlueprintEditor::HasAnyWatches)
		);

	// New document actions
	ToolkitCommands->MapAction( FBlueprintEditorCommands::Get().AddNewVariable,
		FExecuteAction::CreateSP(this, &FBlueprintEditor::OnAddNewVariable),
		FCanExecuteAction::CreateSP(this, &FBlueprintEditor::InEditingMode),
		FIsActionChecked(),
		FIsActionButtonVisible::CreateSP(this, &FBlueprintEditor::NewDocument_IsVisibleForType, CGT_NewVariable));
	
	ToolkitCommands->MapAction( FBlueprintEditorCommands::Get().AddNewLocalVariable,
		FExecuteAction::CreateSP(this, &FBlueprintEditor::OnAddNewLocalVariable),
		FCanExecuteAction::CreateSP(this, &FBlueprintEditor::CanAddNewLocalVariable),
		FIsActionChecked(),
		FIsActionButtonVisible::CreateSP(this, &FBlueprintEditor::NewDocument_IsVisibleForType, CGT_NewLocalVariable));

	ToolkitCommands->MapAction( FBlueprintEditorCommands::Get().AddNewFunction,
		FExecuteAction::CreateSP(this, &FBlueprintEditor::NewDocument_OnClicked, CGT_NewFunctionGraph),
		FCanExecuteAction::CreateSP(this, &FBlueprintEditor::InEditingMode),
		FIsActionChecked(),
		FIsActionButtonVisible::CreateSP(this, &FBlueprintEditor::NewDocument_IsVisibleForType, CGT_NewFunctionGraph));

	ToolkitCommands->MapAction( FBlueprintEditorCommands::Get().AddNewEventGraph,
		FExecuteAction::CreateSP(this, &FBlueprintEditor::NewDocument_OnClicked, CGT_NewEventGraph),
		FCanExecuteAction::CreateSP(this, &FBlueprintEditor::InEditingMode),
		FIsActionChecked(),
		FIsActionButtonVisible::CreateSP(this, &FBlueprintEditor::NewDocument_IsVisibleForType, CGT_NewEventGraph));

	ToolkitCommands->MapAction( FBlueprintEditorCommands::Get().AddNewMacroDeclaration,
		FExecuteAction::CreateSP(this, &FBlueprintEditor::NewDocument_OnClicked, CGT_NewMacroGraph),
		FCanExecuteAction::CreateSP(this, &FBlueprintEditor::InEditingMode),
		FIsActionChecked(),
		FIsActionButtonVisible::CreateSP(this, &FBlueprintEditor::NewDocument_IsVisibleForType, CGT_NewMacroGraph));

	ToolkitCommands->MapAction( FBlueprintEditorCommands::Get().AddNewDelegate,
		FExecuteAction::CreateSP(this, &FBlueprintEditor::OnAddNewDelegate),
		FCanExecuteAction::CreateSP(this, &FBlueprintEditor::InEditingMode),
		FIsActionChecked(),
		FIsActionButtonVisible::CreateSP(this, &FBlueprintEditor::AddNewDelegateIsVisible));

	ToolkitCommands->MapAction( FBlueprintEditorCommands::Get().FindReferencesFromClass,
		FExecuteAction::CreateSP(this, &FBlueprintEditor::OnListObjectsReferencedByClass),
		FCanExecuteAction());

	ToolkitCommands->MapAction( FBlueprintEditorCommands::Get().FindReferencesFromBlueprint,
		FExecuteAction::CreateSP(this, &FBlueprintEditor::OnListObjectsReferencedByBlueprint),
		FCanExecuteAction());

	ToolkitCommands->MapAction( FBlueprintEditorCommands::Get().RepairCorruptedBlueprint,
		FExecuteAction::CreateSP(this, &FBlueprintEditor::OnRepairCorruptedBlueprint),
		FCanExecuteAction());

	/*
	ToolkitCommands->MapAction( FBlueprintEditorCommands::Get().AddNewAnimationGraph,
		FExecuteAction::CreateSP(this, &FBlueprintEditor::NewDocument_OnClicked, CGT_NewAnimationGraph),
		FCanExecuteAction(),
		FIsActionChecked(),
		FIsActionButtonVisible::CreateSP(this, &FBlueprintEditor::NewDocument_IsVisibleForType, CGT_NewAnimationGraph));
	*/
	
	ToolkitCommands->MapAction(FBlueprintEditorCommands::Get().SaveIntermediateBuildProducts,
		FExecuteAction::CreateSP(this, &FBlueprintEditor::ToggleSaveIntermediateBuildProducts),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FBlueprintEditor::GetSaveIntermediateBuildProducts));

	ToolkitCommands->MapAction(FBlueprintEditorCommands::Get().BeginBlueprintMerge,
		FExecuteAction::CreateSP(this, &FBlueprintEditor::CreateMergeToolTab),
		FCanExecuteAction());

	ToolkitCommands->MapAction(FBlueprintEditorCommands::Get().GenerateNativeCode,
		FExecuteAction::CreateSP(this, &FBlueprintEditor::OpenNativeCodeGenerationTool),
		FCanExecuteAction::CreateSP(this, &FBlueprintEditor::CanGenerateNativeCode));

	ToolkitCommands->MapAction(FBlueprintEditorCommands::Get().ShowActionMenuItemSignatures,
		FExecuteAction::CreateLambda([]()
			{ 
				UBlueprintEditorSettings* Settings = GetMutableDefault<UBlueprintEditorSettings>();
				Settings->bShowActionMenuItemSignatures = !Settings->bShowActionMenuItemSignatures;
				Settings->SaveConfig();
			}),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([]()->bool{ return GetDefault<UBlueprintEditorSettings>()->bShowActionMenuItemSignatures; }));
}

void FBlueprintEditor::OpenNativeCodeGenerationTool()
{
	UBlueprint* Blueprint = GetBlueprintObj();
	if (Blueprint)
	{
		FNativeCodeGenerationTool::Open(*Blueprint, SharedThis(this));
	}
}

bool FBlueprintEditor::CanGenerateNativeCode() const
{
	UBlueprint* Blueprint = GetBlueprintObj();
	return Blueprint && FNativeCodeGenerationTool::CanGenerate(*Blueprint);
}

void FBlueprintEditor::FindInBlueprint_Clicked()
{
	SummonSearchUI(true);
}

void FBlueprintEditor::ReparentBlueprint_Clicked()
{
	TArray<UBlueprint*> Blueprints;
	for (int32 i = 0; i < GetEditingObjects().Num(); ++i)
	{
		UBlueprint* Blueprint = Cast<UBlueprint>(GetEditingObjects()[i]);
		if (Blueprint) 
		{
			Blueprints.Add(Blueprint);
		}
	}
	FBlueprintEditorUtils::OpenReparentBlueprintMenu(Blueprints, GetToolkitHost()->GetParentWidget(), FOnClassPicked::CreateSP(this, &FBlueprintEditor::ReparentBlueprint_NewParentChosen));
}

void FBlueprintEditor::ReparentBlueprint_NewParentChosen(UClass* ChosenClass)
{
	UBlueprint* BlueprintObj = GetBlueprintObj();

	if ((BlueprintObj != NULL) && (ChosenClass != NULL) && (ChosenClass != BlueprintObj->ParentClass))
	{
		// Notify user, about common interfaces
		bool bReparent = true;
		{
			FString CommonInterfacesNames;
			for (const FBPInterfaceDescription& InterdaceDesc : BlueprintObj->ImplementedInterfaces)
			{
				if (ChosenClass->ImplementsInterface(*InterdaceDesc.Interface))
				{
					CommonInterfacesNames += InterdaceDesc.Interface->GetName();
					CommonInterfacesNames += TCHAR('\n');
				}
			}
			if (!CommonInterfacesNames.IsEmpty())
			{
				const FText Title = LOCTEXT("CommonInterfacesTitle", "Common interfaces");
				const FText Message = FText::Format(
					LOCTEXT("ReparentWarning_InterfacesImplemented", "Following interfaces are already implemented. Continue reparenting? \n {0}"),
					FText::FromString(CommonInterfacesNames));

				FSuppressableWarningDialog::FSetupInfo Info(Message, Title, "Warning_CommonInterfacesWhileReparenting");
				Info.ConfirmText = LOCTEXT("ReparentYesButton", "Reparent");
				Info.CancelText = LOCTEXT("ReparentNoButton", "Cancel");

				FSuppressableWarningDialog ReparentBlueprintDlg(Info);
				if (ReparentBlueprintDlg.ShowModal() == FSuppressableWarningDialog::Cancel)
				{
					bReparent = false;
				}
			}
		}

		// If the chosen class differs hierarchically from the current class, warn that there may be data loss
		if (bReparent && (!BlueprintObj->ParentClass || !ChosenClass->GetDefaultObject()->IsA(BlueprintObj->ParentClass)))
		{
			const FText Title = LOCTEXT("ReparentTitle", "Reparent Blueprint"); 
			const FText Message = LOCTEXT("ReparentWarning", "Reparenting this blueprint may cause data loss.  Continue reparenting?"); 

			// Warn the user that this may result in data loss
			FSuppressableWarningDialog::FSetupInfo Info( Message, Title, "Warning_ReparentTitle" );
			Info.ConfirmText = LOCTEXT("ReparentYesButton", "Reparent");
			Info.CancelText = LOCTEXT("ReparentNoButton", "Cancel");
			Info.CheckBoxText = FText::GetEmpty();	// not suppressible

			FSuppressableWarningDialog ReparentBlueprintDlg( Info );
			if( ReparentBlueprintDlg.ShowModal() == FSuppressableWarningDialog::Cancel )
			{
				bReparent = false;
			}
		}

		if ( bReparent )
		{
			UE_LOG(LogBlueprint, Warning, TEXT("Reparenting blueprint %s from %s to %s..."), *BlueprintObj->GetFullName(), BlueprintObj->ParentClass ? *BlueprintObj->ParentClass->GetName() : TEXT("[None]"), *ChosenClass->GetName());

			UClass* OldParentClass = BlueprintObj->ParentClass ;
			BlueprintObj->ParentClass = ChosenClass;

			// Ensure that the Blueprint is up-to-date (valid SCS etc.) before compiling
			EnsureBlueprintIsUpToDate(BlueprintObj);
			FBlueprintEditorUtils::RefreshAllNodes(GetBlueprintObj());
			FBlueprintEditorUtils::MarkBlueprintAsModified(BlueprintObj);

			Compile();

			// Ensure that the Blueprint is up-to-date (valid SCS etc.) after compiling (new parent class)
			EnsureBlueprintIsUpToDate(BlueprintObj);

			if (BlueprintObj->NativizationFlag != EBlueprintNativizationFlag::Disabled)
			{
				UBlueprint* ParentBlueprint = UBlueprint::GetBlueprintFromClass(ChosenClass);
				if (ParentBlueprint && ParentBlueprint->NativizationFlag == EBlueprintNativizationFlag::Disabled)
				{
					ParentBlueprint->NativizationFlag = EBlueprintNativizationFlag::Dependency;

					FNotificationInfo Warning(FText::Format(
						LOCTEXT("InterfaceFlaggedForNativization", "{0} flagged for nativization (as a required dependency)."),
						FText::FromName(ParentBlueprint->GetFName())
						)
					);
					Warning.ExpireDuration = 5.0f;
					Warning.bFireAndForget = true;
					Warning.Image = FCoreStyle::Get().GetBrush(TEXT("MessageLog.Warning"));
					FSlateNotificationManager::Get().AddNotification(Warning);
				}
			}

			if (SCSEditor.IsValid())
			{
				SCSEditor->UpdateTree();
			}
		}
	}

	FSlateApplication::Get().DismissAllMenus();
}

bool FBlueprintEditor::ReparentBlueprint_IsVisible() const
{
	UBlueprint* Blueprint = GetBlueprintObj();
	if (Blueprint != NULL)
	{
		// Don't show the reparent option if it's an Interface or we're not in editing mode
		return !FBlueprintEditorUtils::IsInterfaceBlueprint(Blueprint) && InEditingMode() && (BPTYPE_FunctionLibrary != Blueprint->BlueprintType);
	}
	else
	{
		return false;
	}
}

bool FBlueprintEditor::IsDetailsPanelEditingGlobalOptions() const
{
	return CurrentUISelection == FBlueprintEditor::SelectionState_ClassSettings;
}

void FBlueprintEditor::EditGlobalOptions_Clicked()
{
	UBlueprint* Blueprint = GetBlueprintObj();
	if ( Blueprint != nullptr )
	{
		SetUISelectionState(FBlueprintEditor::SelectionState_ClassSettings);

		// Show details for the Blueprint instance we're editing
		Inspector->ShowDetailsForSingleObject(Blueprint);

		TryInvokingDetailsTab();
	}
}

bool FBlueprintEditor::IsDetailsPanelEditingClassDefaults() const
{
	UBlueprint* Blueprint = GetBlueprintObj();
	if ( Blueprint != nullptr )
	{
		if ( Blueprint->GeneratedClass != nullptr )
		{
			UObject* DefaultObject = GetBlueprintObj()->GeneratedClass->GetDefaultObject();
			return Inspector->IsSelected(DefaultObject);
		}
	}

	return false;
}

void FBlueprintEditor::EditClassDefaults_Clicked()
{
	if ( IsEditingSingleBlueprint() )
	{
		UBlueprint* Blueprint = GetBlueprintObj();
		StartEditingDefaults( true, true );
	}
}

// Zooming to fit the entire graph
void FBlueprintEditor::ZoomToWindow_Clicked()
{
	if (SGraphEditor* GraphEd = FocusedGraphEdPtr.Pin().Get())
	{
		GraphEd->ZoomToFit(/*bOnlySelection=*/ false);
	}
}

bool FBlueprintEditor::CanZoomToWindow() const
{
	return FocusedGraphEdPtr.IsValid();
}

// Zooming to fit the current selection
void FBlueprintEditor::ZoomToSelection_Clicked()
{
	if (SGraphEditor* GraphEd = FocusedGraphEdPtr.Pin().Get())
	{
		GraphEd->ZoomToFit(/*bOnlySelection=*/ true);
	}
}

bool FBlueprintEditor::CanZoomToSelection() const
{
	return FocusedGraphEdPtr.IsValid();
}

// Navigating into/out of graphs
void FBlueprintEditor::NavigateToParentGraph_Clicked()
{
	if (FocusedGraphEdPtr.IsValid())
	{
		if (UEdGraph* ParentGraph = Cast<UEdGraph>(FocusedGraphEdPtr.Pin()->GetCurrentGraph()->GetOuter()))
		{
			OpenDocument(ParentGraph, FDocumentTracker::NavigatingCurrentDocument);
		}
	}
}

bool FBlueprintEditor::CanNavigateToParentGraph() const
{
	return FocusedGraphEdPtr.IsValid() && (FocusedGraphEdPtr.Pin()->GetCurrentGraph()->GetOuter()->IsA(UEdGraph::StaticClass()));
}

void FBlueprintEditor::NavigateToChildGraph_Clicked()
{
	if (FocusedGraphEdPtr.IsValid())
	{
		UEdGraph* CurrentGraph = FocusedGraphEdPtr.Pin()->GetCurrentGraph();

		if (CurrentGraph->SubGraphs.Num() > 1)
		{
			// Display a child jump list
			FSlateApplication::Get().PushMenu( 
				GetToolkitHost()->GetParentWidget(),
				FWidgetPath(),
				SNew(SChildGraphPicker, CurrentGraph),
				FSlateApplication::Get().GetCursorPos(), // summon location
				FPopupTransitionEffect( FPopupTransitionEffect::TypeInPopup )
			);
		}
		else if (CurrentGraph->SubGraphs.Num() == 1)
		{
			// Jump immediately to the child if there is only one
			UEdGraph* ChildGraph = CurrentGraph->SubGraphs[0];
			OpenDocument(ChildGraph, FDocumentTracker::NavigatingCurrentDocument);
		}
	}
}

bool FBlueprintEditor::CanNavigateToChildGraph() const
{
	return FocusedGraphEdPtr.IsValid() && (FocusedGraphEdPtr.Pin()->GetCurrentGraph()->SubGraphs.Num() > 0);
}

void FBlueprintEditor::HandleUndoTransaction(const FTransaction* Transaction)
{
	UBlueprint* BlueprintObj = GetBlueprintObj();
	if (BlueprintObj && Transaction)
	{
		bool bAffectsBlueprint = false;
		const UPackage* BlueprintOutermost = BlueprintObj->GetOutermost();

		// Look at the transaction this function is responding to, see if any object in it has an outermost of the Blueprint
		TArray<UObject*> TransactionObjects;
		Transaction->GetTransactionObjects(TransactionObjects);
		for (UObject* Object : TransactionObjects)
		{
			if (Object->GetOutermost() == BlueprintOutermost)
			{
				bAffectsBlueprint = true;
				break;
			}
		}

		// Transaction affects the Blueprint this editor handles, so react as necessary
		if (bAffectsBlueprint)
		{
			SetUISelectionState(NAME_None);

			RefreshEditors();

			FSlateApplication::Get().DismissAllMenus();
		}
	}
}

void FBlueprintEditor::PostUndo(bool bSuccess)
{	
	if (bSuccess)
	{
		const FTransaction* Transaction = GEditor->Trans->GetTransaction(GEditor->Trans->GetQueueLength() - GEditor->Trans->GetUndoCount());
		HandleUndoTransaction(Transaction);
	}
}

void FBlueprintEditor::PostRedo(bool bSuccess)
{
	if (bSuccess)
	{
		const FTransaction* Transaction = GEditor->Trans->GetTransaction(GEditor->Trans->GetQueueLength() - GEditor->Trans->GetUndoCount() - 1);
		HandleUndoTransaction(Transaction);
	}
}

void FBlueprintEditor::UndoGraphAction()
{
	GEditor->UndoTransaction();
}

bool FBlueprintEditor::CanUndoGraphAction() const
{
	return !InDebuggingMode();
}

void FBlueprintEditor::RedoGraphAction()
{
	GEditor->RedoTransaction();
}

bool FBlueprintEditor::CanRedoGraphAction() const
{
	return !InDebuggingMode();
}

void FBlueprintEditor::OnActiveTabChanged(TSharedPtr<SDockTab> PreviouslyActive, TSharedPtr<SDockTab> NewlyActivated)
{
}

void FBlueprintEditor::OnGraphEditorFocused(const TSharedRef<SGraphEditor>& InGraphEditor)
{
	// Update the graph editor that is currently focused
	FocusedGraphEdPtr = InGraphEditor;
	InGraphEditor->SetPinVisibility(PinVisibility);

	// Update the inspector as well, to show selection from the focused graph editor
	FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	FocusInspectorOnGraphSelection(SelectedNodes);

	// During undo, garbage graphs can be temporarily brought into focus, ensure that before a refresh of the MyBlueprint window that the graph is owned by a Blueprint
	if ( FocusedGraphEdPtr.IsValid() && MyBlueprintWidget.IsValid() )
	{
		// The focused graph can be garbage as well
		TWeakObjectPtr< UEdGraph > FocusedGraphPtr = FocusedGraphEdPtr.Pin()->GetCurrentGraph();
		UEdGraph* FocusedGraph = FocusedGraphPtr.Get();

		if ( FocusedGraph != nullptr )
		{
			if ( FBlueprintEditorUtils::FindBlueprintForGraph(FocusedGraph) )
			{
				MyBlueprintWidget->Refresh();
			}
		}
	}
}

void FBlueprintEditor::OnGraphEditorBackgrounded(const TSharedRef<SGraphEditor>& InGraphEditor)
{
	// If the newly active document tab isn't a graph we want to make sure we clear the focused graph pointer.
	// Several other UI reads that, like the MyBlueprints view uses it to determine if it should show the "Local Variable" section.
	FocusedGraphEdPtr = nullptr;

	if ( MyBlueprintWidget.IsValid() == true )
	{
		MyBlueprintWidget->Refresh();
	}
}

void FBlueprintEditor::OnGraphEditorDropActor(const TArray< TWeakObjectPtr<AActor> >& Actors, UEdGraph* Graph, const FVector2D& DropLocation)
{
	// We need to check that the dropped actor is in the right sublevel for the reference
	ULevel* BlueprintLevel = FBlueprintEditorUtils::GetLevelFromBlueprint(GetBlueprintObj());

	if (BlueprintLevel && FBlueprintEditorUtils::IsLevelScriptBlueprint(GetBlueprintObj()))
	{
		FVector2D NodeLocation = DropLocation;
		for (int32 i = 0; i < Actors.Num(); i++)
		{
			AActor* DroppedActor = Actors[i].Get();
			if (DroppedActor&& (DroppedActor->GetLevel() == BlueprintLevel) && !DroppedActor->IsChildActor())
			{
				UK2Node_Literal* LiteralNodeTemplate = NewObject<UK2Node_Literal>();
				LiteralNodeTemplate->SetObjectRef(DroppedActor);

				UK2Node_Literal* ActorRefNode = FEdGraphSchemaAction_K2NewNode::SpawnNodeFromTemplate<UK2Node_Literal>(Graph, LiteralNodeTemplate, NodeLocation);
				NodeLocation.Y += UEdGraphSchema_K2::EstimateNodeHeight(ActorRefNode);
			}
		}
	}
}

void FBlueprintEditor::OnGraphEditorDropStreamingLevel(const TArray< TWeakObjectPtr<ULevelStreaming> >& Levels, UEdGraph* Graph, const FVector2D& DropLocation)
{
	UFunction* TargetFunc = UGameplayStatics::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UGameplayStatics, GetStreamingLevel));
	check(TargetFunc);

	for (int32 i = 0; i < Levels.Num(); i++)
	{
		ULevelStreaming* DroppedLevel = Levels[i].Get();
		if ((DroppedLevel != NULL) && 
			(DroppedLevel->IsA(ULevelStreamingKismet::StaticClass()))) 
		{
			const FVector2D NodeLocation = DropLocation + (i * FVector2D(0,80));
				
			UK2Node_CallFunction* NodeTemplate = NewObject<UK2Node_CallFunction>(Graph);
			NodeTemplate->SetFromFunction(TargetFunc);
			UK2Node_CallFunction* Node = FEdGraphSchemaAction_K2NewNode::SpawnNodeFromTemplate<UK2Node_CallFunction>(Graph, NodeTemplate, NodeLocation);
						
			// Set dropped level package name
			UEdGraphPin* PackageNameInputPin = Node->FindPinChecked(TEXT("PackageName"));
			PackageNameInputPin->DefaultValue = DroppedLevel->GetWorldAssetPackageName();
		}
	}
}

FActionMenuContent FBlueprintEditor::OnCreateGraphActionMenu(UEdGraph* InGraph, const FVector2D& InNodePosition, const TArray<UEdGraphPin*>& InDraggedPins, bool bAutoExpand, SGraphEditor::FActionMenuClosed InOnMenuClosed)
{
	HasOpenActionMenu = InGraph;
	if (!BlueprintEditorImpl::GraphHasUserPlacedNodes(InGraph))
	{
		InstructionsFadeCountdown = BlueprintEditorImpl::InstructionFadeDuration;
	}

	TSharedRef<SBlueprintActionMenu> ActionMenu = 
		SNew(SBlueprintActionMenu, SharedThis(this))
		.GraphObj(InGraph)
		.NewNodePosition(InNodePosition)
		.DraggedFromPins(InDraggedPins)
		.AutoExpandActionMenu(bAutoExpand)
		.OnClosedCallback(InOnMenuClosed)
		.OnCloseReason(this, &FBlueprintEditor::OnGraphActionMenuClosed);

	return FActionMenuContent( ActionMenu, ActionMenu->GetFilterTextBox() );
}

void FBlueprintEditor::OnGraphActionMenuClosed(bool bActionExecuted, bool bContextSensitiveChecked, bool bGraphPinContext)
{
	if (bActionExecuted)
	{
		bContextSensitiveChecked ? AnalyticsStats.GraphActionMenusCtxtSensitiveExecCount++ : AnalyticsStats.GraphActionMenusNonCtxtSensitiveExecCount++;
		UpdateNodeCreationStats( bGraphPinContext ? ENodeCreateAction::PinContext : ENodeCreateAction::GraphContext );
	}
	else
	{
		AnalyticsStats.GraphActionMenusCancelledCount++;
	}

	if (UEdGraph* EditingGraph = GetFocusedGraph())
	{
		// if the user didn't place any nodes...
		if (!BlueprintEditorImpl::GraphHasUserPlacedNodes(EditingGraph))
		{
			InstructionsFadeCountdown = 0.0f;
		}
	}
	HasOpenActionMenu = nullptr;
}

void FBlueprintEditor::OnSelectedNodesChangedImpl(const FGraphPanelSelectionSet& NewSelection)
{
	if ( NewSelection.Num() > 0 )
	{
		SetUISelectionState(FBlueprintEditor::SelectionState_Graph);
	}

	Inspector->ShowDetailsForObjects(NewSelection.Array());
}

void FBlueprintEditor::OnBlueprintChangedImpl(UBlueprint* InBlueprint, bool bIsJustBeingCompiled )
{
	if (InBlueprint)
	{
		// Notify that the blueprint has been changed (update Content browser, etc)
		InBlueprint->PostEditChange();

		// Call PostEditChange() on any Actors that are based on this Blueprint 
		FBlueprintEditorUtils::PostEditChangeBlueprintActors(InBlueprint);

		// Refresh the graphs
		ERefreshBlueprintEditorReason::Type Reason = bIsJustBeingCompiled ? ERefreshBlueprintEditorReason::BlueprintCompiled : ERefreshBlueprintEditorReason::UnknownReason;
		RefreshEditors(Reason);

		// In case objects were deleted, which should close the tab
		if (GetCurrentMode() == FBlueprintEditorApplicationModes::StandardBlueprintEditorMode)
		{
			SaveEditedObjectState();
		}
	}
}

void FBlueprintEditor::OnBlueprintCompiled(UBlueprint* InBlueprint)
{	
	if( InBlueprint )
	{
		UUnrealEdEngine* EditorEngine = GUnrealEd;
		// GUnrealEd can be null after a hot-reload... this seems like a bigger 
		// problem worth investigating (that could affect other systems), but 
		// as I cannot repro it a second time (to see if it gets reset soon after), 
		// we'll just gaurd here for now and see if we can tie this ensure to any 
		// future crash reports
		if (ensure(EditorEngine != nullptr))
		{
			// Compiling will invalidate any cached components in the component visualizer, so clear out active components here
			EditorEngine->ComponentVisManager.ClearActiveComponentVis();
		}

		// This could be made more efficient by tracking which nodes change
		// their bHasCompilerMessage flag, or immediately updating the error info
		// when we assign the flag:
		TArray<UEdGraph*> Graphs;
		InBlueprint->GetAllGraphs(Graphs);
		for (const UEdGraph* Graph : Graphs)
		{
			for (const UEdGraphNode* Node : Graph->Nodes)
			{
				if (Node)
				{
					TSharedPtr<SGraphNode> Widget = Node->DEPRECATED_NodeWidget.Pin();
					if (Widget.IsValid())
					{
						Widget->RefreshErrorInfo();
					}
				}
			}
		}
	}

	OnBlueprintChangedImpl( InBlueprint, true );
}

void FBlueprintEditor::OnBlueprintUnloaded(UBlueprint* InBlueprint)
{
	for (UObject* EditingObj : GetEditingObjects())
	{
		if (Cast<UBlueprint>(EditingObj) == InBlueprint)
		{
			// give the editor a chance to open a replacement
			bPendingDeferredClose = true;
			break;
		}
	}
}

void FBlueprintEditor::Compile()
{
	UBlueprint* BlueprintObj = GetBlueprintObj();
	if (BlueprintObj)
	{
		FMessageLog BlueprintLog("BlueprintLog");

		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("BlueprintName"), FText::FromString(BlueprintObj->GetName()));
		BlueprintLog.NewPage(FText::Format(LOCTEXT("CompilationPageLabel", "Compile {BlueprintName}"), Arguments));

		FCompilerResultsLog LogResults;
		LogResults.SetSourcePath(BlueprintObj->GetPathName());
		LogResults.BeginEvent(TEXT("Compile"));
		LogResults.bLogDetailedResults = GetDefault<UBlueprintEditorSettings>()->bShowDetailedCompileResults;
		LogResults.EventDisplayThresholdMs = GetDefault<UBlueprintEditorSettings>()->CompileEventDisplayThresholdMs;
		EBlueprintCompileOptions CompileOptions = EBlueprintCompileOptions::None;
		if( bSaveIntermediateBuildProducts )
		{
			CompileOptions |= EBlueprintCompileOptions::SaveIntermediateProducts;
		}
		FKismetEditorUtilities::CompileBlueprint(BlueprintObj, CompileOptions, &LogResults);

		LogResults.EndEvent();

		const bool bForceMessageDisplay = ((LogResults.NumWarnings > 0) || (LogResults.NumErrors > 0)) && !BlueprintObj->bIsRegeneratingOnLoad;
		DumpMessagesToCompilerLog(LogResults.Messages, bForceMessageDisplay);

		UBlueprintEditorSettings const* BpEditorSettings = GetDefault<UBlueprintEditorSettings>();
		if ((LogResults.NumErrors > 0) && BpEditorSettings->bJumpToNodeErrors)
		{
			if (UEdGraphNode* NodeWithError = BlueprintEditorImpl::FindNodeWithError(LogResults))
			{
				JumpToNode(NodeWithError, /*bRequestRename =*/false);
			}
		}

		if (BlueprintObj->UpgradeNotesLog.IsValid())
		{
			CompilerResultsListing->AddMessages(BlueprintObj->UpgradeNotesLog->Messages);
		}

		AppendExtraCompilerResults(CompilerResultsListing);

		// send record when player clicks compile and send the result
		// this will make sure how the users activity is
		AnalyticsTrackCompileEvent(BlueprintObj, LogResults.NumErrors, LogResults.NumWarnings);
	}
}

bool FBlueprintEditor::IsSaveOnCompileEnabled() const
{
	UBlueprint* Blueprint = GetBlueprintObj();
	bool const bIsLevelScript = (Cast<ULevelScriptBlueprint>(Blueprint) != nullptr);

	return !bIsLevelScript;
}

FReply FBlueprintEditor::Compile_OnClickWithReply()
{
	Compile();
	return FReply::Handled();
}

void FBlueprintEditor::RefreshAllNodes_OnClicked()
{
	FBlueprintEditorUtils::RefreshAllNodes(GetBlueprintObj());
	RefreshEditors();
	Compile();
}

void FBlueprintEditor::DeleteUnusedVariables_OnClicked()
{
	UBlueprint* BlueprintObj = GetBlueprintObj();
	
	bool bHasAtLeastOneVariableToCheck = false;
	FString PropertyList;
	TArray<FName> VariableNames;
	for (TFieldIterator<UProperty> PropertyIt(BlueprintObj->SkeletonGeneratedClass, EFieldIteratorFlags::ExcludeSuper); PropertyIt; ++PropertyIt)
	{
		UProperty* Property = *PropertyIt;
		// Don't show delegate properties, there is special handling for these
		const bool bDelegateProp = Property->IsA(UDelegateProperty::StaticClass()) || Property->IsA(UMulticastDelegateProperty::StaticClass());
		const bool bShouldShowProp = (!Property->HasAnyPropertyFlags(CPF_Parm) && Property->HasAllPropertyFlags(CPF_BlueprintVisible) && !bDelegateProp);

		if (bShouldShowProp)
		{
			bHasAtLeastOneVariableToCheck = true;
			FName VarName = Property->GetFName();
			
			const int32 VarInfoIndex = FBlueprintEditorUtils::FindNewVariableIndex(BlueprintObj, VarName);
			const bool bHasVarInfo = (VarInfoIndex != INDEX_NONE);
			
			const UObjectPropertyBase* ObjectProperty = Cast<const UObjectPropertyBase>(Property);
			bool bIsTimeline = ObjectProperty &&
				ObjectProperty->PropertyClass &&
				ObjectProperty->PropertyClass->IsChildOf(UTimelineComponent::StaticClass());
			if (!FBlueprintEditorUtils::IsVariableUsed(BlueprintObj, VarName) && !bIsTimeline && bHasVarInfo)
			{
				VariableNames.Add(Property->GetFName());
				if (PropertyList.IsEmpty()) {PropertyList = UEditorEngine::GetFriendlyName(Property);}
				else {PropertyList += FString::Printf(TEXT(", %s"), *UEditorEngine::GetFriendlyName(Property));}
			}
		}
	}

	if (VariableNames.Num() > 0)
	{
		FBlueprintEditorUtils::BulkRemoveMemberVariables(GetBlueprintObj(), VariableNames);
		LogSimpleMessage( FText::Format( LOCTEXT("UnusedVariablesDeletedMessage", "The following variable(s) were deleted successfully: {0}."), FText::FromString( PropertyList ) ) );
	}
	else if (bHasAtLeastOneVariableToCheck)
	{
		LogSimpleMessage(LOCTEXT("AllVariablesInUseMessage", "All variables are currently in use."));
	}
	else
	{
		LogSimpleMessage(LOCTEXT("NoVariablesToSeeMessage", "No variables to check for."));
	}
}

void FBlueprintEditor::FindInBlueprints_OnClicked()
{
	SetCurrentMode(FBlueprintEditorApplicationModes::StandardBlueprintEditorMode);
	
	SummonSearchUI(false);
}

void FBlueprintEditor::ClearAllBreakpoints()
{
	FDebuggingActionCallbacks::ClearBreakpoints(GetBlueprintObj());
}

void FBlueprintEditor::DisableAllBreakpoints()
{
	FDebuggingActionCallbacks::SetEnabledOnAllBreakpoints(GetBlueprintObj(), false);
}

void FBlueprintEditor::EnableAllBreakpoints()
{
	FDebuggingActionCallbacks::SetEnabledOnAllBreakpoints(GetBlueprintObj(), true);
}

void FBlueprintEditor::ClearAllWatches()
{
	FDebuggingActionCallbacks::ClearWatches(GetBlueprintObj());
}

bool FBlueprintEditor::HasAnyBreakpoints() const
{
	return GetBlueprintObj() && GetBlueprintObj()->Breakpoints.Num() > 0;
}

bool FBlueprintEditor::HasAnyEnabledBreakpoints() const
{
	if (!IsEditingSingleBlueprint()) {return false;}

	for (TArray<UBreakpoint*>::TIterator BreakpointIt(GetBlueprintObj()->Breakpoints); BreakpointIt; ++BreakpointIt)
	{
		UBreakpoint* BP = *BreakpointIt;
		if (BP->IsEnabledByUser())
		{
			return true;
		}
	}

	return false;
}

bool FBlueprintEditor::HasAnyDisabledBreakpoints() const
{
	if (!IsEditingSingleBlueprint()) {return false;}

	for (TArray<UBreakpoint*>::TIterator BreakpointIt(GetBlueprintObj()->Breakpoints); BreakpointIt; ++BreakpointIt)
	{
		UBreakpoint* BP = *BreakpointIt;
		if (!BP->IsEnabledByUser())
		{
			return true;
		}
	}

	return false;
}

bool FBlueprintEditor::HasAnyWatches() const
{
	return GetBlueprintObj() && GetBlueprintObj()->WatchedPins.Num() > 0;
}

// Jumps to a hyperlinked node, pin, or graph, if it belongs to this blueprint
void FBlueprintEditor::JumpToHyperlink(const UObject* ObjectReference, bool bRequestRename)
{
	SetCurrentMode(FBlueprintEditorApplicationModes::StandardBlueprintEditorMode);

	if (const UEdGraphNode* Node = Cast<const UEdGraphNode>(ObjectReference))
	{
		if (bRequestRename)
		{
			IsNodeTitleVisible(Node, bRequestRename);
		}
		else
		{
			JumpToNode(Node, false);
		}
	}
	else if (const UEdGraph* Graph = Cast<const UEdGraph>(ObjectReference))
	{
		// Navigating into things should re-use the current tab when it makes sense
		FDocumentTracker::EOpenDocumentCause OpenMode = FDocumentTracker::OpenNewDocument;
		if ((Graph->GetSchema()->GetGraphType(Graph) == GT_Ubergraph) || Cast<UK2Node_Composite>(Graph->GetOuter()))
		{
			// Ubergraphs directly reuse the current graph
			OpenMode = FDocumentTracker::NavigatingCurrentDocument;
		}
		else
		{
			// Walk up the outer chain to see if any tabs have a parent of this document open for edit, and if so
			// we should reuse that one and drill in deeper instead
			for (UObject* WalkPtr = const_cast<UEdGraph*>(Graph); WalkPtr != nullptr; WalkPtr = WalkPtr->GetOuter())
			{
				TArray< TSharedPtr<SDockTab> > TabResults;
				if (FindOpenTabsContainingDocument(WalkPtr, /*out*/ TabResults))
				{
					// See if the parent was active
					bool bIsActive = false;
					for (TSharedPtr<SDockTab> Tab : TabResults)
					{
						if (Tab->IsActive())
						{
							bIsActive = true;
							break;
						}
					}

					if (bIsActive)
					{
						OpenMode = FDocumentTracker::NavigatingCurrentDocument;
						break;
					}
				}
			}
		}

		// Force it to open in a new document if shift is pressed
		const bool bIsShiftPressed = FSlateApplication::Get().GetModifierKeys().IsShiftDown();
		if (bIsShiftPressed)
		{
			OpenMode = FDocumentTracker::ForceOpenNewDocument;
		}

		// Open the document
		OpenDocument(Graph, OpenMode);
	}
	else if (const AActor* ReferencedActor = Cast<const AActor>(ObjectReference))
	{
		// Check if the world is active in the editor. It's possible to open level BPs without formally opening
		// the levels through Find-in-Blueprints
		bool bInOpenWorld = false;
		const TIndirectArray<FWorldContext>& WorldContextList = GEditor->GetWorldContexts();
		const UWorld* ReferencedActorOwningWorld = ReferencedActor->GetWorld();
		for (const FWorldContext& WorldContext : WorldContextList)
		{
			if (WorldContext.World() == ReferencedActorOwningWorld)
			{
				bInOpenWorld = true;
				break;
			}
		}

		// Clear the selection even if we couldn't find it, so the existing selection doesn't get mistaken for the desired to be selected actor
		GEditor->SelectNone(false, false);

		if (bInOpenWorld)
		{
			// Select the in-level actor
			GEditor->SelectActor(const_cast<AActor*>(ReferencedActor), true, true, true);

			// Point the camera at it
			GUnrealEd->Exec(ReferencedActor->GetWorld(), TEXT("CAMERA ALIGN ACTIVEVIEWPORTONLY"));
		}
	}
	else if(const UFunction* Function = Cast<const UFunction>(ObjectReference))
	{
		if (UEdGraph* FunctionGraph = FBlueprintEditorUtils::FindScopeGraph(GetBlueprintObj(), Function))
		{
			OpenDocument(FunctionGraph, FDocumentTracker::OpenNewDocument);
		}
	}
	else if(const UBlueprintGeneratedClass* Class = Cast<const UBlueprintGeneratedClass>(ObjectReference))
	{
		FAssetEditorManager::Get().OpenEditorForAsset(Class->ClassGeneratedBy);
	}
	else if (const UTimelineTemplate* Timeline = Cast<const UTimelineTemplate>(ObjectReference))
	{
		OpenDocument(Timeline, FDocumentTracker::OpenNewDocument);
	}
	else if ((ObjectReference != nullptr) && ObjectReference->IsAsset())
	{
		FAssetEditorManager::Get().OpenEditorForAsset(const_cast<UObject*>(ObjectReference));
	}
	else
	{
		UE_LOG(LogBlueprint, Warning, TEXT("Unknown type of hyperlinked object (%s), cannot focus it"), *GetNameSafe(ObjectReference));
	}

	//@TODO: Hacky way to ensure a message is seen when hitting an exception and doing intraframe debugging
	const FText ExceptionMessage = FKismetDebugUtilities::GetAndClearLastExceptionMessage();
	if (!ExceptionMessage.IsEmpty())
	{
		LogSimpleMessage( ExceptionMessage );
	}
}

void FBlueprintEditor::JumpToPin(const UEdGraphPin* Pin)
{
	if (!Pin->IsPendingKill())
	{
		// Open a graph editor and jump to the pin
		TSharedPtr<SGraphEditor> GraphEditor = OpenGraphAndBringToFront(Pin->GetOwningNode()->GetGraph());
		if (GraphEditor.IsValid())
		{
			GraphEditor->JumpToPin(Pin);
		}
	}
}

void FBlueprintEditor::AddReferencedObjects( FReferenceCollector& Collector )
{
	TArray<UObject*>& LocalEditingObjects = const_cast<TArray<UObject*>&>(GetEditingObjects());

	Collector.AddReferencedObjects(LocalEditingObjects);

	Collector.AddReferencedObjects(StandardLibraries);

	UserDefinedEnumerators.Remove(TWeakObjectPtr<UUserDefinedEnum>()); // Remove NULLs
	for (const TWeakObjectPtr<UUserDefinedEnum>& ObjectPtr : UserDefinedEnumerators)
	{
		if (UObject* Obj = ObjectPtr.Get())
		{
			Collector.AddReferencedObject(Obj);
		}
	}

	UserDefinedStructures.Remove(TWeakObjectPtr<UUserDefinedStruct>()); // Remove NULLs
	for (const TWeakObjectPtr<UUserDefinedStruct>& ObjectPtr : UserDefinedStructures)
	{
		if (UObject* Obj = ObjectPtr.Get())
		{
			Collector.AddReferencedObject(Obj);
		}
	}
}

bool FBlueprintEditor::IsNodeTitleVisible(const UEdGraphNode* Node, bool bRequestRename)
{
	TSharedPtr<SGraphEditor> GraphEditor;
	if(bRequestRename)
	{
		// If we are renaming, the graph will be open already, just grab the tab and it's content and jump to the node.
		TSharedPtr<SDockTab> ActiveTab = DocumentManager->GetActiveTab();
		check(ActiveTab.IsValid());
		GraphEditor = StaticCastSharedRef<SGraphEditor>(ActiveTab->GetContent());
	}
	else
	{
		// Open a graph editor and jump to the node
		GraphEditor = OpenGraphAndBringToFront(Node->GetGraph());
	}

	bool bVisible = false;
	if (GraphEditor.IsValid())
	{
		bVisible = GraphEditor->IsNodeTitleVisible(Node, bRequestRename);
	}
	return bVisible;
}

void FBlueprintEditor::JumpToNode(const UEdGraphNode* Node, bool bRequestRename)
{
	TSharedPtr<SGraphEditor> GraphEditor;
	if(bRequestRename)
	{
		// If we are renaming, the graph will be open already, just grab the tab and it's content and jump to the node.
		TSharedPtr<SDockTab> ActiveTab = DocumentManager->GetActiveTab();
		check(ActiveTab.IsValid());
		GraphEditor = StaticCastSharedRef<SGraphEditor>(ActiveTab->GetContent());
	}
	else
	{
		// Open a graph editor and jump to the node
		GraphEditor = OpenGraphAndBringToFront(Node->GetGraph());
	}

	if (GraphEditor.IsValid())
	{
		GraphEditor->JumpToNode(Node, bRequestRename);
	}
}

UBlueprint* FBlueprintEditor::GetBlueprintObj() const
{
	return GetEditingObjects().Num() == 1 ? Cast<UBlueprint>(GetEditingObjects()[0]) : NULL;
}

bool FBlueprintEditor::IsEditingSingleBlueprint() const
{
	return GetBlueprintObj() != NULL;
}

FString FBlueprintEditor::GetDocumentationLink() const
{
	UBlueprint* Blueprint = GetBlueprintObj();
	if(Blueprint)
	{
		// Jump to more relevant docs if editing macro library or interface
		if(Blueprint->BlueprintType == BPTYPE_MacroLibrary)
		{
			return TEXT("Engine/Blueprints/UserGuide/Types/MacroLibrary");
		}
		else if (Blueprint->BlueprintType == BPTYPE_Interface)
		{
			return TEXT("Engine/Blueprints/UserGuide/Types/Interface");
		}
	}

	return FString(TEXT("Engine/Blueprints"));
}


bool FBlueprintEditor::CanAccessComponentsMode() const
{
	bool bCanAccess = false;

	// Ensure that we're editing a Blueprint
	if(IsEditingSingleBlueprint())
	{
		UBlueprint* Blueprint = GetBlueprintObj();
		bCanAccess = FBlueprintEditorUtils::DoesSupportComponents(Blueprint);
	}
	
	return bCanAccess;
}

void FBlueprintEditor::RegisterToolbarTab(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);
}

void FBlueprintEditor::LogSimpleMessage(const FText& MessageText)
{
	FNotificationInfo Info( MessageText );
	Info.ExpireDuration = 3.0f;
	Info.bUseLargeFont = false;
	TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
	if ( Notification.IsValid() )
	{
		Notification->SetCompletionState( SNotificationItem::CS_Fail );
	}
}

void FBlueprintEditor::DumpMessagesToCompilerLog(const TArray<TSharedRef<FTokenizedMessage>>& Messages, bool bForceMessageDisplay)
{
	CompilerResultsListing->ClearMessages();

	// Note we dont mirror to the output log here as the compiler already does that
	CompilerResultsListing->AddMessages(Messages, false);
	
	if (!bEditorMarkedAsClosed && bForceMessageDisplay && GetCurrentMode() == FBlueprintEditorApplicationModes::StandardBlueprintEditorMode)
	{
		TabManager->InvokeTab(FBlueprintEditorTabs::CompilerResultsID);
	}
}

void FBlueprintEditor::AppendExtraCompilerResults(TSharedPtr<IMessageLogListing> ResultsListing)
{
	// Allow subclasses to append extra data after the compiler finishes dumping all the messages it has.
}

void FBlueprintEditor::DoPromoteToVariable( UBlueprint* InBlueprint, UEdGraphPin* InTargetPin, bool bInToMemberVariable )
{
	UEdGraphNode* PinNode = InTargetPin->GetOwningNode();
	check(PinNode);
	UEdGraph* GraphObj = PinNode->GetGraph();
	check(GraphObj);

	// Used for promoting to local variable
	UEdGraph* FunctionGraph = nullptr;

	const FScopedTransaction Transaction( bInToMemberVariable? LOCTEXT("PromoteToVariable", "Promote To Variable") : LOCTEXT("PromoteToLocalVariable", "Promote to Local Variable") );
	InBlueprint->Modify();
	GraphObj->Modify();

	FName VarName;
	bool bWasSuccessful = false;
	FEdGraphPinType NewPinType = InTargetPin->PinType;
	NewPinType.bIsConst = false;
	NewPinType.bIsReference = false;
	NewPinType.bIsWeakPointer = false;
	if (bInToMemberVariable)
	{
		VarName = FBlueprintEditorUtils::FindUniqueKismetName(GetBlueprintObj(), TEXT("NewVar"));
		bWasSuccessful = FBlueprintEditorUtils::AddMemberVariable( GetBlueprintObj(), VarName, NewPinType, InTargetPin->GetDefaultAsString() );
	}
	else
	{
		ensure(FBlueprintEditorUtils::DoesSupportLocalVariables(GraphObj));
		VarName = FBlueprintEditorUtils::FindUniqueKismetName(GetBlueprintObj(), TEXT("NewLocalVar"));
		FunctionGraph = FBlueprintEditorUtils::GetTopLevelGraph(GraphObj);
		bWasSuccessful = FBlueprintEditorUtils::AddLocalVariable( GetBlueprintObj(), FunctionGraph, VarName, NewPinType, InTargetPin->GetDefaultAsString() );
	}

	if (bWasSuccessful)
	{
		// Create the new setter node
		FEdGraphSchemaAction_K2NewNode NodeInfo;

		// Create get or set node, depending on whether we clicked on an input or output pin
		UK2Node_Variable* TemplateNode = NULL;
		if (InTargetPin->Direction == EGPD_Input)
		{
			TemplateNode = NewObject<UK2Node_VariableGet>();
		}
		else
		{
			TemplateNode = NewObject<UK2Node_VariableSet>();
		}

		if (bInToMemberVariable)
		{
			TemplateNode->VariableReference.SetSelfMember(VarName);
		}
		else
		{
			TemplateNode->VariableReference.SetLocalMember(VarName, FunctionGraph->GetName(), FBlueprintEditorUtils::FindLocalVariableGuidByName(InBlueprint, FunctionGraph, VarName));
		}
		NodeInfo.NodeTemplate = TemplateNode;

		// Set position of new node to be close to node we clicked on
		FVector2D NewNodePos;
		NewNodePos.X = (InTargetPin->Direction == EGPD_Input) ? PinNode->NodePosX - 200 : PinNode->NodePosX + 400;
		NewNodePos.Y = PinNode->NodePosY;

		NodeInfo.PerformAction(GraphObj, InTargetPin, NewNodePos, false);

		RenameNewlyAddedAction(VarName);
	}
}

void FBlueprintEditor::OnPromoteToVariable(bool bInToMemberVariable)
{
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (FocusedGraphEd.IsValid())
	{
		UEdGraphPin* TargetPin = FocusedGraphEd->GetGraphPinForMenu();

		check(IsEditingSingleBlueprint());
		check(GetBlueprintObj()->SkeletonGeneratedClass);
		check(TargetPin);

		DoPromoteToVariable( GetBlueprintObj(), TargetPin, bInToMemberVariable );
	}
}

bool FBlueprintEditor::CanPromoteToVariable(bool bInToMemberVariable) const
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	bool bCanPromote = false;
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (FocusedGraphEd.IsValid())
	{
		if (UEdGraphPin* Pin = FocusedGraphEd->GetGraphPinForMenu())
		{
			if (!Pin->bOrphanedPin && (bInToMemberVariable || (!bInToMemberVariable && FBlueprintEditorUtils::DoesSupportLocalVariables(FocusedGraphEd->GetCurrentGraph()))))
			{
				bCanPromote = K2Schema->CanPromotePinToVariable(*Pin);
			}
		}
	}
	
	return bCanPromote;
}

void FBlueprintEditor::OnSplitStructPin()
{
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (FocusedGraphEd.IsValid())
	{
		UEdGraphPin* TargetPin = FocusedGraphEd->GetGraphPinForMenu();

		check(IsEditingSingleBlueprint());
		check(GetBlueprintObj()->SkeletonGeneratedClass);
		check(TargetPin);

		const FScopedTransaction Transaction( LOCTEXT("SplitStructPin", "Split Struct Pin") );

		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
		K2Schema->SplitPin(TargetPin);
	}
}

bool FBlueprintEditor::CanSplitStructPin() const
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	bool bCanSplit = false;
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (FocusedGraphEd.IsValid())
	{
		if (UEdGraphPin* Pin = FocusedGraphEd->GetGraphPinForMenu())
		{
			bCanSplit = K2Schema->CanSplitStructPin(*Pin);
		}
	}

	return bCanSplit;
}

void FBlueprintEditor::OnRecombineStructPin()
{
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (FocusedGraphEd.IsValid())
	{
		UEdGraphPin* TargetPin = FocusedGraphEd->GetGraphPinForMenu();

		check(IsEditingSingleBlueprint());
		check(GetBlueprintObj()->SkeletonGeneratedClass);
		check(TargetPin);

		const FScopedTransaction Transaction( LOCTEXT("RecombineStructPin", "Recombine Struct Pin") );

		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
		K2Schema->RecombinePin(TargetPin);
	}
}

bool FBlueprintEditor::CanRecombineStructPin() const
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	bool bCanRecombine = false;
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (FocusedGraphEd.IsValid())
	{
		if (UEdGraphPin* Pin = FocusedGraphEd->GetGraphPinForMenu())
		{
			bCanRecombine = K2Schema->CanRecombineStructPin(*Pin);
		}
	}

	return bCanRecombine;
}

void FBlueprintEditor::OnAddExecutionPin()
{
	const FGraphPanelSelectionSet& SelectedNodes = GetSelectedNodes();
	
	// Iterate over all nodes, and add the pin
	for (FGraphPanelSelectionSet::TConstIterator It(SelectedNodes); It; ++It)
	{
		UK2Node_ExecutionSequence* SeqNode = Cast<UK2Node_ExecutionSequence>(*It);
		if (SeqNode != NULL)
		{
			const FScopedTransaction Transaction( LOCTEXT("AddExecutionPin", "Add Execution Pin") );
			SeqNode->Modify();

			SeqNode->AddInputPin();

			const UEdGraphSchema* Schema = SeqNode->GetSchema();
			Schema->ReconstructNode(*SeqNode);
		}
		else if (UK2Node_Switch* SwitchNode = Cast<UK2Node_Switch>(*It))
		{
			const FScopedTransaction Transaction( LOCTEXT("AddExecutionPin", "Add Execution Pin") );
			SwitchNode->Modify();

			SwitchNode->AddPinToSwitchNode();

			const UEdGraphSchema* Schema = SwitchNode->GetSchema();
			Schema->ReconstructNode(*SwitchNode);
		}
	}

	// Refresh the current graph, so the pins can be updated
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (FocusedGraphEd.IsValid())
	{
		FocusedGraphEd->NotifyGraphChanged();
	}
}

bool FBlueprintEditor::CanAddExecutionPin() const
{
	return true;
}

void  FBlueprintEditor::OnRemoveExecutionPin()
{
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (FocusedGraphEd.IsValid())
	{
		const FScopedTransaction Transaction( LOCTEXT("RemoveExecutionPin", "Remove Execution Pin") );

		UEdGraphPin* SelectedPin = FocusedGraphEd->GetGraphPinForMenu();
		UEdGraphNode* OwningNode = SelectedPin->GetOwningNode();

		OwningNode->Modify();
		SelectedPin->Modify();

		if (UK2Node_ExecutionSequence* SeqNode = Cast<UK2Node_ExecutionSequence>(OwningNode))
		{
			SeqNode->RemovePinFromExecutionNode(SelectedPin);
		}
		else if (UK2Node_Switch* SwitchNode = Cast<UK2Node_Switch>(OwningNode))
		{
			SwitchNode->RemovePinFromSwitchNode(SelectedPin);
		}

		// Update the graph so that the node will be refreshed
		FocusedGraphEd->NotifyGraphChanged();
	
		UEdGraph* CurrentGraph = FocusedGraphEd->GetCurrentGraph();
		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraphChecked(CurrentGraph);
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	}
}

bool FBlueprintEditor::CanRemoveExecutionPin() const
{
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (FocusedGraphEd.IsValid())
	{
		if (UEdGraphPin* SelectedPin = FocusedGraphEd->GetGraphPinForMenu())
		{
			UEdGraphNode* OwningNode = SelectedPin->GetOwningNode();

			if (UK2Node_ExecutionSequence* SeqNode = Cast<UK2Node_ExecutionSequence>(OwningNode))
			{
				return SeqNode->CanRemoveExecutionPin();
			}
			else if (UK2Node_Switch* SwitchNode = Cast<UK2Node_Switch>(OwningNode))
			{
				return SwitchNode->CanRemoveExecutionPin(SelectedPin);
			}
		}
	}
	return false;
}

void  FBlueprintEditor::OnRemoveThisStructVarPin()
{
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	UEdGraphPin* SelectedPin = FocusedGraphEd.IsValid() ? FocusedGraphEd->GetGraphPinForMenu() : NULL;
	UEdGraphNode* OwningNode = SelectedPin ? SelectedPin->GetOwningNodeUnchecked() : NULL;
	if (UK2Node_SetFieldsInStruct* SetFilestInStructNode = Cast<UK2Node_SetFieldsInStruct>(OwningNode))
	{
		const FScopedTransaction Transaction(LOCTEXT("RemoveThisStructVarPin", "Remove Struct Var Pin"));
		SetFilestInStructNode->Modify();
		SelectedPin->Modify();
		SetFilestInStructNode->RemoveFieldPins(SelectedPin, UK2Node_SetFieldsInStruct::EPinsToRemove::GivenPin);

		// Update the graph so that the node will be refreshed
		FocusedGraphEd->NotifyGraphChanged();

		UEdGraph* CurrentGraph = FocusedGraphEd->GetCurrentGraph();
		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraphChecked(CurrentGraph);
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	}
}

bool FBlueprintEditor::CanRemoveThisStructVarPin() const
{
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	const UEdGraphPin*  SelectedPin = FocusedGraphEd.IsValid() ? FocusedGraphEd->GetGraphPinForMenu() : NULL;
	return UK2Node_SetFieldsInStruct::ShowCustomPinActions(SelectedPin, false);
}

void  FBlueprintEditor::OnRemoveOtherStructVarPins()
{
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	UEdGraphPin* SelectedPin = FocusedGraphEd.IsValid() ? FocusedGraphEd->GetGraphPinForMenu() : NULL;
	UEdGraphNode* OwningNode = SelectedPin ? SelectedPin->GetOwningNodeUnchecked() : NULL;
	if (UK2Node_SetFieldsInStruct* SetFilestInStructNode = Cast<UK2Node_SetFieldsInStruct>(OwningNode))
	{
		const FScopedTransaction Transaction(LOCTEXT("RemoveOtherStructVarPins", "Remove Other Struct Var Pins"));
		SetFilestInStructNode->Modify();
		SelectedPin->Modify();
		SetFilestInStructNode->RemoveFieldPins(SelectedPin, UK2Node_SetFieldsInStruct::EPinsToRemove::AllOtherPins);

		// Update the graph so that the node will be refreshed
		FocusedGraphEd->NotifyGraphChanged();

		UEdGraph* CurrentGraph = FocusedGraphEd->GetCurrentGraph();
		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraphChecked(CurrentGraph);
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	}
}

bool FBlueprintEditor::CanRemoveOtherStructVarPins() const
{
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	const UEdGraphPin* SelectedPin = FocusedGraphEd.IsValid() ? FocusedGraphEd->GetGraphPinForMenu() : NULL;
	return UK2Node_SetFieldsInStruct::ShowCustomPinActions(SelectedPin, false);
}

void FBlueprintEditor::OnRestoreAllStructVarPins()
{
	const FGraphPanelSelectionSet& SelectedNodes = GetSelectedNodes();
	FGraphPanelSelectionSet::TConstIterator It(SelectedNodes);
	UK2Node_SetFieldsInStruct* Node = (!!It) ? Cast<UK2Node_SetFieldsInStruct>(*It) : NULL;
	if (Node && !Node->AllPinsAreShown())
	{
		const FScopedTransaction Transaction(LOCTEXT("RestoreAllStructVarPins", "Restore all struct var pins"));
		Node->Modify();
		Node->RestoreAllPins();

		// Refresh the current graph, so the pins can be updated
		TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
		if (FocusedGraphEd.IsValid())
		{
			UEdGraph* CurrentGraph = FocusedGraphEd->GetCurrentGraph();
			UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraphChecked(CurrentGraph);
			FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

			FocusedGraphEd->NotifyGraphChanged();
		}
	}

	
}

bool FBlueprintEditor::CanRestoreAllStructVarPins() const
{
	const FGraphPanelSelectionSet& SelectedNodes = GetSelectedNodes();
	FGraphPanelSelectionSet::TConstIterator It(SelectedNodes);
	UK2Node_SetFieldsInStruct* Node = (!!It) ? Cast<UK2Node_SetFieldsInStruct>(*It) : NULL;
	return Node && !Node->AllPinsAreShown();
}

void FBlueprintEditor::OnResetPinToDefaultValue()
{
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (FocusedGraphEd.IsValid())
	{
		UEdGraphPin* TargetPin = FocusedGraphEd->GetGraphPinForMenu();

		check(TargetPin);

		const FScopedTransaction Transaction(LOCTEXT("ResetPinToDefaultValue", "Reset Pin To Default Value"));

		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
		K2Schema->ResetPinToAutogeneratedDefaultValue(TargetPin);
	}
}

bool FBlueprintEditor::CanResetPinToDefaultValue() const
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	bool bCanRecombine = false;
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (FocusedGraphEd.IsValid())
	{
		if (UEdGraphPin* Pin = FocusedGraphEd->GetGraphPinForMenu())
		{
			return !Pin->DoesDefaultValueMatchAutogenerated();
		}
	}

	return false;
}

void FBlueprintEditor::OnAddOptionPin()
{
	const FGraphPanelSelectionSet& SelectedNodes = GetSelectedNodes();

	// Iterate over all nodes, and add the pin
	for (FGraphPanelSelectionSet::TConstIterator It(SelectedNodes); It; ++It)
	{
		UK2Node_Select* SeqNode = Cast<UK2Node_Select>(*It);
		if (SeqNode != NULL)
		{
			const FScopedTransaction Transaction( LOCTEXT("AddOptionPin", "Add Option Pin") );
			SeqNode->Modify();

			SeqNode->AddOptionPinToNode();

			const UEdGraphSchema* Schema = SeqNode->GetSchema();
			Schema->ReconstructNode(*SeqNode);
		}
	}

	// Refresh the current graph, so the pins can be updated
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (FocusedGraphEd.IsValid())
	{
		FocusedGraphEd->NotifyGraphChanged();
	}
}

bool FBlueprintEditor::CanAddOptionPin() const
{
	const FGraphPanelSelectionSet& SelectedNodes = GetSelectedNodes();

	// Iterate over all nodes, and see if all can have a pin removed
	for (FGraphPanelSelectionSet::TConstIterator It(SelectedNodes); It; ++It)
	{
		UK2Node_Select* SeqNode = Cast<UK2Node_Select>(*It);
		// There's a bad node so return false
		if (SeqNode == NULL || !SeqNode->CanAddOptionPinToNode())
		{
			return false;
		}
	}

	return true;
}

void FBlueprintEditor::OnRemoveOptionPin()
{
	const FGraphPanelSelectionSet& SelectedNodes = GetSelectedNodes();

	// Iterate over all nodes, and add the pin
	for (FGraphPanelSelectionSet::TConstIterator It(SelectedNodes); It; ++It)
	{
		UK2Node_Select* SeqNode = Cast<UK2Node_Select>(*It);
		if (SeqNode != NULL)
		{
			const FScopedTransaction Transaction( LOCTEXT("RemoveOptionPin", "Remove Option Pin") );
			SeqNode->Modify();

			SeqNode->RemoveOptionPinToNode();

			const UEdGraphSchema* Schema = SeqNode->GetSchema();
			Schema->ReconstructNode(*SeqNode);
		}
	}

	// Refresh the current graph, so the pins can be updated
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (FocusedGraphEd.IsValid())
	{
		FocusedGraphEd->NotifyGraphChanged();
	}
}

bool FBlueprintEditor::CanRemoveOptionPin() const
{
	const FGraphPanelSelectionSet& SelectedNodes = GetSelectedNodes();

	// Iterate over all nodes, and see if all can have a pin removed
	for (FGraphPanelSelectionSet::TConstIterator It(SelectedNodes); It; ++It)
	{
		UK2Node_Select* SeqNode = Cast<UK2Node_Select>(*It);
		// There's a bad node so return false
		if (SeqNode == NULL || !SeqNode->CanRemoveOptionPinToNode())
		{
			return false;
		}
		// If this node doesn't have at least 3 options return false (need at least 2)
		else
		{
			TArray<UEdGraphPin*> OptionPins;
			SeqNode->GetOptionPins(OptionPins);
			if (OptionPins.Num() <= 2)
			{
				return false;
			}
		}
	}

	return true;
}

void FBlueprintEditor::OnChangePinType()
{
	if (UEdGraphPin* SelectedPin = GetCurrentlySelectedPin())
	{
		// Grab the root pin, that is what we want to edit
		UEdGraphPin* RootPin = SelectedPin;
		while(RootPin->ParentPin)
			{
			RootPin = RootPin->ParentPin;
		}

				const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

				// If this is the index node of the select node, we need to use the index list of types
				UK2Node_Select* SelectNode = Cast<UK2Node_Select>(SelectedPin->GetOwningNode());
				if (SelectNode && SelectNode->GetIndexPin() == SelectedPin)
				{
					TSharedRef<SCompoundWidget> PinChange = SNew(SPinTypeSelector, FGetPinTypeTree::CreateUObject(Schema, &UEdGraphSchema_K2::GetVariableTypeTree))
				.TargetPinType(this, &FBlueprintEditor::OnGetPinType, RootPin)
						.OnPinTypeChanged(this, &FBlueprintEditor::OnChangePinTypeFinished, SelectedPin)
						.Schema(Schema)
						.TypeTreeFilter(ETypeTreeFilter::IndexTypesOnly)
						.IsEnabled(true)
						.bAllowArrays(false);

					PinTypeChangeMenu = FSlateApplication::Get().PushMenu(
						GetToolkitHost()->GetParentWidget(), // Parent widget should be k2 not the menu thats open or it will be closed when the menu is dismissed
						FWidgetPath(),
						PinChange,
						FSlateApplication::Get().GetCursorPos(), // summon location
						FPopupTransitionEffect(FPopupTransitionEffect::TypeInPopup)
						);
				}
				else
				{
					TSharedRef<SCompoundWidget> PinChange = SNew(SPinTypeSelector, FGetPinTypeTree::CreateUObject(Schema, &UEdGraphSchema_K2::GetVariableTypeTree))
				.TargetPinType(this, &FBlueprintEditor::OnGetPinType, RootPin)
						.OnPinTypeChanged(this, &FBlueprintEditor::OnChangePinTypeFinished, SelectedPin)
						.Schema(Schema)
						.TypeTreeFilter(ETypeTreeFilter::None)
						.IsEnabled(true)
						.bAllowArrays(false);

					PinTypeChangeMenu = FSlateApplication::Get().PushMenu(
						GetToolkitHost()->GetParentWidget(), // Parent widget should be k2 not the menu thats open or it will be closed when the menu is dismissed
						FWidgetPath(),
						PinChange,
						FSlateApplication::Get().GetCursorPos(), // summon location
						FPopupTransitionEffect(FPopupTransitionEffect::TypeInPopup)
						);
				}
			}
}

FEdGraphPinType FBlueprintEditor::OnGetPinType(UEdGraphPin* SelectedPin) const
{
	return SelectedPin->PinType;
}

void FBlueprintEditor::OnChangePinTypeFinished(const FEdGraphPinType& PinType, UEdGraphPin* InSelectedPin)
{
	if (FBlueprintEditorUtils::IsPinTypeValid(PinType))
	{
		InSelectedPin->PinType = PinType;
		if (UK2Node_Select* SelectNode = Cast<UK2Node_Select>(InSelectedPin->GetOwningNode()))
			{
			SelectNode->ChangePinType(InSelectedPin);
		}
	}

	if (PinTypeChangeMenu.IsValid())
	{
		PinTypeChangeMenu.Pin()->Dismiss();
	}
}

bool FBlueprintEditor::CanChangePinType() const
{
	if (UEdGraphPin* Pin = GetCurrentlySelectedPin())
	{
		if (UK2Node_Select* SelectNode = Cast<UK2Node_Select>(Pin->GetOwningNode()))
		{
			return SelectNode->CanChangePinType(Pin);
		}
	}
	return false;
}

void FBlueprintEditor::OnAddParentNode()
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	if (UEdGraphNode* SelectedObj = GetSingleSelectedNode())
	{
		// Get the function that the event node or function entry represents
		FFunctionFromNodeHelper FunctionFromNode(SelectedObj);
		if (FunctionFromNode.Function && FunctionFromNode.Node)
		{
			UFunction* ValidParent = Schema->GetCallableParentFunction(FunctionFromNode.Function);
			UEdGraph* TargetGraph = FunctionFromNode.Node->GetGraph();
			if (ValidParent && TargetGraph)
			{
				FGraphNodeCreator<UK2Node_CallParentFunction> FunctionNodeCreator(*TargetGraph);
				UK2Node_CallParentFunction* ParentFunctionNode = FunctionNodeCreator.CreateNode();
				ParentFunctionNode->SetFromFunction(ValidParent);
				ParentFunctionNode->AllocateDefaultPins();

				int32 NodeSizeY = 15;
				if( UK2Node* Node = Cast<UK2Node>(SelectedObj))
				{
					NodeSizeY += Node->DEPRECATED_NodeWidget.IsValid() ? static_cast<int32>(Node->DEPRECATED_NodeWidget.Pin()->GetDesiredSize().Y) : 0;
				}
				ParentFunctionNode->NodePosX = FunctionFromNode.Node->NodePosX;
				ParentFunctionNode->NodePosY = FunctionFromNode.Node->NodePosY + NodeSizeY;
				FunctionNodeCreator.Finalize();
			}
		}
	}
}

bool FBlueprintEditor::CanAddParentNode() const
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	if (UEdGraphNode* SelectedObj = GetSingleSelectedNode())
	{
		// Get the function that the event node or function entry represents
		FFunctionFromNodeHelper FunctionFromNode(SelectedObj);
		if (FunctionFromNode.Function)
		{
			return (Schema->GetCallableParentFunction(FunctionFromNode.Function) != NULL);
		}
	}

	return false;
}

void FBlueprintEditor::OnToggleBreakpoint()
{
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
	{
		UK2Node* SelectedNode = Cast<UK2Node>(*NodeIt);
		if ((SelectedNode != NULL) && SelectedNode->CanPlaceBreakpoints())
		{
			UBreakpoint* ExistingBreakpoint = FKismetDebugUtilities::FindBreakpointForNode(GetBlueprintObj(), SelectedNode);
			if (ExistingBreakpoint == NULL)
			{
				// Add a breakpoint on this node if there isn't one there already
				UBreakpoint* NewBreakpoint = NewObject<UBreakpoint>(GetBlueprintObj());
				FKismetDebugUtilities::SetBreakpointEnabled(NewBreakpoint, true);
				FKismetDebugUtilities::SetBreakpointLocation(NewBreakpoint, SelectedNode);
				GetBlueprintObj()->Breakpoints.Add(NewBreakpoint);
				GetBlueprintObj()->MarkPackageDirty();
			}
			else
			{
				// Remove the breakpoint if it was present
				FKismetDebugUtilities::StartDeletingBreakpoint(ExistingBreakpoint, GetBlueprintObj());
			}
		}
	}
}

bool FBlueprintEditor::CanToggleBreakpoint() const
{
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
	{
		UK2Node* SelectedNode = Cast<UK2Node>(*NodeIt);
		if ((SelectedNode != NULL) && SelectedNode->CanPlaceBreakpoints())
		{
			return true;
		}
	}

	return false;
}

void FBlueprintEditor::OnAddBreakpoint()
{
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
	{
		UK2Node* SelectedNode = Cast<UK2Node>(*NodeIt);
		if ((SelectedNode != NULL) && SelectedNode->CanPlaceBreakpoints())
		{
			// Add a breakpoint on this node if there isn't one there already
			UBreakpoint* ExistingBreakpoint = FKismetDebugUtilities::FindBreakpointForNode(GetBlueprintObj(), SelectedNode);
			if (ExistingBreakpoint == NULL)
			{
				// Add a new breakpoint
				UBreakpoint* NewBreakpoint = NewObject<UBreakpoint>(GetBlueprintObj());
				FKismetDebugUtilities::SetBreakpointEnabled(NewBreakpoint, true);
				FKismetDebugUtilities::SetBreakpointLocation(NewBreakpoint, SelectedNode);
				GetBlueprintObj()->Breakpoints.Add(NewBreakpoint);
				GetBlueprintObj()->MarkPackageDirty();
			}
		}
	}
}

bool FBlueprintEditor::CanAddBreakpoint() const
{
	// See if any of the selected nodes are impure, and thus could have a breakpoint set on them
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
	{
		UK2Node* SelectedNode = Cast<UK2Node>(*NodeIt);
		if ((SelectedNode != NULL) && SelectedNode->CanPlaceBreakpoints())
		{
			UBreakpoint* ExistingBreakpoint = FKismetDebugUtilities::FindBreakpointForNode(GetBlueprintObj(), SelectedNode);
			if (ExistingBreakpoint == NULL)
			{
				return true;
			}
		}
	}

	return false;
}

void FBlueprintEditor::OnRemoveBreakpoint()
{
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
	{
		UEdGraphNode* SelectedNode = CastChecked<UEdGraphNode>(*NodeIt);

		// Remove any pre-existing breakpoint on this node
		if (UBreakpoint* ExistingBreakpoint = FKismetDebugUtilities::FindBreakpointForNode(GetBlueprintObj(), SelectedNode))
		{
			// Remove the breakpoint
			FKismetDebugUtilities::StartDeletingBreakpoint(ExistingBreakpoint, GetBlueprintObj());
		}
	}
}

bool FBlueprintEditor::CanRemoveBreakpoint() const
{
	// See if any of the selected nodes have a breakpoint set
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
	{
		UEdGraphNode* SelectedNode = CastChecked<UEdGraphNode>(*NodeIt);
		if (UBreakpoint* ExistingBreakpoint = FKismetDebugUtilities::FindBreakpointForNode(GetBlueprintObj(), SelectedNode))
		{
			return true;
		}
	}

	return false;
}

void FBlueprintEditor::OnDisableBreakpoint()
{
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
	{
		UEdGraphNode* SelectedNode = CastChecked<UEdGraphNode>(*NodeIt);

		if (UBreakpoint* ExistingBreakpoint = FKismetDebugUtilities::FindBreakpointForNode(GetBlueprintObj(), SelectedNode))
		{
			FKismetDebugUtilities::SetBreakpointEnabled(ExistingBreakpoint, false);
		}
	}
}

bool FBlueprintEditor::CanDisableBreakpoint() const
{
	// See if any of the selected nodes have a breakpoint set
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
	{
		UEdGraphNode* SelectedNode = CastChecked<UEdGraphNode>(*NodeIt);
		if (UBreakpoint* ExistingBreakpoint = FKismetDebugUtilities::FindBreakpointForNode(GetBlueprintObj(), SelectedNode))
		{
			if (ExistingBreakpoint->IsEnabledByUser())
			{
				return true;
			}
		}
	}

	return false;
}

void FBlueprintEditor::OnEnableBreakpoint()
{
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
	{
		UEdGraphNode* SelectedNode = CastChecked<UEdGraphNode>(*NodeIt);

		if (UBreakpoint* ExistingBreakpoint = FKismetDebugUtilities::FindBreakpointForNode(GetBlueprintObj(), SelectedNode))
		{
			FKismetDebugUtilities::SetBreakpointEnabled(ExistingBreakpoint, true);
		}
	}
}

bool FBlueprintEditor::CanEnableBreakpoint() const
{
	// See if any of the selected nodes have a breakpoint set
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
	{
		UEdGraphNode* SelectedNode = CastChecked<UEdGraphNode>(*NodeIt);
		if (UBreakpoint* ExistingBreakpoint = FKismetDebugUtilities::FindBreakpointForNode(GetBlueprintObj(), SelectedNode))
		{
			if (!ExistingBreakpoint->IsEnabledByUser())
			{
				return true;
			}
		}
	}

	return false;
}

void FBlueprintEditor::OnCollapseNodes()
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	// Does the selection set contain anything that is legal to collapse?
	TSet<UEdGraphNode*> CollapsableNodes;
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
	{
		if (UEdGraphNode* SelectedNode = Cast<UEdGraphNode>(*NodeIt))
		{
			if (Schema->CanEncapuslateNode(*SelectedNode))
			{
				CollapsableNodes.Add(SelectedNode);
			}
		}
	}

	// Collapse them
	if (CollapsableNodes.Num())
	{
		UBlueprint* BlueprintObj = GetBlueprintObj();
		const FScopedTransaction Transaction( FGraphEditorCommands::Get().CollapseNodes->GetDescription() );
		BlueprintObj->Modify();

		CollapseNodes(CollapsableNodes);

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified( BlueprintObj );
	}
}

bool FBlueprintEditor::CanCollapseNodes() const
{
	//@TODO: ANIM: Determine what collapsing nodes means in an animation graph, and add any necessary compiler support for it
	if (IsEditingAnimGraph())
	{
		return false;
	}

	// Does the selection set contain anything that is legal to collapse?
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
	{
		if (UEdGraphNode* Node = Cast<UEdGraphNode>(*NodeIt))
		{
			if (Schema->CanEncapuslateNode(*Node))
			{
				return true;
			}
		}
	}

	return false;
}

void FBlueprintEditor::OnCollapseSelectionToFunction()
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	// Does the selection set contain anything that is legal to collapse?
	TSet<UEdGraphNode*> CollapsableNodes;
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
	{
		if (UEdGraphNode* SelectedNode = Cast<UEdGraphNode>(*NodeIt))
		{
			if (Schema->CanEncapuslateNode(*SelectedNode))
			{
				CollapsableNodes.Add(SelectedNode);
			}
		}
	}

	// Collapse them
	if (CollapsableNodes.Num() && CanCollapseSelectionToFunction(CollapsableNodes))
	{
		UBlueprint* BlueprintObj = GetBlueprintObj();
		const FScopedTransaction Transaction( FGraphEditorCommands::Get().CollapseNodes->GetDescription() );
		BlueprintObj->Modify();

		UEdGraphNode* FunctionNode = NULL;
		UEdGraph* FunctionGraph = CollapseSelectionToFunction(FocusedGraphEdPtr.Pin(), CollapsableNodes, FunctionNode);

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified( BlueprintObj );

		RenameNewlyAddedAction(FunctionGraph->GetFName());
	}
}

bool FBlueprintEditor::CanCollapseSelectionToFunction(TSet<class UEdGraphNode*>& InSelection) const
{
	bool bBadConnection = false;
	UEdGraphPin* OutputConnection = NULL;
	UEdGraphPin* InputConnection = NULL;

	// Create a function graph
	UEdGraph* FunctionGraph = FBlueprintEditorUtils::CreateNewGraph(GetBlueprintObj(), FBlueprintEditorUtils::FindUniqueKismetName(GetBlueprintObj(), TEXT("TempGraph")), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
	FBlueprintEditorUtils::AddFunctionGraph<UClass>(GetBlueprintObj(), FunctionGraph, /*bIsUserCreated=*/ true, NULL);

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	FCompilerResultsLog LogResults;
	LogResults.bAnnotateMentionedNodes = false;

	UEdGraphNode* InterfaceTemplateNode = nullptr;

	TArray<UEdGraphPin*> EntryGatewayPins;

	// Runs through every node and fully validates errors with placing selection in a function graph, reporting all errors.
	for (TSet<UEdGraphNode*>::TConstIterator NodeIt(InSelection); NodeIt; ++NodeIt)
	{
		UEdGraphNode* Node = *NodeIt;

		if(!Node->CanPasteHere(FunctionGraph))
		{
			if (UK2Node_CustomEvent* const CustomEvent = Cast<UK2Node_CustomEvent>(Node))
			{
				UEdGraphPin* EventExecPin = K2Schema->FindExecutionPin(*CustomEvent, EGPD_Output);
				check(EventExecPin);

				if(InterfaceTemplateNode)
				{
					LogResults.Error(*LOCTEXT("TooManyCustomEvents_Error", "Can use @@ as a template for creating the function, can only have a single custom event! Previously found @@").ToString(), CustomEvent, InterfaceTemplateNode);
				}
				else
				{
					// The custom event will be used as the template interface for the function.
					InterfaceTemplateNode = CustomEvent;
					if(InputConnection)
					{
						InputConnection = EventExecPin->LinkedTo[0];
					}
					continue;
				}
			}

			LogResults.Error(*LOCTEXT("CannotPasteNodeFunction_Error", "@@ cannot be placed in function graph").ToString(), Node);
			bBadConnection = true;
		}
		else
		{
			for (int32 PinIndex = 0; PinIndex < Node->Pins.Num(); ++PinIndex)
			{
				if(Node->Pins[PinIndex]->PinType.PinCategory == K2Schema->PC_Exec)
				{
					if (Node->Pins[PinIndex]->LinkedTo.Num() == 0 && Node->Pins[PinIndex]->Direction == EGPD_Input)
					{
						EntryGatewayPins.Add(Node->Pins[PinIndex]);
					}
					else
					{
						for (int32 LinkIndex = 0; LinkIndex < Node->Pins[PinIndex]->LinkedTo.Num(); ++LinkIndex)
						{
							if (!InSelection.Contains(Node->Pins[PinIndex]->LinkedTo[LinkIndex]->GetOwningNode()))
							{
								if (Node->Pins[PinIndex]->Direction == EGPD_Input)
								{
									// For input pins, there must be a single connection 
									if (InputConnection == NULL || InputConnection == Node->Pins[PinIndex])
									{
										EntryGatewayPins.Add(Node->Pins[PinIndex]);
										InputConnection = Node->Pins[PinIndex];
									}
									else
									{
										// Check if the input connection was linked, report what node it is connected to
										LogResults.Error(*LOCTEXT("TooManyPathsMultipleInput_Error", "Found too many input connections in selection! @@ is connected to @@, previously found @@ connected to @@").ToString(), Node, Node->Pins[PinIndex]->LinkedTo[LinkIndex]->GetOwningNode(), InputConnection->GetOwningNode(), InputConnection->LinkedTo[0]->GetOwningNode());
										bBadConnection = true;
									}
								}
								else
								{
									// For output pins, as long as they all connect to the same pin, we consider the selection valid for being made into a function
									if (OutputConnection == NULL || OutputConnection == Node->Pins[PinIndex]->LinkedTo[LinkIndex])
									{
										OutputConnection = Node->Pins[PinIndex]->LinkedTo[LinkIndex];
									}
									else
									{
										check(OutputConnection->LinkedTo.Num());

										LogResults.Error(*LOCTEXT("TooManyPathsMultipleOutput_Error", "Found too many output connections in selection! @@ is connected to @@, previously found @@ connected to @@").ToString(), Node, Node->Pins[PinIndex]->LinkedTo[LinkIndex]->GetOwningNode(), OutputConnection->GetOwningNode(), OutputConnection->LinkedTo[0]->GetOwningNode());
										bBadConnection = true;
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if (!bBadConnection && InputConnection == nullptr && EntryGatewayPins.Num() > 1)
	{
		// Too many input gateway pins with no connections.
		LogResults.Error(*LOCTEXT("AmbiguousEntryPaths_Error", "Multiple entry pin possibilities. Unable to convert to a function. Make sure that selection either has only 1 entry pin or exactly 1 entry pin has a connection.").ToString());
		bBadConnection = true;
	}

	// No need to check for cycling if the selection is invalid anyways.
	if(!bBadConnection && FBlueprintEditorUtils::CheckIfSelectionIsCycling(InSelection, LogResults))
	{
		bBadConnection = true;
	}

	FMessageLog MessageLog("BlueprintLog");
	MessageLog.NewPage(LOCTEXT("CollapseToFunctionPageLabel", "Collapse to Function"));
	MessageLog.AddMessages(LogResults.Messages);
	MessageLog.Notify(LOCTEXT("CollapseToFunctionError", "Collapsing to Function Failed!"));

	FBlueprintEditorUtils::RemoveGraph(GetBlueprintObj(), FunctionGraph);
	FunctionGraph->MarkPendingKill();
	return !bBadConnection;
}

bool FBlueprintEditor::CanCollapseSelectionToFunction() const
{
	return !IsEditingAnimGraph();
}

void FBlueprintEditor::OnCollapseSelectionToMacro()
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	// Does the selection set contain anything that is legal to collapse?
	TSet<UEdGraphNode*> CollapsableNodes;
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();

	for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
	{
		if (UEdGraphNode* SelectedNode = Cast<UEdGraphNode>(*NodeIt))
		{
			if (Schema->CanEncapuslateNode(*SelectedNode))
			{
				CollapsableNodes.Add(SelectedNode);
			}
		}
	}

	// Collapse them
	if (CollapsableNodes.Num() && CanCollapseSelectionToMacro(CollapsableNodes))
	{
		UBlueprint* BlueprintObj = GetBlueprintObj();
		const FScopedTransaction Transaction( FGraphEditorCommands::Get().CollapseNodes->GetDescription() );
		BlueprintObj->Modify();

		UEdGraphNode* MacroNode = NULL;
		UEdGraph* MacroGraph = CollapseSelectionToMacro(FocusedGraphEdPtr.Pin(), CollapsableNodes, MacroNode);

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified( BlueprintObj );

		RenameNewlyAddedAction(MacroGraph->GetFName());
	}
}

bool FBlueprintEditor::CanCollapseSelectionToMacro(TSet<class UEdGraphNode*>& InSelection) const
{
	// Create a temporary macro graph
	UEdGraph* MacroGraph = FBlueprintEditorUtils::CreateNewGraph(GetBlueprintObj(), FBlueprintEditorUtils::FindUniqueKismetName(GetBlueprintObj(), TEXT("TempGraph")), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
	FBlueprintEditorUtils::AddMacroGraph(GetBlueprintObj(), MacroGraph, /*bIsUserCreated=*/ true, NULL);

	// Does the selection set contain anything that is legal to collapse?
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();

	bool bCollapseAllowed = true;
	FCompilerResultsLog LogResults;
	LogResults.bAnnotateMentionedNodes = false;

	for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
	{
		UEdGraphNode* Node = Cast<UEdGraphNode>(*NodeIt);

		if(!Node->CanPasteHere(MacroGraph))
		{
			LogResults.Error(*LOCTEXT("CannotPasteNodeMacro_Error", "@@ cannot be placed in macro graph").ToString(), Node);
			bCollapseAllowed = false;
		}
	}

	FMessageLog MessageLog("BlueprintLog");
	MessageLog.NewPage(LOCTEXT("CollapseToMacroPageLabel", "Collapse to Macro"));
	MessageLog.AddMessages(LogResults.Messages);
	MessageLog.Notify(LOCTEXT("CollapseToMacroError", "Collapsing to Macro Failed!"));

	FBlueprintEditorUtils::RemoveGraph(GetBlueprintObj(), MacroGraph);
	MacroGraph->MarkPendingKill();
	return bCollapseAllowed;
}

bool FBlueprintEditor::CanCollapseSelectionToMacro() const
{
	if (FocusedGraphEdPtr.IsValid())
	{
		if(IsEditingAnimGraph())
		{
			return false;
		}
	}

	return true;
}

void FBlueprintEditor::OnPromoteSelectionToFunction()
{
	const FScopedTransaction Transaction( LOCTEXT("ConvertCollapsedGraphToFunction", "Convert Collapse Graph to Function") );
	GetBlueprintObj()->Modify();

	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();

	// Set of nodes to select when finished
	TSet<class UEdGraphNode*> NodesToSelect;

	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	for (FGraphPanelSelectionSet::TConstIterator It(SelectedNodes); It; ++It)
	{
		if (UK2Node_Composite* CompositeNode = Cast<UK2Node_Composite>(*It))
		{
			// Check if there is only one input and one output connection
			TSet<class UEdGraphNode*> NodesInGraph;
			NodesInGraph.Add(CompositeNode);

			if(CanCollapseSelectionToFunction(NodesInGraph))
			{
				DocumentManager->CleanInvalidTabs();

				// Expand the composite node back into the world
				UEdGraph* SourceGraph = CompositeNode->BoundGraph;

				// Expand all composite nodes back in place
				TSet<UEdGraphNode*> ExpandedNodes;
				ExpandNode(CompositeNode, SourceGraph, /*inout*/ ExpandedNodes);
				FBlueprintEditorUtils::RemoveGraph(GetBlueprintObj(), SourceGraph, EGraphRemoveFlags::Recompile);

				//Remove this node from selection
				FocusedGraphEd->SetNodeSelection(CompositeNode, false);

				UEdGraphNode* FunctionNode = NULL;
				CollapseSelectionToFunction(FocusedGraphEd, ExpandedNodes, FunctionNode);
				NodesToSelect.Add(FunctionNode);
			}
			else
			{
				NodesToSelect.Add(CompositeNode);
			}
		}
		else if(UEdGraphNode* Node = Cast<UEdGraphNode>(*It))
		{
			NodesToSelect.Add(Node);
		}
	}

	// Select all nodes that should still be part of selection
	for (UEdGraphNode* NodeToSelect : NodesToSelect)
	{
		FocusedGraphEd->SetNodeSelection(NodeToSelect, true);
	}
}

bool FBlueprintEditor::CanPromoteSelectionToFunction() const
{
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
	{
		if (UK2Node_Composite* CompositeNode = Cast<UK2Node_Composite>(*NodeIt))
		{
			// Check if there is only one input and one output connection
			TSet<class UEdGraphNode*> NodesInGraph;
			NodesInGraph.Add(CompositeNode);

			return true;
		}
	}
	return false;
}

void FBlueprintEditor::OnPromoteSelectionToMacro()
{
	const FScopedTransaction Transaction( LOCTEXT("ConvertCollapsedGraphToMacro", "Convert Collapse Graph to Macro") );
	GetBlueprintObj()->Modify();

	// Set of nodes to select when finished
	TSet<class UEdGraphNode*> NodesToSelect;

	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();

	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	for (FGraphPanelSelectionSet::TConstIterator It(SelectedNodes); It; ++It)
	{
		if (UK2Node_Composite* CompositeNode = Cast<UK2Node_Composite>(*It))
		{
			TSet<class UEdGraphNode*> NodesInGraph;

			// Collect all the nodes to test if they can be made into a function
			for (UEdGraphNode* Node : CompositeNode->BoundGraph->Nodes)
			{
				// Ignore the tunnel nodes
				if (Node->GetClass() != UK2Node_Tunnel::StaticClass())
				{
					NodesInGraph.Add(Node);
				}
			}

			if(CanCollapseSelectionToMacro(NodesInGraph))
			{
				DocumentManager->CleanInvalidTabs();

				// Expand the composite node back into the world
				UEdGraph* SourceGraph = CompositeNode->BoundGraph;

				// Expand all composite nodes back in place
				TSet<UEdGraphNode*> ExpandedNodes;
				ExpandNode(CompositeNode, SourceGraph, /*inout*/ ExpandedNodes);
				FBlueprintEditorUtils::RemoveGraph(GetBlueprintObj(), SourceGraph, EGraphRemoveFlags::Recompile);

				//Remove this node from selection
				FocusedGraphEd->SetNodeSelection(CompositeNode, false);

				UEdGraphNode* MacroNode = NULL;
				CollapseSelectionToMacro(FocusedGraphEd, ExpandedNodes, MacroNode);
				NodesToSelect.Add(MacroNode);
			}
			else
			{
				NodesToSelect.Add(CompositeNode);
			}
		}
		else if(UEdGraphNode* Node = Cast<UEdGraphNode>(*It))
		{
			NodesToSelect.Add(Node);
		}
	}

	// Select all nodes that should still be part of selection
	for (UEdGraphNode* NodeToSelect : NodesToSelect)
	{
		FocusedGraphEd->SetNodeSelection(NodeToSelect, true);
	}
}

bool FBlueprintEditor::CanPromoteSelectionToMacro() const
{
	if (FocusedGraphEdPtr.IsValid())
	{
		if(IsEditingAnimGraph())
		{
			return false;
		}
	}

	for (UObject* SelectedNode : GetSelectedNodes())
	{
		UK2Node_Composite* CompositeNode = Cast<UK2Node_Composite>(SelectedNode);
		if (CompositeNode && CompositeNode->BoundGraph)
		{
			TSet<class UEdGraphNode*> NodesInGraph;

			// Collect all the nodes to test if they can be made into a function
			for (UEdGraphNode* Node : CompositeNode->BoundGraph->Nodes)
			{
				// Ignore the tunnel nodes
				if (Node->GetClass() != UK2Node_Tunnel::StaticClass())
				{
					NodesInGraph.Add(Node);
				}
			}

			return true;
		}
	}
	return false;
}

void FBlueprintEditor::OnExpandNodes()
{
	const FScopedTransaction Transaction( FGraphEditorCommands::Get().ExpandNodes->GetLabel() );
	GetBlueprintObj()->Modify();

	TSet<UEdGraphNode*> ExpandedNodes;
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();

	// Expand selected nodes into the focused graph context.
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
	{
		ExpandedNodes.Empty();
		bool bExpandedNodesNeedUniqueGuid = true;

		DocumentManager->CleanInvalidTabs();

		if (UK2Node_MacroInstance* SelectedMacroInstanceNode = Cast<UK2Node_MacroInstance>(*NodeIt))
		{
			UEdGraph* MacroGraph = SelectedMacroInstanceNode->GetMacroGraph();
			if(MacroGraph)
			{
				// Clone the graph so that we do not delete the original
				UEdGraph* ClonedGraph = FEdGraphUtilities::CloneGraph(MacroGraph, NULL);
				ExpandNode(SelectedMacroInstanceNode, ClonedGraph, /*inout*/ ExpandedNodes);

				ClonedGraph->MarkPendingKill();
			}
		}
		else if (UK2Node_Composite* SelectedCompositeNode = Cast<UK2Node_Composite>(*NodeIt))
		{
			// No need to assign unique GUIDs since the source graph will be removed.
			bExpandedNodesNeedUniqueGuid = false;

			// Expand the composite node back into the world
			UEdGraph* SourceGraph = SelectedCompositeNode->BoundGraph;
			ExpandNode(SelectedCompositeNode, SourceGraph, /*inout*/ ExpandedNodes);

			FBlueprintEditorUtils::RemoveGraph(GetBlueprintObj(), SourceGraph, EGraphRemoveFlags::Recompile);
		}
		else if (UK2Node_CallFunction* SelectedCallFunctionNode = Cast<UK2Node_CallFunction>(*NodeIt))
		{
			const UEdGraphNode* ResultEventNode = NULL;
			UEdGraph* FunctionGraph = SelectedCallFunctionNode->GetFunctionGraph(ResultEventNode);

			// We should never get here when attempting to expand a call function that calls an event.
			check(!ResultEventNode);

			if(FunctionGraph)
			{
				// Clone the graph so that we do not delete the original
				UEdGraph* ClonedGraph = FEdGraphUtilities::CloneGraph(FunctionGraph, NULL);
				ExpandNode(SelectedCallFunctionNode, ClonedGraph, ExpandedNodes);

				ClonedGraph->MarkPendingKill();
			}
		}

		if (ExpandedNodes.Num() > 0)
		{
			FVector2D AvgNodePosition(0.0f, 0.0f);

			for (TSet<UEdGraphNode*>::TIterator It(ExpandedNodes); It; ++It)
			{
				UEdGraphNode* Node = *It;
				AvgNodePosition.X += Node->NodePosX;
				AvgNodePosition.Y += Node->NodePosY;
			}

			float InvNumNodes = 1.0f / float(ExpandedNodes.Num());
			AvgNodePosition.X *= InvNumNodes;
			AvgNodePosition.Y *= InvNumNodes;

			//Remove source node from selection
			UEdGraphNode* SourceNode = CastChecked<UEdGraphNode>(*NodeIt);
			FocusedGraphEd->SetNodeSelection(SourceNode, false);

			for (UEdGraphNode* ExpandedNode : ExpandedNodes)
			{
				ExpandedNode->NodePosX = (ExpandedNode->NodePosX - AvgNodePosition.X) + SourceNode->NodePosX;
				ExpandedNode->NodePosY = (ExpandedNode->NodePosY - AvgNodePosition.Y) + SourceNode->NodePosY;

				ExpandedNode->SnapToGrid(SNodePanel::GetSnapGridSize());

				if (bExpandedNodesNeedUniqueGuid)
				{
					ExpandedNode->CreateNewGuid();
				}

				//Add expanded node to selection
				FocusedGraphEd->SetNodeSelection(ExpandedNode, true);
			}
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprintObj());
}

bool FBlueprintEditor::CanExpandNodes() const
{
	// Does the selection set contain any composite nodes that are legal to expand?
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
	{
		if (Cast<UK2Node_Composite>(*NodeIt))
		{
			return true;
		}
		else if (UK2Node_MacroInstance* SelectedMacroInstanceNode = Cast<UK2Node_MacroInstance>(*NodeIt))
		{
			return SelectedMacroInstanceNode->GetMacroGraph() != NULL;
		}
		else if (UK2Node_CallFunction* SelectedCallFunctionNode = Cast<UK2Node_CallFunction>(*NodeIt))
		{
			// If ResultEventNode is non-NULL, it means it is sourced by an event, we do not want to expand events
			const UEdGraphNode* ResultEventNode = NULL;
			return SelectedCallFunctionNode->GetFunctionGraph(ResultEventNode) != NULL && ResultEventNode == NULL;
		}
	}

	return false;
}

/** Struct used for generically aligning nodes */
struct FAlignmentData
{
	FAlignmentData(UEdGraphNode* InNode, int32& InTargetProperty, float InTargetOffset)
		: Node(InNode), TargetProperty(InTargetProperty), TargetOffset(InTargetOffset)
	{}

	/** The node to position */
	UEdGraphNode* Node;
	/** The property within the node to read/write */
	int32& TargetProperty;
	/** The offset from the property to consider for alignment */
	float TargetOffset;

	/** Get the destination target from this alignment data (property + offset) */
	float GetTarget() const
	{
		return float(TargetProperty) + TargetOffset;
	}
};

enum class EAlignType : uint8
{
	Minimum, Middle, Maximum
};

FVector2D GetNodeSize(const SGraphEditor& GraphEditor, const UEdGraphNode* Node)
{
	FSlateRect Rect;
	if (GraphEditor.GetBoundsForNode(Node, Rect, 0.f))
	{
		return FVector2D(Rect.Right - Rect.Left, Rect.Bottom - Rect.Top);
	}

	return FVector2D(Node->NodeWidth, Node->NodeHeight);
}

/** Helper class for aligning nodes */
struct FAlignmentHelper
{
	/** Construct from a graph editor, an orientation, and an alignment type */
	FAlignmentHelper(TSharedRef<SGraphEditor> InGraphEditor, EOrientation InOrientation, EAlignType InAlignType)
		: GraphEditor(MoveTemp(InGraphEditor))
	{
		// We align to the node that was clicked on, if available (not when invoked from a key shortcut)
		CardinalNode = GraphEditor->GetGraphNodeForMenu();

		Orientation = InOrientation;
		AlignType = InAlignType;

		// Collect all the alignment data for all the selected nodes
		for (UObject* It : GraphEditor->GetSelectedNodes())
		{
			if (UEdGraphNode* Node = Cast<UEdGraphNode>(It))
			{
				AlignmentData.Add(GetAlignmentDataForNode(Node));
			}
		}

		// Sort the data based on target - important for future algorithsm
		AlignmentData.Sort([](const FAlignmentData& A, const FAlignmentData& B){
			return A.GetTarget() < B.GetTarget();
		});
	}

	/** Align all the nodes */
	void Align()
	{
		if (AlignmentData.Num() > 1)
		{
			float Target = DetermineAlignmentTarget();

			for (FAlignmentData& Entry : AlignmentData)
			{
				Entry.Node->Modify();
				Entry.TargetProperty = Target - Entry.TargetOffset;
			}
		}
	}

private:

	/** Collect alignment data for a given node, based on our settings */
	FAlignmentData GetAlignmentDataForNode(UEdGraphNode* Node)
	{
		float PropertyOffset = 0.f;

		const float NodeSize = Orientation == Orient_Horizontal ? GetNodeSize(*GraphEditor, Node).X : GetNodeSize(*GraphEditor, Node).Y;
		switch(AlignType)
		{
			case EAlignType::Minimum:	PropertyOffset = 0.f;			break;
			case EAlignType::Middle:	PropertyOffset = NodeSize*.5f;	break;
			case EAlignType::Maximum:	PropertyOffset = NodeSize;		break;
		}
		int32* Property = Orientation == Orient_Horizontal ? &Node->NodePosX : &Node->NodePosY;
		return FAlignmentData(Node, *Property, PropertyOffset);
	}

	/** Determine the horizontal/vertical position that all nodes should align to */
	float DetermineAlignmentTarget()
	{
		if (CardinalNode)
		{
			return GetAlignmentDataForNode(CardinalNode).GetTarget();
		}

		if (AlignType == EAlignType::Minimum)
		{
			float Target = TNumericLimits<float>::Max();
			for (const FAlignmentData& Entry : AlignmentData)
			{
				Target = FMath::Min(Target, Entry.GetTarget());
			}
			return Target;
		}
		else if (AlignType == EAlignType::Maximum)
		{
			float Target = TNumericLimits<float>::Lowest();
			for (const FAlignmentData& Entry : AlignmentData)
			{
				Target = FMath::Max(Target, Entry.GetTarget());
			}
			return Target;
		}
		else
		{
			// Use the mean
			float SumTotal = 0.f;
			for (const FAlignmentData& Entry : AlignmentData)
			{
				SumTotal += Entry.GetTarget();
			}
			return SumTotal / AlignmentData.Num();
		}
	}

	/** The graph editor */
	TSharedRef<SGraphEditor> GraphEditor;
	/** Whether we are aligning horizontally/vertically */
	EOrientation Orientation;
	/** Whether we are aligning to the minimum/middle/maximum bounds */
	EAlignType AlignType;
	/** The cardinal node that all other nodes should align to (possibly null) */
	UEdGraphNode* CardinalNode;
	/** Generated alignment data */
	TArray<FAlignmentData> AlignmentData;
};

void FBlueprintEditor::OnAlignTop()
{
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (FocusedGraphEd.IsValid())
	{
		const FScopedTransaction Transaction(FGraphEditorCommands::Get().AlignNodesTop->GetLabel());

		FAlignmentHelper Helper(FocusedGraphEd.ToSharedRef(), Orient_Vertical, EAlignType::Minimum);
		Helper.Align();
	}
}

void FBlueprintEditor::OnAlignMiddle()
{
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (FocusedGraphEd.IsValid())
	{
		const FScopedTransaction Transaction(FGraphEditorCommands::Get().AlignNodesMiddle->GetLabel());

		FAlignmentHelper Helper(FocusedGraphEd.ToSharedRef(), Orient_Vertical, EAlignType::Middle);
		Helper.Align();
	}
}

void FBlueprintEditor::OnAlignBottom()
{
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (FocusedGraphEd.IsValid())
	{
		const FScopedTransaction Transaction(FGraphEditorCommands::Get().AlignNodesBottom->GetLabel());

		FAlignmentHelper Helper(FocusedGraphEd.ToSharedRef(), Orient_Vertical, EAlignType::Maximum);
		Helper.Align();
	}
}

void FBlueprintEditor::OnAlignLeft()
{
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (FocusedGraphEd.IsValid())
	{
		const FScopedTransaction Transaction(FGraphEditorCommands::Get().AlignNodesLeft->GetLabel());

		FAlignmentHelper Helper(FocusedGraphEd.ToSharedRef(), Orient_Horizontal, EAlignType::Minimum);
		Helper.Align();
	}
}

void FBlueprintEditor::OnAlignCenter()
{
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (FocusedGraphEd.IsValid())
	{
		const FScopedTransaction Transaction(FGraphEditorCommands::Get().AlignNodesCenter->GetLabel());

		FAlignmentHelper Helper(FocusedGraphEd.ToSharedRef(), Orient_Horizontal, EAlignType::Middle);
		Helper.Align();
	}
}

void FBlueprintEditor::OnAlignRight()
{
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (FocusedGraphEd.IsValid())
	{
		const FScopedTransaction Transaction(FGraphEditorCommands::Get().AlignNodesRight->GetLabel());

		FAlignmentHelper Helper(FocusedGraphEd.ToSharedRef(), Orient_Horizontal, EAlignType::Maximum);
		Helper.Align();
	}
}

void FBlueprintEditor::OnStraightenConnections()
{
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (FocusedGraphEd.IsValid())
	{
		const FScopedTransaction Transaction(FGraphEditorCommands::Get().StraightenConnections->GetLabel());

		if (UEdGraphPin* Pin = FocusedGraphEd->GetGraphPinForMenu())
		{
			FocusedGraphEd->StraightenConnections(Pin, UEdGraphSchema_K2::GetAndResetStraightenDestinationPin());
		}
		else
		{
			FocusedGraphEd->StraightenConnections();
		}
	}
}

/** Distribute the specified array of node data evenly */
void DistributeNodes(TArray<FAlignmentData>& InData)
{
	// Sort the data
	InData.Sort([](const FAlignmentData& A, const FAlignmentData& B){
		return A.TargetProperty + A.TargetOffset/2 < B.TargetProperty + B.TargetOffset/2;
	});

	// Measure the available space
	float TotalWidthOfNodes = 0.f;
	for (int32 Index = 1; Index < InData.Num() - 1; ++Index)
	{
		TotalWidthOfNodes += InData[Index].TargetOffset;
	}

	const float SpaceToDistributeIn = InData.Last().TargetProperty - InData[0].GetTarget();
	const float PaddingAmount = ((SpaceToDistributeIn - TotalWidthOfNodes) / (InData.Num() - 1));

	float TargetPosition = InData[0].GetTarget() + PaddingAmount;

	// Now set all the properties on the target
	for (int32 Index = 1; Index < InData.Num() - 1; ++Index)
	{
		FAlignmentData& Entry = InData[Index];

		Entry.Node->Modify();
		Entry.TargetProperty = TargetPosition;

		TargetPosition = Entry.GetTarget() + PaddingAmount;
	}
}

void FBlueprintEditor::OnDistributeNodesH()
{
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (!FocusedGraphEd.IsValid())
	{
		return;
	}
	
	TArray<FAlignmentData> AlignData;
	for (UObject* It : GetSelectedNodes())
	{
		if (UEdGraphNode* Node = Cast<UEdGraphNode>(It))
		{
			AlignData.Add(FAlignmentData(Node, Node->NodePosX, GetNodeSize(*FocusedGraphEd, Node).X));
		}
	}

	if (AlignData.Num() > 2)
	{
		const FScopedTransaction Transaction(FGraphEditorCommands::Get().DistributeNodesHorizontally->GetLabel());
		DistributeNodes(AlignData);
	}
}

void FBlueprintEditor::OnDistributeNodesV()
{
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (!FocusedGraphEd.IsValid())
	{
		return;
	}
	
	TArray<FAlignmentData> AlignData;
	for (UObject* It : GetSelectedNodes())
	{
		if (UEdGraphNode* Node = Cast<UEdGraphNode>(It))
		{
			AlignData.Add(FAlignmentData(Node, Node->NodePosY, GetNodeSize(*FocusedGraphEd, Node).Y));
		}
	}

	if (AlignData.Num() > 2)
	{
		const FScopedTransaction Transaction(FGraphEditorCommands::Get().DistributeNodesVertically->GetLabel());
		DistributeNodes(AlignData);
	}
}

void FBlueprintEditor::SelectAllNodes()
{
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (FocusedGraphEd.IsValid())
	{
		FocusedGraphEd->SelectAllNodes();
	}
}

bool FBlueprintEditor::CanSelectAllNodes() const
{
	return true;
}

void FBlueprintEditor::DeleteSelectedNodes()
{
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (!FocusedGraphEd.IsValid())
	{
		return;
	}

	const FScopedTransaction Transaction( FGenericCommands::Get().Delete->GetDescription() );
	FocusedGraphEd->GetCurrentGraph()->Modify();

	bool bNeedToModifyStructurally = false;

	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();

	SetUISelectionState(NAME_None);

	for (FGraphPanelSelectionSet::TConstIterator NodeIt( SelectedNodes ); NodeIt; ++NodeIt)
	{
		if (UEdGraphNode* Node = Cast<UEdGraphNode>(*NodeIt))
		{
			if (Node->CanUserDeleteNode())
			{
				UK2Node* K2Node = Cast<UK2Node>(Node);
				if (K2Node != NULL && K2Node->NodeCausesStructuralBlueprintChange())
				{
					bNeedToModifyStructurally = true;
				}

				if (UK2Node_Composite* SelectedNode = Cast<UK2Node_Composite>(*NodeIt))
				{
					//Close the tab for the composite if it was open
					if (SelectedNode->BoundGraph)
					{
						DocumentManager->CleanInvalidTabs();
					}
				}
				else if (UK2Node_Timeline* TimelineNode = Cast<UK2Node_Timeline>(*NodeIt))
				{
					DocumentManager->CleanInvalidTabs();
				}
				AnalyticsTrackNodeEvent( GetBlueprintObj(), Node, true );
				FBlueprintEditorUtils::RemoveNode(GetBlueprintObj(), Node, true);
			}
		}
	}

	if (bNeedToModifyStructurally)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprintObj());
	}
	else
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(GetBlueprintObj());
	}

	//@TODO: Reselect items that were not deleted
}

bool FBlueprintEditor::CanDeleteNodes() const
{
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();

	bool bCanUserDeleteNode = false;

	if(IsEditable(GetFocusedGraph()) && SelectedNodes.Num() > 0)
	{
		for( UObject* NodeObject : SelectedNodes )
		{
			// If any nodes allow deleting, then do not disable the delete option
			UEdGraphNode* Node = Cast<UEdGraphNode>(NodeObject);
			if(Node->CanUserDeleteNode())
			{
				bCanUserDeleteNode = true;
				break;
			}
		}
	}

	return bCanUserDeleteNode;
}

void FBlueprintEditor::DeleteSelectedDuplicatableNodes()
{
	// Cache off the old selection
	const FGraphPanelSelectionSet OldSelectedNodes = GetSelectedNodes();
	
	// Clear the selection and only select the nodes that can be duplicated
	FGraphPanelSelectionSet CurrentSelection;
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (FocusedGraphEd.IsValid())
	{
		FocusedGraphEd->ClearSelectionSet();

		FGraphPanelSelectionSet RemainingNodes;
		for (FGraphPanelSelectionSet::TConstIterator SelectedIter(OldSelectedNodes); SelectedIter; ++SelectedIter)
		{
			UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);
			if ((Node != NULL) && Node->CanDuplicateNode())
			{
				FocusedGraphEd->SetNodeSelection(Node, true);
			}
			else
			{
				RemainingNodes.Add(Node);
			}
		}

		// Delete the duplicatable nodes
		DeleteSelectedNodes();

		// Reselect whatever's left from the original selection after the deletion
		FocusedGraphEd->ClearSelectionSet();

		for (FGraphPanelSelectionSet::TConstIterator SelectedIter(RemainingNodes); SelectedIter; ++SelectedIter)
		{
			if (UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter))
			{
				FocusedGraphEd->SetNodeSelection(Node, true);
			}
		}
	}
}

void FBlueprintEditor::CutSelectedNodes()
{
	CopySelectedNodes();
	// Cut should only delete nodes that can be duplicated
	DeleteSelectedDuplicatableNodes();
}

bool FBlueprintEditor::CanCutNodes() const
{
	return CanCopyNodes() && CanDeleteNodes();
}

void FBlueprintEditor::CopySelectedNodes()
{
	// Export the selected nodes and place the text on the clipboard
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();

	FString ExportedText;

	for (FGraphPanelSelectionSet::TConstIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
	{
		if(UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter))
		{
			Node->PrepareForCopying();
		}
	}

	FEdGraphUtilities::ExportNodesToText(SelectedNodes, /*out*/ ExportedText);
	FPlatformApplicationMisc::ClipboardCopy(*ExportedText);
}

bool FBlueprintEditor::CanCopyNodes() const
{
	// If any of the nodes can be duplicated then we should allow copying
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	for (FGraphPanelSelectionSet::TConstIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
	{
		UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);
		if ((Node != NULL) && Node->CanDuplicateNode())
		{
			return true;
		}
	}
	return false;
}

void FBlueprintEditor::PasteNodes()
{
	// Find the graph editor with focus
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (!FocusedGraphEd.IsValid())
	{
		return;
	}

	PasteNodesHere(FocusedGraphEd->GetCurrentGraph(), FocusedGraphEd->GetPasteLocation());

	// Dump any temporary pre-compile warnings to the compiler log.
	UBlueprint* BlueprintObj = GetBlueprintObj();
	if (BlueprintObj->PreCompileLog.IsValid())
	{
		DumpMessagesToCompilerLog(BlueprintObj->PreCompileLog->Messages, true);
	}
}


/**
 *	When copying and pasting functions from the LSBP operating on an instance to a class BP, 
 *	we should automatically transfer the functions from actors to the components
 */
struct FUpdatePastedNodes
{
	TSet<UK2Node_VariableGet*> AddedTargets;
	TSet<UK2Node_CallFunction*> AddedFunctions;
	TSet<UK2Node_Literal*> ReplacedTargets;
	TSet<UK2Node_CallFunctionOnMember*> ReplacedFunctions;

	UClass* CurrentClass;
	UEdGraph* Graph;
	TSet<UEdGraphNode*>& PastedNodes;
	const UEdGraphSchema_K2* K2Schema;

	FUpdatePastedNodes(UClass* InCurrentClass, TSet<UEdGraphNode*>& InPastedNodes, UEdGraph* InDestinationGraph)
		: CurrentClass(InCurrentClass)
		, Graph(InDestinationGraph)
		, PastedNodes(InPastedNodes)
		, K2Schema(GetDefault<UEdGraphSchema_K2>())
	{
		check(InCurrentClass && InDestinationGraph && K2Schema);
	}

	/**
	 *	Replace UK2Node_CallFunctionOnMember called on actor with a UK2Node_CallFunction. 
	 *	When the blueprint has the member.
	 */
	void ReplaceAll()
	{
		for(UEdGraphNode* PastedNode : PastedNodes)
		{
			if (UK2Node_CallFunctionOnMember* CallOnMember = Cast<UK2Node_CallFunctionOnMember>(PastedNode))
			{
				if (UEdGraphPin* TargetInPin = CallOnMember->FindPin(K2Schema->PN_Self))
				{
					const UClass* TargetClass = Cast<const UClass>(TargetInPin->PinType.PinSubCategoryObject.Get());

					const bool bTargetIsNullOrSingleLinked = (TargetInPin->LinkedTo.Num() == 1) ||
						(!TargetInPin->LinkedTo.Num() && !TargetInPin->DefaultObject);

					const bool bCanCurrentBlueprintReplace = TargetClass
						&& CurrentClass->IsChildOf(TargetClass) // If current class if of the same type, it has the called member
						&& (!CallOnMember->MemberVariableToCallOn.IsSelfContext() && (TargetClass != CurrentClass)) // Make sure the class isn't self, using a explicit check in case the class hasn't been compiled since the member was added
						&& bTargetIsNullOrSingleLinked;

					if (bCanCurrentBlueprintReplace) 
					{
						UEdGraphNode* TargetNode = TargetInPin->LinkedTo.Num() ? TargetInPin->LinkedTo[0]->GetOwningNode() : NULL;
						UK2Node_Literal* TargetLiteralNode = Cast<UK2Node_Literal>(TargetNode);

						const bool bPastedNodeShouldBeReplacedWithTarget = TargetLiteralNode
							&& !TargetLiteralNode->GetObjectRef() //The node delivering target actor is invalid
							&& PastedNodes.Contains(TargetLiteralNode);
						const bool bPastedNodeShouldBeReplacedWithoutTarget = !TargetNode || !PastedNodes.Contains(TargetNode);

						if (bPastedNodeShouldBeReplacedWithTarget || bPastedNodeShouldBeReplacedWithoutTarget)
						{
							Replace(TargetLiteralNode, CallOnMember);
						}
					}
				}
			}
		}

		UpdatePastedCollection();
	}

private:

	void UpdatePastedCollection()
	{
		for (UK2Node_Literal* ReplacedTarget : ReplacedTargets)
		{
			if (ReplacedTarget && ReplacedTarget->GetValuePin() && !ReplacedTarget->GetValuePin()->LinkedTo.Num())
			{
				PastedNodes.Remove(ReplacedTarget);
				Graph->RemoveNode(ReplacedTarget);
			}
		}
		for (UK2Node_CallFunctionOnMember* ReplacedFunction : ReplacedFunctions)
		{
			PastedNodes.Remove(ReplacedFunction);
			Graph->RemoveNode(ReplacedFunction);
		}
		for (UK2Node_VariableGet* AddedTarget : AddedTargets)
		{
			PastedNodes.Add(AddedTarget);
		}
		for (UK2Node_CallFunction* AddedFunction : AddedFunctions)
		{
			PastedNodes.Add(AddedFunction);
		}
	}

	bool MoveAllLinksExeptSelf(UK2Node* NewNode, UK2Node* OldNode)
	{
		bool bResult = true;
		for(UEdGraphPin* OldPin : OldNode->Pins)
		{
			if(OldPin && (OldPin->PinName != K2Schema->PN_Self))
			{
				UEdGraphPin* NewPin = NewNode->FindPin(OldPin->PinName);
				if (NewPin)
				{
					if (!K2Schema->MovePinLinks(*OldPin, *NewPin).CanSafeConnect())
					{
						UE_LOG(LogBlueprint, Error, TEXT("FUpdatePastedNodes: Cannot connect pin '%s' node '%s'"),
							*OldPin->PinName, *OldNode->GetName());
						bResult = false;
					}
				}
				else
				{
					UE_LOG(LogBlueprint, Error, TEXT("FUpdatePastedNodes: Cannot find pin '%s'"), *OldPin->PinName);
					bResult = false;
				}
			}
		}
		return bResult;
	}

	void InitializeNewNode(UK2Node* NewNode, UK2Node* OldNode, float NodePosX = 0.0f, float NodePosY = 0.0f)
	{	
		NewNode->NodePosX = OldNode ? OldNode->NodePosX : NodePosX;
		NewNode->NodePosY = OldNode ? OldNode->NodePosY : NodePosY;
		NewNode->SetFlags(RF_Transactional);
		Graph->AddNode(NewNode, false, false);
		NewNode->PostPlacedNewNode();
		NewNode->AllocateDefaultPins();
	}

	bool Replace(UK2Node_Literal* OldTarget, UK2Node_CallFunctionOnMember* OldCall)
	{
		bool bResult = true;
		check(OldCall);

		UK2Node_VariableGet* NewTarget = NULL;
		
		const UProperty* Property = OldCall->MemberVariableToCallOn.ResolveMember<UProperty>((UClass*)NULL);
		for (UK2Node_VariableGet* AddedTarget : AddedTargets)
		{
			if (AddedTarget && (Property == AddedTarget->VariableReference.ResolveMember<UProperty>(CurrentClass)))
			{
				NewTarget = AddedTarget;
				break;
			}
		}

		if (!NewTarget)
		{
			NewTarget = NewObject<UK2Node_VariableGet>(Graph);
			check(NewTarget);
			NewTarget->SetFromProperty(Property, true);
			AddedTargets.Add(NewTarget);
			const float AutoNodeOffsetX = 160.0f;
			InitializeNewNode(NewTarget, OldTarget, OldCall->NodePosX - AutoNodeOffsetX, OldCall->NodePosY);
		}

		if (OldTarget)
		{
			ReplacedTargets.Add(OldTarget);
		}

		UK2Node_CallFunction* NewCall = NewObject<UK2Node_CallFunction>(Graph);
		check(NewCall);
		NewCall->SetFromFunction(OldCall->GetTargetFunction());
		InitializeNewNode(NewCall, OldCall);
		AddedFunctions.Add(NewCall);

		if (!MoveAllLinksExeptSelf(NewCall, OldCall))
		{
			bResult = false;
		}

		if (NewTarget)
		{
			UEdGraphPin* SelfPin = NewCall->FindPinChecked(K2Schema->PN_Self);
			if (!K2Schema->TryCreateConnection(SelfPin, NewTarget->GetValuePin()))
			{
				UE_LOG(LogBlueprint, Error, TEXT("FUpdatePastedNodes: Cannot connect new self."));
				bResult = false;
			}
		}

		OldCall->BreakAllNodeLinks();

		ReplacedFunctions.Add(OldCall);
		return bResult;
	}
};

void FBlueprintEditor::PasteNodesHere(class UEdGraph* DestinationGraph, const FVector2D& GraphLocation)
{
	// Find the graph editor with focus
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (!FocusedGraphEd.IsValid())
	{
		return;
	}
	// Select the newly pasted stuff
	bool bNeedToModifyStructurally = false;
	{
		const FScopedTransaction Transaction(FGenericCommands::Get().Paste->GetDescription());
		DestinationGraph->Modify();

		// Clear the selection set (newly pasted stuff will be selected)
		SetUISelectionState(NAME_None);

		// Grab the text to paste from the clipboard.
		FString TextToImport;
		FPlatformApplicationMisc::ClipboardPaste(TextToImport);

		// Import the nodes
		TSet<UEdGraphNode*> PastedNodes;
		FEdGraphUtilities::ImportNodesFromText(DestinationGraph, TextToImport, /*out*/ PastedNodes);

		// Update Paste Analytics
		AnalyticsStats.NodePasteCreateCount += PastedNodes.Num();

		{
			UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(DestinationGraph);
			UClass* CurrentClass = Blueprint ? Blueprint->GeneratedClass : nullptr;
			if (CurrentClass)
			{
				FUpdatePastedNodes ReplaceNodes(CurrentClass, PastedNodes, DestinationGraph);
				ReplaceNodes.ReplaceAll();
			}
		}

		//Average position of nodes so we can move them while still maintaining relative distances to each other
		FVector2D AvgNodePosition(0.0f, 0.0f);

		for (TSet<UEdGraphNode*>::TIterator It(PastedNodes); It; ++It)
		{
			UEdGraphNode* Node = *It;
			AvgNodePosition.X += Node->NodePosX;
			AvgNodePosition.Y += Node->NodePosY;
		}

		float InvNumNodes = 1.0f / float(PastedNodes.Num());
		AvgNodePosition.X *= InvNumNodes;
		AvgNodePosition.Y *= InvNumNodes;

		for (TSet<UEdGraphNode*>::TIterator It(PastedNodes); It; ++It)
		{
			UEdGraphNode* Node = *It;
			FocusedGraphEd->SetNodeSelection(Node, true);

			Node->NodePosX = (Node->NodePosX - AvgNodePosition.X) + GraphLocation.X;
			Node->NodePosY = (Node->NodePosY - AvgNodePosition.Y) + GraphLocation.Y;

			Node->SnapToGrid(SNodePanel::GetSnapGridSize());

			// Give new node a different Guid from the old one
			Node->CreateNewGuid();

			UK2Node* K2Node = Cast<UK2Node>(Node);
			if ((K2Node != NULL) && K2Node->NodeCausesStructuralBlueprintChange())
			{
				bNeedToModifyStructurally = true;
			}

			// For pasted Event nodes, we need to see if there is an already existing node in a ghost state that needs to be cleaned up
			if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
			{
				// Gather all existing event nodes
				TArray<UK2Node_Event*> ExistingEventNodes;
				FBlueprintEditorUtils::GetAllNodesOfClass<UK2Node_Event>(GetBlueprintObj(), ExistingEventNodes);

				for (UK2Node_Event* ExistingEventNode : ExistingEventNodes)
				{
					check(ExistingEventNode);

					const bool bIdenticalNode = (EventNode != ExistingEventNode) && ExistingEventNode->bOverrideFunction && UK2Node_Event::AreEventNodesIdentical(EventNode, ExistingEventNode);

					// Check if the nodes are identical, if they are we need to delete the original because it is disabled. Identical nodes that are in an enabled state will never make it this far and still be enabled.
					if (bIdenticalNode)
					{
						// Should not have made it to being a pasted node if the pre-existing node wasn't disabled or was otherwise explicitly disabled by the user.
						ensure(ExistingEventNode->IsAutomaticallyPlacedGhostNode());

						// Destroy the pre-existing node, we do not need it.
						ExistingEventNode->DestroyNode();
					}
				}
			}
			// Log new node created to analytics
			AnalyticsTrackNodeEvent(GetBlueprintObj(), Node, false);
		}
	}

	if (bNeedToModifyStructurally)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprintObj());
	}
	else
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(GetBlueprintObj());
	}

	// Update UI
	FocusedGraphEd->NotifyGraphChanged();
}


bool FBlueprintEditor::CanPasteNodes() const
{
	// Do not allow pasting into interface Blueprints
	if (GetBlueprintObj()->BlueprintType == BPTYPE_Interface)
	{
		return false;
	}

	// Find the graph editor with focus
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (!FocusedGraphEd.IsValid())
	{
		return false;
	}

	FString ClipboardContent;
	FPlatformApplicationMisc::ClipboardPaste(ClipboardContent);

	return IsEditable(GetFocusedGraph()) && FEdGraphUtilities::CanImportNodesFromText(FocusedGraphEd->GetCurrentGraph(), ClipboardContent);
}

void FBlueprintEditor::DuplicateNodes()
{
	// Copy and paste current selection
	CopySelectedNodes();
	PasteNodes();
}

bool FBlueprintEditor::CanDuplicateNodes() const
{
	return CanCopyNodes() && IsEditable(GetFocusedGraph());
}

void FBlueprintEditor::OnAssignReferencedActor()
{
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	USelection* SelectedActors = GEditor->GetSelectedActors();
	if ( SelectedNodes.Num() > 0 && SelectedActors != NULL && SelectedActors->Num() == 1 )
	{
		AActor* SelectedActor = Cast<AActor>( SelectedActors->GetSelectedObject(0) );
		if ( SelectedActor != NULL )
		{
			TArray<UK2Node_ActorBoundEvent*> NodesToAlter;

			for ( FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt )
			{
				UK2Node_ActorBoundEvent* SelectedNode = Cast<UK2Node_ActorBoundEvent>(*NodeIt);
				if ( SelectedNode != NULL )
				{
					NodesToAlter.Add( SelectedNode );
				}
			}

			// only create a transaction if there is a node that is affected.
			if ( NodesToAlter.Num() > 0 )
			{
				const FScopedTransaction Transaction( LOCTEXT("AssignReferencedActor", "Assign referenced Actor") );
				{
					for ( int32 NodeIndex = 0; NodeIndex < NodesToAlter.Num(); NodeIndex++ )
					{
						UK2Node_ActorBoundEvent* CurrentEvent = NodesToAlter[NodeIndex];

						// Store the node's current state and replace the referenced actor
						CurrentEvent->Modify();
						CurrentEvent->EventOwner = SelectedActor;
						CurrentEvent->ReconstructNode();
					}
					FBlueprintEditorUtils::MarkBlueprintAsModified(GetBlueprintObj());
				}
			}
		}
	}
}

bool FBlueprintEditor::CanAssignReferencedActor() const
{
	bool bWouldAssignActors = false;

	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	if ( SelectedNodes.Num() > 0 )
	{
		USelection* SelectedActors = GEditor->GetSelectedActors();

		// If there is only one actor selected and at least one Blueprint graph
		// node is able to receive the assignment then return true.
		if ( SelectedActors != NULL && SelectedActors->Num() == 1 )
		{
			AActor* SelectedActor = Cast<AActor>( SelectedActors->GetSelectedObject(0) );
			if ( SelectedActor != NULL )
			{
				for ( FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt )
				{
					UK2Node_ActorBoundEvent* SelectedNode = Cast<UK2Node_ActorBoundEvent>(*NodeIt);
					if ( SelectedNode != NULL )
					{
						if ( SelectedNode->EventOwner != SelectedActor )
						{
							bWouldAssignActors = true;
							break;
						}
					}
				}
			}
		}
	}

	return bWouldAssignActors;
}

void FBlueprintEditor::OnSelectReferenceInLevel()
{
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();

	if (SelectedNodes.Num() > 0)
	{
		TArray<AActor*> ActorsToSelect;

		// Iterate over all nodes, and select referenced actors.
		for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
		{
			UK2Node* SelectedNode = Cast<UK2Node>(*NodeIt);
			AActor* ReferencedActor = (SelectedNode) ? SelectedNode->GetReferencedLevelActor() : NULL;

			if (ReferencedActor != NULL)
			{
				ActorsToSelect.AddUnique(ReferencedActor);
			}
		}
		// If we found any actors to select clear the existing selection, select them and move the camera to show them.
		if( ActorsToSelect.Num() != 0 )
		{
			// First clear the previous selection
			GEditor->GetSelectedActors()->Modify();
			GEditor->SelectNone( false, true );

			// Now select the actors.
			for (int32 iActor = 0; iActor < ActorsToSelect.Num(); iActor++)
			{
				GEditor->SelectActor(ActorsToSelect[ iActor ], true, true, false);
			}

			// Execute the command to move camera to the object(s).
			GUnrealEd->Exec_Camera( TEXT("ALIGN ACTIVEVIEWPORTONLY"),*GLog); 
		}
	}
}

bool FBlueprintEditor::CanSelectReferenceInLevel() const
{
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();

	bool bCanSelectActors = false;
	if (SelectedNodes.Num() > 0)
	{
		// Iterate over all nodes, testing if they're pointing to actors.
		for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
		{
			UK2Node* SelectedNode = Cast<UK2Node>(*NodeIt);
			const AActor* ReferencedActor = (SelectedNode) ? SelectedNode->GetReferencedLevelActor() : NULL;

			bCanSelectActors = (ReferencedActor != NULL);
			if (ReferencedActor == NULL)
			{
				// Bail early if the selected node isn't referencing an actor
				return false;
			}
		}
	}

	return bCanSelectActors;
}

// Utility helper to get the currently hovered pin in the currently visible graph, or NULL if there isn't one
UEdGraphPin* FBlueprintEditor::GetCurrentlySelectedPin() const
{
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (FocusedGraphEd.IsValid())
	{
		return FocusedGraphEd->GetGraphPinForMenu();
	}

	return NULL;
}

void FBlueprintEditor::RegisterSCSEditorCustomization(const FName& InComponentName, TSharedPtr<ISCSEditorCustomization> InCustomization)
{
	SCSEditorCustomizations.Add(InComponentName, InCustomization);
}

void FBlueprintEditor::UnregisterSCSEditorCustomization(const FName& InComponentName)
{
	SCSEditorCustomizations.Remove(InComponentName);
}

void FBlueprintEditor::CreateMergeToolTab()
{
	MergeTool = IMerge::Get().GenerateMergeWidget(*GetBlueprintObj(), SharedThis(this));
}

void FBlueprintEditor::CreateMergeToolTab(const UBlueprint* BaseBlueprint, const UBlueprint* RemoteBlueprint, const FOnMergeResolved& ResolutionCallback)
{
	OnMergeResolved = ResolutionCallback;
	MergeTool = IMerge::Get().GenerateMergeWidget(BaseBlueprint, RemoteBlueprint, GetBlueprintObj(), ResolutionCallback, SharedThis(this));
}

void FBlueprintEditor::CloseMergeTool()
{
	TSharedPtr<SDockTab> MergeToolPtr = MergeTool.Pin();
	if( MergeToolPtr.IsValid() )
	{
		UBlueprint* Blueprint = GetBlueprintObj();
		UPackage* BpPackage = (Blueprint == nullptr) ? nullptr : Blueprint->GetOutermost();
		// @TODO: right now crashes the editor on closing of the BP editor
		//OnMergeResolved.ExecuteIfBound(BpPackage, EMergeResult::Unknown);
		OnMergeResolved.Unbind();

		MergeToolPtr->RequestCloseTab();
	}
}

TArray<FSCSEditorTreeNodePtrType>  FBlueprintEditor::GetSelectedSCSEditorTreeNodes() const
{
	TArray<FSCSEditorTreeNodePtrType>  Nodes;
	if (SCSEditor.IsValid())
	{
		Nodes = SCSEditor->GetSelectedNodes();
	}
	return Nodes;
}

FSCSEditorTreeNodePtrType FBlueprintEditor::FindAndSelectSCSEditorTreeNode(const UActorComponent* InComponent, bool IsCntrlDown) 
{
	FSCSEditorTreeNodePtrType NodePtr;

	if (SCSEditor.IsValid())
	{
		NodePtr = SCSEditor->GetNodeFromActorComponent(InComponent);
		if(NodePtr.IsValid())
		{
			SCSEditor->SelectNode(NodePtr, IsCntrlDown);
		}
	}

	return NodePtr;
}

void FBlueprintEditor::OnDisallowedPinConnection(const UEdGraphPin* PinA, const UEdGraphPin* PinB)
{
	FDisallowedPinConnection NewRecord;
	NewRecord.PinTypeCategoryA = PinA->PinType.PinCategory;
	NewRecord.bPinIsArrayA = PinA->PinType.IsArray();
	NewRecord.bPinIsReferenceA = PinA->PinType.bIsReference;
	NewRecord.bPinIsWeakPointerA = PinA->PinType.bIsWeakPointer;
	NewRecord.PinTypeCategoryB = PinB->PinType.PinCategory;
	NewRecord.bPinIsArrayB = PinB->PinType.IsArray();
	NewRecord.bPinIsReferenceB = PinB->PinType.bIsReference;
	NewRecord.bPinIsWeakPointerB = PinB->PinType.bIsWeakPointer;
	AnalyticsStats.GraphDisallowedPinConnections.Add(NewRecord);
}

void FBlueprintEditor::OnStartWatchingPin()
{
	if (UEdGraphPin* Pin = GetCurrentlySelectedPin())
	{
		// Follow an input back to it's output
		if ((Pin->Direction == EGPD_Input) && (Pin->LinkedTo.Num() > 0))
		{
			Pin = Pin->LinkedTo[0];
		}

		// Start watching it
		FKismetDebugUtilities::TogglePinWatch(GetBlueprintObj(), Pin);
	}
}

bool FBlueprintEditor::CanStartWatchingPin() const
{
	if (UEdGraphPin* Pin = GetCurrentlySelectedPin())
	{
		// Follow an input back to it's output
		if ((Pin->Direction == EGPD_Input) && (Pin->LinkedTo.Num() > 0))
		{
			Pin = Pin->LinkedTo[0];
		}

		return FKismetDebugUtilities::CanWatchPin(GetBlueprintObj(), Pin);
	}
	return false;
}

void FBlueprintEditor::OnStopWatchingPin()
{
	if (UEdGraphPin* Pin = GetCurrentlySelectedPin())
	{
		// Follow an input back to it's output
		if ((Pin->Direction == EGPD_Input) && (Pin->LinkedTo.Num() > 0))
		{
			Pin = Pin->LinkedTo[0];
		}

		FKismetDebugUtilities::TogglePinWatch(GetBlueprintObj(), Pin);
	}
}

bool FBlueprintEditor::CanStopWatchingPin() const
{
	if (UEdGraphPin* Pin = GetCurrentlySelectedPin())
	{
		// Follow an input back to it's output
		if ((Pin->Direction == EGPD_Input) && (Pin->LinkedTo.Num() > 0))
		{
			Pin = Pin->LinkedTo[0];
		}

		return FKismetDebugUtilities::IsPinBeingWatched(GetBlueprintObj(), Pin);
	}

	return false;
}

bool FBlueprintEditor::CanGoToDefinition() const
{
	const UEdGraphNode* Node = GetSingleSelectedNode();
	return (Node != nullptr) && Node->CanJumpToDefinition();
}

void FBlueprintEditor::OnGoToDefinition()
{
	if (UEdGraphNode* SelectedGraphNode = GetSingleSelectedNode())
	{
		OnNodeDoubleClicked(SelectedGraphNode);
	}
}

FString FBlueprintEditor::GetDocLinkForSelectedNode()
{
	FString DocumentationLink;

	if (const UEdGraphNode* SelectedGraphNode = GetSingleSelectedNode())
	{
		const FString DocLink = SelectedGraphNode->GetDocumentationLink();
		const FString DocExcerpt = SelectedGraphNode->GetDocumentationExcerptName();

		if (!DocLink.IsEmpty() && !DocExcerpt.IsEmpty())
		{
			DocumentationLink = FEditorClassUtils::GetDocumentationLinkFromExcerpt(DocLink, DocExcerpt);
		}
	}

	return DocumentationLink;
}

void FBlueprintEditor::OnGoToDocumentation()
{
	const FString DocumentationLink = GetDocLinkForSelectedNode();
	if (!DocumentationLink.IsEmpty())
	{
		IDocumentation::Get()->Open(DocumentationLink, FDocumentationSourceInfo(TEXT("rightclick_bpnode")));
	}
}

bool FBlueprintEditor::CanGoToDocumentation()
{
	FString DocumentationLink = GetDocLinkForSelectedNode();
	return !DocumentationLink.IsEmpty();
}

void FBlueprintEditor::OnSetEnabledStateForSelectedNodes(ENodeEnabledState NewState)
{
	const FScopedTransaction Transaction(LOCTEXT("SetNodeEnabledState", "Set Node Enabled State"));

	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	for (UObject* SelectedNode : SelectedNodes)
	{
		if (UEdGraphNode* SelectedGraphNode = Cast<UEdGraphNode>(SelectedNode))
		{
			SelectedGraphNode->Modify();
			SelectedGraphNode->SetEnabledState(NewState);
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprintObj());
}

ECheckBoxState FBlueprintEditor::GetEnabledCheckBoxStateForSelectedNodes()
{
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	ECheckBoxState Result = SelectedNodes.Num() > 0 ? ECheckBoxState::Undetermined : ECheckBoxState::Unchecked;
	for (UObject* SelectedNode : SelectedNodes)
	{
		UEdGraphNode* SelectedGraphNode = Cast<UEdGraphNode>(SelectedNode);
		if(SelectedGraphNode)
		{
			const bool bIsSelectedNodeEnabled = SelectedGraphNode->IsNodeEnabled();
			if(Result == ECheckBoxState::Undetermined)
			{
				Result = bIsSelectedNodeEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			}
			else if((!bIsSelectedNodeEnabled && Result == ECheckBoxState::Checked)
				|| (bIsSelectedNodeEnabled && Result == ECheckBoxState::Unchecked))
			{
				Result = ECheckBoxState::Undetermined;
				break;
			}
		}
	}

	return Result;
}

ECheckBoxState FBlueprintEditor::CheckEnabledStateForSelectedNodes(ENodeEnabledState CheckState)
{
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	ECheckBoxState Result = (SelectedNodes.Num() > 0) ? ECheckBoxState::Undetermined : ECheckBoxState::Unchecked;
	for (UObject* SelectedNode : SelectedNodes)
	{
		if (UEdGraphNode* SelectedGraphNode = Cast<UEdGraphNode>(SelectedNode))
		{
			const ENodeEnabledState NodeState = SelectedGraphNode->GetDesiredEnabledState();
			if (Result == ECheckBoxState::Undetermined)
			{
				Result = (NodeState == CheckState) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			}
			else if ((NodeState != CheckState && Result == ECheckBoxState::Checked) || (NodeState == CheckState && Result == ECheckBoxState::Unchecked))
			{
				Result = ECheckBoxState::Undetermined;
				break;
			}
		}
	}

	return Result;
}

void FBlueprintEditor::ToggleSaveIntermediateBuildProducts()
{
	bSaveIntermediateBuildProducts = !bSaveIntermediateBuildProducts;
}

bool FBlueprintEditor::GetSaveIntermediateBuildProducts() const
{
	return bSaveIntermediateBuildProducts;
}

void FBlueprintEditor::OnNodeDoubleClicked(UEdGraphNode* Node)
{
	if (Node->CanJumpToDefinition())
	{
		Node->JumpToDefinition();
	}
}

void FBlueprintEditor::ExtractEventTemplateForFunction(class UK2Node_CustomEvent* InCustomEvent, UEdGraphNode* InGatewayNode, class UK2Node_EditablePinBase* InEntryNode, class UK2Node_EditablePinBase* InResultNode, TSet<UEdGraphNode*>& InCollapsableNodes)
{
	check(InCustomEvent);

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	for(UEdGraphPin* Pin : InCustomEvent->Pins)
	{
		if(Pin->PinType.PinCategory == K2Schema->PC_Exec)
		{
			TArray< UEdGraphPin* > PinLinkList = Pin->LinkedTo;
			for( UEdGraphPin* PinLink : PinLinkList)
			{
				if(!InCollapsableNodes.Contains(PinLink->GetOwningNode()))
				{
					InGatewayNode->Modify();
					Pin->Modify();
					PinLink->Modify();

					K2Schema->MovePinLinks(*Pin, *K2Schema->FindExecutionPin(*InGatewayNode, EGPD_Output));
				}
			}
		}
		else if(Pin->PinType.PinCategory != K2Schema->PC_Delegate)
		{

			TArray< UEdGraphPin* > PinLinkList = Pin->LinkedTo;
			for( UEdGraphPin* PinLink : PinLinkList)
			{
				if(!InCollapsableNodes.Contains(PinLink->GetOwningNode()))
				{
					InGatewayNode->Modify();
					Pin->Modify();
					PinLink->Modify();

					FString PortName = Pin->PinName + TEXT("_Out");
					UEdGraphPin* RemotePortPin = InGatewayNode->FindPin(PortName);
					// For nodes that are connected to the event but not collapsing into the graph, they need to create a pin on the result.
					if(RemotePortPin == nullptr)
					{
						FString UniquePortName = InGatewayNode->CreateUniquePinName(PortName);

						RemotePortPin = InGatewayNode->CreatePin(Pin->Direction, Pin->PinType,UniquePortName);
						InResultNode->CreateUserDefinedPin(UniquePortName, Pin->PinType, EGPD_Input);
					}
					PinLink->BreakAllPinLinks();
					PinLink->MakeLinkTo(RemotePortPin);
				}
				else
				{
					InEntryNode->Modify();

					FString UniquePortName = InGatewayNode->CreateUniquePinName(Pin->PinName);
					InEntryNode->CreateUserDefinedPin(UniquePortName, Pin->PinType, EGPD_Output);
				}
			}
		}
	}
}

void FBlueprintEditor::CollapseNodesIntoGraph(UEdGraphNode* InGatewayNode, UK2Node_EditablePinBase* InEntryNode, UK2Node_EditablePinBase* InResultNode, UEdGraph* InSourceGraph, UEdGraph* InDestinationGraph, TSet<UEdGraphNode*>& InCollapsableNodes, bool bCanDiscardEmptyReturnNode, bool bCanHaveWeakObjPtrParam)
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	// Keep track of the statistics of the node positions so the new nodes can be located reasonably well
	float SumNodeX = 0.0f;
	float SumNodeY = 0.0f;
	float MinNodeX = 1e9f;
	float MinNodeY = 1e9f;
	float MaxNodeX = -1e9f;
	float MaxNodeY = -1e9f;

	UEdGraphNode* InterfaceTemplateNode = nullptr;

	// If our return node only contains an exec pin, then we don't need to add it
	// This helps to mitigate cases where it is unclear which exec pins should be connected to the return node
	bool bDiscardReturnNode = true;

	// For collapsing to functions can use a single event as a template for the function. This event MUST be deleted at the end, and the pins pre-generated. 
	if(InGatewayNode->GetClass() == UK2Node_CallFunction::StaticClass())
	{
		for (UEdGraphNode* Node : InCollapsableNodes)
		{
			if (UK2Node_CustomEvent* const CustomEvent = Cast<UK2Node_CustomEvent>(Node))
			{
				check(!InterfaceTemplateNode);

				InterfaceTemplateNode = CustomEvent;
				InterfaceTemplateNode->Modify();

				ExtractEventTemplateForFunction(CustomEvent, InGatewayNode, InEntryNode, InResultNode, InCollapsableNodes);

				FString GraphName = FBlueprintEditorUtils::GenerateUniqueGraphName(GetBlueprintObj(), CustomEvent->GetNodeTitle(ENodeTitleType::ListView).ToString()).ToString();
				FBlueprintEditorUtils::RenameGraph(InDestinationGraph, GraphName);

				// Remove the node, it has no place in the new graph
				InCollapsableNodes.Remove(Node);
				break;
			}
		}
	}

	// Move the nodes over, which may create cross-graph references that we need fix up ASAP
	for (TSet<UEdGraphNode*>::TConstIterator NodeIt(InCollapsableNodes); NodeIt; ++NodeIt)
	{
		UEdGraphNode* Node = *NodeIt;
		Node->Modify();

		// Update stats
		SumNodeX += Node->NodePosX;
		SumNodeY += Node->NodePosY;
		MinNodeX = FMath::Min<float>(MinNodeX, Node->NodePosX);
		MinNodeY = FMath::Min<float>(MinNodeY, Node->NodePosY);
		MaxNodeX = FMath::Max<float>(MaxNodeX, Node->NodePosX);
		MaxNodeY = FMath::Max<float>(MaxNodeY, Node->NodePosY);

		// Move the node over
		InSourceGraph->Nodes.Remove(Node);
		InDestinationGraph->Nodes.Add(Node);
		Node->Rename(/*NewName=*/ NULL, /*NewOuter=*/ InDestinationGraph);

		// Move the sub-graph to the new graph
		if(UK2Node_Composite* Composite = Cast<UK2Node_Composite>(Node))
		{
			InSourceGraph->SubGraphs.Remove(Composite->BoundGraph);
			InDestinationGraph->SubGraphs.Add(Composite->BoundGraph);
		}

		TArray<UEdGraphPin*> OutputGatewayExecPins;

		// Find cross-graph links
		for (int32 PinIndex = 0; PinIndex < Node->Pins.Num(); ++PinIndex)
		{
			UEdGraphPin* LocalPin = Node->Pins[PinIndex];

			bool bIsGatewayPin = false;
			if(LocalPin->LinkedTo.Num())
			{
				for (int32 LinkIndex = 0; LinkIndex < LocalPin->LinkedTo.Num(); ++LinkIndex)
				{
					UEdGraphPin* TrialPin = LocalPin->LinkedTo[LinkIndex];
					if (!InCollapsableNodes.Contains(TrialPin->GetOwningNode()))
					{
						bIsGatewayPin = true;
						break;
					}
				}
			}
			// If the pin has no links but is an exec pin and this is a function graph, then it is a gateway pin
			else if (InGatewayNode->GetClass() == UK2Node_CallFunction::StaticClass() && K2Schema->IsExecPin(*LocalPin))
			{
				if (LocalPin->Direction == EGPD_Input)
				{
					// Connect the gateway pin to the node, there is no remote pin to hook up because the exec pin was not originally connected
					LocalPin->Modify();
					UK2Node_EditablePinBase* LocalPort = InEntryNode;
					UEdGraphPin* LocalPortPin = LocalPort->Pins[0];
					LocalPin->MakeLinkTo(LocalPortPin);
				}
				else
				{
					OutputGatewayExecPins.Add(LocalPin);
				}
			}

			// Thunk cross-graph links thru the gateway
			if (bIsGatewayPin)
			{
				// Local port is either the entry or the result node in the collapsed graph
				// Remote port is the node placed in the source graph
				UK2Node_EditablePinBase* LocalPort = (LocalPin->Direction == EGPD_Input) ? InEntryNode : InResultNode;

				// Add a new pin to the entry/exit node and to the composite node
				UEdGraphPin* LocalPortPin = NULL;
				UEdGraphPin* RemotePortPin = NULL;

				// Function graphs have a single exec path through them, so only one exec pin for input and another for output. In this fashion, they must not be handled by name.
				if(InGatewayNode->GetClass() == UK2Node_CallFunction::StaticClass() && LocalPin->PinType.PinCategory == K2Schema->PC_Exec)
				{
					LocalPortPin = LocalPort->Pins[0];
					RemotePortPin = K2Schema->FindExecutionPin(*InGatewayNode, (LocalPortPin->Direction == EGPD_Input)? EGPD_Output : EGPD_Input);
				}
				else
				{
					// If there is a custom event being used as a template, we must check to see if any connected pins have already been built
					if(InterfaceTemplateNode && LocalPin->Direction == EGPD_Input)
					{
						// Find the pin on the entry node, we will use that pin's name to find the pin on the remote port
						UEdGraphPin* EntryNodePin = InEntryNode->FindPin(LocalPin->LinkedTo[0]->PinName);
						if(EntryNodePin)
						{
							LocalPin->BreakAllPinLinks();
							LocalPin->MakeLinkTo(EntryNodePin);
							continue;
						}
					}

					if(LocalPin->LinkedTo[0]->GetOwningNode() != InEntryNode)
					{
						FString UniquePortName = InGatewayNode->CreateUniquePinName(LocalPin->PinName);

						if(!RemotePortPin && !LocalPortPin)
						{
							if (LocalPin->Direction == EGPD_Output)
							{
								bDiscardReturnNode = false;
							}
							
							FEdGraphPinType PinType = LocalPin->PinType;
							if (PinType.bIsWeakPointer && !PinType.IsContainer() && !bCanHaveWeakObjPtrParam)
							{
								PinType.bIsWeakPointer = false;
							}
							RemotePortPin = InGatewayNode->CreatePin(LocalPin->Direction, PinType, UniquePortName);
							LocalPortPin = LocalPort->CreateUserDefinedPin(UniquePortName, PinType, (LocalPin->Direction == EGPD_Input)? EGPD_Output : EGPD_Input);
						}
					}
				}

				check(LocalPortPin);
				check(RemotePortPin);

				LocalPin->Modify();

				// Route the links
				for (int32 LinkIndex = 0; LinkIndex < LocalPin->LinkedTo.Num(); ++LinkIndex)
				{
					UEdGraphPin* RemotePin = LocalPin->LinkedTo[LinkIndex];
					RemotePin->Modify();

					if (!InCollapsableNodes.Contains(RemotePin->GetOwningNode()) && RemotePin->GetOwningNode() != InEntryNode && RemotePin->GetOwningNode() != InResultNode)
					{
						// Fix up the remote pin
						RemotePin->LinkedTo.Remove(LocalPin);
						RemotePin->MakeLinkTo(RemotePortPin);

						// The Entry Node only supports a single link, so if we made links above
						// we need to break them now, to make room for the new link.
						if (LocalPort == InEntryNode)
						{
							LocalPortPin->BreakAllPinLinks();
						}

						// Fix up the local pin
						LocalPin->LinkedTo.Remove(RemotePin);
						--LinkIndex;
						LocalPin->MakeLinkTo(LocalPortPin);
					}
				}
			}
		}

		if (OutputGatewayExecPins.Num() > 0)
		{
			UEdGraphPin* LocalResultPortPin = K2Schema->FindExecutionPin(*InResultNode, EGPD_Input);

			// If the Result Node already contains links, then we don't need to make these connections as the intended connections have already been
			// transferred from original graph.
			if (LocalResultPortPin != nullptr && LocalResultPortPin->LinkedTo.Num() == 0)
			{
				// TODO: Some of these pins may not necessarily be terminal pins. We should prompt the user to choose which of these connections should
				// be made to the return node.
				for (UEdGraphPin* LocalPin : OutputGatewayExecPins)
				{
					// Connect the gateway pin to the node, there is no remote pin to hook up because the exec pin was not originally connected
					LocalPin->Modify();
					LocalPin->MakeLinkTo(LocalResultPortPin);
				}
			}
		}
	}

	// Reposition the newly created nodes
	const int32 NumNodes = InCollapsableNodes.Num();

	// Remove the template node if one was used for generating the function
	if(InterfaceTemplateNode)
	{
		if(NumNodes == 0)
		{
			SumNodeX = InterfaceTemplateNode->NodePosX;
			SumNodeY = InterfaceTemplateNode->NodePosY;
		}

		FBlueprintEditorUtils::RemoveNode(GetBlueprintObj(), InterfaceTemplateNode);
	}

	// Using the result pin, we will ensure that there is a path through the function by checking if it is connected. If it is not, link it to the entry node.
	if(UEdGraphPin* ResultExecFunc = K2Schema->FindExecutionPin(*InResultNode, EGPD_Input))
	{
		if(ResultExecFunc->LinkedTo.Num() == 0)
		{
			K2Schema->FindExecutionPin(*InEntryNode, EGPD_Output)->MakeLinkTo(K2Schema->FindExecutionPin(*InResultNode, EGPD_Input));
		}
	}

	const float CenterX = NumNodes == 0? SumNodeX : SumNodeX / NumNodes;
	const float CenterY = NumNodes == 0? SumNodeY : SumNodeY / NumNodes;
	const float MinusOffsetX = 160.0f; //@TODO: Random magic numbers
	const float PlusOffsetX = 300.0f;

	// Put the gateway node at the center of the empty space in the old graph
	InGatewayNode->NodePosX = CenterX;
	InGatewayNode->NodePosY = CenterY;
	InGatewayNode->SnapToGrid(SNodePanel::GetSnapGridSize());

	// Put the entry and exit nodes on either side of the nodes in the new graph
	//@TODO: Should we recenter the whole ensemble?
	if(NumNodes != 0)
	{
		InEntryNode->NodePosX = MinNodeX - MinusOffsetX;
		InEntryNode->NodePosY = CenterY;
		InEntryNode->SnapToGrid(SNodePanel::GetSnapGridSize());

		InResultNode->NodePosX = MaxNodeX + PlusOffsetX;
		InResultNode->NodePosY = CenterY;
		InResultNode->SnapToGrid(SNodePanel::GetSnapGridSize());
	}

	if (bCanDiscardEmptyReturnNode && bDiscardReturnNode)
	{
		InResultNode->DestroyNode();
	}
}

void FBlueprintEditor::CollapseNodes(TSet<UEdGraphNode*>& InCollapsableNodes)
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (!FocusedGraphEd.IsValid())
	{
		return;
	}

	UEdGraph* SourceGraph = FocusedGraphEd->GetCurrentGraph();
	SourceGraph->Modify();

	// Create the composite node that will serve as the gateway into the subgraph
	UK2Node_Composite* GatewayNode = NULL;
	{
		UK2Node_Composite* TemplateNode = NewObject<UK2Node_Composite>();
		GatewayNode = FEdGraphSchemaAction_K2NewNode::SpawnNodeFromTemplate<UK2Node_Composite>(SourceGraph, TemplateNode, FVector2D(0,0));
		GatewayNode->bCanRenameNode = true;
		check(GatewayNode);
	}

	FName GraphName;
	GraphName = FBlueprintEditorUtils::FindUniqueKismetName(GetBlueprintObj(), TEXT("CollapseGraph"));

	// Rename the graph to the correct name
	UEdGraph* DestinationGraph = GatewayNode->BoundGraph;
	TSharedPtr<INameValidatorInterface> NameValidator = MakeShareable(new FKismetNameValidator(GetBlueprintObj(), GraphName));
	FBlueprintEditorUtils::RenameGraphWithSuggestion(DestinationGraph, NameValidator, GraphName.ToString());

	CollapseNodesIntoGraph(GatewayNode, GatewayNode->GetInputSink(), GatewayNode->GetOutputSource(), SourceGraph, DestinationGraph, InCollapsableNodes, false, true);
}

UEdGraph* FBlueprintEditor::CollapseSelectionToFunction(TSharedPtr<SGraphEditor> InRootGraph, TSet<class UEdGraphNode*>& InCollapsableNodes, UEdGraphNode*& OutFunctionNode)
{
	TSharedPtr<SGraphEditor> FocusedGraphEd = InRootGraph;
	if (!FocusedGraphEd.IsValid())
	{
		return NULL;
	}

	UEdGraph* SourceGraph = FocusedGraphEd->GetCurrentGraph();
	SourceGraph->Modify();

	UEdGraph* NewGraph = NULL;

	FName DocumentName = FBlueprintEditorUtils::FindUniqueKismetName(GetBlueprintObj(), TEXT("NewFunction"));

	NewGraph = FBlueprintEditorUtils::CreateNewGraph(GetBlueprintObj(), DocumentName, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
	FBlueprintEditorUtils::AddFunctionGraph<UClass>(GetBlueprintObj(), NewGraph, /*bIsUserCreated=*/ true, NULL);

	TArray<UK2Node_FunctionEntry*> EntryNodes;
	NewGraph->GetNodesOfClass(EntryNodes);
	UK2Node_FunctionEntry* EntryNode = EntryNodes[0];
	UK2Node_FunctionResult* ResultNode = NULL;

	// Create Result
	FGraphNodeCreator<UK2Node_FunctionResult> ResultNodeCreator(*NewGraph);
	UK2Node_FunctionResult* FunctionResult = ResultNodeCreator.CreateNode();

	const UEdGraphSchema_K2* Schema = Cast<UEdGraphSchema_K2>(FunctionResult->GetSchema());
	FunctionResult->NodePosX = EntryNode->NodePosX + EntryNode->NodeWidth + 256;
	FunctionResult->NodePosY = EntryNode->NodePosY;

	ResultNodeCreator.Finalize();

	ResultNode = FunctionResult;

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	// make temp list builder
	FGraphActionListBuilderBase TempListBuilder;
	TempListBuilder.OwnerOfTemporaries = NewObject<UEdGraph>(GetBlueprintObj());
	TempListBuilder.OwnerOfTemporaries->SetFlags(RF_Transient);

	IBlueprintNodeBinder::FBindingSet Bindings;
	OutFunctionNode = UBlueprintFunctionNodeSpawner::Create(FindField<UFunction>(GetBlueprintObj()->SkeletonGeneratedClass, DocumentName))->Invoke(SourceGraph, Bindings, FVector2D::ZeroVector);

	check(OutFunctionNode);

	CollapseNodesIntoGraph(OutFunctionNode, EntryNode, ResultNode, SourceGraph, NewGraph, InCollapsableNodes, true, false);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprintObj());
	OutFunctionNode->ReconstructNode();

	return NewGraph;
}

UEdGraph* FBlueprintEditor::CollapseSelectionToMacro(TSharedPtr<SGraphEditor> InRootGraph, TSet<UEdGraphNode*>& InCollapsableNodes, UEdGraphNode*& OutMacroNode)
{
	TSharedPtr<SGraphEditor> FocusedGraphEd = InRootGraph;
	if (!FocusedGraphEd.IsValid())
	{
		return NULL;
	}

	UEdGraph* SourceGraph = FocusedGraphEd->GetCurrentGraph();
	SourceGraph->Modify();

	UEdGraph* DestinationGraph = NULL;

	FName DocumentName = FBlueprintEditorUtils::FindUniqueKismetName(GetBlueprintObj(), TEXT("NewMacro"));

	DestinationGraph = FBlueprintEditorUtils::CreateNewGraph(GetBlueprintObj(), DocumentName, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
	FBlueprintEditorUtils::AddMacroGraph(GetBlueprintObj(), DestinationGraph, /*bIsUserCreated=*/ true, NULL);

	UK2Node_MacroInstance* MacroTemplate = NewObject<UK2Node_MacroInstance>();
	MacroTemplate->SetMacroGraph(DestinationGraph);

	UK2Node_MacroInstance* GatewayNode = FEdGraphSchemaAction_K2NewNode::SpawnNodeFromTemplate<UK2Node_MacroInstance>(SourceGraph, MacroTemplate, FVector2D(0.0f, 0.0f), false);

	TArray<UK2Node_Tunnel*> TunnelNodes;
	GatewayNode->GetMacroGraph()->GetNodesOfClass(TunnelNodes);

	UK2Node_EditablePinBase* InputSink = nullptr;
	UK2Node_EditablePinBase* OutputSink = nullptr;

	// Retrieve the tunnel nodes to use them to match up pin links that connect to the gateway.
	for (UK2Node_Tunnel* Node : TunnelNodes)
	{
		if (Node->IsEditable())
		{
			if (Node->bCanHaveOutputs)
			{
				InputSink = Node;
			}
			else if (Node->bCanHaveInputs)
			{
				OutputSink = Node;
			}
		}
	}

	CollapseNodesIntoGraph(GatewayNode, InputSink, OutputSink, SourceGraph, DestinationGraph, InCollapsableNodes, false, false);

	OutMacroNode = GatewayNode;
	OutMacroNode->ReconstructNode();

	return DestinationGraph;
}

void FBlueprintEditor::ExpandNode(UEdGraphNode* InNodeToExpand, UEdGraph* InSourceGraph, TSet<UEdGraphNode*>& OutExpandedNodes)
{
 	UEdGraph* DestinationGraph = InNodeToExpand->GetGraph();
	UEdGraph* SourceGraph = InSourceGraph;
	check(SourceGraph);

	// Mark all edited objects so they will appear in the transaction record if needed.
	DestinationGraph->Modify();
	SourceGraph->Modify();
	InNodeToExpand->Modify();

	UEdGraphNode* Entry = NULL;
	UEdGraphNode* Result = NULL;

	const bool bIsCollapsedGraph = InNodeToExpand->IsA<UK2Node_Composite>();

	// Move the nodes over, remembering any that are boundary nodes
	while (SourceGraph->Nodes.Num())
	{
		UEdGraphNode* Node = SourceGraph->Nodes.Pop();

		Node->Modify();
		Node->Rename(/*NewName=*/ NULL, /*NewOuter=*/ DestinationGraph);

		// We do not check CanPasteHere when determining CanCollapseNodes, unlike CanCollapseSelectionToFunction/Macro,
		// so when expanding a collapsed graph we don't want to check the CanPasteHere function:
		if (!bIsCollapsedGraph && !Node->CanPasteHere(DestinationGraph))
		{
			Node->BreakAllNodeLinks();
			continue;
		}

		// Successfully added the node to the graph, we may need to remove flags
		if (Node->HasAllFlags(RF_Transient) && !DestinationGraph->HasAllFlags(RF_Transient))
		{
			Node->SetFlags(RF_Transactional);
			Node->ClearFlags(RF_Transient);
			TArray<UObject*> Subobjects;
			GetObjectsWithOuter(Node, Subobjects);
			for (UObject* Subobject : Subobjects)
			{
				Subobject->ClearFlags(RF_Transient);
				Subobject->SetFlags(RF_Transactional);
			}
		}
		DestinationGraph->Nodes.Add(Node);

		// Want to test exactly against tunnel, we shouldn't collapse embedded collapsed
		// nodes or macros, only the tunnels in/out of the collapsed graph
		if (Node->GetClass() == UK2Node_Tunnel::StaticClass())
		{
			UK2Node_Tunnel* TunnelNode = Cast<UK2Node_Tunnel>(Node);
			if(TunnelNode->bCanHaveOutputs)
			{
				Entry = Node;
			}
			else if(TunnelNode->bCanHaveInputs)
			{
				Result = Node;
			}
		}
		else if (Node->GetClass() == UK2Node_FunctionEntry::StaticClass())
		{
			Entry = Node;
		}
		else if(Node->GetClass() == UK2Node_FunctionResult::StaticClass())
		{
			Result = Node;
		}
		else
		{
			OutExpandedNodes.Add(Node);
		}
	}

	UEdGraphPin* OutputExecPinReconnect = NULL;
	if(UK2Node_CallFunction* CallFunction = Cast<UK2Node_CallFunction>(InNodeToExpand))
	{
		UEdGraphPin* ThenPin = CallFunction->GetThenPin();
		if (ThenPin && ThenPin->LinkedTo.Num())
		{
			OutputExecPinReconnect = ThenPin->LinkedTo[0];
		}
	}

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	K2Schema->CollapseGatewayNode(Cast<UK2Node>(InNodeToExpand), Entry, Result, nullptr, &OutExpandedNodes);

	if(Entry)
	{
		Entry->DestroyNode();
	}

	if(Result)
	{
		Result->DestroyNode();
	}

	// Make sure any subgraphs get propagated appropriately
	if (SourceGraph->SubGraphs.Num() > 0)
	{
		DestinationGraph->SubGraphs.Append(SourceGraph->SubGraphs);
		SourceGraph->SubGraphs.Empty();
	}

	// Remove the gateway node and source graph
	InNodeToExpand->DestroyNode();

	// This should be set for function nodes, all expanded nodes should connect their output exec pins to the original pin.
	if(OutputExecPinReconnect)
	{
		for (TSet<UEdGraphNode*>::TConstIterator NodeIt(OutExpandedNodes); NodeIt; ++NodeIt)
		{
			UEdGraphNode* Node = *NodeIt;
			for (int32 PinIndex = 0; PinIndex < Node->Pins.Num(); ++PinIndex)
			{
				// Only hookup output exec pins that do not have a connection
				if(Node->Pins[PinIndex]->PinType.PinCategory == K2Schema->PC_Exec && Node->Pins[PinIndex]->Direction == EGPD_Output && Node->Pins[PinIndex]->LinkedTo.Num() == 0)
				{
					Node->Pins[PinIndex]->MakeLinkTo(OutputExecPinReconnect);
				}
			}
		}
	}
}

void FBlueprintEditor::SaveEditedObjectState()
{
	check(IsEditingSingleBlueprint());

	// Clear currently edited documents
	GetBlueprintObj()->LastEditedDocuments.Empty();

	// Ask all open documents to save their state, which will update LastEditedDocuments
	DocumentManager->SaveAllState();
}

void FBlueprintEditor::RequestSaveEditedObjectState()
{
	bRequestedSavingOpenDocumentState = true;
}

void FBlueprintEditor::Tick(float DeltaTime)
{
	// Create or update the Blueprint actor instance in the preview scene
	if ( GetPreviewActor() == nullptr )
	{
		UpdatePreviewActor(GetBlueprintObj(), true);
	}

	if (bRequestedSavingOpenDocumentState)
	{
		bRequestedSavingOpenDocumentState = false;

		SaveEditedObjectState();
	}

	if (InstructionsFadeCountdown > 0.f)
	{
		InstructionsFadeCountdown -= DeltaTime;
	}

	if (bPendingDeferredClose)
	{
		IAssetEditorInstance* EditorInst = FAssetEditorManager::Get().FindEditorForAsset(GetBlueprintObj(), /*bFocusIfOpen =*/false);
		check(EditorInst != nullptr);
		EditorInst->CloseWindow();
	}
}
TStatId FBlueprintEditor::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FBlueprintEditor, STATGROUP_Tickables);
}


void FBlueprintEditor::OnStartEditingDefaultsClicked()
{
	StartEditingDefaults(/*bAutoFocus=*/ true);
}

void FBlueprintEditor::OnListObjectsReferencedByClass()
{
	ObjectTools::ShowReferencedObjs(GetBlueprintObj()->GeneratedClass);
}

void FBlueprintEditor::OnListObjectsReferencedByBlueprint()
{
	ObjectTools::ShowReferencedObjs(GetBlueprintObj());
}

void FBlueprintEditor::OnRepairCorruptedBlueprint()
{
	IKismetCompilerInterface& Compiler = FModuleManager::LoadModuleChecked<IKismetCompilerInterface>(KISMET_COMPILER_MODULENAME);
	Compiler.RecoverCorruptedBlueprint(GetBlueprintObj());
}

void FBlueprintEditor::StartEditingDefaults(bool bAutoFocus, bool bForceRefresh)
{
	SetUISelectionState(FBlueprintEditor::SelectionState_ClassDefaults);

	if (IsEditingSingleBlueprint())
	{
		if (GetBlueprintObj()->GeneratedClass != nullptr)
		{
			if ( SCSEditor.IsValid() && GetBlueprintObj()->GeneratedClass->IsChildOf<AActor>() )
			{
				SCSEditor->SelectRoot();
			}
			else
			{
				UObject* DefaultObject = GetBlueprintObj()->GeneratedClass->GetDefaultObject();

				// Update the details panel
				FString Title;
				DefaultObject->GetName(Title);
				SKismetInspector::FShowDetailsOptions Options(FText::FromString(Title), bForceRefresh);
				Options.bShowComponents = false;

				Inspector->ShowDetailsForSingleObject(DefaultObject, Options);

				if ( bAutoFocus )
				{
					TryInvokingDetailsTab();
				}
			}
		}
	}
	
	RefreshStandAloneDefaultsEditor();
}

void FBlueprintEditor::RefreshStandAloneDefaultsEditor()
{
	// Update the details panel
	SKismetInspector::FShowDetailsOptions Options(FText::GetEmpty(), true);

	TArray<UObject*> DefaultObjects;
	for ( int32 i = 0; i < GetEditingObjects().Num(); ++i )
	{
		UBlueprintCore* Blueprint = Cast<UBlueprintCore>(GetEditingObjects()[i]);
		if ( Blueprint && Blueprint->GeneratedClass )
		{
			DefaultObjects.Add(Blueprint->GeneratedClass->GetDefaultObject());
		}
	}

	if ( DefaultObjects.Num() && DefaultEditor.IsValid() )
	{
		DefaultEditor->ShowDetailsForObjects(DefaultObjects);
	}
}

void FBlueprintEditor::RenameNewlyAddedAction(FName InActionName)
{
	TabManager->InvokeTab(FBlueprintEditorTabs::MyBlueprintID);
	TryInvokingDetailsTab(/*Flash*/false);

	if (MyBlueprintWidget.IsValid())
	{
		// Force a refresh immediately, the item has to be present in the list for the rename requests to be successful.
		MyBlueprintWidget->Refresh();
		MyBlueprintWidget->SelectItemByName(InActionName,ESelectInfo::OnMouseClick);
		MyBlueprintWidget->OnRequestRenameOnActionNode();
	}
}

void FBlueprintEditor::OnAddNewVariable()
{
	const FScopedTransaction Transaction( LOCTEXT("AddVariable", "Add Variable") );

	FName VarName = FBlueprintEditorUtils::FindUniqueKismetName(GetBlueprintObj(), TEXT("NewVar"));

	bool bSuccess = MyBlueprintWidget.IsValid() && FBlueprintEditorUtils::AddMemberVariable(GetBlueprintObj(), VarName, MyBlueprintWidget->GetLastPinTypeUsed());

	if(!bSuccess)
	{
		LogSimpleMessage( LOCTEXT("AddVariable_Error", "Adding new variable failed.") );
	}
	else
	{
		RenameNewlyAddedAction(VarName);
	}
}

bool FBlueprintEditor::CanAddNewLocalVariable() const
{
	if (InEditingMode())
	{
		UEdGraph* TargetGraph = FBlueprintEditorUtils::GetTopLevelGraph(FocusedGraphEdPtr.Pin()->GetCurrentGraph());

		return TargetGraph->GetSchema()->GetGraphType(TargetGraph) == GT_Function;
	}
	return false;
}

void FBlueprintEditor::OnAddNewLocalVariable()
{
	// Find the top level graph to place the local variables into
	UEdGraph* TargetGraph = FBlueprintEditorUtils::GetTopLevelGraph(FocusedGraphEdPtr.Pin()->GetCurrentGraph());
	check(TargetGraph->GetSchema()->GetGraphType(TargetGraph) == GT_Function);

	FName VarName = FBlueprintEditorUtils::FindUniqueKismetName(GetBlueprintObj(), TEXT("NewLocalVar"), FindField<UFunction>(GetBlueprintObj()->SkeletonGeneratedClass, TargetGraph->GetFName()));

	bool bSuccess = MyBlueprintWidget.IsValid() && FBlueprintEditorUtils::AddLocalVariable(GetBlueprintObj(), TargetGraph, VarName, MyBlueprintWidget->GetLastPinTypeUsed());

	if(!bSuccess)
	{
		LogSimpleMessage( LOCTEXT("AddLocalVariable_Error", "Adding new local variable failed.") );
	}
	else
	{
		RenameNewlyAddedAction(VarName);
	}
}

void FBlueprintEditor::OnAddNewDelegate()
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	check(NULL != K2Schema);
	UBlueprint* const Blueprint = GetBlueprintObj();
	check(NULL != Blueprint);

	FName Name = FBlueprintEditorUtils::FindUniqueKismetName(GetBlueprintObj(), TEXT("NewEventDispatcher"));


	const FScopedTransaction Transaction( LOCTEXT("AddNewDelegate", "Add New Event Dispatcher") ); 
	Blueprint->Modify();

	FEdGraphPinType DelegateType;
	DelegateType.PinCategory = K2Schema->PC_MCDelegate;
	const bool bVarCreatedSuccess = FBlueprintEditorUtils::AddMemberVariable(Blueprint, Name, DelegateType);
	if(!bVarCreatedSuccess)
	{
		LogSimpleMessage( LOCTEXT("AddDelegateVariable_Error", "Adding new delegate variable failed.") );
		return;
	}

	UEdGraph* const NewGraph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, Name, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
	if(!NewGraph)
	{
		FBlueprintEditorUtils::RemoveMemberVariable(Blueprint, Name);
		LogSimpleMessage( LOCTEXT("AddDelegateVariable_Error", "Adding new delegate variable failed.") );
		return;
	}

	NewGraph->bEditable = false;

	K2Schema->CreateDefaultNodesForGraph(*NewGraph);
	K2Schema->CreateFunctionGraphTerminators(*NewGraph, (UClass*)NULL);
	K2Schema->AddExtraFunctionFlags(NewGraph, (FUNC_BlueprintCallable|FUNC_BlueprintEvent|FUNC_Public));
	K2Schema->MarkFunctionEntryAsEditable(NewGraph, true);

	Blueprint->DelegateSignatureGraphs.Add(NewGraph);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	RenameNewlyAddedAction(Name);
}

void FBlueprintEditor::NewDocument_OnClicked(ECreatedDocumentType GraphType)
{
	FText DocumentNameText;
	bool bResetMyBlueprintFilter = false;

	switch (GraphType)
	{
	case CGT_NewFunctionGraph:
		DocumentNameText = LOCTEXT("NewDocFuncName", "NewFunction");
		bResetMyBlueprintFilter = true;
		break;
	case CGT_NewEventGraph:
		DocumentNameText = LOCTEXT("NewDocEventGraphName", "NewEventGraph");
		bResetMyBlueprintFilter = true;
		break;
	case CGT_NewMacroGraph:
		DocumentNameText = LOCTEXT("NewDocMacroName", "NewMacro");
		bResetMyBlueprintFilter = true;
		break;
	case CGT_NewAnimationGraph:
		DocumentNameText = LOCTEXT("NewDocAnimationGraphName", "NewAnimationGraph");
		bResetMyBlueprintFilter = true;
		break;
	default:
		DocumentNameText = LOCTEXT("NewDocNewName", "NewDocument");
		break;
	}

	FName DocumentName = FName(*DocumentNameText.ToString());

	// Make sure the new name is valid
	DocumentName = FBlueprintEditorUtils::FindUniqueKismetName(GetBlueprintObj(), DocumentNameText.ToString());

	check(IsEditingSingleBlueprint());

	const FScopedTransaction Transaction( LOCTEXT("AddNewFunction", "Add New Function") ); 
	GetBlueprintObj()->Modify();

	UEdGraph* NewGraph = NULL;

	if (GraphType == CGT_NewFunctionGraph)
	{
		NewGraph = FBlueprintEditorUtils::CreateNewGraph(GetBlueprintObj(), DocumentName, UEdGraph::StaticClass(), GetDefaultSchemaClass());
		FBlueprintEditorUtils::AddFunctionGraph<UClass>(GetBlueprintObj(), NewGraph, /*bIsUserCreated=*/ true, NULL);
	}
	else if (GraphType == CGT_NewMacroGraph)
	{
		NewGraph = FBlueprintEditorUtils::CreateNewGraph(GetBlueprintObj(), DocumentName, UEdGraph::StaticClass(), GetDefaultSchemaClass());
		FBlueprintEditorUtils::AddMacroGraph(GetBlueprintObj(), NewGraph, /*bIsUserCreated=*/ true, NULL);
	}
	else if (GraphType == CGT_NewEventGraph)
	{
		NewGraph = FBlueprintEditorUtils::CreateNewGraph(GetBlueprintObj(), DocumentName, UEdGraph::StaticClass(), GetDefaultSchemaClass());
		FBlueprintEditorUtils::AddUbergraphPage(GetBlueprintObj(), NewGraph);
	}
	else if (GraphType == CGT_NewAnimationGraph)
	{
		//@TODO: ANIMREFACTOR: This code belongs in Persona, not in BlueprintEditor
		NewGraph = FBlueprintEditorUtils::CreateNewGraph(GetBlueprintObj(), DocumentName, UAnimationGraph::StaticClass(), UAnimationGraphSchema::StaticClass());
		FBlueprintEditorUtils::AddDomainSpecificGraph(GetBlueprintObj(), NewGraph);
	}
	else
	{
		ensureMsgf(false, TEXT("GraphType is invalid") );
	}

	// Now open the new graph
	if (NewGraph)
	{
		OpenDocument(NewGraph, FDocumentTracker::OpenNewDocument);

		RenameNewlyAddedAction(DocumentName);
	}
	else
	{
		LogSimpleMessage( LOCTEXT("AddDocument_Error", "Adding new document failed.") );
	}
}

bool FBlueprintEditor::NewDocument_IsVisibleForType(ECreatedDocumentType GraphType) const
{
	switch (GraphType)
	{
	case CGT_NewVariable:
		return (GetBlueprintObj()->BlueprintType != BPTYPE_FunctionLibrary) 
			&& (GetBlueprintObj()->BlueprintType != BPTYPE_Interface) 
			&& (GetBlueprintObj()->BlueprintType != BPTYPE_MacroLibrary);
	case CGT_NewFunctionGraph:
		return (GetBlueprintObj()->BlueprintType != BPTYPE_MacroLibrary);
	case CGT_NewMacroGraph:
		return (GetBlueprintObj()->BlueprintType == BPTYPE_MacroLibrary) || (GetBlueprintObj()->BlueprintType == BPTYPE_Normal) || (GetBlueprintObj()->BlueprintType == BPTYPE_LevelScript);
	case CGT_NewAnimationGraph:
		return GetBlueprintObj()->IsA(UAnimBlueprint::StaticClass());
	case CGT_NewEventGraph:
		return FBlueprintEditorUtils::DoesSupportEventGraphs(GetBlueprintObj());
	case CGT_NewLocalVariable:
		return FBlueprintEditorUtils::DoesSupportLocalVariables(GetFocusedGraph()) 
			&& IsFocusedGraphEditable();
	}

	return false;
}

TSubclassOf<UEdGraphSchema> FBlueprintEditor::GetDefaultSchemaClass() const
{
	return UEdGraphSchema_K2::StaticClass();
}

bool FBlueprintEditor::AddNewDelegateIsVisible() const
{
	const UBlueprint* Blueprint = GetBlueprintObj();
	return (NULL != Blueprint)
		&& (Blueprint->BlueprintType != BPTYPE_Interface) 
		&& (Blueprint->BlueprintType != BPTYPE_MacroLibrary)
		&& (Blueprint->BlueprintType != BPTYPE_FunctionLibrary);
}

void FBlueprintEditor::NotifyPreChange(UProperty* PropertyAboutToChange)
{
	// this only delivers message to the "FOCUSED" one, not every one
	// internally it will only deliver the message to the selected node, not all nodes
	FString PropertyName = PropertyAboutToChange->GetName();
	if (FocusedGraphEdPtr.IsValid())
	{
		FocusedGraphEdPtr.Pin()->NotifyPrePropertyChange(PropertyName);
	}
}

void FBlueprintEditor::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, UProperty* PropertyThatChanged)
{
	FString PropertyName = PropertyThatChanged->GetName();
	if (FocusedGraphEdPtr.IsValid())
	{
		FocusedGraphEdPtr.Pin()->NotifyPostPropertyChange(PropertyChangedEvent, PropertyName);
	}
	
	if (IsEditingSingleBlueprint())
	{
		UBlueprint* Blueprint = GetBlueprintObj();
		UPackage* BlueprintPackage = Blueprint->GetOutermost();

		// if any of the objects being edited are in our package, mark us as dirty
		bool bPropertyInBlueprint = false;
		for (int32 ObjectIndex = 0; ObjectIndex < PropertyChangedEvent.GetNumObjectsBeingEdited(); ++ObjectIndex)
		{
			const UObject* Object = PropertyChangedEvent.GetObjectBeingEdited(ObjectIndex);
			if (Object && Object->GetOutermost() == BlueprintPackage)
			{
				bPropertyInBlueprint = true;
				break;
			}
		}

		if (bPropertyInBlueprint)
		{
			// Note: if change type is "interactive," hold off on applying the change (e.g. this will occur if the user is scrubbing a spinbox value; we don't want to apply the change until the mouse is released, for performance reasons)
			if (PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
			{
				FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint, PropertyChangedEvent);

				// Call PostEditChange() on any Actors that might be based on this Blueprint
				FBlueprintEditorUtils::PostEditChangeBlueprintActors(Blueprint);
			}

			// Force updates to occur immediately during interactive mode (otherwise the preview won't refresh because it won't be ticking)
			UpdateSCSPreview(PropertyChangedEvent.ChangeType == EPropertyChangeType::Interactive);
		}
	}
}

void FBlueprintEditor::OnFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent)
{
	FName PropertyName = (PropertyChangedEvent.Property != NULL) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	//@TODO: This code does not belong here (might not even be necessary anymore as they seem to have PostEditChangeProperty impls now)!
	if ((PropertyName == GET_MEMBER_NAME_CHECKED(UK2Node_Switch, bHasDefaultPin)) ||
		(PropertyName == GET_MEMBER_NAME_CHECKED(UK2Node_SwitchInteger, StartIndex)) ||
		(PropertyName == GET_MEMBER_NAME_CHECKED(UK2Node_SwitchString, PinNames)) ||
		(PropertyName == GET_MEMBER_NAME_CHECKED(UK2Node_SwitchName, PinNames)) ||
		(PropertyName == GET_MEMBER_NAME_CHECKED(UK2Node_SwitchString, bIsCaseSensitive)))
	{
		DocumentManager->RefreshAllTabs();
	}
}

FName FBlueprintEditor::GetToolkitFName() const
{
	return FName("BlueprintEditor");
}

FName FBlueprintEditor::GetContextFromBlueprintType(EBlueprintType InType)
{
	switch (InType)
	{
	default:
	case BPTYPE_Normal:
		return FName("BlueprintEditor");
	case BPTYPE_MacroLibrary:
		return FName("BlueprintEditor.MacroLibrary");
	case BPTYPE_Interface:
		return FName("BlueprintEditor.Interface");
	case BPTYPE_LevelScript:
		return FName("BlueprintEditor.LevelScript");
	}
}

FName FBlueprintEditor::GetToolkitContextFName() const
{
	if(GetBlueprintObj())
	{
		return GetContextFromBlueprintType(GetBlueprintObj()->BlueprintType);
	}

	return FName("BlueprintEditor");
}

FText FBlueprintEditor::GetBaseToolkitName() const
{
	return LOCTEXT( "AppLabel", "Blueprint Editor" );
}

FText FBlueprintEditor::GetToolkitName() const
{
	const TArray<UObject*>& EditingObjs = GetEditingObjects();

	if( IsEditingSingleBlueprint() )
	{
		const bool bDirtyState = GetBlueprintObj()->GetOutermost()->IsDirty();

		FFormatNamedArguments Args;
		Args.Add( TEXT("DirtyState"), bDirtyState ? FText::FromString( TEXT( "*" ) ) : FText::GetEmpty() );

		if (FBlueprintEditorUtils::IsLevelScriptBlueprint(GetBlueprintObj()))
		{
			const FString& LevelName = FPackageName::GetShortFName( GetBlueprintObj()->GetOutermost()->GetFName().GetPlainNameString() ).GetPlainNameString();	

			Args.Add( TEXT("LevelName"), FText::FromString( LevelName ) );
			return FText::Format( NSLOCTEXT("KismetEditor", "LevelScriptAppLabel", "{LevelName}{DirtyState} - Level Blueprint Editor"), Args );
		}
		else
		{
			Args.Add( TEXT("BlueprintName"), FText::FromString( GetBlueprintObj()->GetName() ) );
			return FText::Format( NSLOCTEXT("KismetEditor", "BlueprintScriptAppLabel", "{BlueprintName}{DirtyState}"), Args );
		}
	}

	TSubclassOf< UObject > SharedParentClass = NULL;

	for (UObject* EditingObj : EditingObjs)
	{
		UBlueprint* Blueprint = Cast<UBlueprint>(EditingObj);
		check( Blueprint );

		// Initialize with the class of the first object we encounter.
		if( *SharedParentClass == NULL )
		{
			SharedParentClass = Blueprint->ParentClass;
		}

		// If we've encountered an object that's not a subclass of the current best baseclass,
		// climb up a step in the class hierarchy.
		while( !Blueprint->ParentClass->IsChildOf( SharedParentClass ) )
		{
			SharedParentClass = SharedParentClass->GetSuperClass();
		}
	}

	FFormatNamedArguments Args;
	Args.Add( TEXT("NumberOfObjects"), EditingObjs.Num() );
	Args.Add( TEXT("ObjectName"), FText::FromString( SharedParentClass->GetName() ) );
	return FText::Format( NSLOCTEXT("KismetEditor", "ToolkitTitle_UniqueLayerName", "{NumberOfObjects} {ClassName} - Class Defaults"), Args );
}

FText FBlueprintEditor::GetToolkitToolTipText() const
{
	const TArray<UObject*>& EditingObjs = GetEditingObjects();

	if( IsEditingSingleBlueprint() )
	{
		if (FBlueprintEditorUtils::IsLevelScriptBlueprint(GetBlueprintObj()))
		{
			const FString& LevelName = FPackageName::GetShortFName( GetBlueprintObj()->GetOutermost()->GetFName().GetPlainNameString() ).GetPlainNameString();	

			FFormatNamedArguments Args;
			Args.Add( TEXT("LevelName"), FText::FromString( LevelName ) );
			return FText::Format( NSLOCTEXT("KismetEditor", "LevelScriptAppToolTip", "{LevelName} - Level Blueprint Editor"), Args );
		}
		else
		{
			return FAssetEditorToolkit::GetToolTipTextForObject( GetBlueprintObj() );
		}
	}

	TSubclassOf< UObject > SharedParentClass = NULL;

	for (UObject* EditingObj : EditingObjs)
	{
		UBlueprint* Blueprint = Cast<UBlueprint>(EditingObj);
		check( Blueprint );

		// Initialize with the class of the first object we encounter.
		if( *SharedParentClass == NULL )
		{
			SharedParentClass = Blueprint->ParentClass;
		}
		 
		// If we've encountered an object that's not a subclass of the current best baseclass,
		// climb up a step in the class hierarchy.
		while( !Blueprint->ParentClass->IsChildOf( SharedParentClass ) )
		{
			SharedParentClass = SharedParentClass->GetSuperClass();
		}
	}

	FFormatNamedArguments Args;
	Args.Add( TEXT("NumberOfObjects"), EditingObjs.Num() );
	Args.Add( TEXT("ObjectName"), FText::FromString( SharedParentClass->GetName() ) );
	return FText::Format( NSLOCTEXT("KismetEditor", "ToolkitTitle_UniqueLayerName", "{NumberOfObjects} {ClassName} - Class Defaults"), Args );
}

FLinearColor FBlueprintEditor::GetWorldCentricTabColorScale() const
{
	if ((IsEditingSingleBlueprint()) && FBlueprintEditorUtils::IsLevelScriptBlueprint(GetBlueprintObj()))
	{
		return FLinearColor( 0.0f, 0.2f, 0.3f, 0.5f );
	}
	else
	{
		return FLinearColor( 0.0f, 0.0f, 0.3f, 0.5f );
	}
}

bool FBlueprintEditor::IsBlueprintEditor() const
{
	return true;
}

FString FBlueprintEditor::GetWorldCentricTabPrefix() const
{
	check(IsEditingSingleBlueprint());

	if (FBlueprintEditorUtils::IsLevelScriptBlueprint(GetBlueprintObj()))
	{
		return NSLOCTEXT("KismetEditor", "WorldCentricTabPrefix_LevelScript", "Script ").ToString();
	}
	else
	{
		return NSLOCTEXT("KismetEditor", "WorldCentricTabPrefix_Blueprint", "Blueprint ").ToString();
	}
}

void FBlueprintEditor::VariableListWasUpdated()
{
	StartEditingDefaults(/*bAutoFocus=*/ false);
}

bool FBlueprintEditor::GetBoundsForSelectedNodes(class FSlateRect& Rect, float Padding)
{
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (FocusedGraphEd.IsValid())
	{
		return FocusedGraphEd->GetBoundsForSelectedNodes(Rect, Padding);
	}
	return false;
}

void FBlueprintEditor::OnRenameNode()
{
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if(FocusedGraphEd.IsValid())
	{
		const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
		for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
		{
			UEdGraphNode* SelectedNode = Cast<UEdGraphNode>(*NodeIt);
			if (SelectedNode != NULL && SelectedNode->bCanRenameNode)
			{
				FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(SelectedNode, true);
				break;
			}
		}
	}
}

bool FBlueprintEditor::CanRenameNodes() const
{
	if (IsEditable(GetFocusedGraph()))
	{
		if (const UEdGraphNode* SelectedNode = GetSingleSelectedNode())
		{
			return SelectedNode->bCanRenameNode;
		}
	}
	return false;
}

bool FBlueprintEditor::OnNodeVerifyTitleCommit(const FText& NewText, UEdGraphNode* NodeBeingChanged, FText& OutErrorMessage)
{
	bool bValid(false);

	if (NodeBeingChanged && NodeBeingChanged->bCanRenameNode)
	{
		// Clear off any existing error message 
		NodeBeingChanged->ErrorMsg.Empty();
		NodeBeingChanged->bHasCompilerMessage = false;

		if (!NameEntryValidator.IsValid())
		{
			NameEntryValidator = FNameValidatorFactory::MakeValidator(NodeBeingChanged);
		}

		EValidatorResult VResult = NameEntryValidator->IsValid(NewText.ToString(), true);
		if (VResult == EValidatorResult::Ok)
		{
			bValid = true;
		}
		else if (FocusedGraphEdPtr.IsValid()) 
		{
			EValidatorResult Valid = NameEntryValidator->IsValid(NewText.ToString(), false);
			
			NodeBeingChanged->bHasCompilerMessage = true;
			NodeBeingChanged->ErrorMsg = NameEntryValidator->GetErrorString(NewText.ToString(), Valid);
			NodeBeingChanged->ErrorType = EMessageSeverity::Error;
		}
	}
	NameEntryValidator.Reset();

	return bValid;
}

void FBlueprintEditor::OnNodeTitleCommitted(const FText& NewText, ETextCommit::Type CommitInfo, UEdGraphNode* NodeBeingChanged)
{
	if (NodeBeingChanged)
	{
		const FScopedTransaction Transaction( NSLOCTEXT( "K2_RenameNode", "RenameNode", "Rename Node" ) );
		NodeBeingChanged->Modify();
		NodeBeingChanged->OnRenameNode(NewText.ToString());
	}
}



/////////////////////////////////////////////////////

void FBlueprintEditor::OnEditTabClosed(TSharedRef<SDockTab> Tab)
{
	// Update the edited object state
	if (GetBlueprintObj())
	{
		SaveEditedObjectState();
	}
}

// Tries to open the specified graph and bring it's document to the front
TSharedPtr<SGraphEditor> FBlueprintEditor::OpenGraphAndBringToFront(UEdGraph* Graph)
{
	if (!Graph || Graph->IsPendingKill())
	{
		return TSharedPtr<SGraphEditor>();
	}

	// First, switch back to standard mode
	SetCurrentMode(FBlueprintEditorApplicationModes::StandardBlueprintEditorMode);

	// Next, try to make sure there is a copy open
	TSharedPtr<SDockTab> TabWithGraph = OpenDocument(Graph, FDocumentTracker::CreateHistoryEvent);

	// We know that the contents of the opened tabs will be a graph editor.
	TSharedRef<SGraphEditor> NewGraphEditor = StaticCastSharedRef<SGraphEditor>(TabWithGraph->GetContent());

	// Handover the keyboard focus to the new graph editor widget.
	NewGraphEditor->CaptureKeyboard();

	return NewGraphEditor;
}

TSharedPtr<SDockTab> FBlueprintEditor::OpenDocument(const UObject* DocumentID, FDocumentTracker::EOpenDocumentCause Cause)
{
	TSharedRef<FTabPayload_UObject> Payload = FTabPayload_UObject::Make(DocumentID);
	return DocumentManager->OpenDocument(Payload, Cause);
}

void FBlueprintEditor::NavigateTab(FDocumentTracker::EOpenDocumentCause InCause)
{
	OpenDocument(NULL, InCause);
}

void FBlueprintEditor::CloseDocumentTab(const UObject* DocumentID)
{
	TSharedRef<FTabPayload_UObject> Payload = FTabPayload_UObject::Make(DocumentID);
	DocumentManager->CloseTab(Payload);
}

// Finds any open tabs containing the specified document and adds them to the specified array; returns true if at least one is found
bool FBlueprintEditor::FindOpenTabsContainingDocument(const UObject* DocumentID, /*inout*/ TArray< TSharedPtr<SDockTab> >& Results)
{
	int32 StartingCount = Results.Num();

	TSharedRef<FTabPayload_UObject> Payload = FTabPayload_UObject::Make(DocumentID);

	DocumentManager->FindMatchingTabs( Payload, /*inout*/ Results);

	// Did we add anything new?
	return (StartingCount != Results.Num());
}

void FBlueprintEditor::RestoreEditedObjectState()
{
	check(IsEditingSingleBlueprint());	

	UBlueprint* Blueprint = GetBlueprintObj();
	if (Blueprint->LastEditedDocuments.Num() == 0)
	{
		if (FBlueprintEditorUtils::SupportsConstructionScript(Blueprint))
		{
			Blueprint->LastEditedDocuments.Add(FBlueprintEditorUtils::FindUserConstructionScript(Blueprint));
		}

		Blueprint->LastEditedDocuments.Add(FBlueprintEditorUtils::FindEventGraph(Blueprint));
	}

	for (int32 i = 0; i < Blueprint->LastEditedDocuments.Num(); i++)
	{
		if (UObject* Obj = Blueprint->LastEditedDocuments[i].EditedObject)
		{
			if(UEdGraph* Graph = Cast<UEdGraph>(Obj))
			{
				struct LocalStruct
				{
					static TSharedPtr<SDockTab> OpenGraphTree(FBlueprintEditor* InBlueprintEditor, UEdGraph* InGraph)
					{
						FDocumentTracker::EOpenDocumentCause OpenCause = FDocumentTracker::QuickNavigateCurrentDocument;

						for (UObject* OuterObject = InGraph->GetOuter(); OuterObject; OuterObject = OuterObject->GetOuter())
						{
							if (OuterObject->IsA<UBlueprint>())
							{
								// reached up to the blueprint for the graph, we are done climbing the tree
								OpenCause = FDocumentTracker::OpenNewDocument;
								break;
							}
							else if(UEdGraph* OuterGraph = Cast<UEdGraph>(OuterObject))
							{
								// Found another graph, open it up
								OpenGraphTree(InBlueprintEditor, OuterGraph);
								break;
							}
						}

						return InBlueprintEditor->OpenDocument(InGraph, OpenCause);
					}
				};
				TSharedPtr<SDockTab> TabWithGraph = LocalStruct::OpenGraphTree(this, Graph);

				TSharedRef<SGraphEditor> GraphEditor = StaticCastSharedRef<SGraphEditor>(TabWithGraph->GetContent());
				GraphEditor->SetViewLocation(Blueprint->LastEditedDocuments[i].SavedViewOffset, Blueprint->LastEditedDocuments[i].SavedZoomAmount);
			}
			else
			{
				TSharedPtr<SDockTab> TabWithGraph = OpenDocument(Obj, FDocumentTracker::RestorePreviousDocument);
			}
		}
	}
}

bool FBlueprintEditor::CanRecompileModules()
{
	// We're not able to recompile if a compile is already in progress!
	return !IHotReloadModule::Get().IsCurrentlyCompiling();
}

void FBlueprintEditor::OnCreateComment()
{
	TSharedPtr<SGraphEditor> GraphEditor = FocusedGraphEdPtr.Pin();
	if (GraphEditor.IsValid())
	{
		if (UEdGraph* Graph = GraphEditor->GetCurrentGraph())
		{
			if (const UEdGraphSchema* Schema = Graph->GetSchema())
			{
				if (Schema->IsA(UEdGraphSchema_K2::StaticClass()))
				{
					FEdGraphSchemaAction_K2AddComment CommentAction;
					CommentAction.PerformAction(Graph, NULL, GraphEditor->GetPasteLocation());
				}
			}
		}
	}
}

void FBlueprintEditor::SetPinVisibility(SGraphEditor::EPinVisibility Visibility)
{
	PinVisibility = Visibility;
	OnSetPinVisibility.Broadcast(PinVisibility);
}

void FBlueprintEditor::OnFindReferences()
{
	TSharedPtr<SGraphEditor> GraphEditor = FocusedGraphEdPtr.Pin();
	if (GraphEditor.IsValid())
	{
		FString SearchTerm;

		const FGraphPanelSelectionSet SelectedNodes = GraphEditor->GetSelectedNodes();
		for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
		{
			UEdGraphNode* SelectedNode = Cast<UEdGraphNode>(*NodeIt);
			if (SelectedNode != NULL)
			{
				if (UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(SelectedNode))
				{
					SearchTerm =  CustomEvent->CustomFunctionName.ToString();
					if (!SearchTerm.IsEmpty())
					{
						SummonSearchUI(true, FString::Printf(TEXT("\"%s\""), *SearchTerm));
					}
				}
				else
				{
					SearchTerm = SelectedNode->GetFindReferenceSearchString();
					if (!SearchTerm.IsEmpty())
					{
						SummonSearchUI(true, SearchTerm);
					}
				}
			}
		}
	}
}

bool FBlueprintEditor::CanFindReferences()
{
	return GetSingleSelectedNode() != nullptr;
}

AActor* FBlueprintEditor::GetPreviewActor() const
{
	UBlueprint* PreviewBlueprint = GetBlueprintObj();

	// Note: The weak ptr can become stale if the actor is reinstanced due to a Blueprint change, etc. In that 
	// case we look to see if we can find the new instance in the preview world and then update the weak ptr.
	if ( PreviewActorPtr.IsStale(true) && PreviewBlueprint )
	{
		UWorld* PreviewWorld = PreviewScene.GetWorld();
		for ( TActorIterator<AActor> It(PreviewWorld); It; ++It )
		{
			AActor* Actor = *It;
			if ( !Actor->IsPendingKillPending()
				&& Actor->GetClass()->ClassGeneratedBy == PreviewBlueprint )
			{
				PreviewActorPtr = Actor;
				break;
			}
		}
	}

	return PreviewActorPtr.Get();
}

void FBlueprintEditor::UpdatePreviewActor(UBlueprint* InBlueprint, bool bInForceFullUpdate/* = false*/)
{
	// If the components mode isn't available there's no reason to update the preview actor.
	if ( !CanAccessComponentsMode() )
	{
		return;
	}

	AActor* PreviewActor = GetPreviewActor();

	// Signal that we're going to be constructing editor components
	if ( InBlueprint != NULL && InBlueprint->SimpleConstructionScript != NULL )
	{
		InBlueprint->SimpleConstructionScript->BeginEditorComponentConstruction();
	}

	UBlueprint* PreviewBlueprint = GetBlueprintObj();

	// If the Blueprint is changing
	if ( InBlueprint != PreviewBlueprint || bInForceFullUpdate )
	{
		// Destroy the previous actor instance
		DestroyPreview();

		// Save the Blueprint we're creating a preview for
		PreviewBlueprint = InBlueprint;

		// Spawn a new preview actor based on the Blueprint's generated class if it's Actor-based
		if ( PreviewBlueprint && PreviewBlueprint->GeneratedClass && PreviewBlueprint->GeneratedClass->IsChildOf(AActor::StaticClass()) )
		{
			FVector SpawnLocation = FVector::ZeroVector;
			FRotator SpawnRotation = FRotator::ZeroRotator;

			// Spawn an Actor based on the Blueprint's generated class
			FActorSpawnParameters SpawnInfo;
			SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			SpawnInfo.bNoFail = true;
			SpawnInfo.ObjectFlags = RF_Transient|RF_Transactional;

			{
				FMakeClassSpawnableOnScope TemporarilySpawnable(PreviewBlueprint->GeneratedClass);
				PreviewActorPtr = PreviewActor = PreviewScene.GetWorld()->SpawnActor(PreviewBlueprint->GeneratedClass, &SpawnLocation, &SpawnRotation, SpawnInfo);
			}

			check(PreviewActor);

			// Ensure that the actor is visible
			if ( PreviewActor->bHidden )
			{
				PreviewActor->bHidden = false;
				PreviewActor->MarkComponentsRenderStateDirty();				
			}

			// Prevent any audio from playing as a result of spawning
			if (FAudioDevice* AudioDevice = GEngine->GetMainAudioDevice())
			{
				AudioDevice->Flush(PreviewScene.GetWorld());
			}

			// Set the reference to the preview actor for component editing purposes
			if ( PreviewBlueprint->SimpleConstructionScript != nullptr )
			{
				PreviewBlueprint->SimpleConstructionScript->SetComponentEditorActorInstance(PreviewActor);
			}
		}
	}
	else if ( PreviewActor )
	{
		PreviewActor->ReregisterAllComponents();
		PreviewActor->RerunConstructionScripts();
	}

	// Signal that we're done constructing editor components
	if ( InBlueprint != nullptr && InBlueprint->SimpleConstructionScript != nullptr )
	{
		InBlueprint->SimpleConstructionScript->EndEditorComponentConstruction();
	}
}

void FBlueprintEditor::DestroyPreview()
{
	// If the components mode isn't available there's no reason to delete the preview actor.
	if ( !CanAccessComponentsMode() )
	{
		return;
	}

	AActor* PreviewActor = GetPreviewActor();
	if ( PreviewActor != nullptr )
	{
		check(PreviewScene.GetWorld());
		PreviewScene.GetWorld()->EditorDestroyActor(PreviewActor, false);
	}

	UBlueprint* PreviewBlueprint = GetBlueprintObj();

	if ( PreviewBlueprint != nullptr )
	{
		if ( PreviewBlueprint->SimpleConstructionScript != nullptr
			&& PreviewActor == PreviewBlueprint->SimpleConstructionScript->GetComponentEditorActorInstance() )
		{
			// Ensure that all editable component references are cleared
			PreviewBlueprint->SimpleConstructionScript->ClearEditorComponentReferences();

			// Clear the reference to the preview actor instance
			PreviewBlueprint->SimpleConstructionScript->SetComponentEditorActorInstance(nullptr);
		}

		PreviewBlueprint = nullptr;
	}

	PreviewActorPtr = nullptr;
}


FReply FBlueprintEditor::OnSpawnGraphNodeByShortcut(FInputChord InChord, const FVector2D& InPosition, UEdGraph* InGraph)
{
	UEdGraph* Graph = InGraph;
	if (Graph == nullptr)
	{
		return FReply::Handled();
	}

	FScopedTransaction Transaction(LOCTEXT("AddNode", "Add Node"));

	TArray<UEdGraphNode*> OutNodes;
	FVector2D NodeSpawnPos = InPosition;
	FBlueprintSpawnNodeCommands::Get().GetGraphActionByChord(InChord, InGraph, NodeSpawnPos, OutNodes);

	TSet<const UEdGraphNode*> NodesToSelect;

	for (UEdGraphNode* CurrentNode : OutNodes)
	{
		NodesToSelect.Add(CurrentNode);
	}

	// Do not change node selection if no actions were performed
	if(OutNodes.Num() > 0)
	{
		Graph->SelectNodeSet(NodesToSelect, /*bFromUI =*/true);
	}
	else
	{
		Transaction.Cancel();
	}

	return FReply::Handled();
}

void FBlueprintEditor::ToolkitBroughtToFront()
{
	UBlueprint* CurrentBlueprint = GetBlueprintObj();
	if( CurrentBlueprint != NULL )
	{
		UObject* DebugInstance = CurrentBlueprint->GetObjectBeingDebugged();
		CurrentBlueprint->SetObjectBeingDebugged( NULL );
		CurrentBlueprint->SetObjectBeingDebugged( DebugInstance );
	}
}

void FBlueprintEditor::OnNodeSpawnedByKeymap()
{
	UpdateNodeCreationStats( ENodeCreateAction::Keymap );
}

void FBlueprintEditor::UpdateNodeCreationStats( const ENodeCreateAction::Type CreateAction )
{
	switch( CreateAction )
	{
	case ENodeCreateAction::MyBlueprintDragPlacement:
		AnalyticsStats.MyBlueprintNodeDragPlacementCount++;
		break;
	case ENodeCreateAction::PaletteDragPlacement:
		AnalyticsStats.PaletteNodeDragPlacementCount++;
		break;
	case ENodeCreateAction::GraphContext:
		AnalyticsStats.NodeGraphContextCreateCount++;
		break;
	case ENodeCreateAction::PinContext:
		AnalyticsStats.NodePinContextCreateCount++;
		break;
	case ENodeCreateAction::Keymap:
		AnalyticsStats.NodeKeymapCreateCount++;
		break;
	}
}

TSharedPtr<ISCSEditorCustomization> FBlueprintEditor::CustomizeSCSEditor(USceneComponent* InComponentToCustomize) const
{
	check(InComponentToCustomize != NULL);
	const TSharedPtr<ISCSEditorCustomization>* FoundCustomization = SCSEditorCustomizations.Find(InComponentToCustomize->GetClass()->GetFName());
	if(FoundCustomization != NULL)
	{
		return *FoundCustomization;
	}

	return TSharedPtr<ISCSEditorCustomization>();
}

FText FBlueprintEditor::GetPIEStatus() const
{
	UBlueprint* CurrentBlueprint = GetBlueprintObj();
	UWorld *DebugWorld = NULL;
	ENetMode NetMode = NM_Standalone;
	if (CurrentBlueprint)
	{
		DebugWorld = CurrentBlueprint->GetWorldBeingDebugged();
		if (DebugWorld)
		{
			NetMode = DebugWorld->GetNetMode();
		}
		else
		{
			UObject* ObjOuter = CurrentBlueprint->GetObjectBeingDebugged();
			while(DebugWorld == NULL && ObjOuter != NULL)
			{
				ObjOuter = ObjOuter->GetOuter();
				DebugWorld = Cast<UWorld>(ObjOuter);
			}
		}
	}
	
	if (DebugWorld)
	{
		NetMode = DebugWorld->GetNetMode();
	}

	if (NetMode == NM_ListenServer || NetMode == NM_DedicatedServer)
	{
		return LOCTEXT("PIEStatusServerSimulating", "SERVER - SIMULATING");
	}
	else if (NetMode == NM_Client)
	{
		return LOCTEXT("PIEStatusClientSimulating", "CLIENT - SIMULATING");
	}

	return LOCTEXT("PIEStatusSimulating", "SIMULATING");
}

bool FBlueprintEditor::IsEditingAnimGraph() const
{
	if (FocusedGraphEdPtr.IsValid())
	{
		if (UEdGraph* CurrentGraph = FocusedGraphEdPtr.Pin()->GetCurrentGraph())
		{
			if (CurrentGraph->Schema->IsChildOf(UAnimationGraphSchema::StaticClass()) || (CurrentGraph->Schema == UAnimationStateMachineSchema::StaticClass()))
			{
				return true;
			}
		}
	}

	return false;
}

UEdGraph* FBlueprintEditor::GetFocusedGraph() const
{
	if(FocusedGraphEdPtr.IsValid())
	{
		return FocusedGraphEdPtr.Pin()->GetCurrentGraph();
	}
	return nullptr;
}

bool FBlueprintEditor::IsEditable(UEdGraph* InGraph) const
{
	return InEditingMode() && !FBlueprintEditorUtils::IsGraphReadOnly(InGraph);
}

bool FBlueprintEditor::IsGraphReadOnly(UEdGraph* InGraph) const
{
	return FBlueprintEditorUtils::IsGraphReadOnly(InGraph);
}

float FBlueprintEditor::GetInstructionTextOpacity(UEdGraph* InGraph) const
{
	UBlueprintEditorSettings const* Settings = GetDefault<UBlueprintEditorSettings>();
	if ((InGraph == nullptr) || !IsEditable(InGraph) || FBlueprintEditorUtils::IsGraphReadOnly(InGraph) || !Settings->bShowGraphInstructionText)
	{
		return 0.0f;
	}
	else if ((InstructionsFadeCountdown > 0.0f) || (HasOpenActionMenu == InGraph))
	{
		return InstructionsFadeCountdown / BlueprintEditorImpl::InstructionFadeDuration;
	}
	else if (BlueprintEditorImpl::GraphHasUserPlacedNodes(InGraph))
	{
		return 0.0f;
	}
	return 1.0f;
}

FText FBlueprintEditor::GetGraphDecorationString(UEdGraph* InGraph) const
{
	return FText::GetEmpty();
}

bool FBlueprintEditor::IsGraphInCurrentBlueprint(UEdGraph* InGraph) const
{
	bool bEditable = true;

	UBlueprint* EditingBP = GetBlueprintObj();
	if(EditingBP)
	{
		TArray<UEdGraph*> Graphs;
		EditingBP->GetAllGraphs(Graphs);
		bEditable &= Graphs.Contains(InGraph);
	}

	return bEditable;
}

bool FBlueprintEditor::IsFocusedGraphEditable() const
{
	UEdGraph* FocusedGraph = GetFocusedGraph();
	if (FocusedGraph != nullptr)
	{
		return IsEditable(FocusedGraph);
	}
	return true;
}

void FBlueprintEditor::TryInvokingDetailsTab(bool bFlash)
{
	if ( TabManager->CanSpawnTab(FBlueprintEditorTabs::DetailsID) )
	{
		TSharedPtr<SDockTab> BlueprintTab = FGlobalTabmanager::Get()->GetMajorTabForTabManager(TabManager.ToSharedRef());

		// We don't want to force this tab into existence when the blueprint editor isn't in the foreground and actively
		// being interacted with.  So we make sure the window it's in is focused and the tab is in the foreground.
		if ( BlueprintTab.IsValid() && BlueprintTab->IsForeground() )
		{
			TSharedPtr<SWindow> ParentWindow = BlueprintTab->GetParentWindow();
			if ( ParentWindow.IsValid() && ParentWindow->HasFocusedDescendants() )
			{
				if ( !Inspector.IsValid() || !Inspector->GetOwnerTab().IsValid() || Inspector->GetOwnerTab()->GetDockArea().IsValid() )
				{
					// Show the details panel if it doesn't exist.
					TabManager->InvokeTab(FBlueprintEditorTabs::DetailsID);

					if ( bFlash )
					{
						TSharedPtr<SDockTab> OwnerTab = Inspector->GetOwnerTab();
						if ( OwnerTab.IsValid() )
						{
							OwnerTab->FlashTab();
						}
					}
				}
			}
		}
	}
}

void FBlueprintEditor::SelectGraphActionItemByName(const FName& ItemName, ESelectInfo::Type SelectInfo, int32 SectionId, bool bIsCategory)
{
	if (MyBlueprintWidget.IsValid() && Inspector.IsValid())
	{
		// Select Item in "My Blueprint"
		MyBlueprintWidget->SelectItemByName(ItemName, SelectInfo, SectionId, bIsCategory);

		// Find associated variable
		if (FEdGraphSchemaAction_K2Var* SelectedVar = MyBlueprintWidget->SelectionAsVar())
		{
			if (UProperty* SelectedProperty = SelectedVar->GetProperty())
			{
				// Update Details Panel
				Inspector->ShowDetailsForSingleObject(SelectedProperty);
			}
		}
	}
}

/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
