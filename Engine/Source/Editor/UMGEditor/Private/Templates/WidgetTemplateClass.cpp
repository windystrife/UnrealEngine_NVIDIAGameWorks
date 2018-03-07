// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Templates/WidgetTemplateClass.h"

#if WITH_EDITOR
	#include "Editor.h"
#endif // WITH_EDITOR
#include "Widgets/SToolTip.h"
#include "IDocumentation.h"

#include "Blueprint/WidgetTree.h"

#define LOCTEXT_NAMESPACE "UMGEditor"

FWidgetTemplateClass::FWidgetTemplateClass()
	: WidgetClass(nullptr)
{
	// register for any objects replaced
	GEditor->OnObjectsReplaced().AddRaw(this, &FWidgetTemplateClass::OnObjectsReplaced);
}

FWidgetTemplateClass::FWidgetTemplateClass(TSubclassOf<UWidget> InWidgetClass)
	: WidgetClass(InWidgetClass)
{
	Name = WidgetClass->GetDisplayNameText();

	// register for any objects replaced
	GEditor->OnObjectsReplaced().AddRaw(this, &FWidgetTemplateClass::OnObjectsReplaced);
}

FWidgetTemplateClass::~FWidgetTemplateClass()
{
	GEditor->OnObjectsReplaced().RemoveAll(this);
}

FText FWidgetTemplateClass::GetCategory() const
{
	auto DefaultWidget = WidgetClass->GetDefaultObject<UWidget>();
	return DefaultWidget->GetPaletteCategory();
}

UWidget* FWidgetTemplateClass::Create(UWidgetTree* Tree)
{
	return CreateNamed(Tree, NAME_None);
}

const FSlateBrush* GetEditorIcon_Deprecated(UWidget* Widget);

const FSlateBrush* FWidgetTemplateClass::GetIcon() const
{
	// @todo UMG: remove after 4.12
	UWidget* DefaultWidget = WidgetClass->GetDefaultObject<UWidget>();
	return GetEditorIcon_Deprecated(DefaultWidget);
	// return FClassIconFinder::FindIconForClass(WidgetClass);
}

TSharedRef<IToolTip> FWidgetTemplateClass::GetToolTip() const
{
	return IDocumentation::Get()->CreateToolTip(WidgetClass->GetToolTipText(), nullptr, FString(TEXT("Shared/Types/")) + WidgetClass->GetName(), TEXT("Class"));
}

void FWidgetTemplateClass::OnObjectsReplaced(const TMap<UObject*, UObject*>& ReplacementMap)
{
	UObject* const* NewObject = ReplacementMap.Find(WidgetClass.Get());
	if (NewObject)
	{
		WidgetClass = CastChecked<UClass>(*NewObject);
	}
}

UWidget* FWidgetTemplateClass::CreateNamed(class UWidgetTree* Tree, FName NameOverride)
{
	if (NameOverride != NAME_None)
	{
		UObject* ExistingObject = StaticFindObject(UObject::StaticClass(), Tree, *NameOverride.ToString());
		if (ExistingObject != nullptr)
		{
			NameOverride = MakeUniqueObjectName(Tree, WidgetClass.Get(), NameOverride);
		}
	}

	UWidget* NewWidget = Tree->ConstructWidget<UWidget>(WidgetClass.Get(), NameOverride);
	NewWidget->OnCreationFromPalette();

	return NewWidget;
}

#undef LOCTEXT_NAMESPACE
