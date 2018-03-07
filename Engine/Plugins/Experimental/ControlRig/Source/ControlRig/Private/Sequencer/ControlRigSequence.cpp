// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ControlRigSequence.h"
#include "UObject/Package.h"
#include "MovieScene.h"
#include "ControlRig.h"
#include "Engine/Engine.h"

#define LOCTEXT_NAMESPACE "ControlRigSequence"

UControlRigSequence::UControlRigSequence(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, LastExportedFrameRate(30.0f)
{
	bParentContextsAreSignificant = false;
}

void UControlRigSequence::Initialize()
{
	MovieScene = NewObject<UMovieScene>(this, NAME_None, RF_Transactional);
	MovieScene->SetFlags(RF_Transactional);
}

void UControlRigSequence::BindPossessableObject(const FGuid& ObjectId, UObject& PossessedObject, UObject* Context)
{
}

bool UControlRigSequence::CanPossessObject(UObject& Object, UObject* InPlaybackContext) const
{
	return false; // we only support spawnables
}

UObject* UControlRigSequence::GetParentObject(UObject* Object) const
{
	return nullptr;
}

void UControlRigSequence::UnbindPossessableObjects(const FGuid& ObjectId)
{
}

UObject* UControlRigSequence::MakeSpawnableTemplateFromInstance(UObject& InSourceObject, FName ObjectName)
{
	UObject* NewInstance = NewObject<UObject>(MovieScene, InSourceObject.GetClass(), ObjectName);

	UEngine::FCopyPropertiesForUnrelatedObjectsParams CopyParams;
	CopyParams.bNotifyObjectReplacement = false;
	UEngine::CopyPropertiesForUnrelatedObjects(&InSourceObject, NewInstance, CopyParams);

	return NewInstance;
}

bool UControlRigSequence::CanAnimateObject(UObject& InObject) const
{
	return InObject.IsA<UControlRig>();
}

#undef LOCTEXT_NAMESPACE
