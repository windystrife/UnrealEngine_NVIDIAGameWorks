// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraComponentDetails.h"
#include "PropertyHandle.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterface.h"
#include "DetailLayoutBuilder.h"
#include "IDetailPropertyRow.h"
#include "DetailWidgetRow.h"
#include "SInlineEditableTextBlock.h"
#include "Widgets/SToolTip.h"
#include "PropertyCustomizationHelpers.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "EditorStyleSet.h"
#include "NiagaraSystemInstance.h"
#include "Private/ViewModels/NiagaraParameterViewModel.h"
#include "NiagaraEditorUtilities.h"
#include "INiagaraEditorTypeUtilities.h"
#include "NiagaraTypes.h"
#include "ScopedTransaction.h"
#include "SNiagaraParameterEditor.h"
#include "NiagaraEditorModule.h"
#include "PropertyEditorModule.h"
#include "ModuleManager.h"
#include "IStructureDetailsView.h"
#include "IDetailsView.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableText.h"
#include "Editor.h"
#include "NiagaraParameterCollectionViewModel.h"
#include "NiagaraScriptSource.h"
#include "NiagaraNodeInput.h"
#include "NiagaraGraph.h"
#include "GameDelegates.h"
#include "NiagaraEditorStyle.h"
#include "IDetailChildrenBuilder.h"
#include "MultiBoxBuilder.h"
#include "SComboButton.h"
#include "SImage.h"
#include "NiagaraEditorModule.h"
#include "ModuleManager.h"
#include "INiagaraEditorTypeUtilities.h"
#include "SBox.h"
#define LOCTEXT_NAMESPACE "NiagaraComponentDetails"

class FNiagaraComponentNodeBuilder : public IDetailCustomNodeBuilder
{
public:
	FNiagaraComponentNodeBuilder(UNiagaraComponent* InComponent, UNiagaraScript* SourceSpawn, UNiagaraScript* SourceUpdate)						   
	{
		Component = InComponent;
		OriginalScripts.Add(SourceSpawn);
		OriginalScripts.Add(SourceUpdate);
	}

	~FNiagaraComponentNodeBuilder()
	{
	}

	virtual void SetOnRebuildChildren(FSimpleDelegate InOnRegenerateChildren) override
	{
		OnRebuildChildren = InOnRegenerateChildren;
	}

	virtual void GenerateHeaderRowContent(FDetailWidgetRow& NodeRow) {}
	virtual void Tick(float DeltaTime) override {}
	virtual bool RequiresTick() const override { return false; }
	virtual bool InitiallyCollapsed() const { return false; }
	virtual FName GetName() const  override
	{
		static const FName NiagaraComponentNodeBuilder("FNiagaraComponentNodeBuilder");
		return NiagaraComponentNodeBuilder;
	}

	virtual void GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder) override
	{
		TArray<FNiagaraVariable> Parameters;
		FNiagaraParameterStore& ParamStore = Component->GetInitialParameters();
		ParamStore.GetParameters(Parameters);

		FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::GetModuleChecked<FNiagaraEditorModule>("NiagaraEditor");
		
		for (const FNiagaraVariable& Parameter : Parameters)
		{
			TSharedPtr<SWidget> NameWidget;

			NameWidget =
				SNew(STextBlock)
				.TextStyle(FNiagaraEditorStyle::Get(), "NiagaraEditor.ParameterText")
				.Text(FText::FromName(Parameter.GetName()));
			
			IDetailPropertyRow* Row = nullptr;

			TSharedPtr<SWidget> CustomValueWidget;

			if (!Parameter.IsDataInterface())
			{
				TSharedPtr<FStructOnScope> StructOnScope = MakeShareable(new FStructOnScope(Parameter.GetType().GetStruct(), (uint8*)ParamStore.GetParameterData(Parameter)));
				Row = ChildrenBuilder.AddExternalStructureProperty(StructOnScope.ToSharedRef(), NAME_None, Parameter.GetName());

			}
			else 
			{
				int32 DataInterfaceOffset = ParamStore.IndexOf(Parameter);
				UObject* DefaultValueObject = ParamStore.DataInterfaces[DataInterfaceOffset];

				TArray<UObject*> Objects;
				Objects.Add(DefaultValueObject);
				Row = ChildrenBuilder.AddExternalObjects(Objects, Parameter.GetName());

				CustomValueWidget =
					SNew(STextBlock)
					.TextStyle(FNiagaraEditorStyle::Get(), "NiagaraEditor.ParameterText")
					.Text(FText::FromString(FName::NameToDisplayString(DefaultValueObject->GetClass()->GetName(), false)));
			}

			if (Row)
			{
				TSharedPtr<SWidget> DefaultNameWidget;
				TSharedPtr<SWidget> DefaultValueWidget;

				TSharedPtr<IPropertyHandle> PropertyHandle = Row->GetPropertyHandle();

				FDetailWidgetRow& CustomWidget = Row->CustomWidget(true);

				TArray<UObject*> Objects;
				Row->GetPropertyHandle()->GetOuterObjects(Objects);

				Row->GetDefaultWidgets(DefaultNameWidget, DefaultValueWidget, CustomWidget);

				if (Parameter.IsDataInterface())
				{
					PropertyHandle->SetOnPropertyValuePreChange(
						FSimpleDelegate::CreateRaw(this, &FNiagaraComponentNodeBuilder::OnDataInterfacePreChange, Parameter));
					PropertyHandle->SetOnChildPropertyValuePreChange(
						FSimpleDelegate::CreateRaw(this, &FNiagaraComponentNodeBuilder::OnDataInterfacePreChange, Parameter));
					PropertyHandle->SetOnPropertyValueChanged(
						FSimpleDelegate::CreateRaw(this, &FNiagaraComponentNodeBuilder::OnDataInterfaceChanged, Parameter));
					PropertyHandle->SetOnChildPropertyValueChanged(
						FSimpleDelegate::CreateRaw(this, &FNiagaraComponentNodeBuilder::OnDataInterfaceChanged, Parameter));
				}
				else
				{
					PropertyHandle->SetOnPropertyValuePreChange(
						FSimpleDelegate::CreateRaw(this, &FNiagaraComponentNodeBuilder::OnParameterPreChange, Parameter));
					PropertyHandle->SetOnChildPropertyValuePreChange(
						FSimpleDelegate::CreateRaw(this, &FNiagaraComponentNodeBuilder::OnParameterPreChange, Parameter));
					PropertyHandle->SetOnPropertyValueChanged(
						FSimpleDelegate::CreateRaw(this, &FNiagaraComponentNodeBuilder::OnParameterChanged, Parameter));
					PropertyHandle->SetOnChildPropertyValueChanged(
						FSimpleDelegate::CreateRaw(this, &FNiagaraComponentNodeBuilder::OnParameterChanged, Parameter));
				}

				CustomWidget
					.NameContent()
					[
						SNew(SBox)
						.Padding(FMargin(0.0f, 2.0f))
						[
							NameWidget.ToSharedRef()
						]
					];

				TSharedPtr<SWidget> ValueWidget = DefaultValueWidget;
				if (CustomValueWidget.IsValid())
				{
					ValueWidget = CustomValueWidget;
				}

				CustomWidget
					.ValueContent()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.HAlign(HAlign_Fill)
						.Padding(4.0f)
						[
							// Add in the parameter editor factoried above.
							ValueWidget.ToSharedRef()
						]
						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.AutoWidth()
						[
							// Add in the "reset to default" buttons
							SNew(SButton)
							.OnClicked_Raw(this, &FNiagaraComponentNodeBuilder::OnLocationResetClicked, Parameter)
							.Visibility_Raw(this, &FNiagaraComponentNodeBuilder::GetLocationResetVisibility, Parameter)
							.ContentPadding(FMargin(5.f, 0.f))
							.ToolTipText(LOCTEXT("ResetToDefaultToolTip", "Reset to Default"))
							.ButtonStyle(FEditorStyle::Get(), "NoBorder")
							.Content()
							[
								SNew(SImage)
								.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
							]
						]
					];
			}
		}
	}

private:
	void OnCollectionViewModelChanged()
	{
		OnRebuildChildren.ExecuteIfBound();
	}

	void OnParameterPreChange(FNiagaraVariable Var)
	{
		Component->Modify();
	}

	void OnDataInterfacePreChange(FNiagaraVariable Var)
	{
		Component->Modify();
	}

	void OnParameterChanged(FNiagaraVariable Var)
	{
		Component->GetInitialParameters().OnParameterChange();
		Component->SetParameterValueOverriddenLocally(Var.GetName(), true);
	}

	void OnDataInterfaceChanged(FNiagaraVariable Var)
	{
		Component->GetInitialParameters().OnInterfaceChange();
		Component->SetParameterValueOverriddenLocally(Var.GetName(), true);
	}

	bool DoesParameterDifferFromDefault(const FNiagaraVariable& Var)
	{
		return Component->IsParameterValueOverriddenLocally(Var.GetName());
	}

	FReply OnLocationResetClicked(FNiagaraVariable Parameter)
	{
		FScopedTransaction ScopedTransaction(LOCTEXT("ResetParameterValue", "Reset parameter value to system defaults."));
		Component->Modify();
		Component->SetParameterValueOverriddenLocally(Parameter.GetName(), false);
		return FReply::Handled();
	}

	EVisibility GetLocationResetVisibility(FNiagaraVariable Parameter) const
	{
		return Component->IsParameterValueOverriddenLocally(Parameter.GetName()) ? EVisibility::Visible : EVisibility::Collapsed;
	}

private:
	UNiagaraComponent* Component;
	FSimpleDelegate OnRebuildChildren;
	TArray<TSharedRef<FStructOnScope>> CreatedStructOnScopes;
	TArray<UNiagaraScript*> OriginalScripts;
};

TSharedRef<IDetailCustomization> FNiagaraComponentDetails::MakeInstance()
{
	return MakeShareable(new FNiagaraComponentDetails);
}

FNiagaraComponentDetails::FNiagaraComponentDetails() : Builder(nullptr), bQueueForDetailsRefresh(false)
{
	GEditor->RegisterForUndo(this);
}


FNiagaraComponentDetails::~FNiagaraComponentDetails()
{
	GEditor->UnregisterForUndo(this);

	if (GEngine)
	{
		GEngine->OnWorldDestroyed().RemoveAll(this);
	}

	FGameDelegates::Get().GetEndPlayMapDelegate().RemoveAll(this);

	if (ObjectsCustomized.Num() == 1)
	{
		UNiagaraComponent* Component = Cast<UNiagaraComponent>(ObjectsCustomized[0].Get());

		if (Component)
		{
			if (FNiagaraSystemInstance* SystemInstance = Component->GetSystemInstance())
			{
				if (OnResetDelegateHandle.IsValid())
				{
					SystemInstance->OnReset().Remove(OnResetDelegateHandle);
				}

				if (OnInitDelegateHandle.IsValid())
				{
					SystemInstance->OnInitialized().Remove(OnInitDelegateHandle);
				}

				SystemInstance->OnDestroyed().RemoveAll(this);
			}
		}
	}
}

bool FNiagaraComponentDetails::IsTickable() const
{
	return bQueueForDetailsRefresh;
}

void FNiagaraComponentDetails::Tick(float DeltaTime)
{
	if (bQueueForDetailsRefresh)
	{
		if (Builder)
		{
			Builder->ForceRefreshDetails();
			bQueueForDetailsRefresh = false;
		}
	}
}

void FNiagaraComponentDetails::PostUndo(bool bSuccess)
{
	// We may have queued up as a result of an Undo of adding the System itself. The objects we were 
	// referencing may therefore have been removed. If so, we'll have to take a different path later on in the code.
	int32 NumValid = 0;
	for (TWeakObjectPtr<UObject> WeakPtr : ObjectsCustomized)
	{
		if (!WeakPtr.IsStale(true) && WeakPtr.IsValid())
		{
			NumValid++;
		}
	}

	if (Builder)
	{
		// This is tricky. There is a high chance that if the component changed, then
		// any cached FNiagaraVariable that we're holding on to may have been changed out from underneath us.
		// So we essentially must tear down and start again in the UI.
		// HOWEVER, a refresh will invoke a new constructor of the FNiagaraComponentDetails, which puts us 
		// in the queue to handle PostUndo events. This turns quickly into an infinitely loop. Therefore,
		// we circumvent this by telling the editor that we need to queue up an event
		// that we will handle in the subsequent frame's Tick event. Not the cleanest approach, 
		// but because we are doing things like CopyOnWrite, the normal Unreal property editing
		// path is not available to us.
		if (NumValid != 0)
		{
			bQueueForDetailsRefresh = true;
		}
		else
		{
			// If we no longer have any valid pointers, but previously had a builder, that means 
			// that the builder is probably dead or dying soon. We shouldn't trust it any more and
			// we should make sure that we aren't queueing for ticks to refresh either.
			Builder = nullptr;
			bQueueForDetailsRefresh = false;
		}

#if 0
		for (TSharedPtr<FNiagaraParameterViewModelCustomDetails>& ViewModel : ViewModels)
		{
			ViewModel->Reset();
		}
#endif
	}
}

TStatId FNiagaraComponentDetails::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FNiagaraComponentDetails, STATGROUP_Tickables);
}

void FNiagaraComponentDetails::OnPiEEnd()
{
	UE_LOG(LogNiagaraEditor, Log, TEXT("onPieEnd"));
	if (ObjectsCustomized.Num() == 1)
	{
		UNiagaraComponent* Component = Cast<UNiagaraComponent>(ObjectsCustomized[0].Get());

		if (Component)
		{
			if (Component->GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor))
			{
				UE_LOG(LogNiagaraEditor, Log, TEXT("onPieEnd - has package flags"));
				UWorld* TheWorld = UWorld::FindWorldInPackage(Component->GetOutermost());
				if (TheWorld)
				{
					OnWorldDestroyed(TheWorld);
				}
			}
		}
	}
}

void FNiagaraComponentDetails::OnWorldDestroyed(class UWorld* InWorld)
{
	// We have to clear out any temp data interfaces that were bound to the component's package when the world goes away or otherwise
	// we'll report GC leaks..
	if (ObjectsCustomized.Num() == 1)
	{
		UNiagaraComponent* Component = Cast<UNiagaraComponent>(ObjectsCustomized[0].Get());

		if (Component)
		{
			if (Component->GetWorld() == InWorld)
			{
				UE_LOG(LogNiagaraEditor, Log, TEXT("OnWorldDestroyed - matched up"));
				Builder = nullptr;
			}
		}
	}
}

void FNiagaraComponentDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
 	Builder = &DetailBuilder;
// 
// 	// Create a category so this is displayed early in the properties
// 	IDetailCategoryBuilder& NiagaraCategory = DetailBuilder.EditCategory("Niagara", FText::GetEmpty(), ECategoryPriority::Important);
// 
// 	// Store off the properties for analysis in later function calls.
	static const FName ParamCategoryName = TEXT("NiagaraComponent_Parameters");
	static const FName ParamCollectionCategoryName = TEXT("NiagaraComponent_ParameterCollectionOverrides");
	static const FName ScriptCategoryName = TEXT("Parameters");

	DetailBuilder.EditCategory(ScriptCategoryName);

 	TSharedPtr<IPropertyHandle> LocalOverridesPropertyHandle = DetailBuilder.GetProperty(TEXT("Parameters"));
 	if (LocalOverridesPropertyHandle.IsValid())
 	{
 		LocalOverridesPropertyHandle->MarkHiddenByCustomization();
		UProperty* Property = LocalOverridesPropertyHandle->GetProperty();
 	}

	DetailBuilder.GetObjectsBeingCustomized(ObjectsCustomized);

	if (ObjectsCustomized.Num() == 1)
	{
		UNiagaraComponent* Component = Cast<UNiagaraComponent>(ObjectsCustomized[0].Get());

		if (Component == nullptr)
		{
			return;
		}

		if (GEngine)
		{
			GEngine->OnWorldDestroyed().AddRaw(this, &FNiagaraComponentDetails::OnWorldDestroyed);
		}

		FGameDelegates::Get().GetEndPlayMapDelegate().AddRaw(this, &FNiagaraComponentDetails::OnPiEEnd);

		FNiagaraSystemInstance* SystemInstance = Component->GetSystemInstance();
		if (SystemInstance == nullptr)
		{
			return;
		}

		// We'll want to monitor for any serious changes that might require a rebuild of the UI from scratch.
		UE_LOG(LogNiagaraEditor, Log, TEXT("Registering with System instance %p"), SystemInstance);
		SystemInstance->OnReset().RemoveAll(this);
		SystemInstance->OnInitialized().RemoveAll(this);
		SystemInstance->OnDestroyed().RemoveAll(this);
		OnResetDelegateHandle = SystemInstance->OnReset().AddSP(this, &FNiagaraComponentDetails::OnSystemInstanceReset);
		OnInitDelegateHandle = SystemInstance->OnInitialized().AddSP(this, &FNiagaraComponentDetails::OnSystemInstanceReset);
		SystemInstance->OnDestroyed().AddSP(this, &FNiagaraComponentDetails::OnSystemInstanceDestroyed);
		FNiagaraParameterStore* ParamStore = &Component->GetInitialParameters();
		UNiagaraScript* ScriptSpawn = Component->GetAsset()->GetSystemSpawnScript();
		UNiagaraScript* ScriptUpdate = Component->GetAsset()->GetSystemUpdateScript();
		
		IDetailCategoryBuilder& InputParamCategory = DetailBuilder.EditCategory(ParamCategoryName, LOCTEXT("ParamCategoryName", "Parameters"));
		//InputParamCategory.HeaderContent(SNew(SAddParameterButton, InputCollectionViewModel));
		InputParamCategory.AddCustomBuilder(MakeShared<FNiagaraComponentNodeBuilder>(Component, ScriptSpawn, ScriptUpdate));
	}

}

void FNiagaraComponentDetails::OnSystemInstanceReset()
{
	UE_LOG(LogNiagaraEditor, Log, TEXT("OnSystemInstanceReset()"));
	bQueueForDetailsRefresh = true;
}

void FNiagaraComponentDetails::OnSystemInstanceDestroyed()
{
	UE_LOG(LogNiagaraEditor, Log, TEXT("OnSystemInstanceDestroyed()"));
	bQueueForDetailsRefresh = true;
}

FReply FNiagaraComponentDetails::OnSystemOpenRequested(UNiagaraSystem* InSystem)
{
	if (InSystem)
	{
		FAssetEditorManager::Get().OpenEditorForAsset(InSystem);
	}
	return FReply::Handled();
}



#undef LOCTEXT_NAMESPACE
