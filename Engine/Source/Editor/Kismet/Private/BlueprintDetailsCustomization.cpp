// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BlueprintDetailsCustomization.h"
#include "UObject/StructOnScope.h"
#include "IDetailChildrenBuilder.h"
#include "Widgets/Layout/SSpacer.h"
#include "DetailWidgetRow.h"
#include "Engine/UserDefinedStruct.h"
#include "Misc/MessageDialog.h"
#include "UObject/UObjectIterator.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Engine/Engine.h"
#include "EdMode.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EdGraph/EdGraphNode_Documentation.h"
#include "Layout/WidgetPath.h"
#include "SlateOptMacros.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "EdGraphNode_Comment.h"
#include "Components/ChildActorComponent.h"
#include "Components/TimelineComponent.h"
#include "Kismet2/ComponentEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Variable.h"
#include "K2Node_ComponentBoundEvent.h"
#include "K2Node_Tunnel.h"
#include "K2Node_Composite.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_FunctionTerminator.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_MathExpression.h"
#include "Notifications/NotificationManager.h"
#include "Notifications/SNotificationList.h"
#include "ScopedTransaction.h"
#include "PropertyRestriction.h"
#include "BlueprintEditorModes.h"
#include "BlueprintEditorSettings.h"
#include "DetailLayoutBuilder.h"
#include "IDetailPropertyRow.h"
#include "DetailCategoryBuilder.h"
#include "IDetailsView.h"
#include "Widgets/Colors/SColorPicker.h"
#include "SKismetInspector.h"
#include "SSCSEditor.h"
#include "SPinTypeSelector.h"
#include "NodeFactory.h"
#include "Kismet2/Kismet2NameValidators.h"
#include "Widgets/Layout/SWidgetSwitcher.h"

#include "ModuleManager.h"
#include "ISequencerModule.h"
#include "AnimatedPropertyKey.h"

#include "PropertyCustomizationHelpers.h"

#include "Kismet2/BlueprintEditorUtils.h"

#include "ObjectEditorUtils.h"

#include "Editor/SceneOutliner/Private/SSocketChooser.h"

#include "IDocumentationPage.h"
#include "IDocumentation.h"
#include "Widgets/Input/STextComboBox.h"

#include "UObject/TextProperty.h"

#define LOCTEXT_NAMESPACE "BlueprintDetailsCustomization"

namespace BlueprintDocumentationDetailDefs
{
	/** Minimum size of the details title panel */
	static const float DetailsTitleMinWidth = 125.f;
	/** Maximum size of the details title panel */
	static const float DetailsTitleMaxWidth = 300.f;
	/** magic number retrieved from SGraphNodeComment::GetWrapAt() */
	static const float DetailsTitleWrapPadding = 32.0f;
};

void FBlueprintDetails::AddEventsCategory(IDetailLayoutBuilder& DetailBuilder, UProperty* VariableProperty)
{
	UBlueprint* BlueprintObj = GetBlueprintObj();
	check(BlueprintObj);

	if ( UObjectProperty* ComponentProperty = Cast<UObjectProperty>(VariableProperty) )
	{
		UClass* PropertyClass = ComponentProperty->PropertyClass;

		// Check for Ed Graph vars that can generate events
		if ( PropertyClass && BlueprintObj->AllowsDynamicBinding() )
		{
			if ( FBlueprintEditorUtils::CanClassGenerateEvents(PropertyClass) )
			{
				for ( TFieldIterator<UMulticastDelegateProperty> PropertyIt(PropertyClass, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt )
				{
					UProperty* Property = *PropertyIt;

					FName PropertyName = ComponentProperty->GetFName();

					// Check for multicast delegates that we can safely assign
					if ( !Property->HasAnyPropertyFlags(CPF_Parm) && Property->HasAllPropertyFlags(CPF_BlueprintAssignable) )
					{
						FName EventName = Property->GetFName();
						FText EventText = Property->GetDisplayNameText();

						IDetailCategoryBuilder& EventCategory = DetailBuilder.EditCategory(TEXT("Events"), LOCTEXT("Events", "Events"), ECategoryPriority::Uncommon);

						EventCategory.AddCustomRow(EventText)
						.NameContent()
						[
							SNew(SHorizontalBox)
							.ToolTipText(Property->GetToolTipText())

							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(0, 0, 5, 0)
							[
								SNew(SImage)
								.Image(FEditorStyle::GetBrush("GraphEditor.Event_16x"))
							]

							+ SHorizontalBox::Slot()
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Font(IDetailLayoutBuilder::GetDetailFont())
								.Text(EventText)
							]
						]
						.ValueContent()
						.MinDesiredWidth(150)
						.MaxDesiredWidth(200)
						[
							SNew(SButton)
							.ButtonStyle(FEditorStyle::Get(), "FlatButton.Success")
							.HAlign(HAlign_Center)
							.OnClicked(this, &FBlueprintVarActionDetails::HandleAddOrViewEventForVariable, EventName, PropertyName, TWeakObjectPtr<UClass>(PropertyClass))
							.ForegroundColor(FSlateColor::UseForeground())
							[
								SNew(SWidgetSwitcher)
								.WidgetIndex(this, &FBlueprintVarActionDetails::HandleAddOrViewIndexForButton, EventName, PropertyName)

								+ SWidgetSwitcher::Slot()
								[
									SNew(STextBlock)
									.Font(FEditorStyle::GetFontStyle(TEXT("BoldFont")))
									.Text(LOCTEXT("ViewEvent", "View"))
								]

								+ SWidgetSwitcher::Slot()
								[
									SNew(SImage)
									.Image(FEditorStyle::GetBrush("Plus"))
								]
							]
						];
					}
				}
			}
		}
	}
}

FReply FBlueprintDetails::HandleAddOrViewEventForVariable(const FName EventName, FName PropertyName, TWeakObjectPtr<UClass> PropertyClass)
{
	UBlueprint* BlueprintObj = GetBlueprintObj();

	// Find the corresponding variable property in the Blueprint
	UObjectProperty* VariableProperty = FindField<UObjectProperty>(BlueprintObj->SkeletonGeneratedClass, PropertyName);

	if ( VariableProperty )
	{
		if ( !FKismetEditorUtilities::FindBoundEventForComponent(BlueprintObj, EventName, VariableProperty->GetFName()) )
		{
			FKismetEditorUtilities::CreateNewBoundEventForClass(PropertyClass.Get(), EventName, BlueprintObj, VariableProperty);
		}
		else
		{
			const UK2Node_ComponentBoundEvent* ExistingNode = FKismetEditorUtilities::FindBoundEventForComponent(BlueprintObj, EventName, VariableProperty->GetFName());
			if ( ExistingNode )
			{
				FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(ExistingNode);
			}
		}
	}

	return FReply::Handled();
}

int32 FBlueprintDetails::HandleAddOrViewIndexForButton(const FName EventName, FName PropertyName) const
{
	UBlueprint* BlueprintObj = GetBlueprintObj();

	if ( FKismetEditorUtilities::FindBoundEventForComponent(BlueprintObj, EventName, PropertyName) )
	{
		return 0; // View
	}

	return 1; // Add
}

FBlueprintVarActionDetails::~FBlueprintVarActionDetails()
{
	if(MyBlueprint.IsValid())
	{
		// Remove the callback delegate we registered for
		TWeakPtr<FBlueprintEditor> BlueprintEditor = MyBlueprint.Pin()->GetBlueprintEditor();
		if( BlueprintEditor.IsValid() )
		{
			BlueprintEditor.Pin()->OnRefresh().RemoveAll(this);
		}
	}
}

// UProperty Detail Customization
BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void FBlueprintVarActionDetails::CustomizeDetails( IDetailLayoutBuilder& DetailLayout )
{
	CachedVariableProperty = SelectionAsProperty();

	if(!CachedVariableProperty.IsValid())
	{
		return;
	}

	CachedVariableName = GetVariableName();

	TWeakPtr<FBlueprintEditor> BlueprintEditor = MyBlueprint.Pin()->GetBlueprintEditor();
	if( BlueprintEditor.IsValid() )
	{
		BlueprintEditor.Pin()->OnRefresh().AddSP(this, &FBlueprintVarActionDetails::OnPostEditorRefresh);
	}


	UProperty* VariableProperty = CachedVariableProperty.Get();

	// Cache the Blueprint which owns this VariableProperty
	if (UBlueprintGeneratedClass* GeneratedClass = Cast<UBlueprintGeneratedClass>(VariableProperty->GetOwnerClass()))
	{
		PropertyOwnerBlueprint = Cast<UBlueprint>(GeneratedClass->ClassGeneratedBy);
	}

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	
	IDetailCategoryBuilder& Category = DetailLayout.EditCategory("Variable", LOCTEXT("VariableDetailsCategory", "Variable"));
	const FSlateFontInfo DetailFontInfo = IDetailLayoutBuilder::GetDetailFont();
	
	const FString DocLink = TEXT("Shared/Editors/BlueprintEditor/VariableDetails");

	TSharedPtr<SToolTip> VarNameTooltip = IDocumentation::Get()->CreateToolTip(LOCTEXT("VarNameTooltip", "The name of the variable."), NULL, DocLink, TEXT("VariableName"));

	Category.AddCustomRow( LOCTEXT("BlueprintVarActionDetails_VariableNameLabel", "Variable Name") )
	.NameContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("BlueprintVarActionDetails_VariableNameLabel", "Variable Name"))
		.ToolTip(VarNameTooltip)
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MaxDesiredWidth(250.0f)
	[
		SAssignNew(VarNameEditableTextBox, SEditableTextBox)
		.Text(this, &FBlueprintVarActionDetails::OnGetVarName)
		.ToolTip(VarNameTooltip)
		.OnTextChanged(this, &FBlueprintVarActionDetails::OnVarNameChanged)
		.OnTextCommitted(this, &FBlueprintVarActionDetails::OnVarNameCommitted)
		.IsReadOnly(this, &FBlueprintVarActionDetails::GetVariableNameChangeEnabled)
		.Font(IDetailLayoutBuilder::GetDetailFont())
	];

	TSharedPtr<SToolTip> VarTypeTooltip = IDocumentation::Get()->CreateToolTip(LOCTEXT("VarTypeTooltip", "The type of the variable."), NULL, DocLink, TEXT("VariableType"));

	Category.AddCustomRow( LOCTEXT("VariableTypeLabel", "Variable Type") )
	.NameContent()
	[
		SNew(STextBlock)
		.Text( LOCTEXT("VariableTypeLabel", "Variable Type") )
		.ToolTip(VarTypeTooltip)
		.Font(DetailFontInfo)
	]
	.ValueContent()
	.MaxDesiredWidth(980.f)
	[
		SNew(SPinTypeSelector, FGetPinTypeTree::CreateUObject(Schema, &UEdGraphSchema_K2::GetVariableTypeTree))
		.TargetPinType(this, &FBlueprintVarActionDetails::OnGetVarType)
		.OnPinTypeChanged(this, &FBlueprintVarActionDetails::OnVarTypeChanged)
		.IsEnabled(this, &FBlueprintVarActionDetails::GetVariableTypeChangeEnabled)
		.Schema(Schema)
		.TypeTreeFilter(ETypeTreeFilter::None)
		.Font( DetailFontInfo )
		.ToolTip(VarTypeTooltip)
	];

	TSharedPtr<SToolTip> EditableTooltip = IDocumentation::Get()->CreateToolTip(LOCTEXT("VarEditableTooltip", "Whether this variable is publicly editable on instances of this Blueprint."), NULL, DocLink, TEXT("Editable"));

	Category.AddCustomRow( LOCTEXT("IsVariableEditableLabel", "Instance Editable") )
	.Visibility(TAttribute<EVisibility>(this, &FBlueprintVarActionDetails::ShowEditableCheckboxVisibilty))
	.NameContent()
	[
		SNew(STextBlock)
		.Text( LOCTEXT("IsVariableEditableLabel", "Instance Editable") )
		.ToolTip(EditableTooltip)
		.Font( IDetailLayoutBuilder::GetDetailFont() )
	]
	.ValueContent()
	[
		SNew(SCheckBox)
		.IsChecked( this, &FBlueprintVarActionDetails::OnEditableCheckboxState )
		.OnCheckStateChanged( this, &FBlueprintVarActionDetails::OnEditableChanged )
		.IsEnabled(IsVariableInBlueprint())
		.ToolTip(EditableTooltip)
	];
	
	TSharedPtr<SToolTip> ReadOnlyTooltip = IDocumentation::Get()->CreateToolTip(LOCTEXT("VarReadOnlyTooltip", "Whether this variable can be set by Blueprint nodes or if it is read-only."), NULL, DocLink, TEXT("ReadOnly"));

	Category.AddCustomRow(LOCTEXT("IsVariableReadOnlyLabel", "Blueprint Read Only"))
	.Visibility(TAttribute<EVisibility>(this, &FBlueprintVarActionDetails::ShowReadOnlyCheckboxVisibilty))
	.NameContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("IsVariableReadOnlyLabel", "Blueprint Read Only"))
		.ToolTip(ReadOnlyTooltip)
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	[
		SNew(SCheckBox)
		.IsChecked(this, &FBlueprintVarActionDetails::OnReadyOnlyCheckboxState)
		.OnCheckStateChanged(this, &FBlueprintVarActionDetails::OnReadyOnlyChanged)
		.IsEnabled(IsVariableInBlueprint())
		.ToolTip(ReadOnlyTooltip)
	];

	TSharedPtr<SToolTip> ToolTipTooltip = IDocumentation::Get()->CreateToolTip(LOCTEXT("VarToolTipTooltip", "Extra information about this variable, shown when cursor is over it."), NULL, DocLink, TEXT("Tooltip"));

	Category.AddCustomRow( LOCTEXT("IsVariableToolTipLabel", "Tooltip") )
	.Visibility(TAttribute<EVisibility>(this, &FBlueprintVarActionDetails::IsTooltipEditVisible))
	.NameContent()
	[
		SNew(STextBlock)
		.Text( LOCTEXT("IsVariableToolTipLabel", "Tooltip") )
		.ToolTip(ToolTipTooltip)
		.Font( DetailFontInfo )
	]
	.ValueContent()
	[
		SNew(SEditableTextBox)
		.Text( this, &FBlueprintVarActionDetails::OnGetTooltipText )
		.ToolTip(ToolTipTooltip)
		.OnTextCommitted( this, &FBlueprintVarActionDetails::OnTooltipTextCommitted, CachedVariableName )
		.IsEnabled(IsVariableInBlueprint())
		.Font( DetailFontInfo )
	];

	TSharedPtr<SToolTip> Widget3DTooltip = IDocumentation::Get()->CreateToolTip(LOCTEXT("VariableWidget3D_Tooltip", "When true, allows the user to tweak the vector variable by using a 3D transform widget in the viewport (usable when varible is public/enabled)."), NULL, DocLink, TEXT("Widget3D"));

	Category.AddCustomRow( LOCTEXT("VariableWidget3D_Prompt", "Show 3D Widget") )
	.Visibility(TAttribute<EVisibility>(this, &FBlueprintVarActionDetails::Show3DWidgetVisibility))
	.NameContent()
	[
		SNew(STextBlock)
		.ToolTip(Widget3DTooltip)
		.Text(LOCTEXT("VariableWidget3D_Prompt", "Show 3D Widget"))
		.Font( DetailFontInfo )
		.IsEnabled(Is3DWidgetEnabled())
	]
	.ValueContent()
	[
		SNew(SCheckBox)
		.IsChecked( this, &FBlueprintVarActionDetails::OnCreateWidgetCheckboxState )
		.OnCheckStateChanged( this, &FBlueprintVarActionDetails::OnCreateWidgetChanged )
		.IsEnabled(Is3DWidgetEnabled() && IsVariableInBlueprint())
		.ToolTip(Widget3DTooltip)
	];

	TSharedPtr<SToolTip> ExposeOnSpawnTooltip = IDocumentation::Get()->CreateToolTip(LOCTEXT("VariableExposeToSpawn_Tooltip", "Should this variable be exposed as a pin when spawning this Blueprint?"), NULL, DocLink, TEXT("ExposeOnSpawn"));

	Category.AddCustomRow( LOCTEXT("VariableExposeToSpawnLabel", "Expose on Spawn") )
	.Visibility(TAttribute<EVisibility>(this, &FBlueprintVarActionDetails::ExposeOnSpawnVisibility))
	.NameContent()
	[
		SNew(STextBlock)
		.ToolTip(ExposeOnSpawnTooltip)
		.Text( LOCTEXT("VariableExposeToSpawnLabel", "Expose on Spawn") )
		.Font( DetailFontInfo )
	]
	.ValueContent()
	[
		SNew(SCheckBox)
		.IsChecked( this, &FBlueprintVarActionDetails::OnGetExposedToSpawnCheckboxState )
		.OnCheckStateChanged( this, &FBlueprintVarActionDetails::OnExposedToSpawnChanged )
		.IsEnabled(IsVariableInBlueprint())
		.ToolTip(ExposeOnSpawnTooltip)
	];

	TSharedPtr<SToolTip> PrivateTooltip = IDocumentation::Get()->CreateToolTip(LOCTEXT("VariablePrivate_Tooltip", "Should this variable be private (derived blueprints cannot modify it)?"), NULL, DocLink, TEXT("Private"));

	Category.AddCustomRow(LOCTEXT("VariablePrivate", "Private"))
	.Visibility(TAttribute<EVisibility>(this, &FBlueprintVarActionDetails::ExposePrivateVisibility))
	.NameContent()
	[
		SNew(STextBlock)
		.ToolTip(PrivateTooltip)
		.Text( LOCTEXT("VariablePrivate", "Private") )
		.Font( DetailFontInfo )
	]
	.ValueContent()
	[
		SNew(SCheckBox)
		.IsChecked( this, &FBlueprintVarActionDetails::OnGetPrivateCheckboxState )
		.OnCheckStateChanged( this, &FBlueprintVarActionDetails::OnPrivateChanged )
		.IsEnabled(IsVariableInBlueprint())
		.ToolTip(PrivateTooltip)
	];

	TSharedPtr<SToolTip> ExposeToCinematicsTooltip = IDocumentation::Get()->CreateToolTip(LOCTEXT("VariableExposeToCinematics_Tooltip", "Should this variable be exposed for Matinee or Sequencer to modify?"), NULL, DocLink, TEXT("ExposeToCinematics"));

	Category.AddCustomRow( LOCTEXT("VariableExposeToCinematics", "Expose to Cinematics") )
	.Visibility(TAttribute<EVisibility>(this, &FBlueprintVarActionDetails::ExposeToCinematicsVisibility))
	.NameContent()
	[
		SNew(STextBlock)
		.ToolTip(ExposeToCinematicsTooltip)
		.Text( LOCTEXT("VariableExposeToCinematics", "Expose to Cinematics") )
		.Font( DetailFontInfo )
	]
	.ValueContent()
	[
		SNew(SCheckBox)
		.IsChecked( this, &FBlueprintVarActionDetails::OnGetExposedToCinematicsCheckboxState )
		.OnCheckStateChanged( this, &FBlueprintVarActionDetails::OnExposedToCinematicsChanged )
		.IsEnabled(IsVariableInBlueprint())
		.ToolTip(ExposeToCinematicsTooltip)
	];

	FText LocalisedTooltip;
	if (IsConfigCheckBoxEnabled())
	{
		// Build the property specific config variable tool tip
		FFormatNamedArguments ConfigTooltipArgs;
		if (UClass* OwnerClass = VariableProperty->GetOwnerClass())
		{
			OwnerClass = OwnerClass->GetAuthoritativeClass();
			ConfigTooltipArgs.Add(TEXT("ConfigPath"), FText::FromString(OwnerClass->GetConfigName()));
			ConfigTooltipArgs.Add(TEXT("ConfigSection"), FText::FromString(OwnerClass->GetPathName()));
		}
		LocalisedTooltip = FText::Format(LOCTEXT("VariableExposeToConfig_Tooltip", "Should this variable read its default value from a config file if it is present?\r\n\r\nThis is used for customising variable default values and behavior between different projects and configurations.\r\n\r\nConfig file [{ConfigPath}]\r\nConfig section [{ConfigSection}]"), ConfigTooltipArgs);
	}
	else if (IsVariableInBlueprint())
	{
		// mimics the error that UHT would throw
		LocalisedTooltip = LOCTEXT("ObjectVariableConfig_Tooltip", "Not allowed to use 'config' with object variables");
	}
	TSharedPtr<SToolTip> ExposeToConfigTooltip = IDocumentation::Get()->CreateToolTip(LocalisedTooltip, NULL, DocLink, TEXT("ExposeToConfig"));

	Category.AddCustomRow( LOCTEXT("VariableExposeToConfig", "Config Variable"), true )
	.Visibility(TAttribute<EVisibility>(this, &FBlueprintVarActionDetails::ExposeConfigVisibility))
	.NameContent()
	[
		SNew(STextBlock)
		.ToolTip( ExposeToConfigTooltip )
		.Text( LOCTEXT("ExposeToConfigLabel", "Config Variable") )
		.Font( DetailFontInfo )
	]
	.ValueContent()
	[
		SNew(SCheckBox)
		.ToolTip( ExposeToConfigTooltip )
		.IsChecked( this, &FBlueprintVarActionDetails::OnGetConfigVariableCheckboxState )
		.OnCheckStateChanged( this, &FBlueprintVarActionDetails::OnSetConfigVariableState )
		.IsEnabled(this, &FBlueprintVarActionDetails::IsConfigCheckBoxEnabled)
	];

	PopulateCategories(MyBlueprint.Pin().Get(), CategorySource);
	TSharedPtr<SComboButton> NewComboButton;
	TSharedPtr<SListView<TSharedPtr<FText>>> NewListView;

	TSharedPtr<SToolTip> CategoryTooltip = IDocumentation::Get()->CreateToolTip(LOCTEXT("EditCategoryName_Tooltip", "The category of the variable; editing this will place the variable into another category or create a new one."), NULL, DocLink, TEXT("Category"));

	Category.AddCustomRow( LOCTEXT("CategoryLabel", "Category") )
		.Visibility(GetPropertyOwnerBlueprint()? EVisibility::Visible : EVisibility::Hidden)
	.NameContent()
	[
		SNew(STextBlock)
		.Text( LOCTEXT("CategoryLabel", "Category") )
		.ToolTip(CategoryTooltip)
		.Font( DetailFontInfo )
	]
	.ValueContent()
	[
		SAssignNew(NewComboButton, SComboButton)
		.ContentPadding(FMargin(0,0,5,0))
		.IsEnabled(this, &FBlueprintVarActionDetails::GetVariableCategoryChangeEnabled)
		.ToolTip(CategoryTooltip)
		.ButtonContent()
		[
			SNew(SBorder)
			.BorderImage( FEditorStyle::GetBrush("NoBorder") )
			.Padding(FMargin(0, 0, 5, 0))
			[
				SNew(SEditableTextBox)
					.Text(this, &FBlueprintVarActionDetails::OnGetCategoryText)
					.OnTextCommitted(this, &FBlueprintVarActionDetails::OnCategoryTextCommitted, CachedVariableName )
					.ToolTip(CategoryTooltip)
					.SelectAllTextWhenFocused(true)
					.RevertTextOnEscape(true)
					.Font( DetailFontInfo )
			]
		]
		.MenuContent()
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.MaxHeight(400.0f)
			[
				SAssignNew(NewListView, SListView<TSharedPtr<FText>>)
					.ListItemsSource(&CategorySource)
					.OnGenerateRow(this, &FBlueprintVarActionDetails::MakeCategoryViewWidget)
					.OnSelectionChanged(this, &FBlueprintVarActionDetails::OnCategorySelectionChanged)
			]
		]
	];

	CategoryComboButton = NewComboButton;
	CategoryListView = NewListView;

	TSharedPtr<SToolTip> SliderRangeTooltip = IDocumentation::Get()->CreateToolTip(LOCTEXT("SliderRange_Tooltip", "Allows setting the minimum and maximum values for the UI slider for this variable."), NULL, DocLink, TEXT("SliderRange"));

	FName UIMin = TEXT("UIMin");
	FName UIMax = TEXT("UIMax");
	Category.AddCustomRow( LOCTEXT("SliderRangeLabel", "Slider Range") )
	.Visibility(TAttribute<EVisibility>(this, &FBlueprintVarActionDetails::RangeVisibility))
	.NameContent()
	[
		SNew(STextBlock)
		.Text( LOCTEXT("SliderRangeLabel", "Slider Range") )
		.ToolTip(SliderRangeTooltip)
		.Font( DetailFontInfo )
	]
	.ValueContent()
	[
		SNew(SHorizontalBox)
		.ToolTip(SliderRangeTooltip)
		+SHorizontalBox::Slot()
		.FillWidth(1)
		[
			SNew(SEditableTextBox)
			.Text(this, &FBlueprintVarActionDetails::OnGetMetaKeyValue, UIMin)
			.OnTextCommitted(this, &FBlueprintVarActionDetails::OnMetaKeyValueChanged, UIMin)
			.IsEnabled(IsVariableInBlueprint())
			.Font( DetailFontInfo )
		]
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew( STextBlock )
			.Text( LOCTEXT("Min .. Max Separator", " .. ") )
			.Font(DetailFontInfo)
		]
		+SHorizontalBox::Slot()
		.FillWidth(1)
		[
			SNew(SEditableTextBox)
			.Text(this, &FBlueprintVarActionDetails::OnGetMetaKeyValue, UIMax)
			.OnTextCommitted(this, &FBlueprintVarActionDetails::OnMetaKeyValueChanged, UIMax)
			.IsEnabled(IsVariableInBlueprint())
			.Font( DetailFontInfo )
		]
	];

	TSharedPtr<SToolTip> ValueRangeTooltip = IDocumentation::Get()->CreateToolTip(LOCTEXT("ValueRangeLabel_Tooltip", "The range of values allowed by this variable. Values outside of this will be clamped to the range."), NULL, DocLink, TEXT("ValueRange"));

	FName ClampMin = TEXT("ClampMin");
	FName ClampMax = TEXT("ClampMax");
	Category.AddCustomRow(LOCTEXT("ValueRangeLabel", "Value Range"))
	.Visibility(TAttribute<EVisibility>(this, &FBlueprintVarActionDetails::RangeVisibility))
	.NameContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("ValueRangeLabel", "Value Range"))
		.ToolTipText(LOCTEXT("ValueRangeLabel_Tooltip", "The range of values allowed by this variable. Values outside of this will be clamped to the range."))
		.ToolTip(ValueRangeTooltip)
		.Font(DetailFontInfo)
	]
	.ValueContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1)
		[
			SNew(SEditableTextBox)
			.Text(this, &FBlueprintVarActionDetails::OnGetMetaKeyValue, ClampMin)
			.OnTextCommitted(this, &FBlueprintVarActionDetails::OnMetaKeyValueChanged, ClampMin)
			.IsEnabled(IsVariableInBlueprint())
			.Font(DetailFontInfo)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("Min .. Max Separator", " .. "))
			.Font(DetailFontInfo)
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1)
		[
			SNew(SEditableTextBox)
			.Text(this, &FBlueprintVarActionDetails::OnGetMetaKeyValue, ClampMax)
			.OnTextCommitted(this, &FBlueprintVarActionDetails::OnMetaKeyValueChanged, ClampMax)
			.IsEnabled(IsVariableInBlueprint())
			.Font(DetailFontInfo)
		]
	];

	TSharedPtr<SToolTip> BitmaskTooltip = IDocumentation::Get()->CreateToolTip(LOCTEXT("VarBitmaskTooltip", "Whether or not to treat this variable as a bitmask."), nullptr, DocLink, TEXT("Bitmask"));

	Category.AddCustomRow(LOCTEXT("IsVariableBitmaskLabel", "Bitmask"))
	.Visibility(TAttribute<EVisibility>(this, &FBlueprintVarActionDetails::BitmaskVisibility))
	.NameContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("IsVariableBitmaskLabel", "Bitmask"))
		.ToolTip(BitmaskTooltip)
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	[
		SNew(SCheckBox)
		.IsChecked(this, &FBlueprintVarActionDetails::OnBitmaskCheckboxState)
		.OnCheckStateChanged(this, &FBlueprintVarActionDetails::OnBitmaskChanged)
		.IsEnabled(IsVariableInBlueprint())
		.ToolTip(BitmaskTooltip)
	];

	BitmaskEnumTypeNames.Empty();
	BitmaskEnumTypeNames.Add(MakeShareable(new FString(LOCTEXT("BitmaskEnumTypeName_None", "None").ToString())));
	for (TObjectIterator<UEnum> EnumIt; EnumIt; ++EnumIt)
	{
		UEnum* CurrentEnum = *EnumIt;
		if (UEdGraphSchema_K2::IsAllowableBlueprintVariableType(CurrentEnum) && CurrentEnum->HasMetaData(TEXT("Bitflags")))
		{
			BitmaskEnumTypeNames.Add(MakeShareable(new FString(CurrentEnum->GetFName().ToString())));
		}
	}

	TSharedPtr<SToolTip> BitmaskEnumTooltip = IDocumentation::Get()->CreateToolTip(LOCTEXT("VarBitmaskEnumTooltip", "If this is a bitmask, choose an optional enumeration type for the flags. Note that changing this will also reset the default value."), nullptr, DocLink, TEXT("Bitmask Flags"));
	
	Category.AddCustomRow(LOCTEXT("BitmaskEnumLabel", "Bitmask Enum"))
	.Visibility(TAttribute<EVisibility>(this, &FBlueprintVarActionDetails::BitmaskVisibility))
	.NameContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("BitmaskEnumLabel", "Bitmask Enum"))
		.ToolTip(BitmaskEnumTooltip)
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	[
		SNew(STextComboBox)
		.OptionsSource(&BitmaskEnumTypeNames)
		.InitiallySelectedItem(GetBitmaskEnumTypeName())
		.OnSelectionChanged(this, &FBlueprintVarActionDetails::OnBitmaskEnumTypeChanged)
		.IsEnabled(IsVariableInBlueprint() && OnBitmaskCheckboxState() == ECheckBoxState::Checked)
	];

	ReplicationOptions.Empty();
	ReplicationOptions.Add(MakeShareable(new FString("None")));
	ReplicationOptions.Add(MakeShareable(new FString("Replicated")));
	ReplicationOptions.Add(MakeShareable(new FString("RepNotify")));

	TSharedPtr<SToolTip> ReplicationTooltip = IDocumentation::Get()->CreateToolTip( TAttribute<FText>::Create( TAttribute<FText>::FGetter::CreateRaw( this, &FBlueprintVarActionDetails::ReplicationTooltip ) ), NULL, DocLink, TEXT("Replication"));

	Category.AddCustomRow( LOCTEXT("VariableReplicationLabel", "Replication") )
	.Visibility(TAttribute<EVisibility>(this, &FBlueprintVarActionDetails::ReplicationVisibility))
	.NameContent()
	[
		SNew(STextBlock)
		.ToolTip(ReplicationTooltip)
		.Text( LOCTEXT("VariableReplicationLabel", "Replication") )
		.Font( DetailFontInfo )
	]
	.ValueContent()
	[
		SNew(STextComboBox)
		.OptionsSource( &ReplicationOptions )
		.InitiallySelectedItem(GetVariableReplicationType())
		.OnSelectionChanged( this, &FBlueprintVarActionDetails::OnChangeReplication )
		.IsEnabled(this, &FBlueprintVarActionDetails::ReplicationEnabled)
		.ToolTip(ReplicationTooltip)
	];

	ReplicationConditionEnumTypeNames.Empty();
	UEnum* Enum = FindObject<UEnum>(ANY_PACKAGE, TEXT("ELifetimeCondition"), true);
	check(Enum);
	
	for (int32 i = 0; i < Enum->NumEnums(); i++)
	{
		if (!Enum->HasMetaData(TEXT("Hidden"), i))
		{
			ReplicationConditionEnumTypeNames.Add(MakeShareable(new FString(Enum->GetDisplayNameTextByIndex(i).ToString())));
		}
	}

	Category.AddCustomRow(LOCTEXT("VariableReplicationConditionsLabel", "Replication Condition"))
	.Visibility(TAttribute<EVisibility>(this, &FBlueprintVarActionDetails::ReplicationVisibility))
	.NameContent()
	[
		SNew(STextBlock)
		.ToolTip(ReplicationTooltip)
		.Text(LOCTEXT("VariableReplicationConditionsLabel", "Replication Condition"))
		.Font(DetailFontInfo)
	]
	.ValueContent()
	[
		SNew(STextComboBox)
		.OptionsSource(&ReplicationConditionEnumTypeNames)
		.InitiallySelectedItem(GetVariableReplicationCondition())
		.OnSelectionChanged(this, &FBlueprintVarActionDetails::OnChangeReplicationCondition)
		.IsEnabled(this, &FBlueprintVarActionDetails::ReplicationConditionEnabled)
	];


	UBlueprint* BlueprintObj = GetBlueprintObj();

	// Handle event generation
	if ( FBlueprintEditorUtils::DoesSupportEventGraphs(BlueprintObj) )
	{
		AddEventsCategory(DetailLayout, VariableProperty);
	}

	// Add in default value editing for properties that can be edited, local properties cannot be edited
	if ((BlueprintObj != nullptr) && (BlueprintObj->GeneratedClass != nullptr))
	{
		bool bVariableRenamed = false;
		if (VariableProperty != nullptr && IsVariableInBlueprint())
		{
			// Determine the current property name on the CDO is stale
			if (PropertyOwnerBlueprint.IsValid() && VariableProperty)
			{
				UBlueprint* PropertyBlueprint = PropertyOwnerBlueprint.Get();
				const int32 VarIndex = FBlueprintEditorUtils::FindNewVariableIndex(PropertyBlueprint, CachedVariableName);
				if (VarIndex != INDEX_NONE)
				{
					const FGuid VarGuid = PropertyBlueprint->NewVariables[VarIndex].VarGuid;
					if (UBlueprintGeneratedClass* AuthoritiveBPGC = Cast<UBlueprintGeneratedClass>(PropertyBlueprint->GeneratedClass))
					{
						if (const FName* OldName = AuthoritiveBPGC->PropertyGuids.FindKey(VarGuid))
						{
							bVariableRenamed = CachedVariableName != *OldName;
						}
					}
				}
			}
			const UProperty* OriginalProperty = nullptr;
		
			if(!IsALocalVariable(VariableProperty))
			{
				OriginalProperty = FindField<UProperty>(BlueprintObj->GeneratedClass, VariableProperty->GetFName());
			}
			else
			{
				OriginalProperty = VariableProperty;
			}

			if (OriginalProperty == nullptr || bVariableRenamed)
			{
				// Prevent editing the default value of a skeleton property
				VariableProperty = nullptr;
			}
			else if (const UStructProperty* StructProperty = Cast<const UStructProperty>(OriginalProperty))
			{
				// Prevent editing the default value of a stale struct
				const UUserDefinedStruct* BGStruct = Cast<const UUserDefinedStruct>(StructProperty->Struct);
				if (BGStruct && (EUserDefinedStructureStatus::UDSS_UpToDate != BGStruct->Status))
				{
					VariableProperty = nullptr;
				}
			}
		}

		// Find the class containing the variable
		UClass* VariableClass = (VariableProperty ? VariableProperty->GetTypedOuter<UClass>() : nullptr);

		FText ErrorMessage;
		IDetailCategoryBuilder& DefaultValueCategory = DetailLayout.EditCategory(TEXT("DefaultValueCategory"), LOCTEXT("DefaultValueCategoryHeading", "Default Value"));

		if (VariableProperty == nullptr)
		{
			if (BlueprintObj->Status != BS_UpToDate)
			{
				ErrorMessage = LOCTEXT("VariableMissing_DirtyBlueprint", "Please compile the blueprint");
			}
			else
			{
				ErrorMessage = LOCTEXT("VariableMissing_CleanBlueprint", "Failed to find variable property");
			}
		}

		// Show the error message if something went wrong
		if (!ErrorMessage.IsEmpty())
		{
			DefaultValueCategory.AddCustomRow( ErrorMessage )
			[
				SNew(STextBlock)
				.ToolTipText(ErrorMessage)
				.Text(ErrorMessage)
				.Font(DetailFontInfo)
			];
		}
		else 
		{
			if(IsALocalVariable(VariableProperty))
			{
				UFunction* StructScope = Cast<UFunction>(VariableProperty->GetOuter());
				check(StructScope);

				TSharedPtr<FStructOnScope> StructData = MakeShareable(new FStructOnScope((UFunction*)StructScope));
				UEdGraph* Graph = FBlueprintEditorUtils::FindScopeGraph(GetBlueprintObj(), (UFunction*)StructScope);

				// Find the function entry nodes in the current graph
				TArray<UK2Node_FunctionEntry*> EntryNodes;
				Graph->GetNodesOfClass(EntryNodes);

				// There should always be an entry node in the function graph
				check(EntryNodes.Num() > 0);

				const UStructProperty* PotentialUDSProperty = Cast<const UStructProperty>(VariableProperty);
				//UDS requires default data even when the LocalVariable value is empty
				const bool bUDSProperty = PotentialUDSProperty && Cast<const UUserDefinedStruct>(PotentialUDSProperty->Struct);

				UK2Node_FunctionEntry* FuncEntry = EntryNodes[0];
				for (const FBPVariableDescription& LocalVar : FuncEntry->LocalVariables)
				{
					if(LocalVar.VarName == VariableProperty->GetFName()) //Property->GetFName())
					{
						// Only set the default value if there is one
						if(bUDSProperty || !LocalVar.DefaultValue.IsEmpty())
						{
							FBlueprintEditorUtils::PropertyValueFromString(VariableProperty, LocalVar.DefaultValue, StructData->GetStructMemory());
						}
						break;
					}
				}

				if(BlueprintEditor.IsValid())
				{
					TSharedPtr< IDetailsView > DetailsView  = BlueprintEditor.Pin()->GetInspector()->GetPropertyView();

					if(DetailsView.IsValid())
					{
						TWeakObjectPtr<UK2Node_EditablePinBase> EntryNode = FuncEntry;
						DetailsView->OnFinishedChangingProperties().AddSP(this, &FBlueprintVarActionDetails::OnFinishedChangingProperties, StructData, EntryNode);
					}
				}

				IDetailPropertyRow* Row = DefaultValueCategory.AddExternalStructureProperty(StructData, VariableProperty->GetFName());
			}
			else
			{
				UBlueprint* CurrPropertyOwnerBlueprint = IsVariableInheritedByBlueprint() ? GetBlueprintObj() : GetPropertyOwnerBlueprint();
				UObject* TargetBlueprintDefaultObject = nullptr;
				if (CurrPropertyOwnerBlueprint && CurrPropertyOwnerBlueprint->GeneratedClass)
				{
					TargetBlueprintDefaultObject = CurrPropertyOwnerBlueprint->GeneratedClass->GetDefaultObject();
				}
				else if (UBlueprint* PropertyOwnerBP = GetPropertyOwnerBlueprint())
				{
					TargetBlueprintDefaultObject = PropertyOwnerBP->GeneratedClass->GetDefaultObject();
				}
				else if (CachedVariableProperty.IsValid())
				{
					// Capture the non-BP class CDO so we can show the default value
					TargetBlueprintDefaultObject = CachedVariableProperty->GetOwnerClass()->GetDefaultObject();
				}

				if (TargetBlueprintDefaultObject != nullptr)
				{
					// Things are in order, show the property and allow it to be edited
					TArray<UObject*> ObjectList;
					ObjectList.Add(TargetBlueprintDefaultObject);
					IDetailPropertyRow* Row = DefaultValueCategory.AddExternalObjectProperty(ObjectList, VariableProperty->GetFName());
					if (Row != nullptr)
					{
						Row->IsEnabled(IsVariableInheritedByBlueprint());
					}
				}
			}
		}

		TSharedPtr<SToolTip> TransientTooltip = IDocumentation::Get()->CreateToolTip(LOCTEXT("VariableTransient_Tooltip", "Should this variable not serialize and be zero-filled at load?"), NULL, DocLink, TEXT("Transient"));

		Category.AddCustomRow(LOCTEXT("VariableTransient", "Transient"), true)
			.Visibility(TAttribute<EVisibility>(this, &FBlueprintVarActionDetails::GetTransientVisibility))
			.NameContent()
		[
			SNew(STextBlock)
			.ToolTip(TransientTooltip)
			.Text( LOCTEXT("VariableTransient", "Transient") )
			.Font( DetailFontInfo )
		]
		.ValueContent()
		[
			SNew(SCheckBox)
			.IsChecked( this, &FBlueprintVarActionDetails::OnGetTransientCheckboxState )
			.OnCheckStateChanged( this, &FBlueprintVarActionDetails::OnTransientChanged )
			.IsEnabled(IsVariableInBlueprint())
			.ToolTip(TransientTooltip)
		];

		TSharedPtr<SToolTip> SaveGameTooltip = IDocumentation::Get()->CreateToolTip(LOCTEXT("VariableSaveGame_Tooltip", "Should this variable be serialized for saved games?"), NULL, DocLink, TEXT("SaveGame"));

		Category.AddCustomRow(LOCTEXT("VariableSaveGame", "SaveGame"), true)
		.Visibility(TAttribute<EVisibility>(this, &FBlueprintVarActionDetails::GetSaveGameVisibility))
		.NameContent()
		[
			SNew(STextBlock)
			.ToolTip(SaveGameTooltip)
			.Text( LOCTEXT("VariableSaveGame", "SaveGame") )
			.Font( DetailFontInfo )
		]
		.ValueContent()
		[
			SNew(SCheckBox)
			.IsChecked( this, &FBlueprintVarActionDetails::OnGetSaveGameCheckboxState )
			.OnCheckStateChanged( this, &FBlueprintVarActionDetails::OnSaveGameChanged )
			.IsEnabled(IsVariableInBlueprint())
			.ToolTip(SaveGameTooltip)
		];

		TSharedPtr<SToolTip> AdvancedDisplayTooltip = IDocumentation::Get()->CreateToolTip(LOCTEXT("VariableAdvancedDisplay_Tooltip", "Hide this variable in Class Defaults windows by default"), NULL, DocLink, TEXT("AdvancedDisplay"));

		Category.AddCustomRow(LOCTEXT("VariableAdvancedDisplay", "Advanced Display"), true)
			.Visibility(TAttribute<EVisibility>(this, &FBlueprintVarActionDetails::GetAdvancedDisplayVisibility))
			.NameContent()
			[
				SNew(STextBlock)
				.ToolTip(AdvancedDisplayTooltip)
				.Text(LOCTEXT("VariableAdvancedDisplay", "Advanced Display"))
				.Font(DetailFontInfo)
			]
		.ValueContent()
			[
				SNew(SCheckBox)
				.IsChecked(this, &FBlueprintVarActionDetails::OnGetAdvancedDisplayCheckboxState)
				.OnCheckStateChanged(this, &FBlueprintVarActionDetails::OnAdvancedDisplayChanged)
				.IsEnabled(IsVariableInBlueprint())
				.ToolTip(AdvancedDisplayTooltip)
			];

		TSharedPtr<SToolTip> MultilineTooltip = IDocumentation::Get()->CreateToolTip(LOCTEXT("VariableMultilineTooltip_Tooltip", "Allow the value of this variable to have newlines (use Ctrl+Enter to add one while editing)"), NULL, DocLink, TEXT("Multiline"));

		Category.AddCustomRow(LOCTEXT("VariableMultilineTooltip", "Multi line"), true)
			.Visibility(TAttribute<EVisibility>(this, &FBlueprintVarActionDetails::GetMultilineVisibility))
			.NameContent()
			[
				SNew(STextBlock)
				.ToolTip(MultilineTooltip)
				.Text(LOCTEXT("VariableMultiline", "Multi line"))
				.Font(DetailFontInfo)
			]
		.ValueContent()
			[
				SNew(SCheckBox)
				.IsChecked(this, &FBlueprintVarActionDetails::OnGetMultilineCheckboxState)
				.OnCheckStateChanged(this, &FBlueprintVarActionDetails::OnMultilineChanged)
				.IsEnabled(IsVariableInBlueprint())
				.ToolTip(MultilineTooltip)
			];

		TSharedPtr<SToolTip> PropertyFlagsTooltip = IDocumentation::Get()->CreateToolTip(LOCTEXT("DefinedPropertyFlags_Tooltip", "List of defined flags for this property"), NULL, DocLink, TEXT("PropertyFlags"));

		Category.AddCustomRow(LOCTEXT("DefinedPropertyFlags", "Defined Property Flags"), true)
		.WholeRowWidget
		[
			SNew(STextBlock)
			.ToolTip(PropertyFlagsTooltip)
			.Text( LOCTEXT("DefinedPropertyFlags", "Defined Property Flags") )
			.Font( IDetailLayoutBuilder::GetDetailFontBold() )
		];

		Category.AddCustomRow(FText::GetEmpty(), true)
		.WholeRowWidget
		[
			SAssignNew(PropertyFlagWidget, SListView< TSharedPtr< FString > >)
				.OnGenerateRow(this, &FBlueprintVarActionDetails::OnGenerateWidgetForPropertyList)
				.ListItemsSource(&PropertyFlags)
				.SelectionMode(ESelectionMode::None)
				.ScrollbarVisibility(EVisibility::Collapsed)
				.ToolTip(PropertyFlagsTooltip)
		];

		RefreshPropertyFlags();
	}

	// See if anything else wants to customize our details
	FBlueprintEditorModule& BlueprintEditorModule = FModuleManager::GetModuleChecked<FBlueprintEditorModule>("Kismet");
	TArray<TSharedPtr<IDetailCustomization>> Customizations = BlueprintEditorModule.CustomizeVariable(CachedVariableProperty->GetClass(), BlueprintEditor.Pin());
	ExternalDetailCustomizations.Append(Customizations);
	for (TSharedPtr<IDetailCustomization> ExternalDetailCustomization : ExternalDetailCustomizations)
	{
		ExternalDetailCustomization->CustomizeDetails(DetailLayout);
	}
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void FBlueprintVarActionDetails::RefreshPropertyFlags()
{
	UProperty* VariableProperty = CachedVariableProperty.Get();
	if(VariableProperty)
	{
		PropertyFlags.Empty();
		for( const TCHAR* PropertyFlag : ParsePropertyFlags(VariableProperty->PropertyFlags) )
		{
			PropertyFlags.Add(MakeShareable<FString>(new FString(PropertyFlag)));
		}

		PropertyFlagWidget.Pin()->RequestListRefresh();
	}
}

TSharedRef<ITableRow> FBlueprintVarActionDetails::OnGenerateWidgetForPropertyList( TSharedPtr< FString > Item, const TSharedRef<STableViewBase>& OwnerTable )
{
	return SNew(STableRow< TSharedPtr< FString > >, OwnerTable)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			[
				SNew(STextBlock)
					.Text(FText::FromString(*Item.Get()))
					.ToolTipText(FText::FromString(*Item.Get()))
					.Font( IDetailLayoutBuilder::GetDetailFont() )
			]

			+SHorizontalBox::Slot()
				.AutoWidth()
			[
				SNew(SCheckBox)
					.IsChecked(true ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
					.IsEnabled(false)
			]
		];
}

bool FBlueprintVarActionDetails::IsASCSVariable(UProperty* VariableProperty) const
{
	UObjectProperty* VariableObjProp = VariableProperty ? Cast<UObjectProperty>(VariableProperty) : NULL;

	if (VariableObjProp != NULL && VariableObjProp->PropertyClass != NULL && VariableObjProp->PropertyClass->IsChildOf(UActorComponent::StaticClass()))
	{
		return !FBlueprintEditorUtils::IsVariableCreatedByBlueprint(GetBlueprintObj(), VariableObjProp);
	}
	return false;
}

bool FBlueprintVarActionDetails::IsABlueprintVariable(UProperty* VariableProperty) const
{
	UClass* VarSourceClass = VariableProperty ? Cast<UClass>(VariableProperty->GetOuter()) : NULL;
	if(VarSourceClass)
	{
		return (VarSourceClass->ClassGeneratedBy != NULL);
	}
	return false;
}

bool FBlueprintVarActionDetails::IsALocalVariable(UProperty* VariableProperty) const
{
	return VariableProperty && (Cast<UFunction>(VariableProperty->GetOuter()) != NULL);
}

UStruct* FBlueprintVarActionDetails::GetLocalVariableScope(UProperty* VariableProperty) const
{
	if(IsALocalVariable(VariableProperty))
	{
		return Cast<UFunction>(VariableProperty->GetOuter());
	}

	return NULL;
}

bool FBlueprintVarActionDetails::GetVariableNameChangeEnabled() const
{
	bool bIsReadOnly = true;

	UBlueprint* BlueprintObj = GetBlueprintObj();
	check(BlueprintObj != nullptr);

	UProperty* VariableProperty = CachedVariableProperty.Get();
	if(VariableProperty != nullptr && IsVariableInBlueprint())
	{
		if(FBlueprintEditorUtils::FindNewVariableIndex(BlueprintObj, CachedVariableName) != INDEX_NONE)
		{
			bIsReadOnly = false;
		}
		else if(BlueprintObj->FindTimelineTemplateByVariableName(CachedVariableName))
		{
			bIsReadOnly = false;
		}
		else if(IsASCSVariable(VariableProperty) && BlueprintObj->SimpleConstructionScript != nullptr)
		{
			if (USCS_Node* Node = BlueprintObj->SimpleConstructionScript->FindSCSNode(CachedVariableName))
			{
				bIsReadOnly = !FComponentEditorUtils::IsValidVariableNameString(Node->ComponentTemplate, Node->GetVariableName().ToString());
			}
		}
		else if(IsALocalVariable(VariableProperty))
		{
			bIsReadOnly = false;
		}
	}

	return bIsReadOnly;
}

FText FBlueprintVarActionDetails::OnGetVarName() const
{
	return FText::FromName(CachedVariableName);
}

void FBlueprintVarActionDetails::OnVarNameChanged(const FText& InNewText)
{
	bIsVarNameInvalid = true;

	UBlueprint* BlueprintObj = GetBlueprintObj();
	check(BlueprintObj != nullptr);

	UProperty* VariableProperty = CachedVariableProperty.Get();
	if(VariableProperty && IsASCSVariable(VariableProperty) && BlueprintObj->SimpleConstructionScript != nullptr)
	{
		for (USCS_Node* Node : BlueprintObj->SimpleConstructionScript->GetAllNodes())
		{
			if (Node && Node->GetVariableName() == CachedVariableName && !FComponentEditorUtils::IsValidVariableNameString(Node->ComponentTemplate, InNewText.ToString()))
			{
				VarNameEditableTextBox->SetError(LOCTEXT("ComponentVariableRenameFailed_NotValid", "This name is reserved for engine use."));
				return;
			}
		}
	}

	TSharedPtr<INameValidatorInterface> NameValidator = MakeShareable(new FKismetNameValidator(BlueprintObj, CachedVariableName, GetLocalVariableScope(VariableProperty)));

	EValidatorResult ValidatorResult = NameValidator->IsValid(InNewText.ToString());
	if(ValidatorResult == EValidatorResult::AlreadyInUse)
	{
		VarNameEditableTextBox->SetError(FText::Format(LOCTEXT("RenameFailed_InUse", "{0} is in use by another variable or function!"), InNewText));
	}
	else if(ValidatorResult == EValidatorResult::EmptyName)
	{
		VarNameEditableTextBox->SetError(LOCTEXT("RenameFailed_LeftBlank", "Names cannot be left blank!"));
	}
	else if(ValidatorResult == EValidatorResult::TooLong)
	{
		VarNameEditableTextBox->SetError(FText::Format( LOCTEXT("RenameFailed_NameTooLong", "Names must have fewer than {0} characters!"), FText::AsNumber( FKismetNameValidator::GetMaximumNameLength())));
	}
	else if(ValidatorResult == EValidatorResult::LocallyInUse)
	{
		VarNameEditableTextBox->SetError(LOCTEXT("ConflictsWithProperty", "Conflicts with another another local variable or function parameter!"));
	}
	else
	{
		bIsVarNameInvalid = false;
		VarNameEditableTextBox->SetError(FText::GetEmpty());
	}
}

void FBlueprintVarActionDetails::OnVarNameCommitted(const FText& InNewText, ETextCommit::Type InTextCommit)
{
	if(InTextCommit != ETextCommit::OnCleared && !bIsVarNameInvalid)
	{
		const FScopedTransaction Transaction( LOCTEXT( "RenameVariable", "Rename Variable" ) );

		FName NewVarName = FName(*InNewText.ToString());

		// Double check we're not renaming a timeline disguised as a variable
		bool bIsTimeline = false;

		UProperty* VariableProperty = CachedVariableProperty.Get();
		if (VariableProperty != NULL)
		{
			// Don't allow removal of timeline properties - you need to remove the timeline node for that
			UObjectProperty* ObjProperty = Cast<UObjectProperty>(VariableProperty);
			if(ObjProperty != NULL && ObjProperty->PropertyClass == UTimelineComponent::StaticClass())
			{
				bIsTimeline = true;
			}

			// Rename as a timeline if required
			if (bIsTimeline)
			{
				FBlueprintEditorUtils::RenameTimeline(GetBlueprintObj(), CachedVariableName, NewVarName);
			}
			else if(IsALocalVariable(VariableProperty))
			{
				UFunction* LocalVarScope = Cast<UFunction>(VariableProperty->GetOuter());
				FBlueprintEditorUtils::RenameLocalVariable(GetBlueprintObj(), LocalVarScope, CachedVariableName, NewVarName);
			}
			else
			{
				FBlueprintEditorUtils::RenameMemberVariable(GetBlueprintObj(), CachedVariableName, NewVarName);
			}

			check(MyBlueprint.IsValid());
			MyBlueprint.Pin()->SelectItemByName(NewVarName, ESelectInfo::OnMouseClick);
		}
	}

	bIsVarNameInvalid = false;
	VarNameEditableTextBox->SetError(FText::GetEmpty());
}

bool FBlueprintVarActionDetails::GetVariableTypeChangeEnabled() const
{
	UProperty* VariableProperty = CachedVariableProperty.Get();
	if(VariableProperty && IsVariableInBlueprint())
	{
		if (!IsALocalVariable(VariableProperty))
		{
			if(GetBlueprintObj()->SkeletonGeneratedClass->GetAuthoritativeClass() != VariableProperty->GetOwnerClass()->GetAuthoritativeClass())
			{
				return false;
			}
			// If the variable belongs to this class and cannot be found in the member variable list, it is not editable (it may be a component)
			if (FBlueprintEditorUtils::FindNewVariableIndex(GetBlueprintObj(), CachedVariableName) == INDEX_NONE)
			{
				return false;
			}
		}
		return true;
	}
	return false;
}

bool FBlueprintVarActionDetails::GetVariableCategoryChangeEnabled() const
{
	UProperty* VariableProperty = CachedVariableProperty.Get();
	if(VariableProperty && IsVariableInBlueprint())
	{
		if(UClass* VarSourceClass = Cast<UClass>(VariableProperty->GetOuter()))
		{
			// If the variable's source class is the same as the current blueprint's class then it was created in this blueprint and it's category can be changed.
			return VarSourceClass == GetBlueprintObj()->SkeletonGeneratedClass;
		}
		else if(IsALocalVariable(VariableProperty))
		{
			return true;
		}
	}

	return false;
}

FEdGraphPinType FBlueprintVarActionDetails::OnGetVarType() const
{
	UProperty* VariableProperty = CachedVariableProperty.Get();
	if (VariableProperty)
	{
		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
		FEdGraphPinType Type;
		K2Schema->ConvertPropertyToPinType(VariableProperty, Type);
		return Type;
	}
	return FEdGraphPinType();
}

void FBlueprintVarActionDetails::OnVarTypeChanged(const FEdGraphPinType& NewPinType)
{
	if (FBlueprintEditorUtils::IsPinTypeValid(NewPinType))
	{
		FName VarName = CachedVariableName;

		if (VarName != NAME_None)
		{
			// Set the MyBP tab's last pin type used as this, for adding lots of variables of the same type
			MyBlueprint.Pin()->GetLastPinTypeUsed() = NewPinType;

			UProperty* VariableProperty = CachedVariableProperty.Get();
			if(VariableProperty)
			{
				if(IsALocalVariable(VariableProperty))
				{
					FBlueprintEditorUtils::ChangeLocalVariableType(GetBlueprintObj(), GetLocalVariableScope(VariableProperty), VarName, NewPinType);
				}
				else
				{
					FBlueprintEditorUtils::ChangeMemberVariableType(GetBlueprintObj(), VarName, NewPinType);
				}
			}
		}
	}
}

FText FBlueprintVarActionDetails::OnGetTooltipText() const
{
	FName VarName = CachedVariableName;
	if (VarName != NAME_None)
	{
		if ( UBlueprint* OwnerBlueprint = GetPropertyOwnerBlueprint() )
		{
			FString Result;
			FBlueprintEditorUtils::GetBlueprintVariableMetaData(GetPropertyOwnerBlueprint(), VarName, GetLocalVariableScope(CachedVariableProperty.Get()), TEXT("tooltip"), Result);
			return FText::FromString(Result);
		}
	}
	return FText();
}

void FBlueprintVarActionDetails::OnTooltipTextCommitted(const FText& NewText, ETextCommit::Type InTextCommit, FName VarName)
{
	FBlueprintEditorUtils::SetBlueprintVariableMetaData(GetBlueprintObj(), VarName, GetLocalVariableScope(CachedVariableProperty.Get()), TEXT("tooltip"), NewText.ToString() );
}

void FBlueprintVarActionDetails::PopulateCategories(SMyBlueprint* MyBlueprint, TArray<TSharedPtr<FText>>& CategorySource)
{
	// Used to compare found categories to prevent double adds
	TArray<FString> CategoryNameList;

	TArray<FName> VisibleVariables;
	bool bShowUserVarsOnly = MyBlueprint->ShowUserVarsOnly();
	UBlueprint* Blueprint = MyBlueprint->GetBlueprintObj();
	check(Blueprint != NULL);
	if (Blueprint->SkeletonGeneratedClass == NULL)
	{
		UE_LOG(LogBlueprint, Error, TEXT("Blueprint %s has NULL SkeletonGeneratedClass in FBlueprintVarActionDetails::PopulateCategories().  Cannot Populate Categories."), *GetNameSafe(Blueprint));
		return;
	}

	check(Blueprint->SkeletonGeneratedClass != NULL);
	EFieldIteratorFlags::SuperClassFlags SuperClassFlag = EFieldIteratorFlags::ExcludeSuper;
	if(!bShowUserVarsOnly)
	{
		SuperClassFlag = EFieldIteratorFlags::IncludeSuper;
	}

	for (TFieldIterator<UProperty> PropertyIt(Blueprint->SkeletonGeneratedClass, SuperClassFlag); PropertyIt; ++PropertyIt)
	{
		UProperty* Property = *PropertyIt;

		if ((!Property->HasAnyPropertyFlags(CPF_Parm) && Property->HasAllPropertyFlags(CPF_BlueprintVisible)))
		{
			VisibleVariables.Add(Property->GetFName());
		}
	}

	CategorySource.Reset();
	CategorySource.Add(MakeShareable(new FText(LOCTEXT("Default", "Default"))));
	for (int32 i = 0; i < VisibleVariables.Num(); ++i)
	{
		FText Category = FBlueprintEditorUtils::GetBlueprintVariableCategory(Blueprint, VisibleVariables[i], nullptr);
		if (!Category.IsEmpty() && !Category.EqualTo(FText::FromString(Blueprint->GetName())))
		{
			bool bNewCategory = true;
			for (int32 j = 0; j < CategorySource.Num() && bNewCategory; ++j)
			{
				bNewCategory &= !CategorySource[j].Get()->EqualTo(Category);
			}
			if (bNewCategory)
			{
				CategorySource.Add(MakeShareable(new FText(Category)));
			}
		}
	}

	// Search through all function graphs for entry nodes to search for local variables to pull their categories
	for (UEdGraph* FunctionGraph : Blueprint->FunctionGraphs)
	{
		if(UFunction* Function = Blueprint->SkeletonGeneratedClass->FindFunctionByName(FunctionGraph->GetFName()))
		{
			FText FunctionCategory = Function->GetMetaDataText(FBlueprintMetadata::MD_FunctionCategory, TEXT("UObjectCategory"), Function->GetFullGroupName(false));

			if(!FunctionCategory.IsEmpty())
			{
				bool bNewCategory = true;
				for (int32 j = 0; j < CategorySource.Num() && bNewCategory; ++j)
				{
					bNewCategory &= !CategorySource[j].Get()->EqualTo(FunctionCategory);
				}

				if(bNewCategory)
				{
					CategorySource.Add(MakeShareable(new FText(FunctionCategory)));
				}
			}
		}

		UK2Node_EditablePinBase* EntryNode = FBlueprintEditorUtils::GetEntryNode(FunctionGraph);
		if (UK2Node_FunctionEntry* FunctionEntryNode = Cast<UK2Node_FunctionEntry>(EntryNode))
		{
			for (FBPVariableDescription& Variable : FunctionEntryNode->LocalVariables)
			{
				bool bNewCategory = true;
				for (int32 j = 0; j < CategorySource.Num() && bNewCategory; ++j)
				{
					bNewCategory &= !CategorySource[j].Get()->EqualTo(Variable.Category);
				}
				if (bNewCategory)
				{
					CategorySource.Add(MakeShareable(new FText(Variable.Category)));
				}
			}
		}
	}

	for (UEdGraph* MacroGraph : Blueprint->MacroGraphs)
	{
		UK2Node_EditablePinBase* EntryNode = FBlueprintEditorUtils::GetEntryNode(MacroGraph);
		if (UK2Node_Tunnel* TypedEntryNode = ExactCast<UK2Node_Tunnel>(EntryNode))
		{
			bool bNewCategory = true;
			for (int32 j = 0; j < CategorySource.Num() && bNewCategory; ++j)
			{
				bNewCategory &= !CategorySource[j].Get()->EqualTo(TypedEntryNode->MetaData.Category);
			}
			if (bNewCategory)
			{
				CategorySource.Add(MakeShareable(new FText(TypedEntryNode->MetaData.Category)));
			}
		}
	}

	// Pull categories from overridable functions
	for (TFieldIterator<UFunction> FunctionIt(Blueprint->ParentClass, EFieldIteratorFlags::IncludeSuper); FunctionIt; ++FunctionIt)
	{
		const UFunction* Function = *FunctionIt;
		const FName FunctionName = Function->GetFName();

		if (UEdGraphSchema_K2::CanKismetOverrideFunction(Function) && !UEdGraphSchema_K2::FunctionCanBePlacedAsEvent(Function))
		{
			FText FunctionCategory = Function->GetMetaDataText(FBlueprintMetadata::MD_FunctionCategory, TEXT("UObjectCategory"), Function->GetFullGroupName(false));

			if (!FunctionCategory.IsEmpty())
			{
				bool bNewCategory = true;
				for (int32 j = 0; j < CategorySource.Num() && bNewCategory; ++j)
				{
					bNewCategory &= !CategorySource[j].Get()->EqualTo(FunctionCategory);
				}

				if (bNewCategory)
				{
					CategorySource.Add(MakeShareable(new FText(FunctionCategory)));
				}
			}
		}
	}
}

UK2Node_Variable* FBlueprintVarActionDetails::EdGraphSelectionAsVar() const
{
	TWeakPtr<FBlueprintEditor> BlueprintEditor = MyBlueprint.Pin()->GetBlueprintEditor();

	if( BlueprintEditor.IsValid() )
	{
		/** Get the currently selected set of nodes */
		TSet<UObject*> Objects = BlueprintEditor.Pin()->GetSelectedNodes();

		if (Objects.Num() == 1)
		{
			TSet<UObject*>::TIterator Iter(Objects);
			UObject* Object = *Iter;

			if (Object && Object->IsA<UK2Node_Variable>())
			{
				return Cast<UK2Node_Variable>(Object);
			}
		}
	}
	return NULL;
}

UProperty* FBlueprintVarActionDetails::SelectionAsProperty() const
{
	FEdGraphSchemaAction_K2Var* VarAction = MyBlueprintSelectionAsVar();
	if(VarAction)
	{
		return VarAction->GetProperty();
	}
	FEdGraphSchemaAction_K2LocalVar* LocalVarAction = MyBlueprintSelectionAsLocalVar();
	if(LocalVarAction)
	{
		return LocalVarAction->GetProperty();
	}
	UK2Node_Variable* GraphVar = EdGraphSelectionAsVar();
	if(GraphVar)
	{
		return GraphVar->GetPropertyForVariable();
	}
	return NULL;
}

FName FBlueprintVarActionDetails::GetVariableName() const
{
	FEdGraphSchemaAction_K2Var* VarAction = MyBlueprintSelectionAsVar();
	if(VarAction)
	{
		return VarAction->GetVariableName();
	}
	FEdGraphSchemaAction_K2LocalVar* LocalVarAction = MyBlueprintSelectionAsLocalVar();
	if(LocalVarAction)
	{
		return LocalVarAction->GetVariableName();
	}
	UK2Node_Variable* GraphVar = EdGraphSelectionAsVar();
	if(GraphVar)
	{
		return GraphVar->GetVarName();
	}
	return NAME_None;
}

FText FBlueprintVarActionDetails::OnGetCategoryText() const
{
	FName VarName = CachedVariableName;
	if (VarName != NAME_None)
	{
		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

		if ( UBlueprint* OwnerBlueprint = GetPropertyOwnerBlueprint() )
		{
			FText Category = FBlueprintEditorUtils::GetBlueprintVariableCategory(OwnerBlueprint, VarName, GetLocalVariableScope(CachedVariableProperty.Get()));

			// Older blueprints will have their name as the default category and whenever it is the same as the default category, display localized text
			if ( Category.EqualTo(FText::FromString(OwnerBlueprint->GetName())) || Category.EqualTo(K2Schema->VR_DefaultCategory) )
			{
				return K2Schema->VR_DefaultCategory;
			}
			else
			{
				return Category;
			}
		}

		return FText::FromName(VarName);
	}
	return FText();
}

void FBlueprintVarActionDetails::OnCategoryTextCommitted(const FText& NewText, ETextCommit::Type InTextCommit, FName VarName)
{
	if (InTextCommit == ETextCommit::OnEnter || InTextCommit == ETextCommit::OnUserMovedFocus)
	{
		// Remove excess whitespace and prevent categories with just spaces
		FText CategoryName = FText::TrimPrecedingAndTrailing(NewText);

		FBlueprintEditorUtils::SetBlueprintVariableCategory(GetBlueprintObj(), VarName, GetLocalVariableScope(CachedVariableProperty.Get()), CategoryName);
		check(MyBlueprint.IsValid());
		PopulateCategories(MyBlueprint.Pin().Get(), CategorySource);
		MyBlueprint.Pin()->ExpandCategory(CategoryName);
	}
}

TSharedRef< ITableRow > FBlueprintVarActionDetails::MakeCategoryViewWidget( TSharedPtr<FText> Item, const TSharedRef< STableViewBase >& OwnerTable )
{
	return SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
		[
			SNew(STextBlock) .Text(*Item.Get())
		];
}

void FBlueprintVarActionDetails::OnCategorySelectionChanged( TSharedPtr<FText> ProposedSelection, ESelectInfo::Type /*SelectInfo*/ )
{
	FName VarName = CachedVariableName;
	if (ProposedSelection.IsValid() && VarName != NAME_None)
	{
		FText NewCategory = *ProposedSelection.Get();

		FBlueprintEditorUtils::SetBlueprintVariableCategory(GetBlueprintObj(), VarName, GetLocalVariableScope(CachedVariableProperty.Get()), NewCategory );
		CategoryListView.Pin()->ClearSelection();
		CategoryComboButton.Pin()->SetIsOpen(false);
		MyBlueprint.Pin()->ExpandCategory(NewCategory);
	}
}

EVisibility FBlueprintVarActionDetails::ShowEditableCheckboxVisibilty() const
{
	UProperty* VariableProperty = CachedVariableProperty.Get();
	if (VariableProperty && GetPropertyOwnerBlueprint())
	{
		if (IsABlueprintVariable(VariableProperty) && !IsASCSVariable(VariableProperty))
		{
			return EVisibility::Visible;
		}
	}
	return EVisibility::Collapsed;
}

ECheckBoxState FBlueprintVarActionDetails::OnEditableCheckboxState() const
{
	UProperty* VariableProperty = CachedVariableProperty.Get();
	if (VariableProperty)
	{
		return VariableProperty->HasAnyPropertyFlags(CPF_DisableEditOnInstance) ? ECheckBoxState::Unchecked : ECheckBoxState::Checked;
	}
	return ECheckBoxState::Unchecked;
}

void FBlueprintVarActionDetails::OnEditableChanged(ECheckBoxState InNewState)
{
	FName VarName = CachedVariableName;

	// Toggle the flag on the blueprint's version of the variable description, based on state
	const bool bVariableIsExposed = InNewState == ECheckBoxState::Checked;

	UBlueprint* BlueprintObj = MyBlueprint.Pin()->GetBlueprintObj();
	FBlueprintEditorUtils::SetBlueprintOnlyEditableFlag(BlueprintObj, VarName, !bVariableIsExposed);
}

EVisibility FBlueprintVarActionDetails::ShowReadOnlyCheckboxVisibilty() const
{
	UProperty* VariableProperty = CachedVariableProperty.Get();
	if (VariableProperty && GetPropertyOwnerBlueprint())
	{
		if (IsABlueprintVariable(VariableProperty) && !IsASCSVariable(VariableProperty))
		{
			return EVisibility::Visible;
		}
	}
	return EVisibility::Collapsed;
}

ECheckBoxState FBlueprintVarActionDetails::OnReadyOnlyCheckboxState() const
{
	UProperty* VariableProperty = CachedVariableProperty.Get();
	if (VariableProperty)
	{
		return VariableProperty->HasAnyPropertyFlags(CPF_BlueprintReadOnly) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	return ECheckBoxState::Unchecked;
}

void FBlueprintVarActionDetails::OnReadyOnlyChanged(ECheckBoxState InNewState)
{
	FName VarName = CachedVariableName;

	// Toggle the flag on the blueprint's version of the variable description, based on state
	const bool bVariableIsReadOnly = InNewState == ECheckBoxState::Checked;

	UBlueprint* BlueprintObj = MyBlueprint.Pin()->GetBlueprintObj();
	FBlueprintEditorUtils::SetBlueprintPropertyReadOnlyFlag(BlueprintObj, VarName, bVariableIsReadOnly);
}

ECheckBoxState FBlueprintVarActionDetails::OnCreateWidgetCheckboxState() const
{
	UProperty* Property = CachedVariableProperty.Get();
	if (Property)
	{
		bool bMakingWidget = FEdMode::ShouldCreateWidgetForProperty(Property);

		return bMakingWidget ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	return ECheckBoxState::Unchecked;
}

void FBlueprintVarActionDetails::OnCreateWidgetChanged(ECheckBoxState InNewState)
{
	const FName VarName = CachedVariableName;
	if (VarName != NAME_None)
	{
		if (InNewState == ECheckBoxState::Checked)
		{
			FBlueprintEditorUtils::SetBlueprintVariableMetaData(GetBlueprintObj(), VarName, GetLocalVariableScope(CachedVariableProperty.Get()), FEdMode::MD_MakeEditWidget, TEXT("true"));
		}
		else
		{
			FBlueprintEditorUtils::RemoveBlueprintVariableMetaData(GetBlueprintObj(), VarName, GetLocalVariableScope(CachedVariableProperty.Get()), FEdMode::MD_MakeEditWidget);
		}
	}
}

EVisibility FBlueprintVarActionDetails::Show3DWidgetVisibility() const
{
	UProperty* VariableProperty = CachedVariableProperty.Get();
	if (VariableProperty && GetPropertyOwnerBlueprint())
	{
		if (IsABlueprintVariable(VariableProperty) && FEdMode::CanCreateWidgetForProperty(VariableProperty))
		{
			return EVisibility::Visible;
		}
	}
	return EVisibility::Collapsed;
}

bool FBlueprintVarActionDetails::Is3DWidgetEnabled()
{
	UProperty* VariableProperty = CachedVariableProperty.Get();
	if (VariableProperty)
	{
		return ( VariableProperty && !VariableProperty->HasAnyPropertyFlags(CPF_DisableEditOnInstance) ) ;
	}
	return false;
}

ECheckBoxState FBlueprintVarActionDetails::OnGetExposedToSpawnCheckboxState() const
{
	UProperty* Property = CachedVariableProperty.Get();
	if (Property)
	{
		return (Property && Property->GetBoolMetaData(FBlueprintMetadata::MD_ExposeOnSpawn) != false) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	return ECheckBoxState::Unchecked;
}

void FBlueprintVarActionDetails::OnExposedToSpawnChanged(ECheckBoxState InNewState)
{
	const FName VarName = CachedVariableName;
	if (VarName != NAME_None)
	{
		const bool bExposeOnSpawn = (InNewState == ECheckBoxState::Checked);
		if(bExposeOnSpawn)
		{
			FBlueprintEditorUtils::SetBlueprintVariableMetaData(GetBlueprintObj(), VarName, NULL, FBlueprintMetadata::MD_ExposeOnSpawn, TEXT("true"));
		}
		else
		{
			FBlueprintEditorUtils::RemoveBlueprintVariableMetaData(GetBlueprintObj(), VarName, NULL, FBlueprintMetadata::MD_ExposeOnSpawn);
		} 
	}
}

EVisibility FBlueprintVarActionDetails::ExposeOnSpawnVisibility() const
{
	UProperty* VariableProperty = CachedVariableProperty.Get();
	if (VariableProperty && GetPropertyOwnerBlueprint())
	{
		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
		FEdGraphPinType VariablePinType;
		K2Schema->ConvertPropertyToPinType(VariableProperty, VariablePinType);

		const bool bShowPrivacySetting = IsABlueprintVariable(VariableProperty) && !IsASCSVariable(VariableProperty);
		if (bShowPrivacySetting && (K2Schema->FindSetVariableByNameFunction(VariablePinType) != NULL))
		{
			return EVisibility::Visible;
		}
	}
	return EVisibility::Collapsed;
}

ECheckBoxState FBlueprintVarActionDetails::OnGetPrivateCheckboxState() const
{
	UProperty* Property = CachedVariableProperty.Get();
	if (Property)
	{
		return (Property && Property->GetBoolMetaData(FBlueprintMetadata::MD_Private) != false) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	return ECheckBoxState::Unchecked;
}

void FBlueprintVarActionDetails::OnPrivateChanged(ECheckBoxState InNewState)
{
	const FName VarName = CachedVariableName;
	if (VarName != NAME_None)
	{
		const bool bExposeOnSpawn = (InNewState == ECheckBoxState::Checked);
		if(bExposeOnSpawn)
		{
			FBlueprintEditorUtils::SetBlueprintVariableMetaData(GetBlueprintObj(), VarName, NULL, FBlueprintMetadata::MD_Private, TEXT("true"));
		}
		else
		{
			FBlueprintEditorUtils::RemoveBlueprintVariableMetaData(GetBlueprintObj(), VarName, NULL, FBlueprintMetadata::MD_Private);
		}
	}
}

EVisibility FBlueprintVarActionDetails::ExposePrivateVisibility() const
{
	UProperty* Property = CachedVariableProperty.Get();
	if (Property && GetPropertyOwnerBlueprint())
	{
		if (IsABlueprintVariable(Property) && !IsASCSVariable(Property))
		{
			return EVisibility::Visible;
		}
	}
	return EVisibility::Collapsed;
}

ECheckBoxState FBlueprintVarActionDetails::OnGetExposedToCinematicsCheckboxState() const
{
	UProperty* Property = CachedVariableProperty.Get();
	if (Property)
	{
		return Property && Property->HasAnyPropertyFlags(CPF_Interp) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	return ECheckBoxState::Unchecked;
}

void FBlueprintVarActionDetails::OnExposedToCinematicsChanged(ECheckBoxState InNewState)
{
	// Toggle the flag on the blueprint's version of the variable description, based on state
	const bool bExposeToCinematics = (InNewState == ECheckBoxState::Checked);
	
	const FName VarName = CachedVariableName;
	if (VarName != NAME_None)
	{
		FBlueprintEditorUtils::SetInterpFlag(GetBlueprintObj(), VarName, bExposeToCinematics);
	}
}

EVisibility FBlueprintVarActionDetails::ExposeToCinematicsVisibility() const
{
	UProperty* VariableProperty = CachedVariableProperty.Get();
	if (VariableProperty && !IsALocalVariable(VariableProperty))
	{
		const bool bIsInteger = VariableProperty->IsA(UIntProperty::StaticClass());
		const bool bIsByte = VariableProperty->IsA(UByteProperty::StaticClass());
		const bool bIsEnum = VariableProperty->IsA(UEnumProperty::StaticClass());
		const bool bIsFloat = VariableProperty->IsA(UFloatProperty::StaticClass());
		const bool bIsBool = VariableProperty->IsA(UBoolProperty::StaticClass());
		const bool bIsStr = VariableProperty->IsA(UStrProperty::StaticClass());
		const bool bIsVectorStruct = VariableProperty->IsA(UStructProperty::StaticClass()) && Cast<UStructProperty>(VariableProperty)->Struct->GetFName() == NAME_Vector;
		const bool bIsTransformStruct = VariableProperty->IsA(UStructProperty::StaticClass()) && Cast<UStructProperty>(VariableProperty)->Struct->GetFName() == NAME_Transform;
		const bool bIsColorStruct = VariableProperty->IsA(UStructProperty::StaticClass()) && Cast<UStructProperty>(VariableProperty)->Struct->GetFName() == NAME_Color;
		const bool bIsLinearColorStruct = VariableProperty->IsA(UStructProperty::StaticClass()) && Cast<UStructProperty>(VariableProperty)->Struct->GetFName() == NAME_LinearColor;
		const bool bIsActorProperty = VariableProperty->IsA(UObjectProperty::StaticClass()) && Cast<UObjectProperty>(VariableProperty)->PropertyClass->IsChildOf(AActor::StaticClass());

		if (bIsInteger || bIsByte || bIsEnum || bIsFloat || bIsBool || bIsStr || bIsVectorStruct || bIsTransformStruct || bIsColorStruct || bIsLinearColorStruct || bIsActorProperty)
		{
			return EVisibility::Visible;
		}
		else
		{
			ISequencerModule* SequencerModule = FModuleManager::Get().GetModulePtr<ISequencerModule>("Sequencer");
			if (SequencerModule->CanAnimateProperty(FAnimatedPropertyKey::FromProperty(VariableProperty)))
			{
				return EVisibility::Visible;
			}
			else if (UObjectProperty* ObjectProperty = Cast<UObjectProperty>(VariableProperty))
			{
				UClass* ClassType = ObjectProperty->PropertyClass ? ObjectProperty->PropertyClass->GetSuperClass() : nullptr;
				while (ClassType)
				{
					if (SequencerModule->CanAnimateProperty(FAnimatedPropertyKey::FromObjectType(ClassType)))
					{
						return EVisibility::Visible;
					}
					ClassType = ClassType->GetSuperClass();
				}
			}
		}
	}
	return EVisibility::Collapsed;
}

TSharedPtr<FString> FBlueprintVarActionDetails::GetVariableReplicationCondition() const
{
	ELifetimeCondition VariableRepCondition = COND_None;
		
	const UProperty* const Property = CachedVariableProperty.Get();

	if (Property)
	{
		VariableRepCondition = Property->GetBlueprintReplicationCondition();
	}

	return ReplicationConditionEnumTypeNames[(uint8)VariableRepCondition];
}

void FBlueprintVarActionDetails::OnChangeReplicationCondition(TSharedPtr<FString> ItemSelected, ESelectInfo::Type SelectInfo)
{
	int32 NewSelection;
	const bool bFound = ReplicationConditionEnumTypeNames.Find(ItemSelected, NewSelection);
	check(bFound && NewSelection != INDEX_NONE);

	const ELifetimeCondition NewRepCondition = (ELifetimeCondition)NewSelection;

	UBlueprint* const BlueprintObj = GetBlueprintObj();
	const FName VarName = CachedVariableName;

	if (BlueprintObj && VarName != NAME_None)
	{
		const int32 VarIndex = FBlueprintEditorUtils::FindNewVariableIndex(BlueprintObj, VarName);

		if (VarIndex != INDEX_NONE)
		{
			BlueprintObj->NewVariables[VarIndex].ReplicationCondition = NewRepCondition;
		
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BlueprintObj);
		}
	}

}

bool FBlueprintVarActionDetails::ReplicationConditionEnabled() const
{
	const UProperty* const VariableProperty = CachedVariableProperty.Get();
	if (VariableProperty)
	{
		const uint64 *PropFlagPtr = FBlueprintEditorUtils::GetBlueprintVariablePropertyFlags(GetBlueprintObj(), VariableProperty->GetFName());
		uint64 PropFlags = 0;

		if (PropFlagPtr != nullptr)
		{
			PropFlags = *PropFlagPtr;
			return (PropFlags & CPF_Net) > 0;
			
		}
	}

	return false;
}

bool FBlueprintVarActionDetails::ReplicationEnabled() const
{
	// Update FBlueprintVarActionDetails::ReplicationTooltip if you alter this function
	// shat users can understand why replication settins are disabled!
	bool bVariableCanBeReplicated = true;
	const UProperty* const VariableProperty = CachedVariableProperty.Get();
	if (VariableProperty)
	{
		// sets and maps cannot yet be replicated:
		bVariableCanBeReplicated = Cast<USetProperty>(VariableProperty) == nullptr && Cast<UMapProperty>(VariableProperty) == nullptr;
	}
	return bVariableCanBeReplicated && IsVariableInBlueprint();
}

FText FBlueprintVarActionDetails::ReplicationTooltip() const
{
	if(ReplicationEnabled())
	{
		return LOCTEXT("VariableReplicate_Tooltip", "Should this Variable be replicated over the network?");
	}
	else
	{
		return LOCTEXT("VariableReplicateDisabled_Tooltip", "Set and Map properties cannot be replicated");
	}
}

ECheckBoxState FBlueprintVarActionDetails::OnGetConfigVariableCheckboxState() const
{
	UBlueprint* BlueprintObj = GetPropertyOwnerBlueprint();
	const FName VarName = CachedVariableName;
	ECheckBoxState CheckboxValue = ECheckBoxState::Unchecked;

	if( BlueprintObj && VarName != NAME_None )
	{
		const int32 VarIndex = FBlueprintEditorUtils::FindNewVariableIndex( BlueprintObj, VarName );

		if( VarIndex != INDEX_NONE && BlueprintObj->NewVariables[ VarIndex ].PropertyFlags & CPF_Config )
		{
			CheckboxValue = ECheckBoxState::Checked;
		}
	}
	return CheckboxValue;
}

void FBlueprintVarActionDetails::OnSetConfigVariableState( ECheckBoxState InNewState )
{
	UBlueprint* BlueprintObj = GetBlueprintObj();
	const FName VarName = CachedVariableName;

	if( BlueprintObj && VarName != NAME_None )
	{
		const int32 VarIndex = FBlueprintEditorUtils::FindNewVariableIndex( BlueprintObj, VarName );

		if( VarIndex != INDEX_NONE )
		{
			if( InNewState == ECheckBoxState::Checked )
			{
				BlueprintObj->NewVariables[ VarIndex ].PropertyFlags |= CPF_Config;
			}
			else
			{
				BlueprintObj->NewVariables[ VarIndex ].PropertyFlags &= ~CPF_Config;
			}
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified( BlueprintObj );
		}
	}
}

EVisibility FBlueprintVarActionDetails::ExposeConfigVisibility() const
{
	UProperty* Property = CachedVariableProperty.Get();
	if (Property)
	{
		if (IsABlueprintVariable(Property) && !IsASCSVariable(Property))
		{
			return EVisibility::Visible;
		}
	}
	return EVisibility::Collapsed;
}

bool FBlueprintVarActionDetails::IsConfigCheckBoxEnabled() const
{
	bool bEnabled = IsVariableInBlueprint();
	if (bEnabled && CachedVariableProperty.IsValid())
	{
		if (UProperty* VariableProperty = CachedVariableProperty.Get())
		{
			// meant to match up with UHT's FPropertyBase::IsObject(), which it uses to block object properties from being marked with CPF_Config
			bEnabled = VariableProperty->IsA<UClassProperty>() || VariableProperty->IsA<USoftClassProperty>() || VariableProperty->IsA<USoftObjectProperty>() ||
				(!VariableProperty->IsA<UObjectPropertyBase>() && !VariableProperty->IsA<UInterfaceProperty>());
		}
	}
	return bEnabled;
}

FText FBlueprintVarActionDetails::OnGetMetaKeyValue(FName Key) const
{
	FName VarName = CachedVariableName;
	if (VarName != NAME_None)
	{
		if ( UBlueprint* BlueprintObj = GetPropertyOwnerBlueprint() )
		{
			FString Result;
			FBlueprintEditorUtils::GetBlueprintVariableMetaData(BlueprintObj, VarName, GetLocalVariableScope(CachedVariableProperty.Get()), Key, /*out*/ Result);

			return FText::FromString(Result);
		}
	}
	return FText();
}

void FBlueprintVarActionDetails::OnMetaKeyValueChanged(const FText& NewMinValue, ETextCommit::Type CommitInfo, FName Key)
{
	FName VarName = CachedVariableName;
	if (VarName != NAME_None)
	{
		if ((CommitInfo == ETextCommit::OnEnter) || (CommitInfo == ETextCommit::OnUserMovedFocus))
		{
			FBlueprintEditorUtils::SetBlueprintVariableMetaData(GetBlueprintObj(), VarName, GetLocalVariableScope(CachedVariableProperty.Get()), Key, NewMinValue.ToString());
		}
	}
}

EVisibility FBlueprintVarActionDetails::RangeVisibility() const
{
	UProperty* VariableProperty = CachedVariableProperty.Get();
	if (VariableProperty)
	{
		const bool bIsInteger = VariableProperty->IsA(UIntProperty::StaticClass());
		const bool bIsNonEnumByte = (VariableProperty->IsA(UByteProperty::StaticClass()) && Cast<const UByteProperty>(VariableProperty)->Enum == NULL);
		const bool bIsFloat = VariableProperty->IsA(UFloatProperty::StaticClass());

		if (IsABlueprintVariable(VariableProperty) && (bIsInteger || bIsNonEnumByte || bIsFloat))
		{
			return EVisibility::Visible;
		}
	}
	return EVisibility::Collapsed;
}

EVisibility FBlueprintVarActionDetails::BitmaskVisibility() const
{
	UProperty* VariableProperty = CachedVariableProperty.Get();
	if (VariableProperty && IsABlueprintVariable(VariableProperty) && VariableProperty->IsA(UIntProperty::StaticClass()))
	{
		return EVisibility::Visible;
	}

	return EVisibility::Collapsed;
}

ECheckBoxState FBlueprintVarActionDetails::OnBitmaskCheckboxState() const
{
	UProperty* Property = CachedVariableProperty.Get();
	if (Property)
	{
		return (Property && Property->HasMetaData(FBlueprintMetadata::MD_Bitmask)) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	return ECheckBoxState::Unchecked;
}

void FBlueprintVarActionDetails::OnBitmaskChanged(ECheckBoxState InNewState)
{
	const FName VarName = CachedVariableName;
	if (VarName != NAME_None)
	{
		UBlueprint* LocalBlueprint = GetBlueprintObj();

		const bool bIsBitmask = (InNewState == ECheckBoxState::Checked);
		if (bIsBitmask)
		{
			FBlueprintEditorUtils::SetBlueprintVariableMetaData(LocalBlueprint, VarName, nullptr, FBlueprintMetadata::MD_Bitmask, TEXT(""));
		}
		else
		{
			FBlueprintEditorUtils::RemoveBlueprintVariableMetaData(LocalBlueprint, VarName, nullptr, FBlueprintMetadata::MD_Bitmask);
		}

		// Reset default value
		if (LocalBlueprint->GeneratedClass)
		{
			UObject* CDO = LocalBlueprint->GeneratedClass->GetDefaultObject(false);
			UProperty* VarProperty = FindField<UProperty>(LocalBlueprint->GeneratedClass, VarName);

			if (CDO != nullptr && VarProperty != nullptr)
			{
				VarProperty->InitializeValue_InContainer(CDO);
			}
		}

		TArray<UK2Node_Variable*> VariableNodes;
		FBlueprintEditorUtils::GetAllNodesOfClass<UK2Node_Variable>(GetBlueprintObj(), VariableNodes);

		for (TArray<UK2Node_Variable*>::TConstIterator NodeIt(VariableNodes); NodeIt; ++NodeIt)
		{
			UK2Node_Variable* CurrentNode = *NodeIt;
			if (VarName == CurrentNode->GetVarName())
			{
				CurrentNode->ReconstructNode();
			}
		}
	}
}

TSharedPtr<FString> FBlueprintVarActionDetails::GetBitmaskEnumTypeName() const
{
	TSharedPtr<FString> Result;
	const FName VarName = CachedVariableName;

	if (BitmaskEnumTypeNames.Num() > 0 && VarName != NAME_None)
	{
		Result = BitmaskEnumTypeNames[0];

		FString OutValue;
		FBlueprintEditorUtils::GetBlueprintVariableMetaData(GetBlueprintObj(), VarName, nullptr, FBlueprintMetadata::MD_BitmaskEnum, OutValue);

		for (int32 i = 1; i < BitmaskEnumTypeNames.Num(); ++i)
		{
			if (OutValue == *BitmaskEnumTypeNames[i])
			{
				Result = BitmaskEnumTypeNames[i];
				break;
			}
		}
	}

	return Result;
}

void FBlueprintVarActionDetails::OnBitmaskEnumTypeChanged(TSharedPtr<FString> ItemSelected, ESelectInfo::Type SelectInfo)
{
	const FName VarName = CachedVariableName;
	if (VarName != NAME_None)
	{
		UBlueprint* LocalBlueprint = GetBlueprintObj();

		if (ItemSelected == BitmaskEnumTypeNames[0])
		{
			FBlueprintEditorUtils::RemoveBlueprintVariableMetaData(LocalBlueprint, VarName, nullptr, FBlueprintMetadata::MD_BitmaskEnum);
		}
		else if(ItemSelected.IsValid())
		{
			FBlueprintEditorUtils::SetBlueprintVariableMetaData(LocalBlueprint, VarName, nullptr, FBlueprintMetadata::MD_BitmaskEnum, *ItemSelected);
		}

		// Reset default value
		if (LocalBlueprint->GeneratedClass)
		{
			UObject* CDO = LocalBlueprint->GeneratedClass->GetDefaultObject(false);
			UProperty* VarProperty = FindField<UProperty>(LocalBlueprint->GeneratedClass, VarName);

			if (CDO != nullptr && VarProperty != nullptr)
			{
				VarProperty->InitializeValue_InContainer(CDO);
			}
		}

		TArray<UK2Node_Variable*> VariableNodes;
		FBlueprintEditorUtils::GetAllNodesOfClass<UK2Node_Variable>(GetBlueprintObj(), VariableNodes);

		for (TArray<UK2Node_Variable*>::TConstIterator NodeIt(VariableNodes); NodeIt; ++NodeIt)
		{
			UK2Node_Variable* CurrentNode = *NodeIt;
			if (VarName == CurrentNode->GetVarName())
			{
				CurrentNode->ReconstructNode();
			}
		}
	}
}

TSharedPtr<FString> FBlueprintVarActionDetails::GetVariableReplicationType() const
{
	EVariableReplication::Type VariableReplication = EVariableReplication::None;
	
	uint64 PropFlags = 0;
	UProperty* VariableProperty = CachedVariableProperty.Get();

	if (VariableProperty && (IsVariableInBlueprint() || IsVariableInheritedByBlueprint()))
	{
		UBlueprint* BlueprintObj = GetPropertyOwnerBlueprint();
		if (BlueprintObj != nullptr)
		{
			uint64 *PropFlagPtr = FBlueprintEditorUtils::GetBlueprintVariablePropertyFlags(BlueprintObj, VariableProperty->GetFName());

			if (PropFlagPtr != NULL)
			{
				PropFlags = *PropFlagPtr;
				bool IsReplicated = (PropFlags & CPF_Net) > 0;
				bool bHasRepNotify = FBlueprintEditorUtils::GetBlueprintVariableRepNotifyFunc(BlueprintObj, VariableProperty->GetFName()) != NAME_None;
				if (bHasRepNotify)
				{
					// Verify they actually have a valid rep notify function still
					UClass* GenClass = GetPropertyOwnerBlueprint()->SkeletonGeneratedClass;
					UFunction* OnRepFunc = GenClass->FindFunctionByName(FBlueprintEditorUtils::GetBlueprintVariableRepNotifyFunc(BlueprintObj, VariableProperty->GetFName()));
					if (OnRepFunc == NULL || OnRepFunc->NumParms != 0 || OnRepFunc->GetReturnProperty() != NULL)
					{
						bHasRepNotify = false;
						ReplicationOnRepFuncChanged(FName(NAME_None).ToString());
					}
				}

				VariableReplication = !IsReplicated ? EVariableReplication::None :
					bHasRepNotify ? EVariableReplication::RepNotify : EVariableReplication::Replicated;
			}
		}
	}

	return ReplicationOptions[(int32)VariableReplication];
}

void FBlueprintVarActionDetails::OnChangeReplication(TSharedPtr<FString> ItemSelected, ESelectInfo::Type SelectInfo)
{
	int32 NewSelection;
	bool bFound = ReplicationOptions.Find(ItemSelected, NewSelection);
	check(bFound && NewSelection != INDEX_NONE);

	EVariableReplication::Type VariableReplication = (EVariableReplication::Type)NewSelection;
	
	UProperty* VariableProperty = CachedVariableProperty.Get();

	UBlueprint* const BlueprintObj = GetBlueprintObj();
	const FName VarName = CachedVariableName;
	int32 VarIndex = INDEX_NONE;
	if (BlueprintObj && VarName != NAME_None)
	{
		VarIndex = FBlueprintEditorUtils::FindNewVariableIndex(BlueprintObj, VarName);
	}

	if (VariableProperty)
	{
		uint64 *PropFlagPtr = FBlueprintEditorUtils::GetBlueprintVariablePropertyFlags(GetBlueprintObj(), VariableProperty->GetFName());
		if (PropFlagPtr != NULL)
		{
			switch(VariableReplication)
			{
			case EVariableReplication::None:
				*PropFlagPtr &= ~CPF_Net;
				ReplicationOnRepFuncChanged(FName(NAME_None).ToString());

				//set replication condition to none:
				if (VarIndex != INDEX_NONE)
				{
					BlueprintObj->NewVariables[VarIndex].ReplicationCondition = COND_None;
				}
				
				break;
				
			case EVariableReplication::Replicated:
				*PropFlagPtr |= CPF_Net;
				ReplicationOnRepFuncChanged(FName(NAME_None).ToString());	
				break;

			case EVariableReplication::RepNotify:
				*PropFlagPtr |= CPF_Net;
				FString NewFuncName = FString::Printf(TEXT("OnRep_%s"), *VariableProperty->GetName());
				UEdGraph* FuncGraph = FindObject<UEdGraph>(BlueprintObj, *NewFuncName);
				if (!FuncGraph)
				{
					FuncGraph = FBlueprintEditorUtils::CreateNewGraph(BlueprintObj, FName(*NewFuncName), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
					FBlueprintEditorUtils::AddFunctionGraph<UClass>(BlueprintObj, FuncGraph, false, NULL);
				}

				if (FuncGraph)
				{
					ReplicationOnRepFuncChanged(NewFuncName);
				}
				break;
			}

			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BlueprintObj);
		}
	}
}

void FBlueprintVarActionDetails::ReplicationOnRepFuncChanged(const FString& NewOnRepFunc) const
{
	FName NewFuncName = FName(*NewOnRepFunc);
	UProperty* VariableProperty = CachedVariableProperty.Get();

	if (VariableProperty)
	{
		FBlueprintEditorUtils::SetBlueprintVariableRepNotifyFunc(GetBlueprintObj(), VariableProperty->GetFName(), NewFuncName);
		uint64 *PropFlagPtr = FBlueprintEditorUtils::GetBlueprintVariablePropertyFlags(GetBlueprintObj(), VariableProperty->GetFName());
		if (PropFlagPtr != NULL)
		{
			if (NewFuncName != NAME_None)
			{
				*PropFlagPtr |= CPF_RepNotify;
				*PropFlagPtr |= CPF_Net;
			}
			else
			{
				*PropFlagPtr &= ~CPF_RepNotify;
			}
		}
	}
}

EVisibility FBlueprintVarActionDetails::ReplicationVisibility() const
{
	UProperty* VariableProperty = CachedVariableProperty.Get();
	if(VariableProperty)
	{
		if (!IsASCSVariable(VariableProperty) && IsABlueprintVariable(VariableProperty))
		{
			return EVisibility::Visible;
		}
	}
	return EVisibility::Collapsed;
}

TSharedRef<SWidget> FBlueprintVarActionDetails::BuildEventsMenuForVariable() const
{
	if( MyBlueprint.IsValid() )
	{
		TSharedPtr<SMyBlueprint> MyBlueprintPtr = MyBlueprint.Pin();
		FEdGraphSchemaAction_K2Var* Variable = MyBlueprintPtr->SelectionAsVar();
		UObjectProperty* ComponentProperty = Variable ? Cast<UObjectProperty>(Variable->GetProperty()) : NULL;
		TWeakPtr<FBlueprintEditor> BlueprintEditorPtr = MyBlueprintPtr->GetBlueprintEditor();
		if( BlueprintEditorPtr.IsValid() && ComponentProperty )
		{
			TSharedPtr<SSCSEditor> Editor =  BlueprintEditorPtr.Pin()->GetSCSEditor();
			FMenuBuilder MenuBuilder( true, NULL );
			Editor->BuildMenuEventsSection( MenuBuilder, BlueprintEditorPtr.Pin()->GetBlueprintObj(), ComponentProperty->PropertyClass, 
											FCanExecuteAction::CreateSP(BlueprintEditorPtr.Pin().Get(), &FBlueprintEditor::InEditingMode),
											FGetSelectedObjectsDelegate::CreateSP(MyBlueprintPtr.Get(), &SMyBlueprint::GetSelectedItemsForContextMenu));
			return MenuBuilder.MakeWidget();
		}
	}
	return SNullWidget::NullWidget;
}

void FBlueprintVarActionDetails::OnPostEditorRefresh()
{
	CachedVariableProperty = SelectionAsProperty();
	CachedVariableName = GetVariableName();
}

EVisibility FBlueprintVarActionDetails::GetTransientVisibility() const
{
	UProperty* VariableProperty = CachedVariableProperty.Get();
	if (VariableProperty)
	{
		if (IsABlueprintVariable(VariableProperty) && !IsASCSVariable(VariableProperty))
		{
			return EVisibility::Visible;
		}
	}
	return EVisibility::Collapsed;
}

ECheckBoxState FBlueprintVarActionDetails::OnGetTransientCheckboxState() const
{
	UProperty* Property = CachedVariableProperty.Get();
	if (Property)
	{
		return (Property && Property->HasAnyPropertyFlags(CPF_Transient)) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	return ECheckBoxState::Unchecked;
}

void FBlueprintVarActionDetails::OnTransientChanged(ECheckBoxState InNewState)
{
	UProperty* Property = CachedVariableProperty.Get();
	if (Property)
	{
		const bool bTransientFlag = (InNewState == ECheckBoxState::Checked);
		FBlueprintEditorUtils::SetVariableTransientFlag(GetBlueprintObj(), Property->GetFName(), bTransientFlag);
	}
}

EVisibility FBlueprintVarActionDetails::GetSaveGameVisibility() const
{
	UProperty* VariableProperty = CachedVariableProperty.Get();
	if (VariableProperty)
	{
		if (IsABlueprintVariable(VariableProperty) && !IsASCSVariable(VariableProperty))
		{
			return EVisibility::Visible;
		}
	}
	return EVisibility::Collapsed;
}

ECheckBoxState FBlueprintVarActionDetails::OnGetSaveGameCheckboxState() const
{
	UProperty* Property = CachedVariableProperty.Get();
	if (Property)
	{
		return (Property && Property->HasAnyPropertyFlags(CPF_SaveGame)) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	return ECheckBoxState::Unchecked;
}

void FBlueprintVarActionDetails::OnSaveGameChanged(ECheckBoxState InNewState)
{
	UProperty* Property = CachedVariableProperty.Get();
	if (Property)
	{
		const bool bSaveGameFlag = (InNewState == ECheckBoxState::Checked);
		FBlueprintEditorUtils::SetVariableSaveGameFlag(GetBlueprintObj(), Property->GetFName(), bSaveGameFlag);
	}
}

EVisibility FBlueprintVarActionDetails::GetAdvancedDisplayVisibility() const
{
	UProperty* VariableProperty = CachedVariableProperty.Get();
	if (VariableProperty)
	{
		if (IsABlueprintVariable(VariableProperty))
		{
			return EVisibility::Visible;
		}
	}
	return EVisibility::Collapsed;
}

ECheckBoxState FBlueprintVarActionDetails::OnGetAdvancedDisplayCheckboxState() const
{
	UProperty* Property = CachedVariableProperty.Get();
	if (Property)
	{
		return (Property && Property->HasAnyPropertyFlags(CPF_AdvancedDisplay)) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	return ECheckBoxState::Unchecked;
}

void FBlueprintVarActionDetails::OnAdvancedDisplayChanged(ECheckBoxState InNewState)
{
	if (UProperty* Property = CachedVariableProperty.Get())
	{
		const bool bAdvancedFlag = (InNewState == ECheckBoxState::Checked);
		FBlueprintEditorUtils::SetVariableAdvancedDisplayFlag(GetBlueprintObj(), Property->GetFName(), bAdvancedFlag);
	}
}

EVisibility FBlueprintVarActionDetails::GetMultilineVisibility() const
{
	if (UProperty* VariableProperty = CachedVariableProperty.Get())
	{
		if (IsABlueprintVariable(VariableProperty))
		{
			if (VariableProperty->IsA(UTextProperty::StaticClass()) || VariableProperty->IsA(UStrProperty::StaticClass()))
			{
				return EVisibility::Visible;
			}
		}
	}
	return EVisibility::Collapsed;
}

ECheckBoxState FBlueprintVarActionDetails::OnGetMultilineCheckboxState() const
{
	UProperty* Property = CachedVariableProperty.Get();
	if (Property)
	{
		return (Property && Property->GetBoolMetaData(TEXT("MultiLine"))) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	return ECheckBoxState::Unchecked;
}

void FBlueprintVarActionDetails::OnMultilineChanged(ECheckBoxState InNewState)
{
	UProperty* Property = CachedVariableProperty.Get();
	if (Property)
	{
		const bool bMultiline = (InNewState == ECheckBoxState::Checked);
		FBlueprintEditorUtils::SetBlueprintVariableMetaData(GetBlueprintObj(), Property->GetFName(), GetLocalVariableScope(CachedVariableProperty.Get()), TEXT("MultiLine"), bMultiline ? TEXT("true") : TEXT("false"));
	}
}

EVisibility FBlueprintVarActionDetails::IsTooltipEditVisible() const
{
	UProperty* VariableProperty = CachedVariableProperty.Get();
	if (VariableProperty)
	{
		if ((IsABlueprintVariable(VariableProperty) && !IsASCSVariable(VariableProperty)) || IsALocalVariable(VariableProperty))
		{
			return EVisibility::Visible;
		}
	}
	return EVisibility::Collapsed;
}

void FBlueprintVarActionDetails::OnFinishedChangingProperties(const FPropertyChangedEvent& InPropertyChangedEvent, TSharedPtr<FStructOnScope> InStructData, TWeakObjectPtr<UK2Node_EditablePinBase> InEntryNode)
{
	check(InPropertyChangedEvent.MemberProperty
		&& InPropertyChangedEvent.MemberProperty->GetOwnerStruct()
		&& InPropertyChangedEvent.MemberProperty->GetOwnerStruct()->IsA<UFunction>());

	// Find the top level property that was modified within the UFunction
	const UProperty* DirectProperty = InPropertyChangedEvent.MemberProperty;
	while (!Cast<const UFunction>(DirectProperty->GetOuter()))
	{
		DirectProperty = CastChecked<const UProperty>(DirectProperty->GetOuter());
	}

	FString DefaultValueString;
	bool bDefaultValueSet = false;

	if (InStructData.IsValid())
	{
		bDefaultValueSet = FBlueprintEditorUtils::PropertyValueToString(DirectProperty, InStructData->GetStructMemory(), DefaultValueString);

		if(bDefaultValueSet)
		{
			UK2Node_FunctionEntry* FuncEntry = Cast<UK2Node_FunctionEntry>(InEntryNode.Get());

			// Search out the correct local variable in the Function Entry Node and set the default value
			for (FBPVariableDescription& LocalVar : FuncEntry->LocalVariables)
			{
				if (LocalVar.VarName == DirectProperty->GetFName() && LocalVar.DefaultValue != DefaultValueString)
				{
					const FScopedTransaction Transaction(LOCTEXT("ChangeDefaults", "Change Defaults"));

					FuncEntry->Modify();
					GetBlueprintObj()->Modify();
					LocalVar.DefaultValue = DefaultValueString;
					FBlueprintEditorUtils::MarkBlueprintAsModified(GetBlueprintObj());
					break;
				}
			}
		}
	}
}

bool FBlueprintVarActionDetails::IsVariableInheritedByBlueprint() const
{
	UClass* PropertyOwnerClass = nullptr;
	if (UBlueprint* PropertyOwnerBP = GetPropertyOwnerBlueprint())
	{
		PropertyOwnerClass = PropertyOwnerBP->SkeletonGeneratedClass;
	}
	else if (CachedVariableProperty.IsValid())
	{
		PropertyOwnerClass = CachedVariableProperty->GetOwnerClass();
	}
	return GetBlueprintObj()->SkeletonGeneratedClass->IsChildOf(PropertyOwnerClass);
}

static FDetailWidgetRow& AddRow( TArray<TSharedRef<FDetailWidgetRow> >& OutChildRows )
{
	TSharedRef<FDetailWidgetRow> NewRow( new FDetailWidgetRow );
	OutChildRows.Add( NewRow );

	return *NewRow;
}

void FBlueprintGraphArgumentGroupLayout::SetOnRebuildChildren( FSimpleDelegate InOnRegenerateChildren )
{
	GraphActionDetailsPtr.Pin()->SetRefreshDelegate(InOnRegenerateChildren, TargetNode == GraphActionDetailsPtr.Pin()->GetFunctionEntryNode().Get());
}

void FBlueprintGraphArgumentGroupLayout::GenerateChildContent( IDetailChildrenBuilder& ChildrenBuilder )
{
	bool WasContentAdded = false;
	if(TargetNode.IsValid())
	{
		TArray<TSharedPtr<FUserPinInfo>> Pins = TargetNode->UserDefinedPins;

		if(Pins.Num() > 0)
		{
			bool bIsInputNode = TargetNode == GraphActionDetailsPtr.Pin()->GetFunctionEntryNode().Get();
			for (int32 i = 0; i < Pins.Num(); ++i)
			{
				TSharedRef<class FBlueprintGraphArgumentLayout> BlueprintArgumentLayout = MakeShareable(new FBlueprintGraphArgumentLayout(
					TWeakPtr<FUserPinInfo>(Pins[i]),
					TargetNode.Get(),
					GraphActionDetailsPtr,
					FName(*FString::Printf(bIsInputNode ? TEXT("InputArgument%i") : TEXT("OutputArgument%i"), i)),
					bIsInputNode));
				ChildrenBuilder.AddCustomBuilder(BlueprintArgumentLayout);
				WasContentAdded = true;
			}
		}
	}
	if (!WasContentAdded)
	{
		// Add a text widget to let the user know to hit the + icon to add parameters.
		ChildrenBuilder.AddCustomRow(FText::GetEmpty()).WholeRowContent()
			.MaxDesiredWidth(980.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.Padding(0.0f, 0.0f, 4.0f, 0.0f)
					.AutoWidth()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("NoArgumentsAddedForBlueprint", "Please press the + icon above to add parameters"))
						.Font(IDetailLayoutBuilder::GetDetailFont())
					]
			];
	}
}

// Internal
static bool ShouldAllowWildcard(UK2Node_EditablePinBase* TargetNode)
{
	// allow wildcards for tunnel nodes in macro graphs
	if ( TargetNode->IsA(UK2Node_Tunnel::StaticClass()) )
	{
		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
		return ( K2Schema->GetGraphType( TargetNode->GetGraph() ) == GT_Macro );
	}

	return false;
}

void FBlueprintGraphArgumentLayout::GenerateHeaderRowContent( FDetailWidgetRow& NodeRow )
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	ETypeTreeFilter TypeTreeFilter = ETypeTreeFilter::None;
	if (TargetNode->CanModifyExecutionWires())
	{
		TypeTreeFilter |= ETypeTreeFilter::AllowExec;
	}

	if (ShouldAllowWildcard(TargetNode))
	{
		TypeTreeFilter |= ETypeTreeFilter::AllowWildcard;
	}

	NodeRow
	.NameContent()
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.FillWidth(1)
		.VAlign(VAlign_Center)
		[
			SAssignNew(ArgumentNameWidget, SEditableTextBox)
				.Text( this, &FBlueprintGraphArgumentLayout::OnGetArgNameText )
				.OnTextChanged(this, &FBlueprintGraphArgumentLayout::OnArgNameChange)
				.OnTextCommitted(this, &FBlueprintGraphArgumentLayout::OnArgNameTextCommitted)
				.ToolTipText(this, &FBlueprintGraphArgumentLayout::OnGetArgToolTipText)
				.Font( IDetailLayoutBuilder::GetDetailFont() )
				.IsEnabled(!ShouldPinBeReadOnly())
		]
	]
	.ValueContent()
	.MaxDesiredWidth(980.f)
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.Padding(0.0f, 0.0f, 4.0f, 0.0f)
		.AutoWidth()
		[
			SNew(SPinTypeSelector, FGetPinTypeTree::CreateUObject(K2Schema, &UEdGraphSchema_K2::GetVariableTypeTree))
				.TargetPinType(this, &FBlueprintGraphArgumentLayout::OnGetPinInfo)
				.OnPinTypePreChanged(this, &FBlueprintGraphArgumentLayout::OnPrePinInfoChange)
				.OnPinTypeChanged(this, &FBlueprintGraphArgumentLayout::PinInfoChanged)
				.Schema(K2Schema)
				.TypeTreeFilter(TypeTreeFilter)
				.bAllowArrays(!ShouldPinBeReadOnly())
				.IsEnabled(!ShouldPinBeReadOnly(true))
				.Font( IDetailLayoutBuilder::GetDetailFont() )
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.ContentPadding(0)
			.IsEnabled(!IsPinEditingReadOnly())
			.OnClicked(this, &FBlueprintGraphArgumentLayout::OnArgMoveUp)
			.ToolTipText(LOCTEXT("FunctionArgDetailsArgMoveUpTooltip", "Move this parameter up in the list."))
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush("BlueprintEditor.Details.ArgUpButton"))
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2, 0)
		[
			SNew(SButton)
			.ContentPadding(0)
			.IsEnabled(!IsPinEditingReadOnly())
			.OnClicked(this, &FBlueprintGraphArgumentLayout::OnArgMoveDown)
			.ToolTipText(LOCTEXT("FunctionArgDetailsArgMoveDownTooltip", "Move this parameter down in the list."))
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush("BlueprintEditor.Details.ArgDownButton"))
			]
		]
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.Padding(10, 0, 0, 0)
		.AutoWidth()
		[
			PropertyCustomizationHelpers::MakeClearButton(FSimpleDelegate::CreateSP(this, &FBlueprintGraphArgumentLayout::OnRemoveClicked), LOCTEXT("FunctionArgDetailsClearTooltip", "Remove this parameter."), !IsPinEditingReadOnly())
		]

	];
}

void FBlueprintGraphArgumentLayout::GenerateChildContent( IDetailChildrenBuilder& ChildrenBuilder )
{
	if (bHasDefaultValue)
	{
		UEdGraphPin* FoundPin = GetPin();
		if (FoundPin)
		{
			// Certain types are outlawed at the compiler level
			const bool bTypeWithNoDefaults = (FoundPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Object) || (FoundPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Class) || (FoundPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Interface) || UEdGraphSchema_K2::IsExecPin(*FoundPin);

			if (!FoundPin->PinType.bIsReference && !bTypeWithNoDefaults)
			{
				DefaultValuePinWidget = FNodeFactory::CreatePinWidget(FoundPin);
				DefaultValuePinWidget->SetOnlyShowDefaultValue(true);
				TSharedRef<SWidget> DefaultValueWidget = DefaultValuePinWidget->GetDefaultValueWidget();

				if (DefaultValueWidget != SNullWidget::NullWidget)
				{
					ChildrenBuilder.AddCustomRow(LOCTEXT("FunctionArgDetailsDefaultValue", "Default Value"))
						.NameContent()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("FunctionArgDetailsDefaultValue", "Default Value"))
							.ToolTipText(LOCTEXT("FunctionArgDetailsDefaultValueParamTooltip", "The default value of the parameter."))
							.Font(IDetailLayoutBuilder::GetDetailFont())
						]
						.ValueContent()
						.MaxDesiredWidth(512)
						[
							DefaultValueWidget
						];
				}
				else
				{
					DefaultValuePinWidget.Reset();
				}
			}
		}

		// Exec pins can't be passed by reference
		if (FoundPin == nullptr || !UEdGraphSchema_K2::IsExecPin(*FoundPin))
		{
			ChildrenBuilder.AddCustomRow(LOCTEXT("FunctionArgDetailsPassByReference", "Pass-by-Reference"))
				.NameContent()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("FunctionArgDetailsPassByReference", "Pass-by-Reference"))
				.ToolTipText(LOCTEXT("FunctionArgDetailsPassByReferenceTooltip", "Pass this paremeter by reference?"))
				.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			.ValueContent()
				[
					SNew(SCheckBox)
					.IsChecked(this, &FBlueprintGraphArgumentLayout::IsRefChecked)
				.OnCheckStateChanged(this, &FBlueprintGraphArgumentLayout::OnRefCheckStateChanged)
				.IsEnabled(!ShouldPinBeReadOnly())
				];
		}
	}
		
	
}

namespace 
{
	static TArray<UK2Node_EditablePinBase*> GatherAllResultNodes(UK2Node_EditablePinBase* TargetNode)
	{
		if (UK2Node_FunctionResult* ResultNode = Cast<UK2Node_FunctionResult>(TargetNode))
		{
			return (TArray<UK2Node_EditablePinBase*>)ResultNode->GetAllResultNodes();
		}
		TArray<UK2Node_EditablePinBase*> Result;
		if (TargetNode)
		{
			Result.Add(TargetNode);
		}
		return Result;
	}

} // namespace

void FBlueprintGraphArgumentLayout::OnRemoveClicked()
{
	TSharedPtr<FUserPinInfo> ParamItem = ParamItemPtr.Pin();
	if (ParamItem.IsValid())
	{
		const FScopedTransaction Transaction( LOCTEXT( "RemoveParam", "Remove Parameter" ) );

		TSharedPtr<FBaseBlueprintGraphActionDetails> GraphActionDetails = GraphActionDetailsPtr.Pin();
		TArray<UK2Node_EditablePinBase*> TargetNodes = GatherAllResultNodes(TargetNode);
		for (UK2Node_EditablePinBase* Node : TargetNodes)
		{
			Node->Modify();
			Node->RemoveUserDefinedPinByName(ParamItem->PinName);

			if (GraphActionDetails.IsValid())
			{
				GraphActionDetails->OnParamsChanged(Node, true);
			}
		}
	}
}

FReply FBlueprintGraphArgumentLayout::OnArgMoveUp()
{
	const int32 ThisParamIndex = TargetNode->UserDefinedPins.Find( ParamItemPtr.Pin() );
	const int32 NewParamIndex = ThisParamIndex-1;
	if (ThisParamIndex != INDEX_NONE && NewParamIndex >= 0)
	{
		const FScopedTransaction Transaction( LOCTEXT("K2_MovePinUp", "Move Pin Up") );
		TArray<UK2Node_EditablePinBase*> TargetNodes = GatherAllResultNodes(TargetNode);
		for (UK2Node_EditablePinBase* Node : TargetNodes)
		{
			Node->Modify();
			Node->UserDefinedPins.Swap(ThisParamIndex, NewParamIndex);

			TSharedPtr<FBaseBlueprintGraphActionDetails> GraphActionDetails = GraphActionDetailsPtr.Pin();
			if (GraphActionDetails.IsValid())
			{
				GraphActionDetails->OnParamsChanged(Node, true);
			}
		}
	}
	return FReply::Handled();
}

FReply FBlueprintGraphArgumentLayout::OnArgMoveDown()
{
	const int32 ThisParamIndex = TargetNode->UserDefinedPins.Find( ParamItemPtr.Pin() );
	const int32 NewParamIndex = ThisParamIndex+1;
	if (ThisParamIndex != INDEX_NONE && NewParamIndex < TargetNode->UserDefinedPins.Num())
	{
		const FScopedTransaction Transaction( LOCTEXT("K2_MovePinDown", "Move Pin Down") );
		TArray<UK2Node_EditablePinBase*> TargetNodes = GatherAllResultNodes(TargetNode);
		for (UK2Node_EditablePinBase* Node : TargetNodes)
		{
			Node->Modify();
			Node->UserDefinedPins.Swap(ThisParamIndex, NewParamIndex);
			
			TSharedPtr<FBaseBlueprintGraphActionDetails> GraphActionDetails = GraphActionDetailsPtr.Pin();
			if (GraphActionDetails.IsValid())
			{
				GraphActionDetails->OnParamsChanged(Node, true);
			}
		}
	}
	return FReply::Handled();
}

bool FBlueprintGraphArgumentLayout::ShouldPinBeReadOnly(bool bIsEditingPinType/* = false*/) const
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	if (TargetNode && ParamItemPtr.IsValid())
	{
		// Right now, we only care that the user is unable to edit the auto-generated "then" pin
		if ((ParamItemPtr.Pin()->PinType.PinCategory == Schema->PC_Exec) && (!TargetNode->CanModifyExecutionWires()))
		{
			return true;
		}
		else
		{
			// Check if pin editing is read only
			return IsPinEditingReadOnly(bIsEditingPinType);
		}
	}
	
	return false;
}

bool FBlueprintGraphArgumentLayout::IsPinEditingReadOnly(bool bIsEditingPinType/* = false*/) const
{
	if(UEdGraph* NodeGraph = TargetNode->GetGraph())
	{
		// Math expression should not be modified directly (except for the pin type), do not let the user tweak the parameters
		if (!bIsEditingPinType && Cast<UK2Node_MathExpression>(NodeGraph->GetOuter()) )
		{
			return true;
		}
	}
	return false;
}

FText FBlueprintGraphArgumentLayout::OnGetArgNameText() const
{
	if (ParamItemPtr.IsValid())
	{
		return FText::FromString(ParamItemPtr.Pin()->PinName);
	}
	return FText();
}

FText FBlueprintGraphArgumentLayout::OnGetArgToolTipText() const
{
	if (ParamItemPtr.IsValid())
	{
		FText PinTypeText = UEdGraphSchema_K2::TypeToText(ParamItemPtr.Pin()->PinType);
		return FText::Format(LOCTEXT("BlueprintArgToolTipText", "Name: {0}\nType: {1}"), FText::FromString(ParamItemPtr.Pin()->PinName), PinTypeText);
	}
	return FText::GetEmpty();
}

void FBlueprintGraphArgumentLayout::OnArgNameChange(const FText& InNewText)
{
	bool bVerified = true;

	FText ErrorMessage;

	if (!ParamItemPtr.IsValid())
	{
		return;
	}

	if (InNewText.IsEmpty())
	{
		ErrorMessage = LOCTEXT("EmptyArgument", "Name cannot be empty!");
		bVerified = false;
	}
	else
	{
		const FString& OldName = ParamItemPtr.Pin()->PinName;
		bVerified = GraphActionDetailsPtr.Pin()->OnVerifyPinRename(TargetNode, OldName, InNewText.ToString(), ErrorMessage);
	}

	if(!bVerified)
	{
		ArgumentNameWidget.Pin()->SetError(ErrorMessage);
	}
	else
	{
		ArgumentNameWidget.Pin()->SetError(FText::GetEmpty());
	}
}

void FBlueprintGraphArgumentLayout::OnArgNameTextCommitted(const FText& NewText, ETextCommit::Type InTextCommit)
{
	if (!NewText.IsEmpty() && TargetNode && ParamItemPtr.IsValid() && GraphActionDetailsPtr.IsValid() && !ShouldPinBeReadOnly())
	{
		const FString OldName = ParamItemPtr.Pin()->PinName;
		const FString& NewName = NewText.ToString();
		if(OldName != NewName)
		{
			GraphActionDetailsPtr.Pin()->OnPinRenamed(TargetNode, OldName, NewName);
		}
	}
}

FEdGraphPinType FBlueprintGraphArgumentLayout::OnGetPinInfo() const
{
	if (ParamItemPtr.IsValid())
	{
		return ParamItemPtr.Pin()->PinType;
	}
	return FEdGraphPinType();
}

UEdGraphPin* FBlueprintGraphArgumentLayout::GetPin() const
{
	if (ParamItemPtr.IsValid() && TargetNode)
	{
		return TargetNode->FindPin(ParamItemPtr.Pin()->PinName, ParamItemPtr.Pin()->DesiredPinDirection);
	}
	return nullptr;
}

ECheckBoxState FBlueprintGraphArgumentLayout::IsRefChecked() const
{
	FEdGraphPinType PinType = OnGetPinInfo();
	return PinType.bIsReference? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void FBlueprintGraphArgumentLayout::OnRefCheckStateChanged(ECheckBoxState InState)
{
	FEdGraphPinType PinType = OnGetPinInfo();
	PinType.bIsReference = (InState == ECheckBoxState::Checked)? true : false;
	// Note: Container types are implicitly passed by reference. For custom event nodes, the reference flag is essentially
	//  treated as being redundant on container inputs, but we also need to implicitly set the 'const' flag to avoid a compiler note.
	PinType.bIsConst = (PinType.IsContainer() || PinType.bIsReference) && TargetNode && TargetNode->IsA<UK2Node_CustomEvent>();
	PinInfoChanged(PinType);
}

void FBlueprintGraphArgumentLayout::PinInfoChanged(const FEdGraphPinType& PinType)
{
	if (ParamItemPtr.IsValid() && FBlueprintEditorUtils::IsPinTypeValid(PinType))
	{
		const FString PinName = ParamItemPtr.Pin()->PinName;
		TSharedPtr<class FBaseBlueprintGraphActionDetails> GraphActionDetailsPinned = GraphActionDetailsPtr.Pin();
		if (GraphActionDetailsPinned.IsValid())
		{
			TSharedPtr<SMyBlueprint> MyBPPinned = GraphActionDetailsPinned->GetMyBlueprint().Pin();
			if (MyBPPinned.IsValid())
			{
				MyBPPinned->GetLastFunctionPinTypeUsed() = PinType;
			}
			if( !ShouldPinBeReadOnly(true) )
			{
				TArray<UK2Node_EditablePinBase*> TargetNodes = GatherAllResultNodes(TargetNode);
				for (UK2Node_EditablePinBase* Node : TargetNodes)
				{
					if (Node)
					{
						TSharedPtr<FUserPinInfo>* UDPinPtr = Node->UserDefinedPins.FindByPredicate([&](TSharedPtr<FUserPinInfo>& UDPin)
						{
							return UDPin.IsValid() && (UDPin->PinName == PinName);
						});
						if (UDPinPtr)
						{
							(*UDPinPtr)->PinType = PinType;

							// Container types are implicitly passed by reference. For custom event nodes, since they are inputs, also implicitly treat them as 'const' so that they don't result in a compiler note.
							(*UDPinPtr)->PinType.bIsConst = PinType.IsContainer() && Node->IsA<UK2Node_CustomEvent>();

							// Reset default value, it probably doesn't match
							(*UDPinPtr)->PinDefaultValue.Reset();
						}
						GraphActionDetailsPinned->OnParamsChanged(Node);
					}
				}
			}
		}
	}
}

void FBlueprintGraphArgumentLayout::OnPrePinInfoChange(const FEdGraphPinType& PinType)
{
	if (!ShouldPinBeReadOnly(true))
	{
		TArray<UK2Node_EditablePinBase*> TargetNodes = GatherAllResultNodes(TargetNode);
		for (UK2Node_EditablePinBase* Node : TargetNodes)
		{
			if (Node)
			{
				Node->Modify();
			}
		}
	}
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void FBlueprintGraphActionDetails::CustomizeDetails( IDetailLayoutBuilder& DetailLayout )
{
	DetailsLayoutPtr = &DetailLayout;
	ObjectsBeingEdited = DetailsLayoutPtr->GetSelectedObjects();

	SetEntryAndResultNodes();

	UK2Node_EditablePinBase* FunctionEntryNode = FunctionEntryNodePtr.Get();
	UK2Node_EditablePinBase* FunctionResultNode = FunctionResultNodePtr.Get();

	// Fill Access specifiers list
	AccessSpecifierLabels.Empty(3);
	AccessSpecifierLabels.Add( MakeShareable( new FAccessSpecifierLabel( AccessSpecifierProperName(FUNC_Public), FUNC_Public )));
	AccessSpecifierLabels.Add( MakeShareable( new FAccessSpecifierLabel( AccessSpecifierProperName(FUNC_Protected), FUNC_Protected )));
	AccessSpecifierLabels.Add( MakeShareable( new FAccessSpecifierLabel( AccessSpecifierProperName(FUNC_Private), FUNC_Private )));

	const bool bHasAGraph = (GetGraph() != NULL);

	if (FunctionEntryNode && FunctionEntryNode->IsEditable())
	{
		IDetailCategoryBuilder& Category = DetailLayout.EditCategory("Graph", LOCTEXT("FunctionDetailsGraph", "Graph"));
		if (bHasAGraph)
		{
			Category.AddCustomRow( LOCTEXT( "DefaultTooltip", "Description" ) )
			.NameContent()
			[
				SNew(STextBlock)
					.Text( LOCTEXT( "DefaultTooltip", "Description" ) )
					.ToolTipText(LOCTEXT("FunctionTooltipTooltip", "Enter a short message describing the purpose and operation of this graph"))
					.Font( IDetailLayoutBuilder::GetDetailFont() )
			]
			.ValueContent()
			[
				SNew(SMultiLineEditableTextBox)
					.Text( this, &FBlueprintGraphActionDetails::OnGetTooltipText )
					.OnTextCommitted( this, &FBlueprintGraphActionDetails::OnTooltipTextCommitted )
					.Font( IDetailLayoutBuilder::GetDetailFont() )
					.ModiferKeyForNewLine(EModifierKey::Shift)
			];

			// Composite graphs are auto-categorized into their parent graph
			if(!GetGraph()->GetOuter()->GetClass()->IsChildOf(UK2Node_Composite::StaticClass()))
			{
				FBlueprintVarActionDetails::PopulateCategories(MyBlueprint.Pin().Get(), CategorySource);
				TSharedPtr<SComboButton> NewComboButton;
				TSharedPtr<SListView<TSharedPtr<FText>>> NewListView;

				const FString DocLink = TEXT("Shared/Editors/BlueprintEditor/GraphDetails");
				TSharedPtr<SToolTip> CategoryTooltip = IDocumentation::Get()->CreateToolTip(LOCTEXT("EditGraphCategoryName_Tooltip", "The category of the graph; editing this will place the graph into another category or create a new one."), NULL, DocLink, TEXT("Category"));

				Category.AddCustomRow( LOCTEXT("CategoryLabel", "Category") )
					.NameContent()
					[
						SNew(STextBlock)
						.Text( LOCTEXT("CategoryLabel", "Category") )
						.ToolTip(CategoryTooltip)
						.Font( IDetailLayoutBuilder::GetDetailFont() )
					]
				.ValueContent()
					[
						SAssignNew(NewComboButton, SComboButton)
						.ContentPadding(FMargin(0,0,5,0))
						.ToolTip(CategoryTooltip)
						.ButtonContent()
						[
							SNew(SBorder)
							.BorderImage( FEditorStyle::GetBrush("NoBorder") )
							.Padding(FMargin(0, 0, 5, 0))
							[
								SNew(SEditableTextBox)
								.Text(this, &FBlueprintGraphActionDetails::OnGetCategoryText)
								.OnTextCommitted(this, &FBlueprintGraphActionDetails::OnCategoryTextCommitted )
								.ToolTip(CategoryTooltip)
								.SelectAllTextWhenFocused(true)
								.RevertTextOnEscape(true)
								.Font( IDetailLayoutBuilder::GetDetailFont() )
							]
						]
						.MenuContent()
							[
								SNew(SVerticalBox)
								+SVerticalBox::Slot()
								.AutoHeight()
								.MaxHeight(400.0f)
								[
									SAssignNew(NewListView, SListView<TSharedPtr<FText>>)
									.ListItemsSource(&CategorySource)
									.OnGenerateRow(this, &FBlueprintGraphActionDetails::MakeCategoryViewWidget)
									.OnSelectionChanged(this, &FBlueprintGraphActionDetails::OnCategorySelectionChanged)
								]
							]
					];

				CategoryComboButton = NewComboButton;
				CategoryListView = NewListView;

				TSharedPtr<SToolTip> KeywordsTooltip = IDocumentation::Get()->CreateToolTip(LOCTEXT("EditKeywords_Tooltip", "Keywords for searching for the function or macro."), NULL, DocLink, TEXT("Keywords"));
				Category.AddCustomRow( LOCTEXT("KeywordsLabel", "Keywords") )
					.NameContent()
					[
						SNew(STextBlock)
						.Text( LOCTEXT("KeywordsLabel", "Keywords") )
						.ToolTip(KeywordsTooltip)
						.Font( IDetailLayoutBuilder::GetDetailFont() )
					]
				.ValueContent()
					[
						SNew(SEditableTextBox)
						.Text(this, &FBlueprintGraphActionDetails::OnGetKeywordsText)
						.OnTextCommitted(this, &FBlueprintGraphActionDetails::OnKeywordsTextCommitted )
						.ToolTip(KeywordsTooltip)
						.RevertTextOnEscape(true)
						.Font( IDetailLayoutBuilder::GetDetailFont() )
					];

				TSharedPtr<SToolTip> CompactNodeTitleTooltip = IDocumentation::Get()->CreateToolTip(LOCTEXT("EditCompactNodeTitle_Tooltip", "Sets the compact node title for calls to this function or macro. Compact node titles convert a node to display as a compact node and are used as a keyword for searching."), NULL, DocLink, TEXT("Compact Node Title"));
				Category.AddCustomRow( LOCTEXT("CompactNodeTitleLabel", "Compact Node Title") )
					.NameContent()
					[
						SNew(STextBlock)
						.Text( LOCTEXT("CompactNodeTitleLabel", "Compact Node Title") )
						.ToolTip(CompactNodeTitleTooltip)
						.Font( IDetailLayoutBuilder::GetDetailFont() )
					]
				.ValueContent()
					[
						SNew(SEditableTextBox)
						.Text(this, &FBlueprintGraphActionDetails::OnGetCompactNodeTitleText)
						.OnTextCommitted(this, &FBlueprintGraphActionDetails::OnCompactNodeTitleTextCommitted )
						.ToolTip(CompactNodeTitleTooltip)
						.RevertTextOnEscape(true)
						.Font( IDetailLayoutBuilder::GetDetailFont() )
					];
			}

			if (IsAccessSpecifierVisible())
			{
				Category.AddCustomRow( LOCTEXT( "AccessSpecifier", "Access Specifier" ) )
				.NameContent()
				[
					SNew(STextBlock)
						.Text( LOCTEXT( "AccessSpecifier", "Access Specifier" ) )
						.Font( IDetailLayoutBuilder::GetDetailFont() )
				]
				.ValueContent()
				[
					SAssignNew(AccessSpecifierComboButton, SComboButton)
					.ContentPadding(0)
					.ButtonContent()
					[
						SNew(STextBlock)
							.Text(this, &FBlueprintGraphActionDetails::GetCurrentAccessSpecifierName)
							.Font( IDetailLayoutBuilder::GetDetailFont() )
					]
					.MenuContent()
					[
						SNew(SListView<TSharedPtr<FAccessSpecifierLabel> >)
							.ListItemsSource( &AccessSpecifierLabels )
							.OnGenerateRow(this, &FBlueprintGraphActionDetails::HandleGenerateRowAccessSpecifier)
							.OnSelectionChanged(this, &FBlueprintGraphActionDetails::OnAccessSpecifierSelected)
					]
				];
			}
			if (GetInstanceColorVisibility())
			{
				Category.AddCustomRow( LOCTEXT( "InstanceColor", "Instance Color" ) )
				.NameContent()
				[
					SNew(STextBlock)
						.Text( LOCTEXT( "InstanceColor", "Instance Color" ) )
						.ToolTipText( LOCTEXT("FunctionColorTooltip", "Choose a title bar color for references of this graph") )
						.Font( IDetailLayoutBuilder::GetDetailFont() )
				]
				.ValueContent()
				[
					SAssignNew( ColorBlock, SColorBlock )
						.Color( this, &FBlueprintGraphActionDetails::GetNodeTitleColor )
						.IgnoreAlpha(true)
						.OnMouseButtonDown( this, &FBlueprintGraphActionDetails::ColorBlock_OnMouseButtonDown )
				];
			}
			if (IsPureFunctionVisible())
			{
				Category.AddCustomRow( LOCTEXT( "FunctionPure_Tooltip", "Pure" ) )
				.NameContent()
				[
					SNew(STextBlock)
						.Text( LOCTEXT( "FunctionPure_Tooltip", "Pure" ) )
						.ToolTipText( LOCTEXT("FunctionIsPure_Tooltip", "Force this to be a pure function?") )
						.Font( IDetailLayoutBuilder::GetDetailFont() )
				]
				.ValueContent()
				[
					SNew( SCheckBox )
						.IsChecked( this, &FBlueprintGraphActionDetails::GetIsPureFunction )
						.OnCheckStateChanged( this, &FBlueprintGraphActionDetails::OnIsPureFunctionModified )
				];
			}
			if (IsConstFunctionVisible())
			{
				Category.AddCustomRow( LOCTEXT( "FunctionConst_Tooltip", "Const" ), true )
				.NameContent()
				[
					SNew(STextBlock)
					.Text( LOCTEXT( "FunctionConst_Tooltip", "Const" ) )
					.ToolTipText( LOCTEXT("FunctionIsConst_Tooltip", "Force this to be a const function?") )
					.Font( IDetailLayoutBuilder::GetDetailFont() )
				]
				.ValueContent()
				[
					SNew( SCheckBox )
					.IsChecked( this, &FBlueprintGraphActionDetails::GetIsConstFunction )
					.OnCheckStateChanged( this, &FBlueprintGraphActionDetails::OnIsConstFunctionModified )
				];
			}
		}

		if (IsCustomEvent())
		{
			/** A collection of static utility callbacks to provide the custom-event details ui with */
			struct LocalCustomEventUtils
			{
				/** Checks to see if the selected node is NOT an override */
				static bool IsNotCustomEventOverride(TWeakObjectPtr<UK2Node_EditablePinBase> SelectedNode)
				{
					bool bIsOverride = false;
					if (SelectedNode.IsValid())
					{
						UK2Node_CustomEvent const* SelectedCustomEvent = Cast<UK2Node_CustomEvent const>(SelectedNode.Get());
						check(SelectedCustomEvent != NULL);

						bIsOverride = SelectedCustomEvent->IsOverride();
					}

					return !bIsOverride;
				}

				/** If the selected node represent an override, this returns tooltip text explaining why you can't alter the replication settings */
				static FText GetDisabledTooltip(TWeakObjectPtr<UK2Node_EditablePinBase> SelectedNode)
				{
					FText ToolTipOut = FText::GetEmpty();
					if (!IsNotCustomEventOverride(SelectedNode))
					{
						ToolTipOut = LOCTEXT("CannotChangeOverrideReplication", "Cannot alter a custom-event's replication settings when it overrides an event declared in a parent.");
					}
					return ToolTipOut;
				}

				/** Determines if the selected node's "Reliable" net setting should be enabled for the user */
				static bool CanSetReliabilityProperty(TWeakObjectPtr<UK2Node_EditablePinBase> SelectedNode)
				{
					bool bIsReliabilitySettingEnabled = false;
					if (IsNotCustomEventOverride(SelectedNode))
					{
						UK2Node_CustomEvent const* SelectedCustomEvent = Cast<UK2Node_CustomEvent const>(SelectedNode.Get());
						check(SelectedCustomEvent != NULL);

						bIsReliabilitySettingEnabled = ((SelectedCustomEvent->GetNetFlags() & FUNC_Net) != 0);
					}
					return bIsReliabilitySettingEnabled;
				}
			};
			FCanExecuteAction CanExecuteDelegate = FCanExecuteAction::CreateStatic(&LocalCustomEventUtils::IsNotCustomEventOverride, FunctionEntryNodePtr);

			FMenuBuilder RepComboMenu( true, NULL );
			RepComboMenu.AddMenuEntry( 	ReplicationSpecifierProperName(0), 
										LOCTEXT("NotReplicatedToolTip", "This event is not replicated to anyone."),
										FSlateIcon(),
										FUIAction(FExecuteAction::CreateStatic( &FBlueprintGraphActionDetails::SetNetFlags, FunctionEntryNodePtr, 0U ), CanExecuteDelegate));
			RepComboMenu.AddMenuEntry(	ReplicationSpecifierProperName(FUNC_NetMulticast), 
										LOCTEXT("MulticastToolTip", "Replicate this event from the server to everyone else. Server executes this event locally too. Only call this from the server."),
										FSlateIcon(),
										FUIAction(FExecuteAction::CreateStatic( &FBlueprintGraphActionDetails::SetNetFlags, FunctionEntryNodePtr, static_cast<uint32>(FUNC_NetMulticast) ), CanExecuteDelegate));
			RepComboMenu.AddMenuEntry(	ReplicationSpecifierProperName(FUNC_NetServer), 
										LOCTEXT("ServerToolTip", "Replicate this event from net owning client to server."),
										FSlateIcon(),
										FUIAction(FExecuteAction::CreateStatic( &FBlueprintGraphActionDetails::SetNetFlags, FunctionEntryNodePtr, static_cast<uint32>(FUNC_NetServer) ), CanExecuteDelegate));
			RepComboMenu.AddMenuEntry(	ReplicationSpecifierProperName(FUNC_NetClient), 
										LOCTEXT("ClientToolTip", "Replicate this event from the server to owning client."),
										FSlateIcon(),
										FUIAction(FExecuteAction::CreateStatic( &FBlueprintGraphActionDetails::SetNetFlags, FunctionEntryNodePtr, static_cast<uint32>(FUNC_NetClient) ), CanExecuteDelegate));

			Category.AddCustomRow( LOCTEXT( "FunctionReplicate", "Replicates" ) )
			.NameContent()
			[
				SNew(STextBlock)
					.Text( LOCTEXT( "FunctionReplicate", "Replicates" ) )
					.ToolTipText( LOCTEXT("FunctionReplicate_Tooltip", "Should this Event be replicated to all clients when called on the server?") )
					.Font( IDetailLayoutBuilder::GetDetailFont() )
			]
			.ValueContent()
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				[
					SNew(SComboButton)
						.ContentPadding(0)
						.IsEnabled_Static(&LocalCustomEventUtils::IsNotCustomEventOverride, FunctionEntryNodePtr)
						.ToolTipText_Static(&LocalCustomEventUtils::GetDisabledTooltip, FunctionEntryNodePtr)
						.ButtonContent()
						[
							SNew(STextBlock)
								.Text(this, &FBlueprintGraphActionDetails::GetCurrentReplicatedEventString)
								.Font( IDetailLayoutBuilder::GetDetailFont() )
						]
						.MenuContent()
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
							[
								SNew(SVerticalBox)
								+SVerticalBox::Slot()
									.AutoHeight()
									.MaxHeight(400.0f)
								[
									RepComboMenu.MakeWidget()
								]
							]
						]
				]

				+SVerticalBox::Slot()
					.AutoHeight()
					.MaxHeight(400.0f)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
						.AutoWidth()
					[
						SNew( SCheckBox )
							.IsChecked( this, &FBlueprintGraphActionDetails::GetIsReliableReplicatedFunction )
							.IsEnabled_Static(&LocalCustomEventUtils::CanSetReliabilityProperty, FunctionEntryNodePtr)
							.ToolTipText_Static(&LocalCustomEventUtils::GetDisabledTooltip, FunctionEntryNodePtr)
							.OnCheckStateChanged( this, &FBlueprintGraphActionDetails::OnIsReliableReplicationFunctionModified )
						[
							SNew(STextBlock)
								.Text( LOCTEXT( "FunctionReplicateReliable", "Reliable" ) )
						]
					]
				]
			];
		}
		const bool bShowCallInEditor = IsCustomEvent() || FBlueprintEditorUtils::IsBlutility( GetBlueprintObj() ) || (FunctionEntryNode && FunctionEntryNode->IsEditable());
		if( bShowCallInEditor )
		{
			Category.AddCustomRow( LOCTEXT( "EditorCallable", "Call In Editor" ) )
			.NameContent()
			[
				SNew(STextBlock)
					.Text( LOCTEXT( "EditorCallable", "Call In Editor" ) )
					.ToolTipText( LOCTEXT("EditorCallable_Tooltip", "Enable this event to be called from within the editor") )
					.Font( IDetailLayoutBuilder::GetDetailFont() )
			]
			.ValueContent()
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew( SCheckBox )
						.IsChecked( this, &FBlueprintGraphActionDetails::GetIsEditorCallableEvent )
						.ToolTipText( LOCTEXT("EditorCallable_Tooltip", "Enable this event to be called from within the editor" ))
						.OnCheckStateChanged( this, &FBlueprintGraphActionDetails::OnEditorCallableEventModified )
					]
				]
			];
		}

		IDetailCategoryBuilder& InputsCategory = DetailLayout.EditCategory("Inputs", LOCTEXT("FunctionDetailsInputs", "Inputs"));
		
		TSharedRef<FBlueprintGraphArgumentGroupLayout> InputArgumentGroup =
			MakeShareable(new FBlueprintGraphArgumentGroupLayout(SharedThis(this), FunctionEntryNode));
		InputsCategory.AddCustomBuilder(InputArgumentGroup);

		TSharedRef<SHorizontalBox> InputsHeaderContentWidget = SNew(SHorizontalBox);
		TWeakPtr<SWidget> WeakInputsHeaderWidget = InputsHeaderContentWidget;
		InputsHeaderContentWidget->AddSlot()
			[
				SNew(SHorizontalBox)
			];
		InputsHeaderContentWidget->AddSlot()
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(FEditorStyle::Get(), "RoundButton")
				.ForegroundColor(FEditorStyle::GetSlateColor("DefaultForeground"))
				.ContentPadding(FMargin(2, 0))
				.OnClicked(this, &FBlueprintGraphActionDetails::OnAddNewInputClicked)
				.Visibility(this, &FBlueprintGraphActionDetails::GetAddNewInputOutputVisibility)
				.HAlign(HAlign_Right)
				.ToolTipText(LOCTEXT("FunctionNewInputArgTooltip", "Create a new input argument"))
				.VAlign(VAlign_Center)
				.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("FunctionNewInputArg")))
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(FMargin(0, 1))
						[
							SNew(SImage)
							.Image(FEditorStyle::GetBrush("Plus"))
						]

					+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.AutoWidth()
						.Padding(FMargin(2, 0, 0, 0))
						[
							SNew(STextBlock)
							.Font(IDetailLayoutBuilder::GetDetailFontBold())
							.Text(LOCTEXT("FunctionNewParameterInputArg", "New Parameter"))
							.Visibility(this, &FBlueprintGraphActionDetails::OnGetSectionTextVisibility, WeakInputsHeaderWidget)
							.ShadowOffset(FVector2D(1, 1))
						]
				]
			];
		InputsCategory.HeaderContent(InputsHeaderContentWidget);

		if (bHasAGraph)
		{
			IDetailCategoryBuilder& OutputsCategory = DetailLayout.EditCategory("Outputs", LOCTEXT("FunctionDetailsOutputs", "Outputs"));
		
			TSharedRef<FBlueprintGraphArgumentGroupLayout> OutputArgumentGroup =
				MakeShareable(new FBlueprintGraphArgumentGroupLayout(SharedThis(this), FunctionResultNode));
			OutputsCategory.AddCustomBuilder(OutputArgumentGroup);
		
			TSharedRef<SHorizontalBox> OutputsHeaderContentWidget = SNew(SHorizontalBox);
			TWeakPtr<SWidget> WeakOutputsHeaderWidget = OutputsHeaderContentWidget;
			OutputsHeaderContentWidget->AddSlot()
				[
					SNew(SHorizontalBox)
				];
			OutputsHeaderContentWidget->AddSlot()
				.AutoWidth()
				[
					SNew(SButton)
					.ButtonStyle(FEditorStyle::Get(), "RoundButton")
					.ForegroundColor(FEditorStyle::GetSlateColor("DefaultForeground"))
					.ContentPadding(FMargin(2, 0))
					.OnClicked(this, &FBlueprintGraphActionDetails::OnAddNewOutputClicked)
					.Visibility(this, &FBlueprintGraphActionDetails::GetAddNewInputOutputVisibility)
					.HAlign(HAlign_Right)
					.ToolTipText(LOCTEXT("FunctionNewOutputArgTooltip", "Create a new output argument"))
					.VAlign(VAlign_Center)
					.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("FunctionNewOutputArg")))
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(FMargin(0, 1))
					[
						SNew(SImage)
							.Image(FEditorStyle::GetBrush("Plus"))
					]

				+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.AutoWidth()
					.Padding(FMargin(2, 0, 0, 0))
					[
						SNew(STextBlock)
						.Font(IDetailLayoutBuilder::GetDetailFontBold())
						.Text(LOCTEXT("FunctionNewOutputArg", "New Parameter"))
						.Visibility(this, &FBlueprintGraphActionDetails::OnGetSectionTextVisibility, WeakOutputsHeaderWidget)
						.ShadowOffset(FVector2D(1, 1))
					]
				]
			];
			OutputsCategory.HeaderContent(OutputsHeaderContentWidget);
		}
	}
	else
	{
		if (bHasAGraph)
		{
			IDetailCategoryBuilder& Category = DetailLayout.EditCategory("Graph", LOCTEXT("FunctionDetailsGraph", "Graph"));
			Category.AddCustomRow( FText::GetEmpty() )
			[
				SNew(STextBlock)
				.Text( LOCTEXT("GraphPresentButNotEditable", "Graph is not editable.") )
			];
		}
	}

	if (MyBlueprint.IsValid())
	{
		TWeakPtr<FBlueprintEditor> BlueprintEditor = MyBlueprint.Pin()->GetBlueprintEditor();
		if (BlueprintEditor.IsValid())
		{
			BlueprintEditorRefreshDelegateHandle = BlueprintEditor.Pin()->OnRefresh().AddSP(this, &FBlueprintGraphActionDetails::OnPostEditorRefresh);
		}
	}
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

TSharedRef<ITableRow> FBlueprintGraphActionDetails::OnGenerateReplicationComboWidget( TSharedPtr<FReplicationSpecifierLabel> InNetFlag, const TSharedRef<STableViewBase>& OwnerTable )
{
	return
		SNew(STableRow< TSharedPtr<FString> >, OwnerTable)
		[
			SNew( STextBlock )
			.Text( InNetFlag.IsValid() ? InNetFlag.Get()->LocalizedName : FText::GetEmpty() )
			.ToolTipText( InNetFlag.IsValid() ? InNetFlag.Get()->LocalizedToolTip : FText::GetEmpty() )
		];
}

void FBlueprintGraphActionDetails::SetNetFlags( TWeakObjectPtr<UK2Node_EditablePinBase> FunctionEntryNode, uint32 NetFlags)
{
	if( FunctionEntryNode.IsValid() )
	{
		const int32 FlagsToSet = NetFlags ? FUNC_Net|NetFlags : 0;
		const int32 FlagsToClear = FUNC_Net|FUNC_NetMulticast|FUNC_NetServer|FUNC_NetClient;
		// Clear all net flags before setting
		if( FlagsToSet != FlagsToClear )
		{
			const FScopedTransaction Transaction( LOCTEXT("GraphSetNetFlags", "Change Replication") );
			FunctionEntryNode->Modify();

			bool bBlueprintModified = false;

			if (UK2Node_FunctionEntry* TypedEntryNode = Cast<UK2Node_FunctionEntry>(FunctionEntryNode.Get()))
			{
				int32 ExtraFlags = TypedEntryNode->GetExtraFlags();
				ExtraFlags &= ~FlagsToClear;
				ExtraFlags |= FlagsToSet;
				TypedEntryNode->SetExtraFlags(ExtraFlags);
				bBlueprintModified = true;
			}
			if (UK2Node_CustomEvent * CustomEventNode = Cast<UK2Node_CustomEvent>(FunctionEntryNode.Get()))
			{
				CustomEventNode->FunctionFlags &= ~FlagsToClear;
				CustomEventNode->FunctionFlags |= FlagsToSet;
				bBlueprintModified = true;
			}

			if( bBlueprintModified )
			{
				FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified( FunctionEntryNode->GetBlueprint() );
			}
		}
	}
}

FText FBlueprintGraphActionDetails::GetCurrentReplicatedEventString() const
{
	const UK2Node_EditablePinBase * FunctionEntryNode = FunctionEntryNodePtr.Get();
	const UK2Node_CustomEvent* CustomEvent = Cast<const UK2Node_CustomEvent>(FunctionEntryNode);

	uint32 const ReplicatedNetMask = (FUNC_NetMulticast | FUNC_NetServer | FUNC_NetClient);

	FText ReplicationText;

	if(CustomEvent)
	{
		uint32 NetFlags = CustomEvent->FunctionFlags & ReplicatedNetMask;
		if (CustomEvent->IsOverride())
		{
			UFunction* SuperFunction = FindField<UFunction>(CustomEvent->GetBlueprint()->ParentClass, CustomEvent->CustomFunctionName);
			check(SuperFunction != NULL);

			NetFlags = SuperFunction->FunctionFlags & ReplicatedNetMask;
		}
		ReplicationText = ReplicationSpecifierProperName(NetFlags);
	}
	return ReplicationText;
}

bool FBaseBlueprintGraphActionDetails::AttemptToCreateResultNode()
{
	if (!FunctionResultNodePtr.IsValid())
	{
		FunctionResultNodePtr = FBlueprintEditorUtils::FindOrCreateFunctionResultNode(FunctionEntryNodePtr.Get());
	}
	return FunctionResultNodePtr.IsValid();
}

FBaseBlueprintGraphActionDetails::~FBaseBlueprintGraphActionDetails()
{
	if (BlueprintEditorRefreshDelegateHandle.IsValid() && MyBlueprint.IsValid())
	{
		// Remove the callback delegate we registered for
		TWeakPtr<FBlueprintEditor> BlueprintEditor = MyBlueprint.Pin()->GetBlueprintEditor();
		if (BlueprintEditor.IsValid())
		{
			BlueprintEditor.Pin()->OnRefresh().Remove(BlueprintEditorRefreshDelegateHandle);
		}
	}
}

void FBaseBlueprintGraphActionDetails::OnPostEditorRefresh()
{
	/** Blueprint changed, need to refresh inputs in case pin UI changed */
	RegenerateInputsChildrenDelegate.ExecuteIfBound();
	RegenerateOutputsChildrenDelegate.ExecuteIfBound();
}

void FBaseBlueprintGraphActionDetails::SetRefreshDelegate(FSimpleDelegate RefreshDelegate, bool bForInputs)
{
	((bForInputs) ? RegenerateInputsChildrenDelegate : RegenerateOutputsChildrenDelegate) = RefreshDelegate;
}

ECheckBoxState FBlueprintGraphActionDetails::GetIsEditorCallableEvent() const
{
	ECheckBoxState Result = ECheckBoxState::Unchecked;

	if( FunctionEntryNodePtr.IsValid() )
	{
		if( UK2Node_CustomEvent* CustomEventNode = Cast<UK2Node_CustomEvent>(FunctionEntryNodePtr.Get()))
		{
			if( CustomEventNode->bCallInEditor  )
			{
				Result = ECheckBoxState::Checked;
			}
		}
		else if( UK2Node_FunctionEntry* EntryPoint = Cast<UK2Node_FunctionEntry>(FunctionEntryNodePtr.Get()) )
		{
			if( EntryPoint->MetaData.bCallInEditor )
			{
				Result = ECheckBoxState::Checked;
			}
		}
	}
	return Result;
}

void FBlueprintGraphActionDetails::OnEditorCallableEventModified( const ECheckBoxState NewCheckedState ) const
{
	if( FunctionEntryNodePtr.IsValid() )
	{
		const bool bCallInEditor = NewCheckedState == ECheckBoxState::Checked;
		const FText TransactionType = bCallInEditor ?	LOCTEXT( "DisableCallInEditor", "Disable Call In Editor " ) : 
														LOCTEXT( "EnableCallInEditor", "Enable Call In Editor" );

		if( UK2Node_CustomEvent* CustomEventNode = Cast<UK2Node_CustomEvent>(FunctionEntryNodePtr.Get()) )
		{
			if( UBlueprint* Blueprint = FunctionEntryNodePtr->GetBlueprint() )
			{
				const FScopedTransaction Transaction( TransactionType );
				CustomEventNode->bCallInEditor = bCallInEditor;
				FBlueprintEditorUtils::MarkBlueprintAsModified( CustomEventNode->GetBlueprint() );
			}
		}
		else if( UK2Node_FunctionEntry* EntryPoint = Cast<UK2Node_FunctionEntry>(FunctionEntryNodePtr.Get()) )
		{
			const FScopedTransaction Transaction( TransactionType );
			EntryPoint->MetaData.bCallInEditor = bCallInEditor;
			FBlueprintEditorUtils::MarkBlueprintAsModified( EntryPoint->GetBlueprint() );
		}
	}
}

UMulticastDelegateProperty* FBlueprintDelegateActionDetails::GetDelegateProperty() const
{
	if (MyBlueprint.IsValid())
	{
		if (const FEdGraphSchemaAction_K2Delegate* DelegateVar = MyBlueprint.Pin()->SelectionAsDelegate())
		{
			return DelegateVar->GetDelegateProperty();
		}
	}
	return NULL;
}

bool FBlueprintDelegateActionDetails::IsBlueprintProperty() const
{
	const UMulticastDelegateProperty* Property = GetDelegateProperty();
	const UBlueprint* Blueprint = GetBlueprintObj();
	if(Property && Blueprint)
	{
		return (Property->GetOuter() == Blueprint->SkeletonGeneratedClass);
	}

	return false;
}

void FBlueprintDelegateActionDetails::SetEntryNode()
{
	if (UEdGraph* NewTargetGraph = GetGraph())
	{
		TArray<UK2Node_FunctionEntry*> EntryNodes;
		NewTargetGraph->GetNodesOfClass(EntryNodes);

		if ((EntryNodes.Num() > 0) && EntryNodes[0]->IsEditable())
		{
			FunctionEntryNodePtr = EntryNodes[0];
		}
	}
}

UEdGraph* FBlueprintDelegateActionDetails::GetGraph() const
{
	if (MyBlueprint.IsValid())
	{
		if (const FEdGraphSchemaAction_K2Delegate* DelegateVar = MyBlueprint.Pin()->SelectionAsDelegate())
		{
			return DelegateVar->EdGraph;
		}
	}
	return NULL;
}

FText FBlueprintDelegateActionDetails::OnGetTooltipText() const
{
	if (UMulticastDelegateProperty* DelegateProperty = GetDelegateProperty())
	{
		FString Result;
		FBlueprintEditorUtils::GetBlueprintVariableMetaData(GetBlueprintObj(), DelegateProperty->GetFName(), NULL, TEXT("tooltip"), Result);
		return FText::FromString(Result);
	}
	return FText();
}

void FBlueprintDelegateActionDetails::OnTooltipTextCommitted(const FText& NewText, ETextCommit::Type InTextCommit)
{
	if (UMulticastDelegateProperty* DelegateProperty = GetDelegateProperty())
	{
		FBlueprintEditorUtils::SetBlueprintVariableMetaData(GetBlueprintObj(), DelegateProperty->GetFName(), NULL, TEXT("tooltip"), NewText.ToString() );
	}
}

FText FBlueprintDelegateActionDetails::OnGetCategoryText() const
{
	if (UMulticastDelegateProperty* DelegateProperty = GetDelegateProperty())
	{
		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
		FName DelegateName = DelegateProperty->GetFName();
		FText Category = FBlueprintEditorUtils::GetBlueprintVariableCategory(GetBlueprintObj(), DelegateName, NULL);

		// Older blueprints will have their name as the default category
		if( Category.EqualTo(FText::FromString(GetBlueprintObj()->GetName())) || Category.EqualTo(K2Schema->VR_DefaultCategory) )
		{
			return LOCTEXT("DefaultCategory", "Default");
		}
		else
		{
			return Category;
		}
	}
	return FText();
}

void FBlueprintDelegateActionDetails::OnCategoryTextCommitted(const FText& NewText, ETextCommit::Type InTextCommit)
{
	if (InTextCommit == ETextCommit::OnEnter || InTextCommit == ETextCommit::OnUserMovedFocus)
	{
		if (UMulticastDelegateProperty* DelegateProperty = GetDelegateProperty())
		{
			// Remove excess whitespace and prevent categories with just spaces
			FText CategoryName = FText::TrimPrecedingAndTrailing(NewText);
			
			FBlueprintEditorUtils::SetBlueprintVariableCategory(GetBlueprintObj(), DelegateProperty->GetFName(), NULL, CategoryName);
			check(MyBlueprint.IsValid());
			FBlueprintVarActionDetails::PopulateCategories(MyBlueprint.Pin().Get(), CategorySource);
			MyBlueprint.Pin()->ExpandCategory(CategoryName);
		}
	}
}

TSharedRef< ITableRow > FBlueprintDelegateActionDetails::MakeCategoryViewWidget( TSharedPtr<FText> Item, const TSharedRef< STableViewBase >& OwnerTable )
{
	return SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
		[
			SNew(STextBlock) .Text(*Item.Get())
		];
}

void FBlueprintDelegateActionDetails::OnCategorySelectionChanged( TSharedPtr<FText> ProposedSelection, ESelectInfo::Type /*SelectInfo*/ )
{
	UMulticastDelegateProperty* DelegateProperty = GetDelegateProperty();
	if (DelegateProperty && ProposedSelection.IsValid())
	{
		FText NewCategory = *ProposedSelection.Get();

		FBlueprintEditorUtils::SetBlueprintVariableCategory(GetBlueprintObj(), DelegateProperty->GetFName(), NULL, NewCategory);
		CategoryListView.Pin()->ClearSelection();
		CategoryComboButton.Pin()->SetIsOpen(false);
		MyBlueprint.Pin()->ExpandCategory(NewCategory);
	}
}

void FBlueprintDelegateActionDetails::CustomizeDetails( IDetailLayoutBuilder& DetailLayout )
{
	DetailsLayoutPtr = &DetailLayout;
	ObjectsBeingEdited = DetailsLayoutPtr->GetSelectedObjects();

	SetEntryNode();

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	const FSlateFontInfo DetailFontInfo = IDetailLayoutBuilder::GetDetailFont();

	{
		IDetailCategoryBuilder& Category = DetailLayout.EditCategory("Delegate", LOCTEXT("DelegateDetailsCategory", "Delegate"));
		Category.AddCustomRow( LOCTEXT("VariableToolTipLabel", "Tooltip") )
		.NameContent()
		[
			SNew(STextBlock)
				.Text( LOCTEXT("VariableToolTipLabel", "Tooltip") )
				.Font( DetailFontInfo )
		]
		.ValueContent()
		[
			SNew(SEditableTextBox)
				.Text( this, &FBlueprintDelegateActionDetails::OnGetTooltipText )
				.OnTextCommitted( this, &FBlueprintDelegateActionDetails::OnTooltipTextCommitted)
				.Font( DetailFontInfo )
		];

		FBlueprintVarActionDetails::PopulateCategories(MyBlueprint.Pin().Get(), CategorySource);
		TSharedPtr<SComboButton> NewComboButton;
		TSharedPtr<SListView<TSharedPtr<FText>>> NewListView;

		Category.AddCustomRow( LOCTEXT("CategoryLabel", "Category") )
		.NameContent()
		[
			SNew(STextBlock)
				.Text( LOCTEXT("CategoryLabel", "Category") )
				.Font( DetailFontInfo )
		]
		.ValueContent()
		[
			SAssignNew(NewComboButton, SComboButton)
			.ContentPadding(FMargin(0,0,5,0))
			.IsEnabled(this, &FBlueprintDelegateActionDetails::IsBlueprintProperty)
			.ButtonContent()
			[
				SNew(SBorder)
				.BorderImage( FEditorStyle::GetBrush("NoBorder") )
				.Padding(FMargin(0, 0, 5, 0))
				[
					SNew(SEditableTextBox)
						.Text(this, &FBlueprintDelegateActionDetails::OnGetCategoryText)
						.OnTextCommitted(this, &FBlueprintDelegateActionDetails::OnCategoryTextCommitted)
						.SelectAllTextWhenFocused(true)
						.RevertTextOnEscape(true)
						.Font( DetailFontInfo )
				]
			]
			.MenuContent()
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				.MaxHeight(400.0f)
				[
					SAssignNew(NewListView, SListView<TSharedPtr<FText>>)
						.ListItemsSource(&CategorySource)
						.OnGenerateRow(this, &FBlueprintDelegateActionDetails::MakeCategoryViewWidget)
						.OnSelectionChanged(this, &FBlueprintDelegateActionDetails::OnCategorySelectionChanged)
				]
			]
		];

		CategoryComboButton = NewComboButton;
		CategoryListView = NewListView;
	}

	if (UK2Node_EditablePinBase* FunctionEntryNode = FunctionEntryNodePtr.Get())
	{
		IDetailCategoryBuilder& InputsCategory = DetailLayout.EditCategory("DelegateInputs", LOCTEXT("DelegateDetailsInputs", "Inputs"));
		TSharedRef<FBlueprintGraphArgumentGroupLayout> InputArgumentGroup = MakeShareable(new FBlueprintGraphArgumentGroupLayout(SharedThis(this), FunctionEntryNode));
		InputsCategory.AddCustomBuilder(InputArgumentGroup);

		InputsCategory.AddCustomRow( LOCTEXT("FunctionNewInputArg", "New") )
		[
			SNew(SBox)
			.HAlign(HAlign_Right)
			[
				SNew(SButton)
				.Text(LOCTEXT("FunctionNewInputArg", "New"))
				.OnClicked(this, &FBlueprintDelegateActionDetails::OnAddNewInputClicked)
			]
		];

		CollectAvailibleSignatures();

		InputsCategory.AddCustomRow( LOCTEXT("CopySignatureFrom", "Copy signature from") )
		.NameContent()
		[
			SNew(STextBlock)
				.Text(LOCTEXT("CopySignatureFrom", "Copy signature from"))
				.Font( DetailFontInfo )
		]
		.ValueContent()
		[
			SAssignNew(CopySignatureComboButton, STextComboBox)
				.OptionsSource(&FunctionsToCopySignatureFrom)
				.OnSelectionChanged(this, &FBlueprintDelegateActionDetails::OnFunctionSelected)
		];
	}
}

void FBlueprintDelegateActionDetails::CollectAvailibleSignatures()
{
	FunctionsToCopySignatureFrom.Empty();
	if (UMulticastDelegateProperty* Property = GetDelegateProperty())
	{
		if (UClass* ScopeClass = Cast<UClass>(Property->GetOuterUField()))
		{
			for(TFieldIterator<UFunction> It(ScopeClass, EFieldIteratorFlags::IncludeSuper); It; ++It)
			{
				UFunction* Func = *It;
				if (UEdGraphSchema_K2::FunctionCanBeUsedInDelegate(Func) && !UEdGraphSchema_K2::HasFunctionAnyOutputParameter(Func))
				{
					TSharedPtr<FString> ItemData = MakeShareable(new FString(Func->GetName()));
					FunctionsToCopySignatureFrom.Add(ItemData);
				}
			}

			// Sort the function list
			FunctionsToCopySignatureFrom.Sort([](const TSharedPtr<FString>& ElementA, const TSharedPtr<FString>& ElementB) -> bool
			{
				return *ElementA < *ElementB;
			});
		}
	}
}

void FBlueprintDelegateActionDetails::OnFunctionSelected(TSharedPtr<FString> FunctionName, ESelectInfo::Type SelectInfo)
{
	UK2Node_EditablePinBase* FunctionEntryNode = FunctionEntryNodePtr.Get();
	UMulticastDelegateProperty* Property = GetDelegateProperty();
	UClass* ScopeClass = Property ? Cast<UClass>(Property->GetOuterUField()) : NULL;
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	if (FunctionEntryNode && FunctionName.IsValid() && ScopeClass)
	{
		const FName Name( *(*FunctionName) );
		if (UFunction* NewSignature = ScopeClass->FindFunctionByName(Name))
		{
			const FScopedTransaction Transaction(LOCTEXT("CopySignature", "Copy Signature"));

			while (FunctionEntryNode->UserDefinedPins.Num())
			{
				TSharedPtr<FUserPinInfo> Pin = FunctionEntryNode->UserDefinedPins[0];
				FunctionEntryNode->RemoveUserDefinedPin(Pin);
			}

			for (TFieldIterator<UProperty> PropIt(NewSignature); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
			{
				UProperty* FuncParam = *PropIt;
				FEdGraphPinType TypeOut;
				Schema->ConvertPropertyToPinType(FuncParam, TypeOut);
				UEdGraphPin* EdGraphPin = FunctionEntryNode->CreateUserDefinedPin(FuncParam->GetName(), TypeOut, EGPD_Output);
				ensure(EdGraphPin);
			}

			OnParamsChanged(FunctionEntryNode);
		}
	}
}

void FBaseBlueprintGraphActionDetails::OnParamsChanged(UK2Node_EditablePinBase* TargetNode, bool bForceRefresh)
{
	UEdGraph* Graph = GetGraph();

	// TargetNode can be null, if we just removed the result node because there are no more out params
	if (TargetNode)
	{
		RegenerateInputsChildrenDelegate.ExecuteIfBound();
		RegenerateOutputsChildrenDelegate.ExecuteIfBound();

		// Reconstruct the entry/exit definition and recompile the blueprint to make sure the signature has changed before any fixups
		{
			TGuardValue<ESaveOrphanPinMode> GuardSaveMode(TargetNode->OrphanedPinSaveMode, ESaveOrphanPinMode::SaveNone);
			TargetNode->ReconstructNode();
		}

		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

		K2Schema->HandleParameterDefaultValueChanged(TargetNode);
	}
}

struct FPinRenamedHelper : public FBasePinChangeHelper
{
	TSet<UBlueprint*> ModifiedBlueprints;
	TSet<UK2Node*> NodesToRename;

	virtual void EditMacroInstance(UK2Node_MacroInstance* MacroInstance, UBlueprint* Blueprint) override
	{
		NodesToRename.Add(MacroInstance);
		if (Blueprint)
		{
			ModifiedBlueprints.Add(Blueprint);
		}
	}

	virtual void EditCallSite(UK2Node_CallFunction* CallSite, UBlueprint* Blueprint) override
	{
		NodesToRename.Add(CallSite);
		if (Blueprint)
		{
			ModifiedBlueprints.Add(Blueprint);
		}
	}
};

bool FBaseBlueprintGraphActionDetails::OnVerifyPinRename(UK2Node_EditablePinBase* InTargetNode, const FString& InOldName, const FString& InNewName, FText& OutErrorMessage)
{
	// If the name is unchanged, allow the name
	if(InOldName == InNewName)
	{
		return true;
	}

	if( InNewName.Len() > NAME_SIZE )
	{
		OutErrorMessage = FText::Format( LOCTEXT("PinNameTooLong", "The name you entered is too long. Names must be less than {0} characters"), FText::AsNumber( NAME_SIZE ) );
		return false;
	}

	if (InTargetNode)
	{
		// Check if the name conflicts with any of the other internal UFunction's property names (local variables and parameters).
		const UFunction* FoundFunction = FFunctionFromNodeHelper::FunctionFromNode(InTargetNode);
		const UProperty* ExistingProperty = FindField<const UProperty>(FoundFunction, *InNewName);
		if (ExistingProperty)
		{
			OutErrorMessage = LOCTEXT("ConflictsWithProperty", "Conflicts with another another local variable or function parameter!");
			return false;
		}
	}
	return true;
}

bool FBaseBlueprintGraphActionDetails::OnPinRenamed(UK2Node_EditablePinBase* TargetNode, const FString& OldName, const FString& NewName)
{
	// Before changing the name, verify the name
	FText ErrorMessage;
	if(!OnVerifyPinRename(TargetNode, OldName, NewName, ErrorMessage))
	{
		return false;
	}

	UEdGraph* Graph = GetGraph();

	if (TargetNode)
	{
		FPinRenamedHelper PinRenamedHelper;

		const FScopedTransaction Transaction(LOCTEXT("RenameParam", "Rename Parameter"));

		TArray<UK2Node_EditablePinBase*> TerminalNodes = GatherAllResultNodes(FunctionResultNodePtr.Get());
		if (UK2Node_EditablePinBase* EntryNode = FunctionEntryNodePtr.Get())
		{
			TerminalNodes.Add(EntryNode);
		}
		for (UK2Node_EditablePinBase* TerminalNode : TerminalNodes)
		{
			TerminalNode->Modify();
			PinRenamedHelper.NodesToRename.Add(TerminalNode);
		}

		PinRenamedHelper.ModifiedBlueprints.Add(GetBlueprintObj());

		// GATHER 
		PinRenamedHelper.Broadcast(GetBlueprintObj(), TargetNode, Graph);

		// TEST
		for (UK2Node* NodeToRename : PinRenamedHelper.NodesToRename)
		{
			if (ERenamePinResult::ERenamePinResult_NameCollision == NodeToRename->RenameUserDefinedPin(OldName, NewName, true))
			{
				return false;
			}
		}

		// UPDATE
		for (UK2Node* NodeToRename : PinRenamedHelper.NodesToRename)
		{
			NodeToRename->RenameUserDefinedPin(OldName, NewName, false);
		}

		for (UK2Node_EditablePinBase* TerminalNode : TerminalNodes)
		{
			TSharedPtr<FUserPinInfo>* UDPinPtr = TerminalNode->UserDefinedPins.FindByPredicate([&](TSharedPtr<FUserPinInfo>& Pin)
			{
				return Pin.IsValid() && (Pin->PinName == OldName);
			});
			if (UDPinPtr)
			{
				(*UDPinPtr)->PinName = NewName;
			}
		}
	}
	return true;
}

void FBlueprintGraphActionDetails::SetEntryAndResultNodes()
{
	// Clear the entry and exit nodes to the graph
	FunctionEntryNodePtr = NULL;
	FunctionResultNodePtr = NULL;

	if (UEdGraph* NewTargetGraph = GetGraph())
	{
		FBlueprintEditorUtils::GetEntryAndResultNodes(NewTargetGraph, FunctionEntryNodePtr, FunctionResultNodePtr);
	}
	else if (UK2Node_EditablePinBase* Node = GetEditableNode())
	{
		FunctionEntryNodePtr = Node;
	}
}

UEdGraph* FBaseBlueprintGraphActionDetails::GetGraph() const
{
	check(ObjectsBeingEdited.Num() > 0);

	if (ObjectsBeingEdited.Num() == 1)
	{
		UObject* const Object = ObjectsBeingEdited[0].Get();
		if (!Object)
		{
			return nullptr;
		}

		if (Object->IsA<UK2Node_Composite>())
		{
			return Cast<UK2Node_Composite>(Object)->BoundGraph;
		}
		else if (!Object->IsA<UK2Node_MacroInstance>() && (Object->IsA<UK2Node_Tunnel>() || Object->IsA<UK2Node_FunctionTerminator>()))
		{
			return Cast<UK2Node>(Object)->GetGraph();
		}
		else if (UK2Node_CallFunction* FunctionCall = Cast<UK2Node_CallFunction>(Object))
		{
			return FindObject<UEdGraph>(FunctionCall->GetBlueprint(), *(FunctionCall->FunctionReference.GetMemberName().ToString()));
		}
		else if (Object->IsA<UEdGraph>())
		{
			return Cast<UEdGraph>(Object);
		}
	}

	return nullptr;
}

UK2Node_EditablePinBase* FBlueprintGraphActionDetails::GetEditableNode() const
{
	check(ObjectsBeingEdited.Num() > 0);

	if (ObjectsBeingEdited.Num() == 1)
	{
		UObject* const Object = ObjectsBeingEdited[0].Get();
		if (!Object)
		{
			return nullptr;
		}

		if (Object->IsA<UK2Node_CustomEvent>())
		{
			return Cast<UK2Node_CustomEvent>(Object);
		}
	}

	return nullptr;
}

UFunction* FBlueprintGraphActionDetails::FindFunction() const
{
	UEdGraph* Graph = GetGraph();
	if(Graph)
	{
		if(UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph))
		{
			UClass* Class = Blueprint->SkeletonGeneratedClass;

			for (TFieldIterator<UFunction> FunctionIt(Class, EFieldIteratorFlags::IncludeSuper); FunctionIt; ++FunctionIt)
			{
				UFunction* Function = *FunctionIt;
				if (Function->GetName() == Graph->GetName())
				{
					return Function;
				}
			}
		}
	}
	return NULL;
}

FKismetUserDeclaredFunctionMetadata* FBlueprintGraphActionDetails::GetMetadataBlock() const
{
	UK2Node_EditablePinBase* FunctionEntryNode = FunctionEntryNodePtr.Get();
	if (UK2Node_FunctionEntry* TypedEntryNode = Cast<UK2Node_FunctionEntry>(FunctionEntryNode))
	{
		return &(TypedEntryNode->MetaData);
	}
	else if (UK2Node_Tunnel* TunnelNode = ExactCast<UK2Node_Tunnel>(FunctionEntryNode))
	{
		// Must be exactly a tunnel, not a macro instance
		return &(TunnelNode->MetaData);
	}

	return NULL;
}

FText FBlueprintGraphActionDetails::OnGetTooltipText() const
{
	if (FKismetUserDeclaredFunctionMetadata* Metadata = GetMetadataBlock())
	{
		return Metadata->ToolTip;
	}
	else
	{
		return LOCTEXT( "NoTooltip", "(None)" );
	}
}

void FBlueprintGraphActionDetails::OnTooltipTextCommitted(const FText& NewText, ETextCommit::Type InTextCommit)
{
	if (FKismetUserDeclaredFunctionMetadata* Metadata = GetMetadataBlock())
	{
		Metadata->ToolTip = NewText;
		if (UFunction* Function = FindFunction())
		{
			Function->Modify();
			Function->SetMetaData(FBlueprintMetadata::MD_Tooltip, *NewText.ToString());
		}
	}
}

FText FBlueprintGraphActionDetails::OnGetCategoryText() const
{
	if (FKismetUserDeclaredFunctionMetadata* Metadata = GetMetadataBlock())
	{
		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
		if( Metadata->Category.IsEmpty() || Metadata->Category.EqualTo(K2Schema->VR_DefaultCategory) )
		{
			return LOCTEXT("DefaultCategory", "Default");
		}
		
		return Metadata->Category;
	}
	else
	{
		return LOCTEXT( "NoFunctionCategory", "(None)" );
	}
}

void FBlueprintGraphActionDetails::OnCategoryTextCommitted(const FText& NewText, ETextCommit::Type InTextCommit)
{
	if (InTextCommit == ETextCommit::OnEnter || InTextCommit == ETextCommit::OnUserMovedFocus)
	{
		// Remove excess whitespace and prevent categories with just spaces
		FText CategoryName = FText::TrimPrecedingAndTrailing(NewText);

		FBlueprintEditorUtils::SetBlueprintFunctionOrMacroCategory(GetGraph(), CategoryName);
		MyBlueprint.Pin()->Refresh();
	}
}

void FBlueprintGraphActionDetails::OnCategorySelectionChanged( TSharedPtr<FText> ProposedSelection, ESelectInfo::Type /*SelectInfo*/ )
{
	if(ProposedSelection.IsValid())
	{
		if (FKismetUserDeclaredFunctionMetadata* Metadata = GetMetadataBlock())
		{
			FBlueprintEditorUtils::SetBlueprintFunctionOrMacroCategory(GetGraph(), *ProposedSelection.Get());
			MyBlueprint.Pin()->Refresh();

			CategoryListView.Pin()->ClearSelection();
			CategoryComboButton.Pin()->SetIsOpen(false);
			MyBlueprint.Pin()->ExpandCategory(*ProposedSelection.Get());
		}
	}
}

TSharedRef< ITableRow > FBlueprintGraphActionDetails::MakeCategoryViewWidget( TSharedPtr<FText> Item, const TSharedRef< STableViewBase >& OwnerTable )
{
	return SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
		[
			SNew(STextBlock) .Text(*Item.Get())
		];
}

FText FBlueprintGraphActionDetails::OnGetKeywordsText() const
{
	FText ResultKeywords;
	if (FKismetUserDeclaredFunctionMetadata* Metadata = GetMetadataBlock())
	{
		ResultKeywords = Metadata->Keywords;
	}
	return ResultKeywords;
}

void FBlueprintGraphActionDetails::OnKeywordsTextCommitted(const FText& NewText, ETextCommit::Type InTextCommit)
{
	if (InTextCommit == ETextCommit::OnEnter || InTextCommit == ETextCommit::OnUserMovedFocus)
	{
		if (FKismetUserDeclaredFunctionMetadata* Metadata = GetMetadataBlock())
		{
			// Remove excess whitespace and prevent keywords with just spaces
			FText Keywords = FText::TrimPrecedingAndTrailing(NewText);

			if (!Keywords.EqualTo(Metadata->Keywords))
			{
				Metadata->Keywords = Keywords;

				if (UFunction* Function = FindFunction())
				{
					Function->Modify();
					Function->SetMetaData(FBlueprintMetadata::MD_FunctionKeywords, *Keywords.ToString());
				}
				OnParamsChanged(GetFunctionEntryNode().Get(), true);
				FBlueprintEditorUtils::MarkBlueprintAsModified(GetBlueprintObj());
			}
		}
	}
}

FText FBlueprintGraphActionDetails::OnGetCompactNodeTitleText() const
{
	FText ResultKeywords;
	if (FKismetUserDeclaredFunctionMetadata* Metadata = GetMetadataBlock())
	{
		ResultKeywords = Metadata->CompactNodeTitle;
	}
	return ResultKeywords;
}

void FBlueprintGraphActionDetails::OnCompactNodeTitleTextCommitted(const FText& NewText, ETextCommit::Type InTextCommit)
{
	if (InTextCommit == ETextCommit::OnEnter || InTextCommit == ETextCommit::OnUserMovedFocus)
	{
		if (FKismetUserDeclaredFunctionMetadata* Metadata = GetMetadataBlock())
		{
			// Remove excess whitespace and prevent CompactNodeTitle with just spaces
			FText CompactNodeTitle = FText::TrimPrecedingAndTrailing(NewText);

			if (!CompactNodeTitle.EqualTo(Metadata->CompactNodeTitle))
			{
				Metadata->CompactNodeTitle = CompactNodeTitle;

				if (UFunction* Function = FindFunction())
				{
					Function->Modify();

					if (CompactNodeTitle.IsEmpty())
					{
						// Remove the metadata from the function, empty listings will still display the node as Compact
						Function->RemoveMetaData(FBlueprintMetadata::MD_FunctionKeywords);
					}
					else
					{
						Function->SetMetaData(FBlueprintMetadata::MD_CompactNodeTitle, *CompactNodeTitle.ToString());
					}
				}
				OnParamsChanged(GetFunctionEntryNode().Get(), true);
				FBlueprintEditorUtils::MarkBlueprintAsModified(GetBlueprintObj());
			}
		}
	}
}

FText FBlueprintGraphActionDetails::AccessSpecifierProperName( uint32 AccessSpecifierFlag ) const
{
	switch(AccessSpecifierFlag)
	{
	case FUNC_Public:
		return LOCTEXT( "Public", "Public" );
	case FUNC_Private:
		return LOCTEXT( "Private", "Private" );
	case FUNC_Protected:
		return LOCTEXT( "Protected", "Protected" );
	case 0:
		return LOCTEXT( "Unknown", "Unknown" ); // Default?
	}
	return LOCTEXT( "Error", "Error" );
}

FText FBlueprintGraphActionDetails::ReplicationSpecifierProperName( uint32 ReplicationSpecifierFlag ) const
{
	switch(ReplicationSpecifierFlag)
	{
	case FUNC_NetMulticast:
		return LOCTEXT( "MulticastDropDown", "Multicast" );
	case FUNC_NetServer:
		return LOCTEXT( "ServerDropDown", "Run on Server" );
	case FUNC_NetClient:
		return LOCTEXT( "ClientDropDown", "Run on owning Client" );
	case 0:
		return LOCTEXT( "NotReplicatedDropDown", "Not Replicated" );
	}
	return LOCTEXT( "Error", "Error" );
}

TSharedRef<ITableRow> FBlueprintGraphActionDetails::HandleGenerateRowAccessSpecifier( TSharedPtr<FAccessSpecifierLabel> SpecifierName, const TSharedRef<STableViewBase>& OwnerTable )
{
	return SNew(STableRow< TSharedPtr<FAccessSpecifierLabel> >, OwnerTable)
		.Content()
		[
			SNew( STextBlock ) 
				.Text( SpecifierName.IsValid() ? SpecifierName->LocalizedName : FText::GetEmpty() )
		];
}

FText FBlueprintGraphActionDetails::GetCurrentAccessSpecifierName() const
{
	uint32 AccessSpecifierFlag = 0;
	UK2Node_EditablePinBase * FunctionEntryNode = FunctionEntryNodePtr.Get();
	if(UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(FunctionEntryNode))
	{
		AccessSpecifierFlag = FUNC_AccessSpecifiers & EntryNode->GetFunctionFlags();
	}
	else if(UK2Node_CustomEvent* CustomEventNode = Cast<UK2Node_CustomEvent>(FunctionEntryNode))
	{
		AccessSpecifierFlag = FUNC_AccessSpecifiers & CustomEventNode->FunctionFlags;
	}
	return AccessSpecifierProperName( AccessSpecifierFlag );
}

bool FBlueprintGraphActionDetails::IsAccessSpecifierVisible() const
{
	bool bSupportedType = false;
	bool bIsEditable = false;
	UK2Node_EditablePinBase * FunctionEntryNode = FunctionEntryNodePtr.Get();
	if(FunctionEntryNode)
	{
		UBlueprint* Blueprint = FunctionEntryNode->GetBlueprint();
		const bool bIsInterface = FBlueprintEditorUtils::IsInterfaceBlueprint(Blueprint);

		bSupportedType = !bIsInterface && (FunctionEntryNode->IsA<UK2Node_FunctionEntry>() || FunctionEntryNode->IsA<UK2Node_Event>());
		bIsEditable = FunctionEntryNode->IsEditable();
	}
	return bSupportedType && bIsEditable;
}

void FBlueprintGraphActionDetails::OnAccessSpecifierSelected( TSharedPtr<FAccessSpecifierLabel> SpecifierName, ESelectInfo::Type SelectInfo )
{
	if(AccessSpecifierComboButton.IsValid())
	{
		AccessSpecifierComboButton->SetIsOpen(false);
	}

	UK2Node_EditablePinBase* FunctionEntryNode = FunctionEntryNodePtr.Get();
	if(FunctionEntryNode && SpecifierName.IsValid())
	{
		const FScopedTransaction Transaction( LOCTEXT( "ChangeAccessSpecifier", "Change Access Specifier" ) );

		FunctionEntryNode->Modify();
		UFunction* Function = FindFunction();
		if(Function)
		{
			Function->Modify();
		}

		const EFunctionFlags ClearAccessSpecifierMask = ~FUNC_AccessSpecifiers;
		if(UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(FunctionEntryNode))
		{
			int32 ExtraFlags = EntryNode->GetExtraFlags();
			ExtraFlags &= ClearAccessSpecifierMask;
			ExtraFlags |= SpecifierName->SpecifierFlag;
			EntryNode->SetExtraFlags(ExtraFlags);
		}
		else if(UK2Node_Event* EventNode = Cast<UK2Node_Event>(FunctionEntryNode))
		{
			EventNode->FunctionFlags &= ClearAccessSpecifierMask;
			EventNode->FunctionFlags |= SpecifierName->SpecifierFlag;
		}
		if(Function)
		{
			Function->FunctionFlags &= ClearAccessSpecifierMask;
			Function->FunctionFlags |= SpecifierName->SpecifierFlag;
		}

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprintObj());
	}
}

bool FBlueprintGraphActionDetails::GetInstanceColorVisibility() const
{
	// Hide the color editor if it's a top level function declaration.
	// Show it if we're editing a collapsed graph or macro
	UEdGraph* Graph = GetGraph();
	if (Graph)
	{
		const UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
		if (Blueprint)
		{
			const bool bIsTopLevelFunctionGraph = Blueprint->FunctionGraphs.Contains(Graph);
			const bool bIsTopLevelMacroGraph = Blueprint->MacroGraphs.Contains(Graph);
			const bool bIsMacroGraph = Blueprint->BlueprintType == BPTYPE_MacroLibrary;
			return ((bIsMacroGraph || bIsTopLevelMacroGraph) || !bIsTopLevelFunctionGraph);
		}

	}
	
	return false;
}

FLinearColor FBlueprintGraphActionDetails::GetNodeTitleColor() const
{
	if (FKismetUserDeclaredFunctionMetadata* Metadata = GetMetadataBlock())
	{
		return Metadata->InstanceTitleColor;
	}
	else
	{
		return FLinearColor::White;
	}
}

FReply FBlueprintGraphActionDetails::ColorBlock_OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (FKismetUserDeclaredFunctionMetadata* Metadata = GetMetadataBlock())
		{
			TArray<FLinearColor*> LinearColorArray;
			LinearColorArray.Add(&(Metadata->InstanceTitleColor));

			FColorPickerArgs PickerArgs;
			PickerArgs.bIsModal = true;
			PickerArgs.ParentWidget = ColorBlock;
			PickerArgs.DisplayGamma = TAttribute<float>::Create( TAttribute<float>::FGetter::CreateUObject(GEngine, &UEngine::GetDisplayGamma) );
			PickerArgs.LinearColorArray = &LinearColorArray;

			OpenColorPicker(PickerArgs);
		}

		return FReply::Handled();
	}
	else
	{
		return FReply::Unhandled();
	}
}

bool FBlueprintGraphActionDetails::IsCustomEvent() const
{
	return (NULL != Cast<UK2Node_CustomEvent>(FunctionEntryNodePtr.Get()));
}

void FBlueprintGraphActionDetails::OnIsReliableReplicationFunctionModified(const ECheckBoxState NewCheckedState)
{
	UK2Node_EditablePinBase * FunctionEntryNode = FunctionEntryNodePtr.Get();
	UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(FunctionEntryNode);
	if( CustomEvent )
	{
		if (NewCheckedState == ECheckBoxState::Checked)
		{
			if (UK2Node_FunctionEntry* TypedEntryNode = Cast<UK2Node_FunctionEntry>(FunctionEntryNode))
			{
				TypedEntryNode->AddExtraFlags(FUNC_NetReliable);
			}
			if (UK2Node_CustomEvent * CustomEventNode = Cast<UK2Node_CustomEvent>(FunctionEntryNode))
			{
				CustomEventNode->FunctionFlags |= FUNC_NetReliable;
			}
		}
		else
		{
			if (UK2Node_FunctionEntry* TypedEntryNode = Cast<UK2Node_FunctionEntry>(FunctionEntryNode))
			{
				TypedEntryNode->ClearExtraFlags(FUNC_NetReliable);
			}
			if (UK2Node_CustomEvent * CustomEventNode = Cast<UK2Node_CustomEvent>(FunctionEntryNode))
			{
				CustomEventNode->FunctionFlags &= ~FUNC_NetReliable;
			}
		}

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprintObj());
	}
}

ECheckBoxState FBlueprintGraphActionDetails::GetIsReliableReplicatedFunction() const
{
	const UK2Node_EditablePinBase * FunctionEntryNode = FunctionEntryNodePtr.Get();
	const UK2Node_CustomEvent* CustomEvent = Cast<const UK2Node_CustomEvent>(FunctionEntryNode);
	if(!CustomEvent)
	{
		return ECheckBoxState::Undetermined;
	}

	uint32 const NetReliableMask = (FUNC_Net | FUNC_NetReliable);
	if ((CustomEvent->GetNetFlags() & NetReliableMask) == NetReliableMask)
	{
		return ECheckBoxState::Checked;
	}
	
	return ECheckBoxState::Unchecked;
}

bool FBlueprintGraphActionDetails::IsPureFunctionVisible() const
{
	bool bSupportedType = false;
	bool bIsEditable = false;
	UK2Node_EditablePinBase * FunctionEntryNode = FunctionEntryNodePtr.Get();
	if(FunctionEntryNode)
	{
		UBlueprint* Blueprint = FunctionEntryNode->GetBlueprint();
		const bool bIsInterface = FBlueprintEditorUtils::IsInterfaceBlueprint(Blueprint);

		bSupportedType = !bIsInterface && FunctionEntryNode->IsA<UK2Node_FunctionEntry>();
		bIsEditable = FunctionEntryNode->IsEditable();
	}
	return bSupportedType && bIsEditable;
}

void FBlueprintGraphActionDetails::OnIsPureFunctionModified( const ECheckBoxState NewCheckedState )
{
	UK2Node_EditablePinBase * FunctionEntryNode = FunctionEntryNodePtr.Get();
	UFunction* Function = FindFunction();
	UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(FunctionEntryNode);
	if (EntryNode && Function)
	{
		const FScopedTransaction Transaction( LOCTEXT( "ChangePure", "Change Pure" ) );
		EntryNode->Modify();
		Function->Modify();

		//set flags on function entry node also
		Function->FunctionFlags ^= FUNC_BlueprintPure;
		EntryNode->SetExtraFlags(EntryNode->GetExtraFlags() ^ FUNC_BlueprintPure);
		OnParamsChanged(FunctionEntryNode);
	}
}

ECheckBoxState FBlueprintGraphActionDetails::GetIsPureFunction() const
{
	UK2Node_EditablePinBase* FunctionEntryNode = FunctionEntryNodePtr.Get();
	UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(FunctionEntryNode);
	if(!EntryNode)
	{
		return ECheckBoxState::Undetermined;
	}
	return (EntryNode->GetFunctionFlags() & FUNC_BlueprintPure) ? ECheckBoxState::Checked :  ECheckBoxState::Unchecked;
}

bool FBlueprintGraphActionDetails::IsConstFunctionVisible() const
{
	bool bSupportedType = false;
	bool bIsEditable = false;
	UK2Node_EditablePinBase * FunctionEntryNode = FunctionEntryNodePtr.Get();
	if(FunctionEntryNode)
	{
		UBlueprint* Blueprint = FunctionEntryNode->GetBlueprint();

		bSupportedType = FunctionEntryNode->IsA<UK2Node_FunctionEntry>();
		bIsEditable = FunctionEntryNode->IsEditable();
	}
	return bSupportedType && bIsEditable;
}

void FBlueprintGraphActionDetails::OnIsConstFunctionModified( const ECheckBoxState NewCheckedState )
{
	UK2Node_EditablePinBase * FunctionEntryNode = FunctionEntryNodePtr.Get();
	UFunction* Function = FindFunction();
	UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(FunctionEntryNode);
	if(EntryNode && Function)
	{
		const FScopedTransaction Transaction( LOCTEXT( "ChangeConst", "Change Const" ) );
		EntryNode->Modify();
		Function->Modify();

		//set flags on function entry node also
		Function->FunctionFlags ^= FUNC_Const;
		EntryNode->SetExtraFlags(EntryNode->GetExtraFlags() ^ FUNC_Const);
		OnParamsChanged(FunctionEntryNode);
	}
}

ECheckBoxState FBlueprintGraphActionDetails::GetIsConstFunction() const
{
	UK2Node_EditablePinBase* FunctionEntryNode = FunctionEntryNodePtr.Get();
	UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(FunctionEntryNode);
	if(!EntryNode)
	{
		return ECheckBoxState::Undetermined;
	}
	return (EntryNode->GetFunctionFlags() & FUNC_Const) ? ECheckBoxState::Checked :  ECheckBoxState::Unchecked;
}

FReply FBaseBlueprintGraphActionDetails::OnAddNewInputClicked()
{
	UK2Node_EditablePinBase * FunctionEntryNode = FunctionEntryNodePtr.Get();

	if( FunctionEntryNode )
	{
		FScopedTransaction Transaction( LOCTEXT( "AddInParam", "Add In Parameter" ) );
		FunctionEntryNode->Modify();

		FEdGraphPinType PinType = MyBlueprint.Pin()->GetLastFunctionPinTypeUsed();

		// Make sure that if this is an exec node we are allowed one.
		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
		if ((PinType.PinCategory == Schema->PC_Exec) && (!FunctionEntryNode->CanModifyExecutionWires()))
		{
			MyBlueprint.Pin()->ResetLastPinType();
			PinType = MyBlueprint.Pin()->GetLastFunctionPinTypeUsed();
		}
		FString NewPinName = TEXT("NewParam");
		if (FunctionEntryNode->CreateUserDefinedPin(NewPinName, PinType, EGPD_Output))
		{
			OnParamsChanged(FunctionEntryNode, true);
		}
		else
		{
			Transaction.Cancel();
		}
	}

	return FReply::Handled();
}

EVisibility FBlueprintGraphActionDetails::GetAddNewInputOutputVisibility() const
{
	UK2Node_EditablePinBase* FunctionEntryNode = FunctionEntryNodePtr.Get();
	if (FunctionEntryNodePtr.IsValid())
	{
		if(UEdGraph* Graph = FunctionEntryNode->GetGraph())
		{
			// Math expression graphs are read only, do not allow adding or removing of pins
			if(Cast<UK2Node_MathExpression>(Graph->GetOuter()))
			{
				return EVisibility::Collapsed;
			}
		}
	}
	return EVisibility::Visible;
}

EVisibility FBlueprintGraphActionDetails::OnGetSectionTextVisibility(TWeakPtr<SWidget> RowWidget) const
{
	bool ShowText = RowWidget.Pin()->IsHovered();

	// If the row is currently hovered, or a menu is being displayed for a button, keep the button expanded.
	if (ShowText)
	{
		return EVisibility::SelfHitTestInvisible;
	}
	else
	{
		return EVisibility::Collapsed;
	}
}

FReply FBlueprintGraphActionDetails::OnAddNewOutputClicked()
{
	FScopedTransaction Transaction( LOCTEXT( "AddOutParam", "Add Out Parameter" ) );
	
	GetBlueprintObj()->Modify();
	GetGraph()->Modify();
	UK2Node_EditablePinBase* EntryPin = FunctionEntryNodePtr.Get();	
	EntryPin->Modify();
	for (int32 iPin = 0; iPin < EntryPin->Pins.Num() ; iPin++)
	{
		EntryPin->Pins[iPin]->Modify();
	}
	
	UK2Node_EditablePinBase* PreviousResultNode = FunctionResultNodePtr.Get();

	AttemptToCreateResultNode();

	UK2Node_EditablePinBase* FunctionResultNode = FunctionResultNodePtr.Get();
	if( FunctionResultNode )
	{
		FEdGraphPinType PinType = MyBlueprint.Pin()->GetLastFunctionPinTypeUsed();
		PinType.bIsReference = false;
		// Make sure that if this is an exec node we are allowed one.
		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
		if ((PinType.PinCategory == Schema->PC_Exec) && (!FunctionResultNode->CanModifyExecutionWires()))
		{
			MyBlueprint.Pin()->ResetLastPinType();
			PinType = MyBlueprint.Pin()->GetLastFunctionPinTypeUsed();
		}

		const FString NewPinName = FunctionResultNode->CreateUniquePinName(TEXT("NewParam"));
		TArray<UK2Node_EditablePinBase*> TargetNodes = GatherAllResultNodes(FunctionResultNode);
		bool bAllChanged = TargetNodes.Num() > 0;
		for (UK2Node_EditablePinBase* Node : TargetNodes)
		{
			Node->Modify();
			UEdGraphPin* NewPin = Node->CreateUserDefinedPin(NewPinName, PinType, EGPD_Input, false);
			bAllChanged &= (nullptr != NewPin);

			if (bAllChanged)
			{
				OnParamsChanged(Node, true);
			}
			else
			{
				break;
			}
		}
		if (!bAllChanged)
		{
			Transaction.Cancel();
		}

		if (!PreviousResultNode)
		{
			DetailsLayoutPtr->ForceRefreshDetails();
		}
	}
	else
	{
		Transaction.Cancel();
	}

	return FReply::Handled();
}



void FBlueprintInterfaceLayout::GenerateHeaderRowContent( FDetailWidgetRow& NodeRow )
{
	NodeRow
	[
		SNew(STextBlock)
			.Text( bShowsInheritedInterfaces ?
			LOCTEXT("BlueprintInheritedInterfaceTitle", "Inherited Interfaces") :
			LOCTEXT("BlueprintImplementedInterfaceTitle", "Implemented Interfaces") )
			.Font( IDetailLayoutBuilder::GetDetailFont() )
	];
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void FBlueprintInterfaceLayout::GenerateChildContent( IDetailChildrenBuilder& ChildrenBuilder )
{
	UBlueprint* Blueprint = GlobalOptionsDetailsPtr.Pin()->GetBlueprintObj();
	check(Blueprint);

	TArray<FInterfaceName> Interfaces;

	if (!bShowsInheritedInterfaces)
	{
		// Generate a list of interfaces already implemented
		for (const FBPInterfaceDescription& ImplementedInterface : Blueprint->ImplementedInterfaces)
		{
			if (const TSubclassOf<UInterface> Interface = ImplementedInterface.Interface)
			{
				Interfaces.AddUnique(FInterfaceName(Interface->GetFName(), Interface->GetDisplayNameText()));
			}
		}
	}
	else
	{
		// Generate a list of interfaces implemented by classes this blueprint inherited from
		UClass* BlueprintParent = Blueprint->ParentClass;
		while (BlueprintParent)
		{
			for (TArray<FImplementedInterface>::TIterator It(BlueprintParent->Interfaces); It; ++It)
			{
				FImplementedInterface& CurrentInterface = *It;
				if( CurrentInterface.Class )
				{
					Interfaces.Add(FInterfaceName(CurrentInterface.Class->GetFName(), CurrentInterface.Class->GetDisplayNameText()));
				}
			}
			BlueprintParent = BlueprintParent->GetSuperClass();
		}
	}

	for (int32 i = 0; i < Interfaces.Num(); ++i)
	{
		TSharedPtr<SHorizontalBox> Box;
		ChildrenBuilder.AddCustomRow( LOCTEXT( "BlueprintInterfaceValue", "Interface Value" ) )
		[
			SAssignNew(Box, SHorizontalBox)
			+SHorizontalBox::Slot()
			[
				SNew(STextBlock)
					.Text(Interfaces[i].DisplayText)
					.Font( IDetailLayoutBuilder::GetDetailFont() )
			]
		];

		// See if we need to add a button for opening this interface
		if (!bShowsInheritedInterfaces)
		{
			UBlueprintGeneratedClass* Class = Cast<UBlueprintGeneratedClass>(*Blueprint->ImplementedInterfaces[i].Interface);
			if (Class)
			{
				TWeakObjectPtr<UObject> Asset = Class->ClassGeneratedBy;
		
				const TSharedRef<SWidget> BrowseButton = PropertyCustomizationHelpers::MakeBrowseButton(FSimpleDelegate::CreateSP(this, &FBlueprintInterfaceLayout::OnBrowseToInterface, Asset));
				BrowseButton->SetToolTipText( LOCTEXT("BlueprintInterfaceBrowseTooltip", "Opens this interface") );

				Box->AddSlot()
				.AutoWidth()
				.Padding(2.0f, 0.0f)
				[
					BrowseButton
				];
			}
		}

		if (!bShowsInheritedInterfaces)
		{
			Box->AddSlot()
			.AutoWidth()
			[
				PropertyCustomizationHelpers::MakeClearButton(FSimpleDelegate::CreateSP(this, &FBlueprintInterfaceLayout::OnRemoveInterface, Interfaces[i]))
			];
		}
	}

	// Add message if no interfaces are being used
	if (Interfaces.Num() == 0)
	{
		ChildrenBuilder.AddCustomRow(LOCTEXT("BlueprintInterfaceValue", "Interface Value"))
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NoBlueprintInterface", "No Interfaces"))
			.Font(IDetailLayoutBuilder::GetDetailFontItalic())
		];
	}

	if (!bShowsInheritedInterfaces)
	{
		ChildrenBuilder.AddCustomRow( LOCTEXT( "BlueprintAddInterface", "Add Interface" ) )
		[
			SNew(SBox)
			.HAlign(HAlign_Right)
			[
				SAssignNew(AddInterfaceComboButton, SComboButton)
				.ButtonContent()
				[
					SNew(STextBlock)
						.Text(LOCTEXT("BlueprintAddInterfaceButton", "Add"))
				]
				.OnGetMenuContent(this, &FBlueprintInterfaceLayout::OnGetAddInterfaceMenuContent)
			]
		];
	}
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void FBlueprintInterfaceLayout::OnBrowseToInterface(TWeakObjectPtr<UObject> Asset)
{
	if (Asset.IsValid())
	{
		FAssetEditorManager::Get().OpenEditorForAsset(Asset.Get());
	}
}

void FBlueprintInterfaceLayout::OnRemoveInterface(FInterfaceName InterfaceName)
{
	UBlueprint* Blueprint = GlobalOptionsDetailsPtr.Pin()->GetBlueprintObj();
	check(Blueprint);

	const EAppReturnType::Type DialogReturn = FMessageDialog::Open(EAppMsgType::YesNoCancel, NSLOCTEXT("UnrealEd", "TransferInterfaceFunctionsToBlueprint", "Would you like to transfer the interface functions to be part of your blueprint?"));

	if (DialogReturn == EAppReturnType::Cancel)
	{
		// We canceled!
		return;
	}
	const FName InterfaceFName = InterfaceName.Name;

	// Close all graphs that are about to be removed
	TArray<UEdGraph*> Graphs;
	FBlueprintEditorUtils::GetInterfaceGraphs(Blueprint, InterfaceFName, Graphs);
	for( TArray<UEdGraph*>::TIterator GraphIt(Graphs); GraphIt; ++GraphIt )
	{
		GlobalOptionsDetailsPtr.Pin()->GetBlueprintEditorPtr().Pin()->CloseDocumentTab(*GraphIt);
	}

	// Do the work of actually removing the interface
	FBlueprintEditorUtils::RemoveInterface(Blueprint, InterfaceFName, DialogReturn == EAppReturnType::Yes);

	RegenerateChildrenDelegate.ExecuteIfBound();

	OnRefreshInDetailsView();
}

void FBlueprintInterfaceLayout::OnClassPicked(UClass* PickedClass)
{
	if (AddInterfaceComboButton.IsValid())
	{
		AddInterfaceComboButton->SetIsOpen(false);
	}

	UBlueprint* Blueprint = GlobalOptionsDetailsPtr.Pin()->GetBlueprintObj();
	check(Blueprint);

	FBlueprintEditorUtils::ImplementNewInterface( Blueprint, PickedClass->GetFName() );

	RegenerateChildrenDelegate.ExecuteIfBound();

	OnRefreshInDetailsView();
}

TSharedRef<SWidget> FBlueprintInterfaceLayout::OnGetAddInterfaceMenuContent()
{
	UBlueprint* Blueprint = GlobalOptionsDetailsPtr.Pin()->GetBlueprintObj();

	TArray<UBlueprint*> Blueprints;
	Blueprints.Add(Blueprint);
	TSharedRef<SWidget> ClassPicker = FBlueprintEditorUtils::ConstructBlueprintInterfaceClassPicker(Blueprints, FOnClassPicked::CreateSP(this, &FBlueprintInterfaceLayout::OnClassPicked));
	return
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("Menu.Background"))
		[
			// Achieving fixed width by nesting items within a fixed width box.
			SNew(SBox)
			.WidthOverride(350.0f)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.MaxHeight(400.0f)
				.AutoHeight()
				[
					ClassPicker
				]
			]
		];
}

void FBlueprintInterfaceLayout::OnRefreshInDetailsView()
{
	TSharedPtr<SKismetInspector> Inspector = GlobalOptionsDetailsPtr.Pin()->GetBlueprintEditorPtr().Pin()->GetInspector();
	UBlueprint* Blueprint = GlobalOptionsDetailsPtr.Pin()->GetBlueprintObj();
	check(Blueprint);

	// Show details for the Blueprint instance we're editing
	Inspector->ShowDetailsForSingleObject(Blueprint);
}

UBlueprint* FBlueprintGlobalOptionsDetails::GetBlueprintObj() const
{
	if(BlueprintEditorPtr.IsValid())
	{
		return BlueprintEditorPtr.Pin()->GetBlueprintObj();
	}

	return NULL;
}

FText FBlueprintGlobalOptionsDetails::GetParentClassName() const
{
	const UBlueprint* Blueprint = GetBlueprintObj();
	const UClass* ParentClass = Blueprint ? Blueprint->ParentClass : NULL;
	return ParentClass ? ParentClass->GetDisplayNameText() : FText::FromName(NAME_None);
}

bool FBlueprintGlobalOptionsDetails::CanReparent() const
{
	return BlueprintEditorPtr.IsValid() && BlueprintEditorPtr.Pin()->ReparentBlueprint_IsVisible();
}

TSharedRef<SWidget> FBlueprintGlobalOptionsDetails::GetParentClassMenuContent()
{
	TArray<UBlueprint*> Blueprints;
	Blueprints.Add(GetBlueprintObj());
	TSharedRef<SWidget> ClassPicker = FBlueprintEditorUtils::ConstructBlueprintParentClassPicker(Blueprints, FOnClassPicked::CreateSP(this, &FBlueprintGlobalOptionsDetails::OnClassPicked));

	return
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("Menu.Background"))
		[
			// Achieving fixed width by nesting items within a fixed width box.
			SNew(SBox)
			.WidthOverride(350.0f)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.MaxHeight(400.0f)
				.AutoHeight()
				[
					ClassPicker
				]
			]
		];
}

void FBlueprintGlobalOptionsDetails::OnClassPicked(UClass* PickedClass)
{
	ParentClassComboButton->SetIsOpen(false);
	if(BlueprintEditorPtr.IsValid())
	{
		BlueprintEditorPtr.Pin()->ReparentBlueprint_NewParentChosen(PickedClass);
	}

	check(BlueprintEditorPtr.IsValid());
	TSharedPtr<SKismetInspector> Inspector = BlueprintEditorPtr.Pin()->GetInspector();
	// Show details for the Blueprint instance we're editing
	Inspector->ShowDetailsForSingleObject(GetBlueprintObj());
}

bool FBlueprintGlobalOptionsDetails::CanDeprecateBlueprint() const
{
	if (UBlueprint* Blueprint = GetBlueprintObj())
	{
		// If the parent is deprecated, we cannot modify deprecation on this Blueprint
		if (Blueprint->ParentClass && Blueprint->ParentClass->HasAnyClassFlags(CLASS_Deprecated))
		{
			return false;
		}

		return true;
	}

	return false;
}

void FBlueprintGlobalOptionsDetails::OnDeprecateBlueprint(ECheckBoxState InCheckState)
{
	GetBlueprintObj()->bDeprecate = InCheckState == ECheckBoxState::Checked? true : false;
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprintObj());
}

ECheckBoxState FBlueprintGlobalOptionsDetails::IsDeprecatedBlueprint() const
{
	if (UBlueprint* Blueprint = GetBlueprintObj())
	{
		return Blueprint->bDeprecate ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	return ECheckBoxState::Unchecked;
}

FText FBlueprintGlobalOptionsDetails::GetDeprecatedTooltip() const
{
	if(CanDeprecateBlueprint())
	{
		return LOCTEXT("DeprecateBlueprintTooltip", "Deprecate the Blueprint and all child Blueprints to make it no longer placeable in the World nor child classes created from it.");
	}
	
	return LOCTEXT("DisabledDeprecateBlueprintTooltip", "This Blueprint is deprecated because of a parent, it is not possible to remove deprecation from it!");
}

/** Shared tooltip text for both the label and the value field */
static FText GetNativizeLabelTooltip()
{
	return LOCTEXT("NativizeTooltip", "When exclusive nativization is enabled, then this asset will be nativized. NOTE: All super classes must be also nativized.");
}

void FBlueprintGlobalOptionsDetails::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	const UBlueprint* Blueprint = GetBlueprintObj();
	if(Blueprint != NULL)
	{
		// Hide any properties that aren't included in the "Option" category
		for (TFieldIterator<UProperty> PropertyIt(Blueprint->GetClass(), EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
		{
			UProperty* Property = *PropertyIt;
			const FString& Category = Property->GetMetaData(TEXT("Category"));

			if (Category != TEXT("BlueprintOptions") && Category != TEXT("ClassOptions"))
			{
				DetailLayout.HideProperty(DetailLayout.GetProperty(Property->GetFName()));
			}
		}

		// Display the parent class and set up the menu for reparenting
		IDetailCategoryBuilder& Category = DetailLayout.EditCategory("ClassOptions", LOCTEXT("ClassOptions", "Class Options"));
		Category.AddCustomRow( LOCTEXT("ClassOptions", "Class Options") )
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("BlueprintDetails_ParentClass", "Parent Class"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		[
			SAssignNew(ParentClassComboButton, SComboButton)
			.IsEnabled(this, &FBlueprintGlobalOptionsDetails::CanReparent)
			.OnGetMenuContent(this, &FBlueprintGlobalOptionsDetails::GetParentClassMenuContent)
			.ButtonContent()
			[
				SNew(STextBlock)
				.Text(this, &FBlueprintGlobalOptionsDetails::GetParentClassName)
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
		];
		
		const bool bIsInterfaceBP = FBlueprintEditorUtils::IsInterfaceBlueprint(Blueprint);
		const bool bIsMacroLibrary = Blueprint->BlueprintType == BPTYPE_MacroLibrary;
		const bool bIsLevelScriptBP = FBlueprintEditorUtils::IsLevelScriptBlueprint(Blueprint);
		const bool bIsFunctionLibrary = Blueprint->BlueprintType == BPTYPE_FunctionLibrary;
		const bool bSupportsInterfaces = !bIsInterfaceBP && !bIsMacroLibrary && !bIsFunctionLibrary;

		if (bSupportsInterfaces)
		{
			// Interface details customization
			IDetailCategoryBuilder& InterfacesCategory = DetailLayout.EditCategory("Interfaces", LOCTEXT("BlueprintInterfacesDetailsCategory", "Interfaces"));
		
			TSharedRef<FBlueprintInterfaceLayout> InterfaceLayout = MakeShareable(new FBlueprintInterfaceLayout(SharedThis(this), false));
			InterfacesCategory.AddCustomBuilder(InterfaceLayout);
		
			TSharedRef<FBlueprintInterfaceLayout> InheritedInterfaceLayout = MakeShareable(new FBlueprintInterfaceLayout(SharedThis(this), true));
			InterfacesCategory.AddCustomBuilder(InheritedInterfaceLayout);
		}

		// Hide the bDeprecate, we override the functionality.
		static FName DeprecatePropName(TEXT("bDeprecate"));
		DetailLayout.HideProperty(DetailLayout.GetProperty(DeprecatePropName));

		// Hide the experimental CompileMode setting (if not enabled)
		const UBlueprintEditorSettings* EditorSettings = GetDefault<UBlueprintEditorSettings>();
		if (EditorSettings && !EditorSettings->bAllowExplicitImpureNodeDisabling)
		{
			static FName CompileModePropertyName(TEXT("CompileMode"));
			DetailLayout.HideProperty(DetailLayout.GetProperty(CompileModePropertyName));
		}

		// Hide 'run on drag' for LevelBP
		if (bIsLevelScriptBP)
		{
			static FName RunOnDragPropName(TEXT("bRunConstructionScriptOnDrag"));
			DetailLayout.HideProperty(DetailLayout.GetProperty(RunOnDragPropName));
		}
		else
		{
			// Only display the ability to deprecate a Blueprint on non-level Blueprints.
			Category.AddCustomRow( LOCTEXT("DeprecateLabel", "Deprecate"), true )
				.NameContent()
				[
					SNew(STextBlock)
					.Text( LOCTEXT("DeprecateLabel", "Deprecate") )
					.ToolTipText( this, &FBlueprintGlobalOptionsDetails::GetDeprecatedTooltip )
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
				.ValueContent()
				[
					SNew(SCheckBox)
					.IsEnabled( this, &FBlueprintGlobalOptionsDetails::CanDeprecateBlueprint )
					.IsChecked( this, &FBlueprintGlobalOptionsDetails::IsDeprecatedBlueprint )
					.OnCheckStateChanged( this, &FBlueprintGlobalOptionsDetails::OnDeprecateBlueprint )
					.ToolTipText( this, &FBlueprintGlobalOptionsDetails::GetDeprecatedTooltip )
				];
		}

		IDetailCategoryBuilder& PkgCategory = DetailLayout.EditCategory("Packaging", LOCTEXT("BlueprintPackagingCategory", "Packaging"));
		PkgCategory.AddCustomRow(LOCTEXT("NativizeLabel", "Nativize"))
			.NameContent()
			[
				SNew(STextBlock)
					.Text(LOCTEXT("NativizeLabel", "Nativize"))
					.ToolTipText_Static(&GetNativizeLabelTooltip)
					.Font(IDetailLayoutBuilder::GetDetailFont())
			]
			.ValueContent()
			[
				SNew(SCheckBox)
				.IsEnabled(this, &FBlueprintGlobalOptionsDetails::IsNativizeEnabled)
				.IsChecked(this, &FBlueprintGlobalOptionsDetails::GetNativizeState)
				.OnCheckStateChanged(this, &FBlueprintGlobalOptionsDetails::OnNativizeToggled)
				.ToolTipText(this, &FBlueprintGlobalOptionsDetails::GetNativizeTooltip)
			];
	}
}

bool FBlueprintGlobalOptionsDetails::IsNativizeEnabled() const
{
	bool bIsEnabled = false;
	if (UBlueprint* Blueprint = GetBlueprintObj())
	{
		bIsEnabled = (Blueprint->BlueprintType != BPTYPE_MacroLibrary) && (Blueprint->BlueprintType != BPTYPE_LevelScript) && (!FBlueprintEditorUtils::ShouldNativizeImplicitly(Blueprint));
	}
	return bIsEnabled;
}

ECheckBoxState FBlueprintGlobalOptionsDetails::GetNativizeState() const
{
	ECheckBoxState CheckboxState = ECheckBoxState::Undetermined;
	if (UBlueprint* Blueprint = GetBlueprintObj())
	{
		if (FBlueprintEditorUtils::ShouldNativizeImplicitly(Blueprint))
		{
			CheckboxState = ECheckBoxState::Checked;
		}
		else
		{
			switch (Blueprint->NativizationFlag)
			{
			case EBlueprintNativizationFlag::Disabled:
				CheckboxState = ECheckBoxState::Unchecked;
				break;

			case EBlueprintNativizationFlag::ExplicitlyEnabled:
				CheckboxState = ECheckBoxState::Checked;
				break;

			case EBlueprintNativizationFlag::Dependency:
			default:
				// leave "Undetermined"
				break;
			}
		}
	}
	return CheckboxState;	
}

FText FBlueprintGlobalOptionsDetails::GetNativizeTooltip() const
{
	if (!IsNativizeEnabled())
	{
		if (FBlueprintEditorUtils::ShouldNativizeImplicitly(GetBlueprintObj()))
		{
			return LOCTEXT("NativizeImplicitlyTooltip", "This Blueprint must be nativized because it overrides one or more BlueprintCallable functions inherited from a parent Blueprint class that has also been flagged for nativization.");
		}
		else
		{
			return LOCTEXT("NativizeDisabledTooltip", "Macro libraries and level Blueprints cannot be nativized.");
		}
	}
	else if (GetNativizeState() == ECheckBoxState::Undetermined)
	{
		return LOCTEXT("NativizeAsDependencyTooltip", "This Blueprint has been flagged to nativize as a dependency needed by another Blueprint. This will be applied once that Blueprint is saved.");
	}
	return GetNativizeLabelTooltip();
}

void FBlueprintGlobalOptionsDetails::OnNativizeToggled(ECheckBoxState NewState) const
{
	if (UBlueprint* Blueprint = GetBlueprintObj())
	{
		if (NewState == ECheckBoxState::Checked)
		{
			Blueprint->NativizationFlag = EBlueprintNativizationFlag::ExplicitlyEnabled;

			TArray<UClass*> NativizationDependencies;
			FBlueprintEditorUtils::FindNativizationDependencies(Blueprint, NativizationDependencies);

			int32 bDependenciesFlagged = 0;
			// tag all dependencies as needing nativization
			for (int32 DependencyIndex = 0; DependencyIndex < NativizationDependencies.Num(); ++DependencyIndex)
			{
				UClass* Dependency = NativizationDependencies[DependencyIndex];
				if (UBlueprint* DependentBp = UBlueprint::GetBlueprintFromClass(Dependency))
				{
					if (DependentBp->NativizationFlag == EBlueprintNativizationFlag::Disabled)
					{
						DependentBp->NativizationFlag = EBlueprintNativizationFlag::Dependency;
						++bDependenciesFlagged;
					}
					// recursively tag dependencies up the chain...
					// relying on the fact that this only adds to the array via AddUnique()
					FBlueprintEditorUtils::FindNativizationDependencies(DependentBp, NativizationDependencies);
				}
			}


			if (bDependenciesFlagged > 0)
			{
				FNotificationInfo Warning(LOCTEXT("DependenciesMarkedForNativization", "Flagged extra (required dependency) Blueprints for nativization."));
				Warning.ExpireDuration = 5.0f;
				Warning.bFireAndForget = true;
				Warning.Image = FCoreStyle::Get().GetBrush(TEXT("MessageLog.Warning"));
				FSlateNotificationManager::Get().AddNotification(Warning);
			}
		}
		else
		{
			Blueprint->NativizationFlag = EBlueprintNativizationFlag::Disabled;
		}

		// don't need to alter (dirty) compilation state, just the package's save state (since we save this setting to a config on save)
// 		UProperty* NativizeProperty = UBlueprint::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UBlueprint, NativizationFlag));
// 		if (ensure(NativizeProperty != nullptr))
// 		{
// 			FPropertyChangedEvent PropertyChangedEvent(NativizeProperty);
// 			PropertyChangedEvent.ChangeType = EPropertyChangeType::ValueSet;
// 
// 			FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint, PropertyChangedEvent);
// 		}
		Blueprint->MarkPackageDirty();
	}
}

void FBlueprintComponentDetails::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	check( BlueprintEditorPtr.IsValid() );
	TSharedPtr<SSCSEditor> Editor = BlueprintEditorPtr.Pin()->GetSCSEditor();
	check( Editor.IsValid() );
	const UBlueprint* BlueprintObj = GetBlueprintObj();
	check(BlueprintObj != nullptr);

	TArray<FSCSEditorTreeNodePtrType> Nodes = Editor->GetSelectedNodes();

	if (!Nodes.Num())
	{
		CachedNodePtr = nullptr;
	}
	else if (Nodes.Num() == 1)
	{
		CachedNodePtr = Nodes[0];
	}

	if( CachedNodePtr.IsValid() )
	{
		IDetailCategoryBuilder& VariableCategory = DetailLayout.EditCategory("Variable", LOCTEXT("VariableDetailsCategory", "Variable"), ECategoryPriority::Variable);

		VariableNameEditableTextBox = SNew(SEditableTextBox)
			.Text(this, &FBlueprintComponentDetails::OnGetVariableText)
			.OnTextChanged(this, &FBlueprintComponentDetails::OnVariableTextChanged)
			.OnTextCommitted(this, &FBlueprintComponentDetails::OnVariableTextCommitted)
			.IsReadOnly(!CachedNodePtr->CanRename())
			.Font(IDetailLayoutBuilder::GetDetailFont());

		VariableCategory.AddCustomRow(LOCTEXT("BlueprintComponentDetails_VariableNameLabel", "Variable Name"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("BlueprintComponentDetails_VariableNameLabel", "Variable Name"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		[
			VariableNameEditableTextBox.ToSharedRef()
		];

		VariableCategory.AddCustomRow(LOCTEXT("BlueprintComponentDetails_VariableTooltipLabel", "Tooltip"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("BlueprintComponentDetails_VariableTooltipLabel", "Tooltip"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		[
			SNew(SEditableTextBox)
			.Text(this, &FBlueprintComponentDetails::OnGetTooltipText)
			.OnTextCommitted(this, &FBlueprintComponentDetails::OnTooltipTextCommitted, CachedNodePtr->GetVariableName())
			.Font(IDetailLayoutBuilder::GetDetailFont())
		];

		PopulateVariableCategories();
		const FText CategoryTooltip = LOCTEXT("EditCategoryName_Tooltip", "The category of the variable; editing this will place the variable into another category or create a new one.");

		VariableCategory.AddCustomRow( LOCTEXT("BlueprintComponentDetails_VariableCategoryLabel", "Category") )
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("BlueprintComponentDetails_VariableCategoryLabel", "Category"))
			.ToolTipText(CategoryTooltip)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		[
			SAssignNew(VariableCategoryComboButton, SComboButton)
			.ContentPadding(FMargin(0,0,5,0))
			.IsEnabled(this, &FBlueprintComponentDetails::OnVariableCategoryChangeEnabled)
			.ButtonContent()
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("NoBorder"))
				.Padding(FMargin(0, 0, 5, 0))
				[
					SNew(SEditableTextBox)
					.Text(this, &FBlueprintComponentDetails::OnGetVariableCategoryText)
					.OnTextCommitted(this, &FBlueprintComponentDetails::OnVariableCategoryTextCommitted, CachedNodePtr->GetVariableName())
					.ToolTipText(CategoryTooltip)
					.SelectAllTextWhenFocused(true)
					.RevertTextOnEscape(true)
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			]
			.MenuContent()
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				.MaxHeight(400.0f)
				[
					SAssignNew(VariableCategoryListView, SListView<TSharedPtr<FText>>)
					.ListItemsSource(&VariableCategorySource)
					.OnGenerateRow(this, &FBlueprintComponentDetails::MakeVariableCategoryViewWidget)
					.OnSelectionChanged(this, &FBlueprintComponentDetails::OnVariableCategorySelectionChanged)
				]
			]
		];

		IDetailCategoryBuilder& SocketsCategory = DetailLayout.EditCategory("Sockets", LOCTEXT("BlueprintComponentDetailsCategory", "Sockets"), ECategoryPriority::Important);

		SocketsCategory.AddCustomRow(LOCTEXT("BlueprintComponentDetails_Sockets", "Sockets"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("BlueprintComponentDetails_ParentSocket", "Parent Socket"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SEditableTextBox)
				.Text(this, &FBlueprintComponentDetails::GetSocketName)
				.IsReadOnly(true)
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Padding(2.0f, 1.0f)
			[
				PropertyCustomizationHelpers::MakeBrowseButton(
					FSimpleDelegate::CreateSP(this, &FBlueprintComponentDetails::OnBrowseSocket), 
					LOCTEXT( "SocketBrowseButtonToolTipText", "Select a different Parent Socket - cannot change socket on inherited componentes"), 
					TAttribute<bool>(this, &FBlueprintComponentDetails::CanChangeSocket)
				)
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Padding(2.0f, 1.0f)
			[
				PropertyCustomizationHelpers::MakeClearButton(
					FSimpleDelegate::CreateSP(this, &FBlueprintComponentDetails::OnClearSocket), 
					LOCTEXT("SocketClearButtonToolTipText", "Clear the Parent Socket - cannot change socket on inherited componentes"), 
					TAttribute<bool>(this, &FBlueprintComponentDetails::CanChangeSocket)
				)
			]
		];
	}

	// Handle event generation
	if ( FBlueprintEditorUtils::DoesSupportEventGraphs(BlueprintObj) && Nodes.Num() == 1 )
	{
		FName PropertyName = CachedNodePtr->GetVariableName();
		UObjectProperty* VariableProperty = FindField<UObjectProperty>(BlueprintObj->SkeletonGeneratedClass, PropertyName);

		AddEventsCategory(DetailLayout, VariableProperty);
	}

	// Don't show tick properties for components in the blueprint details
	TSharedPtr<IPropertyHandle> PrimaryTickProperty = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UActorComponent, PrimaryComponentTick));
	PrimaryTickProperty->MarkHiddenByCustomization();
}

FText FBlueprintComponentDetails::OnGetVariableText() const
{
	check(CachedNodePtr.IsValid());

	return FText::FromName(CachedNodePtr->GetVariableName());
}

void FBlueprintComponentDetails::OnVariableTextChanged(const FText& InNewText)
{
	check(CachedNodePtr.IsValid());

	bIsVariableNameInvalid = true;

	USCS_Node* SCS_Node = CachedNodePtr->GetSCSNode();
	if(SCS_Node != NULL && !InNewText.IsEmpty() && !FComponentEditorUtils::IsValidVariableNameString(SCS_Node->ComponentTemplate, InNewText.ToString()))
	{
		VariableNameEditableTextBox->SetError(LOCTEXT("ComponentVariableRenameFailed_NotValid", "This name is reserved for engine use."));
		return;
	}

	TSharedPtr<INameValidatorInterface> VariableNameValidator = MakeShareable(new FKismetNameValidator(GetBlueprintObj(), CachedNodePtr->GetVariableName()));

	EValidatorResult ValidatorResult = VariableNameValidator->IsValid(InNewText.ToString());
	if(ValidatorResult == EValidatorResult::AlreadyInUse)
	{
		VariableNameEditableTextBox->SetError(FText::Format(LOCTEXT("ComponentVariableRenameFailed_InUse", "{0} is in use by another variable or function!"), InNewText));
	}
	else if(ValidatorResult == EValidatorResult::EmptyName)
	{
		VariableNameEditableTextBox->SetError(LOCTEXT("RenameFailed_LeftBlank", "Names cannot be left blank!"));
	}
	else if(ValidatorResult == EValidatorResult::TooLong)
	{
		VariableNameEditableTextBox->SetError(FText::Format( LOCTEXT("RenameFailed_NameTooLong", "Names must have fewer than {0} characters!"), FText::AsNumber( FKismetNameValidator::GetMaximumNameLength())));
	}
	else
	{
		bIsVariableNameInvalid = false;
		VariableNameEditableTextBox->SetError(FText::GetEmpty());
	}
}

void FBlueprintComponentDetails::OnVariableTextCommitted(const FText& InNewName, ETextCommit::Type InTextCommit)
{
	if ( !bIsVariableNameInvalid )
	{
		check(CachedNodePtr.IsValid());

		USCS_Node* SCS_Node = CachedNodePtr->GetSCSNode();
		if(SCS_Node != NULL)
		{
			const FScopedTransaction Transaction( LOCTEXT("RenameComponentVariable", "Rename Component Variable") );
			FBlueprintEditorUtils::RenameComponentMemberVariable(GetBlueprintObj(), CachedNodePtr->GetSCSNode(), FName( *InNewName.ToString() ));
		}
	}

	bIsVariableNameInvalid = false;
	VariableNameEditableTextBox->SetError(FText::GetEmpty());
}

FText FBlueprintComponentDetails::OnGetTooltipText() const
{
	check(CachedNodePtr.IsValid());

	FName VarName = CachedNodePtr->GetVariableName();
	if (VarName != NAME_None)
	{
		FString Result;
		FBlueprintEditorUtils::GetBlueprintVariableMetaData(GetBlueprintObj(), VarName, NULL, TEXT("tooltip"), Result);
		return FText::FromString(Result);
	}

	return FText();
}

void FBlueprintComponentDetails::OnTooltipTextCommitted(const FText& NewText, ETextCommit::Type InTextCommit, FName VarName)
{
	FBlueprintEditorUtils::SetBlueprintVariableMetaData(GetBlueprintObj(), VarName, NULL, TEXT("tooltip"), NewText.ToString() );
}

bool FBlueprintComponentDetails::OnVariableCategoryChangeEnabled() const
{
	check(CachedNodePtr.IsValid());

	return !CachedNodePtr->CanRename();
}

FText FBlueprintComponentDetails::OnGetVariableCategoryText() const
{
	check(CachedNodePtr.IsValid());

	FName VarName = CachedNodePtr->GetVariableName();
	if (VarName != NAME_None)
	{
		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

		FText Category = FBlueprintEditorUtils::GetBlueprintVariableCategory(GetBlueprintObj(), VarName, NULL);

		// Older blueprints will have their name as the default category
		if( Category.EqualTo(FText::FromString(GetBlueprintObj()->GetName())) )
		{
			return K2Schema->VR_DefaultCategory;
		}
		else
		{
			return Category;
		}
	}

	return FText();
}

void FBlueprintComponentDetails::OnVariableCategoryTextCommitted(const FText& NewText, ETextCommit::Type InTextCommit, FName VarName)
{
	check(CachedNodePtr.IsValid());

	if (InTextCommit == ETextCommit::OnEnter || InTextCommit == ETextCommit::OnUserMovedFocus)
	{
		FBlueprintEditorUtils::SetBlueprintVariableCategory(GetBlueprintObj(), CachedNodePtr->GetVariableName(), NULL, NewText);
		PopulateVariableCategories();
	}
}

void FBlueprintComponentDetails::OnVariableCategorySelectionChanged( TSharedPtr<FText> ProposedSelection, ESelectInfo::Type /*SelectInfo*/ )
{
	check(CachedNodePtr.IsValid());

	FName VarName = CachedNodePtr->GetVariableName();
	if (ProposedSelection.IsValid() && VarName != NAME_None)
	{
		FText NewCategory = *ProposedSelection.Get();
		FBlueprintEditorUtils::SetBlueprintVariableCategory(GetBlueprintObj(), VarName, NULL, NewCategory);

		check(VariableCategoryListView.IsValid());
		check(VariableCategoryComboButton.IsValid());

		VariableCategoryListView->ClearSelection();
		VariableCategoryComboButton->SetIsOpen(false);
	}
}

TSharedRef< ITableRow > FBlueprintComponentDetails::MakeVariableCategoryViewWidget( TSharedPtr<FText> Item, const TSharedRef< STableViewBase >& OwnerTable )
{
	return SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
	[
		SNew(STextBlock)
			.Text(*Item.Get())
	];
}

void FBlueprintComponentDetails::PopulateVariableCategories()
{
	UBlueprint* BlueprintObj = GetBlueprintObj();

	check(BlueprintObj);
	check(BlueprintObj->SkeletonGeneratedClass);

	TSet<FName> VisibleVariables;
	for (TFieldIterator<UProperty> PropertyIt(BlueprintObj->SkeletonGeneratedClass, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
	{
		UProperty* Property = *PropertyIt;

		if ((!Property->HasAnyPropertyFlags(CPF_Parm) && Property->HasAllPropertyFlags(CPF_BlueprintVisible)))
		{
			VisibleVariables.Add(Property->GetFName());
		}
	}

	FBlueprintEditorUtils::GetSCSVariableNameList(BlueprintObj, VisibleVariables);

	VariableCategorySource.Empty();
	VariableCategorySource.Add(MakeShareable(new FText(LOCTEXT("Default", "Default"))));
	for (const FName& VariableName : VisibleVariables)
	{
		FText Category = FBlueprintEditorUtils::GetBlueprintVariableCategory(BlueprintObj, VariableName, nullptr);
		if (!Category.IsEmpty() && !Category.EqualTo(FText::FromString(BlueprintObj->GetName())))
		{
			bool bNewCategory = true;
			for (int32 j = 0; j < VariableCategorySource.Num() && bNewCategory; ++j)
			{
				bNewCategory &= !VariableCategorySource[j].Get()->EqualTo(Category);
			}
			if (bNewCategory)
			{
				VariableCategorySource.Add(MakeShareable(new FText(Category)));
			}
		}
	}
}

FText FBlueprintComponentDetails::GetSocketName() const
{
	check(CachedNodePtr.IsValid());

	if (CachedNodePtr->GetSCSNode() != NULL)
	{
		return FText::FromName(CachedNodePtr->GetSCSNode()->AttachToName);
	}
	return FText::GetEmpty();
}

bool FBlueprintComponentDetails::CanChangeSocket() const
{
	check(CachedNodePtr.IsValid());

	if (CachedNodePtr->GetSCSNode() != NULL)
	{
		return !CachedNodePtr->IsInherited();
	}
	return true;
}

void FBlueprintComponentDetails::OnBrowseSocket()
{
	check(CachedNodePtr.IsValid());

	if (CachedNodePtr->GetSCSNode() != NULL)
	{
		TSharedPtr<SSCSEditor> Editor = BlueprintEditorPtr.Pin()->GetSCSEditor();
		check( Editor.IsValid() );

		FSCSEditorTreeNodePtrType ParentFNode = CachedNodePtr->GetParent();

		if (ParentFNode.IsValid())
		{
			if (USceneComponent* ParentSceneComponent = Cast<USceneComponent>(ParentFNode->GetEditableComponentTemplate(Editor->GetBlueprint())))
			{
				if (ParentSceneComponent->HasAnySockets())
				{
					// Pop up a combo box to pick socket from mesh
					FSlateApplication::Get().PushMenu(
						Editor.ToSharedRef(),
						FWidgetPath(),
						SNew(SSocketChooserPopup)
						.SceneComponent( ParentSceneComponent )
						.OnSocketChosen( this, &FBlueprintComponentDetails::OnSocketSelection ),
						FSlateApplication::Get().GetCursorPos(),
						FPopupTransitionEffect( FPopupTransitionEffect::TypeInPopup )
						);
				}
			}
		}
	}
}

void FBlueprintComponentDetails::OnClearSocket()
{
	check(CachedNodePtr.IsValid());

	if (CachedNodePtr->GetSCSNode() != NULL)
	{
		CachedNodePtr->GetSCSNode()->AttachToName = NAME_None;
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprintObj());
	}
}

void FBlueprintComponentDetails::OnSocketSelection( FName SocketName )
{
	check(CachedNodePtr.IsValid());

	USCS_Node* SCS_Node = CachedNodePtr->GetSCSNode();
	if (SCS_Node != NULL)
	{
		// Record selection if there is an actual asset attached
		SCS_Node->AttachToName = SocketName;
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprintObj());
	}
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void FBlueprintGraphNodeDetails::CustomizeDetails( IDetailLayoutBuilder& DetailLayout )
{
	const TArray<TWeakObjectPtr<UObject>>& SelectedObjects = DetailLayout.GetSelectedObjects();
	if( SelectedObjects.Num() == 1 )
	{
		if (SelectedObjects[0].IsValid() && SelectedObjects[0]->IsA<UEdGraphNode>())
		{
			GraphNodePtr = Cast<UEdGraphNode>(SelectedObjects[0].Get());
		}
	}

	if(!GraphNodePtr.IsValid() || !GraphNodePtr.Get()->bCanRenameNode)
	{
		return;
	}

	IDetailCategoryBuilder& Category = DetailLayout.EditCategory("GraphNodeDetail", LOCTEXT("GraphNodeDetailsCategory", "Graph Node"), ECategoryPriority::Important);
	const FSlateFontInfo DetailFontInfo = IDetailLayoutBuilder::GetDetailFont();
	FText RowHeader;
	FText NameContent;

	if( GraphNodePtr->IsA( UEdGraphNode_Comment::StaticClass() ))
	{
		RowHeader = LOCTEXT("GraphNodeDetail_CommentRowTitle", "Comment");
		NameContent = LOCTEXT("GraphNodeDetail_CommentContentTitle", "Comment Text");
	}
	else
	{
		RowHeader = LOCTEXT("GraphNodeDetail_NodeRowTitle", "Node Title");
		NameContent = LOCTEXT("GraphNodeDetail_ContentTitle", "Name");
	}

	bool bNameAllowsMultiLine = false;
	if( GraphNodePtr.IsValid() && GraphNodePtr.Get()->IsA<UEdGraphNode_Comment>() )
	{
		bNameAllowsMultiLine = true;
	}

	TSharedPtr<SWidget> EditNameWidget;
	float WidgetMinDesiredWidth = BlueprintDocumentationDetailDefs::DetailsTitleMinWidth;
	float WidgetMaxDesiredWidth = BlueprintDocumentationDetailDefs::DetailsTitleMaxWidth;
	if( bNameAllowsMultiLine )
	{
		SAssignNew(MultiLineNameEditableTextBox, SMultiLineEditableTextBox)
		.Text(this, &FBlueprintGraphNodeDetails::OnGetName)
		.OnTextChanged(this, &FBlueprintGraphNodeDetails::OnNameChanged)
		.OnTextCommitted(this, &FBlueprintGraphNodeDetails::OnNameCommitted)
		.ClearKeyboardFocusOnCommit(true)
		.ModiferKeyForNewLine(EModifierKey::Shift)
		.RevertTextOnEscape(true)
		.SelectAllTextWhenFocused(true)
		.IsReadOnly(this, &FBlueprintGraphNodeDetails::IsNameReadOnly)
		.Font(DetailFontInfo)
		.WrapTextAt(WidgetMaxDesiredWidth - BlueprintDocumentationDetailDefs::DetailsTitleWrapPadding);

		EditNameWidget = MultiLineNameEditableTextBox;
	}
	else
	{
		SAssignNew(NameEditableTextBox, SEditableTextBox)
		.Text(this, &FBlueprintGraphNodeDetails::OnGetName)
		.OnTextChanged(this, &FBlueprintGraphNodeDetails::OnNameChanged)
		.OnTextCommitted(this, &FBlueprintGraphNodeDetails::OnNameCommitted)
		.Font(DetailFontInfo);

		EditNameWidget = NameEditableTextBox;
		WidgetMaxDesiredWidth = WidgetMinDesiredWidth;
	}

	Category.AddCustomRow( RowHeader )
	.NameContent()
	[
		SNew(STextBlock)
		.Text( NameContent )
		.Font(DetailFontInfo)
	]
	.ValueContent()
	.MinDesiredWidth(WidgetMinDesiredWidth)
	.MaxDesiredWidth(WidgetMaxDesiredWidth)
	[
		EditNameWidget.ToSharedRef()
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void FBlueprintGraphNodeDetails::SetNameError( const FText& Error )
{
	if( NameEditableTextBox.IsValid() )
	{
		NameEditableTextBox->SetError( Error );
	}
	if( MultiLineNameEditableTextBox.IsValid() )
	{
		MultiLineNameEditableTextBox->SetError( Error );
	}
}

bool FBlueprintGraphNodeDetails::IsNameReadOnly() const
{
	bool bReadOnly = true;
	if(GraphNodePtr.IsValid())
	{
		bReadOnly = !GraphNodePtr->bCanRenameNode;
	}
	return bReadOnly;
}

FText FBlueprintGraphNodeDetails::OnGetName() const
{
	FText Name;
	if(GraphNodePtr.IsValid())
	{
		Name = GraphNodePtr->GetNodeTitle( ENodeTitleType::EditableTitle );
	}
	return Name;
}

struct FGraphNodeNameValidatorHelper
{
	static EValidatorResult Validate(TWeakObjectPtr<UEdGraphNode> GraphNodePtr, TWeakPtr<FBlueprintEditor> BlueprintEditorPtr, const FString& NewName)
	{
		check(GraphNodePtr.IsValid() && BlueprintEditorPtr.IsValid());
		TSharedPtr<INameValidatorInterface> NameValidator = GraphNodePtr->MakeNameValidator();
		if (!NameValidator.IsValid())
		{
			const FName NodeName(*GraphNodePtr->GetNodeTitle(ENodeTitleType::EditableTitle).ToString());
			NameValidator = MakeShareable(new FKismetNameValidator(BlueprintEditorPtr.Pin()->GetBlueprintObj(), NodeName));
		}
		return NameValidator->IsValid(NewName);
	}
};

void FBlueprintGraphNodeDetails::OnNameChanged(const FText& InNewText)
{
	if( GraphNodePtr.IsValid() && BlueprintEditorPtr.IsValid() )
	{
		const EValidatorResult ValidatorResult = FGraphNodeNameValidatorHelper::Validate(GraphNodePtr, BlueprintEditorPtr, InNewText.ToString());
		if(ValidatorResult == EValidatorResult::AlreadyInUse)
		{
			SetNameError(FText::Format(LOCTEXT("RenameFailed_InUse", "{0} is in use by another variable or function!"), InNewText));
		}
		else if(ValidatorResult == EValidatorResult::EmptyName)
		{
			SetNameError(LOCTEXT("RenameFailed_LeftBlank", "Names cannot be left blank!"));
		}
		else if(ValidatorResult == EValidatorResult::TooLong)
		{
			SetNameError(FText::Format( LOCTEXT("RenameFailed_NameTooLong", "Names must have fewer than {0} characters!"), FText::AsNumber( FKismetNameValidator::GetMaximumNameLength())));
		}
		else
		{
			SetNameError(FText::GetEmpty());
		}
	}
}

void FBlueprintGraphNodeDetails::OnNameCommitted(const FText& InNewText, ETextCommit::Type InTextCommit)
{
	if (BlueprintEditorPtr.IsValid() && GraphNodePtr.IsValid())
	{
		if (FGraphNodeNameValidatorHelper::Validate(GraphNodePtr, BlueprintEditorPtr, InNewText.ToString()) == EValidatorResult::Ok)
		{
			BlueprintEditorPtr.Pin()->OnNodeTitleCommitted(InNewText, InTextCommit, GraphNodePtr.Get());
		}
	}
}

UBlueprint* FBlueprintGraphNodeDetails::GetBlueprintObj() const
{
	if(BlueprintEditorPtr.IsValid())
	{
		return BlueprintEditorPtr.Pin()->GetBlueprintObj();
	}

	return NULL;
}

TSharedRef<IDetailCustomization> FChildActorComponentDetails::MakeInstance(TWeakPtr<FBlueprintEditor> BlueprintEditorPtrIn)
{
	return MakeShareable(new FChildActorComponentDetails(BlueprintEditorPtrIn));
}

FChildActorComponentDetails::FChildActorComponentDetails(TWeakPtr<FBlueprintEditor> BlueprintEditorPtrIn)
	: BlueprintEditorPtr(BlueprintEditorPtrIn)
{
}

void FChildActorComponentDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TSharedPtr<IPropertyHandle> ActorClassProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UChildActorComponent, ChildActorClass));
	if (ActorClassProperty->IsValidHandle())
	{
		if (BlueprintEditorPtr.IsValid())
		{
			// only restrict for the components view (you can successfully add 
			// a self child component in the execution graphs)
			if (BlueprintEditorPtr.Pin()->GetCurrentMode() == FBlueprintEditorApplicationModes::BlueprintComponentsMode)
			{
				if (UBlueprint* Blueprint = BlueprintEditorPtr.Pin()->GetBlueprintObj())
				{
					FText RestrictReason = LOCTEXT("NoSelfChildActors", "Cannot append a child-actor of this blueprint type (could cause infinite recursion).");
					TSharedPtr<FPropertyRestriction> ClassRestriction = MakeShareable(new FPropertyRestriction(MoveTemp(RestrictReason)));

					ClassRestriction->AddDisabledValue(Blueprint->GetName());
					ClassRestriction->AddDisabledValue(Blueprint->GetPathName());
					if (Blueprint->GeneratedClass)
					{
						ClassRestriction->AddDisabledValue(Blueprint->GeneratedClass->GetName());
						ClassRestriction->AddDisabledValue(Blueprint->GeneratedClass->GetPathName());
					}

					ActorClassProperty->AddRestriction(ClassRestriction.ToSharedRef());
				}
			}
		}

		TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
		DetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);

		IDetailCategoryBuilder& CategoryBuilder = DetailBuilder.EditCategory(TEXT("ChildActorComponent"));
				
		// Ensure ordering is what we want by adding class in first
		CategoryBuilder.AddProperty(GET_MEMBER_NAME_CHECKED(UChildActorComponent, ChildActorClass));

		IDetailPropertyRow& CATRow = CategoryBuilder.AddProperty(GET_MEMBER_NAME_CHECKED(UChildActorComponent, ChildActorTemplate));
		CATRow.Visibility(TAttribute<EVisibility>::Create([ObjectsBeingCustomized]()
		{
			for (const TWeakObjectPtr<UObject>& ObjectBeingCustomized : ObjectsBeingCustomized)
			{
				if (UChildActorComponent* CAC = Cast<UChildActorComponent>(ObjectBeingCustomized.Get()))
				{
					if (CAC->ChildActorTemplate == nullptr)
					{
						return EVisibility::Hidden;
					}
				}
				else
				{
					return EVisibility::Hidden;
				}
			}

			return EVisibility::Visible;
		}));
	}
}

void FBlueprintDocumentationDetails::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	check( BlueprintEditorPtr.IsValid() );
	// find currently selected edgraph documentation node
	DocumentationNodePtr = EdGraphSelectionAsDocumentNode();

	if( DocumentationNodePtr.IsValid() )
	{
		// Cache Link
		DocumentationLink = DocumentationNodePtr->GetDocumentationLink();
		DocumentationExcerpt = DocumentationNodePtr->GetDocumentationExcerptName();

		IDetailCategoryBuilder& DocumentationCategory = DetailLayout.EditCategory("Documentation", LOCTEXT("DocumentationDetailsCategory", "Documentation"), ECategoryPriority::Default);

		DocumentationCategory.AddCustomRow( LOCTEXT( "DocumentationLinkLabel", "Documentation Link" ))
		.NameContent()
		.HAlign( HAlign_Fill )
		[
			SNew( STextBlock )
			.Text( LOCTEXT( "FBlueprintDocumentationDetails_Link", "Link" ) )
			.ToolTipText( LOCTEXT( "FBlueprintDocumentationDetails_LinkPathTooltip", "The documentation content path" ))
			.Font( IDetailLayoutBuilder::GetDetailFont() )
		]
		.ValueContent()
		.HAlign( HAlign_Left )
		.MinDesiredWidth( BlueprintDocumentationDetailDefs::DetailsTitleMinWidth )
		.MaxDesiredWidth( BlueprintDocumentationDetailDefs::DetailsTitleMaxWidth )
		[
			SNew( SEditableTextBox )
			.Padding( FMargin( 4.f, 2.f ))
			.Text( this, &FBlueprintDocumentationDetails::OnGetDocumentationLink )
			.ToolTipText( LOCTEXT( "FBlueprintDocumentationDetails_LinkTooltip", "The path of the documentation content relative to /Engine/Documentation/Source" ))
			.OnTextCommitted( this, &FBlueprintDocumentationDetails::OnDocumentationLinkCommitted )
			.Font( IDetailLayoutBuilder::GetDetailFont() )
		];

		DocumentationCategory.AddCustomRow( LOCTEXT( "DocumentationExcerptsLabel", "Documentation Excerpts" ))
		.NameContent()
		.HAlign( HAlign_Left )
		[
			SNew( STextBlock )
			.Text( LOCTEXT( "FBlueprintDocumentationDetails_Excerpt", "Excerpt" ) )
			.ToolTipText( LOCTEXT( "FBlueprintDocumentationDetails_ExcerptTooltip", "The current documentation excerpt" ))
			.Font( IDetailLayoutBuilder::GetDetailFont() )
		]
		.ValueContent()
		.HAlign( HAlign_Left )
		.MinDesiredWidth( BlueprintDocumentationDetailDefs::DetailsTitleMinWidth )
		.MaxDesiredWidth( BlueprintDocumentationDetailDefs::DetailsTitleMaxWidth )
		[
			SAssignNew( ExcerptComboButton, SComboButton )
			.ContentPadding( 2.f )
			.IsEnabled( this, &FBlueprintDocumentationDetails::OnExcerptChangeEnabled )
			.ButtonContent()
			[
				SNew(SBorder)
				.BorderImage( FEditorStyle::GetBrush( "NoBorder" ))
				.Padding( FMargin( 0, 0, 5, 0 ))
				[
					SNew( STextBlock )
					.Text( this, &FBlueprintDocumentationDetails::OnGetDocumentationExcerpt )
					.ToolTipText( LOCTEXT( "FBlueprintDocumentationDetails_ExcerptComboTooltip", "Select Excerpt" ))
					.Font( IDetailLayoutBuilder::GetDetailFont() )
				]
			]
			.OnGetMenuContent( this, &FBlueprintDocumentationDetails::GenerateExcerptList )
		];
	}
}

TWeakObjectPtr<UEdGraphNode_Documentation> FBlueprintDocumentationDetails::EdGraphSelectionAsDocumentNode()
{
	DocumentationNodePtr.Reset();

	if( BlueprintEditorPtr.IsValid() )
	{
		/** Get the currently selected set of nodes */
		if( BlueprintEditorPtr.Pin()->GetNumberOfSelectedNodes() == 1 )
		{
			TSet<UObject*> Objects = BlueprintEditorPtr.Pin()->GetSelectedNodes();
			TSet<UObject*>::TIterator Iter( Objects );
			UObject* Object = *Iter;

			if( Object && Object->IsA<UEdGraphNode_Documentation>() )
			{
				DocumentationNodePtr = Cast<UEdGraphNode_Documentation>( Object );
			}
		}
	}
	return DocumentationNodePtr;
}

FText FBlueprintDocumentationDetails::OnGetDocumentationLink() const
{
	return FText::FromString( DocumentationLink );
}

FText FBlueprintDocumentationDetails::OnGetDocumentationExcerpt() const
{
	return FText::FromString( DocumentationExcerpt );
}

bool FBlueprintDocumentationDetails::OnExcerptChangeEnabled() const
{
	return IDocumentation::Get()->PageExists( DocumentationLink );
}

void FBlueprintDocumentationDetails::OnDocumentationLinkCommitted( const FText& InNewName, ETextCommit::Type InTextCommit )
{
	DocumentationLink = InNewName.ToString();
	DocumentationExcerpt = NSLOCTEXT( "FBlueprintDocumentationDetails", "ExcerptCombo_DefaultText", "Select Excerpt" ).ToString();
}

TSharedRef< ITableRow > FBlueprintDocumentationDetails::MakeExcerptViewWidget( TSharedPtr<FString> Item, const TSharedRef< STableViewBase >& OwnerTable )
{
	return 
		SNew( STableRow<TSharedPtr<FString>>, OwnerTable )
		[
			SNew( STextBlock )
			.Text( FText::FromString(*Item.Get()) )
		];
}

void FBlueprintDocumentationDetails::OnExcerptSelectionChanged( TSharedPtr<FString> ProposedSelection, ESelectInfo::Type /*SelectInfo*/ )
{
	if( ProposedSelection.IsValid() && DocumentationNodePtr.IsValid() )
	{
		DocumentationNodePtr->Link = DocumentationLink;
		DocumentationExcerpt = *ProposedSelection.Get();
		DocumentationNodePtr->Excerpt = DocumentationExcerpt;
		ExcerptComboButton->SetIsOpen( false );
	}
}

TSharedRef<SWidget> FBlueprintDocumentationDetails::GenerateExcerptList()
{
	ExcerptList.Empty();

	if( IDocumentation::Get()->PageExists( DocumentationLink ))
	{
		TSharedPtr<IDocumentationPage> DocumentationPage = IDocumentation::Get()->GetPage( DocumentationLink, NULL );
		TArray<FExcerpt> Excerpts;
		DocumentationPage->GetExcerpts( Excerpts );

		for (const FExcerpt& Excerpt : Excerpts)
		{
			ExcerptList.Add( MakeShareable( new FString( Excerpt.Name )));
		}
	}

	return
		SNew( SHorizontalBox )
		+SHorizontalBox::Slot()
		.Padding( 2.f )
		[
			SNew( SListView< TSharedPtr<FString>> )
			.ListItemsSource( &ExcerptList )
			.OnGenerateRow( this, &FBlueprintDocumentationDetails::MakeExcerptViewWidget )
			.OnSelectionChanged( this, &FBlueprintDocumentationDetails::OnExcerptSelectionChanged )
		];
}


#undef LOCTEXT_NAMESPACE
