// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "NiagaraSystemEditorData.h"
#include "NiagaraStackEditorData.h"

const FName UNiagaraSystemEditorFolder::GetFolderName() const
{
	return FolderName;
}

void UNiagaraSystemEditorFolder::SetFolderName(FName InFolderName)
{
	FolderName = InFolderName;
}

const TArray<UNiagaraSystemEditorFolder*>& UNiagaraSystemEditorFolder::GetChildFolders() const
{
	return ChildFolders;
}

void UNiagaraSystemEditorFolder::AddChildFolder(UNiagaraSystemEditorFolder* ChildFolder)
{
	Modify();
	ChildFolders.Add(ChildFolder);
}

void UNiagaraSystemEditorFolder::RemoveChildFolder(UNiagaraSystemEditorFolder* ChildFolder)
{
	Modify();
	ChildFolders.Remove(ChildFolder);
}

const TArray<FGuid>& UNiagaraSystemEditorFolder::GetChildEmitterHandleIds() const
{
	return ChildEmitterHandleIds;
}

void UNiagaraSystemEditorFolder::AddChildEmitterHandleId(FGuid ChildEmitterHandleId)
{
	Modify();
	ChildEmitterHandleIds.Add(ChildEmitterHandleId);
}

void UNiagaraSystemEditorFolder::RemoveChildEmitterHandleId(FGuid ChildEmitterHandleId)
{
	Modify();
	ChildEmitterHandleIds.Remove(ChildEmitterHandleId);
}

UNiagaraSystemEditorData::UNiagaraSystemEditorData(const FObjectInitializer& ObjectInitializer)
{
	RootFolder = ObjectInitializer.CreateDefaultSubobject<UNiagaraSystemEditorFolder>(this, TEXT("RootFolder"));
	StackEditorData = ObjectInitializer.CreateDefaultSubobject<UNiagaraStackEditorData>(this, TEXT("StackEditorData"));
	OwnerTransform.SetLocation(FVector(0.0f, 0.0f, 100.0f));
}

void UNiagaraSystemEditorData::PostLoad()
{
	Super::PostLoad();
	if (RootFolder == nullptr)
	{
		RootFolder = NewObject<UNiagaraSystemEditorFolder>(this, TEXT("RootFolder"), RF_Transactional);
	}
	if (StackEditorData == nullptr)
	{
		StackEditorData = NewObject<UNiagaraStackEditorData>(this, TEXT("StackEditorData"), RF_Transactional);
	}
}

UNiagaraSystemEditorFolder& UNiagaraSystemEditorData::GetRootFolder() const
{
	return *RootFolder;
}

UNiagaraStackEditorData& UNiagaraSystemEditorData::GetStackEditorData() const
{
	return *StackEditorData;
}