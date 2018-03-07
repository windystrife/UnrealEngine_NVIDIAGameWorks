// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once


#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Misc/Guid.h"
#include "MovieSceneFolder.generated.h"

class UMovieSceneTrack;

/** Reprents a folder used for organizing objects in tracks in a movie scene. */
UCLASS()
class MOVIESCENE_API UMovieSceneFolder : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Gets the name of this folder. */
	FName GetFolderName() const;

	/** Sets the name of this folder. */
	void SetFolderName( FName InFolderName );

	/** Gets the folders contained by this folder. */
	const TArray<UMovieSceneFolder*>& GetChildFolders() const;

	/** Adds a child folder to this folder. */
	void AddChildFolder( UMovieSceneFolder* InChildFolder );

	/** Removes a child folder from this folder. */
	void RemoveChildFolder( UMovieSceneFolder* InChildFolder );

	/** Gets the master tracks contained by this folder. */
	const TArray<UMovieSceneTrack*>& GetChildMasterTracks() const;

	/** Adds a master track to this folder. */
	void AddChildMasterTrack( UMovieSceneTrack* InMasterTrack );

	/** Removes a master track from this folder. */
	void RemoveChildMasterTrack( UMovieSceneTrack* InMasterTrack );

	/** Gets the guids for the object bindings contained by this folder. */
	const TArray<FGuid>& GetChildObjectBindings() const;

	/** Adds a guid for an object binding to this folder. */
	void AddChildObjectBinding(const FGuid& InObjectBinding );

	/** Removes a guid for an object binding from this folder. */
	void RemoveChildObjectBinding( const FGuid& InObjectBinding );


	virtual void Serialize( FArchive& Archive );

#if WITH_EDITORONLY_DATA
	/**
	 * Get this folder's color.
	 *
	 * @return The folder color.
	 */
	const FColor& GetFolderColor() const
	{
		return FolderColor;
	}

	/**
	 * Set this folder's color.
	 *
	 * @param InFolderColor The folder color to set.
	 */
	void SetFolderColor(const FColor& InFolderColor)
	{
		FolderColor = InFolderColor;
	}
#endif

private:
	/** The name of this folder. */
	UPROPERTY()
	FName FolderName;

	/** The folders contained by this folder. */
	UPROPERTY()
	TArray<UMovieSceneFolder*> ChildFolders;

	/** The master tracks contained by this folder. */
	UPROPERTY()
	TArray<UMovieSceneTrack*> ChildMasterTracks;

	/** The guid strings used to serialize the guids for the object bindings contained by this folder. */
	UPROPERTY()
	TArray<FString> ChildObjectBindingStrings;

#if WITH_EDITORONLY_DATA
	/** This folder's color */
	UPROPERTY(EditAnywhere, Category=General, DisplayName=Color)
	FColor FolderColor;
#endif

	/** The guids for the object bindings contained by this folder. */
	TArray<FGuid> ChildObjectBindings;
};

