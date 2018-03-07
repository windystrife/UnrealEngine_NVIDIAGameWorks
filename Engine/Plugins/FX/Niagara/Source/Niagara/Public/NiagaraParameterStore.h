// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraCommon.h"
#include "NiagaraDataInterface.h"
#include "NiagaraParameterStore.generated.h"

struct FNiagaraParameterStore;

//Binding from one parameter store to another.
//This does no tracking of lifetimes etc so the owner must ensure safe use and rebinding when needed etc.
struct FNiagaraParameterStoreBinding
{
	FNiagaraParameterStoreBinding()
	{}

	typedef TTuple<int32, int32, int32> TParameterBinding;
	/** Bindings of parameter data. Src offset, Dest offset and Size. */
	TArray<TParameterBinding> ParmeterBindings;

	typedef TTuple<int32, int32> TInterfaceBinding;
	/** Bindings of data interfaces. Src and Dest offsets.*/
	TArray<TInterfaceBinding> InterfaceBindings;

	FORCEINLINE void Empty()
	{
		ParmeterBindings.Empty();
		InterfaceBindings.Empty();
	}

	FORCEINLINE_DEBUGGABLE void BindParameter(FNiagaraParameterStore& DestStore, int32 DestOffset, FNiagaraParameterStore& SrcStore, FNiagaraVariable& Parameter);
	
	/** TODO: Merge contiguous ranges into a single binding. */
	//FORCEINLINE_DEBUGGABLE void Optimize();

	FORCEINLINE_DEBUGGABLE void Tick(FNiagaraParameterStore& SrcStore, FNiagaraParameterStore& DestStore, bool bForce = false);
};

/** Base storage class for Niagara parameter values. */
USTRUCT()
struct NIAGARA_API FNiagaraParameterStore
{
	GENERATED_USTRUCT_BODY()

	/** Owner of this store. Used to provide an outer to data interfaces in this store. */
	UPROPERTY()
	UObject* Owner;
	
	/** Map from parameter defs to their offset in the data table or the data interface. TODO: Separate out into a layout and instance class to reduce duplicated data for this?  */
	UPROPERTY()
	TMap<FNiagaraVariable, int32> ParameterOffsets;
	
	/** Buffer containing parameter data. Indexed using offsets in ParameterOffsets */
	UPROPERTY()
	TArray<uint8> ParameterData;
	
	/** Data interfaces for this script. Possibly overridden with externally owned interfaces. Also indexed by ParameterOffsets. */
	UPROPERTY()
	TArray<UNiagaraDataInterface*> DataInterfaces;

	/** Bindings between this parameter store and others we push data into when we tick. */
	TMap<FNiagaraParameterStore*, FNiagaraParameterStoreBinding> Bindings;

	/** Marks our parameters as dirty. They will be pushed to any bound stores on tick if true. */
	uint32 bParametersDirty : 1;
	/** Marks our interfaces as dirty. They will be pushed to any bound stores on tick if true. */
	uint32 bInterfacesDirty : 1;
	/** Marks our layout as dirty. All bindings must be recreated and all parameters pushed again. */
	uint32 bLayoutDirty : 1;

	FNiagaraParameterStore();
	FNiagaraParameterStore(UObject* InOwner);
	FNiagaraParameterStore(const FNiagaraParameterStore& Other);
	FNiagaraParameterStore& operator=(const FNiagaraParameterStore& Other);

	~FNiagaraParameterStore();

	/** Binds this parameter store to another. During Tick the values of this store will be pushed into all bound stores */
	void Bind(FNiagaraParameterStore* DestStore);
	/** Unbinds this store form one it's bound to. */
	void Unbind(FNiagaraParameterStore* DestStore);
	/** Recreates any bindings to reflect a layout change etc. */
	void Rebind();
	/** Recreates any bindings to reflect a layout change etc. */
	void TransferBindings(FNiagaraParameterStore& OtherStore);
	/** Handles any update such as pushing parameters to bound stores etc. */
	void Tick();

	/**
	Adds the passed parameter to this store.
	Does nothing if this parameter is already present.
	Returns true if we added a new parameter.
	*/
	bool AddParameter(const FNiagaraVariable& Param, bool bInitialize=true);

	/** Removes the passed parameter if it exists in the store. */
	bool RemoveParameter(const FNiagaraVariable& Param);

	/** Renames the passed parameter. */
	void RenameParameter(const FNiagaraVariable& Param, FName NewName);

	/** Removes all parameters from this store and releases any data. */
	void Empty(bool bClearBindings=true);

	FORCEINLINE void GetParameters(TArray<FNiagaraVariable>& OutParametres) const { return ParameterOffsets.GenerateKeyArray(OutParametres); }

	FORCEINLINE const TArray<UNiagaraDataInterface*>& GetDataInterfaces()const { return DataInterfaces; }
	FORCEINLINE const TArray<uint8>& GetParameterDataArray()const { return ParameterData; }
	FORCEINLINE TArray<uint8>& GetParameterDataArray() { return ParameterData; }

	/** Gets the index of the passed parameter. If it is a data interface, this is an offset into the data interface table, otherwise a byte offset into he parameter data buffer. */
	int32 IndexOf(const FNiagaraVariable& Parameter)const
	{
		const int32* Off = ParameterOffsets.Find(Parameter);
		if ( Off )
		{
			return *Off;
		}
		return INDEX_NONE;
	}

	/** Gets the typed parameter data. */
	template<typename T>
	FORCEINLINE_DEBUGGABLE void GetParameterValue(T& OutValue, const FNiagaraVariable& Parameter)const
	{
		check(Parameter.GetSizeInBytes() == sizeof(T));
		int32 Offset = IndexOf(Parameter);
		if (Offset != INDEX_NONE)
		{
			OutValue = *(const T*)(GetParameterData(Offset));
		}
	}

	template<typename T>
	FORCEINLINE_DEBUGGABLE T GetParameterValue(const FNiagaraVariable& Parameter)const
	{
		check(Parameter.GetSizeInBytes() == sizeof(T));
		int32 Offset = IndexOf(Parameter);
		if (Offset != INDEX_NONE)
		{
			return *(const T*)(GetParameterData(Offset));
		}
		return T();
	}

	FORCEINLINE const uint8* GetParameterData(int32 Offset)const
	{
		return ParameterData.GetData() + Offset;
	}

	/** Returns the parameter data for the passed parameter if it exists in this store. Null if not. */
	FORCEINLINE_DEBUGGABLE const uint8* GetParameterData(const FNiagaraVariable& Parameter)const
	{
		int32 Offset = IndexOf(Parameter);
		if (Offset != INDEX_NONE)
		{
			return GetParameterData(Offset);
		}
		return nullptr;
	}

	FORCEINLINE_DEBUGGABLE uint8* GetParameterData(const FNiagaraVariable& Parameter)
	{
		int32 Offset = IndexOf(Parameter);
		if (Offset != INDEX_NONE)
		{
			return GetParameterData_Internal(Offset);
		}
		return nullptr;
	}

	/** Returns the data interface at the passed offset. */
	FORCEINLINE void GetDataInterface(UNiagaraDataInterface* OutInterface, int32 Offset)const
	{
		OutInterface = GetDataInterface(Offset);
	}

	/** Returns the data interface at the passed offset. */
	FORCEINLINE UNiagaraDataInterface* GetDataInterface(int32 Offset)const
	{
		if (DataInterfaces.IsValidIndex(Offset))
		{
			return DataInterfaces[Offset];
		}

		return nullptr;
	}

	/** Returns the data interface for the passed parameter if it exists in this store. */
	FORCEINLINE_DEBUGGABLE UNiagaraDataInterface* GetDataInterface(const FNiagaraVariable& Parameter)const
	{
		int32 Offset = IndexOf(Parameter);
		UNiagaraDataInterface* Interface = GetDataInterface(Offset);
		checkSlow(!Interface || Parameter.GetType().GetClass() == Interface->GetClass());
		return Interface;
	}

	/** Returns the associated FNiagaraVariable for the passed data interface if it exists in the store. Null if not.*/
	const FNiagaraVariable* FindVariable(UNiagaraDataInterface* Interface)const;

	/** Copies the passed parameter from this parameter store into another. */
	FORCEINLINE_DEBUGGABLE void CopyParameterData(FNiagaraParameterStore& DestStore, const FNiagaraVariable& Parameter) const
	{
		int32 DestIndex = DestStore.IndexOf(Parameter);
		int32 SrcIndex = IndexOf(Parameter);
		if (DestIndex != INDEX_NONE && SrcIndex != INDEX_NONE)
		{
			DestStore.SetParameterData(GetParameterData_Internal(SrcIndex), DestIndex, Parameter.GetSizeInBytes());
		}
	}

	template<typename T>
	FORCEINLINE_DEBUGGABLE void SetParameterValue(const T& InValue, const FNiagaraVariable& Param, bool bAdd=false)
	{
		check(Param.GetSizeInBytes() == sizeof(T));
		int32 Offset = IndexOf(Param);
		if (Offset != INDEX_NONE)
		{
			//*(T*)(GetParameterData_Internal(Offset)) = InValue;
			//Until we solve our alignment issues, temporarily just doing a memcpy here.
			FMemory::Memcpy(GetParameterData_Internal(Offset), &InValue, sizeof(T));
			OnParameterChange();
		}
		else
		{
			if (bAdd)
			{
				AddParameter(Param);
				Offset = IndexOf(Param);
				check(Offset != INDEX_NONE);
				*(T*)(GetParameterData_Internal(Offset)) = InValue;
			}
		}
	}

	FORCEINLINE_DEBUGGABLE void SetParameterData(const uint8* Data, int32 Offset, int32 Size)
	{
		FMemory::Memcpy(GetParameterData_Internal(Offset), Data, Size);
		OnParameterChange();
	}

	FORCEINLINE_DEBUGGABLE void SetParameterData(const uint8* Data, const FNiagaraVariable& Param)
	{
		int32 Offset = IndexOf(Param);
		if (Offset != INDEX_NONE)
		{
			checkSlow(!Param.IsDataInterface());
			FMemory::Memcpy(GetParameterData_Internal(Offset), Data, Param.GetSizeInBytes());
			OnParameterChange();
		}
	}

	/**
	* Sets the parameter using the internally stored data in the passed FNiagaraVariable.
	* TODO: Remove this. IMO FNiagaraVariable should be just for data definition and all storage should be done via this class.
	*/
	FORCEINLINE_DEBUGGABLE void SetParameter(const FNiagaraVariable& Param)
	{
		checkSlow(Param.IsDataAllocated());
		int32 Offset = IndexOf(Param);
		if (Offset != INDEX_NONE)
		{
			FMemory::Memcpy(GetParameterData_Internal(Offset), Param.GetData(), Param.GetSizeInBytes());
			OnParameterChange();
		}
	}

	FORCEINLINE_DEBUGGABLE void SetDataInterface(UNiagaraDataInterface* InInterface, int32 Offset)
	{
		DataInterfaces[Offset] = InInterface;
		OnInterfaceChange();
	}

	FORCEINLINE_DEBUGGABLE void SetDataInterface(UNiagaraDataInterface* InInterface, const FNiagaraVariable& Parameter)
	{
		int32 Offset = IndexOf(Parameter);
		if (Offset != INDEX_NONE)
		{
			DataInterfaces[Offset] = InInterface;
			OnInterfaceChange();
		}
	}

	FORCEINLINE void OnParameterChange() { bParametersDirty = true; }
	FORCEINLINE void OnInterfaceChange() { bInterfacesDirty = true; }

protected:
	FORCEINLINE void OnLayoutChange() { bLayoutDirty = true; }

	/** Returns the parameter data at the passed offset. */
	FORCEINLINE const uint8* GetParameterData_Internal(int32 Offset) const
	{
		return ParameterData.GetData() + Offset;
	}

	FORCEINLINE uint8* GetParameterData_Internal(int32 Offset) 
	{
		return ParameterData.GetData() + Offset;
	}
};


FORCEINLINE_DEBUGGABLE void FNiagaraParameterStoreBinding::BindParameter(FNiagaraParameterStore& DestStore, int32 DestOffset, FNiagaraParameterStore& SrcStore, FNiagaraVariable& Parameter)
{
	int32 SrcOffset = SrcStore.IndexOf(Parameter);
	checkSlow(DestOffset == DestStore.IndexOf(Parameter));

	if (SrcOffset == INDEX_NONE || DestOffset == INDEX_NONE)
	{
		//UE_LOG(LogNiagara, Error, TEXT("Failed to bind parameter %s."), *Parameter.GetName().ToString());
		return;
	}

	if (Parameter.IsDataInterface())
	{
		InterfaceBindings.AddUnique(TInterfaceBinding(SrcOffset, DestOffset));
	}
	else
	{
		ParmeterBindings.AddUnique(TParameterBinding(SrcOffset, DestOffset, Parameter.GetSizeInBytes()));
	}
}

/** Merge contiguous ranges into a single binding. */
// FORCEINLINE_DEBUGGABLE void FNiagaraParameterStoreBinding::Optimize()
// {
// 	//TODO
// }

FORCEINLINE_DEBUGGABLE void FNiagaraParameterStoreBinding::Tick(FNiagaraParameterStore& SrcStore, FNiagaraParameterStore& DestStore, bool bForce)
{
	if (SrcStore.bParametersDirty || bForce)
	{
		DestStore.bParametersDirty = true;
		for (TParameterBinding& Binding : ParmeterBindings)
		{
			int32 SrcOffset = Binding.Get<0>();
			int32 DestOffset = Binding.Get<1>();
			int32 Size = Binding.Get<2>();
			DestStore.SetParameterData(SrcStore.GetParameterData(SrcOffset), DestOffset, Size);
		}
	}

	if (SrcStore.bInterfacesDirty || bForce)
	{
		DestStore.bInterfacesDirty = true;
		for (TInterfaceBinding& Binding : InterfaceBindings)
		{
			DestStore.SetDataInterface(SrcStore.GetDataInterface(Binding.Get<0>()), Binding.Get<1>());
		}
	}
}

//////////////////////////////////////////////////////////////////////////

/**
Direct binding to a parameter store to allow efficient gets/sets from code etc. 
Does no tracking of lifetimes etc so users are responsible for safety.
*/
template<typename T>
struct FNiagaraParameterDirectBinding
{
	T* ValuePtr;

	FNiagaraParameterDirectBinding()
		: ValuePtr(nullptr)
	{}

	T* Init(FNiagaraParameterStore& InStore, const FNiagaraVariable& DestVariable)
	{
		check(DestVariable.GetSizeInBytes() == sizeof(T));
		ValuePtr = (T*)InStore.GetParameterData(DestVariable);
		return ValuePtr;
	}

	FORCEINLINE void SetValue(const T& InValue)
	{
		if (ValuePtr)
		{
			*ValuePtr = InValue;
		}
	}

	FORCEINLINE T GetValue()const 
	{
		if (ValuePtr)
		{
			return *ValuePtr;
		}
		return T();
	}
};

template<>
struct FNiagaraParameterDirectBinding<FMatrix>
{
	FMatrix* ValuePtr;

	FNiagaraParameterDirectBinding():ValuePtr(nullptr){}

	FMatrix* Init(FNiagaraParameterStore& InStore, const FNiagaraVariable& DestVariable)
	{
		check(DestVariable.GetSizeInBytes() == sizeof(FMatrix));
		ValuePtr = (FMatrix*)InStore.GetParameterData(DestVariable);
		return ValuePtr;
	}

	FORCEINLINE void SetValue(const FMatrix& InValue)
	{
		if (ValuePtr)
		{
			FMemory::Memcpy(ValuePtr, &InValue, sizeof(FMatrix));//Temp annoyance until we fix the alignment issues with parameter stores.
		}
	}

	FORCEINLINE FMatrix GetValue()const
	{
		FMatrix Ret;
		if (ValuePtr)
		{
			FMemory::Memcpy(&Ret, ValuePtr, sizeof(FMatrix));//Temp annoyance until we fix the alignment issues with parameter stores.
		}
		return Ret;
	}
};

template<>
struct FNiagaraParameterDirectBinding<FVector4>
{
	FVector4* ValuePtr;

	FNiagaraParameterDirectBinding() :ValuePtr(nullptr) {}

	FVector4* Init(FNiagaraParameterStore& InStore, const FNiagaraVariable& DestVariable)
	{
		check(DestVariable.GetSizeInBytes() == sizeof(FVector4));
		ValuePtr = (FVector4*)InStore.GetParameterData(DestVariable);
		return ValuePtr;
	}

	FORCEINLINE void SetValue(const FVector4& InValue)
	{
		if (ValuePtr)
		{
			FMemory::Memcpy(ValuePtr, &InValue, sizeof(FVector4));//Temp annoyance until we fix the alignment issues with parameter stores.
		}
	}

	FORCEINLINE FVector4 GetValue()const
	{
		FVector4 Ret;
		if (ValuePtr)
		{
			FMemory::Memcpy(&Ret, ValuePtr, sizeof(FVector4));//Temp annoyance until we fix the alignment issues with parameter stores.
		}
		return Ret;
	}
};