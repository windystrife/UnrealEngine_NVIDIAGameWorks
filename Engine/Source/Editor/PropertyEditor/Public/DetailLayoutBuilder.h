// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "EditorStyleSet.h"
#include "AssetThumbnail.h"
#include "PropertyHandle.h"
#include "IDetailPropertyRow.h"

class IDetailCategoryBuilder;

namespace ECategoryPriority
{
	enum Type
	{

		// Highest sort priority
		Variable = 0,
		Transform,
		Important,
		TypeSpecific,
		Default,
		// Lowest sort priority
		Uncommon,
	};
}

/**
 * The builder for laying custom details
 */
class IDetailLayoutBuilder
{
	
public:
	virtual ~IDetailLayoutBuilder(){}

	/**
	 * @return the font used for properties and details
	 */ 
	static FSlateFontInfo GetDetailFont() { return FEditorStyle::GetFontStyle( TEXT("PropertyWindow.NormalFont") ); }

	/**
	 * @return the bold font used for properties and details
	 */ 
	static FSlateFontInfo GetDetailFontBold() { return FEditorStyle::GetFontStyle( TEXT("PropertyWindow.BoldFont") ); }
	
	/**
	 * @return the italic font used for properties and details
	 */ 
	static FSlateFontInfo GetDetailFontItalic() { return FEditorStyle::GetFontStyle( TEXT("PropertyWindow.ItalicFont") ); }
	
	/**
	 * @return the parent detail view for this layout builder
	 */
	virtual const class IDetailsView* GetDetailsView() const = 0;

	/**
	 * @return The base class of the objects being customized in this detail layout
	 */
	virtual UClass* GetBaseClass() const = 0;
	
	/**
	 * Get the root objects observed by this layout.  
	 * This is not guaranteed to be the same as the objects customized by this builder.  See GetObjectsBeingCustomized for that.
	 */
	virtual const TArray< TWeakObjectPtr<UObject> >& GetSelectedObjects() const = 0;

	/**
	 * Gets the current object(s) being customized by this builder
	 *
	 * If this is a sub-object customization it will return those sub objects.  Otherwise the root objects will be returned.
	 */
	virtual void GetObjectsBeingCustomized( TArray< TWeakObjectPtr<UObject> >& OutObjects ) const = 0;

	/**
	 * Gets the current struct(s) being customized by this builder
	 *
	 * If this is a sub-struct customization it will return those sub struct.  Otherwise the root struct will be returned.
	 */
	virtual void GetStructsBeingCustomized( TArray< TSharedPtr<FStructOnScope> >& OutStructs ) const = 0;

	/**
	 *	@return the utilities various widgets need access to certain features of PropertyDetails
	 */
	virtual const TSharedRef< class IPropertyUtilities > GetPropertyUtilities() const = 0; 


	/**
	 * Edits an existing category or creates a new one
	 * 
	 * @param CategoryName				The name of the category
	 * @param NewLocalizedDisplayName	The new display name of the category (optional)
	 * @param CategoryType				Category type to define sort order.  Category display order is sorted by this type (optional)
	 */
	virtual IDetailCategoryBuilder& EditCategory( FName CategoryName, const FText& NewLocalizedDisplayName = FText::GetEmpty(), ECategoryPriority::Type CategoryType = ECategoryPriority::Default ) = 0;

	/** 
	 * Adds the property to its given category automatically. Useful in detail customizations which want to preserve categories.
	 * @param InPropertyHandle			The handle to the property that you want to add to its own category.
	 * @return the property row with which the property was added.
	 */
	virtual IDetailPropertyRow& AddPropertyToCategory(TSharedPtr<IPropertyHandle> InPropertyHandle) = 0;

	/**
	* Adds a custom row to the property's category automatically. Useful in detail customizations which want to preserve categories.
	* @param InPropertyHandle			The handle to the property that you want to add to its own category.
	* @param InCustomSearchString		A string which is used to filter this custom row when a user types into the details panel search box.
	* @return the detail widget that can be further customized. 
	*/
	virtual FDetailWidgetRow& AddCustomRowToCategory(TSharedPtr<IPropertyHandle> InPropertyHandle, const FText& InCustomSearchString, bool bForAdvanced = false) = 0;

	/**
	 * Hides an entire category
	 *
	 * @param CategoryName	The name of the category to hide
	 */
	virtual void HideCategory( FName CategoryName ) = 0;
	
	/**
	 * Gets a handle to a property which can be used to read and write the property value and identify the property in other detail customization interfaces.
	 * Instructions
	 *
	 * @param Path	The path to the property.  Can be just a name of the property or a path in the format outer.outer.value[optional_index_for_static_arrays]
	 * @param ClassOutermost	Optional outer class if accessing a property outside of the current class being customized
	 * @param InstanceName		Optional instance name if multiple UProperty's of the same type exist. such as two identical structs, the instance name is one of the struct variable names)
	    Examples:

		struct MyStruct
		{ 
			int32 StaticArray[3];
			float FloatVar;
		}

		class MyActor
		{ 
			MyStruct Struct1;
			MyStruct Struct2;
			float MyFloat
		}
		
		To access StaticArray at index 2 from Struct2 in MyActor, your path would be MyStruct.StaticArray[2]" and your instance name is "Struct2"
		To access MyFloat in MyActor you can just pass in "MyFloat" because the name of the property is unambiguous
	 */
	virtual TSharedRef<IPropertyHandle> GetProperty( const FName PropertyPath, const UClass* ClassOutermost = NULL, FName InstanceName = NAME_None )  = 0;

	/**
	 * Gets the top level property, for showing the warning for experimental or early access class
	 * 
	 * @return the top level property name
	 */

	virtual FName GetTopLevelProperty() = 0;

	/**
	 * Hides a property from view 
	 *
	 * @param PropertyHandle	The handle of the property to hide from view
	 */
	virtual void HideProperty( const TSharedPtr<IPropertyHandle> PropertyHandle ) = 0;

	/**
	 * Hides a property from view
	 *
	 * @param Path						The path to the property.  Can be just a name of the property or a path in the format outer.outer.value[optional_index_for_static_arrays]
	 * @param NewLocalizedDisplayName	Optional display name to show instead of the default name
	 * @param ClassOutermost			Optional outer class if accessing a property outside of the current class being customized
	 * @param InstanceName				Optional instance name if multiple UProperty's of the same type exist. such as two identical structs, the instance name is one of the struct variable names)
	 * See IDetailCategoryBuilder::GetProperty for clarification of parameters
	 */
	virtual void HideProperty( FName PropertyPath, const UClass* ClassOutermost = NULL, FName InstanceName = NAME_None ) = 0;

	/**
	 * Refreshes the details view and regenerates all the customized layouts
	 * Use only when you need to remove or add complicated dynamic items
	 */
	virtual void ForceRefreshDetails() = 0;

	/**
	 * Gets the thumbnail pool that should be used for rendering thumbnails in the details view                   
	 */
	virtual TSharedPtr<class FAssetThumbnailPool> GetThumbnailPool() const = 0;

	/**
	 * @return true if the property should be visible in the details panel or false if the specific details panel is not showing this property
	 */
	virtual bool IsPropertyVisible( TSharedRef<IPropertyHandle> PropertyHandle ) const = 0;

	/**
	 * @return true if the property should be visible in the details panel or false if the specific details panel is not showing this property
	 */
	virtual bool IsPropertyVisible( const struct FPropertyAndParent& PropertyAndParent ) const = 0;

	/**
	 * @return True if an object in the builder is a class default object
	 */
	virtual bool HasClassDefaultObject() const = 0;
};
