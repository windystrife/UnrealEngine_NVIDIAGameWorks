// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraParameterStore.h"
#include "NiagaraCommon.h"
#include "NiagaraDataSet.h"
#include "NiagaraComponent.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraParameterCollection.h"


//////////////////////////////////////////////////////////////////////////

FNiagaraParameterStore::FNiagaraParameterStore()
	: Owner(nullptr)
	, bParametersDirty(true)
	, bInterfacesDirty(true)
{
}

FNiagaraParameterStore::FNiagaraParameterStore(UObject* InOwner)
	: Owner(InOwner)
	, bParametersDirty(true)
	, bInterfacesDirty(true)
	, bLayoutDirty(true)
{

}

FNiagaraParameterStore::FNiagaraParameterStore(const FNiagaraParameterStore& Other)
{
	*this = Other;
}

FNiagaraParameterStore& FNiagaraParameterStore::operator=(const FNiagaraParameterStore& Other)
{
	Owner = Other.Owner;
	ParameterOffsets = Other.ParameterOffsets;
	ParameterData = Other.ParameterData;
	DataInterfaces = Other.DataInterfaces;
	//Don't copy bindings. We just want the data.
	return *this;
}

FNiagaraParameterStore::~FNiagaraParameterStore()
{
}

void FNiagaraParameterStore::Bind(FNiagaraParameterStore* DestStore)
{
	check(DestStore);
	FNiagaraParameterStoreBinding& Binding = Bindings.Add(DestStore);	

	Binding.Empty();
	for (auto& VarInfo : DestStore->ParameterOffsets)
	{
		Binding.BindParameter(*DestStore, VarInfo.Value, *this, VarInfo.Key);
	}

	//Tick once immediately to ensure initial value is correct.
	Binding.Tick(*this, *DestStore, true);
}

void FNiagaraParameterStore::Unbind(FNiagaraParameterStore* DestStore)
{
	Bindings.Remove(DestStore);
}

void FNiagaraParameterStore::Rebind()
{
	for (TPair<FNiagaraParameterStore*, FNiagaraParameterStoreBinding>& Binding : Bindings)
	{
		Binding.Value.Empty();
		for (auto& VarInfo : Binding.Key->ParameterOffsets)
		{
			Binding.Value.BindParameter(*Binding.Key, VarInfo.Value, *this, VarInfo.Key);
		}

		//Tick once immediately to ensure initial value is correct.
		//TODO: Can probably rework binding so that we don't need to force all to update on a rebind.
		Binding.Value.Tick(*this, *Binding.Key, true);
	}
}

void FNiagaraParameterStore::TransferBindings(FNiagaraParameterStore& OtherStore)
{
	for (TPair<FNiagaraParameterStore*, FNiagaraParameterStoreBinding>& Binding : Bindings)
	{
		OtherStore.Bind(Binding.Key);
	}
	Bindings.Empty();
}

void FNiagaraParameterStore::Tick()
{
	if (bLayoutDirty)
	{
		Rebind();
	}
	else
	{
		for (auto& Binding : Bindings)
		{
			Binding.Value.Tick(*this, *Binding.Key);
		}
	}

	//We have to have ticked all our source stores before now.
	bParametersDirty = false;
	bInterfacesDirty = false;
	bLayoutDirty = false;
}

/**
Adds the passed parameter to this store.
Does nothing if this parameter is already present.
Returns true if we added a new parameter.
*/
bool FNiagaraParameterStore::AddParameter(const FNiagaraVariable& Param, bool bInitInterfaces)
{
	if (int32* Existing = ParameterOffsets.Find(Param))
	{
		return false;
	}

	if (Param.GetType().IsDataInterface())
	{
		int32 Offset = DataInterfaces.AddZeroed();
		ParameterOffsets.Add(Param) = Offset;
		DataInterfaces[Offset] = bInitInterfaces ? NewObject<UNiagaraDataInterface>(Owner, const_cast<UClass*>(Param.GetType().GetClass()), NAME_None, RF_Transactional) : nullptr;
	}
	else
	{
		int32 ParamSize = Param.GetSizeInBytes();
		int32 ParamAlignment = Param.GetAlignment();
		//int32 Offset = AlignArbitrary(ParameterData.Num(), ParamAlignment);//TODO: We need to handle alignment better here. Need to both satisfy CPU and GPU alignment concerns. VM doesn't care but the VM complier needs to be aware. Probably best to have everything adhere to GPU alignment rules.
		int32 Offset = ParameterData.Num();
		ParameterOffsets.Add(Param) = Offset;
		ParameterData.AddUninitialized(ParamSize);

		//Temporary to init param data from FNiagaraVariable storage. This will be removed when we change the UNiagaraScript to use a parameter store too.
		if (Param.IsDataAllocated())
		{
			FMemory::Memcpy(GetParameterData_Internal(Offset), Param.GetData(), ParamSize);
		}
	}

	OnLayoutChange();
	return true;
}

bool FNiagaraParameterStore::RemoveParameter(const FNiagaraVariable& ToRemove)
{
	if (int32* ExistingIndex = ParameterOffsets.Find(ToRemove))
	{
		//TODO: Ensure direct bindings are either updated or disallowed here.
		//We have to regenerate the store and the offsets on removal. This shouldn't happen at runtime!
		TMap<FNiagaraVariable, int32> NewOffsets;
		TArray<uint8> NewData;
		TArray<UNiagaraDataInterface*> NewInterfaces;
		for (TPair<FNiagaraVariable, int32>& Existing : ParameterOffsets)
		{
			FNiagaraVariable& ExistingVar = Existing.Key;
			int32& ExistingOffset = Existing.Value;

			//Add all but the one to remove to our
			if (ExistingVar != ToRemove)
			{
				if (ExistingVar.GetType().IsDataInterface())
				{
					int32 Offset = NewInterfaces.AddZeroed();
					NewOffsets.Add(ExistingVar) = Offset;
					NewInterfaces[Offset] = DataInterfaces[ExistingOffset];
				}
				else
				{
					int32 Offset = NewData.Num();
					int32 ParamSize = ExistingVar.GetSizeInBytes();
					NewOffsets.Add(ExistingVar) = Offset;
					NewData.AddUninitialized(ParamSize);
					FMemory::Memcpy(NewData.GetData() + Offset, ParameterData.GetData() + ExistingOffset, ParamSize);
				}
			}
		}

		ParameterOffsets = NewOffsets;
		ParameterData = NewData;
		DataInterfaces = NewInterfaces;

		OnLayoutChange();
		return true;
	}

	return false;
}

void FNiagaraParameterStore::RenameParameter(const FNiagaraVariable& Param, FName NewName)
{
	int32 Idx = IndexOf(Param);
	if(Idx != INDEX_NONE)
	{
		FNiagaraVariable NewParam = Param;
		NewParam.SetName(NewName);
		AddParameter(NewParam);

		int32 NewIdx = IndexOf(NewParam);
		if (Param.IsDataInterface())
		{
			SetDataInterface(GetDataInterface(Idx), NewIdx);
		}
		else
		{
			SetParameterData(GetParameterData_Internal(Idx), NewIdx, Param.GetSizeInBytes());
		}
		RemoveParameter(Param);

		OnLayoutChange();
	}
}

void FNiagaraParameterStore::Empty(bool bClearBindings)
{
	ParameterOffsets.Empty();
	ParameterData.Empty();
	DataInterfaces.Empty();
	if (bClearBindings)
	{
		Bindings.Empty();
	}
	bParametersDirty = true;
	bInterfacesDirty = true;
	bLayoutDirty = true;
}

const FNiagaraVariable* FNiagaraParameterStore::FindVariable(UNiagaraDataInterface* Interface)const
{
	int32 Idx = DataInterfaces.IndexOfByKey(Interface);
	if (Idx != INDEX_NONE)
	{
		for (const TPair<FNiagaraVariable, int32>& Existing : ParameterOffsets)
		{
			const FNiagaraVariable& ExistingVar = Existing.Key;
			const int32& ExistingOffset = Existing.Value;

			if (ExistingOffset == Idx && ExistingVar.GetType().GetClass() == Interface->GetClass())
			{
				return &ExistingVar;
			}
		}
	}
	return nullptr;
}

//////////////////////////////////////////////////////////////////////////