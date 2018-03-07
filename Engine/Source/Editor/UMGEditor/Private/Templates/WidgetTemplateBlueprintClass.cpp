// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Templates/WidgetTemplateBlueprintClass.h"

#include "Widgets/SToolTip.h"
#include "IDocumentation.h"
#include "WidgetBlueprint.h"
#include "Toolkits/AssetEditorManager.h"

#include "Kismet2/BlueprintEditorUtils.h"

#define LOCTEXT_NAMESPACE "UMGEditor"

FWidgetTemplateBlueprintClass::FWidgetTemplateBlueprintClass(const FAssetData& InWidgetAssetData, TSubclassOf<UUserWidget> InUserWidgetClass)
	: FWidgetTemplateClass(), WidgetAssetData(InWidgetAssetData)
{
	if (InUserWidgetClass)
	{
		WidgetClass = *InUserWidgetClass;
		Name = WidgetClass->GetDisplayNameText();
	}
	else
	{
		Name = FText::FromString(FName::NameToDisplayString(WidgetAssetData.AssetName.ToString(), false));
	}
}

FWidgetTemplateBlueprintClass::~FWidgetTemplateBlueprintClass()
{
}

FText FWidgetTemplateBlueprintClass::GetCategory() const
{
	if ( WidgetClass.Get() )
	{
		UUserWidget* DefaultUserWidget = WidgetClass->GetDefaultObject<UUserWidget>();
		return DefaultUserWidget->GetPaletteCategory();
	}
	else
	{
		//If the blueprint is unloaded we need to extract it from the asset metadata.
		FText FoundPaletteCategoryText = WidgetAssetData.GetTagValueRef<FText>(GET_MEMBER_NAME_CHECKED(UWidgetBlueprint, PaletteCategory));
		if (!FoundPaletteCategoryText.IsEmpty())
		{
			return FoundPaletteCategoryText;
		}
		else
		{
			auto DefaultUserWidget = UUserWidget::StaticClass()->GetDefaultObject<UUserWidget>();
			return DefaultUserWidget->GetPaletteCategory();
		}
	}
}

UWidget* FWidgetTemplateBlueprintClass::Create(UWidgetTree* Tree)
{
	// Load the blueprint asset if needed
	if (!WidgetClass.Get())
	{
		FString AssetPath = WidgetAssetData.ObjectPath.ToString();
		UWidgetBlueprint* LoadedWidget = LoadObject<UWidgetBlueprint>(nullptr, *AssetPath);
		WidgetClass = *LoadedWidget->GeneratedClass;
	}

	return FWidgetTemplateClass::CreateNamed(Tree, FName(*FBlueprintEditorUtils::GetClassNameWithoutSuffix(WidgetClass.Get())));
}

const FSlateBrush* GetEditorIcon_Deprecated(UWidget* Widget);

const FSlateBrush* FWidgetTemplateBlueprintClass::GetIcon() const
{
	// @todo UMG: remove after 4.12
	auto DefaultUserWidget = UUserWidget::StaticClass()->GetDefaultObject<UUserWidget>();
	return GetEditorIcon_Deprecated(DefaultUserWidget);
	// return FClassIconFinder::FindIconForClass(UUserWidget::StaticClass());
}

TSharedRef<IToolTip> FWidgetTemplateBlueprintClass::GetToolTip() const
{
	FText Description;

	FString DescriptionStr = WidgetAssetData.GetTagValueRef<FString>( GET_MEMBER_NAME_CHECKED( UBlueprint, BlueprintDescription ) );
	if ( !DescriptionStr.IsEmpty() )
	{
		DescriptionStr.ReplaceInline( TEXT( "\\n" ), TEXT( "\n" ) );
		Description = FText::FromString( MoveTemp(DescriptionStr) );
	}
	else
	{
		Description = Name;
	}

	return IDocumentation::Get()->CreateToolTip( Description, nullptr, FString( TEXT( "Shared/Types/" ) ) + Name.ToString(), TEXT( "Class" ) );
}

FReply FWidgetTemplateBlueprintClass::OnDoubleClicked()
{
	FAssetEditorManager::Get().OpenEditorForAsset( WidgetAssetData.GetAsset() );
	return FReply::Handled();
}

FAssetData FWidgetTemplateBlueprintClass::GetWidgetAssetData()
{
	return WidgetAssetData;
}

bool FWidgetTemplateBlueprintClass::Supports(UClass* InClass)
{
	return InClass != nullptr && InClass->IsChildOf(UWidgetBlueprint::StaticClass());
}

#undef LOCTEXT_NAMESPACE
