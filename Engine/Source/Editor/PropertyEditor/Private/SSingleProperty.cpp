// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SSingleProperty.h"
#include "UObject/UnrealType.h"
#include "AssetThumbnail.h"
#include "IPropertyUtilities.h"
#include "PropertyNode.h"
#include "Widgets/Text/STextBlock.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "Presentation/PropertyEditor/PropertyEditor.h"
#include "ObjectPropertyNode.h"
#include "PropertyEditorHelpers.h"
#include "UserInterface/PropertyEditor/SResetToDefaultPropertyEditor.h"
#include "Widgets/Colors/SColorPicker.h"


class FSinglePropertyUtilities : public IPropertyUtilities
{
public:

	FSinglePropertyUtilities( const TWeakPtr< SSingleProperty >& InView )
		: View( InView )
	{
	}

	virtual class FNotifyHook* GetNotifyHook() const override
	{
		TSharedPtr< SSingleProperty > PinnedView = View.Pin();
		return PinnedView->GetNotifyHook();
	}

	virtual void CreateColorPickerWindow( const TSharedRef< class FPropertyEditor >& PropertyEditor, bool bUseAlpha ) const override
	{
		TSharedPtr< SSingleProperty > PinnedView = View.Pin();

		if ( PinnedView.IsValid() )
		{
			PinnedView->CreateColorPickerWindow( PropertyEditor, bUseAlpha );
		}
	}

	virtual void EnqueueDeferredAction( FSimpleDelegate DeferredAction ) override
	{
		 // not implemented
	}

	virtual bool AreFavoritesEnabled() const override
	{
		// not implemented
		return false;
	}

	virtual void ToggleFavorite( const TSharedRef< class FPropertyEditor >& PropertyEditor ) const override
	{
		// not implemented
	}

	virtual bool IsPropertyEditingEnabled() const override
	{
		return true;
	}

	virtual void ForceRefresh() override {}

	virtual void RequestRefresh() override {}

	virtual TSharedPtr<class FAssetThumbnailPool> GetThumbnailPool() const override
	{
		// not implemented
		return NULL;
	}

	virtual void NotifyFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent) override
	{}

	virtual bool DontUpdateValueWhileEditing() const override
	{
		return false;
	}

	const TArray<TWeakObjectPtr<UObject>>& GetSelectedObjects() const override
	{
		static TArray<TWeakObjectPtr<UObject>> Empty;
		return Empty;
	}

	virtual bool HasClassDefaultObject() const override
	{
		return false;
	}

private:
	TWeakPtr< SSingleProperty > View;
};

void SSingleProperty::Construct( const FArguments& InArgs )
{
	PropertyName = InArgs._PropertyName;
	NameOverride = InArgs._NameOverride;
	NamePlacement = InArgs._NamePlacement;
	NotifyHook = InArgs._NotifyHook;
	PropertyFont = InArgs._PropertyFont;

	PropertyUtilities = MakeShareable( new FSinglePropertyUtilities( SharedThis( this ) ) );

	SetObject( InArgs._Object );
}

void SSingleProperty::SetObject( UObject* InObject )
{
	DestroyColorPicker();

	if( !RootPropertyNode.IsValid() )
	{
		RootPropertyNode = MakeShareable( new FObjectPropertyNode );
	}

	RootPropertyNode->RemoveAllObjects();
	ValueNode.Reset();

	if( InObject )
	{
		RootPropertyNode->AddObject( InObject );
	}


	FPropertyNodeInitParams InitParams;
	InitParams.ParentNode = NULL;
	InitParams.Property = NULL;
	InitParams.ArrayOffset = 0;
	InitParams.ArrayIndex = INDEX_NONE;
	// we'll generate the children
	InitParams.bAllowChildren = false;
	InitParams.bForceHiddenPropertyVisibility = false;

	RootPropertyNode->InitNode( InitParams );

	ValueNode = RootPropertyNode->GenerateSingleChild( PropertyName );

	bool bIsAcceptableProperty = false;
	// valid criteria for standalone properties 
	if( ValueNode.IsValid() )
	{
		UProperty* Property = ValueNode->GetProperty();

		bIsAcceptableProperty = true;
		// not an array property (dynamic or static)
		bIsAcceptableProperty &= !( Property->IsA( UArrayProperty::StaticClass() ) || (Property->ArrayDim > 1 && ValueNode->GetArrayIndex() == INDEX_NONE) );
		// not a struct property unless its a built in type like a vector
		bIsAcceptableProperty &= ( !Property->IsA( UStructProperty::StaticClass() ) || PropertyEditorHelpers::IsBuiltInStructProperty( Property ) );
	}

	if( bIsAcceptableProperty )
	{
		ValueNode->RebuildChildren();

		TSharedRef< FPropertyEditor > PropertyEditor = FPropertyEditor::Create( ValueNode.ToSharedRef(), TSharedPtr< IPropertyUtilities >( PropertyUtilities ).ToSharedRef() );
		ValueNode->SetDisplayNameOverride( NameOverride );

		TSharedPtr<SHorizontalBox> HorizontalBox;

		ChildSlot
		[
			SAssignNew( HorizontalBox, SHorizontalBox )
		];

		if( NamePlacement != EPropertyNamePlacement::Hidden )
		{
			HorizontalBox->AddSlot()
			.Padding( 2.0f, 0.0f, 2.0f, 4.0f )
			.AutoWidth()
			.VAlign( VAlign_Center )
			[
				SNew( SPropertyNameWidget, PropertyEditor )
				.DisplayResetToDefault( false )
			];
		}

		HorizontalBox->AddSlot()
		.Padding( 0.0f, 2.0f, 0.0f, 2.0f )
		.FillWidth(1.0f)
		.VAlign( VAlign_Center )
		[
			SNew( SPropertyValueWidget, PropertyEditor, PropertyUtilities.ToSharedRef() )
		];

		if (!PropertyEditor->GetPropertyHandle()->HasMetaData(TEXT("NoResetToDefault")))
		{
			HorizontalBox->AddSlot()
			.Padding( 2.0f )
			.AutoWidth()
			.VAlign( VAlign_Center )
			[
				SNew( SResetToDefaultPropertyEditor,  PropertyEditor->GetPropertyHandle() )
			];
		}
	}
	else
	{
		ChildSlot
		[
			SNew(STextBlock)
			.Font(PropertyFont)
			.Text(NSLOCTEXT("PropertyEditor", "SinglePropertyInvalidType", "Cannot Edit Inline"))
			.ToolTipText(NSLOCTEXT("PropertyEditor", "SinglePropertyInvalidType_Tooltip", "Properties of this type cannot be edited inline; edit it elsewhere"))
		];

		// invalid or missing property
		RootPropertyNode->RemoveAllObjects();
		ValueNode.Reset();
		RootPropertyNode.Reset();
	}
}

void SSingleProperty::SetOnPropertyValueChanged( FSimpleDelegate& InOnPropertyValueChanged )
{
	if( HasValidProperty() )
	{
		ValueNode->OnPropertyValueChanged().Add( InOnPropertyValueChanged );
	}
}

void SSingleProperty::ReplaceObjects( const TMap<UObject*, UObject*>& OldToNewObjectMap )
{
	if( HasValidProperty() )
	{
		TArray<UObject*> NewObjectList;
		bool bObjectsReplaced = false;

		// Scan all objects and look for objects which need to be replaced
		for ( TPropObjectIterator Itor( RootPropertyNode->ObjectIterator() ); Itor; ++Itor )
		{
			UObject* Replacement = OldToNewObjectMap.FindRef( Itor->Get() );
			if( Replacement )
			{
				bObjectsReplaced = true;
				NewObjectList.Add( Replacement );
			}
			else
			{
				NewObjectList.Add( Itor->Get() );
			}
		}

		// if any objects were replaced update the observed objects
		if( bObjectsReplaced )
		{
			SetObject( NewObjectList[0] );
		}
	}
}

void SSingleProperty::RemoveDeletedObjects( const TArray<UObject*>& DeletedObjects )
{

	if( HasValidProperty() )
	{
		// Scan all objects and look for objects which need to be replaced
		for ( TPropObjectIterator Itor( RootPropertyNode->ObjectIterator() ); Itor; ++Itor )
		{
			if( DeletedObjects.Contains( Itor->Get() ) )
			{
				SetObject( NULL );
				break;
			}
		}
	}
}

void SSingleProperty::CreateColorPickerWindow( const TSharedRef< class FPropertyEditor >& PropertyEditor, bool bUseAlpha )
{
	if( HasValidProperty() )
	{
		auto Node = PropertyEditor->GetPropertyNode();
		check( &Node.Get() == ValueNode.Get() );
		UProperty* Property = Node->GetProperty();
		check(Property);

		FReadAddressList ReadAddresses;
		Node->GetReadAddress( false, ReadAddresses, false );

		TArray<FLinearColor*> LinearColor;
		TArray<FColor*> DWORDColor;
		if( ReadAddresses.Num() ) 
		{
			const uint8* Addr = ReadAddresses.GetAddress(0);
			if( Addr )
			{
				if( Cast<UStructProperty>(Property)->Struct->GetFName() == NAME_Color )
				{
					DWORDColor.Add((FColor*)Addr);
				}
				else
				{
					check( Cast<UStructProperty>(Property)->Struct->GetFName() == NAME_LinearColor );
					LinearColor.Add((FLinearColor*)Addr);
				}
			}
		}

		FColorPickerArgs PickerArgs;
		PickerArgs.ParentWidget = AsShared();
		PickerArgs.bUseAlpha = bUseAlpha;
		PickerArgs.DisplayGamma = TAttribute<float>::Create( TAttribute<float>::FGetter::CreateUObject(GEngine, &UEngine::GetDisplayGamma) );
		PickerArgs.ColorArray = &DWORDColor;
		PickerArgs.LinearColorArray = &LinearColor;
		PickerArgs.OnColorCommitted = FOnLinearColorValueChanged::CreateSP( this, &SSingleProperty::SetColorPropertyFromColorPicker);

		OpenColorPicker(PickerArgs);
	}
}

void SSingleProperty::SetColorPropertyFromColorPicker(FLinearColor NewColor)
{
	if( HasValidProperty() )
	{
		UProperty* NodeProperty = ValueNode->GetProperty();
		check(NodeProperty);

		//@todo if multiple objects we need to iterate
		ValueNode->NotifyPreChange(NodeProperty, GetNotifyHook());

		FPropertyChangedEvent ChangeEvent(NodeProperty, EPropertyChangeType::ValueSet);
		ValueNode->NotifyPostChange( ChangeEvent, GetNotifyHook() );
	}
}
