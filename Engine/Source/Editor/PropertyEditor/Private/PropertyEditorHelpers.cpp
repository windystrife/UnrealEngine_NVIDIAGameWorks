// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PropertyEditorHelpers.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SCheckBox.h"
#include "UserInterface/PropertyEditor/PropertyEditorConstants.h"
#include "IDocumentation.h"

#include "PropertyHandleImpl.h"

#include "UserInterface/PropertyEditor/SPropertyEditor.h"
#include "UserInterface/PropertyEditor/SPropertyEditorNumeric.h"
#include "UserInterface/PropertyEditor/SPropertyEditorArray.h"
#include "UserInterface/PropertyEditor/SPropertyEditorCombo.h"
#include "UserInterface/PropertyEditor/SPropertyEditorEditInline.h"
#include "UserInterface/PropertyEditor/SPropertyEditorText.h"
#include "UserInterface/PropertyEditor/SPropertyEditorBool.h"
#include "UserInterface/PropertyEditor/SPropertyEditorArrayItem.h"
#include "UserInterface/PropertyEditor/SPropertyEditorTitle.h"
#include "UserInterface/PropertyEditor/SPropertyEditorDateTime.h"
#include "UserInterface/PropertyEditor/SResetToDefaultPropertyEditor.h"
#include "UserInterface/PropertyEditor/SPropertyEditorAsset.h"
#include "UserInterface/PropertyEditor/SPropertyEditorClass.h"
#include "UserInterface/PropertyEditor/SPropertyEditorSet.h"
#include "UserInterface/PropertyEditor/SPropertyEditorMap.h"

#include "Kismet2/KismetEditorUtilities.h"
#include "EditorClassUtils.h"
#include "Engine/Selection.h"

#define LOCTEXT_NAMESPACE "PropertyEditor"

void SPropertyNameWidget::Construct( const FArguments& InArgs, TSharedPtr<FPropertyEditor> InPropertyEditor )
{
	PropertyEditor = InPropertyEditor;

	TSharedPtr<SHorizontalBox> HorizontalBox;
	ChildSlot
	[
		SAssignNew(HorizontalBox, SHorizontalBox)
		+SHorizontalBox::Slot()
		.Padding( FMargin( 0, 1, 0, 1 ) )
		.FillWidth(1)
		[
			SNew(SBorder)
			.BorderImage_Static( &PropertyEditorConstants::GetOverlayBrush, PropertyEditor.ToSharedRef() )
			.Padding( FMargin( 0.0f, 2.0f ) )
			.VAlign(VAlign_Center)
			[
				SNew( SPropertyEditorTitle, PropertyEditor.ToSharedRef() )
				.StaticDisplayName( PropertyEditor->GetDisplayName() )
				.OnDoubleClicked( InArgs._OnDoubleClicked )
                .ToolTip( IDocumentation::Get()->CreateToolTip( PropertyEditor->GetToolTipText(), NULL, PropertyEditor->GetDocumentationLink(), PropertyEditor->GetDocumentationExcerptName() ) )
			]
		]
	
	];

	if( InArgs._DisplayResetToDefault && !PropertyEditor->GetPropertyHandle()->HasMetaData(TEXT("NoResetToDefault")) )
	{
		HorizontalBox->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(2,1)
		[
			SNew( SResetToDefaultPropertyEditor, PropertyEditor->GetPropertyHandle())
		];
	}
}

void SPropertyValueWidget::Construct( const FArguments& InArgs, TSharedPtr<FPropertyEditor> PropertyEditor, TSharedPtr<IPropertyUtilities> InPropertyUtilities )
{
	MinDesiredWidth = 0.0f;
	MaxDesiredWidth = 0.0f;

	bCreatedResetButton = false;
	SetEnabled( TAttribute<bool>( PropertyEditor.ToSharedRef(), &FPropertyEditor::IsPropertyEditingEnabled ) );


	ValueEditorWidget = ConstructPropertyEditorWidget( PropertyEditor, InPropertyUtilities, InArgs._OptionalResetWidget );

	if ( !ValueEditorWidget->GetToolTip().IsValid() )
	{
		ValueEditorWidget->SetToolTipText( PropertyEditor->GetToolTipText() );
	}


	if( InArgs._ShowPropertyButtons )
	{
		TSharedRef<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);

		HorizontalBox->AddSlot()
		.FillWidth(1) // Fill the entire width if possible
		.VAlign(VAlign_Center)	
		[
			ValueEditorWidget.ToSharedRef()
		];

		TArray< TSharedRef<SWidget> > RequiredButtons;
		PropertyEditorHelpers::MakeRequiredPropertyButtons( PropertyEditor.ToSharedRef(), /*OUT*/RequiredButtons );

		for( int32 ButtonIndex = 0; ButtonIndex < RequiredButtons.Num(); ++ButtonIndex )
		{
			HorizontalBox->AddSlot()
				.AutoWidth()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Padding( 2.0f, 1.0f )
				[ 
					RequiredButtons[ButtonIndex]
				];
		}

		ChildSlot
		[
			HorizontalBox
		];

	}
	else
	{
		ChildSlot
		.VAlign(VAlign_Center)	
		[
			ValueEditorWidget.ToSharedRef()
		];
	}


}

TSharedRef<SWidget> SPropertyValueWidget::ConstructPropertyEditorWidget( TSharedPtr<FPropertyEditor>& PropertyEditor, TSharedPtr<IPropertyUtilities> InPropertyUtilities, TSharedRef<SWidget> InResetDefaultWidget)
{
	const TSharedRef<FPropertyEditor> PropertyEditorRef = PropertyEditor.ToSharedRef();
	const TSharedRef<IPropertyUtilities> PropertyUtilitiesRef = InPropertyUtilities.ToSharedRef();

	const TSharedRef< FPropertyNode > PropertyNode = PropertyEditorRef->GetPropertyNode();
	const int32 NodeArrayIndex = PropertyNode->GetArrayIndex();
	UProperty* Property = PropertyNode->GetProperty();
	
	FSlateFontInfo FontStyle = FEditorStyle::GetFontStyle( PropertyEditorConstants::PropertyFontStyle );
	TSharedPtr<SWidget> PropertyWidget; 
	if( Property )
	{
		// ORDER MATTERS: first widget type to support the property node wins!
		if ( SPropertyEditorArray::Supports(PropertyEditorRef) )
		{
			TSharedRef<SPropertyEditorArray> ArrayWidget = 
				SAssignNew( PropertyWidget, SPropertyEditorArray, PropertyEditorRef )
				.Font( FontStyle );

			ArrayWidget->GetDesiredWidth( MinDesiredWidth, MaxDesiredWidth );
		}
		else if ( SPropertyEditorSet::Supports(PropertyEditorRef) )
		{
			TSharedRef<SPropertyEditorSet> SetWidget =
				SAssignNew( PropertyWidget, SPropertyEditorSet, PropertyEditorRef )
				.Font( FontStyle );

			SetWidget->GetDesiredWidth( MinDesiredWidth, MaxDesiredWidth );
		}
		else if ( SPropertyEditorMap::Supports(PropertyEditorRef) )
		{
			TSharedRef<SPropertyEditorMap> MapWidget =
				SAssignNew( PropertyWidget, SPropertyEditorMap, PropertyEditorRef )
				.Font( FontStyle );

			MapWidget->GetDesiredWidth( MinDesiredWidth, MaxDesiredWidth );
		}
		else if ( SPropertyEditorAsset::Supports( PropertyEditorRef ) )
		{
			TSharedRef<SPropertyEditorAsset> AssetWidget = 
				SAssignNew( PropertyWidget, SPropertyEditorAsset, PropertyEditorRef )
				.ThumbnailPool( PropertyUtilitiesRef->GetThumbnailPool() )
				.ResetToDefaultSlot()
				[
					InResetDefaultWidget
				];
			if (InResetDefaultWidget != SNullWidget::NullWidget)
			{
				bCreatedResetButton = true;
			}
			AssetWidget->GetDesiredWidth( MinDesiredWidth, MaxDesiredWidth );
		}
		else if ( SPropertyEditorClass::Supports( PropertyEditorRef ) )
		{
			TSharedRef<SPropertyEditorClass> ClassWidget = 
				SAssignNew( PropertyWidget, SPropertyEditorClass, PropertyEditorRef )
				.Font( FontStyle );

			ClassWidget->GetDesiredWidth( MinDesiredWidth, MaxDesiredWidth );
		}
		else if ( SPropertyEditorNumeric<float>::Supports( PropertyEditorRef ) )
		{
			auto NumericWidget = 
				SAssignNew( PropertyWidget, SPropertyEditorNumeric<float>, PropertyEditorRef )
				.Font( FontStyle );

			NumericWidget->GetDesiredWidth( MinDesiredWidth, MaxDesiredWidth );
		}
		else if ( SPropertyEditorNumeric<double>::Supports( PropertyEditorRef ) )
		{
			auto NumericWidget =
				SAssignNew( PropertyWidget, SPropertyEditorNumeric<double>, PropertyEditorRef )
				.Font( FontStyle );

			NumericWidget->GetDesiredWidth( MinDesiredWidth, MaxDesiredWidth );
		}
		else if (SPropertyEditorNumeric<int8>::Supports(PropertyEditorRef))
		{
			auto NumericWidget =
				SAssignNew(PropertyWidget, SPropertyEditorNumeric<int8>, PropertyEditorRef)
				.Font(FontStyle);

			NumericWidget->GetDesiredWidth(MinDesiredWidth, MaxDesiredWidth);
		}
		else if (SPropertyEditorNumeric<int16>::Supports(PropertyEditorRef))
		{
			auto NumericWidget =
				SAssignNew(PropertyWidget, SPropertyEditorNumeric<int16>, PropertyEditorRef)
				.Font(FontStyle);

			NumericWidget->GetDesiredWidth(MinDesiredWidth, MaxDesiredWidth);
		}
		else if ( SPropertyEditorNumeric<int32>::Supports( PropertyEditorRef ) )
		{
			auto NumericWidget = 
				SAssignNew( PropertyWidget, SPropertyEditorNumeric<int32>, PropertyEditorRef )
				.Font( FontStyle );

			NumericWidget->GetDesiredWidth( MinDesiredWidth, MaxDesiredWidth );
		}
		else if (SPropertyEditorNumeric<int64>::Supports(PropertyEditorRef))
		{
			auto NumericWidget =
				SAssignNew(PropertyWidget, SPropertyEditorNumeric<int64>, PropertyEditorRef)
				.Font(FontStyle);

			NumericWidget->GetDesiredWidth(MinDesiredWidth, MaxDesiredWidth);
		}
		else if ( SPropertyEditorNumeric<uint8>::Supports( PropertyEditorRef ) )
		{
			auto NumericWidget = 
				SAssignNew( PropertyWidget, SPropertyEditorNumeric<uint8>, PropertyEditorRef )
				.Font( FontStyle );

			NumericWidget->GetDesiredWidth( MinDesiredWidth, MaxDesiredWidth );
		}
		else if (SPropertyEditorNumeric<uint16>::Supports(PropertyEditorRef))
		{
			auto NumericWidget =
				SAssignNew(PropertyWidget, SPropertyEditorNumeric<uint16>, PropertyEditorRef)
				.Font(FontStyle);

			NumericWidget->GetDesiredWidth(MinDesiredWidth, MaxDesiredWidth);
		}
		else if (SPropertyEditorNumeric<uint32>::Supports(PropertyEditorRef))
		{
			auto NumericWidget =
				SAssignNew(PropertyWidget, SPropertyEditorNumeric<uint32>, PropertyEditorRef)
				.Font(FontStyle);

			NumericWidget->GetDesiredWidth(MinDesiredWidth, MaxDesiredWidth);
		}
		else if (SPropertyEditorNumeric<uint64>::Supports(PropertyEditorRef))
		{
			auto NumericWidget =
				SAssignNew(PropertyWidget, SPropertyEditorNumeric<uint64>, PropertyEditorRef)
				.Font(FontStyle);

			NumericWidget->GetDesiredWidth(MinDesiredWidth, MaxDesiredWidth);
		}
		else if ( SPropertyEditorCombo::Supports( PropertyEditorRef ) )
		{
			TSharedRef<SPropertyEditorCombo> ComboWidget = 
				SAssignNew( PropertyWidget, SPropertyEditorCombo, PropertyEditorRef )
				.Font( FontStyle );

			ComboWidget->GetDesiredWidth( MinDesiredWidth, MaxDesiredWidth );
		}
		else if ( SPropertyEditorEditInline::Supports( PropertyEditorRef ) )
		{
			TSharedRef<SPropertyEditorEditInline> EditInlineWidget = 
				SAssignNew( PropertyWidget, SPropertyEditorEditInline, PropertyEditorRef )
				.Font( FontStyle );

			EditInlineWidget->GetDesiredWidth( MinDesiredWidth, MaxDesiredWidth );
		}
		else if ( SPropertyEditorText::Supports( PropertyEditorRef ) )
		{
			TSharedRef<SPropertyEditorText> TextWidget = 
				SAssignNew( PropertyWidget, SPropertyEditorText, PropertyEditorRef )
				.Font( FontStyle );

			TextWidget->GetDesiredWidth( MinDesiredWidth, MaxDesiredWidth );
		}
		else if ( SPropertyEditorBool::Supports( PropertyEditorRef ) )
		{
			TSharedRef<SPropertyEditorBool> BoolWidget = 
				SAssignNew( PropertyWidget, SPropertyEditorBool, PropertyEditorRef );

			BoolWidget->GetDesiredWidth( MinDesiredWidth, MaxDesiredWidth );

		}
		else if ( SPropertyEditorArrayItem::Supports( PropertyEditorRef ) )
		{
			TSharedRef<SPropertyEditorArrayItem> ArrayItemWidget = 
				SAssignNew( PropertyWidget, SPropertyEditorArrayItem, PropertyEditorRef )
				.Font( FontStyle );

			ArrayItemWidget->GetDesiredWidth( MinDesiredWidth, MaxDesiredWidth );
		}
		else if ( SPropertyEditorDateTime::Supports( PropertyEditorRef ) )
		{
			TSharedRef<SPropertyEditorDateTime> DateTimeWidget =
				SAssignNew( PropertyWidget, SPropertyEditorDateTime, PropertyEditorRef )
				.Font( FontStyle );
		}
	}

	if( !PropertyWidget.IsValid() )
	{
		TSharedRef<SPropertyEditor> BasePropertyEditorWidget = 
			SAssignNew( PropertyWidget, SPropertyEditor, PropertyEditorRef )
			.Font( FontStyle );

		BasePropertyEditorWidget->GetDesiredWidth( MinDesiredWidth, MaxDesiredWidth );

	}

	return PropertyWidget.ToSharedRef();
}

void SEditConditionWidget::Construct( const FArguments& Args, TSharedPtr<FPropertyEditor> InPropertyEditor )
{
	PropertyEditor = InPropertyEditor;
	CustomEditCondition = Args._CustomEditCondition;

	SetVisibility( HasEditCondition() ? EVisibility::Visible : EVisibility::Collapsed );

	ChildSlot
	[
		// Some properties become irrelevant depending on the value of other properties.
		// We prevent the user from editing those properties by disabling their widgets.
		// This is a shortcut for toggling the property that disables us.
		SNew( SCheckBox )
			.OnCheckStateChanged( this, &SEditConditionWidget::OnEditConditionCheckChanged )
			.IsChecked( this, &SEditConditionWidget::OnGetEditConditionCheckState )
	];
}

bool SEditConditionWidget::HasEditCondition() const
{	
	return
			( PropertyEditor.IsValid() && PropertyEditor->HasEditCondition() && PropertyEditor->SupportsEditConditionToggle() )
		||	( CustomEditCondition.OnEditConditionValueChanged.IsBound() );
}

void SEditConditionWidget::OnEditConditionCheckChanged( ECheckBoxState CheckState )
{
	if( PropertyEditor.IsValid() && PropertyEditor->HasEditCondition() && PropertyEditor->SupportsEditConditionToggle() )
	{
		PropertyEditor->SetEditConditionState( CheckState == ECheckBoxState::Checked );
	}
	else
	{
		CustomEditCondition.OnEditConditionValueChanged.ExecuteIfBound( CheckState == ECheckBoxState::Checked );
	}
}

ECheckBoxState SEditConditionWidget::OnGetEditConditionCheckState() const
{
	bool bEditConditionMet = ( PropertyEditor.IsValid() && PropertyEditor->HasEditCondition() && PropertyEditor->IsEditConditionMet() ) || CustomEditCondition.EditConditionValue.Get();
	return bEditConditionMet ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

namespace PropertyEditorHelpers
{
	bool IsBuiltInStructProperty( const UProperty* Property )
	{
		bool bIsBuiltIn = false;

		const UStructProperty* StructProp = Cast<const UStructProperty>( Property );
		if( StructProp && StructProp->Struct )
		{
			FName StructName = StructProp->Struct->GetFName();

			bIsBuiltIn = StructName == NAME_Rotator || 
						 StructName == NAME_Color ||  
						 StructName == NAME_LinearColor || 
						 StructName == NAME_Vector ||
						 StructName == NAME_Quat ||
						 StructName == NAME_Vector4 ||
						 StructName == NAME_Vector2D ||
						 StructName == NAME_IntPoint;
		}

		return bIsBuiltIn;
	}

	bool IsChildOfArray( const FPropertyNode& InPropertyNode )
	{
		return GetArrayParent( InPropertyNode ) != NULL;
	}

	bool IsChildOfSet(  const FPropertyNode& InPropertyNode )
	{
		return GetSetParent( InPropertyNode ) != NULL;
	}

	bool IsChildOfMap(const FPropertyNode& InPropertyNode)
	{
		return GetMapParent(InPropertyNode) != NULL;
	}

	bool IsStaticArray( const FPropertyNode& InPropertyNode )
	{
		const UProperty* NodeProperty = InPropertyNode.GetProperty();
		return NodeProperty && NodeProperty->ArrayDim != 1 && InPropertyNode.GetArrayIndex() == -1;
	}

	bool IsDynamicArray( const FPropertyNode& InPropertyNode )
	{
		const UProperty* NodeProperty = InPropertyNode.GetProperty();
		return NodeProperty && Cast<const UArrayProperty>(NodeProperty) != NULL;
	}

	const UProperty* GetArrayParent( const FPropertyNode& InPropertyNode )
	{
		const UProperty* ParentProperty = InPropertyNode.GetParentNode() != NULL ? InPropertyNode.GetParentNode()->GetProperty() : NULL;
		
		if( ParentProperty )
		{
			if( (ParentProperty->IsA<UArrayProperty>()) || // dynamic array
				(InPropertyNode.GetArrayIndex() != INDEX_NONE && ParentProperty->ArrayDim > 0) ) //static array
			{
				return ParentProperty;
			}
		}

		return NULL;
	}

	const UProperty* GetSetParent( const FPropertyNode& InPropertyNode )
	{
		const UProperty* ParentProperty = InPropertyNode.GetParentNode() != NULL ? InPropertyNode.GetParentNode()->GetProperty() : NULL;

		if ( ParentProperty )
		{
			if (ParentProperty->IsA<USetProperty>())
			{
				return ParentProperty;
			}
		}

		return NULL;
	}

	const UProperty* GetMapParent( const FPropertyNode& InPropertyNode )
	{
		const UProperty* ParentProperty = InPropertyNode.GetParentNode() != NULL ? InPropertyNode.GetParentNode()->GetProperty() : NULL;

		if (ParentProperty)
		{
			if (ParentProperty->IsA<UMapProperty>())
			{
				return ParentProperty;
			}

			//@todo: Also check a key/value node parent property?
		}

		return NULL;
	}

	bool IsEditInlineClassAllowed( UClass* CheckClass, bool bAllowAbstract ) 
	{
		return !CheckClass->HasAnyClassFlags(CLASS_Hidden|CLASS_HideDropDown|CLASS_Deprecated)
			&&	(bAllowAbstract || !CheckClass->HasAnyClassFlags(CLASS_Abstract));
	}

	FText GetToolTipText( const UProperty* const Property )
	{
		if( Property )
		{
			return Property->GetToolTipText();
		}

		return FText::GetEmpty();
	}

	FString GetDocumentationLink( const UProperty* const Property )
	{
		if ( Property != NULL )
		{
			UStruct* OwnerStruct = Property->GetOwnerStruct();

			if ( OwnerStruct != NULL )
			{
				return FString::Printf( TEXT("Shared/Types/%s%s"), OwnerStruct->GetPrefixCPP(), *OwnerStruct->GetName() );
			}
		}

		return TEXT("");
	}

	FString GetEnumDocumentationLink(const UProperty* const Property)
	{
		if(Property != NULL)
		{
			const UByteProperty* ByteProperty = Cast<UByteProperty>(Property);
			const UEnumProperty* EnumProperty = Cast<UEnumProperty>(Property);
			if(ByteProperty || EnumProperty || (Property->IsA(UStrProperty::StaticClass()) && Property->HasMetaData(TEXT("Enum"))))
			{
				UEnum* Enum = nullptr;
				if(ByteProperty)
				{
					Enum = ByteProperty->Enum;
				}
				else if (EnumProperty)
				{
					Enum = EnumProperty->GetEnum();
				}
				else
				{

					const FString& EnumName = Property->GetMetaData(TEXT("Enum"));
					Enum = FindObject<UEnum>(ANY_PACKAGE, *EnumName, true);
				}

				if(Enum)
				{
					return FString::Printf(TEXT("Shared/Enums/%s"), *Enum->GetName());
				}
			}
		}

		return TEXT("");
	}

	FString GetDocumentationExcerptName(const UProperty* const Property)
	{
		if ( Property != NULL )
		{
			return Property->GetName();
		}

		return TEXT("");
	}

	TSharedPtr<IPropertyHandle> GetPropertyHandle( TSharedRef<FPropertyNode> PropertyNode, FNotifyHook* NotifyHook, TSharedPtr<IPropertyUtilities> PropertyUtilities )
	{
		TSharedPtr<IPropertyHandle> PropertyHandle;

		// Always check arrays first, many types can be static arrays
		if( FPropertyHandleArray::Supports( PropertyNode ) )
		{
			PropertyHandle = MakeShareable( new FPropertyHandleArray( PropertyNode, NotifyHook, PropertyUtilities ) );
		}
		else if( FPropertyHandleInt::Supports( PropertyNode ) )
		{
			PropertyHandle = MakeShareable( new FPropertyHandleInt( PropertyNode, NotifyHook, PropertyUtilities ) );
		}
		else if( FPropertyHandleFloat::Supports( PropertyNode ) )
		{
			PropertyHandle = MakeShareable( new FPropertyHandleFloat( PropertyNode, NotifyHook, PropertyUtilities ) );
		}
		else if ( FPropertyHandleDouble::Supports( PropertyNode ) )
		{
			PropertyHandle = MakeShareable( new FPropertyHandleDouble( PropertyNode, NotifyHook, PropertyUtilities ) );
		}
		else if( FPropertyHandleBool::Supports( PropertyNode ) )
		{
			PropertyHandle = MakeShareable( new FPropertyHandleBool( PropertyNode, NotifyHook, PropertyUtilities ) ) ;
		}
		else if( FPropertyHandleByte::Supports( PropertyNode ) )
		{
			PropertyHandle = MakeShareable( new FPropertyHandleByte( PropertyNode, NotifyHook, PropertyUtilities ) );
		}
		else if( FPropertyHandleObject::Supports( PropertyNode ) )
		{
			PropertyHandle = MakeShareable( new FPropertyHandleObject( PropertyNode, NotifyHook, PropertyUtilities ) );
		}
		else if( FPropertyHandleString::Supports( PropertyNode ) ) 
		{
			PropertyHandle = MakeShareable( new FPropertyHandleString( PropertyNode, NotifyHook, PropertyUtilities ) );
		}
		else if (FPropertyHandleText::Supports(PropertyNode))
		{
			PropertyHandle = MakeShareable(new FPropertyHandleText(PropertyNode, NotifyHook, PropertyUtilities));
		}
		else if( FPropertyHandleVector::Supports( PropertyNode ) )
		{
			PropertyHandle = MakeShareable( new FPropertyHandleVector( PropertyNode, NotifyHook, PropertyUtilities ) );
		}
		else if( FPropertyHandleRotator::Supports( PropertyNode ) )
		{
			PropertyHandle = MakeShareable( new FPropertyHandleRotator( PropertyNode, NotifyHook, PropertyUtilities ) );
		}
		else if (FPropertyHandleSet::Supports(PropertyNode))
		{
			PropertyHandle = MakeShareable( new FPropertyHandleSet( PropertyNode, NotifyHook, PropertyUtilities ) );
		}
		else if ( FPropertyHandleMap::Supports(PropertyNode) )
		{
			PropertyHandle = MakeShareable( new FPropertyHandleMap( PropertyNode, NotifyHook, PropertyUtilities ) );
		}
		else
		{
			// Untyped or doesn't support getting the property directly but the property is still valid(probably struct property)
			PropertyHandle = MakeShareable( new FPropertyHandleBase( PropertyNode, NotifyHook, PropertyUtilities ) ); 
		}

		return PropertyHandle;
	}

	static bool SupportsObjectPropertyButtons( UProperty* NodeProperty, bool bUsingAssetPicker )
	{
		return (NodeProperty->IsA<UObjectPropertyBase>() || NodeProperty->IsA<UInterfaceProperty>()) && (!bUsingAssetPicker || !SPropertyEditorAsset::Supports(NodeProperty));
	}
	
	static bool IsSoftObjectPath( const UProperty* Property )
	{
		const UStructProperty* StructProp = Cast<const UStructProperty>( Property );
		return StructProp && StructProp->Struct == TBaseStructure<FSoftObjectPath>::Get();
	}

	static bool IsSoftClassPath( const UProperty* Property )
	{
		const UStructProperty* StructProp = Cast<const UStructProperty>(Property);
		return StructProp && StructProp->Struct == TBaseStructure<FSoftClassPath>::Get();
	}

	void GetRequiredPropertyButtons( TSharedRef<FPropertyNode> PropertyNode, TArray<EPropertyButton::Type>& OutRequiredButtons, bool bUsingAssetPicker )
	{
		UProperty* NodeProperty = PropertyNode->GetProperty();

		// If no property is bound, don't create any buttons.
		if ( !NodeProperty )
		{
			return;
		}

		// If the property is an item of a const container, don't create any buttons.
		const UArrayProperty* OuterArrayProp = Cast<UArrayProperty>( NodeProperty->GetOuter() );
		const USetProperty* OuterSetProp = Cast<USetProperty>( NodeProperty->GetOuter() );
		const UMapProperty* OuterMapProp = Cast<UMapProperty>( NodeProperty->GetOuter() );

		//////////////////////////////
		// Handle a container property.
		if( NodeProperty->IsA(UArrayProperty::StaticClass()) || NodeProperty->IsA(USetProperty::StaticClass()) || NodeProperty->IsA(UMapProperty::StaticClass()) )
		{
			if (!NodeProperty->IsA(UArrayProperty::StaticClass()))
		{
				// Only Sets and Maps get a Documentation widget
				OutRequiredButtons.Add(EPropertyButton::Documentation);
			}
			
			if( !(NodeProperty->PropertyFlags & CPF_EditFixedSize) )
			{
				OutRequiredButtons.Add( EPropertyButton::Add );
				OutRequiredButtons.Add( EPropertyButton::Empty );
			}
		}

		//////////////////////////////
		// Handle an object property.
		
		
		if( SupportsObjectPropertyButtons( NodeProperty, bUsingAssetPicker ) )
		{
			//ignore this node if the consistency check should happen for the children
			bool bStaticSizedArray = (NodeProperty->ArrayDim > 1) && (PropertyNode->GetArrayIndex() == -1);
			if (!bStaticSizedArray)
			{
					if( PropertyNode->HasNodeFlags(EPropertyNodeFlags::EditInlineNew) )
					{
						// hmmm, seems like this code could be removed and the code inside the 'if <UClassProperty>' check
						// below could be moved outside the else....but is there a reason to allow class properties to have the
						// following buttons if the class property is marked 'editinline' (which is effectively what this logic is doing)
						if( !(NodeProperty->PropertyFlags & CPF_NoClear) )
						{
							OutRequiredButtons.Add( EPropertyButton::Clear );
						}
					}
					else
					{
						// ignore class properties
						if( (Cast<const UClassProperty>( NodeProperty ) == NULL) && (Cast<const USoftClassProperty>( NodeProperty ) == NULL) )
						{
							UObjectPropertyBase* ObjectProperty = Cast<UObjectPropertyBase>( NodeProperty );

							if( ObjectProperty && ObjectProperty->PropertyClass->IsChildOf( AActor::StaticClass() ) )
							{
								// add button for picking the actor from the viewport
								OutRequiredButtons.Add( EPropertyButton::PickActorInteractive );
							}
							else
							{
								// add button for filling the value of this item with the selected object from the GB
								OutRequiredButtons.Add( EPropertyButton::Use );
							}

							// add button to display the generic browser
							OutRequiredButtons.Add( EPropertyButton::Browse );

							// reference to object resource that isn't dynamically created (i.e. some content package)
							if( !(NodeProperty->PropertyFlags & CPF_NoClear) )
							{
								// add button to clear the text
								OutRequiredButtons.Add( EPropertyButton::Clear );
							}
							
							// Do not allow actor object properties to show the asset picker
							if( ( ObjectProperty && !ObjectProperty->PropertyClass->IsChildOf( AActor::StaticClass() ) ) || IsSoftObjectPath(NodeProperty) )
							{
								// add button for picking the asset from an asset picker
								OutRequiredButtons.Add( EPropertyButton::PickAsset );
							}
							else if( ObjectProperty && ObjectProperty->PropertyClass->IsChildOf( AActor::StaticClass() ) )
							{
								// add button for picking the actor from the scene outliner
								OutRequiredButtons.Add( EPropertyButton::PickActor );
							}
						}
					}
				}
			}

		//////////////////////////////
		// Handle a class property.

		UClassProperty* ClassProp = Cast<UClassProperty>(NodeProperty);
		if( ClassProp || IsSoftClassPath(NodeProperty))
		{
			OutRequiredButtons.Add( EPropertyButton::Use );			
			OutRequiredButtons.Add( EPropertyButton::Browse );

			UClass* Class = (ClassProp ? ClassProp->MetaClass : FEditorClassUtils::GetClassFromString(NodeProperty->GetMetaData("MetaClass")));

			if (Class && FKismetEditorUtilities::CanCreateBlueprintOfClass(Class) && !NodeProperty->HasMetaData("DisallowCreateNew"))
			{
				OutRequiredButtons.Add( EPropertyButton::NewBlueprint );
			}

			if( !(NodeProperty->PropertyFlags & CPF_NoClear) )
			{
				OutRequiredButtons.Add( EPropertyButton::Clear );
			}
		}
		else if (NodeProperty->IsA<USoftClassProperty>() )
		{
			OutRequiredButtons.Add( EPropertyButton::Use );
			
			OutRequiredButtons.Add( EPropertyButton::Browse );

			if( !(NodeProperty->PropertyFlags & CPF_NoClear) )
			{
				OutRequiredButtons.Add( EPropertyButton::Clear );
			}
		}

		if( OuterArrayProp )
		{
			if( PropertyNode->HasNodeFlags(EPropertyNodeFlags::SingleSelectOnly) && !(OuterArrayProp->PropertyFlags & CPF_EditFixedSize) )
			{
				if (OuterArrayProp->HasMetaData(TEXT("NoElementDuplicate")))
				{
					OutRequiredButtons.Add( EPropertyButton::Insert_Delete );
				}
				else
				{
					OutRequiredButtons.Add( EPropertyButton::Insert_Delete_Duplicate );
				}
			}
		}

		if (OuterSetProp || OuterMapProp)
		{
			UProperty* OuterNodeProperty = Cast<UProperty>(NodeProperty->GetOuter());

			if ( PropertyNode->HasNodeFlags(EPropertyNodeFlags::SingleSelectOnly) && !(OuterNodeProperty->PropertyFlags & CPF_EditFixedSize) )
			{
				OutRequiredButtons.Add(EPropertyButton::Delete);
			}
		}

	}
	
	void MakeRequiredPropertyButtons( const TSharedRef<FPropertyNode>& PropertyNode, const TSharedRef<IPropertyUtilities>& PropertyUtilities,  TArray< TSharedRef<SWidget> >& OutButtons, const TArray<EPropertyButton::Type>& ButtonsToIgnore, bool bUsingAssetPicker  )
	{
		const TSharedRef<FPropertyEditor> PropertyEditor = FPropertyEditor::Create( PropertyNode, PropertyUtilities );
		PropertyEditorHelpers::MakeRequiredPropertyButtons( PropertyEditor, OutButtons, ButtonsToIgnore, bUsingAssetPicker );
	}

	TSharedRef<SWidget> MakePropertyReorderHandle(const TSharedRef<FPropertyNode>& PropertyNode, TSharedPtr<SDetailSingleItemRow> InParentRow)
	{
		TSharedRef<SArrayRowHandle> Handle = SNew(SArrayRowHandle)
			.Content()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(5.0f, 0.0f)
				[
					SNew(SImage)
					.Image(FCoreStyle::Get().GetBrush("VerticalBoxDragIndicatorShort"))
				]
			]
		.ParentRow(InParentRow);
		TWeakPtr<FPropertyNode> NodePtr(PropertyNode);
		TAttribute<bool>::FGetter IsPropertyButtonEnabledDelegate = TAttribute<bool>::FGetter::CreateStatic(&IsPropertyButtonEnabled, NodePtr);
		TAttribute<bool> IsEnabledAttribute = TAttribute<bool>::Create(IsPropertyButtonEnabledDelegate);
		Handle->SetEnabled(IsEnabledAttribute);
		return Handle;
	}

	void MakeRequiredPropertyButtons( const TSharedRef< FPropertyEditor >& PropertyEditor, TArray< TSharedRef<SWidget> >& OutButtons, const TArray<EPropertyButton::Type>& ButtonsToIgnore, bool bUsingAssetPicker )
	{
		TArray< EPropertyButton::Type > RequiredButtons;
		GetRequiredPropertyButtons( PropertyEditor->GetPropertyNode(), RequiredButtons, bUsingAssetPicker );

		for( int32 ButtonIndex = 0; ButtonIndex < RequiredButtons.Num(); ++ButtonIndex )
		{
			if( !ButtonsToIgnore.Contains( RequiredButtons[ButtonIndex] ) )
			{
				OutButtons.Add( MakePropertyButton( RequiredButtons[ButtonIndex], PropertyEditor ) );
			}
		}
	}

	/**
	 * A helper function that retrieves the path name of the currently selected 
	 * item (the value that will be used to set the associated property from the 
	 * "use selection" button)
	 * 
	 * @param  PropertyNode		The associated property that the selection is a candidate for.
	 * @return Empty if the selection isn't compatible with the specified property, else the path-name of the object/class selected in the editor. 
	 */
	static FString GetSelectionPathNameForProperty(TSharedRef<FPropertyNode> PropertyNode)
	{
		FString SelectionPathName;

		UProperty* Property = PropertyNode->GetProperty();
		UClassProperty* ClassProperty = Cast<UClassProperty>(Property);
		USoftClassProperty* SoftClassProperty = Cast<USoftClassProperty>(Property);

		if (ClassProperty || SoftClassProperty)
		{
			UClass const* const SelectedClass = GEditor->GetFirstSelectedClass(ClassProperty ? ClassProperty->MetaClass : SoftClassProperty->MetaClass);
			if (SelectedClass != nullptr)
			{
				SelectionPathName = SelectedClass->GetPathName();
			}
		}
		else
		{
			UClass* ObjectClass = UObject::StaticClass();

			bool bMustBeLevelActor = false;
			UClass* RequiredInterface = nullptr;

			if (UObjectPropertyBase* ObjectProperty = Cast<UObjectPropertyBase>(Property))
			{
				ObjectClass = ObjectProperty->PropertyClass;
				bMustBeLevelActor = ObjectProperty->GetOwnerProperty()->GetBoolMetaData(TEXT("MustBeLevelActor"));
				RequiredInterface = ObjectProperty->GetOwnerProperty()->GetClassMetaData(TEXT("MustImplement"));
			}
			else if (UInterfaceProperty* InterfaceProperty = Cast<UInterfaceProperty>(Property))
			{
				ObjectClass = InterfaceProperty->InterfaceClass;
			}

			UObject* SelectedObject = nullptr;
			if (bMustBeLevelActor)
			{
				USelection* const SelectedSet = GEditor->GetSelectedActors();
				SelectedObject = SelectedSet->GetTop(ObjectClass, RequiredInterface);
			}
			else 
			{
				USelection* const SelectedSet = GEditor->GetSelectedSet(ObjectClass);
				SelectedObject = SelectedSet->GetTop(ObjectClass, RequiredInterface);
			}

			if (SelectedObject != nullptr)
			{
				SelectionPathName = SelectedObject->GetPathName();
			}
		}

		return SelectionPathName;
	}

	
	static bool IsPropertyButtonEnabled( TWeakPtr<FPropertyNode> PropertyNode )
	{
		return PropertyNode.IsValid() ? !PropertyNode.Pin()->IsEditConst() : false;
	}

	/**
	 * A helper method that checks to see if the editor's current selection is 
	 * compatible with the specified property.
	 * 
	 * @param  PropertyNode		The property you desire to set from the "use selected" button.
	 * @return False if the currently selected object is restricted for the specified property, true otherwise.
	 */
	static bool IsUseSelectedUnrestricted(TWeakPtr<FPropertyNode> PropertyNode)
	{
		TSharedPtr<FPropertyNode> PropertyNodePin = PropertyNode.Pin();
		return ( PropertyNodePin.IsValid() && IsPropertyButtonEnabled(PropertyNode) ) ? !PropertyNodePin->IsRestricted( GetSelectionPathNameForProperty( PropertyNodePin.ToSharedRef() ) ) : false;
	}

	/**
	 * A helper method that checks to see if the editor's current selection is 
	 * restricted, and then returns a tooltip explaining why (otherwise, it 
	 * returns a default explanation of the "use selected" button).
	 * 
	 * @param  PropertyNode		The property that would be set from the "use selected" button.
	 * @return A tooltip for the "use selected" button.
	 */
	static FText GetUseSelectedTooltip(TWeakPtr<FPropertyNode> PropertyNode)
	{
		TSharedPtr<FPropertyNode> PropertyNodePin = PropertyNode.Pin();
		FText ToolTip;
		if (PropertyNodePin.IsValid() && !PropertyNodePin->GenerateRestrictionToolTip(GetSelectionPathNameForProperty(PropertyNodePin.ToSharedRef()), ToolTip))
		{
			ToolTip = LOCTEXT("UseButtonToolTipText", "Use Selected Asset from Content Browser");
		}

		return ToolTip;
	}

	TSharedRef<SWidget> MakePropertyButton( const EPropertyButton::Type ButtonType, const TSharedRef< FPropertyEditor >& PropertyEditor )
	{
		TSharedPtr<SWidget> NewButton;

		TWeakPtr<FPropertyNode> WeakPropertyEditor = PropertyEditor->GetPropertyNode();

		TAttribute<bool>::FGetter IsPropertyButtonEnabledDelegate = TAttribute<bool>::FGetter::CreateStatic(&IsPropertyButtonEnabled, WeakPropertyEditor);
		TAttribute<bool> IsEnabledAttribute = TAttribute<bool>::Create( IsPropertyButtonEnabledDelegate );

		switch( ButtonType )
		{
		case EPropertyButton::Add:
			NewButton = PropertyCustomizationHelpers::MakeAddButton( FSimpleDelegate::CreateSP( PropertyEditor, &FPropertyEditor::AddItem ), FText(), IsEnabledAttribute );
			break;

		case EPropertyButton::Empty:
			NewButton = PropertyCustomizationHelpers::MakeEmptyButton( FSimpleDelegate::CreateSP( PropertyEditor, &FPropertyEditor::EmptyArray ), FText(), IsEnabledAttribute );
			break;

		case EPropertyButton::Delete:
		case EPropertyButton::Insert_Delete:
		case EPropertyButton::Insert_Delete_Duplicate:
			{
				FExecuteAction InsertAction; 
				FExecuteAction DeleteAction = FExecuteAction::CreateSP( PropertyEditor, &FPropertyEditor::DeleteItem );
				FExecuteAction DuplicateAction;

				if (ButtonType == EPropertyButton::Insert_Delete || ButtonType == EPropertyButton::Insert_Delete_Duplicate)
				{
					InsertAction = FExecuteAction::CreateSP(PropertyEditor, &FPropertyEditor::InsertItem);
				}

				if (ButtonType == EPropertyButton::Insert_Delete_Duplicate)
				{
					DuplicateAction = FExecuteAction::CreateSP( PropertyEditor, &FPropertyEditor::DuplicateItem );
				}

				NewButton = PropertyCustomizationHelpers::MakeInsertDeleteDuplicateButton( InsertAction, DeleteAction, DuplicateAction );
				NewButton->SetEnabled( IsEnabledAttribute );
				break;
			}

		case EPropertyButton::Browse:
			NewButton = PropertyCustomizationHelpers::MakeBrowseButton( FSimpleDelegate::CreateSP( PropertyEditor, &FPropertyEditor::BrowseTo ) );
			break;

		case EPropertyButton::Clear:
			NewButton = PropertyCustomizationHelpers::MakeClearButton( FSimpleDelegate::CreateSP( PropertyEditor, &FPropertyEditor::ClearItem ), FText(), IsEnabledAttribute );
			break;

		case EPropertyButton::Use:
			{
				FSimpleDelegate OnClickDelegate = FSimpleDelegate::CreateSP(PropertyEditor, &FPropertyEditor::UseSelected);
				TAttribute<bool>::FGetter EnabledDelegate = TAttribute<bool>::FGetter::CreateStatic(&IsUseSelectedUnrestricted, WeakPropertyEditor);
				TAttribute<FText>::FGetter TooltipDelegate = TAttribute<FText>::FGetter::CreateStatic(&GetUseSelectedTooltip, WeakPropertyEditor);

				NewButton = PropertyCustomizationHelpers::MakeUseSelectedButton(OnClickDelegate, TAttribute<FText>::Create(TooltipDelegate), TAttribute<bool>::Create(EnabledDelegate));
				break;
			}

		case EPropertyButton::PickAsset:
			NewButton = PropertyCustomizationHelpers::MakeAssetPickerAnchorButton( FOnGetAllowedClasses::CreateSP( PropertyEditor, &FPropertyEditor::OnGetClassesForAssetPicker ), FOnAssetSelected::CreateSP( PropertyEditor, &FPropertyEditor::OnAssetSelected ) );
			break;

		case EPropertyButton::PickActor:
			NewButton = PropertyCustomizationHelpers::MakeActorPickerAnchorButton( FOnGetActorFilters::CreateSP( PropertyEditor, &FPropertyEditor::OnGetActorFiltersForSceneOutliner ), FOnActorSelected::CreateSP( PropertyEditor, &FPropertyEditor::OnActorSelected ) );
			break;

		case EPropertyButton::PickActorInteractive:
			NewButton = PropertyCustomizationHelpers::MakeInteractiveActorPicker( FOnGetAllowedClasses::CreateSP( PropertyEditor, &FPropertyEditor::OnGetClassesForAssetPicker ), FOnShouldFilterActor(), FOnActorSelected::CreateSP( PropertyEditor, &FPropertyEditor::OnActorSelected ) );
			break;

		case EPropertyButton::NewBlueprint:
			NewButton = PropertyCustomizationHelpers::MakeNewBlueprintButton( FSimpleDelegate::CreateSP( PropertyEditor, &FPropertyEditor::MakeNewBlueprint )  );
			break;

		case EPropertyButton::EditConfigHierarchy:
			NewButton = PropertyCustomizationHelpers::MakeEditConfigHierarchyButton(FSimpleDelegate::CreateSP(PropertyEditor, &FPropertyEditor::EditConfigHierarchy));
			break;

		case EPropertyButton::Documentation:
			NewButton = PropertyCustomizationHelpers::MakeDocumentationButton(PropertyEditor);
			break;

		default:
			checkf( 0, TEXT( "Unknown button type" ) );
			break;
		}

		return NewButton.ToSharedRef();
	}

	void CollectObjectNodes( TSharedPtr<FPropertyNode> StartNode, TArray<FObjectPropertyNode*>& OutObjectNodes )
	{
		if( StartNode->AsObjectNode() != NULL )
		{
			OutObjectNodes.Add( StartNode->AsObjectNode() );
		}
			
		for( int32 ChildIndex = 0; ChildIndex < StartNode->GetNumChildNodes(); ++ChildIndex )
		{
			CollectObjectNodes( StartNode->GetChildNode( ChildIndex ), OutObjectNodes );
		}
		
	}

	TArray<FName> GetValidEnumsFromPropertyOverride(const UProperty* Property, const UEnum* InEnum)
	{
		TArray<FName> ValidEnumValues;

		static const FName ValidEnumValuesName("ValidEnumValues");
		if(Property->HasMetaData(ValidEnumValuesName))
		{
			TArray<FString> ValidEnumValuesAsString;

			Property->GetMetaData(ValidEnumValuesName).ParseIntoArray(ValidEnumValuesAsString, TEXT(","));
			for(auto& Value : ValidEnumValuesAsString)
			{
				Value.TrimStartInline();
				ValidEnumValues.Add(*InEnum->GenerateFullEnumName(*Value));
			}
		}

		return ValidEnumValues;
	}

	bool IsCategoryHiddenByClass(const TSharedPtr<FComplexPropertyNode>& InRootNode, FName CategoryName)
	{
		return InRootNode->AsObjectNode() && InRootNode->AsObjectNode()->GetHiddenCategories().Contains(CategoryName);
	}

	/**
	* Determines whether or not a property should be visible in the default generated detail layout
	*
	* @param PropertyNode	The property node to check
	* @param ParentNode	The parent property node to check
	* @return true if the property should be visible
	*/
	bool IsVisibleStandaloneProperty(const FPropertyNode& PropertyNode, const FPropertyNode& ParentNode)
	{
		const UProperty* Property = PropertyNode.GetProperty();
		const UArrayProperty* ParentArrayProperty = Cast<const UArrayProperty>(ParentNode.GetProperty());

		bool bIsVisibleStandalone = false;
		if (Property)
		{
			if (Property->IsA(UObjectPropertyBase::StaticClass()))
			{
				// Do not add this child node to the current map if its a single object property in a category (serves no purpose for UI)
				bIsVisibleStandalone = !ParentArrayProperty && (PropertyNode.GetNumChildNodes() == 0 || PropertyNode.GetNumChildNodes() > 1);
			}
			else if (Property->IsA(UArrayProperty::StaticClass()) || (Property->ArrayDim > 1 && PropertyNode.GetArrayIndex() == INDEX_NONE))
			{
				// Base array properties are always visible
				bIsVisibleStandalone = true;
			}
			else
			{
				bIsVisibleStandalone = true;
			}

		}

		return bIsVisibleStandalone;
	}

}

#undef LOCTEXT_NAMESPACE
