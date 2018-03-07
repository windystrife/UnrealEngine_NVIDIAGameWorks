// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WidgetReference.h"
#include "WidgetBlueprintEditor.h"

class FMenuBuilder;
class UWidgetBlueprint;
class UWidgetSlotPair;
class UWidgetTree;

//////////////////////////////////////////////////////////////////////////
// FWidgetBlueprintEditorUtils

class FWidgetBlueprintEditorUtils
{
public:
	static bool VerifyWidgetRename(TSharedRef<class FWidgetBlueprintEditor> BlueprintEditor, FWidgetReference Widget, const FText& NewName, FText& OutErrorMessage);

	static bool RenameWidget(TSharedRef<class FWidgetBlueprintEditor> BlueprintEditor, const FName& OldObjectName, const FString& NewDisplayName);

	static void CreateWidgetContextMenu(FMenuBuilder& MenuBuilder, TSharedRef<FWidgetBlueprintEditor> BlueprintEditor, FVector2D TargetLocation);

	static void CopyWidgets(UWidgetBlueprint* BP, TSet<FWidgetReference> Widgets);

	static void PasteWidgets(TSharedRef<FWidgetBlueprintEditor> BlueprintEditor, UWidgetBlueprint* BP, FWidgetReference ParentWidget, FName SlotName, FVector2D PasteLocation);

	static void DeleteWidgets(UWidgetBlueprint* BP, TSet<FWidgetReference> Widgets);

	static void CutWidgets(UWidgetBlueprint* BP, TSet<FWidgetReference> Widgets);

	static bool IsBindWidgetProperty(UProperty* InProperty);
	static bool IsBindWidgetProperty(UProperty* InProperty, bool& bIsOptional);

	static bool IsUsableWidgetClass(UClass* WidgetClass);

public:
	static void ExportWidgetsToText(TArray<UWidget*> WidgetsToExport, /*out*/ FString& ExportedText);

	static void ImportWidgetsFromText(UWidgetBlueprint* BP, const FString& TextToImport, /*out*/ TSet<UWidget*>& ImportedWidgetSet, /*out*/ TMap<FName, UWidgetSlotPair*>& PastedExtraSlotData);

	/** Exports the individual properties of an object to text and stores them in a map. */
	static void ExportPropertiesToText(UObject* Object, TMap<FName, FString>& ExportedProperties);

	/** Attempts to import any property in the map and apply it to a property with the same name on the object. */
	static void ImportPropertiesFromText(UObject* Object, const TMap<FName, FString>& ExportedProperties);

	static INamedSlotInterface* FindNamedSlotHostForContent(UWidget* WidgetTemplate, UWidgetTree* WidgetTree);

	static UWidget* FindNamedSlotHostWidgetForContent(UWidget* WidgetTemplate, UWidgetTree* WidgetTree);

	static void FindAllAncestorNamedSlotHostWidgetsForContent(TArray<FWidgetReference>& OutSlotHostWidgets, UWidget* WidgetTemplate, TSharedRef<FWidgetBlueprintEditor> BlueprintEditor);

	static bool RemoveNamedSlotHostContent(UWidget* WidgetTemplate, INamedSlotInterface* NamedSlotHost);

private:

	static void ExecuteOpenSelectedWidgetsForEdit( TSet<FWidgetReference> SelectedWidgets );

	static bool FindAndRemoveNamedSlotContent(UWidget* WidgetTemplate, UWidgetTree* WidgetTree);

	static bool CanOpenSelectedWidgetsForEdit( TSet<FWidgetReference> SelectedWidgets );

	static void BuildWrapWithMenu(FMenuBuilder& Menu, TSharedRef<FWidgetBlueprintEditor> BlueprintEditor, UWidgetBlueprint* BP, TSet<FWidgetReference> Widgets);

	static void WrapWidgets(TSharedRef<FWidgetBlueprintEditor> BlueprintEditor, UWidgetBlueprint* BP, TSet<FWidgetReference> Widgets, UClass* WidgetClass);

	static void BuildReplaceWithMenu(FMenuBuilder& Menu, TSharedRef<FWidgetBlueprintEditor> BlueprintEditor, UWidgetBlueprint* BP, TSet<FWidgetReference> Widgets);

	static void ReplaceWidgetWithSelectedTemplate(TSharedRef<FWidgetBlueprintEditor> BlueprintEditor, UWidgetBlueprint* BP, FWidgetReference Widget);

	static bool CanBeReplacedWithTemplate(TSharedRef<FWidgetBlueprintEditor> BlueprintEditor, UWidgetBlueprint* BP, FWidgetReference Widget);

	static void ReplaceWidgetWithChildren(TSharedRef<FWidgetBlueprintEditor> BlueprintEditor, UWidgetBlueprint* BP, FWidgetReference Widget);

	static void ReplaceWidgets(TSharedRef<FWidgetBlueprintEditor> BlueprintEditor, UWidgetBlueprint* BP, TSet<FWidgetReference> Widgets, UClass* WidgetClass);
};
