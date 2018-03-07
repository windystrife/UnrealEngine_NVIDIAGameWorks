// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "K2Node_MatineeController.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Event.h"
#include "Kismet2/BlueprintEditorUtils.h"

#include "BlueprintNodeBinder.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintBoundNodeSpawner.h"
#include "KismetCompiler.h"
#include "MatineeDelegates.h"
#include "Matinee/MatineeActor.h"
#include "Matinee/InterpData.h"

UK2Node_MatineeController::UK2Node_MatineeController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (GetOutermost() != GetTransientPackage())
	{
		// Register the delegate ONLY if this isn't the transient package (temp node during compilation)
		FMatineeDelegates::Get().OnEventKeyframeAdded.AddUObject(this, &UK2Node_MatineeController::OnEventKeyframeAdded);
		FMatineeDelegates::Get().OnEventKeyframeRenamed.AddUObject(this, &UK2Node_MatineeController::OnEventKeyframeRenamed);
		FMatineeDelegates::Get().OnEventKeyframeRemoved.AddUObject(this, &UK2Node_MatineeController::OnEventKeyframeRemoved);
	}
}

void UK2Node_MatineeController::BeginDestroy()
{
	Super::BeginDestroy();

	FMatineeDelegates::Get().OnEventKeyframeAdded.RemoveAll(this);
	FMatineeDelegates::Get().OnEventKeyframeRenamed.RemoveAll(this);
	FMatineeDelegates::Get().OnEventKeyframeRemoved.RemoveAll(this);
}

void UK2Node_MatineeController::AllocateDefaultPins()
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	// Preload the matinee data, if needed, so that we can have all the event tracks we need
	if (MatineeActor)
	{
		PreloadObject(MatineeActor);
		PreloadObject(MatineeActor->MatineeData);
	}

	// Create the "finished" playing pin
	CreatePin(EGPD_Output, K2Schema->PC_Exec, FString(), nullptr, K2Schema->PN_MatineeFinished);

	// Create pins for each event
	if (MatineeActor && MatineeActor->MatineeData)
	{
		TArray<FName> EventNames;
		MatineeActor->MatineeData->GetAllEventNames(EventNames);

		for(int32 i=0; i<EventNames.Num(); i++)
		{
			FName EventName = EventNames[i];
			CreatePin(EGPD_Output, K2Schema->PC_Exec, FString(), nullptr, EventName.ToString());			
		}
	}

	Super::AllocateDefaultPins();
}

void UK2Node_MatineeController::PreloadRequiredAssets()
{
	if(MatineeActor)
	{
		PreloadObject(MatineeActor);
		PreloadObject(MatineeActor->MatineeData);
	}
	Super::PreloadRequiredAssets();
}

FLinearColor UK2Node_MatineeController::GetNodeTitleColor() const
{
	return FLinearColor(1.0f, 0.51f, 0.0f);
}


FText UK2Node_MatineeController::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if(MatineeActor != NULL)
	{
		return FText::FromString(MatineeActor->GetActorLabel());
	}
	else
	{
		return NSLOCTEXT("K2Node", "InvalidMatineeController", "INVALID MATINEECONTROLLER");
	}
}

AActor* UK2Node_MatineeController::GetReferencedLevelActor() const
{
	return MatineeActor;
}

UEdGraphPin* UK2Node_MatineeController::GetFinishedPin() const
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	return FindPin(K2Schema->PN_MatineeFinished);
}

void UK2Node_MatineeController::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	if (SourceGraph != CompilerContext.ConsolidatedEventGraph)
	{
		CompilerContext.MessageLog.Error(*FString::Printf(*NSLOCTEXT("KismetCompiler", "InvalidNodeOutsideUbergraph_Error", "Unexpected node @@ found outside ubergraph.").ToString()), this);
		return;
	}

	Super::ExpandNode(CompilerContext, SourceGraph);

	if (MatineeActor != NULL)
	{
		UFunction* MatineeEventSig = FindObject<UFunction>(AMatineeActor::StaticClass(), TEXT("OnMatineeEvent__DelegateSignature"));
		check(MatineeEventSig != NULL);

		const UEdGraphSchema_K2* Schema = CompilerContext.GetSchema();

		// Create event for each exec output pin
		for(int32 PinIdx=0; PinIdx<Pins.Num(); PinIdx++)
		{
			UEdGraphPin* MatineePin = Pins[PinIdx];
			if(MatineePin->Direction == EGPD_Output && MatineePin->PinType.PinCategory == Schema->PC_Exec)
			{
				FName EventFuncName = MatineeActor->GetFunctionNameForEvent( FName(*(MatineePin->PinName)) );

				UK2Node_Event* MatineeEventNode = CompilerContext.SpawnIntermediateEventNode<UK2Node_Event>(this, MatineePin, SourceGraph);
				MatineeEventNode->EventReference.SetFromField<UFunction>(MatineeEventSig, false);
				MatineeEventNode->CustomFunctionName = EventFuncName;
				MatineeEventNode->bInternalEvent = true;
				MatineeEventNode->AllocateDefaultPins();

				// Move connection from matinee output to event node output
				UEdGraphPin* EventOutputPin = Schema->FindExecutionPin(*MatineeEventNode, EGPD_Output);
				check(EventOutputPin != NULL);
				CompilerContext.MovePinLinksToIntermediate(*MatineePin, *EventOutputPin);
			}
		}
	}
}

void UK2Node_MatineeController::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* ActionKey = GetClass();
	if (!ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		return;
	}

	auto CanBindObjectLambda = [](UObject const* BindingObject)
	{
		if (AMatineeActor const* Actor = Cast<AMatineeActor>(BindingObject))
		{
			// Make sure the Actor has a world
			if (Actor->GetWorld())
			{
				return true;
			}
		}
		return false;
	};

	auto UiSpecOverride = [](const FBlueprintActionContext& /*Context*/, const IBlueprintNodeBinder::FBindingSet& Bindings, FBlueprintActionUiSpec* UiSpecOut)
	{
		if (Bindings.Num() == 1)
		{
			UiSpecOut->MenuName = FText::Format(NSLOCTEXT("K2Node", "MatineeeControllerTitle", "Create a Matinee Controller for {0}"),
				FText::FromString((*(Bindings.CreateConstIterator()))->GetName()));
		}
		else if (Bindings.Num() > 1)
		{
			UiSpecOut->MenuName = FText::Format(NSLOCTEXT("K2Node", "MultipleMatineeeControllerTitle", "Create Matinee Controllers for {0} selected MatineeActors"),
				FText::AsNumber(Bindings.Num()));
		}
		else
		{
			UiSpecOut->MenuName = NSLOCTEXT("K2Node", "FallbackMatineeeControllerTitle", "Error: No MatineeActors in Context");
		}
	};

	auto PostBindSetupLambda = [](UEdGraphNode* NewNode, UObject* BindObject)
	{
		UK2Node_MatineeController* MatineeNode = CastChecked<UK2Node_MatineeController>(NewNode);
		MatineeNode->MatineeActor = CastChecked<AMatineeActor>(BindObject);
		MatineeNode->MatineeActor->MatineeControllerName = MatineeNode->GetFName();
		return true;
	};

	auto FindPreExistingNodeLambda = []( const UBlueprint* Blueprint, IBlueprintNodeBinder::FBindingSet const& BindingSet ) -> UEdGraphNode*
	{
		TArray<UK2Node_MatineeController*> ExistingMatineeControllers;
		FBlueprintEditorUtils::GetAllNodesOfClass(Blueprint, ExistingMatineeControllers);
		for (auto Controller : ExistingMatineeControllers)
		{
			if (BindingSet.Contains( Controller->MatineeActor ))
			{
				return Controller;
			}
		}
		return nullptr;
	};

	UBlueprintBoundNodeSpawner* NodeSpawner = UBlueprintBoundNodeSpawner::Create(GetClass());
	NodeSpawner->CanBindObjectDelegate = UBlueprintBoundNodeSpawner::FCanBindObjectDelegate::CreateStatic(CanBindObjectLambda);
	NodeSpawner->OnBindObjectDelegate  = UBlueprintBoundNodeSpawner::FOnBindObjectDelegate::CreateStatic(PostBindSetupLambda);
	NodeSpawner->DynamicUiSignatureGetter = UBlueprintBoundNodeSpawner::FUiSpecOverrideDelegate::CreateStatic(UiSpecOverride);
	NodeSpawner->FindPreExistingNodeDelegate = UBlueprintBoundNodeSpawner::FFindPreExistingNodeDelegate::CreateStatic( FindPreExistingNodeLambda );
	ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
}

void UK2Node_MatineeController::OnEventKeyframeAdded(const AMatineeActor* InMatineeActor, const FName& InPinName, int32 InIndex)
{
	if ( MatineeActor == InMatineeActor )
	{
		// Only add unique event names to the controller node
		if (!FindPin(InPinName.ToString()))
		{
			const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

			// Add one to the index as we insert "finished" at index 0
			CreatePin(EGPD_Output, K2Schema->PC_Exec, FString(), nullptr, InPinName.ToString(), EPinContainerType::None, false, false, InIndex + 1);

			// Update and refresh the blueprint
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprint());
		}
	}
}

void UK2Node_MatineeController::OnEventKeyframeRenamed(const AMatineeActor* InMatineeActor, const FName& InOldPinName, const FName& InNewPinName)
{
	if ( MatineeActor == InMatineeActor )
	{
		if( UEdGraphPin* OldPin = FindPin(InOldPinName.ToString()) )
		{
			OldPin->Modify();
			OldPin->PinName = InNewPinName.ToString();

			GetGraph()->NotifyGraphChanged();
		}
	}
}

void UK2Node_MatineeController::OnEventKeyframeRemoved(const AMatineeActor* InMatineeActor, const TArray<FName>& InPinNames)
{
	if ( MatineeActor == InMatineeActor )
	{
		bool bNeedsRefresh = false;
		for(int32 PinIdx=0; PinIdx<InPinNames.Num(); PinIdx++)
		{
			if(UEdGraphPin* Pin = FindPin(InPinNames[PinIdx].ToString()))
			{
				RemovePin(Pin);
				bNeedsRefresh = true;
			}
		}

		if ( bNeedsRefresh )
		{
			// Update and refresh the blueprint
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprint());
		}
	}
}
