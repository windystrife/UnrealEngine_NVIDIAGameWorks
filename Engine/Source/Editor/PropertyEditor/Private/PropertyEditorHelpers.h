// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Widgets/SWidget.h"
#include "IPropertyUtilities.h"
#include "Widgets/SCompoundWidget.h"
#include "Framework/SlateDelegates.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "PropertyNode.h"
#include "Presentation/PropertyEditor/PropertyEditor.h"
#include "SDetailSingleItemRow.h"

class FNotifyHook;
class FObjectPropertyNode;

/** Property button enums. */
namespace EPropertyButton
{
	enum Type
	{
		Add,
		Empty,
		Insert_Delete_Duplicate,
		Insert_Delete,
		Insert,
		Delete,
		Duplicate,
		Browse,
		PickAsset,
		PickActor,
		PickActorInteractive,
		Clear,
		Use,
		NewBlueprint,
		EditConfigHierarchy,
		Documentation,
	};
}

class SPropertyNameWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SPropertyNameWidget )
		:_DisplayResetToDefault(true)
	{}
		SLATE_EVENT( FOnClicked, OnDoubleClicked )
		SLATE_ARGUMENT( bool, DisplayResetToDefault )
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, TSharedPtr<FPropertyEditor> PropertyEditor );

private:
	TSharedPtr< class FPropertyEditor > PropertyEditor;
};

class SPropertyValueWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SPropertyValueWidget )
		: _ShowPropertyButtons( true ),
		_OptionalResetWidget(SNullWidget::NullWidget)
	{}
		SLATE_ARGUMENT( bool, ShowPropertyButtons )
		SLATE_ARGUMENT( TSharedRef<SWidget>, OptionalResetWidget)
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, TSharedPtr<FPropertyEditor> InPropertyEditor, TSharedPtr<IPropertyUtilities> InPropertyUtilities );

	/** @return The minimum desired with if this property value */
	float GetMinDesiredWidth() const { return MinDesiredWidth; }

	/** @return The maximum desired with if this property value */
	float GetMaxDesiredWidth() const { return MaxDesiredWidth; }

	/** @return Whether this widget handles its own reset button */
	bool CreatedResetButton() const { return bCreatedResetButton; }

private:
	TSharedRef<SWidget> ConstructPropertyEditorWidget( TSharedPtr<FPropertyEditor>& PropertyEditor, TSharedPtr<IPropertyUtilities> InPropertyUtilities, TSharedRef<SWidget> InOptionalResetDefaultWidget = SNullWidget::NullWidget);
private:
	TSharedPtr< SWidget > ValueEditorWidget;
	/** The minimum desired with if this property value */
	float MinDesiredWidth;
	/** The maximum desired with if this property value */
	float MaxDesiredWidth;
	/** Whether or not this value widget handled its own reset button */
	bool bCreatedResetButton;
};


struct FCustomEditCondition
{
	TAttribute<bool> EditConditionValue;
	FOnBooleanValueChanged OnEditConditionValueChanged;
};

class SEditConditionWidget : public SCompoundWidget
{
	SLATE_BEGIN_ARGS( SEditConditionWidget )
	{}
		SLATE_ARGUMENT( FCustomEditCondition, CustomEditCondition )
	SLATE_END_ARGS()

	void Construct( const FArguments& Args, TSharedPtr<FPropertyEditor> InPropertyEditor );

private:
	void OnEditConditionCheckChanged( ECheckBoxState CheckState );
	bool HasEditCondition() const;
	ECheckBoxState OnGetEditConditionCheckState() const;
private:
	TSharedPtr<FPropertyEditor> PropertyEditor;
	FCustomEditCondition CustomEditCondition;
};

namespace PropertyEditorHelpers
{

	static bool IsPropertyButtonEnabled(TWeakPtr<FPropertyNode> PropertyNode);
	/**
	 * Returns whether or not a property is a built in struct property like a vector or color
	 *
	 * @return true if the property is built in type, false otherwise
	 */
	bool IsBuiltInStructProperty( const UProperty* Property );

	/**
	 * Returns whether or not a property is a child of an array (static or dynamic)
	 *
	 * @return true if the property is in an array, false otherwise
	 */
	bool IsChildOfArray( const FPropertyNode& InPropertyNode );

	/**
	 * @return true if the property is a child (within) a set, false otherwise
	 */
	bool IsChildOfSet( const FPropertyNode& InPropertyNode );

	/**
	 * @return true if the property is a child (within) a map, false otherwise
	 */
	bool IsChildOfMap( const FPropertyNode& InPropertyNode );
	
	/**
	 * Returns whether or not a property a static array
	 *
	 * @param InPropertyNode	The property node containing the property to check
	 */
	bool IsStaticArray( const FPropertyNode& InPropertyNode );
	/**
	 * Returns whether or not a property a static array
	 *
	 * @param InPropertyNode	The property node containing the property to check
	 */
	bool IsDynamicArray( const FPropertyNode& InPropertyNode );

	/**
	 * Gets the array parent of a property if it is in a dynamic or static array
	 */
	const UProperty* GetArrayParent( const FPropertyNode& InPropertyNode );

	/**
	 * Gets the set parent of a property if it is in a set
	 */
	const UProperty* GetSetParent( const FPropertyNode& InPropertyNode );

	/**
	 * Gets the map parent of a property if it is in a map
	 */
	const UProperty* GetMapParent(const FPropertyNode& InPropertyNode);

	/**
	 * Returns if a class is acceptable for edit inline
	 *
	 * @param CheckClass		The class to check
	 * @param bAllowAbstract	True if abstract classes are allowed
	 */
	bool IsEditInlineClassAllowed( UClass* CheckClass, bool bAllowAbstract );

	/**
	 * @return The text that represents the specified properties tooltip
	 */
	FText GetToolTipText( const UProperty* const Property );

	/**
	 * @return The link to the documentation that describes this property in detail
	 */
	FString GetDocumentationLink( const UProperty* const Property );

	/**
	* @return The link to the documentation that describes this enum property in detail
	*/
	FString GetEnumDocumentationLink(const UProperty* const Property);

	/**
	 * @return The name of the excerpt that describes this property in detail in the documentation file linked to this property
	 */
	FString GetDocumentationExcerptName( const UProperty* const Property );

	/**
	 * Gets a property handle for the specified property node
	 * 
	 * @param PropertyNode		The property node to get the handle from
	 * @param NotifyHook		Optional notify hook for the handle to use
	 */
	TSharedPtr<IPropertyHandle> GetPropertyHandle( TSharedRef<FPropertyNode> PropertyNode, FNotifyHook* NotifyHook, TSharedPtr<IPropertyUtilities> PropertyUtilities );

	/**
	 * Generates a list of required button types for the property
	 *
	 * @param PropertyNode			The property node to get required buttons from
	 * @param OutRequiredbuttons	The list of required buttons 
	 * @param bUsingAssetPicker		Whether or not the property editor is using the asset picker
	 */
	void GetRequiredPropertyButtons( TSharedRef<FPropertyNode> PropertyNode, TArray<EPropertyButton::Type>& OutRequiredButtons, bool bUsingAssetPicker = true );

	/**
	 * Makes property buttons widgets that accompany a property
	 *
	 * @param PropertyNode				The property node to make buttons for
	 * @param ButtonTypeToDelegateMap	A mapping of button type to the delegate to call when the button is clicked
	 * @param OutButtons				The populated list of button widgets
	 * @param ButtonsToIgnore			Specify those buttons which should be ignored and not added
	 * @param bUsingAssetPicker			Whether or not the property editor is using the asset picker
	 */
	void MakeRequiredPropertyButtons( const TSharedRef<FPropertyNode>& PropertyNode, const TSharedRef<IPropertyUtilities>& PropertyUtilities, TArray< TSharedRef<SWidget> >& OutButtons, const TArray<EPropertyButton::Type>& ButtonsToIgnore = TArray<EPropertyButton::Type>(), bool bUsingAssetPicker = true );
	void MakeRequiredPropertyButtons( const TSharedRef< FPropertyEditor >& PropertyEditor, TArray< TSharedRef<SWidget> >& OutButtons, const TArray<EPropertyButton::Type>& ButtonsToIgnore = TArray<EPropertyButton::Type>(), bool bUsingAssetPicker = true );

	TSharedRef<SWidget> MakePropertyButton( const EPropertyButton::Type ButtonType, const TSharedRef< FPropertyEditor >& PropertyEditor );
	TSharedRef<SWidget> MakePropertyReorderHandle(const TSharedRef<FPropertyNode>& PropertyNode, TSharedPtr<SDetailSingleItemRow> InParentRow);
	/**
	 * Recursively finds all object property nodes in a property tree
	 *
	 * @param StartNode	The node to start from
	 * @param OutObjectNodes	List of all object nodes found
	 */
	void CollectObjectNodes( TSharedPtr<FPropertyNode> StartNode, TArray<FObjectPropertyNode*>& OutObjectNodes );

	/**
	 * Returns any enums that are explicitly allowed by the "AllowedEnumValues" metadata on UProperty using the specified enum.
	 *
	 * @param Property	The property which may contain the "AllowedEnumValues" metadata
	 * @param InEnum	The enum to search
	 * @return The array of allowed enums.  NOTE: If an empty array is returned all enum values are allowed.  It is an error for a property to hide all enum values so that state is undefined here.
	 */
	TArray<FName> GetValidEnumsFromPropertyOverride(const UProperty* Property, const UEnum* InEnum);
	/**
	 * Whether or not a category is hidden by a given root object
	 * @param InRootNode	The root node that for the objects we are customizing
	 * @param CategoryName	The name of the category to check
	 * @return true if a category is hidden, false otherwise
	 */
	bool IsCategoryHiddenByClass(const TSharedPtr<FComplexPropertyNode>& InRootNode, FName CategoryName);
	
	/**
	 * Determines whether or not a property should be visible in the default generated detail layout
	 *
	 * @param PropertyNode	The property node to check
	 * @param ParentNode	The parent property node to check
	 * @return true if the property should be visible
	 */
	bool IsVisibleStandaloneProperty(const FPropertyNode& PropertyNode, const FPropertyNode& ParentNode);

}

