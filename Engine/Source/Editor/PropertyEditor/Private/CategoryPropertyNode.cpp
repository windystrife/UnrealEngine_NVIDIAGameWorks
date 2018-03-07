// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "CategoryPropertyNode.h"
#include "Misc/ConfigCacheIni.h"
#include "ItemPropertyNode.h"
#include "ObjectEditorUtils.h"


FCategoryPropertyNode::FCategoryPropertyNode(void)
	: FPropertyNode()
{
}

FCategoryPropertyNode::~FCategoryPropertyNode(void)
{
}

bool FCategoryPropertyNode::IsSubcategory() const
{
	return GetParentNode() != NULL && const_cast<FPropertyNode*>( GetParentNode() )->AsCategoryNode() != NULL;
}

FText FCategoryPropertyNode::GetDisplayName() const 
{
	FString SubcategoryName = GetSubcategoryName();
	return FText::FromString( SubcategoryName );
}	
/**
 * Overridden function for special setup
 */
void FCategoryPropertyNode::InitBeforeNodeFlags()
{

}
/**
 * Overridden function for Creating Child Nodes
 */
void FCategoryPropertyNode::InitChildNodes()
{
	const bool bShowHiddenProperties = !!HasNodeFlags( EPropertyNodeFlags::ShouldShowHiddenProperties );
	const bool bShouldShowDisableEditOnInstance = !!HasNodeFlags(EPropertyNodeFlags::ShouldShowDisableEditOnInstance);

	TArray<UProperty*> Properties;
	// The parent of a category window has to be an object window.
	auto ComplexNode = FindComplexParent();
	if (ComplexNode)
	{
		// Get a list of properties that are in the same category
		for (TFieldIterator<UProperty> It(ComplexNode->GetBaseStructure()); It; ++It)
		{
			bool bMetaDataAllowVisible = true;
			if (!bShowHiddenProperties)
			{
				static const FName Name_bShowOnlyWhenTrue("bShowOnlyWhenTrue");
				const FString& MetaDataVisibilityCheckString = It->GetMetaData(Name_bShowOnlyWhenTrue);
				if (MetaDataVisibilityCheckString.Len())
				{
					//ensure that the metadata visibility string is actually set to true in order to show this property
					// @todo Remove this
					GConfig->GetBool(TEXT("UnrealEd.PropertyFilters"), *MetaDataVisibilityCheckString, bMetaDataAllowVisible, GEditorPerProjectIni);
				}
			}

			if (bMetaDataAllowVisible)
			{
				static const FName Name_InlineEditConditionToggle("InlineEditConditionToggle");
				const bool bOnlyShowAsInlineEditCondition = (*It)->HasMetaData(Name_InlineEditConditionToggle);
				const bool bShowIfEditableProperty = (*It)->HasAnyPropertyFlags(CPF_Edit);
				const bool bShowIfDisableEditOnInstance = !(*It)->HasAnyPropertyFlags(CPF_DisableEditOnInstance) || bShouldShowDisableEditOnInstance;

				// Add if we are showing non-editable props and this is the 'None' category, 
				// or if this is the right category, and we are either showing non-editable
				if (FObjectEditorUtils::GetCategoryFName(*It) == CategoryName && (bShowHiddenProperties || (bShowIfEditableProperty && !bOnlyShowAsInlineEditCondition && bShowIfDisableEditOnInstance)))
				{
					Properties.Add(*It);
				}
			}
		}
	}

	for( int32 PropertyIndex = 0 ; PropertyIndex < Properties.Num() ; ++PropertyIndex )
	{
		TSharedPtr<FItemPropertyNode> NewItemNode( new FItemPropertyNode );

		FPropertyNodeInitParams InitParams;
		InitParams.ParentNode = SharedThis(this);
		InitParams.Property = Properties[PropertyIndex];
		InitParams.ArrayOffset = 0;
		InitParams.ArrayIndex = INDEX_NONE;
		InitParams.bAllowChildren = true;
		InitParams.bForceHiddenPropertyVisibility = bShowHiddenProperties;
		InitParams.bCreateDisableEditOnInstanceNodes = bShouldShowDisableEditOnInstance;

		NewItemNode->InitNode( InitParams );

		AddChildNode(NewItemNode);
	}
}


/**
 * Appends my path, including an array index (where appropriate)
 */
bool FCategoryPropertyNode::GetQualifiedName( FString& PathPlusIndex, const bool bWithArrayIndex, const FPropertyNode* StopParent, bool bIgnoreCategories ) const
{
	bool bAddedAnything = false;

	if( ParentNode && StopParent != ParentNode )
	{
		bAddedAnything = ParentNode->GetQualifiedName(PathPlusIndex, bWithArrayIndex, StopParent, bIgnoreCategories );
		if( bAddedAnything )
		{
			PathPlusIndex += TEXT(".");
		}
	}
	
	if( !bIgnoreCategories )
	{
		bAddedAnything = true;
		GetCategoryName().AppendString(PathPlusIndex);
	}

	return bAddedAnything;
}


FString FCategoryPropertyNode::GetSubcategoryName() const 
{
	FString SubcategoryName;
	{
		// The category name may actually contain a path of categories.  When displaying this category
		// in the property window, we only want the leaf part of the path
		const FString& CategoryPath = GetCategoryName().ToString();
		FString CategoryDelimiterString;
		CategoryDelimiterString.AppendChar( FPropertyNodeConstants::CategoryDelimiterChar );  
		const int32 LastDelimiterCharIndex = CategoryPath.Find( CategoryDelimiterString, ESearchCase::IgnoreCase, ESearchDir::FromEnd );
		if( LastDelimiterCharIndex != INDEX_NONE )
		{
			// Grab the last sub-category from the path
			SubcategoryName = CategoryPath.Mid( LastDelimiterCharIndex + 1 );
		}
		else
		{
			// No sub-categories, so just return the original (clean) category path
			SubcategoryName = CategoryPath;
		}
	}
	return SubcategoryName;
}
