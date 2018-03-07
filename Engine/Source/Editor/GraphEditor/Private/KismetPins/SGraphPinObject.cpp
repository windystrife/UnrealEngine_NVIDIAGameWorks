// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "KismetPins/SGraphPinObject.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Editor.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "ScopedTransaction.h"
#include "Engine/Selection.h"
#include "AssetRegistryModule.h"

#define LOCTEXT_NAMESPACE "SGraphPinObject"

namespace GraphPinObjectDefs
{
	// Active Combo pin alpha
	static const float ActiveComboAlpha = 1.f;
	// InActive Combo pin alpha
	static const float InActiveComboAlpha = 0.6f;
	// Active foreground pin alpha
	static const float ActivePinForegroundAlpha = 1.f;
	// InActive foreground pin alpha
	static const float InactivePinForegroundAlpha = 0.15f;
	// Active background pin alpha
	static const float ActivePinBackgroundAlpha = 0.8f;
	// InActive background pin alpha
	static const float InactivePinBackgroundAlpha = 0.4f;
};

void SGraphPinObject::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	SGraphPin::Construct(SGraphPin::FArguments(), InGraphPinObj);
}

TSharedRef<SWidget>	SGraphPinObject::GetDefaultValueWidget()
{
	if( AllowSelfPinWidget() )
	{
		if(GraphPinObj->GetSchema()->IsSelfPin(*GraphPinObj))
		{
			return SNew(SEditableTextBox)
				.Style( FEditorStyle::Get(), "Graph.EditableTextBox" )
				.Text( this, &SGraphPinObject::GetValue )
				.SelectAllTextWhenFocused(false)
				.Visibility( this, &SGraphPinObject::GetDefaultValueVisibility )
				.IsReadOnly(true)
				.ForegroundColor( FSlateColor::UseForeground() );
		}
	}
	// Don't show literal buttons for component type objects
	if (GraphPinObj->GetSchema()->ShouldShowAssetPickerForPin(GraphPinObj))
	{
		return
			SNew(SHorizontalBox)
			.Visibility( this, &SGraphPin::GetDefaultValueVisibility )
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2,0)
			.MaxWidth(100.0f)
			[
				SAssignNew(AssetPickerAnchor, SComboButton)
				.ButtonStyle( FEditorStyle::Get(), "PropertyEditor.AssetComboStyle" )
				.ForegroundColor( this, &SGraphPinObject::OnGetComboForeground)
				.ContentPadding( FMargin(2,2,2,1) )
				.ButtonColorAndOpacity( this, &SGraphPinObject::OnGetWidgetBackground )
				.MenuPlacement(MenuPlacement_BelowAnchor)
				.ButtonContent()
				[
					SNew(STextBlock)
					.ColorAndOpacity( this, &SGraphPinObject::OnGetComboForeground )
					.TextStyle( FEditorStyle::Get(), "PropertyEditor.AssetClass" )
					.Font( FEditorStyle::GetFontStyle( "PropertyWindow.NormalFont" ) )
					.Text( this, &SGraphPinObject::OnGetComboTextValue )
					.ToolTipText( this, &SGraphPinObject::GetObjectToolTip )
				]
				.OnGetMenuContent(this, &SGraphPinObject::GenerateAssetPicker)
			]
			// Use button
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(1,0)
			.VAlign(VAlign_Center)
			[
				SAssignNew(UseButton, SButton)
				.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
				.ButtonColorAndOpacity( this, &SGraphPinObject::OnGetWidgetBackground )
				.OnClicked( GetOnUseButtonDelegate() )
				.ContentPadding(1.f)
				.ToolTipText(NSLOCTEXT("GraphEditor", "ObjectGraphPin_Use_Tooltip", "Use asset browser selection"))
				[
					SNew(SImage)
					.ColorAndOpacity( this, &SGraphPinObject::OnGetWidgetForeground )
					.Image( FEditorStyle::GetBrush(TEXT("PropertyWindow.Button_Use")) )
				]
			]
			// Browse button
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(1,0)
			.VAlign(VAlign_Center)
			[
				SAssignNew(BrowseButton, SButton)
				.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
				.ButtonColorAndOpacity( this, &SGraphPinObject::OnGetWidgetBackground )
				.OnClicked( GetOnBrowseButtonDelegate() )
				.ContentPadding(0)
				.ToolTipText(NSLOCTEXT("GraphEditor", "ObjectGraphPin_Browse_Tooltip", "Browse"))
				[
					SNew(SImage)
					.ColorAndOpacity( this, &SGraphPinObject::OnGetWidgetForeground )
					.Image( FEditorStyle::GetBrush(TEXT("PropertyWindow.Button_Browse")) )
				]
			];
	}

	return SNullWidget::NullWidget;
}

FOnClicked SGraphPinObject::GetOnUseButtonDelegate()
{
	return FOnClicked::CreateSP( this, &SGraphPinObject::OnClickUse );
}

FOnClicked SGraphPinObject::GetOnBrowseButtonDelegate()
{
	return FOnClicked::CreateSP( this, &SGraphPinObject::OnClickBrowse );
}

FText SGraphPinObject::GetObjectToolTip() const
{
	return GetValue();
}

FString SGraphPinObject::GetObjectToolTipAsString() const
{
	return GetValue().ToString();
}

FReply SGraphPinObject::OnClickUse()
{
	FEditorDelegates::LoadSelectedAssetsIfNeeded.Broadcast();

	UClass* ObjectClass = Cast<UClass>(GraphPinObj->PinType.PinSubCategoryObject.Get());
	if (ObjectClass != NULL)
	{
		UObject* SelectedObject = GEditor->GetSelectedObjects()->GetTop(ObjectClass);
		if(SelectedObject != NULL)
		{
			GraphPinObj->GetSchema()->TrySetDefaultObject(*GraphPinObj, SelectedObject);
		}
	}

	return FReply::Handled();
}

FReply SGraphPinObject::OnClickBrowse()
{
	const FAssetData& AssetData = GetAssetData(false);
	if (AssetData.IsValid())
	{
		TArray<FAssetData> Objects;
		Objects.Add(AssetData);

		GEditor->SyncBrowserToObjects(Objects);
	}
	return FReply::Handled();
}

TSharedRef<SWidget> SGraphPinObject::GenerateAssetPicker()
{
	// This class and its children are the classes that we can show objects for
	UClass* AllowedClass = Cast<UClass>(GraphPinObj->PinType.PinSubCategoryObject.Get());

	if (AllowedClass == NULL)
	{
		AllowedClass = UObject::StaticClass();
	}

	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	FAssetPickerConfig AssetPickerConfig;
	AssetPickerConfig.Filter.ClassNames.Add(AllowedClass->GetFName());
	AssetPickerConfig.bAllowNullSelection = true;
	AssetPickerConfig.Filter.bRecursiveClasses = true;
	AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateSP(this, &SGraphPinObject::OnAssetSelectedFromPicker);
	AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
	AssetPickerConfig.bAllowDragging = false;

	// Check with the node to see if there is any "AllowClasses" metadata for the pin
	FString ClassFilterString = GraphPinObj->GetOwningNode()->GetPinMetaData(GraphPinObj->PinName, FName(TEXT("AllowedClasses")));
	if( !ClassFilterString.IsEmpty() )
	{
		// Clear out the allowed class names and have the pin's metadata override.
		AssetPickerConfig.Filter.ClassNames.Empty();

		// Parse and add the classes from the metadata
		TArray<FString> CustomClassFilterNames;
		ClassFilterString.ParseIntoArray(CustomClassFilterNames, TEXT(","), true);
		for(auto It = CustomClassFilterNames.CreateConstIterator(); It; ++It)
		{
			AssetPickerConfig.Filter.ClassNames.Add(FName(**It));
		}
	}

	return
		SNew(SBox)
		.HeightOverride(300)
		.WidthOverride(300)
		[
			SNew(SBorder)
			.BorderImage( FEditorStyle::GetBrush("Menu.Background") )
			[
				ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
			]
		];
}

void SGraphPinObject::OnAssetSelectedFromPicker(const struct FAssetData& AssetData)
{
	const FAssetData& CurrentAssetData = GetAssetData(true);
	if(CurrentAssetData != AssetData)
	{
		const FScopedTransaction Transaction( NSLOCTEXT("GraphEditor", "ChangeObjectPinValue", "Change Object Pin Value" ) );
		GraphPinObj->Modify();

		// Close the asset picker
		AssetPickerAnchor->SetIsOpen(false);

		// Set the object found from the asset picker
		GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, AssetData.ObjectPath.ToString());
	}
}


FText SGraphPinObject::GetValue() const
{
	const FAssetData& CurrentAssetData = GetAssetData(true);
	FText Value;
	if (CurrentAssetData.IsValid())
	{
		Value = FText::FromString(CurrentAssetData.GetFullName());
	}
	else
	{
		if (GraphPinObj->GetSchema()->IsSelfPin(*GraphPinObj))
		{
			Value =  FText::FromString(GraphPinObj->PinName);
		}
		else
		{
			Value = FText::GetEmpty();
		}
	}
	return Value;
}

FText SGraphPinObject::GetObjectName() const
{
	FText Value = FText::GetEmpty();
	
	if (GraphPinObj != NULL)
	{
		const FAssetData& CurrentAssetData = GetAssetData(true);
		if (CurrentAssetData.IsValid())
		{
			Value = FText::FromString(CurrentAssetData.AssetName.ToString());
			int32 StringLen = Value.ToString().Len();

			//If string is too long, then truncate (eg. "abcdefgijklmnopq" is converted as "abcd...nopq")
			const int32 MaxAllowedLength = 16;
			if (StringLen > MaxAllowedLength)
			{
				//Take first 4 characters
				FString TruncatedStr(Value.ToString().Left(4));
				TruncatedStr += FString( TEXT("..."));
				
				//Take last 4 characters
				TruncatedStr += Value.ToString().Right(4);
				Value = FText::FromString(TruncatedStr);
			}
		}
	}
	return Value;
}

FText SGraphPinObject::GetDefaultComboText() const
{
	return LOCTEXT( "DefaultComboText", "Select Asset" );
}

FText SGraphPinObject::OnGetComboTextValue() const
{
	FText Value = GetDefaultComboText();
	
	if (GraphPinObj != nullptr)
	{
		const FAssetData& CurrentAssetData = GetAssetData(true);

		UObject* DefaultObject = GraphPinObj->DefaultObject;
		if (UField* Field = Cast<UField>(DefaultObject))
		{
			Value = Field->GetDisplayNameText();
		}
		else if (CurrentAssetData.IsValid())
		{
			Value = FText::FromString(CurrentAssetData.AssetName.ToString());
		}
	}
	return Value;
}

FSlateColor SGraphPinObject::OnGetComboForeground() const
{
	float Alpha = (IsHovered() || bOnlyShowDefaultValue) ? GraphPinObjectDefs::ActiveComboAlpha : GraphPinObjectDefs::InActiveComboAlpha;
	return FSlateColor( FLinearColor( 1.f, 1.f, 1.f, Alpha ) );
}

FSlateColor SGraphPinObject::OnGetWidgetForeground() const
{
	float Alpha = (IsHovered() || bOnlyShowDefaultValue) ? GraphPinObjectDefs::ActivePinForegroundAlpha : GraphPinObjectDefs::InactivePinForegroundAlpha;
	return FSlateColor( FLinearColor( 1.f, 1.f, 1.f, Alpha ) );
}

FSlateColor SGraphPinObject::OnGetWidgetBackground() const
{
	float Alpha = (IsHovered() || bOnlyShowDefaultValue) ? GraphPinObjectDefs::ActivePinBackgroundAlpha : GraphPinObjectDefs::InactivePinBackgroundAlpha;
	return FSlateColor( FLinearColor( 1.f, 1.f, 1.f, Alpha ) );
}

const FAssetData& SGraphPinObject::GetAssetData(bool bRuntimePath) const
{
	// For normal assets, the editor and runtime path are the same
	if (GraphPinObj->DefaultObject)
	{
		if (!GraphPinObj->DefaultObject->GetPathName().Equals(CachedAssetData.ObjectPath.ToString(), ESearchCase::CaseSensitive))
		{
			// This always uses the exact object pointed at
			CachedAssetData = FAssetData(GraphPinObj->DefaultObject, true);
		}
	}
	else if (!GraphPinObj->DefaultValue.IsEmpty())
	{
		FName ObjectPath = FName(*GraphPinObj->DefaultValue);
		if (ObjectPath != CachedAssetData.ObjectPath)
		{
			const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

			CachedAssetData = AssetRegistryModule.Get().GetAssetByObjectPath(ObjectPath);

			if (!CachedAssetData.IsValid())
			{
				FString PackageName = FPackageName::ObjectPathToPackageName(GraphPinObj->DefaultValue);
				FString PackagePath = FPackageName::GetLongPackagePath(PackageName);
				FString ObjectName = FPackageName::ObjectPathToObjectName(GraphPinObj->DefaultValue);

				// Fake one
				CachedAssetData = FAssetData(FName(*PackageName), FName(*PackagePath), FName(*ObjectName), UObject::StaticClass()->GetFName());
			}
		}
	}
	else
	{
		if (CachedAssetData.IsValid())
		{
			CachedAssetData = FAssetData();
		}
	}

	return CachedAssetData;
}

#undef LOCTEXT_NAMESPACE
