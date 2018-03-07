// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "StructBox.h"

void FStructBox::Destroy(UScriptStruct* ActualStruct)
{
	if (ActualStruct && StructMemory)
	{
		ActualStruct->DestroyStruct(StructMemory);
	}

	if (StructMemory)
	{
		FMemory::Free(StructMemory);
		StructMemory = nullptr;
	}
}

FStructBox::~FStructBox()
{
	ensure(ScriptStruct || !StructMemory);
	Destroy(ScriptStruct);
}

FStructBox& FStructBox::operator=(const FStructBox& Other)
{
	if (this != &Other)
	{
		Destroy(ScriptStruct);

		ScriptStruct = Other.ScriptStruct;

		if (Other.IsValid())
		{
			Create(ScriptStruct);
			ScriptStruct->CopyScriptStruct(StructMemory, Other.StructMemory);
		}
	}

	return *this;
}

void FStructBox::Create(UScriptStruct* ActualStruct)
{
	check(NULL == StructMemory);
	StructMemory = (uint8*)FMemory::Malloc(ActualStruct->GetStructureSize());
	ActualStruct->InitializeStruct(StructMemory);
}

bool FStructBox::Serialize(FArchive& Ar)
{
	auto OldStruct = ScriptStruct;
	Ar << ScriptStruct;
	bool bValidBox = IsValid();
	Ar << bValidBox;

	if (Ar.IsLoading())
	{
		if (OldStruct != ScriptStruct)
		{
			Destroy(OldStruct);
		}
		if (ScriptStruct && !StructMemory && bValidBox)
		{
			Create(ScriptStruct);
		}
	}

	ensure(bValidBox || !IsValid());
	if (IsValid() && bValidBox)
	{
		ScriptStruct->SerializeItem(Ar, StructMemory, nullptr);
	}

	return true;
}

bool FStructBox::Identical(const FStructBox* Other, uint32 PortFlags) const
{
	if (!Other)
	{
		return false;
	}

	if (ScriptStruct != Other->ScriptStruct)
	{
		return false;
	}

	if (!ScriptStruct)
	{
		return true;
	}

	if (!StructMemory && !Other->StructMemory)
	{
		return true;
	}

	return ScriptStruct->CompareScriptStruct(StructMemory, Other->StructMemory, PortFlags);
}

void FStructBox::AddStructReferencedObjects(class FReferenceCollector& Collector) const
{
	Collector.AddReferencedObject(const_cast<FStructBox*>(this)->ScriptStruct);
	if (ScriptStruct && StructMemory && ScriptStruct->RefLink)
	{
		ScriptStruct->SerializeBin(Collector.GetVerySlowReferenceCollectorArchive(), StructMemory);
	}
}
