// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/SSettingsEditor.h"
#include "UObject/UnrealType.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SBoxPanel.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SScrollBox.h"
#include "EditorStyleSet.h"
#include "AnalyticsEventAttribute.h"
#include "EngineAnalytics.h"
#include "Interfaces/IAnalyticsProvider.h"
#include "Widgets/Text/STextBlock.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/SSettingsSectionHeader.h"
#include "SSettingsEditorCheckoutNotice.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/PlatformFile.h"

#define LOCTEXT_NAMESPACE "SSettingsEditor"

/* SSettingsEditor structors
 *****************************************************************************/

SSettingsEditor::~SSettingsEditor()
{
	Model->OnSelectionChanged().RemoveAll(this);
	SettingsContainer->OnCategoryModified().RemoveAll(this);
	FInternationalization::Get().OnCultureChanged().RemoveAll(this);
}


/* SSettingsEditor interface
 *****************************************************************************/

void SSettingsEditor::Construct( const FArguments& InArgs, const ISettingsEditorModelRef& InModel )
{
	bIsActiveTimerRegistered = false;
	Model = InModel;
	SettingsContainer = InModel->GetSettingsContainer();
	OnApplicationRestartRequiredDelegate = InArgs._OnApplicationRestartRequired;

	// initialize settings view
	FDetailsViewArgs DetailsViewArgs;
	{
		DetailsViewArgs.bAllowSearch = true;
		DetailsViewArgs.bHideSelectionTip = true;
		DetailsViewArgs.bLockable = false;
		DetailsViewArgs.bSearchInitialKeyFocus = true;
		DetailsViewArgs.bUpdatesFromSelection = false;
		DetailsViewArgs.NotifyHook = this;
		DetailsViewArgs.bShowOptions = true;
		DetailsViewArgs.bShowModifiedPropertiesOption = false;
		DetailsViewArgs.bAllowMultipleTopLevelObjects = true;
		DetailsViewArgs.bShowActorLabel = false;
		DetailsViewArgs.bCustomNameAreaLocation = true;
		DetailsViewArgs.bCustomFilterAreaLocation = true;
		DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
		DetailsViewArgs.bShowPropertyMatrixButton = false;
	}

	SettingsView = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor").CreateDetailView(DetailsViewArgs);
	SettingsView->SetVisibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &SSettingsEditor::HandleSettingsViewVisibility)));
	SettingsView->SetIsPropertyEditingEnabledDelegate(FIsPropertyEditingEnabled::CreateSP(this, &SSettingsEditor::HandleSettingsViewEnabled));

	TSharedPtr<FSettingsDetailRootObjectCustomization> RootObjectCustomization = MakeShareable(new FSettingsDetailRootObjectCustomization(Model, SettingsView.ToSharedRef()));
	RootObjectCustomization->Initialize();
	SettingsView->SetRootObjectCustomizationInstance(RootObjectCustomization);

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(16.0f, 0.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 16.0f)
			[
				// categories menu
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SAssignNew(CategoriesBox, SVerticalBox)
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SSpacer)
						.Size(FVector2D(24.0f, 0.0f))
					]
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(24.0f, 0.0f, 24.0f, 0.0f))
			[
				SNew(SSeparator)
				.Orientation(Orient_Vertical)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(0.0f, 16.0f)
			[
				SNew(SVerticalBox)
				.Visibility(this, &SSettingsEditor::HandleSettingsBoxVisibility)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SettingsView->GetFilterAreaWidget().ToSharedRef()
				]
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					// settings area
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SettingsView.ToSharedRef()
					]
					+ SOverlay::Slot()
					.Expose(CustomWidgetSlot)
				]
			]
		]
	];

	FInternationalization::Get().OnCultureChanged().AddSP(this, &SSettingsEditor::HandleCultureChanged);
	Model->OnSelectionChanged().AddSP(this, &SSettingsEditor::HandleModelSelectionChanged);
	SettingsContainer->OnCategoryModified().AddSP(this, &SSettingsEditor::HandleSettingsContainerCategoryModified);

	ReloadCategories();
}

/* FNotifyHook interface
 *****************************************************************************/

void SSettingsEditor::NotifyPostChange( const FPropertyChangedEvent& PropertyChangedEvent, class FEditPropertyChain* PropertyThatChanged )
{
	if (PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		UObject* Outer = PropertyChangedEvent.Property->GetOuter();

		// Note while there could be multiple objects in the details panel, only one is ever edited at once
		const UObject* ObjectBeingEdited = PropertyChangedEvent.GetObjectBeingEdited(0);
		if (ObjectBeingEdited != nullptr)
		{
		// Get the section from the edited object.  We cannot use the selected section as multiple sections can be shown at once in the settings details panel.
		ISettingsSectionPtr Section = Model->GetSectionFromSectionObject(ObjectBeingEdited);
			if (Section.IsValid())
		{
			FString RelativePath;
			bool bIsSourceControlled = false;
			bool bIsNewFile = false;

			// Attempt to checkout the file automatically
			if (ObjectBeingEdited->GetClass()->HasAnyClassFlags(CLASS_DefaultConfig))
			{
				RelativePath = ObjectBeingEdited->GetDefaultConfigFilename();
				bIsSourceControlled = true;
			}
			else if (ObjectBeingEdited->GetClass()->HasAnyClassFlags(CLASS_Config))
			{
				RelativePath = ObjectBeingEdited->GetClass()->GetConfigName();
			}

			FString FullPath = FPaths::ConvertRelativePathToFull(RelativePath);

			if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*FullPath))
			{
				bIsNewFile = true;
			}

			if (!bIsSourceControlled || !SettingsHelpers::CheckOutOrAddFile(FullPath))
			{
				SettingsHelpers::MakeWritable(FullPath);
			}
			
			RecordPreferenceChangedAnalytics(Section, PropertyChangedEvent);

			// Determine if the Property is an Array or Array Element
				bool bIsArrayOrArrayElement = PropertyThatChanged->GetActiveMemberNode()->GetValue()->IsA(UArrayProperty::StaticClass())
				|| PropertyThatChanged->GetActiveMemberNode()->GetValue()->ArrayDim > 1
				|| ((Outer != nullptr) && Outer->IsA(UArrayProperty::StaticClass()));

			bool bIsSetOrSetElement = PropertyThatChanged->GetActiveMemberNode()->GetValue()->IsA(USetProperty::StaticClass())
				|| ((Outer != nullptr) && Outer->IsA(USetProperty::StaticClass()));

			bool bIsMapOrMapElement = PropertyThatChanged->GetActiveMemberNode()->GetValue()->IsA(UMapProperty::StaticClass())
				|| ((Outer != nullptr) && Outer->IsA(UMapProperty::StaticClass()));

			if (Section->GetSettingsObject()->GetClass()->HasAnyClassFlags(CLASS_DefaultConfig) && !bIsArrayOrArrayElement && !bIsSetOrSetElement && !bIsMapOrMapElement)
			{
				Section->GetSettingsObject()->UpdateSinglePropertyInConfigFile(PropertyThatChanged->GetActiveMemberNode()->GetValue(), Section->GetSettingsObject()->GetDefaultConfigFilename());
			}
			else
			{
				Section->Save();
			}

			if (bIsNewFile && bIsSourceControlled)
			{
				SettingsHelpers::CheckOutOrAddFile(FullPath);
			}

			static const FName ConfigRestartRequiredKey = "ConfigRestartRequired";
			if (PropertyChangedEvent.Property->GetBoolMetaData(ConfigRestartRequiredKey) || PropertyChangedEvent.MemberProperty->GetBoolMetaData(ConfigRestartRequiredKey))
			{
				OnApplicationRestartRequiredDelegate.ExecuteIfBound();
			}
		}
	}
}
}


/* SSettingsEditor implementation
 *****************************************************************************/

TWeakObjectPtr<UObject> SSettingsEditor::GetSelectedSettingsObject() const
{
	ISettingsSectionPtr SelectedSection = Model->GetSelectedSection();

	if (SelectedSection.IsValid())
	{
		return SelectedSection->GetSettingsObject();
	}

	return nullptr;
}

TSharedRef<SWidget> SSettingsEditor::MakeCategoryWidget( const ISettingsCategoryRef& Category, const TArray<ISettingsSectionPtr>& InSections )
{
	// create section widgets
	TSharedRef<SVerticalBox> SectionsBox = SNew(SVerticalBox);

	
	if (InSections.Num() == 0)
	{
		return SNullWidget::NullWidget;
	}

	// list the sections
	for (const ISettingsSectionPtr Section : InSections)
	{
		SectionsBox->AddSlot()
			.HAlign(HAlign_Left)
			.Padding(0.0f, 10.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 4.0f, 0.0f)
					.VAlign(VAlign_Center)
					[
						SNew(SImage)
							.Image(FEditorStyle::Get().GetBrush("TreeArrow_Collapsed_Hovered"))
							.Visibility(this, &SSettingsEditor::HandleSectionLinkImageVisibility, Section)
					]

				+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(SHyperlink)
							.OnNavigate(this, &SSettingsEditor::HandleSectionLinkNavigate, Section)
							.Text(Section->GetDisplayName())
							.ToolTipText(Section->GetDescription())
					]
			];

		if (!Model->GetSelectedSection().IsValid())
		{
			Model->SelectSection(Section);
		}
	}

	// create category widget
	return SNew(SVerticalBox)

	+ SVerticalBox::Slot()
		.AutoHeight()
		[
			// category title
			SNew(STextBlock)
				.Font(FEditorStyle::GetFontStyle("SettingsEditor.CatgoryAndSectionFont"))
				.Text(Category->GetDisplayName())
		]

	+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			// sections list
			SectionsBox
		];
}


void SSettingsEditor::RecordPreferenceChangedAnalytics( ISettingsSectionPtr SelectedSection, const FPropertyChangedEvent& PropertyChangedEvent ) const
{
	UProperty* ChangedProperty = PropertyChangedEvent.MemberProperty;
	// submit analytics data
	if(FEngineAnalytics::IsAvailable() && ChangedProperty != nullptr && ChangedProperty->GetOwnerClass() != nullptr)
	{
		TArray<FAnalyticsEventAttribute> EventAttributes;
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("PropertySection"), SelectedSection->GetDisplayName().ToString()));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("PropertyClass"), ChangedProperty->GetOwnerClass()->GetName()));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("PropertyName"), ChangedProperty->GetName()));

		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.PreferencesChanged"), EventAttributes);
	}
}


void SSettingsEditor::ReloadCategories()
{
	CategoriesBox->ClearChildren();

	CategoriesBox->AddSlot()
	.AutoHeight()
	.HAlign(HAlign_Left)
	.Padding(0.0f, 5.0f, 0.0f, 18.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0.0f, 0.0f, 4.0f, 0.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SImage)
			.Image(FEditorStyle::Get().GetBrush("TreeArrow_Collapsed_Hovered"))
			.Visibility(this, &SSettingsEditor::HandleAllSectionsLinkImageVisibility)
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SHyperlink)
			.OnNavigate(this, &SSettingsEditor::HandleAllSectionsLinkNavigate)
			.Text(LOCTEXT("AllPropertiesLink", "All Settings"))
			.ToolTipText(LOCTEXT("AllPropertiesLink_Tooltip", "Show all settings"))
		]
	];

	TArray<ISettingsCategoryPtr> Categories;
	SettingsContainer->GetCategories(Categories);

	TArray<TSharedPtr<ISettingsSection>> CategorySettingsSections;

	TArray<UObject*> SettingsObjects;
	for (const ISettingsCategoryPtr Category : Categories)
	{
		CategorySettingsSections.Reset();

		Category->GetSections(CategorySettingsSections);

		// sort the sections alphabetically
		struct FSectionSortPredicate
		{
			FORCEINLINE bool operator()(ISettingsSectionPtr A, ISettingsSectionPtr B) const
			{
				if (!A.IsValid() && !B.IsValid())
				{
					return false;
				}

				if (A.IsValid() != B.IsValid())
				{
					return B.IsValid();
				}

				return (A->GetDisplayName().CompareTo(B->GetDisplayName()) < 0);
			}
		};

		CategorySettingsSections.Sort(FSectionSortPredicate());

		CategoriesBox->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 16.0f)
			[
				MakeCategoryWidget(Category.ToSharedRef(), CategorySettingsSections)
			];

		for(TSharedPtr<ISettingsSection>& Section : CategorySettingsSections)
		{
			UObject* SettingsObject = Section->GetSettingsObject().Get();
			if(SettingsObject)
			{
				SettingsObjects.Add(SettingsObject);
			}
		}
	}

	SettingsView->SetObjects(SettingsObjects);


}




/* SSettingsEditor callbacks
 *****************************************************************************/


void SSettingsEditor::HandleCultureChanged()
{
	ReloadCategories();
}

void SSettingsEditor::HandleModelSelectionChanged()
{
	ISettingsSectionPtr SelectedSection = Model->GetSelectedSection();

	if (SelectedSection.IsValid())
	{
		TSharedPtr<SWidget> CustomWidget = SelectedSection->GetCustomWidget().Pin();

		// show settings widget
		if (CustomWidget.IsValid())
		{
			CustomWidgetSlot->AttachWidget( CustomWidget.ToSharedRef() );
		}
		else
		{
			CustomWidgetSlot->AttachWidget( SNullWidget::NullWidget );
		}

		// focus settings widget
		TSharedPtr<SWidget> FocusWidget;

		if (CustomWidget.IsValid())
		{
			FocusWidget = CustomWidget;
		}
		else
		{
			FocusWidget = SettingsView;
		}

		FWidgetPath FocusWidgetPath;

		if (FSlateApplication::Get().GeneratePathToWidgetUnchecked(FocusWidget.ToSharedRef(), FocusWidgetPath))
		{
			FSlateApplication::Get().SetKeyboardFocus(FocusWidgetPath, EFocusCause::SetDirectly);
		}

		bShowingAllSettings = false;
	}
	else
	{
		bShowingAllSettings = true;
		CustomWidgetSlot->AttachWidget( SNullWidget::NullWidget );
	}

	// clear the global search terms when selecting a specific category
	SettingsView->ClearSearch();
}

void SSettingsEditor::HandleSectionLinkNavigate( ISettingsSectionPtr Section )
{
	Model->SelectSection(Section);
}

void SSettingsEditor::HandleAllSectionsLinkNavigate()
{
	Model->SelectSection(nullptr);
	SettingsView->RefreshRootObjectVisibility();
}

EVisibility SSettingsEditor::HandleAllSectionsLinkImageVisibility() const
{
	return bShowingAllSettings ? EVisibility::Visible : EVisibility::Hidden;
}

EVisibility SSettingsEditor::HandleSectionLinkImageVisibility( ISettingsSectionPtr Section ) const
{
	if (Model->GetSelectedSection() == Section)
	{
		return EVisibility::Visible;
	}

	return EVisibility::Hidden;
}

EVisibility SSettingsEditor::HandleSettingsBoxVisibility() const
{
	ISettingsSectionPtr SelectedSection = Model->GetSelectedSection();

	if (SelectedSection.IsValid() || bShowingAllSettings)
	{
		return EVisibility::Visible;
	}

	return EVisibility::Hidden;
}

void SSettingsEditor::HandleSettingsContainerCategoryModified( const FName& CategoryName )
{
	if ( !bIsActiveTimerRegistered )
	{
		bIsActiveTimerRegistered = true;
		RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SSettingsEditor::UpdateCategoriesCallback));
	}
}

EActiveTimerReturnType SSettingsEditor::UpdateCategoriesCallback(double InCurrentTime, float InDeltaTime)
{
	bIsActiveTimerRegistered = false;

	ReloadCategories();

	return EActiveTimerReturnType::Stop;
}

bool SSettingsEditor::HandleSettingsViewEnabled() const
{
	ISettingsSectionPtr SelectedSection = Model->GetSelectedSection();

	return (SelectedSection.IsValid() && SelectedSection->CanEdit()) || bShowingAllSettings;
}


EVisibility SSettingsEditor::HandleSettingsViewVisibility() const
{
	ISettingsSectionPtr SelectedSection = Model->GetSelectedSection();

	if (bShowingAllSettings || (SelectedSection.IsValid() && SelectedSection->GetSettingsObject().IsValid()))
	{
		return EVisibility::Visible;
	}

	return EVisibility::Hidden;
}


#undef LOCTEXT_NAMESPACE
