// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"
#include "Templates/SubclassOf.h"
#include "KeyPropertyParams.h"
#include "ISequencer.h"
#include "MovieSceneTrack.h"
#include "UObject/Package.h"
#include "MovieSceneTrackEditor.h"
#include "KeyframeTrackEditor.h"
#include "ISequencerObjectChangeListener.h"
#include "AnimatedPropertyKey.h"
#include "Tracks/MovieScenePropertyTrack.h"

/**
* Tools for animatable property types such as floats ands vectors
*/
template<typename TrackType, typename SectionType, typename KeyDataType>
class FPropertyTrackEditor
	: public FKeyframeTrackEditor<TrackType, SectionType, KeyDataType>
{
public:
	/**
	 * Constructor
	 *
	 * @param InSequencer The sequencer instance to be used by this tool
	 */
	FPropertyTrackEditor( TSharedRef<ISequencer> InSequencer )
		: FKeyframeTrackEditor<TrackType, SectionType, KeyDataType>( InSequencer )
	{ }

	/**
	 * Constructor
	 *
	 * @param InSequencer The sequencer instance to be used by this tool
	 * @param InWatchedPropertyTypes A list of property types that this editor can animate
	 */
	FPropertyTrackEditor( TSharedRef<ISequencer> InSequencer, TArrayView<const FAnimatedPropertyKey> InWatchedPropertyTypes )
		: FKeyframeTrackEditor<TrackType, SectionType, KeyDataType>( InSequencer )
	{
		for (const FAnimatedPropertyKey& Key : InWatchedPropertyTypes)
		{
			AddWatchedProperty(Key);
		}
	}

	DEPRECATED(4.16, "Please use FPropertyTrackEditor(TSharedRef<ISequencer>, TArrayView<const FAnimatedPropertyKey>)")
	FPropertyTrackEditor( TSharedRef<ISequencer> InSequencer, FName WatchedPropertyTypeName )
		: FKeyframeTrackEditor<TrackType, SectionType, KeyDataType>( InSequencer )
	{
		AddWatchedPropertyType( WatchedPropertyTypeName );
	}

	DEPRECATED(4.16, "Please use FPropertyTrackEditor(TSharedRef<ISequencer>, TArrayView<const FAnimatedPropertyKey>)")
	FPropertyTrackEditor( TSharedRef<ISequencer> InSequencer, FName WatchedPropertyTypeName1, FName WatchedPropertyTypeName2 )
		: FKeyframeTrackEditor<TrackType, SectionType, KeyDataType>( InSequencer )
	{
		AddWatchedPropertyType( WatchedPropertyTypeName1 );
		AddWatchedPropertyType( WatchedPropertyTypeName2 );
	}

	DEPRECATED(4.16, "Please use FPropertyTrackEditor(TSharedRef<ISequencer>, TArrayView<const FAnimatedPropertyKey>)")
	FPropertyTrackEditor( TSharedRef<ISequencer> InSequencer, FName WatchedPropertyTypeName1, FName WatchedPropertyTypeName2, FName WatchedPropertyTypeName3 )
		: FKeyframeTrackEditor<TrackType, SectionType, KeyDataType>( InSequencer )
	{
		AddWatchedPropertyType( WatchedPropertyTypeName1 );
		AddWatchedPropertyType( WatchedPropertyTypeName2 );
		AddWatchedPropertyType( WatchedPropertyTypeName3 );
	}

	DEPRECATED(4.16, "Please use FPropertyTrackEditor(TSharedRef<ISequencer>, TArrayView<const FAnimatedPropertyKey>)")
	FPropertyTrackEditor( TSharedRef<ISequencer> InSequencer, TArray<FName> InWatchedPropertyTypeNames )
		: FKeyframeTrackEditor<TrackType, SectionType, KeyDataType>( InSequencer )
	{
		for ( FName WatchedPropertyTypeName : InWatchedPropertyTypeNames )
		{
			AddWatchedPropertyType( WatchedPropertyTypeName );
		}
	}

	~FPropertyTrackEditor()
	{
		TSharedPtr<ISequencer> SequencerPtr = FMovieSceneTrackEditor::GetSequencer();
		if ( SequencerPtr.IsValid() )
		{
			ISequencerObjectChangeListener& ObjectChangeListener = SequencerPtr->GetObjectChangeListener();
			for ( FAnimatedPropertyKey PropertyKey : WatchedProperties )
			{
				ObjectChangeListener.GetOnAnimatablePropertyChanged( PropertyKey ).RemoveAll( this );
			}
		}
	}

public:

	//~ ISequencerTrackEditor interface

    virtual bool SupportsSequence(UMovieSceneSequence* InSequence) const override
    {
        return true;
    }

	virtual bool SupportsType( TSubclassOf<UMovieSceneTrack> Type ) const override
	{
		return Type == TrackType::StaticClass();
	}

protected:

	/**
	* Generates keys based on the new value from the property property change parameters.
	*
	* @param PropertyChangedParams Parameters associated with the property change.
	* @param NewGeneratedKeys New keys which should be added due to the property change.
	* @param DefaultGeneratedKeys Default value keys which should not be added, but may be needed for setting up defaults on new multi-channel tracks.
	*/
	virtual void GenerateKeysFromPropertyChanged( const FPropertyChangedParams& PropertyChangedParams, TArray<KeyDataType>& NewGeneratedKeys, TArray<KeyDataType>& DefaultGeneratedKeys ) = 0;

	/** When true, this track editor will only be used on properties which have specified it as a custom track class. This is necessary to prevent duplicate
		property change handling in cases where a custom track editor handles the same type of data as one of the standard track editors. */
	virtual bool ForCustomizedUseOnly() { return false; }

	/** 
	 * Initialized values on a track after it's been created, but before any sections or keys have been added.
	 * @param NewTrack The newly created track.
	 * @param PropertyChangedParams The property change parameters which caused this track to be created.
	 */
	virtual void InitializeNewTrack( TrackType* NewTrack, FPropertyChangedParams PropertyChangedParams )
	{
		const UProperty* ChangedProperty = PropertyChangedParams.PropertyPath.GetLeafMostProperty().Property.Get();
		if (ChangedProperty)
		{
			NewTrack->SetPropertyNameAndPath( ChangedProperty->GetFName(), PropertyChangedParams.GetPropertyPathString() );
#if WITH_EDITORONLY_DATA

			FText DisplayText;

			// Set up the appropriate name for the track from an array/nested struct index if necessary
			for (int32 PropertyIndex = PropertyChangedParams.PropertyPath.GetNumProperties() - 1; PropertyIndex >= 0; --PropertyIndex)
			{
				const FPropertyInfo& Info = PropertyChangedParams.PropertyPath.GetPropertyInfo(PropertyIndex);
				const UArrayProperty* ParentArrayProperty = PropertyIndex > 0 ? Cast<UArrayProperty>(PropertyChangedParams.PropertyPath.GetPropertyInfo(PropertyIndex - 1).Property.Get()) : nullptr;

				UProperty* ArrayInnerProperty = Info.Property.Get();
				if (ArrayInnerProperty && Info.ArrayIndex != INDEX_NONE)
				{
					DisplayText = FText::Format(NSLOCTEXT("PropertyTrackEditor", "DisplayTextArrayFormat", "{0} ({1}[{2}])"),
						ChangedProperty->GetDisplayNameText(),
						(ParentArrayProperty ? ParentArrayProperty : ArrayInnerProperty)->GetDisplayNameText(),
						FText::AsNumber(Info.ArrayIndex)
					);
					break;
				}
			}

			if (DisplayText.IsEmpty())
			{
				for (int32 PropertyIndex = PropertyChangedParams.PropertyPath.GetNumProperties() - 1; PropertyIndex >= 0; --PropertyIndex)
				{
					const UStructProperty* ParentStructProperty = PropertyIndex > 0 ? Cast<UStructProperty>(PropertyChangedParams.PropertyPath.GetPropertyInfo(PropertyIndex - 1).Property.Get()) : nullptr;
					if (ParentStructProperty)
					{
						DisplayText = FText::Format(NSLOCTEXT("PropertyTrackEditor", "DisplayTextStructFormat", "{0} ({1})"),
							ChangedProperty->GetDisplayNameText(),
							ParentStructProperty->GetDisplayNameText()
						);
						break;
					}
				}
			}

			if (DisplayText.IsEmpty())
			{
				DisplayText = ChangedProperty->GetDisplayNameText();
			}

			NewTrack->SetDisplayName(DisplayText);
#endif
		}
	}

	virtual UMovieSceneTrack* AddTrack(UMovieScene* FocusedMovieScene, const FGuid& ObjectHandle, TSubclassOf<class UMovieSceneTrack> TrackClass, FName UniqueTypeName) override
	{
		UMovieSceneTrack* Track = FocusedMovieScene->AddTrack(TrackClass, ObjectHandle);
		if (UMovieScenePropertyTrack* PropertyTrack = Cast<UMovieScenePropertyTrack>(Track))
		{
			PropertyTrack->UniqueTrackName = UniqueTypeName;
		}
		return Track;
	}

protected:

	/** Adds a callback for property changes for the supplied property type name. */
	void AddWatchedProperty( FAnimatedPropertyKey PropertyKey )
	{
		FMovieSceneTrackEditor::GetSequencer()->GetObjectChangeListener().GetOnAnimatablePropertyChanged( PropertyKey ).AddRaw( this, &FPropertyTrackEditor<TrackType, SectionType, KeyDataType>::OnAnimatedPropertyChanged );
		WatchedProperties.Add( PropertyKey );
	}

private:

	/** Adds a callback for property changes for the supplied property type name. */
	void AddWatchedPropertyType( FName WatchedPropertyTypeName )
	{
		AddWatchedProperty(FAnimatedPropertyKey::FromPropertyTypeName(WatchedPropertyTypeName));
	}

	/** Get a customized track class from the property if there is one, otherwise return nullptr. */
	TSubclassOf<UMovieSceneTrack> GetCustomizedTrackClass( const UProperty* Property )
	{
		// Look for a customized track class for this property on the meta data
		const FString& MetaSequencerTrackClass = Property->GetMetaData( TEXT( "SequencerTrackClass" ) );
		if ( !MetaSequencerTrackClass.IsEmpty() )
		{
			UClass* MetaClass = FindObject<UClass>( ANY_PACKAGE, *MetaSequencerTrackClass );
			if ( !MetaClass )
			{
				MetaClass = LoadObject<UClass>( nullptr, *MetaSequencerTrackClass );
			}
			return MetaClass;
		}
		return nullptr;
	}

	/**
	* Called by the details panel when an animatable property changes
	*
	* @param InObjectsThatChanged	List of objects that changed
	* @param KeyPropertyParams		Parameters for the property change.
	*/
	virtual void OnAnimatedPropertyChanged( const FPropertyChangedParams& PropertyChangedParams )
	{
		FMovieSceneTrackEditor::AnimatablePropertyChanged( FOnKeyProperty::CreateRaw( this, &FPropertyTrackEditor::OnKeyProperty, PropertyChangedParams ) );
	}

	/** Adds a key based on a property change. */
	FKeyPropertyResult OnKeyProperty( float KeyTime, FPropertyChangedParams PropertyChangedParams )
	{
		FKeyPropertyResult KeyPropertyResult;

		TArray<KeyDataType> NewKeysForPropertyChange;
		TArray<KeyDataType> DefaultKeysForPropertyChange;
		GenerateKeysFromPropertyChanged( PropertyChangedParams, NewKeysForPropertyChange, DefaultKeysForPropertyChange );

		UProperty* Property = PropertyChangedParams.PropertyPath.GetLeafMostProperty().Property.Get();
		if (!Property)
		{
			return KeyPropertyResult;
		}

		TSubclassOf<UMovieSceneTrack> CustomizedClass = GetCustomizedTrackClass( Property );
		TSubclassOf<UMovieSceneTrack> TrackClass;
		
		if (CustomizedClass != nullptr)
		{
			TrackClass = CustomizedClass;
		}
		else
		{
			TrackClass = TrackType::StaticClass();
		}

		FName UniqueName(*PropertyChangedParams.PropertyPath.ToString(TEXT(".")));

		// If the track class has been customized for this property then it's possible this track editor doesn't support it, 
		// also check for track editors which should only be used for customization.
		if ( SupportsType( TrackClass ) && ( ForCustomizedUseOnly() == false || *CustomizedClass != nullptr) )
		{
			return FKeyframeTrackEditor<TrackType, SectionType, KeyDataType>::AddKeysToObjects(
				PropertyChangedParams.ObjectsThatChanged,
				KeyTime,
				NewKeysForPropertyChange,
				DefaultKeysForPropertyChange,
				PropertyChangedParams.KeyMode,
				TrackClass,
				UniqueName,
				[&](TrackType* NewTrack) { InitializeNewTrack(NewTrack, PropertyChangedParams); }
			);
		}
		else
		{
			return KeyPropertyResult;
		}
	}

private:

	/** An array of property type names which are being watched for changes. */
	TArray<FAnimatedPropertyKey> WatchedProperties;
};
