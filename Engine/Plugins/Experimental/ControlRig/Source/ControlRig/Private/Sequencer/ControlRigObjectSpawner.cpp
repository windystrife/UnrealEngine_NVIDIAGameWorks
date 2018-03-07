// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ControlRigObjectSpawner.h"
#include "ControlRig.h"
#include "MovieSceneSpawnable.h"
#include "IMovieScenePlayer.h"
#include "Package.h"

TSharedRef<IMovieSceneObjectSpawner> FControlRigObjectSpawner::CreateObjectSpawner()
{
	return MakeShareable(new FControlRigObjectSpawner);
}

FControlRigObjectSpawner::FControlRigObjectSpawner()
{
	ObjectHolderPtr = NewObject<UControlRigObjectHolder>();
	ObjectHolderPtr->AddToRoot();
}

FControlRigObjectSpawner::~FControlRigObjectSpawner()
{
	if (ObjectHolderPtr.IsValid())
	{
		ObjectHolderPtr->RemoveFromRoot();
	}
}

UClass* FControlRigObjectSpawner::GetSupportedTemplateType() const
{
	return UControlRig::StaticClass();
}

UObject* FControlRigObjectSpawner::SpawnObject(FMovieSceneSpawnable& Spawnable, FMovieSceneSequenceIDRef TemplateID, IMovieScenePlayer& Player)
{
	UObject* ObjectTemplate = Spawnable.GetObjectTemplate();

	if (UControlRig* ControlRig = Cast<UControlRig>(ObjectTemplate))
	{
		FName ObjectName = *(ControlRig->GetClass()->GetName() + Spawnable.GetGuid().ToString() + FString::FromInt(TemplateID.GetInternalValue()));
		UControlRig* SpawnedObject = NewObject<UControlRig>(ObjectHolderPtr.Get(), ControlRig->GetClass(), ObjectName, RF_Transient);
		ObjectHolderPtr->Objects.Add(SpawnedObject);
		SpawnedObject->Initialize();

		return SpawnedObject;
	}

	return nullptr;
}

void FControlRigObjectSpawner::DestroySpawnedObject(UObject& Object)
{
	if (UControlRig* ControlRig = Cast<UControlRig>(&Object))
	{
		ControlRig->Rename(nullptr, GetTransientPackage());
		ControlRig->MarkPendingKill();
		ObjectHolderPtr->Objects.Remove(&Object);
	}
}
