// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"

class SWidget;

struct FExcerpt
{
	/** FExcerpt constructor */
	FExcerpt()
		: LineNumber( INDEX_NONE )
	{}

	/** FExcerpt overloaded constructor */
	FExcerpt( const FString& InName, const TSharedPtr< SWidget >& InContent, const TMap< FString, FString >& InVariables, int32 InLineNumber )
		: Name( InName )
		, Content( InContent )
		, Variables( InVariables )
		, LineNumber( InLineNumber )
	{}

	/** Excerpt name */
	FString Name;
	/** Slate content for excerpt */
	TSharedPtr<SWidget> Content;
	TMap< FString, FString > Variables;
	int32 LineNumber;
	/** Rich text version of the excerpt */
	FString RichText;
};

class IDocumentationPage
{
public:

	/** Returns true if this page contains a match for the ExcerptName */
	virtual bool HasExcerpt( const FString& ExcerptName ) = 0;
	/** Return the number of excerpts this page holds */
	virtual int32 GetNumExcerpts() const = 0;
	/** Populates the argument excerpt with this content found using the excer */
	virtual bool GetExcerpt( const FString& ExcerptName, FExcerpt& Excerpt) = 0;
	/** Populates the argument TArray with Excerpts this page contains */
	virtual void GetExcerpts( /*OUT*/ TArray< FExcerpt >& Excerpts ) = 0;
	/** Builds the Excerpt content using the excerpt name from the argument */
	virtual bool GetExcerptContent( FExcerpt& Excerpt ) = 0;

	/** Returns the title of the excerpt */
	virtual FText GetTitle() = 0;

	/** Rebuilds the excerpt content */
	virtual void Reload() = 0;

	/** Sets the argument as the width control for text wrap in the excerpt widgets */
	virtual void SetTextWrapAt( TAttribute<float> WrapAt ) = 0;

};
