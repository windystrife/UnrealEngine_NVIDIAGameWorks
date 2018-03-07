// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneFolder.h"

UMovieSceneFolder::UMovieSceneFolder( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
#if WITH_EDITORONLY_DATA
	, FolderColor(FColor::White)
#endif
{
}

FName UMovieSceneFolder::GetFolderName() const
{
	return FolderName;
}


void UMovieSceneFolder::SetFolderName( FName InFolderName )
{
	FolderName = InFolderName;
}


const TArray<UMovieSceneFolder*>& UMovieSceneFolder::GetChildFolders() const
{
	return ChildFolders;
}


void UMovieSceneFolder::AddChildFolder( UMovieSceneFolder* InChildFolder )
{
	ChildFolders.Add( InChildFolder );
}


void UMovieSceneFolder::RemoveChildFolder( UMovieSceneFolder* InChildFolder )
{
	ChildFolders.Remove(InChildFolder);
}


const TArray<UMovieSceneTrack*>& UMovieSceneFolder::GetChildMasterTracks() const
{
	return ChildMasterTracks;
}


void UMovieSceneFolder::AddChildMasterTrack( UMovieSceneTrack* InMasterTrack )
{
	ChildMasterTracks.Add( InMasterTrack );
}


void UMovieSceneFolder::RemoveChildMasterTrack( UMovieSceneTrack* InMasterTrack )
{
	ChildMasterTracks.Remove( InMasterTrack );
}


const TArray<FGuid>& UMovieSceneFolder::GetChildObjectBindings() const
{
	return ChildObjectBindings;
}


void UMovieSceneFolder::AddChildObjectBinding(const FGuid& InObjectBinding )
{
	ChildObjectBindings.Add( InObjectBinding );
}


void UMovieSceneFolder::RemoveChildObjectBinding( const FGuid& InObjectBinding )
{
	ChildObjectBindings.Remove( InObjectBinding );
}


void UMovieSceneFolder::Serialize( FArchive& Archive )
{
	if ( Archive.IsLoading() )
	{
		Super::Serialize( Archive );
		ChildObjectBindings.Empty();
		for ( const FString& ChildObjectBindingString : ChildObjectBindingStrings )
		{
			FGuid ChildObjectBinding;
			FGuid::Parse( ChildObjectBindingString, ChildObjectBinding );
			ChildObjectBindings.Add( ChildObjectBinding );
		}
	}
	else
	{
		ChildObjectBindingStrings.Empty();
		for ( const FGuid& ChildObjectBinding : ChildObjectBindings )
		{
			ChildObjectBindingStrings.Add( ChildObjectBinding.ToString() );
		}
		Super::Serialize( Archive );
	}
}
