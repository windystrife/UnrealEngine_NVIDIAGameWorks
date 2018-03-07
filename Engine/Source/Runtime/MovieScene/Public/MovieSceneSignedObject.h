// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Misc/Guid.h"
#include "MovieSceneSignedObject.generated.h"

class ITransactionObjectAnnotation;

UCLASS()
class UMovieSceneSignedObject : public UObject
{
public:
	GENERATED_BODY()

	MOVIESCENE_API UMovieSceneSignedObject(const FObjectInitializer& Init);

	/**
	 * 
	 */
	MOVIESCENE_API void MarkAsChanged();

	/**
	 * 
	 */
	const FGuid& GetSignature() const
	{
		return Signature;
	}

	/** Event that is triggered whenever this object's signature has changed */
	DECLARE_EVENT(UMovieSceneSignedObject, FOnSignatureChanged)
	FOnSignatureChanged& OnSignatureChanged() { return OnSignatureChangedEvent; }

public:

	MOVIESCENE_API virtual bool Modify(bool bAlwaysMarkDirty = true) override;
	MOVIESCENE_API virtual void PostInitProperties() override;
	MOVIESCENE_API virtual void PostLoad() override;

#if WITH_EDITOR
	MOVIESCENE_API virtual void PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent) override;
	MOVIESCENE_API virtual void PostEditUndo() override;
	MOVIESCENE_API virtual void PostEditUndo(TSharedPtr<ITransactionObjectAnnotation> TransactionAnnotation) override;
#endif

private:

	/** Unique generation signature */
	UPROPERTY()
	FGuid Signature;

#if WITH_EDITOR
	/** Keep track of the above signature before and after post load to ensure that it got deserialized. If it didn't this will create deterministic cooking issues. */
	FGuid PreLoadSignature;
#endif

	/** Event that is triggered whenever this object's signature has changed */
	FOnSignatureChanged OnSignatureChangedEvent;
};
