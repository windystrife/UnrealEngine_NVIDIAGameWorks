// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraParameterCollectionAssetViewModel.h"
#include "NiagaraParameterCollection.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraCollectionParameterViewModel.h"

#include "Editor.h"

#include "ScopedTransaction.h"
#include "EditorSupportDelegates.h"
#include "ModuleManager.h"
#include "TNiagaraViewModelManager.h"
#include "Math/NumericLimits.h"

#include "NiagaraDataInterface.h"

#include "NiagaraNodeParameterCollection.h"

#define LOCTEXT_NAMESPACE "NiagaraScriptInputCollection"

template<> TMap<UNiagaraParameterCollection*, TArray<FNiagaraParameterCollectionAssetViewModel*>> TNiagaraViewModelManager<UNiagaraParameterCollection, FNiagaraParameterCollectionAssetViewModel>::ObjectsToViewModels{};

FNiagaraParameterCollectionAssetViewModel::FNiagaraParameterCollectionAssetViewModel(UNiagaraParameterCollection* InCollection, FText InDisplayName, ENiagaraParameterEditMode InParameterEditMode)
	: FNiagaraParameterCollectionViewModel(InParameterEditMode)
	, Collection(InCollection)
	, Instance(InCollection->GetDefaultInstance())
	, DisplayName(InDisplayName)
{
	check(Collection && Instance);

	RegisteredHandle = RegisterViewModelWithMap(Collection, this);
	GEditor->RegisterForUndo(this);

	RefreshParameterViewModels();
}

FNiagaraParameterCollectionAssetViewModel::FNiagaraParameterCollectionAssetViewModel(UNiagaraParameterCollectionInstance* InInstance, FText InDisplayName, ENiagaraParameterEditMode InParameterEditMode)
	: FNiagaraParameterCollectionViewModel(InParameterEditMode)
	, Collection(InInstance->GetParent())
	, Instance(InInstance)
	, DisplayName(InDisplayName)
{
	check(Instance);

	RegisteredHandle = RegisterViewModelWithMap(Collection, this);
	GEditor->UnregisterForUndo(this);

	RefreshParameterViewModels();
}

FNiagaraParameterCollectionAssetViewModel::~FNiagaraParameterCollectionAssetViewModel()
{
	for (int32 i = 0; i < ParameterViewModels.Num(); i++)
	{
		FNiagaraCollectionParameterViewModel* ParameterViewModel = (FNiagaraCollectionParameterViewModel*)(&ParameterViewModels[i].Get());
		if (ParameterViewModel != nullptr)
		{
			ParameterViewModel->OnNameChanged().RemoveAll(this);
			ParameterViewModel->OnTypeChanged().RemoveAll(this);
			ParameterViewModel->OnDefaultValueChanged().RemoveAll(this);
		}
	}
	ParameterViewModels.Empty();

	GEditor->UnregisterForUndo(this);
	UnregisterViewModelWithMap(RegisteredHandle);
}

void FNiagaraParameterCollectionAssetViewModel::NotifyPreChange(UProperty* PropertyAboutToChange)
{
	if (PropertyAboutToChange->GetFName() == GET_MEMBER_NAME_CHECKED(UNiagaraParameterCollectionInstance, Collection))
	{
		GEditor->BeginTransaction(LOCTEXT("ChangeNPCInstanceParent", "Change Parent"));
		Instance->Empty();
	}
}

void FNiagaraParameterCollectionAssetViewModel::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, UProperty* PropertyThatChanged)
{
	if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UNiagaraParameterCollectionInstance, Collection))
	{
		//Instance has changed parent;
		check(!Instance->IsDefaultInstance());
		Collection = Instance->GetParent();
		RefreshParameterViewModels();
		GEditor->EndTransaction();
	}
}

FText FNiagaraParameterCollectionAssetViewModel::GetDisplayName() const
{
	return DisplayName;
}

FName FNiagaraParameterCollectionAssetViewModel::GenerateNewName(FNiagaraTypeDefinition Type)const
{
	check(Collection && Instance);

	FName ProposedName = *(TEXT("New") + Type.GetName());
	TSet<FName> Existing;
	Existing.Reserve(ParameterViewModels.Num());
	for (TSharedRef<INiagaraParameterViewModel> ParameterViewModel : ParameterViewModels)
	{
		Existing.Add(ParameterViewModel->GetName());
	}

	return *Collection->ParameterNameFromFriendlyName(FNiagaraEditorUtilities::GetUniqueName(ProposedName, Existing).ToString());
}

void FNiagaraParameterCollectionAssetViewModel::AddParameter(TSharedPtr<FNiagaraTypeDefinition> ParameterType)
{
	check(Collection && Instance);

	FScopedTransaction ScopedTransaction(LOCTEXT("AddNPCParameter", "Add Parameter"));
	Collection->Modify();

	FNiagaraTypeDefinition Type = *ParameterType.Get();
	FName NewName = GenerateNewName(Type);

	int32 ParamIdx = Collection->AddParameter(NewName, Type);

	//TODO: It'd be nice to be able to get a default value for types in runtime code and do this inside the parameter store itself.
	if (!Type.IsDataInterface())
	{
		TArray<uint8> DefaultData;
		FNiagaraEditorUtilities::GetTypeDefaultValue(Type, DefaultData);
		Instance->GetParameterStore().SetParameterData(DefaultData.GetData(), Collection->GetParameters()[ParamIdx]);
	}

	CollectionChanged(false);
	RefreshParameterViewModels();

	for (TSharedRef<INiagaraParameterViewModel> ParameterViewModel : ParameterViewModels)
	{
		if (ParameterViewModel->GetName() == NewName)
		{
			GetSelection().SetSelectedObject(ParameterViewModel);
			break;
		}
	}	
}

void FNiagaraParameterCollectionAssetViewModel::UpdateOpenInstances()
{
	TArray<TSharedPtr<FNiagaraParameterCollectionAssetViewModel>> ViewModels;
	if (FNiagaraParameterCollectionAssetViewModel::GetAllViewModelsForObject(Collection, ViewModels))
	{
		for (TSharedPtr<FNiagaraParameterCollectionAssetViewModel> ViewModel : ViewModels)
		{
			if (ViewModel.Get() != this)
			{
				//This is not sufficient. 
				//If we rename a parameter for example, we'll lose any overrides it had. Improve this.
				ViewModel->RefreshParameterViewModels();
			}
		}
	}
}

void FNiagaraParameterCollectionAssetViewModel::RemoveParameter(FNiagaraVariable& Parameter)
{
	FScopedTransaction ScopedTransaction(LOCTEXT("RemoveNPCParameter", "Remove Parameter"));
	Collection->RemoveParameter(Parameter);

	CollectionChanged(true);

	RefreshParameterViewModels();
}

void FNiagaraParameterCollectionAssetViewModel::PostUndo(bool bSuccess)
{
	Collection = Instance->GetParent();
	CollectionChanged(true);

	RefreshParameterViewModels();
}

void FNiagaraParameterCollectionAssetViewModel::DeleteSelectedParameters()
{
	check(Collection && Instance);
	if (GetSelection().GetSelectedObjects().Num() > 0)
	{
		TSet<FNiagaraVariable> VarsToDelete;
		for (TSharedRef<INiagaraParameterViewModel> Parameter : GetSelection().GetSelectedObjects())
		{
			FNiagaraVariable ToDelete(*Parameter->GetType().Get(), *Collection->ParameterNameFromFriendlyName(Parameter->GetName().ToString()));
			VarsToDelete.Add(ToDelete);
		}
		GetSelection().ClearSelectedObjects();

		FScopedTransaction ScopedTransaction(LOCTEXT("DeleteNPCParameter", "Delete Parameter"));
		for (FNiagaraVariable ParamToDelete : VarsToDelete)
		{
			Collection->RemoveParameter(ParamToDelete);
		}

		CollectionChanged(true);
		RefreshParameterViewModels();
	}
}

const TArray<TSharedRef<INiagaraParameterViewModel>>& FNiagaraParameterCollectionAssetViewModel::GetParameters()
{
	return ParameterViewModels;
}

EVisibility FNiagaraParameterCollectionAssetViewModel::GetAddButtonVisibility()const
{
	return Instance->IsDefaultInstance() ? EVisibility::Visible : EVisibility::Hidden;
}

void FNiagaraParameterCollectionAssetViewModel::CollectionChanged(bool bRecompile)
{
	for (TObjectIterator<UNiagaraParameterCollectionInstance> It; It; ++It)
	{
		if (It->Collection == Collection)
		{
			It->SyncWithCollection();
		}
	}

	//Refresh any existing view models that might be showing changed instances.
	UpdateOpenInstances();

	//Refresh any nodes that are referencing this collection.
	for (TObjectIterator<UNiagaraNodeParameterCollection> It; It; ++It)
	{
		if (It->GetReferencedAsset() == Collection)
		{
			It->RefreshFromExternalChanges();
		}
	}

	if (bRecompile)
	{
		//Update any active systems that are using this collection.
		FNiagaraSystemUpdateContext Context(Collection, true);
	}
}

void FNiagaraParameterCollectionAssetViewModel::RefreshParameterViewModels()
{
	if (!Collection)
	{
		return;
	}

	for (int32 i = 0; i < ParameterViewModels.Num(); i++)
	{
		FNiagaraCollectionParameterViewModel* ParameterViewModel = (FNiagaraCollectionParameterViewModel*)(&ParameterViewModels[i].Get());
		if (ParameterViewModel != nullptr)
		{
			ParameterViewModel->Reset();
		}
	}

	ParameterViewModels.Empty();

	for (int32 i = 0; i < Collection->GetParameters().Num(); ++i)
	{
		FNiagaraVariable& Var = Collection->GetParameters()[i];

		TSharedPtr<FNiagaraCollectionParameterViewModel> ParameterViewModel = MakeShareable(new FNiagaraCollectionParameterViewModel(Var, Instance, ParameterEditMode));

		ParameterViewModel->OnNameChanged().AddRaw(this, &FNiagaraParameterCollectionAssetViewModel::OnParameterNameChanged, Var);
		ParameterViewModel->OnTypeChanged().AddRaw(this, &FNiagaraParameterCollectionAssetViewModel::OnParameterTypeChanged, Var);
		ParameterViewModel->OnDefaultValueChanged().AddRaw(this, &FNiagaraParameterCollectionAssetViewModel::OnParameterValueChangedInternal, ParameterViewModel.ToSharedRef());
		ParameterViewModel->OnProvidedChanged().AddRaw(this, &FNiagaraParameterCollectionAssetViewModel::OnParameterProvidedChanged, Var);
		ParameterViewModels.Add(ParameterViewModel.ToSharedRef());
	}

	OnCollectionChangedDelegate.Broadcast();
}

bool FNiagaraParameterCollectionAssetViewModel::SupportsType(const FNiagaraTypeDefinition& Type) const
{
	return Type != FNiagaraTypeDefinition::GetGenericNumericDef();
}

void FNiagaraParameterCollectionAssetViewModel::OnParameterNameChanged(FName OldName, FName NewName, FNiagaraVariable ParameterVariable)
{
	//How can we update any other open instances here?

	int32 Index = Collection->IndexOfParameter(ParameterVariable);
	check(Index != INDEX_NONE);

	FName ParamName = *Collection->ParameterNameFromFriendlyName(GetParameters()[Index]->GetName().ToString());
	Collection->GetParameters()[Index].SetName(ParamName);
	Instance->RenameParameter(ParameterVariable, ParamName);
	CollectionChanged(false);

	RefreshParameterViewModels();
}

void FNiagaraParameterCollectionAssetViewModel::SetAllParametersEditingEnabled(bool bInEnabled)
{
	for (TSharedRef<INiagaraParameterViewModel>& ParameterViewModel : ParameterViewModels)
	{
		ParameterViewModel->SetEditingEnabled(bInEnabled);
	}
}

void FNiagaraParameterCollectionAssetViewModel::SetAllParametersTooltipOverrides(const FText& Override)
{
	for (TSharedRef<INiagaraParameterViewModel>& ParameterViewModel : ParameterViewModels)
	{
		ParameterViewModel->SetTooltipOverride(Override);
	}
}

TSharedPtr<INiagaraParameterViewModel> FNiagaraParameterCollectionAssetViewModel::GetParameterViewModel(const FName& Name)
{
	for (TSharedRef<INiagaraParameterViewModel>& ParameterViewModel : ParameterViewModels)
	{
		if (ParameterViewModel->GetName() == Name)
		{
			return TSharedPtr<INiagaraParameterViewModel>(ParameterViewModel);
		}
	}
	return TSharedPtr<INiagaraParameterViewModel>();
}

void FNiagaraParameterCollectionAssetViewModel::OnParameterTypeChanged(FNiagaraVariable ParameterVariable)
{
	int32 Index = Collection->IndexOfParameter(ParameterVariable);
	check(Index != INDEX_NONE);

	Collection->Modify();

	Collection->GetDefaultInstance()->RemoveParameter(ParameterVariable);

	FNiagaraTypeDefinition Type = *ParameterViewModels[Index]->GetType();
	Collection->GetParameters()[Index].SetType(Type);

	Collection->GetDefaultInstance()->AddParameter(Collection->GetParameters()[Index]);
	
	//TODO: It'd be nice to be able to get a default value for types in runtime code and do this inside the parameter store itself.
	if (!Type.IsDataInterface())
	{
		TArray<uint8> DefaultData;
		FNiagaraEditorUtilities::GetTypeDefaultValue(Type, DefaultData);
		Collection->GetDefaultInstance()->GetParameterStore().SetParameterData(DefaultData.GetData(), Collection->GetParameters()[Index]);
	}

	CollectionChanged(true);

	RefreshParameterViewModels();
}

void FNiagaraParameterCollectionAssetViewModel::OnParameterProvidedChanged(FNiagaraVariable ParameterVariable)
{
	//int32 Index = Collection->IndexOfParameter(ParameterVariable);
	//check(Index != INDEX_NONE);

	RefreshParameterViewModels();
	//CollectionChanged(false);
}

void FNiagaraParameterCollectionAssetViewModel::OnParameterValueChangedInternal(TSharedRef<FNiagaraCollectionParameterViewModel> ChangedParameter)
{
	OnParameterValueChanged().Broadcast(ChangedParameter->GetName());
}

#undef LOCTEXT_NAMESPACE // "NiagaraScriptInputCollectionViewModel"
