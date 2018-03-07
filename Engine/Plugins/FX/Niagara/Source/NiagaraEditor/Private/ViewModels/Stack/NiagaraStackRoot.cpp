// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraStackRoot.h"
#include "NiagaraStackScriptItemGroup.h"
#include "NiagaraStackEmitterSpawnScriptItemGroup.h"
#include "NiagaraStackRenderItemGroup.h"
#include "NiagaraStackEventHandlerGroup.h"
#include "NiagaraStackEventScriptItemGroup.h"
#include "NiagaraSystemViewModel.h"
#include "NiagaraEmitterViewModel.h"
#include "NiagaraSystemScriptViewModel.h"
#include "NiagaraScriptViewModel.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemEditorData.h"
#include "NiagaraEmitterEditorData.h"
#include "NiagaraStackParameterStoreGroup.h"

#define LOCTEXT_NAMESPACE "NiagaraStackViewModel"

UNiagaraStackRoot::UNiagaraStackRoot()
	: EmitterSpawnGroup(nullptr)
	, EmitterUpdateGroup(nullptr)
	, ParticleSpawnGroup(nullptr)
	, ParticleUpdateGroup(nullptr)
	, AddEventHandlerGroup(nullptr)
	, RenderGroup(nullptr)
	, SystemExposedVariablesGroup(nullptr)
{
}

void UNiagaraStackRoot::Initialize(TSharedRef<FNiagaraSystemViewModel> InSystemViewModel, TSharedRef<FNiagaraEmitterViewModel> InEmitterViewModel)
{
	Super::Initialize(InSystemViewModel, InEmitterViewModel);
	EmitterSpawnGroup = nullptr;
	EmitterUpdateGroup = nullptr;
	ParticleSpawnGroup = nullptr;
	ParticleUpdateGroup = nullptr;
	AddEventHandlerGroup = nullptr;
	RenderGroup = nullptr;
	SystemExposedVariablesGroup = nullptr;
}


bool UNiagaraStackRoot::GetShouldShowInStack() const
{
	return false;
}

void UNiagaraStackRoot::RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren)
{
	// We only allow displaying and editing system stacks if the system isn't transient which is the case in the emitter editor.
	bool bShowSystemGroups = GetSystemViewModel()->GetSystemIsTransient() == false;

	// Create static entries as needed.
	if (bShowSystemGroups && SystemExposedVariablesGroup == nullptr)
	{
		SystemExposedVariablesGroup = NewObject<UNiagaraStackParameterStoreGroup>(this);
		SystemExposedVariablesGroup->Initialize(GetSystemViewModel(), GetEmitterViewModel(), GetSystemViewModel()->GetOrCreateEditorData().GetStackEditorData(), GetSystemViewModel()->GetSystemScriptViewModel(), &GetSystemViewModel()->GetSystem(), &GetSystemViewModel()->GetSystem().GetExposedParameters());
		SystemExposedVariablesGroup->SetDisplayName(LOCTEXT("SystemExposedVariablesGroup", "System Exposed Variables"));
		SystemExposedVariablesGroup->SetTooltipText(LOCTEXT("SystemExposedVariablesGroupToolTip", "Displays the variables created in the User namespace. These variables are exposed to owning UComponents, blueprints, etc."));
	}

	if (bShowSystemGroups && SystemSpawnGroup == nullptr)
	{
		SystemSpawnGroup = NewObject<UNiagaraStackScriptItemGroup>(this);
		SystemSpawnGroup->Initialize(GetSystemViewModel(), GetEmitterViewModel(), GetSystemViewModel()->GetOrCreateEditorData().GetStackEditorData(), GetSystemViewModel()->GetSystemScriptViewModel(), ENiagaraScriptUsage::SystemSpawnScript);
		SystemSpawnGroup->SetDisplayName(LOCTEXT("SystemSpawnGroupName", "System Spawn"));
		SystemSpawnGroup->SetTooltipText(LOCTEXT("SystemSpawnGroupToolTip", "Occurs once at System creation on the CPU. Modules in this section should initialize defaults and/or do initial setup.\r\nModules are executed in order from top to bottom of the stack."));
	}

	if (bShowSystemGroups && SystemUpdateGroup == nullptr)
	{
		SystemUpdateGroup = NewObject<UNiagaraStackScriptItemGroup>(this);
		SystemUpdateGroup->Initialize(GetSystemViewModel(), GetEmitterViewModel(), GetSystemViewModel()->GetOrCreateEditorData().GetStackEditorData(), GetSystemViewModel()->GetSystemScriptViewModel(), ENiagaraScriptUsage::SystemUpdateScript);
		SystemUpdateGroup->SetDisplayName(LOCTEXT("SystemUpdateGroupName", "System Update"));
		SystemUpdateGroup->SetTooltipText(LOCTEXT("SystemUpdateGroupToolTip", "Occurs every Emitter tick on the CPU.Modules in this section should compute values for parameters for emitter or particle update or spawning this frame.\r\nModules are executed in order from top to bottom of the stack."));
	}

	if (EmitterSpawnGroup == nullptr)
	{
		EmitterSpawnGroup = NewObject<UNiagaraStackEmitterSpawnScriptItemGroup>(this);
		EmitterSpawnGroup->Initialize(GetSystemViewModel(), GetEmitterViewModel(), GetEmitterViewModel()->GetOrCreateEditorData().GetStackEditorData(), GetEmitterViewModel()->GetSharedScriptViewModel(), ENiagaraScriptUsage::EmitterSpawnScript);
		EmitterSpawnGroup->SetDisplayName(LOCTEXT("EmitterSpawnGroupName", "Emitter Spawn"));
		EmitterSpawnGroup->SetTooltipText(LOCTEXT("EmitterSpawnGroupTooltip", "Occurs once at Emitter creation on the CPU. Modules in this section should initialize defaults and/or do initial setup.\r\nModules are executed in order from top to bottom of the stack."));
	}

	if (EmitterUpdateGroup == nullptr)
	{
		EmitterUpdateGroup = NewObject<UNiagaraStackScriptItemGroup>(this);
		EmitterUpdateGroup->Initialize(GetSystemViewModel(), GetEmitterViewModel(), GetEmitterViewModel()->GetOrCreateEditorData().GetStackEditorData(), GetEmitterViewModel()->GetSharedScriptViewModel(), ENiagaraScriptUsage::EmitterUpdateScript);
		EmitterUpdateGroup->SetDisplayName(LOCTEXT("EmitterUpdateGroupName", "Emitter Update"));
		EmitterUpdateGroup->SetTooltipText(LOCTEXT("EmitterUpdateGroupTooltip", "Occurs every Emitter tick on the CPU. Modules in this section should compute values for parameters for Particle Update or Spawning this frame.\r\nModules are executed in order from top to bottom of the stack."));
	}

	if (ParticleSpawnGroup == nullptr)
	{
		ParticleSpawnGroup = NewObject<UNiagaraStackScriptItemGroup>(this);
		ParticleSpawnGroup->Initialize(GetSystemViewModel(), GetEmitterViewModel(), GetEmitterViewModel()->GetOrCreateEditorData().GetStackEditorData(), GetEmitterViewModel()->GetSharedScriptViewModel(), ENiagaraScriptUsage::ParticleSpawnScript);
		ParticleSpawnGroup->SetDisplayName(LOCTEXT("ParticleSpawnGroupName", "Particle Spawn"));
		ParticleSpawnGroup->SetTooltipText(LOCTEXT("ParticleSpawnGroupTooltip", "Called once per created particle. Modules in this section should set up initial values for each particle.\r\nIf \"Use Interpolated Spawning\" is set, we will also run the Particle Update script after the Particle Spawn script.\r\nModules are executed in order from top to bottom of the stack."));
	}

	if (ParticleUpdateGroup == nullptr)
	{
		ParticleUpdateGroup = NewObject<UNiagaraStackScriptItemGroup>(this);
		ParticleUpdateGroup->Initialize(GetSystemViewModel(), GetEmitterViewModel(), GetEmitterViewModel()->GetOrCreateEditorData().GetStackEditorData(), GetEmitterViewModel()->GetSharedScriptViewModel(), ENiagaraScriptUsage::ParticleUpdateScript);
		ParticleUpdateGroup->SetDisplayName(LOCTEXT("ParticleUpdateGroupName", "Particle Update"));
		ParticleUpdateGroup->SetTooltipText(LOCTEXT("ParticleUpdateGroupTooltip", "Called every frame per particle. Modules in this section should update new values for this frame.\r\nModules are executed in order from top to bottom of the stack."));
	}

	if (AddEventHandlerGroup == nullptr)
	{
		AddEventHandlerGroup = NewObject<UNiagaraStackEventHandlerGroup>(this);
		AddEventHandlerGroup->Initialize(GetSystemViewModel(), GetEmitterViewModel(), GetEmitterViewModel()->GetOrCreateEditorData().GetStackEditorData());
		AddEventHandlerGroup->SetDisplayName(LOCTEXT("EventGroupName", "Add Event Handler"));
		AddEventHandlerGroup->SetTooltipText(LOCTEXT("EventGroupTooltip", "Determines how this Emitter responds to incoming events. There can be more than one event handler script stack per Emitter."));
		AddEventHandlerGroup->SetOnItemAdded(UNiagaraStackEventHandlerGroup::FOnItemAdded::CreateUObject(this, &UNiagaraStackRoot::EmitterEventArraysChanged));
	}

	if (RenderGroup == nullptr)
	{
		RenderGroup = NewObject<UNiagaraStackRenderItemGroup>(this);
		RenderGroup->Initialize(GetSystemViewModel(), GetEmitterViewModel(), GetEmitterViewModel()->GetOrCreateEditorData().GetStackEditorData());
		RenderGroup->SetDisplayName(LOCTEXT("RenderGroupName", "Render"));
		RenderGroup->SetTooltipText(LOCTEXT("RendererGroupTooltip", "Describes how we should display/present each particle. Note that this doesn't have to be visual. Multiple renderers are supported. Order in this stack is not necessarily relevant to draw order."));
	}

	// Populate new children
	if (bShowSystemGroups)
	{
		NewChildren.Add(SystemExposedVariablesGroup);
		NewChildren.Add(SystemSpawnGroup);
		NewChildren.Add(SystemUpdateGroup);
	}
	NewChildren.Add(EmitterSpawnGroup);
	NewChildren.Add(EmitterUpdateGroup);
	NewChildren.Add(ParticleSpawnGroup);
	NewChildren.Add(ParticleUpdateGroup);

	for (int32 EventHandlerIdx = 0; EventHandlerIdx < GetEmitterViewModel()->GetEmitter()->EventHandlerScriptProps.Num(); EventHandlerIdx++)
	{
		UNiagaraStackEventScriptItemGroup* EventHandlerGroup = nullptr;
		for (UNiagaraStackEntry* CurrentChild : CurrentChildren)
		{
			UNiagaraStackEventScriptItemGroup* CurrentEventHandlerChild = Cast<UNiagaraStackEventScriptItemGroup>(CurrentChild);
			if (CurrentEventHandlerChild != nullptr && CurrentEventHandlerChild->GetScriptOccurrence() == EventHandlerIdx)
			{
				EventHandlerGroup = CurrentEventHandlerChild;
				break;
			}
		}

		if (EventHandlerGroup == nullptr)
		{
			EventHandlerGroup = NewObject<UNiagaraStackEventScriptItemGroup>(this, NAME_None, RF_Transactional);
			EventHandlerGroup->Initialize(GetSystemViewModel(), GetEmitterViewModel(), GetEmitterViewModel()->GetEditorData().GetStackEditorData(), GetEmitterViewModel()->GetSharedScriptViewModel(), ENiagaraScriptUsage::ParticleEventScript, EventHandlerIdx);
			EventHandlerGroup->SetOnModifiedEventHandlers(UNiagaraStackEventScriptItemGroup::FOnModifiedEventHandlers::CreateUObject(this, &UNiagaraStackRoot::EmitterEventArraysChanged));
			EventHandlerGroup->SetTooltipText(LOCTEXT("EventGroupTooltip", "Determines how this Emitter responds to incoming events. There can be more than one event handler script stack per Emitter."));
		}

		NewChildren.Add(EventHandlerGroup);
	}

	NewChildren.Add(AddEventHandlerGroup);
	NewChildren.Add(RenderGroup);
}

void UNiagaraStackRoot::EmitterEventArraysChanged()
{
	RefreshChildren();
}

#undef LOCTEXT_NAMESPACE