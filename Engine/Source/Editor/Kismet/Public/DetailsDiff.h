// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "PropertyPath.h"
#include "DiffUtils.h"

class IDetailsView;

class KISMET_API FDetailsDiff
{
public:
	DECLARE_DELEGATE( FOnDisplayedPropertiesChanged );

	FDetailsDiff( const UObject* InObject, FOnDisplayedPropertiesChanged InOnDisplayedPropertiesChanged );
	~FDetailsDiff();

	void HighlightProperty( const FPropertySoftPath& PropertyName );
	TSharedRef< SWidget > DetailsWidget();
	TArray<FPropertySoftPath> GetDisplayedProperties() const;

	void DiffAgainst(const FDetailsDiff& Newer, TArray< FSingleObjectDiffEntry > &OutDifferences) const;

private:
	void HandlePropertiesChanged();

	FOnDisplayedPropertiesChanged OnDisplayedPropertiesChanged;

	TArray< FPropertyPath > DifferingProperties;
	const UObject* DisplayedObject;

	TSharedPtr< class IDetailsView > DetailsView;
};
