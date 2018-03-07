// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DeviceProfileDetails.h"
#include "HAL/IConsoleManager.h"
#include "Layout/Margin.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Input/SComboBox.h"
#include "EditorStyleSet.h"
#include "DeviceProfiles/DeviceProfile.h"
#include "DeviceProfiles/DeviceProfileManager.h"
#include "PropertyHandle.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailGroup.h"
#include "DetailCategoryBuilder.h"
#include "TextureLODSettingsDetails.h"
#include "Widgets/Input/SSearchBox.h"


#define LOCTEXT_NAMESPACE "DeviceProfileDetails"


////////////////////////////////////////////////
// FDeviceProfileDetails


void FDeviceProfileDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// Hide all the properties apart from the Console Variables.
	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory("DeviceSettings");

	TSharedPtr<IPropertyHandle> DeviceTypeHandle = DetailBuilder.GetProperty("DeviceType");
	DetailBuilder.HideProperty(DeviceTypeHandle);

	TSharedPtr<IPropertyHandle> MeshLODSettingsHandle = DetailBuilder.GetProperty("MeshLODSettings");
	DetailBuilder.HideProperty(MeshLODSettingsHandle);

	// Setup the parent profile panel
	ParentProfileDetails = MakeShareable(new FDeviceProfileParentPropertyDetails(&DetailBuilder));
	ParentProfileDetails->CreateParentPropertyView();

	// Setup the console variable editor
	ConsoleVariablesDetails = MakeShareable(new FDeviceProfileConsoleVariablesPropertyDetails(&DetailBuilder));
	ConsoleVariablesDetails->CreateConsoleVariablesPropertyView();

	TextureLODSettingsDetails = MakeShareable(new FDeviceProfileTextureLODSettingsDetails(&DetailBuilder));
	TextureLODSettingsDetails->CreateTextureLODSettingsPropertyView();
}


////////////////////////////////////////////////
// DeviceProfilePropertyConstants


/** Property layout constants, we will use this for consistent spacing across the details view */
namespace DeviceProfilePropertyConstants
{
	const FMargin PropertyPadding(2.0f, 0.0f, 2.0f, 0.0f);
	const FMargin CVarSelectionMenuPadding(10.0f, 2.0f);
}


////////////////////////////////////////////////
// DeviceProfileCVarFormatHelper


/** Some helper fucntions to assist us with displaying Console Variables from the CVars property */
namespace DeviceProfileCVarFormatHelper
{
	/** The available Console Variable Categories a CVar will be listed under. */
	enum ECVarGroup
	{
		CVG_Uncategorized = 0,
		CVG_Rendering,
		CVG_Physics,
		CVG_Network,
		CVG_Console,
		CVG_Compatibility,
		CVG_UserInterface,
		CVG_ScalabilityGroups,

		Max_CVarCategories,
	};


	/**
	 *	Convert the enum to a string version
	 *
	 *	@param CatEnum - The ECVarGroup index
	 *
	 *	@return The name of the group.
	 */
	FText CategoryTextFromEnum(DeviceProfileCVarFormatHelper::ECVarGroup CatEnum)
	{
		FText CategoryText;
		if (CatEnum == ECVarGroup::CVG_Uncategorized)
		{
			CategoryText = LOCTEXT("UncategorizedCVarGroupTitle","Uncategorized");
		}
		else if (CatEnum == ECVarGroup::CVG_Rendering)
		{
			CategoryText = LOCTEXT("RenderingCVarGroupTitle","Rendering");
		}
		else if (CatEnum == ECVarGroup::CVG_Physics)
		{
			CategoryText = LOCTEXT("PhysicsCVarGroupTitle","Physics");
		}
		else if (CatEnum == ECVarGroup::CVG_Network)
		{
			CategoryText = LOCTEXT("NetworkCVarGroupTitle","Network");
		}
		else if (CatEnum == ECVarGroup::CVG_Console)
		{
			CategoryText = LOCTEXT("ConsoleCVarGroupTitle","Console");
		}
		else if (CatEnum == ECVarGroup::CVG_Compatibility)
		{
			CategoryText = LOCTEXT("CompatibilityCVarGroupTitle","Compatibility");
		}
		else if (CatEnum == ECVarGroup::CVG_UserInterface)
		{
			CategoryText = LOCTEXT("UICVarGroupTitle", "User Interface");
		}
		else if (CatEnum == ECVarGroup::CVG_ScalabilityGroups)
		{
			CategoryText = LOCTEXT("ScalabilityGroupCVarGroupTitle", "Scalability Group");
		}

		return CategoryText;
	}


	/**
	*	Convert the Console Variable Category from the CVar prefix
	*
	*	@param InPrefix - The Prefix of the Console Variable
	*
	*	@return The name of the group.
	*/
	FText CategoryTextFromPrefix(const FString& InPrefix)
	{
		FString LowerPrefix = InPrefix.ToLower();

		FText CategoryText;
		if (LowerPrefix == TEXT("r") || LowerPrefix == TEXT("r."))
		{
			CategoryText = LOCTEXT("RenderingCVarGroupTitle", "Rendering");
		}
		else if (LowerPrefix == TEXT("p") || LowerPrefix == TEXT("p."))
		{
			CategoryText = LOCTEXT("PhysicsCVarGroupTitle", "Physics");
		}
		else if (LowerPrefix == TEXT("net") || LowerPrefix == TEXT("net."))
		{
			CategoryText = LOCTEXT("NetworkCVarGroupTitle", "Network");
		}
		else if (LowerPrefix == TEXT("con") || LowerPrefix == TEXT("con."))
		{
			CategoryText = LOCTEXT("ConsoleCVarGroupTitle", "Console");
		}
		else if (LowerPrefix == TEXT("compat") || LowerPrefix == TEXT("compat."))
		{
			CategoryText = LOCTEXT("CompatibilityCVarGroupTitle", "Compatibility");
		}
		else if (LowerPrefix == TEXT("ui") || LowerPrefix == TEXT("ui."))
		{
			CategoryText = LOCTEXT("UICVarGroupTitle", "User Interface");
		}
		else if (LowerPrefix == TEXT("sg") || LowerPrefix == TEXT("sg."))
		{
			CategoryText = LOCTEXT("ScalabilityGroupCVarGroupTitle", "Scalability Group");
		}
		else
		{
			CategoryText = LOCTEXT("UncategorizedCVarGroupTitle", "Uncategorized");
		}

		return CategoryText;
	}


	/**
	*	Convert the Console Variable Category from the CVar prefix
	*
	*	@param InPrefix - The Prefix of the Console Variable
	*
	*	@return The name of the group.
	*/
	FString CVarPrefixFromCategoryString(const FString& CategoryName)
	{
		FString CVarPrefix;

		FString LowerCategory = CategoryName.ToLower();
		if (LowerCategory == TEXT("rendering"))
		{
			CVarPrefix = TEXT("r");
		}
		else if (LowerCategory == TEXT("physics"))
		{
			CVarPrefix = TEXT("p");
		}
		else if (LowerCategory == TEXT("network"))
		{
			CVarPrefix = TEXT("net");
		}
		else if (LowerCategory == TEXT("console"))
		{
			CVarPrefix = TEXT("con");
		}
		else if (LowerCategory == TEXT("compatibility"))
		{
			CVarPrefix = TEXT("compat");
		}
		else if (LowerCategory == TEXT("user interface"))
		{
			CVarPrefix = TEXT("ui");
		}
		else if (LowerCategory == TEXT("scalability group"))
		{
			CVarPrefix = TEXT("sg");
		}

		return CVarPrefix;
	}
};


////////////////////////////////////////////////
// FConsoleVariablesAvailableVisitor


/**
 * Console variable visitor which collects our desired information from the console manager iterator
 */
class FConsoleVariablesAvailableVisitor
{
public:
	// @param Name must not be 0
	// @param CVar must not be 0
	static void OnConsoleVariable(const TCHAR *Name, IConsoleObject* CVar, TArray<TSharedPtr<FString>>& Sink)
	{
		if(CVar->AsVariable())
		{
			Sink.Add(MakeShareable(new FString(Name)));
		}

	}
};


////////////////////////////////////////////////
// SCVarSelectionPanel


/**
 * Slate Widget to display all available CVars for a given Console Variable group.
 */
class SCVarSelectionPanel : public SCompoundWidget
{
private:

	/** Delegate type to notify listeners that a CVar was selected for add */
	DECLARE_DELEGATE_OneParam(FOnCVarAddedDelegate, const FString&);

public:

	SLATE_BEGIN_ARGS(SCVarSelectionPanel) {}
		SLATE_DEFAULT_SLOT(FArguments, Content)
		SLATE_EVENT(FOnCVarAddedDelegate,OnCVarSelected)
	SLATE_END_ARGS()


	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, const FString& CVarPrefix);

	/**
	 * Handle the cvar selection from this panel
	 *
	 * @param CVar - The selected CVar
	 */
	FReply HandleCVarSelected(const TSharedPtr<FString> CVar);

	/** 
	 * Row generation widget for the list of available CVars for add
	 *
	 * @param InItem		- The CVar, as a string, we are creating a row for
	 * @param OwnerTable	- The table widget that this row will be added to
	 */
	TSharedRef<ITableRow> GenerateCVarItemRow(TSharedPtr<FString> InItem, const TSharedRef<STableViewBase>& OwnerTable);

	/**
	 * Called by Slate when the filter box changes text.
	 *
	 * @param InFilterText - The substring which we should use to filter CVars
	 */
	void OnFilterTextChanged(const FText& InFilterText);


private:

	/** Handle to the list view of selectable console variables */
	TSharedPtr<SListView<TSharedPtr<FString>>> CVarListView;

	/** Text entry to filter console variable strings */
	TSharedPtr<SSearchBox> CVarFilterBox;

	/** The collection of CVars for a groups selection panel*/
	TArray<TSharedPtr<FString>> CVarsToDisplay;

	/** The collection of CVars for a groups selection panel*/
	TArray<TSharedPtr<FString>> AllAvailableCVars;

	/** Delegate used to notify listeners that a CVar was selected for add */
	FOnCVarAddedDelegate OnCVarSelected;


	/** True if the search box will take keyboard focus next frame */
	bool bPendingFocusNextFrame;
};


void SCVarSelectionPanel::Construct(const FArguments& InArgs, const FString& CVarPrefix)
{
	OnCVarSelected = InArgs._OnCVarSelected;

	TArray<TSharedPtr<FString>> UnprocessedCVars;

	IConsoleManager::Get().ForEachConsoleObjectThatStartsWith(
		FConsoleObjectVisitor::CreateStatic< TArray<TSharedPtr<FString>>& >(
		&FConsoleVariablesAvailableVisitor::OnConsoleVariable,
		UnprocessedCVars), *CVarPrefix);

	if (DeviceProfileCVarFormatHelper::CategoryTextFromPrefix(CVarPrefix).ToString() == DeviceProfileCVarFormatHelper::CategoryTextFromEnum(DeviceProfileCVarFormatHelper::CVG_Uncategorized).ToString())
	{
		for(TArray<TSharedPtr<FString>>::TIterator CVarIt(UnprocessedCVars); CVarIt; ++CVarIt)
		{
			if((*CVarIt)->Contains(TEXT(".")) == false)
			{
				AllAvailableCVars.Add(*CVarIt);
				CVarsToDisplay.Add(*CVarIt);
			}
		}
	}
	else
	{
		AllAvailableCVars = UnprocessedCVars;
		CVarsToDisplay = UnprocessedCVars;
	}

	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(300)
		.HeightOverride(512)
		.Content()
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.Padding(FMargin(4.0f))
			.AutoHeight()
			[
				SAssignNew(CVarFilterBox,SSearchBox)
				.OnTextChanged(this, &SCVarSelectionPanel::OnFilterTextChanged)
			]
			+ SVerticalBox::Slot()
			[
				SAssignNew(CVarListView, SListView<TSharedPtr<FString>>)
				.ListItemsSource(&CVarsToDisplay)
				.OnGenerateRow(this, &SCVarSelectionPanel::GenerateCVarItemRow)			
			]
		]
	];
}


FReply SCVarSelectionPanel::HandleCVarSelected(const TSharedPtr<FString> CVar)
{
	OnCVarSelected.ExecuteIfBound(*CVar.Get());

	return FReply::Handled();
}


TSharedRef<ITableRow> SCVarSelectionPanel::GenerateCVarItemRow(TSharedPtr<FString> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
		[
			SNew(SButton)
			.ForegroundColor(FSlateColor::UseForeground())
			.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
			.OnClicked(this, &SCVarSelectionPanel::HandleCVarSelected, InItem)
			.ContentPadding(DeviceProfilePropertyConstants::CVarSelectionMenuPadding)
			.ToolTipText(LOCTEXT("CVarSelectionMenuTooltip","Select a Console Variable to add to the device profile"))
			[
				SNew(STextBlock)
				.Text(FText::FromString(*InItem))
			]
		];
}


void SCVarSelectionPanel::OnFilterTextChanged(const FText& InFilterText)
{
	const FString CurrentFilterText = InFilterText.ToString();

	// Recreate the list of available CVars using the filter
	CVarsToDisplay.Empty();
	for(auto& NextCVar : AllAvailableCVars)
	{
		if(CurrentFilterText.Len() == 0 || NextCVar->Contains(CurrentFilterText))
		{
			CVarsToDisplay.Add(NextCVar);
		}
	}

	CVarListView->RequestListRefresh();
}


////////////////////////////////////////////////
// FDeviceProfileParentPropertyDetails


FDeviceProfileParentPropertyDetails::FDeviceProfileParentPropertyDetails(IDetailLayoutBuilder* InDetailBuilder)
	: DetailBuilder(InDetailBuilder)
	, ActiveDeviceProfile(nullptr)
{
	ParentPropertyNameHandle = DetailBuilder->GetProperty("BaseProfileName");

	TArray<UObject*> OuterObjects;
	ParentPropertyNameHandle->GetOuterObjects(OuterObjects);
	if (OuterObjects.Num() == 1)
	{
		ActiveDeviceProfile = CastChecked<UDeviceProfile>(OuterObjects[0]);
	}
}


void FDeviceProfileParentPropertyDetails::CreateParentPropertyView()
{
	UDeviceProfile* ParentProfile = ActiveDeviceProfile ? Cast<UDeviceProfile>(ActiveDeviceProfile->Parent) : nullptr;
	while(ParentProfile != nullptr)
	{
		ParentProfile->OnCVarsUpdated().BindSP(this, &FDeviceProfileParentPropertyDetails::OnParentPropertyChanged);
		ParentProfile = Cast<UDeviceProfile>(ParentProfile->Parent);
	}

	DetailBuilder->HideProperty(ParentPropertyNameHandle);

	FString CurrentParentName;
	ParentPropertyNameHandle->GetValue(CurrentParentName);

	IDetailCategoryBuilder& ParentDetailCategory = DetailBuilder->EditCategory("ParentDeviceProfile");
	IDetailGroup& ParentNameGroup = ParentDetailCategory.AddGroup(TEXT("ParentProfileName"), LOCTEXT("ParentProfileOptionsGroupTitle", "Parent Profile Name"));

	ParentNameGroup.HeaderRow()
	[
		SNew(SBox)
		.Padding(DeviceProfilePropertyConstants::PropertyPadding)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("DeviceProfileSelectParentPropertyTitle", "Selected Parent:"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
	];

	AvailableParentProfiles.Add(MakeShareable(new FString(LOCTEXT("NoParentSelection", "None").ToString())));
	if(ActiveDeviceProfile != nullptr)
	{
		TArray<UDeviceProfile*> AllPossibleParents;
		UDeviceProfileManager::Get().GetAllPossibleParentProfiles(ActiveDeviceProfile, AllPossibleParents);

		for(auto& NextProfile : AllPossibleParents)
		{
			AvailableParentProfiles.Add(MakeShareable(new FString(NextProfile->GetName())));
		}
	}


	FText ParentNameText = CurrentParentName.Len() > 0 ? FText::FromString(CurrentParentName) : LOCTEXT("NoParentSelection", "None");
	ParentNameGroup.AddWidgetRow()
	[
		SNew(SComboBox<TSharedPtr<FString>>)
		.OptionsSource(&AvailableParentProfiles)
		.OnGenerateWidget(this, &FDeviceProfileParentPropertyDetails::HandleDeviceProfileParentComboBoxGenarateWidget)
		.OnSelectionChanged(this, &FDeviceProfileParentPropertyDetails::HandleDeviceProfileParentSelectionChanged)
		.Content()
		[
			SNew(STextBlock)
			.Text(ParentNameText)
		]
	];
	
	// If we have a parent, display Console Variable information
	if(ActiveDeviceProfile != nullptr && ActiveDeviceProfile->BaseProfileName.Len() > 0)
	{
		// Get a list of the current profiles CVar names to use as a filter when showing parent CVars
		TArray<FString> DeviceProfileCVarNames;
		for (auto& NextActiveProfileCVar : ActiveDeviceProfile->CVars)
		{
			FString CVarName;
			FString CVarValue;
			NextActiveProfileCVar.Split(TEXT("="), &CVarName, &CVarValue);
			
			DeviceProfileCVarNames.Add(CVarName);
		}

		TMap<FString, FString> ParentCVarInformation;
		ActiveDeviceProfile->GatherParentCVarInformationRecursively(ParentCVarInformation);

		IDetailGroup* ParentCVarsGroup = nullptr;
		for(auto& ParentCVar : ParentCVarInformation)
		{
			FString ParentCVarName;
			FString ParentCVarValue;
			ParentCVar.Value.Split(TEXT("="), &ParentCVarName, &ParentCVarValue);

			// Do not display Parent CVars if the child has them overridden
			bool bDisplayCVar = DeviceProfileCVarNames.Find(ParentCVarName) == INDEX_NONE;
			if(bDisplayCVar)
			{
				if(ParentCVarsGroup == nullptr)
				{
					ParentCVarsGroup = &ParentDetailCategory.AddGroup(TEXT("ParentProfileOptions"), LOCTEXT("ParentConsoleOptionsGroupTitle", "Parent Console Variables"));

					ParentCVarsGroup->HeaderRow()
					[
						SNew(SBox)
						.Padding(DeviceProfilePropertyConstants::PropertyPadding)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("DeviceProfileParentCVarsTitle", "Inherited Console Variables"))
							.Font(IDetailLayoutBuilder::GetDetailFont())
						]
					];
				}

				ParentCVarsGroup->AddWidgetRow()
				.IsEnabled(true)
				.Visibility(EVisibility::Visible)
				.NameContent()
				[
					SNew(STextBlock)
					.Text(FText::FromString(ParentCVarName))
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
				.ValueContent()
				[
					SNew(STextBlock)
					.Text(FText::FromString(ParentCVarValue))
					.Font(IDetailLayoutBuilder::GetDetailFont())
				];
			}
		}
	}
}


void FDeviceProfileParentPropertyDetails::HandleDeviceProfileParentSelectionChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	ActiveDeviceProfile->BaseProfileName = *NewSelection.Get() == LOCTEXT("NoParentSelection", "None").ToString() ? TEXT("") : *NewSelection.Get();
	// -> Refresh the UI of the Details view to display the parent selection
	DetailBuilder->ForceRefreshDetails();

}


TSharedRef<SWidget> FDeviceProfileParentPropertyDetails::HandleDeviceProfileParentComboBoxGenarateWidget(TSharedPtr<FString> InItem)
{
	return SNew(SBox)
	.Padding(DeviceProfilePropertyConstants::CVarSelectionMenuPadding)
	[
		SNew(STextBlock)
		.Text(FText::FromString(*InItem))
	];
}


void FDeviceProfileParentPropertyDetails::OnParentPropertyChanged()
{
	DetailBuilder->ForceRefreshDetails();
}


////////////////////////////////////////////////
// FDeviceProfileConsoleVariablesPropertyDetails


FDeviceProfileConsoleVariablesPropertyDetails::FDeviceProfileConsoleVariablesPropertyDetails(IDetailLayoutBuilder* InDetailBuilder)
: DetailBuilder(InDetailBuilder)
{
	CVarsHandle = DetailBuilder->GetProperty("CVars");
}


void FDeviceProfileConsoleVariablesPropertyDetails::CreateConsoleVariablesPropertyView()
{
	FSimpleDelegate OnCVarPropertyChangedDelegate = FSimpleDelegate::CreateSP(this, &FDeviceProfileConsoleVariablesPropertyDetails::OnCVarPropertyChanged);
	CVarsHandle->SetOnPropertyValueChanged(OnCVarPropertyChangedDelegate);

	DetailBuilder->HideProperty(CVarsHandle);
	TSharedPtr<IPropertyHandleArray> CVarsArrayHandle = CVarsHandle->AsArray();

	IDetailCategoryBuilder& CVarDetailCategory = DetailBuilder->EditCategory("ConsoleVariables");

	uint32 CVarCount = 0;
	ensure(CVarsArrayHandle->GetNumElements(CVarCount) == FPropertyAccess::Success);

	// Sort the properties handles into Categories
	TMap<FString, TArray<TSharedRef<IPropertyHandle>>> CategoryPropertyMap;

	// Add all the CVar groups, even if these are empty
	for (int32 CategoryIdx = 0; CategoryIdx < (int32)DeviceProfileCVarFormatHelper::Max_CVarCategories; CategoryIdx++)
	{
		CategoryPropertyMap.FindOrAdd(DeviceProfileCVarFormatHelper::CategoryTextFromEnum((DeviceProfileCVarFormatHelper::ECVarGroup)CategoryIdx).ToString());
	}

	for (uint32 CVarPropertyIdx = 0; CVarPropertyIdx < CVarCount; CVarPropertyIdx++)
	{
		// Get the current CVar as a string
		FString CVarValue;
		TSharedRef<IPropertyHandle> CVarElementHandle = CVarsArrayHandle->GetElement(CVarPropertyIdx);
		ensure(CVarElementHandle->GetValue(CVarValue) == FPropertyAccess::Success);

		// Parse the CVar entry and obtain the name and Category Name
		const FString CVarName = CVarValue.Left(CVarValue.Find(TEXT("=")));
		int32 CategoryEndIndex = CVarName.Find(TEXT("."));

		FString CVarAbrv = CVarName.Left(CategoryEndIndex);
		FText CVarCategory = DeviceProfileCVarFormatHelper::CategoryTextFromPrefix(CVarAbrv);

		TArray<TSharedRef<IPropertyHandle>>* CurrentPropertyCategoryGroup = CategoryPropertyMap.Find(CVarCategory.ToString());
		CurrentPropertyCategoryGroup->Add(CVarElementHandle);
	}


	// Put the property handles into the UI group for the details view.
	for (auto& Current : CategoryPropertyMap)
	{
		const TArray<TSharedRef<IPropertyHandle>>& CurrentGroupsProperties = Current.Value;
		const FText GroupName = FText::FromString(Current.Key);

		FString CVarPrefix = DeviceProfileCVarFormatHelper::CVarPrefixFromCategoryString(GroupName.ToString());
		if (CVarPrefix.Len() > 0)
		{
			CVarPrefix += TEXT(".");
		}

		// Find the Property table UI Group for the current CVar
		IDetailGroup& CVarGroup = CVarDetailCategory.AddGroup(*GroupName.ToString(), GroupName);
		CVarDetailGroups.Add(GroupName.ToString(), &CVarGroup);
		CVarGroup.HeaderRow()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding(DeviceProfilePropertyConstants::PropertyPadding)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(GroupName)
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
				+ SHorizontalBox::Slot()
				.Padding(DeviceProfilePropertyConstants::PropertyPadding)
				.AutoWidth()
				[
					SNew(SComboButton)
					.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
					.ContentPadding(4.0f)
					.ForegroundColor(FSlateColor::UseForeground())
					.IsFocusable(false)
					.ButtonContent()
					[
						SNew(SImage)
						.Image(FEditorStyle::GetBrush("PropertyWindow.Button_AddToArray"))
					]
					.MenuContent()
					[
						SNew(SCVarSelectionPanel, CVarPrefix)
						.OnCVarSelected(this, &FDeviceProfileConsoleVariablesPropertyDetails::HandleCVarAdded)
					]
				]
				+ SHorizontalBox::Slot()
				.Padding(DeviceProfilePropertyConstants::PropertyPadding)
				.AutoWidth()
				[
					SNew(SButton)
					.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
					.OnClicked(this, &FDeviceProfileConsoleVariablesPropertyDetails::OnRemoveAllFromGroup, GroupName)
					.ContentPadding(4.0f)
					.ForegroundColor(FSlateColor::UseForeground())
					.IsFocusable(false)
					[
						SNew(SImage)
						.Image(FEditorStyle::GetBrush("PropertyWindow.Button_EmptyArray"))
					]
				]
			];

		for (auto& Property : CurrentGroupsProperties)
		{
			CreateRowWidgetForCVarProperty(Property, CVarGroup);
		}

		CVarDetailCategory.InitiallyCollapsed(true);
	}
}


void FDeviceProfileConsoleVariablesPropertyDetails::CreateRowWidgetForCVarProperty(TSharedPtr<IPropertyHandle> InProperty, IDetailGroup& InGroup) const
{
	FString UnformattedCVar;
	ensure(InProperty->GetValue(UnformattedCVar) == FPropertyAccess::Success);

	const int32 CVarNameValueSplitIdx = UnformattedCVar.Find(TEXT("="));
	ensure(CVarNameValueSplitIdx != INDEX_NONE);
	const FString CVarName = UnformattedCVar.Left(CVarNameValueSplitIdx);
	const FString CVarValueAsString = UnformattedCVar.Right(UnformattedCVar.Len() - (CVarNameValueSplitIdx + 1));

	InGroup.AddWidgetRow()
	.IsEnabled(true)
	.Visibility(EVisibility::Visible)
	.NameContent()
	[
		SNew(STextBlock)
		.Text(FText::FromString(CVarName))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		[
			SNew(SEditableTextBox)
			.Text(FText::FromString(CVarValueAsString))
			.SelectAllTextWhenFocused(true)
			.OnTextCommitted(this, &FDeviceProfileConsoleVariablesPropertyDetails::OnCVarValueCommited, InProperty)
		]
		+ SHorizontalBox::Slot()
		.Padding(DeviceProfilePropertyConstants::PropertyPadding)
		.AutoWidth()
		[
			SNew(SButton)
			.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
			.OnClicked(this, &FDeviceProfileConsoleVariablesPropertyDetails::OnRemoveCVarProperty, InProperty)
			.ContentPadding(4.0f)
			.ForegroundColor(FSlateColor::UseForeground())
			.IsFocusable(false)
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush("PropertyWindow.Button_Delete"))
			]
		]
	];
}


void FDeviceProfileConsoleVariablesPropertyDetails::HandleCVarAdded(const FString& SelectedCVar)
{
	if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*SelectedCVar))
	{
		const FString CompleteCVarString = FString::Printf(TEXT("%s=%s"), *SelectedCVar, *CVar->GetString());

		TArray<void*> RawPtrs;
		CVarsHandle->AccessRawData(RawPtrs);

		// Update the CVars with the selection
		{
			CVarsHandle->NotifyPreChange();
			for (void* RawPtr : RawPtrs)
			{
				TArray<FString>& Array = *(TArray<FString>*)RawPtr;

				Array.Add(CompleteCVarString);
			}
			CVarsHandle->NotifyPostChange();
		}

		// Update the UI with the selection
		// -> Close the selection menu
		FSlateApplication::Get().DismissAllMenus();
	}
}


void FDeviceProfileConsoleVariablesPropertyDetails::OnCVarValueCommited(const FText& CommentText, ETextCommit::Type CommitInfo, TSharedPtr<IPropertyHandle> CVarPropertyHandle)
{
	if (CVarPropertyHandle->IsValidHandle())
	{
		const TSharedPtr<IPropertyHandle> ParentHandle = CVarPropertyHandle->GetParentHandle();
		const TSharedPtr<IPropertyHandleArray> ParentArrayHandle = ParentHandle->AsArray();

		// Get the current CVar as a string
		FString OldCompleteCVarValue;
		ensure(CVarPropertyHandle->GetValue(OldCompleteCVarValue) == FPropertyAccess::Success);

		// Update the CVar. I.e. MyCVar=1
		const FString CVarLHS = OldCompleteCVarValue.Left(OldCompleteCVarValue.Find(TEXT("=")));
		const FString CVarRHS = CommentText.ToString();
		
		FString NewCompleteCVar = FString::Printf(TEXT("%s=%s"), *CVarLHS, *CVarRHS);

		if(OldCompleteCVarValue!=NewCompleteCVar)
		{
			CVarPropertyHandle->SetValue(NewCompleteCVar);
		}
	}
}


FReply FDeviceProfileConsoleVariablesPropertyDetails::OnRemoveCVarProperty(TSharedPtr<IPropertyHandle> CVarPropertyHandle)
{
	if (CVarPropertyHandle->IsValidHandle())
	{
		const TSharedPtr<IPropertyHandle> ParentHandle = CVarPropertyHandle->GetParentHandle();
		const TSharedPtr<IPropertyHandleArray> ParentArrayHandle = ParentHandle->AsArray();

		ParentArrayHandle->DeleteItem(CVarPropertyHandle->GetIndexInArray());
	}

	OnCVarPropertyChanged();

	return FReply::Handled();
}


FReply FDeviceProfileConsoleVariablesPropertyDetails::OnRemoveAllFromGroup(FText GroupName)
{
	const TSharedPtr<IPropertyHandleArray> CVarsArrayHandle = CVarsHandle->AsArray();

	uint32 CVarCount = 0;
	ensure(CVarsArrayHandle->GetNumElements(CVarCount) == FPropertyAccess::Success);

	FString CVarPrefix = DeviceProfileCVarFormatHelper::CVarPrefixFromCategoryString(GroupName.ToString());

	for (int32 CVarPropertyIdx = CVarCount-1; CVarPropertyIdx >= 0; CVarPropertyIdx--)
	{
		// Get the current CVar as a string
		FString CVarValue;
		TSharedRef<IPropertyHandle> CVarElementHandle = CVarsArrayHandle->GetElement(CVarPropertyIdx);
		ensure(CVarElementHandle->GetValue(CVarValue) == FPropertyAccess::Success);

		// Parse the CVar entry and obtain the name and Category Name
		const FString CVarName = CVarValue.Left(CVarValue.Find(TEXT("=")));
		int32 CategoryEndIndex = CVarName.Find(TEXT("."));

		FString CurrentCVarPrefix = CVarName.Left(CategoryEndIndex);
		if(CVarPrefix == CurrentCVarPrefix)
		{
			CVarsArrayHandle->DeleteItem(CVarElementHandle->GetIndexInArray());
		}
	}

	OnCVarPropertyChanged();

	return FReply::Handled();
}


void FDeviceProfileConsoleVariablesPropertyDetails::OnCVarPropertyChanged()
{
	DetailBuilder->ForceRefreshDetails();
}

#undef LOCTEXT_NAMESPACE
