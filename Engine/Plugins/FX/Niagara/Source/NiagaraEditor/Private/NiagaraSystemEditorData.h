// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraSystemEditorData.generated.h"

class UNiagaraStackEditorData;

/** Editor only folder data for emitters in a system. */
UCLASS()
class UNiagaraSystemEditorFolder : public UObject
{
	GENERATED_BODY()

public:
	const FName GetFolderName() const;

	void SetFolderName(FName InFolderName);

	const TArray<UNiagaraSystemEditorFolder*>& GetChildFolders() const;

	void AddChildFolder(UNiagaraSystemEditorFolder* ChildFolder);

	void RemoveChildFolder(UNiagaraSystemEditorFolder* ChildFolder);

	const TArray<FGuid>& GetChildEmitterHandleIds() const;

	void AddChildEmitterHandleId(FGuid ChildEmitterHandleId);

	void RemoveChildEmitterHandleId(FGuid ChildEmitterHandleId);

private:
	UPROPERTY()
	FName FolderName;

	UPROPERTY()
	TArray<UNiagaraSystemEditorFolder*> ChildFolders;

	UPROPERTY()
	TArray<FGuid> ChildEmitterHandleIds;
};

/** Editor only UI data for systems. */
UCLASS()
class UNiagaraSystemEditorData : public UObject
{
	GENERATED_BODY()

public:
	UNiagaraSystemEditorData(const FObjectInitializer& ObjectInitializer);

	virtual void PostLoad() override;

	/** Gets the root folder for UI folders for emitters. */
	UNiagaraSystemEditorFolder& GetRootFolder() const;

	/** Gets the stack editor data for the system. */
	UNiagaraStackEditorData& GetStackEditorData() const;

	const FTransform& GetOwnerTransform() const {
		return OwnerTransform;
	}

	void SetOwnerTransform(const FTransform& InTransform)  {
		OwnerTransform = InTransform;
	}

private:
	UPROPERTY(Instanced)
	UNiagaraSystemEditorFolder* RootFolder;

	UPROPERTY(Instanced)
	UNiagaraStackEditorData* StackEditorData;

	UPROPERTY()
	FTransform OwnerTransform;
};