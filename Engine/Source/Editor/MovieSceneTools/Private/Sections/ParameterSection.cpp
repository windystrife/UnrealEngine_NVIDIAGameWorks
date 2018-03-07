// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Sections/ParameterSection.h"
#include "ISectionLayoutBuilder.h"
#include "ScopedTransaction.h"
#include "Sections/MovieSceneParameterSection.h"
#include "FloatCurveKeyArea.h"


#define LOCTEXT_NAMESPACE "ParameterSection"

DECLARE_DELEGATE(FLayoutParameterDelegate);


struct FIndexAndLayoutParameterDelegate
{
	int32 Index;
	FLayoutParameterDelegate LayoutParameterDelegate;

	bool operator<(FIndexAndLayoutParameterDelegate const& Other) const
	{
		return Index < Other.Index;
	}
};


void LayoutScalarParameter( ISectionLayoutBuilder* LayoutBuilder, FScalarParameterNameAndCurve* ScalarParameterNameAndCurve, UMovieSceneParameterSection* ParameterSection )
{
	LayoutBuilder->AddKeyArea(
		ScalarParameterNameAndCurve->ParameterName,
		FText::FromName( ScalarParameterNameAndCurve->ParameterName ),
		MakeShareable( new FFloatCurveKeyArea( &ScalarParameterNameAndCurve->ParameterCurve, ParameterSection ) ) );
}


void LayoutVectorParameter( ISectionLayoutBuilder* LayoutBuilder, FVectorParameterNameAndCurves* VectorParameterNameAndCurves, UMovieSceneParameterSection* ParameterSection )
{
	LayoutBuilder->PushCategory( VectorParameterNameAndCurves->ParameterName, FText::FromName( VectorParameterNameAndCurves->ParameterName ) );
	{
		LayoutBuilder->AddKeyArea( "X", NSLOCTEXT( "ParameterSection", "XArea", "X" ),
			MakeShareable( new FFloatCurveKeyArea( &VectorParameterNameAndCurves->XCurve, ParameterSection ) ) );
		LayoutBuilder->AddKeyArea( "Y", NSLOCTEXT( "ParameterSection", "YArea", "Y" ), 
			MakeShareable( new FFloatCurveKeyArea( &VectorParameterNameAndCurves->YCurve, ParameterSection ) ) );
		LayoutBuilder->AddKeyArea( "Z", NSLOCTEXT( "ParameterSection", "ZArea", "Z" ), 
			MakeShareable( new FFloatCurveKeyArea( &VectorParameterNameAndCurves->ZCurve, ParameterSection ) ) );
	}
	LayoutBuilder->PopCategory();
}


void LayoutColorParameter( ISectionLayoutBuilder* LayoutBuilder, FColorParameterNameAndCurves* ColorParameterNameAndCurves, UMovieSceneParameterSection* ParameterSection )
{
	LayoutBuilder->PushCategory( ColorParameterNameAndCurves->ParameterName, FText::FromName( ColorParameterNameAndCurves->ParameterName ) );
	{
		LayoutBuilder->AddKeyArea( "R", NSLOCTEXT( "ParameterSection", "RedArea", "Red" ),
			MakeShareable( new FFloatCurveKeyArea( &ColorParameterNameAndCurves->RedCurve, ParameterSection ) ) );
		LayoutBuilder->AddKeyArea( "G", NSLOCTEXT( "ParameterSection", "GreenArea", "Green" ),
			MakeShareable( new FFloatCurveKeyArea( &ColorParameterNameAndCurves->GreenCurve, ParameterSection ) ) );
		LayoutBuilder->AddKeyArea( "B", NSLOCTEXT( "ParameterSection", "BlueArea", "Blue" ),
			MakeShareable( new FFloatCurveKeyArea( &ColorParameterNameAndCurves->BlueCurve, ParameterSection ) ) );
		LayoutBuilder->AddKeyArea( "A", NSLOCTEXT( "ParameterSection", "OpacityArea", "Opacity" ),
			MakeShareable( new FFloatCurveKeyArea( &ColorParameterNameAndCurves->AlphaCurve, ParameterSection ) ) );
	}
	LayoutBuilder->PopCategory();
}

void FParameterSection::GenerateSectionLayout( class ISectionLayoutBuilder& LayoutBuilder ) const
{
	// TODO - This seems a bit convoluted here to have to use delegates to get the layout to happen in the right order based on the indices.
	UMovieSceneParameterSection* ParameterSection = Cast<UMovieSceneParameterSection>( &SectionObject );
	TArray<FIndexAndLayoutParameterDelegate> IndexAndLayoutDelegates;

	// Collect scalar parameters.
	TArray<FScalarParameterNameAndCurve>* ScalarParameterNamesAndCurves = ParameterSection->GetScalarParameterNamesAndCurves();
	for ( int32 i = 0; i < ScalarParameterNamesAndCurves->Num(); i++)
	{
		FIndexAndLayoutParameterDelegate IndexAndLayoutDelegate;
		IndexAndLayoutDelegate.Index = (*ScalarParameterNamesAndCurves)[i].Index;
		IndexAndLayoutDelegate.LayoutParameterDelegate.BindStatic(&LayoutScalarParameter, &LayoutBuilder, &(*ScalarParameterNamesAndCurves)[i], ParameterSection);
		IndexAndLayoutDelegates.Add(IndexAndLayoutDelegate);
	}

	// Collect vector parameters
	TArray<FVectorParameterNameAndCurves>* VectorParameterNamesAndCurves = ParameterSection->GetVectorParameterNamesAndCurves();
	for ( int32 i = 0; i < VectorParameterNamesAndCurves->Num(); i++ )
	{
		FIndexAndLayoutParameterDelegate IndexAndLayoutDelegate;
		IndexAndLayoutDelegate.Index = ( *VectorParameterNamesAndCurves )[i].Index;
		IndexAndLayoutDelegate.LayoutParameterDelegate.BindStatic( &LayoutVectorParameter, &LayoutBuilder, &( *VectorParameterNamesAndCurves )[i], ParameterSection );
		IndexAndLayoutDelegates.Add( IndexAndLayoutDelegate );
	}

	// Collect Color parameters
	TArray<FColorParameterNameAndCurves>* ColorParameterNamesAndCurves = ParameterSection->GetColorParameterNamesAndCurves();
	for ( int32 i = 0; i < ColorParameterNamesAndCurves->Num(); i++ )
	{
		FIndexAndLayoutParameterDelegate IndexAndLayoutDelegate;
		IndexAndLayoutDelegate.Index = (*ColorParameterNamesAndCurves)[i].Index;
		IndexAndLayoutDelegate.LayoutParameterDelegate.BindStatic( &LayoutColorParameter, &LayoutBuilder, &(*ColorParameterNamesAndCurves)[i], ParameterSection );
		IndexAndLayoutDelegates.Add( IndexAndLayoutDelegate );
	}

	// Sort and perform layout
	IndexAndLayoutDelegates.Sort();
	for ( FIndexAndLayoutParameterDelegate& IndexAndLayoutDelegate : IndexAndLayoutDelegates )
	{
		IndexAndLayoutDelegate.LayoutParameterDelegate.Execute();
	}
}


bool FParameterSection::RequestDeleteCategory( const TArray<FName>& CategoryNamePath )
{
	const FScopedTransaction Transaction( LOCTEXT( "DeleteVectorOrColorParameter", "Delete vector or color parameter" ) );
	UMovieSceneParameterSection* ParameterSection = Cast<UMovieSceneParameterSection>( &SectionObject );
	if( ParameterSection->Modify() )
	{
		bool bVectorParameterDeleted = ParameterSection->RemoveVectorParameter( CategoryNamePath[0] );
		bool bColorParameterDeleted = ParameterSection->RemoveColorParameter( CategoryNamePath[0] );
		return bVectorParameterDeleted || bColorParameterDeleted;
	}
	return false;
}


bool FParameterSection::RequestDeleteKeyArea( const TArray<FName>& KeyAreaNamePath )
{
	// Only handle paths with a single name, in all other cases the user is deleting a component of a vector parameter.
	if ( KeyAreaNamePath.Num() == 1)
	{
		const FScopedTransaction Transaction( LOCTEXT( "DeleteScalarParameter", "Delete scalar parameter" ) );
		UMovieSceneParameterSection* ParameterSection = Cast<UMovieSceneParameterSection>( &SectionObject );
		if (ParameterSection->TryModify())
		{
			return ParameterSection->RemoveScalarParameter( KeyAreaNamePath[0] );
		}
	}
	return false;
}


#undef LOCTEXT_NAMESPACE
