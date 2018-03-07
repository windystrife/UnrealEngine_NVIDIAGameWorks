// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "Widgets/SToolTip.h"

template< typename ObjectType > class TAttribute;

namespace FEditorClassUtils
{

	/**
	 * Gets the page that documentation for this class is contained on
	 *
	 * @param	InClass		Class we want to find the documentation page of
	 * @return				Path to the documentation page
	 */
	UNREALED_API FString GetDocumentationPage(const UClass* Class);

	/**
	 * Gets the excerpt to use for this class
	 * Excerpt will be contained on the page returned by GetDocumentationPage
	 *
	 * @param	InClass		Class we want to find the documentation excerpt of
	 * @return				Name of the to the documentation excerpt
	 */
	UNREALED_API FString GetDocumentationExcerpt(const UClass* Class);

	/**
	 * Gets the tooltip to display for a given class
	 *
	 * @param	InClass		Class we want to build a tooltip for
	 * @return				Shared reference to the constructed tooltip
	 */
	UNREALED_API TSharedRef<SToolTip> GetTooltip(const UClass* Class);

	/**
	 * Gets the tooltip to display for a given class with specified text for the tooltip
	 *
	 * @param	InClass			Class we want to build a tooltip for
	 * @param	OverrideText	The text to display on the standard tooltip
	 * @return					Shared reference to the constructed tooltip
	 */
	UNREALED_API TSharedRef<SToolTip> GetTooltip(const UClass* Class, const TAttribute<FText>& OverrideText);

	/**
	 * Returns the link path to the documentation for a given class
	 *
	 * @param	Class		Class we want to build a link for
	 * @return				The path to the documentation for the class
	 */
	UNREALED_API FString GetDocumentationLink(const UClass* Class, const FString& OverrideExcerpt = FString());


	/**
	 * Return link path from a specified excerpt
	 */
	UNREALED_API FString GetDocumentationLinkFromExcerpt(const FString& DocLink, const FString DocExcerpt);

	/**
	 * Creates a link widget to the documentation for a given class
	 *
	 * @param	Class		Class we want to build a link for
	 * @return				Shared pointer to the constructed tooltip
	 */
	UNREALED_API TSharedRef<SWidget> GetDocumentationLinkWidget(const UClass* Class);

	/**
	 * Creates a link to the source code or blueprint for a given class
	 *
	 * @param	Class			Class we want to build a link for
	 * @param	ObjectWeakPtr	Optional object to set blueprint debugging to in the case we are choosing a blueprint
	 * @return					Shared pointer to the constructed tooltip
	 */
	UNREALED_API TSharedRef<SWidget> GetSourceLink(const UClass* Class, const TWeakObjectPtr<UObject> ObjectWeakPtr);

	/**
	 * Creates a link to the source code or blueprint for a given class formatted however you need. Example "Edit {0}"
	 *
	 * @param	Class			Class we want to build a link for
	 * @param	ObjectWeakPtr	Optional object to set blueprint debugging to in the case we are choosing a blueprint
	 * @param	BlueprintFormat	The text format for blueprint links.
	 * @param	CodeFormat		The text format for C++ code file links.
	 * @return					Shared pointer to the constructed tooltip
	 */
	UNREALED_API TSharedRef<SWidget> GetSourceLinkFormatted(const UClass* Class, const TWeakObjectPtr<UObject> ObjectWeakPtr, const FText& BlueprintFormat, const FText& CodeFormat);

	/**
	 * Fetches a UClass from the string name of the class
	 *
	 * @param	ClassName		Name of the class we want the UClass for
	 * @return					UClass pointer if it exists
	 */
	UNREALED_API UClass* GetClassFromString(const FString& ClassName);
};
