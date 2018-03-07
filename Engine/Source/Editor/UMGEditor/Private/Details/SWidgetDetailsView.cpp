// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Details/SWidgetDetailsView.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SCheckBox.h"

#include "IDetailsView.h"
#include "IDetailKeyframeHandler.h"
#include "Animation/UMGDetailKeyframeHandler.h"
#include "Details/DetailWidgetExtensionHandler.h"
#include "EditorClassUtils.h"

#include "Customizations/UMGDetailCustomizations.h"
#include "PropertyEditorModule.h"
#include "Customizations/SlateBrushCustomization.h"
#include "Customizations/SlateFontInfoCustomization.h"

#include "Customizations/WidgetNavigationCustomization.h"
#include "Customizations/CanvasSlotCustomization.h"
#include "Customizations/HorizontalAlignmentCustomization.h"
#include "Customizations/VerticalAlignmentCustomization.h"
#include "Customizations/SlateChildSizeCustomization.h"
#include "Customizations/TextJustifyCustomization.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Blueprint/WidgetTree.h"
#include "WidgetBlueprintEditorUtils.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "UMG"

void SWidgetDetailsView::Construct(const FArguments& InArgs, TSharedPtr<FWidgetBlueprintEditor> InBlueprintEditor)
{
	BlueprintEditor = InBlueprintEditor;

	// Create a property view
	FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FNotifyHook* NotifyHook = this;
	FDetailsViewArgs DetailsViewArgs(
		/*bUpdateFromSelection=*/ false,
		/*bLockable=*/ false,
		/*bAllowSearch=*/ true,
		FDetailsViewArgs::HideNameArea,
		/*bHideSelectionTip=*/ true,
		/*InNotifyHook=*/ NotifyHook,
		/*InSearchInitialKeyFocus=*/ false,
		/*InViewIdentifier=*/ NAME_None);
	DetailsViewArgs.DefaultsOnlyVisibility = EEditDefaultsOnlyNodeVisibility::Automatic;

	PropertyView = EditModule.CreateDetailView(DetailsViewArgs);

	// Create a handler for keyframing via the details panel
	TSharedRef<IDetailKeyframeHandler> KeyframeHandler = MakeShareable( new FUMGDetailKeyframeHandler( InBlueprintEditor ) );
	PropertyView->SetKeyframeHandler( KeyframeHandler );

	// Create a handler for property binding via the details panel
	TSharedRef<FDetailWidgetExtensionHandler> BindingHandler = MakeShareable( new FDetailWidgetExtensionHandler( InBlueprintEditor ) );
	PropertyView->SetExtensionHandler(BindingHandler);

	// Notify us of object selection changes so we can update the package re-mapping
	PropertyView->SetOnObjectArrayChanged(FOnObjectArrayChanged::CreateSP(this, &SWidgetDetailsView::OnPropertyViewObjectArrayChanged));

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 6)
		[
			SNew(SHorizontalBox)
			.Visibility(this, &SWidgetDetailsView::GetCategoryAreaVisibility)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 6, 0)
			[
				SNew(SBox)
				.WidthOverride(200.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SEditableTextBox)
					.SelectAllTextWhenFocused(true)
					.ToolTipText(LOCTEXT("CategoryToolTip", "Sets the category of the widget"))
					.HintText(LOCTEXT("Category", "Category"))
					.Text(this, &SWidgetDetailsView::GetCategoryText)
					.OnTextCommitted(this, &SWidgetDetailsView::HandleCategoryTextCommitted)
				]
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 6)
		[
			SNew(SHorizontalBox)
			.Visibility(this, &SWidgetDetailsView::GetNameAreaVisibility)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 3, 0)
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
				.Image(this, &SWidgetDetailsView::GetNameIcon)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 6, 0)
			[
				SNew(SBox)
				.WidthOverride(200.0f)
				.VAlign(VAlign_Center)
				[
					SAssignNew(NameTextBox, SEditableTextBox)
					.SelectAllTextWhenFocused(true)
					.HintText(LOCTEXT("Name", "Name"))
					.Text(this, &SWidgetDetailsView::GetNameText)
					.OnTextChanged(this, &SWidgetDetailsView::HandleNameTextChanged)
					.OnTextCommitted(this, &SWidgetDetailsView::HandleNameTextCommitted)
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SCheckBox)
				.IsChecked(this, &SWidgetDetailsView::GetIsVariable)
				.OnCheckStateChanged(this, &SWidgetDetailsView::HandleIsVariableChanged)
				.Padding(FMargin(3,1,3,1))
				[
					SNew(STextBlock)
					.Text(LOCTEXT("IsVariable", "Is Variable"))
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(15,0,0,0)
			[
				SAssignNew(ClassLinkArea, SBox)
			]
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			PropertyView.ToSharedRef()
		]
	];

	BlueprintEditor.Pin()->OnSelectedWidgetsChanging.AddRaw(this, &SWidgetDetailsView::OnEditorSelectionChanging);
	BlueprintEditor.Pin()->OnSelectedWidgetsChanged.AddRaw(this, &SWidgetDetailsView::OnEditorSelectionChanged);

	RegisterCustomizations();
	
	// Refresh the selection in the details panel.
	OnEditorSelectionChanged();
}

SWidgetDetailsView::~SWidgetDetailsView()
{
	if ( BlueprintEditor.IsValid() )
	{
		BlueprintEditor.Pin()->OnSelectedWidgetsChanging.RemoveAll(this);
		BlueprintEditor.Pin()->OnSelectedWidgetsChanged.RemoveAll(this);
	}

	// Unregister the property type layouts
	static FName PropertyEditor("PropertyEditor");
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(PropertyEditor);

	PropertyView->UnregisterInstancedCustomPropertyTypeLayout(TEXT("WidgetNavigation"));
	PropertyView->UnregisterInstancedCustomPropertyTypeLayout(TEXT("PanelSlot"));
	PropertyView->UnregisterInstancedCustomPropertyTypeLayout(TEXT("EHorizontalAlignment"));
	PropertyView->UnregisterInstancedCustomPropertyTypeLayout(TEXT("EVerticalAlignment"));
	PropertyView->UnregisterInstancedCustomPropertyTypeLayout(TEXT("SlateChildSize"));
	PropertyView->UnregisterInstancedCustomPropertyTypeLayout(TEXT("SlateBrush"));
	PropertyView->UnregisterInstancedCustomPropertyTypeLayout(TEXT("SlateFontInfo"));
	PropertyView->UnregisterInstancedCustomPropertyTypeLayout(TEXT("ETextJustify"));
}

void SWidgetDetailsView::RegisterCustomizations()
{
	PropertyView->RegisterInstancedCustomPropertyLayout(UWidget::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FBlueprintWidgetCustomization::MakeInstance, BlueprintEditor.Pin().ToSharedRef(), BlueprintEditor.Pin()->GetBlueprintObj()));

	static FName PropertyEditor("PropertyEditor");
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(PropertyEditor);

	PropertyView->RegisterInstancedCustomPropertyTypeLayout(TEXT("WidgetNavigation"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FWidgetNavigationCustomization::MakeInstance, BlueprintEditor.Pin().ToSharedRef()));
	PropertyView->RegisterInstancedCustomPropertyTypeLayout(TEXT("PanelSlot"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FCanvasSlotCustomization::MakeInstance, BlueprintEditor.Pin()->GetBlueprintObj()));
	PropertyView->RegisterInstancedCustomPropertyTypeLayout(TEXT("EHorizontalAlignment"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FHorizontalAlignmentCustomization::MakeInstance));
	PropertyView->RegisterInstancedCustomPropertyTypeLayout(TEXT("EVerticalAlignment"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FVerticalAlignmentCustomization::MakeInstance));
	PropertyView->RegisterInstancedCustomPropertyTypeLayout(TEXT("SlateChildSize"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FSlateChildSizeCustomization::MakeInstance));
	PropertyView->RegisterInstancedCustomPropertyTypeLayout(TEXT("SlateBrush"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FSlateBrushStructCustomization::MakeInstance, false));
	PropertyView->RegisterInstancedCustomPropertyTypeLayout(TEXT("SlateFontInfo"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FSlateFontInfoStructCustomization::MakeInstance));
	PropertyView->RegisterInstancedCustomPropertyTypeLayout(TEXT("ETextJustify"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FTextJustifyCustomization::MakeInstance));
}

void SWidgetDetailsView::OnEditorSelectionChanging()
{
	ClearFocusIfOwned();

	// We force the destruction of the currently monitored object when selection is about to change, to ensure all migrations occur
	// immediately.
	SelectedObjects.Empty();
	PropertyView->SetObjects(SelectedObjects);
}

void SWidgetDetailsView::OnEditorSelectionChanged()
{
	// Clear selection in the property view.
	SelectedObjects.Empty();
	PropertyView->SetObjects(SelectedObjects);

	// Add any selected widgets to the list of pending selected objects.
	TSet< FWidgetReference > SelectedWidgets = BlueprintEditor.Pin()->GetSelectedWidgets();
	if ( SelectedWidgets.Num() > 0 )
	{
		for ( FWidgetReference& WidgetRef : SelectedWidgets )
		{
			// Edit actions will go directly to the preview widget, changes will be
			// propagated to the template via SWidgetDetailsView::NotifyPostChange
			SelectedObjects.Add(WidgetRef.GetPreview());
		}
	}

	// Add any selected objects (non-widgets) to the pending selected objects.
	TSet< TWeakObjectPtr<UObject> > Selection = BlueprintEditor.Pin()->GetSelectedObjects();
	for ( TWeakObjectPtr<UObject> Selected : Selection )
	{
		if ( UObject* S = Selected.Get() )
		{
			SelectedObjects.Add(S);
		}
	}

	// If only 1 valid selected object exists, update the class link to point to the right class.
	if ( SelectedObjects.Num() == 1 && SelectedObjects[0].IsValid() )
	{
		ClassLinkArea->SetContent(FEditorClassUtils::GetSourceLink(SelectedObjects[0]->GetClass(), TWeakObjectPtr<UObject>()));
	}
	else
	{
		ClassLinkArea->SetContent(SNullWidget::NullWidget);
	}

	// Update the preview view to look at the current selection set.
	const bool bForceRefresh = false;
	PropertyView->SetObjects(SelectedObjects, bForceRefresh);
}

void SWidgetDetailsView::OnPropertyViewObjectArrayChanged(const FString& InTitle, const TArray<TWeakObjectPtr<UObject>>& InObjects)
{
	// Update the package remapping for all selected objects so that we can find the correct localization ID when editing text properties (since we edit a copy of the real data, not connected to the asset package)
	UBlueprint* UMGBlueprint = BlueprintEditor.Pin()->GetBlueprintObj();
	if (UMGBlueprint)
	{
		UPackage* UMGPackage = UMGBlueprint->GetOutermost();
		if (UMGPackage)
		{
			TMap<TWeakObjectPtr<UObject>, TWeakObjectPtr<UPackage>> ObjectToPackageMapping;

			for (const TWeakObjectPtr<UObject>& Object : InObjects)
			{
				ObjectToPackageMapping.Add(Object, UMGPackage);
			}

			PropertyView->SetObjectPackageOverrides(ObjectToPackageMapping);
		}
	}
}

void SWidgetDetailsView::ClearFocusIfOwned()
{
	static bool bIsReentrant = false;
	if ( !bIsReentrant )
	{
		bIsReentrant = true;
		// When the selection is changed, we may be potentially actively editing a property,
		// if this occurs we need need to immediately clear keyboard focus
		if ( FSlateApplication::Get().HasFocusedDescendants(AsShared()) )
		{
			FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::Mouse);
		}
		bIsReentrant = false;
	}
}

bool SWidgetDetailsView::IsWidgetCDOSelected() const
{
	if ( SelectedObjects.Num() == 1 )
	{
		UWidget* Widget = Cast<UWidget>(SelectedObjects[0].Get());
		
		// since we're passing preview widget when selecting root (owner) widget, 
		// this is also considered as selecting CDO so the category shows up correctly
		if (Widget && Widget == BlueprintEditor.Pin()->GetPreview())
		{
			return true;
		}
		else if (Widget && !Widget->HasAnyFlags(RF_ClassDefaultObject))
		{
			return false;
		}
	}

	return false;
}

EVisibility SWidgetDetailsView::GetNameAreaVisibility() const
{
	return IsWidgetCDOSelected() ? EVisibility::Collapsed : EVisibility::Visible;
}

EVisibility SWidgetDetailsView::GetCategoryAreaVisibility() const
{
	if ( SelectedObjects.Num() == 0 )
	{
		return EVisibility::Collapsed;
	}

	return IsWidgetCDOSelected() ? EVisibility::Visible : EVisibility::Collapsed;
}

void SWidgetDetailsView::HandleCategoryTextCommitted(const FText& Text, ETextCommit::Type CommitType)
{
	if ( SelectedObjects.Num() == 1 && !Text.IsEmptyOrWhitespace() )
	{
		if ( UUserWidget* Widget = Cast<UUserWidget>(SelectedObjects[0].Get()) )
		{
			UUserWidget* WidgetCDO = Widget->GetClass()->GetDefaultObject<UUserWidget>();
			WidgetCDO->PaletteCategory = Text;

			// Set the new category on the widget blueprint as well so that it's available when the blueprint isn't loaded.
			UWidgetBlueprint* Blueprint = BlueprintEditor.Pin()->GetWidgetBlueprintObj();
			Blueprint->PaletteCategory = Text.ToString();

			// Immediately force a rebuild so that all palettes update to show it in a new category.
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		}
	}
}

FText SWidgetDetailsView::GetCategoryText() const
{
	if ( SelectedObjects.Num() == 1 )
	{
		if ( UUserWidget* Widget = Cast<UUserWidget>(SelectedObjects[0].Get()) )
		{
			UUserWidget* WidgetCDO = Widget->GetClass()->GetDefaultObject<UUserWidget>();
			FText Category = WidgetCDO->PaletteCategory;
			if ( Category.EqualToCaseIgnored(UUserWidget::StaticClass()->GetDefaultObject<UUserWidget>()->PaletteCategory) )
			{
				return FText::GetEmpty();
			}
			else
			{
				return Category;
			}
		}
	}

	return FText::GetEmpty();
}

const FSlateBrush* GetEditorIcon_Deprecated(UWidget* Widget);

const FSlateBrush* SWidgetDetailsView::GetNameIcon() const
{
	if ( SelectedObjects.Num() == 1 )
	{
		UWidget* Widget = Cast<UWidget>(SelectedObjects[0].Get());
		if ( Widget )
		{
			// @todo UMG: remove after 4.12
			return GetEditorIcon_Deprecated(Widget);
			// return FClassIconFinder::FindIconForClass(Widget->GetClass());
		}
	}

	return nullptr;
}

FText SWidgetDetailsView::GetNameText() const
{
	if ( SelectedObjects.Num() == 1 )
	{
		UWidget* Widget = Cast<UWidget>(SelectedObjects[0].Get());
		if ( Widget )
		{
			return Widget->IsGeneratedName() ? FText::FromName(Widget->GetFName()) : Widget->GetLabelText();
		}
	}
	
	return FText::GetEmpty();
}

void SWidgetDetailsView::HandleNameTextChanged(const FText& Text)
{
	FText OutErrorMessage;
	if ( !HandleVerifyNameTextChanged(Text, OutErrorMessage) )
	{
		NameTextBox->SetError(OutErrorMessage);
	}
	else
	{
		NameTextBox->SetError(FText::GetEmpty());
	}
}

bool SWidgetDetailsView::HandleVerifyNameTextChanged(const FText& InText, FText& OutErrorMessage)
{
	if ( SelectedObjects.Num() == 1 )
	{
		UWidget* PreviewWidget = Cast<UWidget>(SelectedObjects[0].Get());
		
		TSharedRef<class FWidgetBlueprintEditor> BlueprintEditorRef = BlueprintEditor.Pin().ToSharedRef();

		FWidgetReference WidgetRef = BlueprintEditorRef->GetReferenceFromPreview(PreviewWidget);

		return FWidgetBlueprintEditorUtils::VerifyWidgetRename(BlueprintEditorRef, WidgetRef, InText, OutErrorMessage);
	}
	else
	{
		return false;
	}
}

void SWidgetDetailsView::HandleNameTextCommitted(const FText& Text, ETextCommit::Type CommitType)
{
	static bool IsReentrant = false;

	if ( !IsReentrant )
	{
		IsReentrant = true;
		if ( SelectedObjects.Num() == 1 )
		{
			FText DummyText;
			if ( HandleVerifyNameTextChanged(Text, DummyText) )
			{
				UWidget* Widget = Cast<UWidget>(SelectedObjects[0].Get());
				FWidgetBlueprintEditorUtils::RenameWidget(BlueprintEditor.Pin().ToSharedRef(), Widget->GetFName(), Text.ToString());
			}
		}
		IsReentrant = false;

		if (CommitType == ETextCommit::OnUserMovedFocus || CommitType == ETextCommit::OnCleared)
		{
			NameTextBox->SetError(FText::GetEmpty());
		}
	}
}

ECheckBoxState SWidgetDetailsView::GetIsVariable() const
{
	if ( SelectedObjects.Num() == 1 )
	{
		UWidget* Widget = Cast<UWidget>(SelectedObjects[0].Get());
		if ( Widget )
		{
			return Widget->bIsVariable ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		}
	}

	return ECheckBoxState::Unchecked;
}

void SWidgetDetailsView::HandleIsVariableChanged(ECheckBoxState CheckState)
{
	if ( SelectedObjects.Num() == 1 )
	{
		TSharedPtr<FWidgetBlueprintEditor> BPEditor = BlueprintEditor.Pin();

		UWidget* Widget = Cast<UWidget>(SelectedObjects[0].Get());
		UWidgetBlueprint* Blueprint = BlueprintEditor.Pin()->GetWidgetBlueprintObj();
		
		FWidgetReference WidgetRef = BPEditor->GetReferenceFromTemplate(Blueprint->WidgetTree->FindWidget(Widget->GetFName()));
		if ( WidgetRef.IsValid() )
		{
			UWidget* Template = WidgetRef.GetTemplate();
			UWidget* Preview = WidgetRef.GetPreview();

			const FScopedTransaction Transaction(LOCTEXT("VariableToggle", "Variable Toggle"));
			Template->Modify();
			Preview->Modify();

			Template->bIsVariable = Preview->bIsVariable = CheckState == ECheckBoxState::Checked ? true : false;

			// Refresh references and flush editors
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		}
	}
}

void SWidgetDetailsView::NotifyPreChange(FEditPropertyChain* PropertyAboutToChange)
{
	// During auto-key do not migrate values
	if( BlueprintEditor.Pin()->GetSequencer()->GetAutoChangeMode() == EAutoChangeMode::None )
	{
		TSharedPtr<FWidgetBlueprintEditor> Editor = BlueprintEditor.Pin();

		const bool bIsModify = true;
		Editor->MigrateFromChain(PropertyAboutToChange, bIsModify);
	}
}

void SWidgetDetailsView::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FEditPropertyChain* PropertyThatChanged)
{
	const static FName DesignerRebuildName("DesignerRebuild");

	if ( PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive && BlueprintEditor.Pin()->GetSequencer()->GetAutoChangeMode() == EAutoChangeMode::None )
	{
		TSharedPtr<FWidgetBlueprintEditor> Editor = BlueprintEditor.Pin();

		const bool bIsModify = false;
		Editor->MigrateFromChain(PropertyThatChanged, bIsModify);

		// Any time we migrate a property value we need to mark the blueprint as structurally modified so users don't need 
		// to recompile it manually before they see it play in game using the latest version.
		FBlueprintEditorUtils::MarkBlueprintAsModified(BlueprintEditor.Pin()->GetBlueprintObj());
	}

	// If the property that changed is marked as "DesignerRebuild" we invalidate
	// the preview.
	if ( PropertyChangedEvent.Property->HasMetaData(DesignerRebuildName) || PropertyThatChanged->GetActiveMemberNode()->GetValue()->HasMetaData(DesignerRebuildName) )
	{
		const bool bViewOnly = true;
		BlueprintEditor.Pin()->InvalidatePreview(bViewOnly);
	}
}

#undef LOCTEXT_NAMESPACE
