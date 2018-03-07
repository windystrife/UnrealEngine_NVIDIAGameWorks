// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PropertyCustomizationHelpers.h"
#include "IDetailChildrenBuilder.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Engine/Texture.h"
#include "Factories/Factory.h"
#include "Editor.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "DetailLayoutBuilder.h"
#include "UserInterface/PropertyEditor/SPropertyAssetPicker.h"
#include "UserInterface/PropertyEditor/SPropertyMenuAssetPicker.h"
#include "UserInterface/PropertyEditor/SPropertySceneOutliner.h"
#include "UserInterface/PropertyEditor/SPropertyMenuActorPicker.h"
#include "Presentation/PropertyEditor/PropertyEditor.h"
#include "UserInterface/PropertyEditor/SPropertyEditorAsset.h"
#include "UserInterface/PropertyEditor/SPropertyEditorCombo.h"
#include "UserInterface/PropertyEditor/SPropertyEditorClass.h"
#include "UserInterface/PropertyEditor/SPropertyEditorInteractiveActorPicker.h"
#include "UserInterface/PropertyEditor/SPropertyEditorSceneDepthPicker.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "IDocumentation.h"
#include "SResetToDefaultPropertyEditor.h"

#define LOCTEXT_NAMESPACE "PropertyCustomizationHelpers"

namespace PropertyCustomizationHelpers
{
	class SPropertyEditorButton : public SButton
	{
	public:

		SLATE_BEGIN_ARGS( SPropertyEditorButton ) 
			: _Text( )
			, _Image( FEditorStyle::GetBrush("Default") )
			, _IsFocusable( true )
		{}
			SLATE_ARGUMENT( FText, Text )
			SLATE_ARGUMENT( const FSlateBrush*, Image )
			SLATE_EVENT( FSimpleDelegate, OnClickAction )

			/** Sometimes a button should only be mouse-clickable and never keyboard focusable. */
			SLATE_ARGUMENT( bool, IsFocusable )
		SLATE_END_ARGS()

		void Construct( const FArguments& InArgs )
		{
			OnClickAction = InArgs._OnClickAction;

			SButton::FArguments ButtonArgs = SButton::FArguments()
				.ButtonStyle( FEditorStyle::Get(), "HoverHintOnly" )
				.OnClicked( this, &SPropertyEditorButton::OnClick )
				.ToolTipText( InArgs._Text )
				.ContentPadding( 4.0f )
				.ForegroundColor( FSlateColor::UseForeground() )
				.IsFocusable(InArgs._IsFocusable)
				[ 
					SNew( SImage )
					.Image( InArgs._Image )
					.ColorAndOpacity( FSlateColor::UseForeground() )
				]; 

			SButton::Construct( ButtonArgs );
		}


	private:
		FReply OnClick()
		{
			OnClickAction.ExecuteIfBound();
			return FReply::Handled();
		}
	private:
		FSimpleDelegate OnClickAction;
	};

	TSharedRef<SWidget> MakeAddButton( FSimpleDelegate OnAddClicked, TAttribute<FText> OptionalToolTipText, TAttribute<bool> IsEnabled )
	{
		return	
			SNew( SPropertyEditorButton )
			.Text( LOCTEXT( "AddButtonLabel", "Add" ) )
			.ToolTipText( OptionalToolTipText.Get().IsEmpty() ? LOCTEXT( "AddButtonToolTipText", "Adds Element") : OptionalToolTipText )
			.Image( FEditorStyle::GetBrush("PropertyWindow.Button_AddToArray") )
			.OnClickAction( OnAddClicked )
			.IsEnabled(IsEnabled)
			.IsFocusable( false );
	}

	TSharedRef<SWidget> MakeRemoveButton( FSimpleDelegate OnRemoveClicked, TAttribute<FText> OptionalToolTipText, TAttribute<bool> IsEnabled )
	{
		return	
			SNew( SPropertyEditorButton )
			.Text( LOCTEXT( "RemoveButtonLabel", "Remove" ) )
			.ToolTipText( OptionalToolTipText.Get().IsEmpty() ? LOCTEXT( "RemoveButtonToolTipText", "Removes Element") : OptionalToolTipText )
			.Image( FEditorStyle::GetBrush("PropertyWindow.Button_RemoveFromArray") )
			.OnClickAction( OnRemoveClicked )
			.IsEnabled(IsEnabled)
			.IsFocusable( false );
	}

	TSharedRef<SWidget> MakeEmptyButton( FSimpleDelegate OnEmptyClicked, TAttribute<FText> OptionalToolTipText, TAttribute<bool> IsEnabled )
	{
		return
			SNew( SPropertyEditorButton )
			.Text( LOCTEXT( "EmptyButtonLabel", "Empty" ) )
			.ToolTipText( OptionalToolTipText.Get().IsEmpty() ? LOCTEXT( "EmptyButtonToolTipText", "Removes All Elements") : OptionalToolTipText )
			.Image( FEditorStyle::GetBrush("PropertyWindow.Button_EmptyArray") )
			.OnClickAction( OnEmptyClicked )
			.IsEnabled(IsEnabled)
			.IsFocusable( false );
	}

	TSharedRef<SWidget> MakeUseSelectedButton( FSimpleDelegate OnUseSelectedClicked, TAttribute<FText> OptionalToolTipText, TAttribute<bool> IsEnabled )
	{
		return
			SNew( SPropertyEditorButton )
			.Text( LOCTEXT( "UseButtonLabel", "Use") )
			.ToolTipText( OptionalToolTipText.Get().IsEmpty() ? LOCTEXT( "UseButtonToolTipText", "Use Selected Asset from Content Browser") : OptionalToolTipText )
			.Image( FEditorStyle::GetBrush("PropertyWindow.Button_Use") )
			.OnClickAction( OnUseSelectedClicked )
			.IsEnabled(IsEnabled)
			.IsFocusable( false );
	}

	TSharedRef<SWidget> MakeDeleteButton( FSimpleDelegate OnDeleteClicked, TAttribute<FText> OptionalToolTipText, TAttribute<bool> IsEnabled )
	{
		return
			SNew( SPropertyEditorButton )
			.Text( LOCTEXT( "DeleteButtonLabel", "Delete") )
			.ToolTipText( OptionalToolTipText.Get().IsEmpty() ? LOCTEXT( "DeleteButtonToolTipText", "Delete") : OptionalToolTipText )
			.Image( FEditorStyle::GetBrush("PropertyWindow.Button_Delete") )
			.OnClickAction( OnDeleteClicked )
			.IsEnabled(IsEnabled)
			.IsFocusable( false );
	}

	TSharedRef<SWidget> MakeClearButton( FSimpleDelegate OnClearClicked, TAttribute<FText> OptionalToolTipText, TAttribute<bool> IsEnabled )
	{
		return
			SNew( SPropertyEditorButton )
			.Text( LOCTEXT( "ClearButtonLabel", "Clear") )
			.ToolTipText( OptionalToolTipText.Get().IsEmpty() ? LOCTEXT( "ClearButtonToolTipText", "Clear Path") : OptionalToolTipText )
			.Image( FEditorStyle::GetBrush("PropertyWindow.Button_Clear") )
			.OnClickAction( OnClearClicked )
			.IsEnabled(IsEnabled)
			.IsFocusable( false );
	}

	TSharedRef<SWidget> MakeBrowseButton( FSimpleDelegate OnFindClicked, TAttribute<FText> OptionalToolTipText, TAttribute<bool> IsEnabled )
	{
		return
			SNew( SPropertyEditorButton )
			.Text( LOCTEXT( "BrowseButtonLabel", "Browse") )
			.ToolTipText( OptionalToolTipText.Get().IsEmpty() ? LOCTEXT( "BrowseButtonToolTipText", "Browse to Asset in Content Browser") : OptionalToolTipText )
			.Image( FEditorStyle::GetBrush("PropertyWindow.Button_Browse") )
			.OnClickAction( OnFindClicked )
			.IsEnabled(IsEnabled)
			.IsFocusable( false );
	}

	TSharedRef<SWidget> MakeNewBlueprintButton( FSimpleDelegate OnFindClicked, TAttribute<FText> OptionalToolTipText, TAttribute<bool> IsEnabled )
	{
		return
			SNew( SPropertyEditorButton )
			.Text( LOCTEXT( "NewBlueprintButtonLabel", "New Blueprint") )
			.ToolTipText( OptionalToolTipText.Get().IsEmpty() ? LOCTEXT( "NewBlueprintButtonToolTipText", "Create New Blueprint") : OptionalToolTipText )
			.Image( FEditorStyle::GetBrush("PropertyWindow.Button_CreateNewBlueprint") )
			.OnClickAction( OnFindClicked )
			.IsEnabled(IsEnabled)
			.IsFocusable( false );
	}

	TSharedRef<SWidget> MakeInsertDeleteDuplicateButton(FExecuteAction OnInsertClicked, FExecuteAction OnDeleteClicked, FExecuteAction OnDuplicateClicked)
	{
		FMenuBuilder MenuContentBuilder( true, nullptr, nullptr, true );
		{
			if (OnInsertClicked.IsBound())
			{
				FUIAction InsertAction(OnInsertClicked);
				MenuContentBuilder.AddMenuEntry(LOCTEXT("InsertButtonLabel", "Insert"), FText::GetEmpty(), FSlateIcon(), InsertAction);
			}

			if (OnDeleteClicked.IsBound())
			{
				FUIAction DeleteAction(OnDeleteClicked);
				MenuContentBuilder.AddMenuEntry(LOCTEXT("DeleteButtonLabel", "Delete"), FText::GetEmpty(), FSlateIcon(), DeleteAction);
			}

			if (OnDuplicateClicked.IsBound())
			{
				FUIAction DuplicateAction( OnDuplicateClicked );
				MenuContentBuilder.AddMenuEntry( LOCTEXT( "DuplicateButtonLabel", "Duplicate"), FText::GetEmpty(), FSlateIcon(), DuplicateAction );
			}
		}

		return
			SNew(SComboButton)
			.ButtonStyle( FEditorStyle::Get(), "HoverHintOnly" )
			.ContentPadding(2)
			.ForegroundColor( FSlateColor::UseForeground() )
			.HasDownArrow(true)
			.MenuContent()
			[
				MenuContentBuilder.MakeWidget()
			];
	}

	TSharedRef<SWidget> MakeAssetPickerAnchorButton( FOnGetAllowedClasses OnGetAllowedClasses, FOnAssetSelected OnAssetSelectedFromPicker )
	{
		return 
			SNew( SPropertyAssetPicker )
			.OnGetAllowedClasses( OnGetAllowedClasses )
			.OnAssetSelected( OnAssetSelectedFromPicker );
	}

	TSharedRef<SWidget> MakeAssetPickerWithMenu( const FAssetData& InitialObject, const bool AllowClear, const TArray<const UClass*>& AllowedClasses, const TArray<UFactory*>& NewAssetFactories, FOnShouldFilterAsset OnShouldFilterAsset, FOnAssetSelected OnSet, FSimpleDelegate OnClose)
	{
		return
			SNew(SPropertyMenuAssetPicker)
			.InitialObject(InitialObject)
			.AllowClear(AllowClear)
			.AllowedClasses(AllowedClasses)
			.NewAssetFactories(NewAssetFactories)
			.OnShouldFilterAsset(OnShouldFilterAsset)
			.OnSet(OnSet)
			.OnClose(OnClose);
	}

	TSharedRef<SWidget> MakeActorPickerAnchorButton( FOnGetActorFilters OnGetActorFilters, FOnActorSelected OnActorSelectedFromPicker )
	{
		return 
			SNew( SPropertySceneOutliner )
			.OnGetActorFilters( OnGetActorFilters )
			.OnActorSelected( OnActorSelectedFromPicker );
	}

	TSharedRef<SWidget> MakeActorPickerWithMenu( AActor* const InitialActor, const bool AllowClear, FOnShouldFilterActor ActorFilter, FOnActorSelected OnSet, FSimpleDelegate OnClose, FSimpleDelegate OnUseSelected )
	{
		return 
			SNew( SPropertyMenuActorPicker )
			.InitialActor(InitialActor)
			.AllowClear(AllowClear)
			.ActorFilter(ActorFilter)
			.OnSet(OnSet)
			.OnClose(OnClose)
			.OnUseSelected(OnUseSelected);
	}

	TSharedRef<SWidget> MakeInteractiveActorPicker( FOnGetAllowedClasses OnGetAllowedClasses, FOnShouldFilterActor OnShouldFilterActor, FOnActorSelected OnActorSelectedFromPicker )
	{
		return 
			SNew( SPropertyEditorInteractiveActorPicker )
			.ToolTipText( LOCTEXT( "PickButtonLabel", "Pick Actor from scene") )
			.OnGetAllowedClasses( OnGetAllowedClasses )
			.OnShouldFilterActor( OnShouldFilterActor )
			.OnActorSelected( OnActorSelectedFromPicker );
	}

	TSharedRef<SWidget> MakeSceneDepthPicker(FOnSceneDepthLocationSelected OnSceneDepthLocationSelected)
	{
		return
			SNew(SPropertyEditorSceneDepthPicker)
			.ToolTipText(LOCTEXT("PickSceneDepthLabel", "Sample Scene Depth from scene"))
			.OnSceneDepthLocationSelected(OnSceneDepthLocationSelected);
	}

	TSharedRef<SWidget> MakeEditConfigHierarchyButton(FSimpleDelegate OnEditConfigClicked, TAttribute<FText> OptionalToolTipText, TAttribute<bool> IsEnabled)
	{
		return
			SNew(SPropertyEditorButton)
			.Text(LOCTEXT("EditConfigHierarchyButtonLabel", "Edit Config Hierarchy"))
			.ToolTipText(OptionalToolTipText.Get().IsEmpty() ? LOCTEXT("EditConfigHierarchyButtonToolTipText", "Edit the config values of this property") : OptionalToolTipText)
			.Image(FEditorStyle::GetBrush("DetailsView.EditConfigProperties"))
			.OnClickAction(OnEditConfigClicked)
			.IsEnabled(IsEnabled)
			.IsFocusable(false);
	}

	TSharedRef<SWidget> MakeDocumentationButton(const TSharedRef<FPropertyEditor>& InPropertyEditor)
	{
		TSharedPtr<IPropertyHandle> PropertyHandle = InPropertyEditor->GetPropertyHandle();

		FString DocLink;
		FString DocExcerptName;

		if (PropertyHandle.IsValid() && PropertyHandle->HasDocumentation())
		{
			DocLink = PropertyHandle->GetDocumentationLink();
			DocExcerptName = PropertyHandle->GetDocumentationExcerptName();
		}
		else
		{
			DocLink = InPropertyEditor->GetDocumentationLink();
			DocExcerptName = InPropertyEditor->GetDocumentationExcerptName();
		}

		return IDocumentation::Get()->CreateAnchor(DocLink, FString(), DocExcerptName);
	}

	UBoolProperty* GetEditConditionProperty(const UProperty* InProperty, bool& bNegate)
	{
		UBoolProperty* EditConditionProperty = NULL;
		bNegate = false;

		if ( InProperty != NULL )
		{
			// find the name of the property that should be used to determine whether this property should be editable
			FString ConditionPropertyName = InProperty->GetMetaData(TEXT("EditCondition"));

			// Support negated edit conditions whose syntax is !BoolProperty
			if ( ConditionPropertyName.StartsWith(FString(TEXT("!"))) )
			{
				bNegate = true;
				// Chop off the negation from the property name
				ConditionPropertyName = ConditionPropertyName.Right(ConditionPropertyName.Len() - 1);
			}

			// for now, only support boolean conditions, and only allow use of another property within the same struct as the conditional property
			if ( ConditionPropertyName.Len() > 0 && !ConditionPropertyName.Contains(TEXT(".")) )
			{
				UStruct* Scope = InProperty->GetOwnerStruct();
				EditConditionProperty = FindField<UBoolProperty>(Scope, *ConditionPropertyName);
			}
		}

		return EditConditionProperty;
	}

	TArray<UFactory*> GetNewAssetFactoriesForClasses(const TArray<const UClass*>& Classes)
	{
		TArray<UFactory*> Factories;
		for (TObjectIterator<UClass> It; It; ++It)
		{
			UClass* Class = *It;
			if (Class->IsChildOf(UFactory::StaticClass()) && !Class->HasAnyClassFlags(CLASS_Abstract))
			{
				UFactory* Factory = Class->GetDefaultObject<UFactory>();
				if (Factory->ShouldShowInNewMenu() && ensure(!Factory->GetDisplayName().IsEmpty()))
				{
					UClass* SupportedClass = Factory->GetSupportedClass();
					if (SupportedClass != nullptr && Classes.ContainsByPredicate([=](const UClass* InClass) { return SupportedClass->IsChildOf(InClass); }))
					{
						Factories.Add(Factory);
					}
				}
			}
		}

		Factories.Sort([](UFactory& A, UFactory& B) -> bool
		{
			return A.GetDisplayName().CompareToCaseIgnored(B.GetDisplayName()) < 0;
		});

		return Factories;
	}
}

void SObjectPropertyEntryBox::Construct( const FArguments& InArgs )
{
	ObjectPath = InArgs._ObjectPath;
	OnObjectChanged = InArgs._OnObjectChanged;
	OnShouldSetAsset = InArgs._OnShouldSetAsset;

	bool bDisplayThumbnail = InArgs._DisplayThumbnail;
	FIntPoint ThumbnailSize(64, 64);

	if( InArgs._PropertyHandle.IsValid() && InArgs._PropertyHandle->IsValidHandle() )
	{
		PropertyHandle = InArgs._PropertyHandle;

		// check if the property metadata wants us to display a thumbnail
		const FString& DisplayThumbnailString = PropertyHandle->GetProperty()->GetMetaData(TEXT("DisplayThumbnail"));
		if(DisplayThumbnailString.Len() > 0)
		{
			bDisplayThumbnail = DisplayThumbnailString == TEXT("true");
		}

		// check if the property metadata has an override to the thumbnail size
		const FString& ThumbnailSizeString = PropertyHandle->GetProperty()->GetMetaData(TEXT("ThumbnailSize"));
		if ( ThumbnailSizeString.Len() > 0 )
		{
			FVector2D ParsedVector;
			if ( ParsedVector.InitFromString(ThumbnailSizeString) )
			{
				ThumbnailSize.X = (int32)ParsedVector.X;
				ThumbnailSize.Y = (int32)ParsedVector.Y;
			}
		}

		// if being used with an object property, check the allowed class is valid for the property
		UObjectPropertyBase* ObjectProperty = Cast<UObjectPropertyBase>(PropertyHandle->GetProperty());
		if (ObjectProperty != NULL)
		{
			checkSlow(InArgs._AllowedClass->IsChildOf(ObjectProperty->PropertyClass));
		}
	}

	TSharedPtr<SResetToDefaultPropertyEditor> ResetButton = nullptr;

	if (InArgs._CustomResetToDefault.IsSet() || (PropertyHandle.IsValid() && !PropertyHandle->HasMetaData(TEXT("NoResetToDefault")) && !PropertyHandle->IsResetToDefaultCustomized()))
	{
		SAssignNew(ResetButton, SResetToDefaultPropertyEditor, PropertyHandle)
			.IsEnabled(true)
			.CustomResetToDefault(InArgs._CustomResetToDefault);		
	};

	TSharedRef<SWidget> ResetWidget = ResetButton.IsValid() ? ResetButton.ToSharedRef() : SNullWidget::NullWidget;

	ChildSlot
	[	
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.FillWidth(1)
		.VAlign(VAlign_Center)	
		[
			SAssignNew(PropertyEditorAsset, SPropertyEditorAsset)
				.ObjectPath( this, &SObjectPropertyEntryBox::OnGetObjectPath )
				.Class( InArgs._AllowedClass )
				.NewAssetFactories( InArgs._NewAssetFactories )
				.OnSetObject(this, &SObjectPropertyEntryBox::OnSetObject)
				.ThumbnailPool(InArgs._ThumbnailPool)
				.DisplayThumbnail(bDisplayThumbnail)
				.OnShouldFilterAsset(InArgs._OnShouldFilterAsset)
				.AllowClear(InArgs._AllowClear)
				.DisplayUseSelected(InArgs._DisplayUseSelected)
				.DisplayBrowse(InArgs._DisplayBrowse)
				.EnableContentPicker(InArgs._EnableContentPicker)
				.PropertyHandle(PropertyHandle)
				.ThumbnailSize(ThumbnailSize)
				.DisplayCompactSize(InArgs._DisplayCompactSize)
				.CustomContentSlot()
				[
					InArgs._CustomContentSlot.Widget
				]
				.ResetToDefaultSlot()
				[
					ResetWidget
				]
		]
	];
}

FString SObjectPropertyEntryBox::OnGetObjectPath() const
{
	FString StringReference;
	if (ObjectPath.IsSet())
	{
		StringReference = ObjectPath.Get();
	}
	else if( PropertyHandle.IsValid() )
	{
		PropertyHandle->GetValueAsFormattedString( StringReference );
	}
	
	return StringReference;
}

void SObjectPropertyEntryBox::OnSetObject(const FAssetData& AssetData)
{
	if( PropertyHandle.IsValid() && PropertyHandle->IsValidHandle() )
	{
		if (!OnShouldSetAsset.IsBound() || OnShouldSetAsset.Execute(AssetData))
		{
			FString ObjectPathName = TEXT("None");
			if (AssetData.IsValid())
			{
				ObjectPathName = AssetData.ObjectPath.ToString();
			}

			PropertyHandle->SetValueFromFormattedString(ObjectPathName);
		}
	}
	OnObjectChanged.ExecuteIfBound(AssetData);
}

void SClassPropertyEntryBox::Construct(const FArguments& InArgs)
{
	ChildSlot
	[	
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		[
			SAssignNew(PropertyEditorClass, SPropertyEditorClass)
				.MetaClass(InArgs._MetaClass)
				.RequiredInterface(InArgs._RequiredInterface)
				.AllowAbstract(InArgs._AllowAbstract)
				.IsBlueprintBaseOnly(InArgs._IsBlueprintBaseOnly)
				.AllowNone(InArgs._AllowNone)
				.ShowViewOptions(!InArgs._HideViewOptions)
				.ShowTree(InArgs._ShowTreeView)
				.SelectedClass(InArgs._SelectedClass)
				.OnSetClass(InArgs._OnSetClass)
		]
	];
}

void SProperty::Construct( const FArguments& InArgs, TSharedPtr<IPropertyHandle> InPropertyHandle )
{
	TSharedPtr<SWidget> ChildSlotContent;

	const FText& DisplayName = InArgs._DisplayName.Get();

	PropertyHandle = InPropertyHandle;

	if( PropertyHandle->IsValidHandle() )
	{
		InPropertyHandle->MarkHiddenByCustomization();

		if( InArgs._CustomWidget.Widget != SNullWidget::NullWidget )
		{
			TSharedRef<SWidget> CustomWidget = InArgs._CustomWidget.Widget;

			// If the name should be displayed create it now
			if( InArgs._ShouldDisplayName )
			{
				CustomWidget = 
					SNew( SHorizontalBox )
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Right)
					.Padding( 4.0f, 0.0f )
					.FillWidth(1.0f)
					[
						InPropertyHandle->CreatePropertyNameWidget( DisplayName )
					]
					+ SHorizontalBox::Slot()
					.Padding( 0.0f, 0.0f )
					.VAlign(VAlign_Center)
					.FillWidth(1.0f)
					[
						CustomWidget
					];
			}

			ChildSlotContent = CustomWidget;
		}
		else
		{
			if( InArgs._ShouldDisplayName )
			{
				ChildSlotContent = 
					SNew( SHorizontalBox )
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Right)
					.Padding( 3.0f, 0.0f )
					.FillWidth(1.0f)
					[
						InPropertyHandle->CreatePropertyNameWidget( DisplayName )
					]
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.FillWidth(1.0f)
					[
						InPropertyHandle->CreatePropertyValueWidget()
					];
			}
			else
			{
				ChildSlotContent = InPropertyHandle->CreatePropertyValueWidget();
			}
		}
	}
	else
	{
		// The property was not found, just filter out this widget completely
		// Note a spacer widget is used instead of setting the visibility of this widget in the case that a user overrides the visibility of this widget
		ChildSlotContent = 
			SNew( SSpacer )
			.Visibility( EVisibility::Collapsed );
	}
	
	ChildSlot
	[
		ChildSlotContent.ToSharedRef()
	];
}

void SProperty::ResetToDefault()
{
	if( PropertyHandle->IsValidHandle() )
	{
		PropertyHandle->ResetToDefault();
	}
}

FText SProperty::GetResetToDefaultLabel() const
{
	if( PropertyHandle->IsValidHandle() )
	{
		return PropertyHandle->GetResetToDefaultLabel();
	}

	return FText();
}

bool SProperty::ShouldShowResetToDefault() const
{
	return PropertyHandle->IsValidHandle() && !PropertyHandle->IsEditConst() && PropertyHandle->DiffersFromDefault();
}

bool SProperty::IsValidProperty() const
{
	return PropertyHandle.IsValid() && PropertyHandle->IsValidHandle();
}

/**
 * Builds up a list of unique materials while creating some information about the materials
 */
class FMaterialListBuilder : public IMaterialListBuilder
{
	friend class FMaterialList;
public:

	/** 
	 * Adds a new material to the list
	 * 
	 * @param SlotIndex		The slot (usually mesh element index) where the material is located on the component
	 * @param Material		The material being used
	 * @param bCanBeReplced	Whether or not the material can be replaced by a user
	 */
	virtual void AddMaterial( uint32 SlotIndex, UMaterialInterface* Material, bool bCanBeReplaced ) override
	{
		int32 NumMaterials = MaterialSlots.Num();

		FMaterialListItem MaterialItem( Material, SlotIndex, bCanBeReplaced ); 
		if( !UniqueMaterials.Contains( MaterialItem ) ) 
		{
			MaterialSlots.Add( MaterialItem );
			UniqueMaterials.Add( MaterialItem );
		}

		// Did we actually add material?  If we did then we need to increment the number of materials in the element
		if( MaterialSlots.Num() > NumMaterials )
		{
			// Resize the array to support the slot if needed
			if( !MaterialCount.IsValidIndex(SlotIndex) )
			{
				int32 NumToAdd = (SlotIndex - MaterialCount.Num()) + 1;
				if( NumToAdd > 0 )
				{
					MaterialCount.AddZeroed( NumToAdd );
				}
			}

			++MaterialCount[SlotIndex];
		}
	}

	/** Empties the list */
	void Empty()
	{
		UniqueMaterials.Empty();
		MaterialSlots.Reset();
		MaterialCount.Reset();
	}

	/** Sorts the list by slot index */
	void Sort()
	{
		struct FSortByIndex
		{
			bool operator()( const FMaterialListItem& A, const FMaterialListItem& B ) const
			{
				return A.SlotIndex < B.SlotIndex;
			}
		};

		MaterialSlots.Sort( FSortByIndex() );
	}

	/** @return The number of materials in the list */
	uint32 GetNumMaterials() const { return MaterialSlots.Num(); }

	/** @return The number of materials in the list at a given slot */
	uint32 GetNumMaterialsInSlot( uint32 Index ) const { return MaterialCount[Index]; }
private:
	/** All unique materials */
	TSet<FMaterialListItem> UniqueMaterials;
	/** All material items in the list */
	TArray<FMaterialListItem> MaterialSlots;
	/** Material counts for each slot.  The slot is the index and the value at that index is the count */
	TArray<uint32> MaterialCount;
};

/**
 * A view of a single item in an FMaterialList
 */
class FMaterialItemView : public TSharedFromThis<FMaterialItemView>
{
public:
	/**
	 * Creates a new instance of this class
	 *
	 * @param Material				The material to view
	 * @param InOnMaterialChanged	Delegate for when the material changes
	 */
	static TSharedRef<FMaterialItemView> Create(
		const FMaterialListItem& Material, 
		FOnMaterialChanged InOnMaterialChanged,
		FOnGenerateWidgetsForMaterial InOnGenerateNameWidgetsForMaterial, 
		FOnGenerateWidgetsForMaterial InOnGenerateWidgetsForMaterial, 
		FOnResetMaterialToDefaultClicked InOnResetToDefaultClicked,
		int32 InMultipleMaterialCount,
		bool bShowUsedTextures,
		bool bDisplayCompactSize)
	{
		return MakeShareable( new FMaterialItemView( Material, InOnMaterialChanged, InOnGenerateNameWidgetsForMaterial, InOnGenerateWidgetsForMaterial, InOnResetToDefaultClicked, InMultipleMaterialCount, bShowUsedTextures, bDisplayCompactSize) );
	}

	TSharedRef<SWidget> CreateNameContent()
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("ElementIndex"), MaterialItem.SlotIndex);

		return 
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.VAlign(VAlign_Center)
			[
				SNew( STextBlock )
				.Font( IDetailLayoutBuilder::GetDetailFont() )
				.Text( FText::Format(LOCTEXT("ElementIndex", "Element {ElementIndex}"), Arguments ) )
			]
			+SVerticalBox::Slot()
			.Padding(0.0f,4.0f)
			.AutoHeight()
			[
				OnGenerateCustomNameWidgets.IsBound() ? OnGenerateCustomNameWidgets.Execute( MaterialItem.Material.Get(), MaterialItem.SlotIndex ) : StaticCastSharedRef<SWidget>( SNullWidget::NullWidget )
			];
	}

	TSharedRef<SWidget> CreateValueContent( const TSharedPtr<FAssetThumbnailPool>& ThumbnailPool )
	{
		FIntPoint ThumbnailSize(64, 64);

		FResetToDefaultOverride ResetToDefaultOverride = FResetToDefaultOverride::Create(
			FIsResetToDefaultVisible::CreateSP(this, &FMaterialItemView::GetReplaceVisibility),
			FResetToDefaultHandler::CreateSP(this, &FMaterialItemView::OnResetToBaseClicked)
		);

		return
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding( 0.0f )
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Fill)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew( SObjectPropertyEntryBox )
						.ObjectPath(MaterialItem.Material->GetPathName())
						.AllowedClass(UMaterialInterface::StaticClass())
						.OnObjectChanged(this, &FMaterialItemView::OnSetObject)
						.ThumbnailPool(ThumbnailPool)
						.DisplayCompactSize(bDisplayCompactSize)
						.CustomResetToDefault(ResetToDefaultOverride)
						.CustomContentSlot()
						[
							SNew( SBox )
							.HAlign(HAlign_Left)
							.VAlign(VAlign_Center)
							[
								SNew(SHorizontalBox)
								+SHorizontalBox::Slot()
								.VAlign(VAlign_Center)
								.Padding(0.0f, 0.0f, 3.0f, 0.0f)
								.AutoWidth()
								[
									// Add a menu for displaying all textures 
									SNew( SComboButton )
									.OnGetMenuContent( this, &FMaterialItemView::OnGetTexturesMenuForMaterial )
									.VAlign(VAlign_Center)
									.ContentPadding(2)
									.IsEnabled( this, &FMaterialItemView::IsTexturesMenuEnabled )
									.Visibility( bShowUsedTextures ? EVisibility::Visible : EVisibility::Hidden )
									.ButtonContent()
									[
										SNew( STextBlock )
										.Font( IDetailLayoutBuilder::GetDetailFont() )
										.ToolTipText( LOCTEXT("ViewTexturesToolTip", "View the textures used by this material" ) )
										.Text( LOCTEXT("ViewTextures","Textures") )
									]
								]
								+SHorizontalBox::Slot()
								.Padding(3.0f, 0.0f)
								.FillWidth(1.0f)
								[
									OnGenerateCustomMaterialWidgets.IsBound() && bDisplayCompactSize ? OnGenerateCustomMaterialWidgets.Execute(MaterialItem.Material.Get(), MaterialItem.SlotIndex) : StaticCastSharedRef<SWidget>(SNullWidget::NullWidget)
								]
							]
						]
					]
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(2)
				.VAlign( VAlign_Center )
				[
					OnGenerateCustomMaterialWidgets.IsBound() && !bDisplayCompactSize ? OnGenerateCustomMaterialWidgets.Execute( MaterialItem.Material.Get(), MaterialItem.SlotIndex ) : StaticCastSharedRef<SWidget>( SNullWidget::NullWidget )
				]
			];
	}

private:

	FMaterialItemView(	const FMaterialListItem& InMaterial, 
						FOnMaterialChanged& InOnMaterialChanged, 
						FOnGenerateWidgetsForMaterial& InOnGenerateNameWidgets, 
						FOnGenerateWidgetsForMaterial& InOnGenerateMaterialWidgets, 
						FOnResetMaterialToDefaultClicked& InOnResetToDefaultClicked,
						int32 InMultipleMaterialCount,
						bool bInShowUsedTextures,
						bool bInDisplayCompactSize)
						
		: MaterialItem( InMaterial )
		, OnMaterialChanged( InOnMaterialChanged )
		, OnGenerateCustomNameWidgets( InOnGenerateNameWidgets )
		, OnGenerateCustomMaterialWidgets( InOnGenerateMaterialWidgets )
		, OnResetToDefaultClicked( InOnResetToDefaultClicked )
		, MultipleMaterialCount( InMultipleMaterialCount )
		, bShowUsedTextures( bInShowUsedTextures )
		, bDisplayCompactSize(bInDisplayCompactSize)
	{

	}

	void ReplaceMaterial( UMaterialInterface* NewMaterial, bool bReplaceAll = false )
	{
		UMaterialInterface* PrevMaterial = NULL;
		if( MaterialItem.Material.IsValid() )
		{
			PrevMaterial = MaterialItem.Material.Get();
		}

		if( NewMaterial != PrevMaterial )
		{
			// Replace the material
			OnMaterialChanged.ExecuteIfBound( NewMaterial, PrevMaterial, MaterialItem.SlotIndex, bReplaceAll );
		}
	}

	void OnSetObject( const FAssetData& AssetData )
	{
		const bool bReplaceAll = false;

		UMaterialInterface* NewMaterial = Cast<UMaterialInterface>(AssetData.GetAsset());
		ReplaceMaterial( NewMaterial, bReplaceAll );
	}

	/**
	 * @return Whether or not the textures menu is enabled
	 */
	bool IsTexturesMenuEnabled() const
	{
		return MaterialItem.Material.Get() != NULL;
	}

	TSharedRef<SWidget> OnGetTexturesMenuForMaterial()
	{
		FMenuBuilder MenuBuilder( true, NULL );

		if( MaterialItem.Material.IsValid() )
		{
			UMaterialInterface* Material = MaterialItem.Material.Get();

			TArray< UTexture* > Textures;
			Material->GetUsedTextures(Textures, EMaterialQualityLevel::Num, false, ERHIFeatureLevel::Num, true);

			// Add a menu item for each texture.  Clicking on the texture will display it in the content browser
			for( int32 TextureIndex = 0; TextureIndex < Textures.Num(); ++TextureIndex )
			{
				// UObject for delegate compatibility
				UObject* Texture = Textures[TextureIndex];

				FUIAction Action( FExecuteAction::CreateSP( this, &FMaterialItemView::GoToAssetInContentBrowser, TWeakObjectPtr<UObject>(Texture) ) );

				MenuBuilder.AddMenuEntry( FText::FromString( Texture->GetName() ), LOCTEXT( "BrowseTexture_ToolTip", "Find this texture in the content browser" ), FSlateIcon(), Action );
			}
		}

		return MenuBuilder.MakeWidget();
	}

	/**
	 * Finds the asset in the content browser
	 */
	void GoToAssetInContentBrowser( TWeakObjectPtr<UObject> Object )
	{
		if( Object.IsValid() )
		{
			TArray< UObject* > Objects;
			Objects.Add( Object.Get() );
			GEditor->SyncBrowserToObjects( Objects );
		}
	}

	/**
	 * Called to get the visibility of the replace button
	 */
	bool GetReplaceVisibility(TSharedPtr<IPropertyHandle> PropertyHandle) const
	{
		// Only show the replace button if the current material can be replaced
		if (OnMaterialChanged.IsBound() && MaterialItem.bCanBeReplaced)
		{
			return true;
		}

		return false;
	}

	/**
	 * Called when reset to base is clicked
	 */
	void OnResetToBaseClicked(TSharedPtr<IPropertyHandle> PropertyHandle)
	{
		// Only allow reset to base if the current material can be replaced
		if( MaterialItem.Material.IsValid() && MaterialItem.bCanBeReplaced )
		{
			bool bReplaceAll = false;
			ReplaceMaterial( NULL, bReplaceAll );
			OnResetToDefaultClicked.ExecuteIfBound( MaterialItem.Material.Get(), MaterialItem.SlotIndex );
		}
	}

private:
	FMaterialListItem MaterialItem;
	FOnMaterialChanged OnMaterialChanged;
	FOnGenerateWidgetsForMaterial OnGenerateCustomNameWidgets;
	FOnGenerateWidgetsForMaterial OnGenerateCustomMaterialWidgets;
	FOnResetMaterialToDefaultClicked OnResetToDefaultClicked;
	int32 MultipleMaterialCount;
	bool bShowUsedTextures;
	bool bDisplayCompactSize;
};


FMaterialList::FMaterialList(IDetailLayoutBuilder& InDetailLayoutBuilder, FMaterialListDelegates& InMaterialListDelegates, bool bInAllowCollapse, bool bInShowUsedTextures, bool bInDisplayCompactSize)
	: MaterialListDelegates( InMaterialListDelegates )
	, DetailLayoutBuilder( InDetailLayoutBuilder )
	, MaterialListBuilder( new FMaterialListBuilder )
	, bAllowCollpase(bInAllowCollapse)
	, bShowUsedTextures(bInShowUsedTextures)
	, bDisplayCompactSize(bInDisplayCompactSize)
{
}

void FMaterialList::OnDisplayMaterialsForElement( int32 SlotIndex )
{
	// We now want to display all the materials in the element
	ExpandedSlots.Add( SlotIndex );

	MaterialListBuilder->Empty();
	MaterialListDelegates.OnGetMaterials.ExecuteIfBound( *MaterialListBuilder );

	OnRebuildChildren.ExecuteIfBound();
}

void FMaterialList::OnHideMaterialsForElement( int32 SlotIndex )
{
	// No longer want to expand the element
	ExpandedSlots.Remove( SlotIndex );

	// regenerate the materials
	MaterialListBuilder->Empty();
	MaterialListDelegates.OnGetMaterials.ExecuteIfBound( *MaterialListBuilder );
	
	OnRebuildChildren.ExecuteIfBound();
}


void FMaterialList::Tick( float DeltaTime )
{
	// Check each material to see if its still valid.  This allows the material list to stay up to date when materials are changed out from under us
	if( MaterialListDelegates.OnGetMaterials.IsBound() )
	{
		// Whether or not to refresh the material list
		bool bRefrestMaterialList = false;

		// Get the current list of materials from the user
		MaterialListBuilder->Empty();
		MaterialListDelegates.OnGetMaterials.ExecuteIfBound( *MaterialListBuilder );

		if( MaterialListBuilder->GetNumMaterials() != DisplayedMaterials.Num() )
		{
			// The array sizes differ so we need to refresh the list
			bRefrestMaterialList = true;
		}
		else
		{
			// Compare the new list against the currently displayed list
			for( int32 MaterialIndex = 0; MaterialIndex < MaterialListBuilder->MaterialSlots.Num(); ++MaterialIndex )
			{
				const FMaterialListItem& Item = MaterialListBuilder->MaterialSlots[MaterialIndex];

				// The displayed materials is out of date if there isn't a 1:1 mapping between the material sets
				if( !DisplayedMaterials.IsValidIndex( MaterialIndex ) || DisplayedMaterials[ MaterialIndex ] != Item )
				{
					bRefrestMaterialList = true;
					break;
				}
			}
		}

		if (!bRefrestMaterialList && MaterialListDelegates.OnMaterialListDirty.IsBound())
		{
			bRefrestMaterialList = MaterialListDelegates.OnMaterialListDirty.Execute();
		}

		if( bRefrestMaterialList )
		{
			OnRebuildChildren.ExecuteIfBound();
		}
	}
}

void FMaterialList::GenerateHeaderRowContent( FDetailWidgetRow& NodeRow )
{
	NodeRow.CopyAction(FUIAction(FExecuteAction::CreateSP(this, &FMaterialList::OnCopyMaterialList), FCanExecuteAction::CreateSP(this, &FMaterialList::OnCanCopyMaterialList)));
	NodeRow.PasteAction(FUIAction(FExecuteAction::CreateSP(this, &FMaterialList::OnPasteMaterialList)));

	if (bAllowCollpase)
	{
		NodeRow.NameContent()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("MaterialHeaderTitle", "Materials"))
				.Font(IDetailLayoutBuilder::GetDetailFont())
			];
	}
}

void FMaterialList::GenerateChildContent( IDetailChildrenBuilder& ChildrenBuilder )
{
	ViewedMaterials.Empty();
	DisplayedMaterials.Empty();
	if( MaterialListBuilder->GetNumMaterials() > 0 )
	{
		DisplayedMaterials = MaterialListBuilder->MaterialSlots;

		MaterialListBuilder->Sort();
		TArray<FMaterialListItem>& MaterialSlots = MaterialListBuilder->MaterialSlots;

		int32 CurrentSlot = INDEX_NONE;
		bool bDisplayAllMaterialsInSlot = true;
		for( auto It = MaterialSlots.CreateConstIterator(); It; ++It )
		{
			const FMaterialListItem& Material = *It;

			if( CurrentSlot != Material.SlotIndex )
			{
				// We've encountered a new slot.  Make a widget to display that
				CurrentSlot = Material.SlotIndex;

				uint32 NumMaterials = MaterialListBuilder->GetNumMaterialsInSlot(CurrentSlot);

				// If an element is expanded we want to display all its materials
				bool bWantToDisplayAllMaterials = NumMaterials > 1 && ExpandedSlots.Contains(CurrentSlot);

				// If we are currently displaying an expanded set of materials for an element add a link to collapse all of them
				if( bWantToDisplayAllMaterials )
				{
					FDetailWidgetRow& ChildRow = ChildrenBuilder.AddCustomRow( LOCTEXT( "HideAllMaterialSearchString", "Hide All Materials") );

					FFormatNamedArguments Arguments;
					Arguments.Add(TEXT("ElementSlot"), CurrentSlot);
					ChildRow
					.ValueContent()
					.MaxDesiredWidth(0.0f)// No Max Width
					[
						SNew( SBox )
							.HAlign( HAlign_Center )
							[
								SNew( SHyperlink )
									.TextStyle( FEditorStyle::Get(), "MaterialList.HyperlinkStyle" )
									.Text( FText::Format(LOCTEXT("HideAllMaterialLinkText", "Hide All Materials on Element {ElementSlot}"), Arguments ) )
									.OnNavigate( this, &FMaterialList::OnHideMaterialsForElement, CurrentSlot )
							]
					];
				}	

				if( NumMaterials > 1 && !bWantToDisplayAllMaterials )
				{
					// The current slot has multiple elements to view
					bDisplayAllMaterialsInSlot = false;

					FDetailWidgetRow& ChildRow = ChildrenBuilder.AddCustomRow( FText::GetEmpty() );

					AddMaterialItem( ChildRow, CurrentSlot, FMaterialListItem( NULL, CurrentSlot, true ), !bDisplayAllMaterialsInSlot );
				}
				else
				{
					bDisplayAllMaterialsInSlot = true;
				}

			}

			// Display each thumbnail element unless we shouldn't display multiple materials for one slot
			if( bDisplayAllMaterialsInSlot )
			{
				FDetailWidgetRow& ChildRow = ChildrenBuilder.AddCustomRow( Material.Material.IsValid()? FText::FromString(Material.Material->GetName()) : FText::GetEmpty() );

				AddMaterialItem( ChildRow, CurrentSlot, Material, !bDisplayAllMaterialsInSlot );
			}
		}
	}
	else
	{
		FDetailWidgetRow& ChildRow = ChildrenBuilder.AddCustomRow( LOCTEXT("NoMaterials", "No Materials") );

		ChildRow
		[
			SNew( SBox )
			.HAlign( HAlign_Center )
			[
				SNew( STextBlock )
				.Text( LOCTEXT("NoMaterials", "No Materials") ) 
				.Font( IDetailLayoutBuilder::GetDetailFont() )
			]
		];
	}		
}

bool FMaterialList::OnCanCopyMaterialList() const
{
	if (MaterialListDelegates.OnCanCopyMaterialList.IsBound())
	{
		return MaterialListDelegates.OnCanCopyMaterialList.Execute();
	}

	return false;
}

void FMaterialList::OnCopyMaterialList()
{
	if (MaterialListDelegates.OnCopyMaterialList.IsBound())
	{
		MaterialListDelegates.OnCopyMaterialList.Execute();
	}
}

void FMaterialList::OnPasteMaterialList()
{
	if (MaterialListDelegates.OnPasteMaterialList.IsBound())
	{
		MaterialListDelegates.OnPasteMaterialList.Execute();
	}
}

bool FMaterialList::OnCanCopyMaterialItem(int32 CurrentSlot) const
{
	if (MaterialListDelegates.OnCanCopyMaterialItem.IsBound())
	{
		return MaterialListDelegates.OnCanCopyMaterialItem.Execute(CurrentSlot);
	}

	return false;
}

void FMaterialList::OnCopyMaterialItem(int32 CurrentSlot)
{
	if (MaterialListDelegates.OnCopyMaterialItem.IsBound())
	{
		MaterialListDelegates.OnCopyMaterialItem.Execute(CurrentSlot);
	}
}

void FMaterialList::OnPasteMaterialItem(int32 CurrentSlot)
{
	if (MaterialListDelegates.OnPasteMaterialItem.IsBound())
	{
		MaterialListDelegates.OnPasteMaterialItem.Execute(CurrentSlot);
	}
}
			
void FMaterialList::AddMaterialItem( FDetailWidgetRow& Row, int32 CurrentSlot, const FMaterialListItem& Item, bool bDisplayLink )
{
	uint32 NumMaterials = MaterialListBuilder->GetNumMaterialsInSlot(CurrentSlot);

	TSharedRef<FMaterialItemView> NewView = FMaterialItemView::Create( Item, MaterialListDelegates.OnMaterialChanged, MaterialListDelegates.OnGenerateCustomNameWidgets, MaterialListDelegates.OnGenerateCustomMaterialWidgets, MaterialListDelegates.OnResetMaterialToDefaultClicked, NumMaterials, bShowUsedTextures, bDisplayCompactSize);

	TSharedPtr<SWidget> RightSideContent;
	if( bDisplayLink )
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("NumMaterials"), NumMaterials);

		RightSideContent = 
			SNew( SBox )
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Top)
				[
					SNew( SHyperlink )
					.TextStyle( FEditorStyle::Get(), "MaterialList.HyperlinkStyle" )
					.Text( FText::Format(LOCTEXT("DisplayAllMaterialLinkText", "Display {NumMaterials} materials"), Arguments) )
					.ToolTipText( LOCTEXT("DisplayAllMaterialLink_ToolTip","Display all materials. Drag and drop a material here to replace all materials.") )
					.OnNavigate( this, &FMaterialList::OnDisplayMaterialsForElement, CurrentSlot )
				];
	}
	else
	{
		RightSideContent = NewView->CreateValueContent( DetailLayoutBuilder.GetThumbnailPool() );
		ViewedMaterials.Add( NewView );
	}

	Row.CopyAction(FUIAction(FExecuteAction::CreateSP(this, &FMaterialList::OnCopyMaterialItem, Item.SlotIndex), FCanExecuteAction::CreateSP(this, &FMaterialList::OnCanCopyMaterialItem, Item.SlotIndex)));
	Row.PasteAction(FUIAction(FExecuteAction::CreateSP(this, &FMaterialList::OnPasteMaterialItem, Item.SlotIndex)));

	Row.NameContent()
	[
		NewView->CreateNameContent()
	]
	.ValueContent()
	.MinDesiredWidth(250.f)
	.MaxDesiredWidth(0.0f) // no maximum
	[
		RightSideContent.ToSharedRef()
	];
}

TSharedRef<SWidget> PropertyCustomizationHelpers::MakePropertyComboBox(const TSharedPtr<IPropertyHandle>& InPropertyHandle, FOnGetPropertyComboBoxStrings OnGetStrings, FOnGetPropertyComboBoxValue OnGetValue, FOnPropertyComboBoxValueSelected OnValueSelected)
{
	FSlateFontInfo FontStyle = FEditorStyle::GetFontStyle(PropertyEditorConstants::PropertyFontStyle);

	return SNew(SPropertyEditorCombo)
		.PropertyHandle(InPropertyHandle)
		.OnGetComboBoxStrings(OnGetStrings)
		.OnGetComboBoxValue(OnGetValue)
		.OnComboBoxValueSelected(OnValueSelected)
		.Font(FontStyle);
}

//////////////////////////////////////////////////////////////////////////
//
// Sections list

/**
* Builds up a list of unique Sections while creating some information about the Sections
*/
class FSectionListBuilder : public ISectionListBuilder
{
	friend class FSectionList;
public:
	
	FSectionListBuilder(int32 InThumbnailSize)
		:ThumbnailSize(InThumbnailSize)
	{}
	
	/**
	* Adds a new Section to the list
	*
	* @param SlotIndex		The slot (usually mesh element index) where the Section is located on the component
	* @param Section		The Section being used
	* @param bCanBeReplced	Whether or not the Section can be replaced by a user
	*/
	virtual void AddSection(int32 LodIndex, int32 SectionIndex, FName InMaterialSlotName, int32 InMaterialSlotIndex, FName InOriginalMaterialSlotName, const TMap<int32, FName> &InAvailableMaterialSlotName, const UMaterialInterface* Material, bool IsSectionUsingCloth) override
	{
		FSectionListItem SectionItem(LodIndex, SectionIndex, InMaterialSlotName, InMaterialSlotIndex, InOriginalMaterialSlotName, InAvailableMaterialSlotName, Material, IsSectionUsingCloth, ThumbnailSize);
		if (!Sections.Contains(SectionItem))
		{
			Sections.Add(SectionItem);
			if (!SectionsByLOD.Contains(SectionItem.LodIndex))
			{
				TArray<FSectionListItem> LodSectionsArray;
				LodSectionsArray.Add(SectionItem);
				SectionsByLOD.Add(SectionItem.LodIndex, LodSectionsArray);
			}
			else
			{
				//Remove old entry
				TArray<FSectionListItem> &ExistingSections = *SectionsByLOD.Find(SectionItem.LodIndex);
				for (int32 ExistingSectionIndex = 0; ExistingSectionIndex < ExistingSections.Num(); ++ExistingSectionIndex)
				{
					const FSectionListItem &ExistingSectionItem = ExistingSections[ExistingSectionIndex];
					if (ExistingSectionItem.LodIndex == LodIndex && ExistingSectionItem.SectionIndex == SectionIndex)
					{
						ExistingSections.RemoveAt(ExistingSectionIndex);
						break;
					}
				}
				ExistingSections.Add(SectionItem);
			}
		}
	}

	/** Empties the list */
	void Empty()
	{
		Sections.Reset();
		SectionsByLOD.Reset();
	}

	/** Sorts the list by lod and section index */
	void Sort()
	{
		struct FSortByIndex
		{
			bool operator()(const FSectionListItem& A, const FSectionListItem& B) const
			{
				return (A.LodIndex == B.LodIndex) ? A.SectionIndex < B.SectionIndex : A.LodIndex < B.LodIndex;
			}
		};

		Sections.Sort(FSortByIndex());
	}

	/** @return The number of Sections in the list */
	uint32 GetNumSections() const
	{
		return Sections.Num();
	}

	uint32 GetNumSections(int32 LodIndex) const
	{
		return SectionsByLOD.Contains(LodIndex) ? SectionsByLOD.Find(LodIndex)->Num() : 0;
	}

private:
	/** All Section items in the list */
	TArray<FSectionListItem> Sections;
	/** All Section items in the list */
	TMap<int32, TArray<FSectionListItem>> SectionsByLOD;

	int32 ThumbnailSize;
};


/**
* A view of a single item in an FSectionList
*/
class FSectionItemView : public TSharedFromThis<FSectionItemView>
{
public:
	/**
	* Creates a new instance of this class
	*
	* @param Section				The Section to view
	* @param InOnSectionChanged	Delegate for when the Section changes
	*/
	static TSharedRef<FSectionItemView> Create(
		const FSectionListItem& Section,
		FOnSectionChanged InOnSectionChanged,
		FOnGenerateWidgetsForSection InOnGenerateNameWidgetsForSection,
		FOnGenerateWidgetsForSection InOnGenerateWidgetsForSection,
		FOnResetSectionToDefaultClicked InOnResetToDefaultClicked,
		int32 InMultipleSectionCount,
		int32 InThumbnailSize)
	{
		return MakeShareable(new FSectionItemView(Section, InOnSectionChanged, InOnGenerateNameWidgetsForSection, InOnGenerateWidgetsForSection, InOnResetToDefaultClicked, InMultipleSectionCount, InThumbnailSize));
	}

	TSharedRef<SWidget> CreateNameContent()
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("SectionIndex"), SectionItem.SectionIndex);
		return
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text(FText::Format(LOCTEXT("SectionIndex", "Section {SectionIndex}"), Arguments))
			]
			+ SVerticalBox::Slot()
			.Padding(0.0f, 4.0f)
			.AutoHeight()
			[
				OnGenerateCustomNameWidgets.IsBound() ? OnGenerateCustomNameWidgets.Execute(SectionItem.LodIndex, SectionItem.SectionIndex) : StaticCastSharedRef<SWidget>(SNullWidget::NullWidget)
			];
	}

	TSharedRef<SWidget> CreateValueContent(const TSharedPtr<FAssetThumbnailPool>& ThumbnailPool)
	{
		FText MaterialSlotNameTooltipText = SectionItem.IsSectionUsingCloth ? FText(LOCTEXT("SectionIndex_MaterialSlotNameTooltip", "Cannot change the material slot when the mesh section use the cloth system.")) : FText::GetEmpty();
		return
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Fill)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Fill)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(SPropertyEditorAsset)
						.ObjectPath(SectionItem.Material->GetPathName())
						.Class(UMaterialInterface::StaticClass())
						.DisplayThumbnail(true)
						.ThumbnailSize(FIntPoint(ThumbnailSize, ThumbnailSize))
						.DisplayUseSelected(false)
						.AllowClear(false)
						.DisplayBrowse(false)
						.EnableContentPicker(false)
						.ThumbnailPool(ThumbnailPool)
						.DisplayCompactSize(true)
						.CustomContentSlot()
						[
							SNew( SBox )
							.HAlign(HAlign_Fill)
							[
								SNew(SVerticalBox)
								+SVerticalBox::Slot()
								.AutoHeight()
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot()
									.Padding(0)
									.VAlign(VAlign_Center)
									.AutoWidth()
									[
										SNew(SBox)
										.HAlign(HAlign_Right)
										.MinDesiredWidth(65.0f)
										[
											SNew(STextBlock)
											.Font(IDetailLayoutBuilder::GetDetailFont())
											.Text(LOCTEXT("SectionListItemMaterialSlotNameLabel", "Material Slot"))
											.ToolTipText(MaterialSlotNameTooltipText)
										]
									]
									+ SHorizontalBox::Slot()
									.VAlign(VAlign_Center)
									.FillWidth(1.0f)
									.Padding(5, 0, 0, 0)
									[
										SNew(SBox)
										.HAlign(HAlign_Fill)
										.VAlign(VAlign_Center)
										.MinDesiredWidth(210.0f)
										[
											//Material Slot Name
											SNew(SComboButton)
											.OnGetMenuContent(this, &FSectionItemView::OnGetMaterialSlotNameMenuForSection)
											.VAlign(VAlign_Center)
											.ContentPadding(2)
											.IsEnabled(!SectionItem.IsSectionUsingCloth)
											.ButtonContent()
											[
												SNew(STextBlock)
												.Font(IDetailLayoutBuilder::GetDetailFont())
												.Text(this, &FSectionItemView::GetCurrentMaterialSlotName)
												.ToolTipText(MaterialSlotNameTooltipText)
											]
										]
									]
								]
								+SVerticalBox::Slot()
								.AutoHeight()
								.VAlign(VAlign_Center)
								[
									OnGenerateCustomSectionWidgets.IsBound() ? OnGenerateCustomSectionWidgets.Execute(SectionItem.LodIndex, SectionItem.SectionIndex) : StaticCastSharedRef<SWidget>(SNullWidget::NullWidget)
								]
							]
						]
					]
				]
			];
	}

private:

	FSectionItemView(const FSectionListItem& InSection,
		FOnSectionChanged& InOnSectionChanged,
		FOnGenerateWidgetsForSection& InOnGenerateNameWidgets,
		FOnGenerateWidgetsForSection& InOnGenerateSectionWidgets,
		FOnResetSectionToDefaultClicked& InOnResetToDefaultClicked,
		int32 InMultipleSectionCount,
		int32 InThumbnailSize)
		: SectionItem(InSection)
		, OnSectionChanged(InOnSectionChanged)
		, OnGenerateCustomNameWidgets(InOnGenerateNameWidgets)
		, OnGenerateCustomSectionWidgets(InOnGenerateSectionWidgets)
		, OnResetToDefaultClicked(InOnResetToDefaultClicked)
		, MultipleSectionCount(InMultipleSectionCount)
		, ThumbnailSize(InThumbnailSize)
	{

	}

	TSharedRef<SWidget> OnGetMaterialSlotNameMenuForSection()
	{
		FMenuBuilder MenuBuilder(true, NULL);

		// Add a menu item for each texture.  Clicking on the texture will display it in the content browser
		for (auto kvp : SectionItem.AvailableMaterialSlotName)
		{
			FName AvailableMaterialSlotName = kvp.Value;
			int32 AvailableMaterialSlotIndex = kvp.Key;

			FUIAction Action(FExecuteAction::CreateSP(this, &FSectionItemView::SetMaterialSlotName, AvailableMaterialSlotIndex, AvailableMaterialSlotName));

			FString MaterialSlotDisplayName;
			AvailableMaterialSlotName.ToString(MaterialSlotDisplayName);
			MaterialSlotDisplayName = TEXT("[") + FString::FromInt(kvp.Key) + TEXT("] ") + MaterialSlotDisplayName;
			MenuBuilder.AddMenuEntry(FText::FromString(MaterialSlotDisplayName), LOCTEXT("BrowseAvailableMaterialSlotName_ToolTip", "Set the material slot name for this section"), FSlateIcon(), Action);
		}

		return MenuBuilder.MakeWidget();
	}

	void SetMaterialSlotName(int32 MaterialSlotIndex, FName NewSlotName)
	{
		OnSectionChanged.ExecuteIfBound(SectionItem.LodIndex, SectionItem.SectionIndex, MaterialSlotIndex, NewSlotName);
	}

	FText GetCurrentMaterialSlotName() const
	{
		FString MaterialSlotDisplayName;
		SectionItem.MaterialSlotName.ToString(MaterialSlotDisplayName);
		MaterialSlotDisplayName = TEXT("[") + FString::FromInt(SectionItem.MaterialSlotIndex) + TEXT("] ") + MaterialSlotDisplayName;
		return FText::FromString(MaterialSlotDisplayName);
	}

	/**
	* Called when reset to base is clicked
	*/
	void OnResetToBaseClicked(TSharedRef<IPropertyHandle> PropertyHandle)
	{
		OnResetToDefaultClicked.ExecuteIfBound(SectionItem.LodIndex, SectionItem.SectionIndex);
	}

private:
	FSectionListItem SectionItem;
	FOnSectionChanged OnSectionChanged;
	FOnGenerateWidgetsForSection OnGenerateCustomNameWidgets;
	FOnGenerateWidgetsForSection OnGenerateCustomSectionWidgets;
	FOnResetSectionToDefaultClicked OnResetToDefaultClicked;
	int32 MultipleSectionCount;
	int32 ThumbnailSize;
};


FSectionList::FSectionList(IDetailLayoutBuilder& InDetailLayoutBuilder, FSectionListDelegates& InSectionListDelegates, bool bInAllowCollapse, int32 InThumbnailSize, int32 InSectionsLodIndex)
	: SectionListDelegates(InSectionListDelegates)
	, DetailLayoutBuilder(InDetailLayoutBuilder)
	, SectionListBuilder(new FSectionListBuilder(InThumbnailSize))
	, bAllowCollpase(bInAllowCollapse)
	, ThumbnailSize(InThumbnailSize)
	, SectionsLodIndex(InSectionsLodIndex)
{
}

void FSectionList::OnDisplaySectionsForLod(int32 LodIndex)
{
	// We now want to display all the materials in the element
	ExpandedSlots.Add(LodIndex);

	SectionListBuilder->Empty();
	SectionListDelegates.OnGetSections.ExecuteIfBound(*SectionListBuilder);

	OnRebuildChildren.ExecuteIfBound();
}

void FSectionList::OnHideSectionsForLod(int32 SlotIndex)
{
	// No longer want to expand the element
	ExpandedSlots.Remove(SlotIndex);

	// regenerate the Sections
	SectionListBuilder->Empty();
	SectionListDelegates.OnGetSections.ExecuteIfBound(*SectionListBuilder);

	OnRebuildChildren.ExecuteIfBound();
}


void FSectionList::Tick(float DeltaTime)
{
	// Check each Section to see if its still valid.  This allows the Section list to stay up to date when Sections are changed out from under us
	if (SectionListDelegates.OnGetSections.IsBound())
	{
		// Whether or not to refresh the Section list
		bool bRefrestSectionList = false;

		// Get the current list of Sections from the user
		SectionListBuilder->Empty();
		SectionListDelegates.OnGetSections.ExecuteIfBound(*SectionListBuilder);

		if (SectionListBuilder->GetNumSections() != DisplayedSections.Num())
		{
			// The array sizes differ so we need to refresh the list
			bRefrestSectionList = true;
		}
		else
		{
			// Compare the new list against the currently displayed list
			for (int32 SectionIndex = 0; SectionIndex < SectionListBuilder->Sections.Num(); ++SectionIndex)
			{
				const FSectionListItem& Item = SectionListBuilder->Sections[SectionIndex];

				// The displayed Sections is out of date if there isn't a 1:1 mapping between the Section sets
				if (!DisplayedSections.IsValidIndex(SectionIndex) || DisplayedSections[SectionIndex] != Item)
				{
					bRefrestSectionList = true;
					break;
				}
			}
		}

		if (bRefrestSectionList)
		{
			OnRebuildChildren.ExecuteIfBound();
		}
	}
}

void FSectionList::GenerateHeaderRowContent(FDetailWidgetRow& NodeRow)
{
	NodeRow.CopyAction(FUIAction(FExecuteAction::CreateSP(this, &FSectionList::OnCopySectionList), FCanExecuteAction::CreateSP(this, &FSectionList::OnCanCopySectionList)));
	NodeRow.PasteAction(FUIAction(FExecuteAction::CreateSP(this, &FSectionList::OnPasteSectionList)));

	NodeRow.NameContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("SectionHeaderTitle", "Sections"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	];
	if (SectionListDelegates.OnGenerateLodComboBox.IsBound())
	{
		NodeRow.ValueContent()
		[
			SectionListDelegates.OnGenerateLodComboBox.Execute(SectionsLodIndex)
		];
	}
}

void FSectionList::GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder)
{
	ViewedSections.Empty();
	DisplayedSections.Empty();
	if (SectionListBuilder->GetNumSections() > 0)
	{
		DisplayedSections = SectionListBuilder->Sections;

		SectionListBuilder->Sort();
		TArray<FSectionListItem>& Sections = SectionListBuilder->Sections;

		int32 CurrentLODIndex = INDEX_NONE;
		bool bDisplayAllSectionsInSlot = true;
		for (auto It = Sections.CreateConstIterator(); It; ++It)
		{
			const FSectionListItem& Section = *It;

			CurrentLODIndex = Section.LodIndex;

			// Display each thumbnail element unless we shouldn't display multiple Sections for one slot
			if (bDisplayAllSectionsInSlot)
			{
				FDetailWidgetRow& ChildRow = ChildrenBuilder.AddCustomRow(Section.Material.IsValid() ? FText::FromString(Section.Material->GetName()) : FText::GetEmpty());
				AddSectionItem(ChildRow, CurrentLODIndex, FSectionListItem(CurrentLODIndex, Section.SectionIndex, Section.MaterialSlotName, Section.MaterialSlotIndex, Section.OriginalMaterialSlotName, Section.AvailableMaterialSlotName, Section.Material.Get(), Section.IsSectionUsingCloth, ThumbnailSize), !bDisplayAllSectionsInSlot);
			}
		}
	}
	else
	{
		FDetailWidgetRow& ChildRow = ChildrenBuilder.AddCustomRow(LOCTEXT("NoSections", "No Sections"));

		ChildRow
			[
				SNew(SBox)
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("NoSections", "No Sections"))
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			];
	}
}

bool FSectionList::OnCanCopySectionList() const
{
	if (SectionListDelegates.OnCanCopySectionList.IsBound())
	{
		return SectionListDelegates.OnCanCopySectionList.Execute();
	}

	return false;
}

void FSectionList::OnCopySectionList()
{
	if (SectionListDelegates.OnCopySectionList.IsBound())
	{
		SectionListDelegates.OnCopySectionList.Execute();
	}
}

void FSectionList::OnPasteSectionList()
{
	if (SectionListDelegates.OnPasteSectionList.IsBound())
	{
		SectionListDelegates.OnPasteSectionList.Execute();
	}
}

bool FSectionList::OnCanCopySectionItem(int32 LODIndex, int32 SectionIndex) const
{
	if (SectionListDelegates.OnCanCopySectionItem.IsBound())
	{
		return SectionListDelegates.OnCanCopySectionItem.Execute(LODIndex, SectionIndex);
	}

	return false;
}

void FSectionList::OnCopySectionItem(int32 LODIndex, int32 SectionIndex)
{
	if (SectionListDelegates.OnCopySectionItem.IsBound())
	{
		SectionListDelegates.OnCopySectionItem.Execute(LODIndex, SectionIndex);
	}
}

void FSectionList::OnPasteSectionItem(int32 LODIndex, int32 SectionIndex)
{
	if (SectionListDelegates.OnPasteSectionItem.IsBound())
	{
		SectionListDelegates.OnPasteSectionItem.Execute(LODIndex, SectionIndex);
	}
}

void FSectionList::AddSectionItem(FDetailWidgetRow& Row, int32 LodIndex, const struct FSectionListItem& Item, bool bDisplayLink)
{
	uint32 NumSections = SectionListBuilder->GetNumSections(LodIndex);

	TSharedRef<FSectionItemView> NewView = FSectionItemView::Create(Item, SectionListDelegates.OnSectionChanged, SectionListDelegates.OnGenerateCustomNameWidgets, SectionListDelegates.OnGenerateCustomSectionWidgets, SectionListDelegates.OnResetSectionToDefaultClicked, NumSections, ThumbnailSize);

	TSharedPtr<SWidget> RightSideContent;
	if (bDisplayLink)
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("NumSections"), NumSections);

		RightSideContent =
			SNew(SBox)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Top)
			[
				SNew(SHyperlink)
				.TextStyle(FEditorStyle::Get(), "MaterialList.HyperlinkStyle")
				.Text(FText::Format(LOCTEXT("DisplayAllSectionLinkText", "Display {NumSections} Sections"), Arguments))
				.ToolTipText(LOCTEXT("DisplayAllSectionLink_ToolTip", "Display all Sections. Drag and drop a Section here to replace all Sections."))
				.OnNavigate(this, &FSectionList::OnDisplaySectionsForLod, LodIndex)
			];
	}
	else
	{
		RightSideContent = NewView->CreateValueContent(DetailLayoutBuilder.GetThumbnailPool());
		ViewedSections.Add(NewView);
	}

	Row.CopyAction(FUIAction(FExecuteAction::CreateSP(this, &FSectionList::OnCopySectionItem, LodIndex, Item.SectionIndex), FCanExecuteAction::CreateSP(this, &FSectionList::OnCanCopySectionItem, LodIndex, Item.SectionIndex)));
	Row.PasteAction(FUIAction(FExecuteAction::CreateSP(this, &FSectionList::OnPasteSectionItem, LodIndex, Item.SectionIndex)));

	Row.NameContent()
		[
			NewView->CreateNameContent()
		]
	.ValueContent()
		.MinDesiredWidth(250.0f)
		.MaxDesiredWidth(0.0f) // no maximum
		[
			RightSideContent.ToSharedRef()
		];
}


void SMaterialSlotWidget::Construct(const FArguments& InArgs, int32 SlotIndex, bool bIsMaterialUsed)
{
	TSharedPtr<SHorizontalBox> SlotNameBox;

	TSharedRef<SWidget> DeleteButton =
		PropertyCustomizationHelpers::MakeDeleteButton(
			InArgs._OnDeleteMaterialSlot,
			LOCTEXT("CustomNameMaterialNotUsedDeleteTooltip", "Delete this material slot"),
			InArgs._CanDeleteMaterialSlot);

	ChildSlot
	[
		SAssignNew(SlotNameBox, SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text(LOCTEXT("MaterialArrayNameLabelStringKey", "Slot Name"))
			]
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.Padding(5.0f, 3.0f, 0.0f, 3.0f)
		[
			SNew(SBox)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Fill)
			.MinDesiredWidth(160.0f)
			[
				SNew(SEditableTextBox)
				.Text(InArgs._MaterialName)
				.OnTextChanged(InArgs._OnMaterialNameChanged)
				.OnTextCommitted(InArgs._OnMaterialNameCommitted)
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
		]
	];

	
	if (bIsMaterialUsed)
	{
		DeleteButton->SetVisibility(EVisibility::Hidden);
	}
	

	SlotNameBox->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Left)
		.Padding(2)
		[
			DeleteButton
		];
}

#undef LOCTEXT_NAMESPACE
