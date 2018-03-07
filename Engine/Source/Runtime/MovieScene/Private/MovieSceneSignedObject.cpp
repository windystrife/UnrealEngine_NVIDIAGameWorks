// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneSignedObject.h"
#include "Templates/Casts.h"
#include "MovieSceneSequence.h"
#include "Package.h"

UMovieSceneSignedObject::UMovieSceneSignedObject(const FObjectInitializer& Init)
	: Super(Init)
{
#if WITH_EDITOR
	PreLoadSignature.A = PreLoadSignature.B = PreLoadSignature.C = PreLoadSignature.D = 0xFFFFFFFF;
#endif
}

void UMovieSceneSignedObject::PostInitProperties()
{
	Super::PostInitProperties();

	// Always seed newly created objects with a new signature
	// (CDO and archetypes always have a zero GUID)
	if (!HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) && Signature == GetDefault<UMovieSceneSignedObject>()->Signature)
	{
		Signature = FGuid::NewGuid();
#if WITH_EDITOR
		PreLoadSignature = Signature;
#endif
	}
}

void UMovieSceneSignedObject::PostLoad()
{
	Super::PostLoad();
#if WITH_EDITOR
	if (!HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) && Signature == PreLoadSignature)
	{
		UPackage* Package = GetOutermost();
		UMovieSceneSequence* Sequence = GetTypedOuter<UMovieSceneSequence>();
		FString PackageName = Package ? Package->GetName() : TEXT("Unknown package");
		FString SequenceName = Sequence ? Sequence->GetName() : TEXT("Unknown sequence");
		UE_LOG(LogMovieScene, Warning, TEXT("Legacy data detected in sequence '%s (%s)'. This will cause deterministic cooking issues. Please resave the package."), *PackageName, *SequenceName);
	}
#endif
}

void UMovieSceneSignedObject::MarkAsChanged()
{
	Signature = FGuid::NewGuid();

	OnSignatureChangedEvent.Broadcast();
	
	UObject* Outer = GetOuter();
	while (Outer)
	{
		UMovieSceneSignedObject* TypedOuter = Cast<UMovieSceneSignedObject>(Outer);
		if (TypedOuter)
		{
			TypedOuter->MarkAsChanged();
			break;
		}
		Outer = Outer->GetOuter();
	}
}

bool UMovieSceneSignedObject::Modify(bool bAlwaysMarkDirty)
{
	bool bModified = Super::Modify(bAlwaysMarkDirty);
	if ( bAlwaysMarkDirty )
	{
		MarkAsChanged();
	}
	return bModified;
}

#if WITH_EDITOR

void UMovieSceneSignedObject::PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	MarkAsChanged();
}

void UMovieSceneSignedObject::PostEditUndo()
{
	Super::PostEditUndo();
	MarkAsChanged();
}

void UMovieSceneSignedObject::PostEditUndo(TSharedPtr<ITransactionObjectAnnotation> TransactionAnnotation)
{
	Super::PostEditUndo(TransactionAnnotation);
	MarkAsChanged();
}

#endif

