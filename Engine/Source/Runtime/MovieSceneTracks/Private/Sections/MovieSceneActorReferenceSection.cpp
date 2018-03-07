// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneActorReferenceSection.h"


UMovieSceneActorReferenceSection::UMovieSceneActorReferenceSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{ }


FGuid UMovieSceneActorReferenceSection::Eval( float Position ) const
{
	int32 ActorGuidIndex = ActorGuidIndexCurve.Evaluate( Position );
	return ActorGuidIndex >= 0 && ActorGuidIndex < ActorGuids.Num() ? ActorGuids[ActorGuidIndex] : FGuid();
}


void UMovieSceneActorReferenceSection::MoveSection( float DeltaPosition, TSet<FKeyHandle>& KeyHandles )
{
	Super::MoveSection( DeltaPosition, KeyHandles );

	// Move the curve
	ActorGuidIndexCurve.ShiftCurve( DeltaPosition, KeyHandles );
}


void UMovieSceneActorReferenceSection::DilateSection( float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles )
{	
	Super::DilateSection(DilationFactor, Origin, KeyHandles);
	
	ActorGuidIndexCurve.ScaleCurve( Origin, DilationFactor, KeyHandles );
}


void UMovieSceneActorReferenceSection::GetKeyHandles(TSet<FKeyHandle>& OutKeyHandles, TRange<float> TimeRange) const
{
	if (!TimeRange.Overlaps(GetRange()))
	{
		return;
	}

	for ( auto It( ActorGuidIndexCurve.GetKeyHandleIterator() ); It; ++It )
	{
		float Time = ActorGuidIndexCurve.GetKeyTime( It.Key() );
		if (TimeRange.Contains(Time))
		{
			OutKeyHandles.Add(It.Key());
		}
	}
}


TOptional<float> UMovieSceneActorReferenceSection::GetKeyTime( FKeyHandle KeyHandle ) const
{
	if ( ActorGuidIndexCurve.IsKeyHandleValid( KeyHandle ) )
	{
		return TOptional<float>( ActorGuidIndexCurve.GetKeyTime( KeyHandle ) );
	}
	return TOptional<float>();
}


void UMovieSceneActorReferenceSection::SetKeyTime( FKeyHandle KeyHandle, float Time )
{
	if ( ActorGuidIndexCurve.IsKeyHandleValid( KeyHandle ) )
	{
		ActorGuidIndexCurve.SetKeyTime( KeyHandle, Time );
	}
}


void UMovieSceneActorReferenceSection::AddKey( float Time, const FGuid& Value, EMovieSceneKeyInterpolation KeyInterpolation )
{
	AddKey(Time, Value);
}


FKeyHandle UMovieSceneActorReferenceSection::AddKey(float Time, const FGuid& Value)
{
	if (IsTimeWithinSection(Time))
	{
		if (TryModify())
		{
			FKeyHandle ExistingKeyHandle = ActorGuidIndexCurve.FindKey(Time);
			if (ActorGuidIndexCurve.IsKeyHandleValid(ExistingKeyHandle))
			{
				int32 ActorGuidIndex = ActorGuidIndexCurve.GetKeyValue(ExistingKeyHandle);
				if (ActorGuidIndex >= 0 && ActorGuidIndex < ActorGuids.Num())
				{
					ActorGuids[ActorGuidIndex] = Value;
					return ExistingKeyHandle;
				}
				else
				{
					int32 NewActorGuidIndex = ActorGuids.Add(Value);
					return ActorGuidIndexCurve.UpdateOrAddKey(Time, NewActorGuidIndex);
				}
			}
			else
			{
				int32 NewActorGuidIndex = ActorGuids.Add(Value);
				return ActorGuidIndexCurve.AddKey(Time, NewActorGuidIndex);
			}
		}
	}
	return FKeyHandle();
}


bool UMovieSceneActorReferenceSection::NewKeyIsNewData(float Time, const FGuid& Value) const
{
	return Eval(Time) != Value;
}


bool UMovieSceneActorReferenceSection::HasKeys( const FGuid& Value ) const
{
	return ActorGuidIndexCurve.GetNumKeys() > 0;
}


void UMovieSceneActorReferenceSection::SetDefault( const FGuid& Value )
{
	int32 CurrentDefault = ActorGuidIndexCurve.GetDefaultValue();
	if (!ActorGuids.IsValidIndex(CurrentDefault) || ActorGuids[CurrentDefault] != Value)
	{
		if (TryModify())
		{
			int32 DefaultIndex = ActorGuids.Add( Value );
			ActorGuidIndexCurve.SetDefaultValue( DefaultIndex );
		}
	}
}


void UMovieSceneActorReferenceSection::ClearDefaults()
{
	ActorGuidIndexCurve.ClearDefaultValue();
}


void UMovieSceneActorReferenceSection::PreSave(const class ITargetPlatform* TargetPlatform)
{
	ActorGuidStrings.Empty();
	for ( const FGuid& ActorGuid : ActorGuids )
	{
		ActorGuidStrings.Add( ActorGuid.ToString() );
	}
	Super::PreSave(TargetPlatform);
}


void UMovieSceneActorReferenceSection::PostLoad()
{
	Super::PostLoad();
	ActorGuids.Empty();
	for ( const FString& ActorGuidString : ActorGuidStrings )
	{
		FGuid ActorGuid;
		FGuid::Parse( ActorGuidString, ActorGuid );
		ActorGuids.Add( ActorGuid );
	}
}
