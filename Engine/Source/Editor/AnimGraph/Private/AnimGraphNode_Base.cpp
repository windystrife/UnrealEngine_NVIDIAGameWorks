// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "AnimGraphNode_Base.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Animation/AnimInstance.h"
#include "AnimationGraphSchema.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "AnimBlueprintNodeOptionalPinManager.h"
#include "IAnimNodeEditMode.h"
#include "AnimNodeEditModes.h"
#include "AnimationGraph.h"
#include "EditorModeManager.h"
#include "Toolkits/AssetEditorManager.h"

/////////////////////////////////////////////////////
// UAnimGraphNode_Base

UAnimGraphNode_Base::UAnimGraphNode_Base(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UAnimGraphNode_Base::PreEditChange(UProperty* PropertyThatWillChange)
{
	Super::PreEditChange(PropertyThatWillChange);

	if (PropertyThatWillChange && PropertyThatWillChange->GetFName() == GET_MEMBER_NAME_CHECKED(FOptionalPinFromProperty, bShowPin))
	{
		FOptionalPinManager::CacheShownPins(ShowPinForProperties, OldShownPins);
	}
}

void UAnimGraphNode_Base::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	FName PropertyName = (PropertyChangedEvent.Property != NULL) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	if ((PropertyName == GET_MEMBER_NAME_CHECKED(FOptionalPinFromProperty, bShowPin)))
	{
		FOptionalPinManager::EvaluateOldShownPins(ShowPinForProperties, OldShownPins, this);
		GetSchema()->ReconstructNode(*this);
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);

	PropertyChangeEvent.Broadcast(PropertyChangedEvent);
}

void UAnimGraphNode_Base::CreateOutputPins()
{
	if (!IsSinkNode())
	{
		const UAnimationGraphSchema* Schema = GetDefault<UAnimationGraphSchema>();
		CreatePin(EGPD_Output, Schema->PC_Struct, FString(), FPoseLink::StaticStruct(), TEXT("Pose"));
	}
}

void UAnimGraphNode_Base::InternalPinCreation(TArray<UEdGraphPin*>* OldPins)
{
	// preload required assets first before creating pins
	PreloadRequiredAssets();

	const UAnimationGraphSchema* Schema = GetDefault<UAnimationGraphSchema>();
	if (const UStructProperty* NodeStruct = GetFNodeProperty())
	{
		// Display any currently visible optional pins
		{
			UObject* NodeDefaults = GetArchetype();
			FAnimBlueprintNodeOptionalPinManager OptionalPinManager(this, OldPins);
			OptionalPinManager.AllocateDefaultPins(NodeStruct->Struct, NodeStruct->ContainerPtrToValuePtr<uint8>(this), NodeDefaults ? NodeStruct->ContainerPtrToValuePtr<uint8>(NodeDefaults) : nullptr);
		}

		// Create the output pin, if needed
		CreateOutputPins();
	}
}

void UAnimGraphNode_Base::AllocateDefaultPins()
{
	InternalPinCreation(NULL);
}

void UAnimGraphNode_Base::ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins)
{
	InternalPinCreation(&OldPins);

	RestoreSplitPins(OldPins);
}

bool UAnimGraphNode_Base::CanJumpToDefinition() const
{
	return GetJumpTargetForDoubleClick() != nullptr;
}

void UAnimGraphNode_Base::JumpToDefinition() const
{
	if (UObject* HyperlinkTarget = GetJumpTargetForDoubleClick())
	{
		FAssetEditorManager::Get().OpenEditorForAsset(HyperlinkTarget);
	}
}

FLinearColor UAnimGraphNode_Base::GetNodeTitleColor() const
{
	return FLinearColor::Black;
}

UScriptStruct* UAnimGraphNode_Base::GetFNodeType() const
{
	UScriptStruct* BaseFStruct = FAnimNode_Base::StaticStruct();

	for (TFieldIterator<UProperty> PropIt(GetClass(), EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
	{
		if (UStructProperty* StructProp = Cast<UStructProperty>(*PropIt))
		{
			if (StructProp->Struct->IsChildOf(BaseFStruct))
			{
				return StructProp->Struct;
			}
		}
	}

	return NULL;
}

UStructProperty* UAnimGraphNode_Base::GetFNodeProperty() const
{
	UScriptStruct* BaseFStruct = FAnimNode_Base::StaticStruct();

	for (TFieldIterator<UProperty> PropIt(GetClass(), EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
	{
		if (UStructProperty* StructProp = Cast<UStructProperty>(*PropIt))
		{
			if (StructProp->Struct->IsChildOf(BaseFStruct))
			{
				return StructProp;
			}
		}
	}

	return NULL;
}

FString UAnimGraphNode_Base::GetNodeCategory() const
{
	return TEXT("Misc.");
}

void UAnimGraphNode_Base::GetNodeAttributes( TArray<TKeyValuePair<FString, FString>>& OutNodeAttributes ) const
{
	OutNodeAttributes.Add( TKeyValuePair<FString, FString>( TEXT( "Type" ), TEXT( "AnimGraphNode" ) ));
	OutNodeAttributes.Add( TKeyValuePair<FString, FString>( TEXT( "Class" ), GetClass()->GetName() ));
	OutNodeAttributes.Add( TKeyValuePair<FString, FString>( TEXT( "Name" ), GetName() ));
}

void UAnimGraphNode_Base::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	// actions get registered under specific object-keys; the idea is that 
	// actions might have to be updated (or deleted) if their object-key is  
	// mutated (or removed)... here we use the node's class (so if the node 
	// type disappears, then the action should go with it)
	UClass* ActionKey = GetClass();
	// to keep from needlessly instantiating a UBlueprintNodeSpawner, first   
	// check to make sure that the registrar is looking for actions of this type
	// (could be regenerating actions for a specific asset, and therefore the 
	// registrar would only accept actions corresponding to that asset)
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);
		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

FText UAnimGraphNode_Base::GetMenuCategory() const
{
	return FText::FromString(GetNodeCategory());
}

void UAnimGraphNode_Base::GetPinAssociatedProperty(const UScriptStruct* NodeType, const UEdGraphPin* InputPin, UProperty*& OutProperty, int32& OutIndex) const
{
	OutProperty = nullptr;
	OutIndex = INDEX_NONE;

	//@TODO: Name-based hackery, avoid the roundtrip and better indicate when it's an array pose pin
	int32 UnderscoreIndex = InputPin->PinName.Find(TEXT("_"), ESearchCase::CaseSensitive);
	if (UnderscoreIndex != INDEX_NONE)
	{
		FString ArrayName = InputPin->PinName.Left(UnderscoreIndex);
		int32 ArrayIndex = FCString::Atoi(*(InputPin->PinName.Mid(UnderscoreIndex + 1)));

		if (UArrayProperty* ArrayProperty = FindField<UArrayProperty>(NodeType, *ArrayName))
		{
			OutProperty = ArrayProperty;
			OutIndex = ArrayIndex;
		}
	}
	
	// If the array check failed or we have no underscores
	if(OutProperty == nullptr)
	{
		if (UProperty* Property = FindField<UProperty>(NodeType, *(InputPin->PinName)))
		{
			OutProperty = Property;
			OutIndex = INDEX_NONE;
		}
	}
}

FPoseLinkMappingRecord UAnimGraphNode_Base::GetLinkIDLocation(const UScriptStruct* NodeType, UEdGraphPin* SourcePin)
{
	if (SourcePin->LinkedTo.Num() > 0)
	{
		if (UAnimGraphNode_Base* LinkedNode = Cast<UAnimGraphNode_Base>(FBlueprintEditorUtils::FindFirstCompilerRelevantNode(SourcePin->LinkedTo[0])))
		{
			//@TODO: Name-based hackery, avoid the roundtrip and better indicate when it's an array pose pin
			int32 UnderscoreIndex = SourcePin->PinName.Find(TEXT("_"), ESearchCase::CaseSensitive);
			if (UnderscoreIndex != INDEX_NONE)
			{
				FString ArrayName = SourcePin->PinName.Left(UnderscoreIndex);
				int32 ArrayIndex = FCString::Atoi(*(SourcePin->PinName.Mid(UnderscoreIndex + 1)));

				if (UArrayProperty* ArrayProperty = FindField<UArrayProperty>(NodeType, *ArrayName))
				{
					if (UStructProperty* Property = Cast<UStructProperty>(ArrayProperty->Inner))
					{
						if (Property->Struct->IsChildOf(FPoseLinkBase::StaticStruct()))
						{
							return FPoseLinkMappingRecord::MakeFromArrayEntry(this, LinkedNode, ArrayProperty, ArrayIndex);
						}
					}
				}
			}
			else
			{
				if (UStructProperty* Property = FindField<UStructProperty>(NodeType, *(SourcePin->PinName)))
				{
					if (Property->Struct->IsChildOf(FPoseLinkBase::StaticStruct()))
					{
						return FPoseLinkMappingRecord::MakeFromMember(this, LinkedNode, Property);
					}
				}
			}
		}
	}

	return FPoseLinkMappingRecord::MakeInvalid();
}

void UAnimGraphNode_Base::CreatePinsForPoseLink(UProperty* PoseProperty, int32 ArrayIndex)
{
	const UAnimationGraphSchema* Schema = GetDefault<UAnimationGraphSchema>();
	UScriptStruct* A2PoseStruct = FA2Pose::StaticStruct();

	// pose input
	const FString NewPinName = (ArrayIndex == INDEX_NONE) ? PoseProperty->GetName() : FString::Printf(TEXT("%s_%d"), *(PoseProperty->GetName()), ArrayIndex);
	CreatePin(EGPD_Input, Schema->PC_Struct, FString(), A2PoseStruct, NewPinName);
}

void UAnimGraphNode_Base::PostProcessPinName(const UEdGraphPin* Pin, FString& DisplayName) const
{
	if (Pin->Direction == EGPD_Output)
	{
		if (Pin->PinName == TEXT("Pose"))
		{
			DisplayName.Reset();
		}
	}
}

bool UAnimGraphNode_Base::CanCreateUnderSpecifiedSchema(const UEdGraphSchema* DesiredSchema) const
{
	return DesiredSchema->GetClass()->IsChildOf(UAnimationGraphSchema::StaticClass());
}

FString UAnimGraphNode_Base::GetDocumentationLink() const
{
	return TEXT("Shared/GraphNodes/Animation");
}

void UAnimGraphNode_Base::GetPinHoverText(const UEdGraphPin& Pin, FString& HoverTextOut) const
{
	if (UAnimationGraphSchema::IsLocalSpacePosePin(Pin.PinType))
	{
		HoverTextOut = TEXT("Animation Pose");
	}
	else if (UAnimationGraphSchema::IsComponentSpacePosePin(Pin.PinType))
	{
		HoverTextOut = TEXT("Animation Pose (Component Space)");
	}
	else
	{
		Super::GetPinHoverText(Pin, HoverTextOut);
	}
}

void UAnimGraphNode_Base::HandleAnimReferenceCollection(UAnimationAsset* AnimAsset, TArray<UAnimationAsset*>& AnimationAssets) const
{
	if(AnimAsset)
	{
		AnimAsset->HandleAnimReferenceCollection(AnimationAssets, true);
	}
}

void UAnimGraphNode_Base::OnNodeSelected(bool bInIsSelected, FEditorModeTools& InModeTools, FAnimNode_Base* InRuntimeNode)
{
	const FEditorModeID ModeID = GetEditorMode();
	if (ModeID != NAME_None)
	{
		if (bInIsSelected)
		{
			InModeTools.ActivateMode(ModeID);
			if (FEdMode* EdMode = InModeTools.GetActiveMode(ModeID))
			{
				static_cast<IAnimNodeEditMode*>(EdMode)->EnterMode(this, InRuntimeNode);
			}
		}
		else
		{
			if (FEdMode* EdMode = InModeTools.GetActiveMode(ModeID))
			{
				static_cast<IAnimNodeEditMode*>(EdMode)->ExitMode();
			}
			InModeTools.DeactivateMode(ModeID);
		}
	}
}

FEditorModeID UAnimGraphNode_Base::GetEditorMode() const
{
	return AnimNodeEditModes::AnimNode;
}

FAnimNode_Base* UAnimGraphNode_Base::FindDebugAnimNode(USkeletalMeshComponent * PreviewSkelMeshComp) const
{
	FAnimNode_Base* DebugNode = nullptr;

	if (PreviewSkelMeshComp != nullptr && PreviewSkelMeshComp->GetAnimInstance() != nullptr)
	{
		// find an anim node index from debug data
		UAnimBlueprintGeneratedClass* AnimBlueprintClass = Cast<UAnimBlueprintGeneratedClass>(PreviewSkelMeshComp->GetAnimInstance()->GetClass());
		if (AnimBlueprintClass)
		{
			FAnimBlueprintDebugData& DebugData = AnimBlueprintClass->GetAnimBlueprintDebugData();
			int32* IndexPtr = DebugData.NodePropertyToIndexMap.Find(this);

			if (IndexPtr)
			{
				int32 AnimNodeIndex = *IndexPtr;
				// reverse node index temporarily because of a bug in NodeGuidToIndexMap
				AnimNodeIndex = AnimBlueprintClass->AnimNodeProperties.Num() - AnimNodeIndex - 1;

				DebugNode = AnimBlueprintClass->AnimNodeProperties[AnimNodeIndex]->ContainerPtrToValuePtr<FAnimNode_Base>(PreviewSkelMeshComp->GetAnimInstance());
			}
		}
	}

	return DebugNode;
}

EAnimAssetHandlerType UAnimGraphNode_Base::SupportsAssetClass(const UClass* AssetClass) const
{
	return EAnimAssetHandlerType::NotSupported;
}


void UAnimGraphNode_Base::PinDefaultValueChanged(UEdGraphPin* Pin)
{
	Super::PinDefaultValueChanged(Pin);

	CopyPinDefaultsToNodeData(Pin);

	if(UAnimationGraph* AnimationGraph = Cast<UAnimationGraph>(GetGraph()))
	{
		AnimationGraph->OnPinDefaultValueChanged.Broadcast(Pin);
	}
}

bool UAnimGraphNode_Base::IsPinExposedAndLinked(const FString& InPinName, const EEdGraphPinDirection InDirection) const
{
	UEdGraphPin* Pin = FindPin(InPinName, InDirection);
	return Pin != nullptr && Pin->LinkedTo.Num() > 0 && Pin->LinkedTo[0] != nullptr;
}