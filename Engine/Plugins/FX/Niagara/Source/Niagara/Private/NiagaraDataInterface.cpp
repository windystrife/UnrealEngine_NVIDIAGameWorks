// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataInterface.h"
#include "Curves/CurveVector.h"
#include "Curves/CurveLinearColor.h"
#include "Curves/CurveFloat.h"
#include "NiagaraTypes.h"

UNiagaraDataInterface::UNiagaraDataInterface(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

bool UNiagaraDataInterface::CopyTo(UNiagaraDataInterface* Destination) const 
{
	if (Destination == nullptr || Destination->GetClass() != GetClass())
	{
		return false;
	}
	return true;
}

bool UNiagaraDataInterface::Equals(const UNiagaraDataInterface* Other) const
{
	if (Other == nullptr || Other->GetClass() != GetClass())
	{
		return false;
	}
	return true;
}

bool UNiagaraDataInterface::IsDataInterfaceType(const FNiagaraTypeDefinition& TypeDef)
{
	const UClass* Class = TypeDef.GetClass();
	if (Class && Class->IsChildOf(UNiagaraDataInterface::StaticClass()))
	{
		return true;
	}
	return false;
}

