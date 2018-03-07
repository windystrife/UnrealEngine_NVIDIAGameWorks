// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "K2Node_AnimGetter.h"
#include "Animation/AnimBlueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Animation/AnimInstance.h"
#include "AnimationGraphSchema.h"
#include "AnimationTransitionSchema.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "AnimGraphNode_AssetPlayerBase.h"
#include "AnimGraphNode_StateMachine.h"
#include "AnimStateNode.h"
#include "AnimationStateMachineGraph.h"
#include "AnimationCustomTransitionSchema.h"
#include "AnimStateTransitionNode.h"

#define LOCTEXT_NAMESPACE "AnimGetter"

void UK2Node_AnimGetter::AllocateDefaultPins()
{
	Super::AllocateDefaultPins();

	TArray<UEdGraphPin*> PinsToHide;
	TArray<FString> PinNames;

	// TODO: Find a nicer way to maybe pull these down from the instance class and allow 
	// projects to add new parameters from derived instances
	PinNames.Add(TEXT("CurrentTime"));
	PinNames.Add(TEXT("AssetPlayerIndex"));
	PinNames.Add(TEXT("MachineIndex"));
	PinNames.Add(TEXT("StateIndex"));
	PinNames.Add(TEXT("TransitionIndex"));

	for(FString& PinName : PinNames)
	{
		if(UEdGraphPin* FoundPin = FindPin(PinName))
		{
			PinsToHide.Add(FoundPin);
		}
	}

	for(UEdGraphPin* Pin : PinsToHide)
	{
		Pin->bHidden = true;
	}
}

FText UK2Node_AnimGetter::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return CachedTitle;
}

bool UK2Node_AnimGetter::CanCreateUnderSpecifiedSchema(const UEdGraphSchema* Schema) const
{
	return Cast<UAnimationGraphSchema>(Schema) != NULL || Cast<UAnimationTransitionSchema>(Schema) != NULL;
}

void UK2Node_AnimGetter::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	// First cache the available functions for getters
	UClass* ActionKey = GetClass();
	const UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(ActionRegistrar.GetActionKeyFilter());
	if(AnimBlueprint && ActionRegistrar.IsOpenForRegistration(AnimBlueprint))
	{
		UClass* BPClass = *AnimBlueprint->ParentClass;
		while(BPClass && !BPClass->HasAnyClassFlags(CLASS_Native))
		{
			BPClass = BPClass->GetSuperClass();
		}

		if(BPClass)
		{
			TArray<UFunction*> AnimGetters;
			for(TFieldIterator<UFunction> FuncIter(BPClass) ; FuncIter ; ++FuncIter)
			{
				UFunction* Func = *FuncIter;

				if(Func->HasMetaData(TEXT("AnimGetter")) && Func->HasAnyFunctionFlags(FUNC_Native))
				{
					AnimGetters.Add(Func);
				}
			}

			auto UiSpecOverride = [](const FBlueprintActionContext& /*Context*/, const IBlueprintNodeBinder::FBindingSet& Bindings, FBlueprintActionUiSpec* UiSpecOut, FText Title)
			{
				UiSpecOut->MenuName = Title;
			};

			TArray<UAnimGraphNode_AssetPlayerBase*> AssetPlayerNodes;
			TArray<UAnimGraphNode_StateMachine*> MachineNodes;
			TArray<UAnimStateNode*> StateNodes;
			TArray<UAnimStateTransitionNode*> TransitionNodes;
			
			FBlueprintEditorUtils::GetAllNodesOfClass(AnimBlueprint, AssetPlayerNodes);
			FBlueprintEditorUtils::GetAllNodesOfClass(AnimBlueprint, MachineNodes);
			FBlueprintEditorUtils::GetAllNodesOfClass(AnimBlueprint, StateNodes);
			FBlueprintEditorUtils::GetAllNodesOfClass(AnimBlueprint, TransitionNodes);

			for(UFunction* Getter : AnimGetters)
			{
				FNodeSpawnData Params;
				Params.AnimInstanceClass = BPClass;
				Params.Getter = Getter;
				Params.SourceBlueprint = AnimBlueprint;
				Params.GetterContextString = Getter->GetMetaData(TEXT("GetterContext"));

				if(GetterRequiresParameter(Getter, TEXT("AssetPlayerIndex")))
				{
					for(UAnimGraphNode_Base* AssetNode : AssetPlayerNodes)
					{
						// Should always succeed
						if(UAnimationAsset* NodeAsset = AssetNode->GetAnimationAsset())
						{
							FText Title = FText::Format(LOCTEXT("NodeTitle", "{0} ({1})"), Getter->GetDisplayNameText(), FText::FromString(*NodeAsset->GetName()));
							Params.SourceNode = AssetNode;
							Params.CachedTitle = Title;

							UBlueprintNodeSpawner* Spawner = UBlueprintNodeSpawner::Create(UK2Node_AnimGetter::StaticClass(), /*AssetNode->GetGraph()*/nullptr, UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateUObject(this, &UK2Node_AnimGetter::PostSpawnNodeSetup, Params));
							Spawner->DynamicUiSignatureGetter = UBlueprintNodeSpawner::FUiSpecOverrideDelegate::CreateStatic(UiSpecOverride, Title);
							ActionRegistrar.AddBlueprintAction(AnimBlueprint, Spawner);
						}
					}
				}
				else if(GetterRequiresParameter(Getter, TEXT("MachineIndex")))
				{
					if(GetterRequiresParameter(Getter, TEXT("StateIndex")))
					{
						for(UAnimStateNode* StateNode : StateNodes)
						{
							// Get the state machine node from the outer chain
							UAnimationStateMachineGraph* Graph = Cast<UAnimationStateMachineGraph>(StateNode->GetOuter());
							if(Graph)
							{
								if(UAnimGraphNode_StateMachine* MachineNode = Cast<UAnimGraphNode_StateMachine>(Graph->GetOuter()))
								{
									Params.SourceNode = MachineNode;
								}
							}

							FText Title = FText::Format(LOCTEXT("NodeTitle", "{0} ({1})"), Getter->GetDisplayNameText(), StateNode->GetNodeTitle(ENodeTitleType::ListView));
							Params.SourceStateNode = StateNode;
							Params.CachedTitle = Title;

							UBlueprintNodeSpawner* Spawner = UBlueprintNodeSpawner::Create(UK2Node_AnimGetter::StaticClass(), /*StateNode->GetGraph()*/nullptr, UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateUObject(this, &UK2Node_AnimGetter::PostSpawnNodeSetup, Params));
							Spawner->DynamicUiSignatureGetter = UBlueprintNodeSpawner::FUiSpecOverrideDelegate::CreateStatic(UiSpecOverride, Title);
							ActionRegistrar.AddBlueprintAction(AnimBlueprint, Spawner);
						}
					}
					else if(GetterRequiresParameter(Getter, TEXT("TransitionIndex")))
					{
						for(UAnimStateTransitionNode* TransitionNode : TransitionNodes)
						{
							UAnimationStateMachineGraph* Graph = Cast<UAnimationStateMachineGraph>(TransitionNode->GetOuter());
							if(Graph)
							{
								if(UAnimGraphNode_StateMachine* MachineNode = Cast<UAnimGraphNode_StateMachine>(Graph->GetOuter()))
								{
									Params.SourceNode = MachineNode;
								}
							}

							FText Title = FText::Format(LOCTEXT("NodeTitle", "{0} ({1})"), Getter->GetDisplayNameText(), TransitionNode->GetNodeTitle(ENodeTitleType::ListView));
							Params.SourceStateNode = TransitionNode;
							Params.CachedTitle = Title;

							UBlueprintNodeSpawner* Spawner = UBlueprintNodeSpawner::Create(UK2Node_AnimGetter::StaticClass(), /*TransitionNode->GetGraph()*/nullptr, UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateUObject(this, &UK2Node_AnimGetter::PostSpawnNodeSetup, Params));
							Spawner->DynamicUiSignatureGetter = UBlueprintNodeSpawner::FUiSpecOverrideDelegate::CreateStatic(UiSpecOverride, Title);
							ActionRegistrar.AddBlueprintAction(AnimBlueprint, Spawner);
						}
					}
					else
					{
						// Only requires the state machine
						for(UAnimGraphNode_StateMachine* MachineNode : MachineNodes)
						{
							FText Title = FText::Format(LOCTEXT("NodeTitle", "{0} ({1})"), Getter->GetDisplayNameText(), MachineNode->GetNodeTitle(ENodeTitleType::ListView));
							Params.SourceNode = MachineNode;
							Params.CachedTitle = Title;

							UBlueprintNodeSpawner* Spawner = UBlueprintNodeSpawner::Create(UK2Node_AnimGetter::StaticClass(), /*MachineNode*/nullptr, UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateUObject(this, &UK2Node_AnimGetter::PostSpawnNodeSetup, Params));
							Spawner->DynamicUiSignatureGetter = UBlueprintNodeSpawner::FUiSpecOverrideDelegate::CreateStatic(UiSpecOverride, Title);
							ActionRegistrar.AddBlueprintAction(AnimBlueprint, Spawner);
						}
					}
				}
				else
				{
					// Doesn't operate on a node, only need one entry
					FText Title = FText::Format(LOCTEXT("NodeTitleNoNode", "{0}"), Getter->GetDisplayNameText());
					Params.CachedTitle = Title;

					UBlueprintNodeSpawner* Spawner = UBlueprintNodeSpawner::Create(UK2Node_AnimGetter::StaticClass(), nullptr, UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateUObject(this, &UK2Node_AnimGetter::PostSpawnNodeSetup, Params));
					Spawner->DynamicUiSignatureGetter = UBlueprintNodeSpawner::FUiSpecOverrideDelegate::CreateStatic(UiSpecOverride, Title);
					ActionRegistrar.AddBlueprintAction(AnimBlueprint, Spawner);
				}
			}
		}
	}
}

bool UK2Node_AnimGetter::IsActionFilteredOut(FBlueprintActionFilter const& Filter)
{
	if(Filter.Context.Graphs.Num() > 0)
	{
		UEdGraph* CurrGraph = Filter.Context.Graphs[0];

		if(CurrGraph && Filter.Context.Blueprints.Num() > 0)
		{
			UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(Filter.Context.Blueprints[0]);
			check(AnimBlueprint);

			if(SourceAnimBlueprint == AnimBlueprint)
			{
				// Get the native anim instance derived class
				UClass* NativeInstanceClass = AnimBlueprint->ParentClass;
				while(NativeInstanceClass && !NativeInstanceClass->HasAnyClassFlags(CLASS_Native))
				{
					NativeInstanceClass = NativeInstanceClass->GetSuperClass();
				}

				if(GetterClass != NativeInstanceClass)
				{
					// If the anim instance containing the getter is not the class we're currently using then bail
					return true;
				}

				const UEdGraphSchema* Schema = CurrGraph->GetSchema();
				
				// Bail if we aren't meant for this graph at all
				if(!IsContextValidForSchema(Schema))
				{
					return true;
				}

				if(Cast<UAnimationTransitionSchema>(Schema) || Cast<UAnimationCustomTransitionSchema>(Schema))
				{
					if(!SourceNode && !SourceStateNode)
					{
						// No dependancies, always allow
						return false;
					}

					// Inside a transition graph
					if(SourceNode)
					{
						if(UAnimStateTransitionNode* TransitionNode = Cast<UAnimStateTransitionNode>(CurrGraph->GetOuter()))
						{
							if(SourceStateNode)
							{
								if(UAnimStateTransitionNode* SourceTransitionNode = Cast<UAnimStateTransitionNode>(SourceStateNode))
								{
									// if we have a transition node, make sure it's the same as the one we're in
									if(SourceTransitionNode == TransitionNode)
									{
										return false;
									}
								}
								else if(UAnimStateNode* PreviousStateNode = Cast<UAnimStateNode>(TransitionNode->GetPreviousState()))
								{
									// Only allow actions using states that are referencing the previous state
									if(SourceStateNode == PreviousStateNode)
									{
										return false;
									}
								}
							}
							else if(UAnimGraphNode_StateMachine* MachineNode = Cast<UAnimGraphNode_StateMachine>(SourceNode))
							{
								// Available everywhere
								return false;
							}
							else if(UAnimStateNode* PrevStateNode = Cast<UAnimStateNode>(TransitionNode->GetPreviousState()))
							{
								// Make sure the attached asset node is in the source graph
								if(SourceNode && SourceNode->GetGraph() == PrevStateNode->BoundGraph)
								{
									return false;
								}
							}
						}
					}
				}
				else if(Cast<UAnimationGraphSchema>(Schema))
				{
					// Inside normal anim graph
					if(SourceStateNode)
					{
						for(UBlueprint* Blueprint : Filter.Context.Blueprints)
						{
							TArray<UAnimStateNode*> StateNodes;
							FBlueprintEditorUtils::GetAllNodesOfClass(Blueprint, StateNodes);

							if(StateNodes.Contains(SourceStateNode))
							{
								return false;
							}
						}
					}
				}
			}
		}
	}

	return true;
}

bool UK2Node_AnimGetter::GetterRequiresParameter(const UFunction* Getter, FString ParamName) const
{
	bool bRequiresParameter = false;

	for(TFieldIterator<UProperty> PropIt(Getter); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
	{
		UProperty* Prop = *PropIt;
		
		if(Prop->GetName() == ParamName)
		{
			bRequiresParameter = true;
			break;
		}
	}

	return bRequiresParameter;
}


void UK2Node_AnimGetter::PostSpawnNodeSetup(UEdGraphNode* NewNode, bool bIsTemplateNode, FNodeSpawnData SpawnData)
{
	UK2Node_AnimGetter* TypedNode = CastChecked<UK2Node_AnimGetter>(NewNode);

	// Apply parameters
	TypedNode->SourceNode = SpawnData.SourceNode;
	TypedNode->SourceStateNode = SpawnData.SourceStateNode;
	TypedNode->GetterClass = SpawnData.AnimInstanceClass;
	TypedNode->SourceAnimBlueprint = SpawnData.SourceBlueprint;
	TypedNode->SetFromFunction((UFunction*)SpawnData.Getter);
	TypedNode->CachedTitle = SpawnData.CachedTitle;
	
	SpawnData.GetterContextString.ParseIntoArray(TypedNode->Contexts, TEXT("|"), 1);
}

bool UK2Node_AnimGetter::IsContextValidForSchema(const UEdGraphSchema* Schema) const
{
	if(Contexts.Num() == 0)
	{
		// Valid in all graphs
		return true;
	}
	
	for(const FString& Context : Contexts)
	{
		UClass* ClassToCheck = nullptr;
		if(Context == TEXT("CustomBlend"))
		{
			ClassToCheck = UAnimationCustomTransitionSchema::StaticClass();
		}

		if(Context == TEXT("Transition"))
		{
			ClassToCheck = UAnimationTransitionSchema::StaticClass();
		}

		if(Context == TEXT("AnimGraph"))
		{
			ClassToCheck = UAnimationGraphSchema::StaticClass();
		}

		return Schema->GetClass() == ClassToCheck;
	}

	return false;
}

FNodeSpawnData::FNodeSpawnData()
	: SourceNode(nullptr)
	, SourceStateNode(nullptr)
	, AnimInstanceClass(nullptr)
	, SourceBlueprint(nullptr)
	, Getter(nullptr)
{

}

#undef LOCTEXT_NAMESPACE
