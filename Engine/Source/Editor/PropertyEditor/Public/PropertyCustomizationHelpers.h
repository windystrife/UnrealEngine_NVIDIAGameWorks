// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Misc/Attribute.h"
#include "Layout/Margin.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Framework/SlateDelegates.h"
#include "Materials/MaterialInterface.h"
#include "PropertyHandle.h"
#include "IDetailCustomNodeBuilder.h"
#include "DetailWidgetRow.h"
#include "SResetToDefaultMenu.h"
#include "ActorPickerMode.h"
#include "SceneDepthPickerMode.h"
#include "IDetailPropertyRow.h"

class AActor;
struct FAssetData;
class FAssetThumbnailPool;
class FMaterialItemView;
class FMaterialListBuilder;
class FPropertyEditor;
class IDetailChildrenBuilder;
class IDetailLayoutBuilder;
class IMaterialListBuilder;
class SPropertyEditorAsset;
class SPropertyEditorClass;
class UFactory;
class SToolTip;

namespace SceneOutliner
{
	struct FOutlinerFilters;
}


DECLARE_DELEGATE_OneParam(FOnAssetSelected, const FAssetData& /*AssetData*/);
DECLARE_DELEGATE_RetVal_OneParam(bool, FOnShouldSetAsset, const FAssetData& /*AssetData*/);
DECLARE_DELEGATE_RetVal_OneParam(bool, FOnShouldFilterAsset, const FAssetData& /*AssetData*/);
DECLARE_DELEGATE_OneParam( FOnGetActorFilters, TSharedPtr<SceneOutliner::FOutlinerFilters>& );
DECLARE_DELEGATE_ThreeParams(FOnGetPropertyComboBoxStrings, TArray< TSharedPtr<FString> >&, TArray<TSharedPtr<SToolTip>>&, TArray<bool>&);
DECLARE_DELEGATE_RetVal(FString, FOnGetPropertyComboBoxValue);
DECLARE_DELEGATE_OneParam(FOnPropertyComboBoxValueSelected, const FString&);

namespace PropertyCustomizationHelpers
{
	PROPERTYEDITOR_API TSharedRef<SWidget> MakeAddButton( FSimpleDelegate OnAddClicked, TAttribute<FText> OptionalToolTipText = FText(), TAttribute<bool> IsEnabled = true );
	PROPERTYEDITOR_API TSharedRef<SWidget> MakeRemoveButton( FSimpleDelegate OnRemoveClicked, TAttribute<FText> OptionalToolTipText = FText(), TAttribute<bool> IsEnabled = true );
	PROPERTYEDITOR_API TSharedRef<SWidget> MakeEmptyButton( FSimpleDelegate OnEmptyClicked, TAttribute<FText> OptionalToolTipText = FText(), TAttribute<bool> IsEnabled = true );
	PROPERTYEDITOR_API TSharedRef<SWidget> MakeInsertDeleteDuplicateButton( FExecuteAction OnInsertClicked, FExecuteAction OnDeleteClicked, FExecuteAction OnDuplicateClicked );
	PROPERTYEDITOR_API TSharedRef<SWidget> MakeDeleteButton( FSimpleDelegate OnDeleteClicked, TAttribute<FText> OptionalToolTipText = FText(), TAttribute<bool> IsEnabled = true );
	PROPERTYEDITOR_API TSharedRef<SWidget> MakeClearButton( FSimpleDelegate OnClearClicked, TAttribute<FText> OptionalToolTipText = FText(), TAttribute<bool> IsEnabled = true );
	PROPERTYEDITOR_API TSharedRef<SWidget> MakeNewBlueprintButton( FSimpleDelegate OnFindClicked, TAttribute<FText> OptionalToolTipText = FText(), TAttribute<bool> IsEnabled = true );
	PROPERTYEDITOR_API TSharedRef<SWidget> MakeUseSelectedButton( FSimpleDelegate OnUseSelectedClicked, TAttribute<FText> OptionalToolTipText = FText(), TAttribute<bool> IsEnabled = true );
	PROPERTYEDITOR_API TSharedRef<SWidget> MakeBrowseButton( FSimpleDelegate OnClearClicked, TAttribute<FText> OptionalToolTipText = FText(), TAttribute<bool> IsEnabled = true );
	PROPERTYEDITOR_API TSharedRef<SWidget> MakeAssetPickerAnchorButton( FOnGetAllowedClasses OnGetAllowedClasses, FOnAssetSelected OnAssetSelectedFromPicker );
	PROPERTYEDITOR_API TSharedRef<SWidget> MakeAssetPickerWithMenu( const FAssetData& InitialObject, const bool AllowClear, const TArray<const UClass*>& AllowedClasses, const TArray<UFactory*>& NewAssetFactories, FOnShouldFilterAsset OnShouldFilterAsset, FOnAssetSelected OnSet, FSimpleDelegate OnClose );
	PROPERTYEDITOR_API TSharedRef<SWidget> MakeActorPickerAnchorButton( FOnGetActorFilters OnGetActorFilters, FOnActorSelected OnActorSelectedFromPicker );
	PROPERTYEDITOR_API TSharedRef<SWidget> MakeActorPickerWithMenu( AActor* const InitialActor, const bool AllowClear, FOnShouldFilterActor ActorFilter, FOnActorSelected OnSet, FSimpleDelegate OnClose, FSimpleDelegate OnUseSelected );
	PROPERTYEDITOR_API TSharedRef<SWidget> MakeInteractiveActorPicker(FOnGetAllowedClasses OnGetAllowedClasses, FOnShouldFilterActor OnShouldFilterActor, FOnActorSelected OnActorSelectedFromPicker);
	PROPERTYEDITOR_API TSharedRef<SWidget> MakeSceneDepthPicker(FOnSceneDepthLocationSelected OnSceneDepthLocationSelected);
	PROPERTYEDITOR_API TSharedRef<SWidget> MakeEditConfigHierarchyButton(FSimpleDelegate OnEditConfigClicked, TAttribute<FText> OptionalToolTipText = FText(), TAttribute<bool> IsEnabled = true);
	PROPERTYEDITOR_API TSharedRef<SWidget> MakeDocumentationButton(const TSharedRef<FPropertyEditor>& InPropertyEditor);

	/** @return the UBoolProperty edit condition property if one exists. */
	PROPERTYEDITOR_API UBoolProperty* GetEditConditionProperty(const UProperty* InProperty, bool& bNegate);

	/** Returns a list of factories which can be used to create new assets, based on the supplied class */
	PROPERTYEDITOR_API TArray<UFactory*> GetNewAssetFactoriesForClasses(const TArray<const UClass*>& Classes);

	/** 
	 * Build a combo button that you bind to a Name or String property or use general delegates
	 * 
	 * @param InPropertyHandle	If set, will bind to a specific property. If this is null, all 3 delegates must be set
	 * @param OnGetStrings		Delegate that will generate the list of possible strings. If not set will generate using property handle
	 * @param OnGetValue		Delegate that is called to get the current string value to display. If not set will generate using property handle
	 * @param OnValueSelected	Delegate called when a string is selected. If not set will set the property handle
	 */
	PROPERTYEDITOR_API TSharedRef<SWidget> MakePropertyComboBox(const TSharedPtr<IPropertyHandle>& InPropertyHandle, FOnGetPropertyComboBoxStrings OnGetStrings = FOnGetPropertyComboBoxStrings(), FOnGetPropertyComboBoxValue OnGetValue = FOnGetPropertyComboBoxValue(), FOnPropertyComboBoxValueSelected OnValueSelected = FOnPropertyComboBoxValueSelected());
}


/** Delegate used to get a generic object */
DECLARE_DELEGATE_RetVal( const UObject*, FOnGetObject );

/** Delegate used to set a generic object */
DECLARE_DELEGATE_OneParam( FOnSetObject, const FAssetData& );


/**
 * Simulates an object property field 
 * Can be used when a property should act like a UObjectProperty but it isn't one
 */
class SObjectPropertyEntryBox : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SObjectPropertyEntryBox )
		: _AllowedClass( UObject::StaticClass() )
		, _AllowClear( true )
		, _DisplayUseSelected( true )
		, _DisplayBrowse( true )
		, _EnableContentPicker(true)
		, _DisplayCompactSize(false)
		, _DisplayThumbnail(true)
	{}
		/** The path to the object */
		SLATE_ATTRIBUTE( FString, ObjectPath )
		/** Optional property handle that can be used instead of the object path */
		SLATE_ARGUMENT( TSharedPtr<IPropertyHandle>, PropertyHandle )
		/** Thumbnail pool */
		SLATE_ARGUMENT( TSharedPtr<FAssetThumbnailPool>, ThumbnailPool )
		/** Class that is allowed in the asset picker */
		SLATE_ARGUMENT( UClass*, AllowedClass )
		/** Optional list of factories which may be used to create new assets */
		SLATE_ARGUMENT( TOptional<TArray<UFactory*>>, NewAssetFactories )
		/** Called to check if an asset should be set */
		SLATE_EVENT(FOnShouldSetAsset, OnShouldSetAsset)
		/** Called when the object value changes */
		SLATE_EVENT(FOnSetObject, OnObjectChanged)
		/** Called to check if an asset is valid to use */
		SLATE_EVENT(FOnShouldFilterAsset, OnShouldFilterAsset)
		/** Whether the asset can be 'None' */
		SLATE_ARGUMENT(bool, AllowClear)
		/** Whether to show the 'Use Selected' button */
		SLATE_ARGUMENT(bool, DisplayUseSelected)
		/** Whether to show the 'Browse' button */
		SLATE_ARGUMENT(bool, DisplayBrowse)
		/** Whether to enable the content Picker */
		SLATE_ARGUMENT(bool, EnableContentPicker)
		/** A custom reset to default override */
		SLATE_ARGUMENT(TOptional<FResetToDefaultOverride>, CustomResetToDefault)
		/** Whether or not to display a smaller, compact size for the asset thumbnail */ 
		SLATE_ARGUMENT(bool, DisplayCompactSize)
		/** Whether or not to display the asset thumbnail */ 
		SLATE_ARGUMENT(bool, DisplayThumbnail)
		/** A custom content slot for widgets */ 
		SLATE_NAMED_SLOT(FArguments, CustomContentSlot)
	SLATE_END_ARGS()

	PROPERTYEDITOR_API void Construct( const FArguments& InArgs );

private:
	/**
	 * Delegate function called when an object is changed
	 */
	void OnSetObject(const FAssetData& InObject);

	/** @return the object path for the object we are viewing */
	FString OnGetObjectPath() const;
private:
	/** Delegate to call to determine whether the asset should be set */
	FOnShouldSetAsset OnShouldSetAsset;
	/** Delegate to call when the object changes */
	FOnSetObject OnObjectChanged;
	/** Path to the object */
	TAttribute<FString> ObjectPath;
	/** Handle to a property we modify (if any)*/
	TSharedPtr<IPropertyHandle> PropertyHandle;
	/** The widget used to edit the object 'property' */
	TSharedPtr<SPropertyEditorAsset> PropertyEditorAsset;
};


/** Delegate used to set a class */
DECLARE_DELEGATE_OneParam( FOnSetClass, const UClass* );


/**
 * Simulates a class property field 
 * Can be used when a property should act like a UClassProperty but it isn't one
 */
class SClassPropertyEntryBox : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SClassPropertyEntryBox)
		: _MetaClass(UObject::StaticClass())
		, _RequiredInterface(nullptr)
		, _AllowAbstract(false)
		, _IsBlueprintBaseOnly(false)
		, _AllowNone(true)
		, _HideViewOptions(false)
		, _ShowTreeView(false)
	{}
		/** The meta class that the selected class must be a child-of (required) */
		SLATE_ARGUMENT(const UClass*, MetaClass)
		/** An interface that the selected class must implement (optional) */
		SLATE_ARGUMENT(const UClass*, RequiredInterface)
		/** Whether or not abstract classes are allowed (optional) */
		SLATE_ARGUMENT(bool, AllowAbstract)
		/** Should only base blueprints be displayed? (optional) */
		SLATE_ARGUMENT(bool, IsBlueprintBaseOnly)
		/** Should we be able to select "None" as a class? (optional) */
		SLATE_ARGUMENT(bool, AllowNone)
		/** Show the View Options part of the class picker dialog*/
		SLATE_ARGUMENT(bool, HideViewOptions)
		/** Show the class picker as a tree view rather than a list*/
		SLATE_ARGUMENT(bool, ShowTreeView)
		/** Attribute used to get the currently selected class (required) */
		SLATE_ATTRIBUTE(const UClass*, SelectedClass)
		/** Delegate used to set the currently selected class (required) */
		SLATE_EVENT(FOnSetClass, OnSetClass)
	SLATE_END_ARGS()

	PROPERTYEDITOR_API void Construct(const FArguments& InArgs);

private:
	/** The widget used to edit the class 'property' */
	TSharedPtr<SPropertyEditorClass> PropertyEditorClass;
};


/**
 * Represents a widget that can display a UProperty 
 * With the ability to customize the look of the property                 
 */
class SProperty
	: public SCompoundWidget
{
public:
	DECLARE_DELEGATE( FOnPropertyValueChanged );

	SLATE_BEGIN_ARGS( SProperty )
		: _ShouldDisplayName( true )
		, _DisplayResetToDefault( true )
		{}
		/** The display name to use in the default property widget */
		SLATE_ATTRIBUTE( FText, DisplayName )
		/** Whether or not to display the property name */
		SLATE_ARGUMENT( bool, ShouldDisplayName )
		/** The widget to display for this property instead of the default */
		SLATE_DEFAULT_SLOT( FArguments, CustomWidget )
		/** Whether or not to display the default reset to default button.  Note this value has no affect if overriding the widget */
		SLATE_ARGUMENT( bool, DisplayResetToDefault )
	SLATE_END_ARGS()

	virtual ~SProperty(){}
	PROPERTYEDITOR_API void Construct( const FArguments& InArgs, TSharedPtr<IPropertyHandle> InPropertyHandle );

	/**
	 * Resets the property to default                   
	 */
	PROPERTYEDITOR_API virtual void ResetToDefault();

	/**
	 * @return Whether or not the reset to default option should be visible                    
	 */
	PROPERTYEDITOR_API virtual bool ShouldShowResetToDefault() const;

	/**
	 * @return a label suitable for displaying in a reset to default menu
	 */
	PROPERTYEDITOR_API virtual FText GetResetToDefaultLabel() const;

	/**
	 * Returns whether or not this property is valid.  Sometimes property widgets are created even when their UProperty is not exposed to the user.  In that case the property is invalid
	 * Properties can also become invalid if selection changes in the detail view and this value is stored somewhere.
	 * @return Whether or not the property is valid.                   
	 */
	PROPERTYEDITOR_API virtual bool IsValidProperty() const;

protected:
	/** The handle being accessed by this widget */
	TSharedPtr<IPropertyHandle> PropertyHandle;
};


DECLARE_DELEGATE_ThreeParams( FOnGenerateArrayElementWidget, TSharedRef<IPropertyHandle>, int32, IDetailChildrenBuilder& );

class FDetailArrayBuilder
	: public IDetailCustomNodeBuilder
{
public:

	FDetailArrayBuilder(TSharedRef<IPropertyHandle> InBaseProperty, bool InGenerateHeader = true, bool InDisplayResetToDefault = true, bool InDisplayElementNum = true)
		: ArrayProperty( InBaseProperty->AsArray() )
		, BaseProperty( InBaseProperty )
		, bGenerateHeader( InGenerateHeader)
		, bDisplayResetToDefault(InDisplayResetToDefault)
		, bDisplayElementNum(InDisplayElementNum)
	{
		check( ArrayProperty.IsValid() );

		// Delegate for when the number of children in the array changes
		FSimpleDelegate OnNumChildrenChanged = FSimpleDelegate::CreateRaw( this, &FDetailArrayBuilder::OnNumChildrenChanged );
		ArrayProperty->SetOnNumElementsChanged( OnNumChildrenChanged );

		BaseProperty->MarkHiddenByCustomization();
	}
	
	~FDetailArrayBuilder()
	{
		FSimpleDelegate Empty;
		ArrayProperty->SetOnNumElementsChanged( Empty );
	}

	void SetDisplayName( const FText& InDisplayName )
	{
		DisplayName = InDisplayName;
	}

	void OnGenerateArrayElementWidget( FOnGenerateArrayElementWidget InOnGenerateArrayElementWidget )
	{
		OnGenerateArrayElementWidgetDelegate = InOnGenerateArrayElementWidget;
	}

	virtual bool RequiresTick() const override { return false; }

	virtual void Tick( float DeltaTime ) override {}

	virtual FName GetName() const override
	{
		return BaseProperty->GetProperty()->GetFName();
	}
	
	virtual bool InitiallyCollapsed() const override { return false; }

	virtual void GenerateHeaderRowContent( FDetailWidgetRow& NodeRow ) override
	{
		if (bGenerateHeader)
		{
			const bool bDisplayResetToDefaultInNameContent = false;

			TSharedPtr<SHorizontalBox> ContentHorizontalBox;
			SAssignNew(ContentHorizontalBox, SHorizontalBox);
			if (bDisplayElementNum)
			{
				ContentHorizontalBox->AddSlot()
				[
					BaseProperty->CreatePropertyValueWidget()
				];
			}

			NodeRow
			.FilterString(!DisplayName.IsEmpty() ? DisplayName : BaseProperty->GetPropertyDisplayName())
			.NameContent()
			[
				BaseProperty->CreatePropertyNameWidget(DisplayName, FText::GetEmpty(), bDisplayResetToDefaultInNameContent)
			]
			.ValueContent()
			[
				ContentHorizontalBox.ToSharedRef()
			];

			if (bDisplayResetToDefault)
			{
				TSharedPtr<SResetToDefaultMenu> ResetToDefaultMenu;
				ContentHorizontalBox->AddSlot()
				.AutoWidth()
				.Padding(FMargin(2.0f, 0.0f, 0.0f, 0.0f))
				[
					SAssignNew(ResetToDefaultMenu, SResetToDefaultMenu)
				];
				ResetToDefaultMenu->AddProperty(BaseProperty);
			}
		}
	}

	virtual void GenerateChildContent( IDetailChildrenBuilder& ChildrenBuilder ) override
	{
		uint32 NumChildren = 0;
		ArrayProperty->GetNumElements( NumChildren );

		for( uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex )
		{
			TSharedRef<IPropertyHandle> ElementHandle = ArrayProperty->GetElement( ChildIndex );

			OnGenerateArrayElementWidgetDelegate.Execute( ElementHandle, ChildIndex, ChildrenBuilder );
		}
	}

	virtual void RefreshChildren()
	{
		OnRebuildChildren.ExecuteIfBound();
	}

protected:

	virtual void SetOnRebuildChildren( FSimpleDelegate InOnRebuildChildren  ) override { OnRebuildChildren = InOnRebuildChildren; } 

	void OnNumChildrenChanged()
	{
		OnRebuildChildren.ExecuteIfBound();
	}

private:

	FText DisplayName;
	FOnGenerateArrayElementWidget OnGenerateArrayElementWidgetDelegate;
	TSharedPtr<IPropertyHandleArray> ArrayProperty;
	TSharedRef<IPropertyHandle> BaseProperty;
	FSimpleDelegate OnRebuildChildren;
	bool bGenerateHeader;
	bool bDisplayResetToDefault;
	bool bDisplayElementNum;
};


/**
 * Delegate called when we need to get new materials for the list
 */
DECLARE_DELEGATE_OneParam(FOnGetMaterials, IMaterialListBuilder&);

/**
 * Delegate called when a user changes the material
 */
DECLARE_DELEGATE_FourParams( FOnMaterialChanged, UMaterialInterface*, UMaterialInterface*, int32, bool );

DECLARE_DELEGATE_RetVal_TwoParams( TSharedRef<SWidget>, FOnGenerateWidgetsForMaterial, UMaterialInterface*, int32 );

DECLARE_DELEGATE_TwoParams( FOnResetMaterialToDefaultClicked, UMaterialInterface*, int32 );

DECLARE_DELEGATE_RetVal(bool, FOnMaterialListDirty);

DECLARE_DELEGATE_RetVal(bool, FOnCanCopyMaterialList);
DECLARE_DELEGATE(FOnCopyMaterialList);
DECLARE_DELEGATE(FOnPasteMaterialList);

DECLARE_DELEGATE_RetVal_OneParam(bool, FOnCanCopyMaterialItem, int32);
DECLARE_DELEGATE_OneParam(FOnCopyMaterialItem, int32);
DECLARE_DELEGATE_OneParam(FOnPasteMaterialItem, int32);

struct FMaterialListDelegates
{
	FMaterialListDelegates()
		: OnGetMaterials()
		, OnMaterialChanged()
		, OnGenerateCustomNameWidgets()
		, OnGenerateCustomMaterialWidgets()
		, OnResetMaterialToDefaultClicked()
	{}

	/** Delegate called to populate the list with materials */
	FOnGetMaterials OnGetMaterials;
	/** Delegate called when a user changes the material */
	FOnMaterialChanged OnMaterialChanged;
	/** Delegate called to generate custom widgets under the name of in the left column of a details panel*/
	FOnGenerateWidgetsForMaterial OnGenerateCustomNameWidgets;
	/** Delegate called to generate custom widgets under each material */
	FOnGenerateWidgetsForMaterial OnGenerateCustomMaterialWidgets;
	/** Delegate called when a material list item should be reset to default */
	FOnResetMaterialToDefaultClicked OnResetMaterialToDefaultClicked;
	/** Delegate called when we tick the material list to know if the list is dirty*/
	FOnMaterialListDirty OnMaterialListDirty;

	/** Delegate called Copying a material list */
	FOnCopyMaterialList OnCopyMaterialList;
	/** Delegate called to know if we can copy a material list */
	FOnCanCopyMaterialList OnCanCopyMaterialList;
	/** Delegate called Pasting a material list */
	FOnPasteMaterialList OnPasteMaterialList;

	/** Delegate called Copying a material item */
	FOnCopyMaterialItem OnCopyMaterialItem;
	/** Delegate called to know if we can copy a material item */
	FOnCanCopyMaterialItem OnCanCopyMaterialItem;
	/** Delegate called Pasting a material item */
	FOnPasteMaterialItem OnPasteMaterialItem;
};


/**
 * Builds up a list of unique materials while creating some information about the materials
 */
class IMaterialListBuilder
{
public:

	/** Virtual destructor. */
	virtual ~IMaterialListBuilder(){};

	/** 
	 * Adds a new material to the list
	 * 
	 * @param SlotIndex The slot (usually mesh element index) where the material is located on the component.
	 * @param Material The material being used.
	 * @param bCanBeReplced Whether or not the material can be replaced by a user.
	 */
	virtual void AddMaterial( uint32 SlotIndex, UMaterialInterface* Material, bool bCanBeReplaced ) = 0;
};


/**
 * A Material item in a material list slot
 */
struct FMaterialListItem
{
	/** Material being used */
	TWeakObjectPtr<UMaterialInterface> Material;

	/** Slot on a component where this material is at (mesh element) */
	int32 SlotIndex;

	/** Whether or not this material can be replaced by a new material */
	bool bCanBeReplaced;

	FMaterialListItem( UMaterialInterface* InMaterial = NULL, uint32 InSlotIndex = 0, bool bInCanBeReplaced = false )
		: Material( InMaterial )
		, SlotIndex( InSlotIndex )
		, bCanBeReplaced( bInCanBeReplaced )
	{}

	friend uint32 GetTypeHash( const FMaterialListItem& InItem )
	{
		return GetTypeHash( InItem.Material ) + InItem.SlotIndex ;
	}

	bool operator==( const FMaterialListItem& Other ) const
	{
		return Material == Other.Material && SlotIndex == Other.SlotIndex ;
	}

	bool operator!=( const FMaterialListItem& Other ) const
	{
		return !(*this == Other);
	}
};


class FMaterialList
	: public IDetailCustomNodeBuilder
	, public TSharedFromThis<FMaterialList>
{
public:
	PROPERTYEDITOR_API FMaterialList( IDetailLayoutBuilder& InDetailLayoutBuilder, FMaterialListDelegates& MaterialListDelegates, bool bInAllowCollapse = false, bool bInShowUsedTextures = true, bool bInDisplayCompactSize = false);

	/**
	 * @return true if materials are being displayed.                                                          
	 */
	bool IsDisplayingMaterials() const { return true; }

private:

	/**
	 * Called when a user expands all materials in a slot.
	 *
	 * @param SlotIndex The index of the slot being expanded.
	 */
	void OnDisplayMaterialsForElement( int32 SlotIndex );

	/**
	 * Called when a user hides all materials in a slot.
	 *
	 * @param SlotIndex The index of the slot being hidden.
	 */
	void OnHideMaterialsForElement( int32 SlotIndex );

	/** IDetailCustomNodeBuilder interface */
	virtual void SetOnRebuildChildren( FSimpleDelegate InOnRebuildChildren  ) override { OnRebuildChildren = InOnRebuildChildren; } 
	virtual bool RequiresTick() const override { return true; }
	virtual void Tick( float DeltaTime ) override;
	virtual void GenerateHeaderRowContent( FDetailWidgetRow& NodeRow ) override;
	virtual void GenerateChildContent( IDetailChildrenBuilder& ChildrenBuilder ) override;
	virtual FName GetName() const override { return NAME_None; }
	virtual bool InitiallyCollapsed() const override { return bAllowCollpase; }

	/**
	 * Adds a new material item to the list
	 *
	 * @param Row			The row to add the item to
	 * @param CurrentSlot	The slot id of the material
	 * @param Item			The material item to add
	 * @param bDisplayLink	If a link to the material should be displayed instead of the actual item (for multiple materials)
	 */
	void AddMaterialItem(FDetailWidgetRow& Row, int32 CurrentSlot, const FMaterialListItem& Item, bool bDisplayLink);

private:
	bool OnCanCopyMaterialList() const;
	void OnCopyMaterialList();
	void OnPasteMaterialList();

	bool OnCanCopyMaterialItem(int32 CurrentSlot) const;
	void OnCopyMaterialItem(int32 CurrentSlot);
	void OnPasteMaterialItem(int32 CurrentSlot);

	/** Delegates for the material list */
	FMaterialListDelegates MaterialListDelegates;

	/** Called to rebuild the children of the detail tree */
	FSimpleDelegate OnRebuildChildren;

	/** Parent detail layout this list is in */
	IDetailLayoutBuilder& DetailLayoutBuilder;

	/** Set of all unique displayed materials */
	TArray< FMaterialListItem > DisplayedMaterials;

	/** Set of all materials currently in view (may be less than DisplayedMaterials) */
	TArray< TSharedRef<FMaterialItemView> > ViewedMaterials;

	/** Set of all expanded slots */
	TSet<uint32> ExpandedSlots;

	/** Material list builder used to generate materials */
	TSharedRef<FMaterialListBuilder> MaterialListBuilder;

	/** Allow Collapse of material header row. Right now if you allow collapse, it will initially collapse. */
	bool bAllowCollpase;
	/** Whether or not to use the used textures menu for each material entry */
	bool bShowUsedTextures;
	/** Whether or not to display a compact form of material entry*/
	bool bDisplayCompactSize;
};


/**
 * Helper class to create a material slot name widget for material lists
 */
class SMaterialSlotWidget : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SMaterialSlotWidget)
	{}
		SLATE_ATTRIBUTE(FText, MaterialName)
		SLATE_EVENT(FOnTextChanged, OnMaterialNameChanged)
		SLATE_EVENT(FOnTextCommitted, OnMaterialNameCommitted)
		SLATE_ATTRIBUTE(bool, CanDeleteMaterialSlot)
		SLATE_EVENT(FSimpleDelegate, OnDeleteMaterialSlot)
	SLATE_END_ARGS()

	PROPERTYEDITOR_API void Construct(const FArguments& InArgs, int32 SlotIndex, bool bIsMaterialUsed);
};

//////////////////////////////////////////////////////////////////////////
//
// SECTION LIST

/**
* Delegate called when we need to get new sections for the list
*/
DECLARE_DELEGATE_OneParam(FOnGetSections, class ISectionListBuilder&);

/**
* Delegate called when a user changes the Section
*/
DECLARE_DELEGATE_FourParams(FOnSectionChanged, int32, int32, int32, FName);

DECLARE_DELEGATE_RetVal_TwoParams(TSharedRef<SWidget>, FOnGenerateWidgetsForSection, int32, int32);

DECLARE_DELEGATE_TwoParams(FOnResetSectionToDefaultClicked, int32, int32);

DECLARE_DELEGATE_RetVal_OneParam(TSharedRef<SWidget>, FOnGenerateLODComboBox, int32);

DECLARE_DELEGATE_RetVal(bool, FOnCanCopySectionList);
DECLARE_DELEGATE(FOnCopySectionList);
DECLARE_DELEGATE(FOnPasteSectionList);

DECLARE_DELEGATE_RetVal_TwoParams(bool, FOnCanCopySectionItem, int32, int32);
DECLARE_DELEGATE_TwoParams(FOnCopySectionItem, int32, int32);
DECLARE_DELEGATE_TwoParams(FOnPasteSectionItem, int32, int32);

struct FSectionListDelegates
{
	FSectionListDelegates()
		: OnGetSections()
		, OnSectionChanged()
		, OnGenerateCustomNameWidgets()
		, OnGenerateCustomSectionWidgets()
		, OnResetSectionToDefaultClicked()
		, OnGenerateLodComboBox()
	{}

	/** Delegate called to populate the list with Sections */
	FOnGetSections OnGetSections;
	/** Delegate called when a user changes the Section */
	FOnSectionChanged OnSectionChanged;
	/** Delegate called to generate custom widgets under the name of in the left column of a details panel*/
	FOnGenerateWidgetsForSection OnGenerateCustomNameWidgets;
	/** Delegate called to generate custom widgets under each Section */
	FOnGenerateWidgetsForSection OnGenerateCustomSectionWidgets;
	/** Delegate called when a Section list item should be reset to default */
	FOnResetSectionToDefaultClicked OnResetSectionToDefaultClicked;

	/** Delegate called when a Section list generate the LOD section combo box */
	FOnGenerateLODComboBox OnGenerateLodComboBox;

	/** Delegate called Copying a section list */
	FOnCopySectionList OnCopySectionList;
	/** Delegate called to know if we can copy a section list */
	FOnCanCopySectionList OnCanCopySectionList;
	/** Delegate called Pasting a section list */
	FOnPasteSectionList OnPasteSectionList;

	/** Delegate called Copying a section item */
	FOnCopySectionItem OnCopySectionItem;
	/** Delegate called To know if we can copy a section item */
	FOnCanCopySectionItem OnCanCopySectionItem;
	/** Delegate called Pasting a section item */
	FOnPasteSectionItem OnPasteSectionItem;
};

/**
* Builds up a list of unique Sections while creating some information about the Sections
*/
class ISectionListBuilder
{
public:
	virtual ~ISectionListBuilder() {};
	/**
	* Adds a new Section to the list
	*
	* @param SlotIndex		The slot (usually mesh element index) where the Section is located on the component
	* @param Section		The Section being used
	* @param bCanBeReplced	Whether or not the Section can be replaced by a user
	*/
	virtual void AddSection(int32 LodIndex, int32 SectionIndex, FName InMaterialSlotName, int32 InMaterialSlotIndex, FName InOriginalMaterialSlotName, const TMap<int32, FName> &InAvailableMaterialSlotName, const UMaterialInterface* Material, bool IsSectionUsingCloth) = 0;
};


/**
* A Section item in a Section list slot
*/
struct FSectionListItem
{
	/** LodIndex of the Section*/
	int32 LodIndex;
	/** Section index */
	int32 SectionIndex;

	/* Is this section is using cloth */
	bool IsSectionUsingCloth;

	/* Size of the preview material thumbnail */
	int32 ThumbnailSize;

	/** Material being readonly view in the list */
	TWeakObjectPtr<UMaterialInterface> Material;

	/* Material Slot Name */
	FName MaterialSlotName;
	int32 MaterialSlotIndex;
	FName OriginalMaterialSlotName;

	/* Available material slot name*/
	TMap<int32, FName> AvailableMaterialSlotName;


	FSectionListItem(int32 InLodIndex, int32 InSectionIndex, FName InMaterialSlotName, int32 InMaterialSlotIndex, FName InOriginalMaterialSlotName, const TMap<int32, FName> &InAvailableMaterialSlotName, const UMaterialInterface* InMaterial, bool InIsSectionUsingCloth, int32 InThumbnailSize)
		: LodIndex(InLodIndex)
		, SectionIndex(InSectionIndex)
		, IsSectionUsingCloth(InIsSectionUsingCloth)
		, ThumbnailSize(InThumbnailSize)
		, Material(InMaterial)
		, MaterialSlotName(InMaterialSlotName)
		, MaterialSlotIndex(InMaterialSlotIndex)
		, OriginalMaterialSlotName(InOriginalMaterialSlotName)
		, AvailableMaterialSlotName(InAvailableMaterialSlotName)
	{}

	bool operator==(const FSectionListItem& Other) const
	{
		bool IsSectionItemEqual = LodIndex == Other.LodIndex && SectionIndex == Other.SectionIndex && MaterialSlotIndex == Other.MaterialSlotIndex && MaterialSlotName == Other.MaterialSlotName && Material == Other.Material && AvailableMaterialSlotName.Num() == Other.AvailableMaterialSlotName.Num() && IsSectionUsingCloth == Other.IsSectionUsingCloth;
		if (IsSectionItemEqual)
		{
			for (auto Kvp : AvailableMaterialSlotName)
			{
				if (!Other.AvailableMaterialSlotName.Contains(Kvp.Key))
				{
					IsSectionItemEqual = false;
					break;
				}
				FName OtherName = *(Other.AvailableMaterialSlotName.Find(Kvp.Key));
				if (Kvp.Value != OtherName)
				{
					IsSectionItemEqual = false;
					break;
				}
			}
		}
		return IsSectionItemEqual;
	}

	bool operator!=(const FSectionListItem& Other) const
	{
		return !(*this == Other);
	}
};


class FSectionList : public IDetailCustomNodeBuilder, public TSharedFromThis<FSectionList>
{
public:
	PROPERTYEDITOR_API FSectionList(IDetailLayoutBuilder& InDetailLayoutBuilder, FSectionListDelegates& SectionListDelegates, bool bInAllowCollapse, int32 InThumbnailSize, int32 InSectionsLodIndex);

	/**
	* @return true if Sections are being displayed
	*/
	bool IsDisplayingSections() const { return true; }
private:
	/**
	* Called when a user expands all materials in a slot
	*/
	void OnDisplaySectionsForLod(int32 LodIndex);

	/**
	* Called when a user hides all materials in a slot
	*/
	void OnHideSectionsForLod(int32 LodIndex);

	/** IDetailCustomNodeBuilder interface */
	virtual void SetOnRebuildChildren(FSimpleDelegate InOnRebuildChildren) override { OnRebuildChildren = InOnRebuildChildren; }
	virtual bool RequiresTick() const override { return true; }
	virtual void Tick(float DeltaTime) override;
	virtual void GenerateHeaderRowContent(FDetailWidgetRow& NodeRow) override;
	virtual void GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder) override;
	virtual FName GetName() const override { return NAME_None; }
	virtual bool InitiallyCollapsed() const override { return bAllowCollpase; }

	/**
	* Adds a new Section item to the list
	*
	* @param Row			The row to add the item to
	* @param LodIndex		The Lod of the section
	* @param Item			The Section item to add
	* @param bDisplayLink	If a link to the Section should be displayed instead of the actual item (for multiple Sections)
	*/
	void AddSectionItem(class FDetailWidgetRow& Row, int32 LodIndex, const struct FSectionListItem& Item, bool bDisplayLink);

private:
	bool OnCanCopySectionList() const;
	void OnCopySectionList();
	void OnPasteSectionList();

	bool OnCanCopySectionItem(int32 LODIndex, int32 SectionIndex) const;
	void OnCopySectionItem(int32 LODIndex, int32 SectionIndex);
	void OnPasteSectionItem(int32 LODIndex, int32 SectionIndex);

	/** Delegates for the Section list */
	FSectionListDelegates SectionListDelegates;
	/** Called to rebuild the children of the detail tree */
	FSimpleDelegate OnRebuildChildren;
	/** Parent detail layout this list is in */
	IDetailLayoutBuilder& DetailLayoutBuilder;
	/** Set of all unique displayed Sections */
	TArray< FSectionListItem > DisplayedSections;
	/** Set of all Sections currently in view (may be less than DisplayedSections) */
	TArray< TSharedRef<class FSectionItemView> > ViewedSections;
	/** Set of all expanded slots */
	TSet<uint32> ExpandedSlots;
	/** Section list builder used to generate Sections */
	TSharedRef<class FSectionListBuilder> SectionListBuilder;
	/** Allow Collapse of Section header row. Right now if you allow collapse, it will initially collapse. */
	bool bAllowCollpase;

	int32 ThumbnailSize;
	int32 SectionsLodIndex;
};
