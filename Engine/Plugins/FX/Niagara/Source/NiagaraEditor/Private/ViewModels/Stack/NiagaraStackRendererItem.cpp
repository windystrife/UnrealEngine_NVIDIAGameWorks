// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraStackRendererItem.h"
#include "NiagaraStackObject.h"
#include "NiagaraStackItemExpander.h"
#include "NiagaraEmitter.h"
#include "NiagaraStackEditorData.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraScript.h"
#include "NiagaraEmitterViewModel.h"
#include "NiagaraScriptViewModel.h"
#include "Internationalization.h"
#include "NiagaraNodeAssignment.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraConstants.h"
#include "NiagaraScriptGraphViewModel.h"
#include "NiagaraGraph.h"
#include "ScopedTransaction.h"
#include "NiagaraEmitter.h"
#include "NiagaraScriptSource.h"
#include "ScopedTransaction.h"
#include "NiagaraStackErrorItem.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "NiagaraStackGraphUtilities.h"

#define LOCTEXT_NAMESPACE "UNiagaraStackRendererItem"

UNiagaraStackRendererItem::UNiagaraStackRendererItem()
	: RendererProperties(nullptr)
	, RendererObject(nullptr)
{
}

void UNiagaraStackRendererItem::Initialize(TSharedRef<FNiagaraSystemViewModel> InSystemViewModel, TSharedRef<FNiagaraEmitterViewModel> InEmitterViewModel, UNiagaraStackEditorData& InStackEditorData, UNiagaraRendererProperties* InRendererProperties)
{
	checkf(RendererProperties == nullptr, TEXT("Can not initialize more than once."));
	Super::Initialize(InSystemViewModel, InEmitterViewModel, InStackEditorData);
	RendererProperties = InRendererProperties;
}

TArray<FNiagaraVariable> UNiagaraStackRendererItem::GetMissingVariables(UNiagaraRendererProperties* RendererProperties, UNiagaraEmitter* Emitter)
{
	TArray<FNiagaraVariable> MissingAttributes;
	const TArray<FNiagaraVariable>& RequiredAttrs = RendererProperties->GetRequiredAttributes();
	const UNiagaraScript* Script = Emitter->SpawnScriptProps.Script;
	if (Script != nullptr)
	{
		MissingAttributes.Empty();
		for (FNiagaraVariable Attr : RequiredAttrs)
		{
			FNiagaraVariable OriginalAttr = Attr;
			// TODO .. should we always be namespaced?
			FString AttrName = Attr.GetName().ToString();
			if (AttrName.RemoveFromStart(TEXT("Particles.")))
			{
				Attr.SetName(*AttrName);
			}

			bool ContainsVar = Script->Attributes.ContainsByPredicate([&Attr](const FNiagaraVariable& Var) { return Var.GetName() == Attr.GetName(); });
			if (!ContainsVar)
			{
				MissingAttributes.Add(OriginalAttr);
			}
		}
	}
	return MissingAttributes;
}

bool UNiagaraStackRendererItem::AddMissingVariable(UNiagaraEmitter* Emitter, const FNiagaraVariable& Variable)
{
	UNiagaraScript* Script = Emitter->SpawnScriptProps.Script;
	if (!Script)
	{
		return false;
	}
	UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(Script->GetSource());
	if (!Source)
	{
		return false;
	}

	UNiagaraGraph* Graph = Source->NodeGraph;
	if (!Graph)
	{
		return false;
	}

	UNiagaraNodeOutput* OutputNode = Graph->FindOutputNode(ENiagaraScriptUsage::ParticleSpawnScript);
	if (!OutputNode)
	{
		return false;
	}

	FScopedTransaction ScopedTransaction(LOCTEXT("FixRendererError", "Fixing rendering module error: Add Attribute"));
	Graph->Modify();

	FGraphNodeCreator<UNiagaraNodeAssignment> NodeBuilder(*Graph);
	UNiagaraNodeAssignment* NewAssignmentNode = NodeBuilder.CreateNode();
	NewAssignmentNode->AssignmentTarget = Variable;
	FString VarDefaultValue = FNiagaraConstants::GetAttributeDefaultValue(NewAssignmentNode->AssignmentTarget);
	NewAssignmentNode->AssignmentDefaultValue = VarDefaultValue;
	NodeBuilder.Finalize();

	TArray<FNiagaraStackGraphUtilities::FStackNodeGroup> StackNodeGroups;
	FNiagaraStackGraphUtilities::GetStackNodeGroups(*OutputNode, StackNodeGroups);

	FNiagaraStackGraphUtilities::FStackNodeGroup AssignmentGroup;
	AssignmentGroup.StartNodes.Add(NewAssignmentNode);
	AssignmentGroup.EndNode = NewAssignmentNode;

	FNiagaraStackGraphUtilities::FStackNodeGroup& OutputGroup = StackNodeGroups[StackNodeGroups.Num() - 1];
	FNiagaraStackGraphUtilities::FStackNodeGroup& OutputGroupPrevious = StackNodeGroups[StackNodeGroups.Num() - 2];
	FNiagaraStackGraphUtilities::ConnectStackNodeGroup(AssignmentGroup, OutputGroupPrevious, OutputGroup);

	FNiagaraStackGraphUtilities::RelayoutGraph(*Graph);
	return true;
}

UNiagaraRendererProperties* UNiagaraStackRendererItem::GetRendererProperties()
{
	return RendererProperties;
}

FText UNiagaraStackRendererItem::GetDisplayName() const
{
	if (RendererProperties != nullptr)
	{
		return FText::FromString(RendererProperties->GetClass()->GetName());
	}
	else
	{
		return FText::FromName(NAME_None);
	}
}

void UNiagaraStackRendererItem::Delete()
{
	const FScopedTransaction Transaction(LOCTEXT("DeleteRenderer", "Delete Renderer"));

	UNiagaraEmitter* Emitter = GetEmitterViewModel()->GetEmitter();
	Emitter->Modify();
	Emitter->RendererProperties.Remove(RendererProperties);

	ModifiedGroupItemsDelegate.ExecuteIfBound();
}


FName UNiagaraStackRendererItem::GetItemBackgroundName() const
{
	return "NiagaraEditor.Stack.Item.BackgroundColor";
}

int32 UNiagaraStackRendererItem::GetErrorCount() const
{
	return MissingAttributes.Num();
}

bool UNiagaraStackRendererItem::GetErrorFixable(int32 ErrorIdx) const
{
	if (ErrorIdx < MissingAttributes.Num())
	{
		return true;
	}
	return false;
}

bool UNiagaraStackRendererItem::TryFixError(int32 ErrorIdx)
{
	FNiagaraVariable MissingVar = MissingAttributes[ErrorIdx];
	if (AddMissingVariable(GetEmitterViewModel()->GetEmitter(), MissingVar))
	{
		FNotificationInfo Info(FText::Format(LOCTEXT("AddedVariableForFix", "Added {0} to the Spawn script to support the renderer."), FText::FromName(MissingVar.GetName())));
		Info.ExpireDuration = 5.0f;
		Info.bFireAndForget = true;
		Info.Image = FCoreStyle::Get().GetBrush(TEXT("MessageLog.Info"));
		FSlateNotificationManager::Get().AddNotification(Info);
		return true;
	}
	return false;
}

FText UNiagaraStackRendererItem::GetErrorText(int32 ErrorIdx) const
{
	if (ErrorIdx < MissingAttributes.Num())
	{
		const FNiagaraVariable& Attr = MissingAttributes[ErrorIdx];
		return FText::Format(LOCTEXT("FailedRendererBind", "Missing attribute \"{0}\" of Type \"{1}\"."), FText::FromName(Attr.GetName()), Attr.GetType().GetNameText());
	}
	return FText();
}

void UNiagaraStackRendererItem::RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren)
{
	if (RendererObject == nullptr)
	{
		RendererObject = NewObject<UNiagaraStackObject>(this);
		RendererObject->Initialize(GetSystemViewModel(), GetEmitterViewModel(), RendererProperties);
	}

	if (RendererExpander == nullptr)
	{
		RendererExpander = NewObject<UNiagaraStackItemExpander>(this);
		RendererExpander->Initialize(GetSystemViewModel(), GetEmitterViewModel(), GetStackEditorData(), RendererProperties->GetName(), false);
		RendererExpander->SetOnExpnadedChanged(UNiagaraStackItemExpander::FOnExpandedChanged::CreateUObject(this, &UNiagaraStackRendererItem::RendererExpandedChanged));
	}
	
	if (GetStackEditorData().GetStackEntryIsExpanded(RendererProperties->GetName(), false))
	{
		NewChildren.Add(RendererObject);
	}

	NewChildren.Add(RendererExpander);

	MissingAttributes = GetMissingVariables(RendererProperties, GetEmitterViewModel()->GetEmitter());
}

void UNiagaraStackRendererItem::RendererExpandedChanged()
{
	RefreshChildren();
}

#undef LOCTEXT_NAMESPACE