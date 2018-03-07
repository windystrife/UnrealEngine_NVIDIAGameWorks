// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MaterialEditorInstanceDetailCustomization.h"
#include "Misc/MessageDialog.h"
#include "Misc/Guid.h"
#include "UObject/UnrealType.h"
#include "Layout/Margin.h"
#include "Misc/Attribute.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "EditorStyleSet.h"
#include "Materials/MaterialInterface.h"
#include "MaterialEditor/DEditorFontParameterValue.h"
#include "MaterialEditor/DEditorScalarParameterValue.h"
#include "MaterialEditor/DEditorStaticComponentMaskParameterValue.h"
#include "MaterialEditor/DEditorStaticSwitchParameterValue.h"
#include "MaterialEditor/DEditorTextureParameterValue.h"
#include "MaterialEditor/DEditorVectorParameterValue.h"
#include "MaterialEditor/MaterialEditorInstanceConstant.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialExpressionParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "Materials/MaterialExpressionFontSampleParameter.h"
#include "EditorSupportDelegates.h"
#include "DetailWidgetRow.h"
#include "PropertyHandle.h"
#include "IDetailPropertyRow.h"
#include "DetailLayoutBuilder.h"
#include "IDetailGroup.h"
#include "DetailCategoryBuilder.h"
#include "PropertyCustomizationHelpers.h"
#include "ScopedTransaction.h"
#include "Materials/MaterialInstanceConstant.h"

#define LOCTEXT_NAMESPACE "MaterialInstanceEditor"

TSharedRef<IDetailCustomization> FMaterialInstanceParameterDetails::MakeInstance(UMaterialEditorInstanceConstant* MaterialInstance, FGetShowHiddenParameters InShowHiddenDelegate)
{
	return MakeShareable(new FMaterialInstanceParameterDetails(MaterialInstance, InShowHiddenDelegate));
}

FMaterialInstanceParameterDetails::FMaterialInstanceParameterDetails(UMaterialEditorInstanceConstant* MaterialInstance, FGetShowHiddenParameters InShowHiddenDelegate)
	: MaterialEditorInstance(MaterialInstance)
	, ShowHiddenDelegate(InShowHiddenDelegate)
{
}

TOptional<float> FMaterialInstanceParameterDetails::OnGetValue(TSharedRef<IPropertyHandle> PropertyHandle)
{
	float Value = 0.0f;
	if (PropertyHandle->GetValue(Value) == FPropertyAccess::Success)
	{
		return TOptional<float>(Value);
	}

	// Value couldn't be accessed. Return an unset value
	return TOptional<float>();
}

void FMaterialInstanceParameterDetails::OnValueCommitted(float NewValue, ETextCommit::Type CommitType, TSharedRef<IPropertyHandle> PropertyHandle)
{	
	// Try setting as float, if that fails then set as int
	ensure(PropertyHandle->SetValue(NewValue) == FPropertyAccess::Success);
}

void FMaterialInstanceParameterDetails::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	// Create a new category for a custom layout for the MIC parameters at the very top
	FName GroupsCategoryName = TEXT("ParameterGroups");
	IDetailCategoryBuilder& GroupsCategory = DetailLayout.EditCategory(GroupsCategoryName, LOCTEXT("MICParamGroupsTitle", "Parameter Groups"));
	TSharedRef<IPropertyHandle> ParameterGroupsProperty = DetailLayout.GetProperty("ParameterGroups");

	CreateGroupsWidget(ParameterGroupsProperty, GroupsCategory);

	// Create default category for class properties
	const FName DefaultCategoryName = NAME_None;
	IDetailCategoryBuilder& DefaultCategory = DetailLayout.EditCategory(DefaultCategoryName);

	// Add PhysMaterial property
	DefaultCategory.AddProperty("PhysMaterial");

	// Customize Parent property so we can check for recursively set parents
	TSharedRef<IPropertyHandle> ParentPropertyHandle = DetailLayout.GetProperty("Parent");
	IDetailPropertyRow& ParentPropertyRow = DefaultCategory.AddProperty(ParentPropertyHandle);

	ParentPropertyHandle->MarkResetToDefaultCustomized();
	
	TSharedPtr<SWidget> NameWidget;
	TSharedPtr<SWidget> ValueWidget;
	FDetailWidgetRow Row;

	ParentPropertyRow.GetDefaultWidgets(NameWidget, ValueWidget, Row);
	
	ParentPropertyHandle->ClearResetToDefaultCustomized();

	const bool bShowChildren = true;
	ParentPropertyRow.CustomWidget(bShowChildren)
		.NameContent()
		.MinDesiredWidth(Row.NameWidget.MinWidth)
		.MaxDesiredWidth(Row.NameWidget.MaxWidth)
		[
			NameWidget.ToSharedRef()
		]
		.ValueContent()
		.MinDesiredWidth(Row.ValueWidget.MinWidth)
		.MaxDesiredWidth(Row.ValueWidget.MaxWidth)
		[
			SNew(SObjectPropertyEntryBox)
			.PropertyHandle(ParentPropertyHandle)
			.AllowedClass(UMaterialInterface::StaticClass())
			.ThumbnailPool(DetailLayout.GetThumbnailPool())
			.AllowClear(true)
			.OnShouldSetAsset(this, &FMaterialInstanceParameterDetails::OnShouldSetAsset)
		];

	ValueWidget.Reset();


	// Add/hide other properties
	DefaultCategory.AddProperty("LightmassSettings");
	DetailLayout.HideProperty("bUseOldStyleMICEditorGroups");
	DetailLayout.HideProperty("ParameterGroups");

	{
		IDetailPropertyRow& PropertyRow = DefaultCategory.AddProperty("RefractionDepthBias");
		PropertyRow.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FMaterialInstanceParameterDetails::ShouldShowMaterialRefractionSettings)));
	}

	{
		IDetailPropertyRow& PropertyRow = DefaultCategory.AddProperty("bOverrideSubsurfaceProfile");
		PropertyRow.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FMaterialInstanceParameterDetails::ShouldShowSubsurfaceProfile)));
	}

	{
		IDetailPropertyRow& PropertyRow = DefaultCategory.AddProperty("SubsurfaceProfile");
		PropertyRow.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FMaterialInstanceParameterDetails::ShouldShowSubsurfaceProfile)));
	}

	DetailLayout.HideProperty("BasePropertyOverrides");
	CreateBasePropertyOverrideWidgets(DetailLayout);

	// Add the preview mesh property directly from the material instance 
	FName PreviewingCategoryName = TEXT("Previewing");
	IDetailCategoryBuilder& PreviewingCategory = DetailLayout.EditCategory(PreviewingCategoryName, LOCTEXT("MICPreviewingCategoryTitle", "Previewing"));

	TArray<UObject*> ExternalObjects;
	ExternalObjects.Add(MaterialEditorInstance->SourceInstance);

	PreviewingCategory.AddExternalObjectProperty(ExternalObjects, TEXT("PreviewMesh"));

}

void FMaterialInstanceParameterDetails::CreateGroupsWidget(TSharedRef<IPropertyHandle> ParameterGroupsProperty, IDetailCategoryBuilder& GroupsCategory)
{
	check(MaterialEditorInstance);

	for (int32 GroupIdx = 0; GroupIdx < MaterialEditorInstance->ParameterGroups.Num(); ++GroupIdx)
	{
		FEditorParameterGroup& ParameterGroup = MaterialEditorInstance->ParameterGroups[GroupIdx];

		IDetailGroup& DetailGroup = GroupsCategory.AddGroup( ParameterGroup.GroupName, FText::FromName(ParameterGroup.GroupName), false, true );

		CreateSingleGroupWidget( ParameterGroup, ParameterGroupsProperty->GetChildHandle(GroupIdx), DetailGroup );
	}
}

void FMaterialInstanceParameterDetails::CreateSingleGroupWidget(FEditorParameterGroup& ParameterGroup, TSharedPtr<IPropertyHandle> ParameterGroupProperty, IDetailGroup& DetailGroup )
{
	TSharedPtr<IPropertyHandle> ParametersArrayProperty = ParameterGroupProperty->GetChildHandle("Parameters");

	// Create a custom widget for each parameter in the group
	for (int32 ParamIdx = 0; ParamIdx < ParameterGroup.Parameters.Num(); ++ParamIdx)
	{
		TSharedPtr<IPropertyHandle> ParameterProperty = ParametersArrayProperty->GetChildHandle(ParamIdx);

		FString ParameterName = ParameterGroup.Parameters[ParamIdx]->ParameterName.ToString();
		
		UDEditorParameterValue* Parameter = ParameterGroup.Parameters[ParamIdx];
		UDEditorFontParameterValue* FontParam = Cast<UDEditorFontParameterValue>(Parameter);
		UDEditorScalarParameterValue* ScalarParam = Cast<UDEditorScalarParameterValue>(Parameter);
		UDEditorStaticComponentMaskParameterValue* CompMaskParam = Cast<UDEditorStaticComponentMaskParameterValue>(Parameter);
		UDEditorStaticSwitchParameterValue* SwitchParam = Cast<UDEditorStaticSwitchParameterValue>(Parameter);
		UDEditorTextureParameterValue* TextureParam = Cast<UDEditorTextureParameterValue>(Parameter);
		UDEditorVectorParameterValue* VectorParam = Cast<UDEditorVectorParameterValue>(Parameter);

		if( ScalarParam || SwitchParam || TextureParam || VectorParam || FontParam)
		{
			if (ScalarParam && ScalarParam->SliderMax > ScalarParam->SliderMin)
			{
				TSharedPtr<IPropertyHandle> ParameterValueProperty = ParameterProperty->GetChildHandle("ParameterValue");
				ParameterValueProperty->SetInstanceMetaData("UIMin",FString::Printf(TEXT("%f"), ScalarParam->SliderMin));
				ParameterValueProperty->SetInstanceMetaData("UIMax",FString::Printf(TEXT("%f"), ScalarParam->SliderMax));
			}

			CreateParameterValueWidget(Parameter, ParameterProperty, DetailGroup );
		}
		else if (CompMaskParam)
		{
			CreateMaskParameterValueWidget(Parameter, ParameterProperty, DetailGroup );
		}
		else
		{
			// Unsupported parameter type
			check(false);
		}
	}
}

void FMaterialInstanceParameterDetails::CreateParameterValueWidget(UDEditorParameterValue* Parameter, TSharedPtr<IPropertyHandle> ParameterProperty, IDetailGroup& DetailGroup )
{
	TSharedPtr<IPropertyHandle> ParameterValueProperty = ParameterProperty->GetChildHandle("ParameterValue");

	if( ParameterValueProperty->IsValidHandle() )
	{
		TAttribute<bool> IsParamEnabled = TAttribute<bool>::Create( TAttribute<bool>::FGetter::CreateSP( this, &FMaterialInstanceParameterDetails::IsOverriddenExpression, Parameter ) ) ;

		IDetailPropertyRow& PropertyRow = DetailGroup.AddPropertyRow( ParameterValueProperty.ToSharedRef() );

		FIsResetToDefaultVisible IsResetVisible = FIsResetToDefaultVisible::CreateSP(this, &FMaterialInstanceParameterDetails::ShouldShowResetToDefault, Parameter);
		FResetToDefaultHandler ResetHandler = FResetToDefaultHandler::CreateSP(this, &FMaterialInstanceParameterDetails::ResetToDefault, Parameter);
		FResetToDefaultOverride ResetOverride = FResetToDefaultOverride::Create(IsResetVisible, ResetHandler);

		PropertyRow
		.DisplayName( FText::FromName(Parameter->ParameterName) )
		.ToolTip( GetParameterExpressionDescription(Parameter) )
		.EditCondition( IsParamEnabled, FOnBooleanValueChanged::CreateSP( this, &FMaterialInstanceParameterDetails::OnOverrideParameter, Parameter ) )
		.Visibility( TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FMaterialInstanceParameterDetails::ShouldShowExpression, Parameter)) )
		// Handle reset to default manually
		.OverrideResetToDefault(ResetOverride);
	}
}

void FMaterialInstanceParameterDetails::CreateMaskParameterValueWidget(UDEditorParameterValue* Parameter, TSharedPtr<IPropertyHandle> ParameterProperty, IDetailGroup& DetailGroup )
{
	TSharedPtr<IPropertyHandle> ParameterValueProperty = ParameterProperty->GetChildHandle("ParameterValue");
	TSharedPtr<IPropertyHandle> RMaskProperty = ParameterValueProperty->GetChildHandle("R");
	TSharedPtr<IPropertyHandle> GMaskProperty = ParameterValueProperty->GetChildHandle("G");
	TSharedPtr<IPropertyHandle> BMaskProperty = ParameterValueProperty->GetChildHandle("B");
	TSharedPtr<IPropertyHandle> AMaskProperty = ParameterValueProperty->GetChildHandle("A");

	if( ParameterValueProperty->IsValidHandle() )
	{
		TAttribute<bool> IsParamEnabled = TAttribute<bool>::Create( TAttribute<bool>::FGetter::CreateSP( this, &FMaterialInstanceParameterDetails::IsOverriddenExpression, Parameter ) ) ;

		IDetailPropertyRow& PropertyRow = DetailGroup.AddPropertyRow( ParameterValueProperty.ToSharedRef() );
		PropertyRow.EditCondition( IsParamEnabled, FOnBooleanValueChanged::CreateSP( this, &FMaterialInstanceParameterDetails::OnOverrideParameter, Parameter ) );
		// Handle reset to default manually
		PropertyRow.OverrideResetToDefault(FResetToDefaultOverride::Create(FResetToDefaultHandler::CreateSP(this, &FMaterialInstanceParameterDetails::ResetToDefault, Parameter)));
		PropertyRow.Visibility( TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FMaterialInstanceParameterDetails::ShouldShowExpression, Parameter)) );

		const FText ParameterName = FText::FromName(Parameter->ParameterName); 

		FDetailWidgetRow& CustomWidget = PropertyRow.CustomWidget();
		CustomWidget
		.FilterString( ParameterName )
		.NameContent()
		[
			SNew(STextBlock)
			.Text( ParameterName )
			.ToolTipText( GetParameterExpressionDescription(Parameter) )
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		]
		.ValueContent()
		.MaxDesiredWidth(200.0f)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.AutoWidth()
				[
					RMaskProperty->CreatePropertyNameWidget( FText::GetEmpty(), FText::GetEmpty(), false )
				]
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.AutoWidth()
				[
					RMaskProperty->CreatePropertyValueWidget()
				]
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.Padding( FMargin( 10.0f, 0.0f, 0.0f, 0.0f ) )
				.AutoWidth()
				[
					GMaskProperty->CreatePropertyNameWidget( FText::GetEmpty(), FText::GetEmpty(), false )
				]
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.AutoWidth()
				[
					GMaskProperty->CreatePropertyValueWidget()
				]
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.Padding( FMargin( 10.0f, 0.0f, 0.0f, 0.0f ) )
				.AutoWidth()
				[
					BMaskProperty->CreatePropertyNameWidget( FText::GetEmpty(), FText::GetEmpty(), false )
				]
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.AutoWidth()
				[
					BMaskProperty->CreatePropertyValueWidget()
				]
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.Padding( FMargin( 10.0f, 0.0f, 0.0f, 0.0f ) )
				.AutoWidth()
				[
					AMaskProperty->CreatePropertyNameWidget( FText::GetEmpty(), FText::GetEmpty(), false )
				]
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.AutoWidth()
				[
					AMaskProperty->CreatePropertyValueWidget()
				]
			]
		];
	}
}

bool FMaterialInstanceParameterDetails::IsVisibleExpression(UDEditorParameterValue* Parameter)
{
	return MaterialEditorInstance->VisibleExpressions.Contains(Parameter->ExpressionId);
}

EVisibility FMaterialInstanceParameterDetails::ShouldShowExpression(UDEditorParameterValue* Parameter) const
{
	bool bShowHidden = true;

	ShowHiddenDelegate.ExecuteIfBound(bShowHidden);

	return (bShowHidden || MaterialEditorInstance->VisibleExpressions.Contains(Parameter->ExpressionId))? EVisibility::Visible: EVisibility::Collapsed;
}

bool FMaterialInstanceParameterDetails::IsOverriddenExpression(UDEditorParameterValue* Parameter)
{
	return Parameter->bOverride != 0;
}

void FMaterialInstanceParameterDetails::OnOverrideParameter(bool NewValue, class UDEditorParameterValue* Parameter)
{
	const FScopedTransaction Transaction( LOCTEXT( "OverrideParameter", "Override Parameter" ) );
	Parameter->Modify();
	Parameter->bOverride = NewValue;

	// Fire off a dummy event to the material editor instance, so it knows to update the material, then refresh the viewports.
	FPropertyChangedEvent OverrideEvent(NULL);
	MaterialEditorInstance->PostEditChangeProperty( OverrideEvent );
	FEditorSupportDelegates::RedrawAllViewports.Broadcast();
}

bool FMaterialInstanceParameterDetails::OnShouldSetAsset(const FAssetData& AssetData) const
{
	UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(AssetData.GetAsset());

	if (MaterialInstance != nullptr)
	{
		bool bIsChild = MaterialInstance->IsChildOf(MaterialEditorInstance->SourceInstance);
		if (bIsChild)
		{
			FMessageDialog::Open(
				EAppMsgType::Ok,
				FText::Format(LOCTEXT("CannotSetExistingChildAsParent", "Cannot set {0} as a parent as it is already a child of this material instance."), FText::FromName(AssetData.AssetName)));
		}
		return !bIsChild;
	}

	return true;
}

FText FMaterialInstanceParameterDetails::GetParameterExpressionDescription(UDEditorParameterValue* Parameter) const
{
	UMaterial* BaseMaterial = MaterialEditorInstance->SourceInstance->GetMaterial();
	if ( BaseMaterial )
	{
		UMaterialExpression* MaterialExpression = BaseMaterial->FindExpressionByGUID<UMaterialExpression>(Parameter->ExpressionId);

		if (MaterialExpression)
		{
			return FText::FromString(MaterialExpression->Desc);
		}
	}

	return FText::GetEmpty();
}

void FMaterialInstanceParameterDetails::ResetToDefault(TSharedPtr<IPropertyHandle> PropertyHandle, class UDEditorParameterValue* Parameter)
{
	const FScopedTransaction Transaction( LOCTEXT( "ResetToDefault", "Reset To Default" ) );
	Parameter->Modify();
	FName ParameterName = Parameter->ParameterName;
	
	UDEditorFontParameterValue* FontParam = Cast<UDEditorFontParameterValue>(Parameter);
	UDEditorScalarParameterValue* ScalarParam = Cast<UDEditorScalarParameterValue>(Parameter);
	UDEditorStaticComponentMaskParameterValue* CompMaskParam = Cast<UDEditorStaticComponentMaskParameterValue>(Parameter);
	UDEditorStaticSwitchParameterValue* SwitchParam = Cast<UDEditorStaticSwitchParameterValue>(Parameter);
	UDEditorTextureParameterValue* TextureParam = Cast<UDEditorTextureParameterValue>(Parameter);
	UDEditorVectorParameterValue* VectorParam = Cast<UDEditorVectorParameterValue>(Parameter);

	if (ScalarParam)
	{
		float OutValue;
		if (MaterialEditorInstance->Parent->GetScalarParameterValue(ParameterName, OutValue))
		{
			ScalarParam->ParameterValue = OutValue;
			MaterialEditorInstance->CopyToSourceInstance();
		}
	}
	else if (FontParam)
	{
		UFont* OutFontValue;
		int32 OutFontPage;
		if (MaterialEditorInstance->Parent->GetFontParameterValue(ParameterName, OutFontValue,OutFontPage))
		{
			FontParam->ParameterValue.FontValue = OutFontValue;
			FontParam->ParameterValue.FontPage = OutFontPage;
			MaterialEditorInstance->CopyToSourceInstance();
		}
	}
	else if (TextureParam)
	{
		UTexture* OutValue;
		if (MaterialEditorInstance->Parent->GetTextureParameterValue(ParameterName, OutValue))
		{
			TextureParam->ParameterValue = OutValue;
			MaterialEditorInstance->CopyToSourceInstance();
		}
	}
	else if (VectorParam)
	{
		FLinearColor OutValue;
		if (MaterialEditorInstance->Parent->GetVectorParameterValue(ParameterName, OutValue))
		{
			VectorParam->ParameterValue = OutValue;
			MaterialEditorInstance->CopyToSourceInstance();
		}
	}
	else if (SwitchParam)
	{
		bool OutValue;
		FGuid TempGuid(0,0,0,0);
		if (MaterialEditorInstance->Parent->GetStaticSwitchParameterValue(ParameterName, OutValue, TempGuid))
		{
			SwitchParam->ParameterValue = OutValue;
			MaterialEditorInstance->CopyToSourceInstance();
		}
	}
	else if (CompMaskParam)
	{
		bool OutValue[4];
		FGuid TempGuid(0,0,0,0);
		if (MaterialEditorInstance->Parent->GetStaticComponentMaskParameterValue(ParameterName, OutValue[0], OutValue[1], OutValue[2], OutValue[3], TempGuid))
		{
			CompMaskParam->ParameterValue.R = OutValue[0];
			CompMaskParam->ParameterValue.G = OutValue[1];
			CompMaskParam->ParameterValue.B = OutValue[2];
			CompMaskParam->ParameterValue.A = OutValue[3];
			MaterialEditorInstance->CopyToSourceInstance();
		}
	}
}

bool FMaterialInstanceParameterDetails::ShouldShowResetToDefault(TSharedPtr<IPropertyHandle> PropertyHandle, class UDEditorParameterValue* Parameter)
{
	FName ParameterName = Parameter->ParameterName;

	UDEditorFontParameterValue* FontParam = Cast<UDEditorFontParameterValue>(Parameter);
	UDEditorScalarParameterValue* ScalarParam = Cast<UDEditorScalarParameterValue>(Parameter);
	UDEditorStaticComponentMaskParameterValue* CompMaskParam = Cast<UDEditorStaticComponentMaskParameterValue>(Parameter);
	UDEditorStaticSwitchParameterValue* SwitchParam = Cast<UDEditorStaticSwitchParameterValue>(Parameter);
	UDEditorTextureParameterValue* TextureParam = Cast<UDEditorTextureParameterValue>(Parameter);
	UDEditorVectorParameterValue* VectorParam = Cast<UDEditorVectorParameterValue>(Parameter);

	if (ScalarParam)
	{
		float OutValue;
		if (MaterialEditorInstance->Parent->GetScalarParameterValue(ParameterName, OutValue))
		{
			if (ScalarParam->ParameterValue != OutValue)
			{
				return true;
			}
		}
	}
	else if (FontParam)
	{
		UFont* OutFontValue;
		int32 OutFontPage;
		if (MaterialEditorInstance->Parent->GetFontParameterValue(ParameterName, OutFontValue, OutFontPage))
		{
			if (FontParam->ParameterValue.FontValue != OutFontValue ||
				FontParam->ParameterValue.FontPage != OutFontPage)
			{
				return true;
			}
		}
	}
	else if (TextureParam)
	{
		UTexture* OutValue;
		if (MaterialEditorInstance->Parent->GetTextureParameterValue(ParameterName, OutValue))
		{
			if (TextureParam->ParameterValue != OutValue)
			{
				return true;
			}
		}
	}
	else if (VectorParam)
	{
		FLinearColor OutValue;
		if (MaterialEditorInstance->Parent->GetVectorParameterValue(ParameterName, OutValue))
		{
			if (VectorParam->ParameterValue != OutValue)
			{
				return true;
			}
		}
	}
	else if (SwitchParam)
	{
		bool OutValue;
		FGuid TempGuid(0, 0, 0, 0);
		if (MaterialEditorInstance->Parent->GetStaticSwitchParameterValue(ParameterName, OutValue, TempGuid))
		{
			if (SwitchParam->ParameterValue != OutValue)
			{
				return true;
			}
		}
	}
	else if (CompMaskParam)
	{
		bool OutValue[4];
		FGuid TempGuid(0, 0, 0, 0);
		if (MaterialEditorInstance->Parent->GetStaticComponentMaskParameterValue(ParameterName, OutValue[0], OutValue[1], OutValue[2], OutValue[3], TempGuid))
		{
			if (CompMaskParam->ParameterValue.R != OutValue[0] ||
			CompMaskParam->ParameterValue.G != OutValue[1] ||
			CompMaskParam->ParameterValue.B != OutValue[2] ||
			CompMaskParam->ParameterValue.A != OutValue[3])
			{
				return true;
			}
		}
	}
	return false;
}

EVisibility FMaterialInstanceParameterDetails::ShouldShowMaterialRefractionSettings() const
{
	return (MaterialEditorInstance->SourceInstance->GetMaterial()->bUsesDistortion && IsTranslucentBlendMode(MaterialEditorInstance->SourceInstance->GetBlendMode())) ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility FMaterialInstanceParameterDetails::ShouldShowSubsurfaceProfile() const
{
	EMaterialShadingModel Model = MaterialEditorInstance->SourceInstance->GetShadingModel();

	return (Model == MSM_SubsurfaceProfile) ? EVisibility::Visible : EVisibility::Collapsed;
}


void FMaterialInstanceParameterDetails::CreateBasePropertyOverrideWidgets(IDetailLayoutBuilder& DetailLayout)
{
	IDetailCategoryBuilder& DetailCategory = DetailLayout.EditCategory(NAME_None);

	static FName GroupName(TEXT("BasePropertyOverrideGroup"));
	IDetailGroup& BasePropertyOverrideGroup = DetailCategory.AddGroup(GroupName, LOCTEXT("BasePropertyOverrideGroup", "Material Property Overrides"), false, false);

	TAttribute<bool> IsOverrideOpacityClipMaskValueEnabled = TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FMaterialInstanceParameterDetails::OverrideOpacityClipMaskValueEnabled));
	TAttribute<bool> IsOverrideBlendModeEnabled = TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FMaterialInstanceParameterDetails::OverrideBlendModeEnabled));
	TAttribute<bool> IsOverrideShadingModelEnabled = TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FMaterialInstanceParameterDetails::OverrideShadingModelEnabled));
	TAttribute<bool> IsOverrideTwoSidedEnabled = TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FMaterialInstanceParameterDetails::OverrideTwoSidedEnabled));
	TAttribute<bool> IsOverrideDitheredLODTransitionEnabled = TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FMaterialInstanceParameterDetails::OverrideDitheredLODTransitionEnabled));

	TSharedRef<IPropertyHandle> BasePropertyOverridePropery = DetailLayout.GetProperty("BasePropertyOverrides");
	TSharedPtr<IPropertyHandle> OpacityClipMaskValueProperty = BasePropertyOverridePropery->GetChildHandle("OpacityMaskClipValue");
	TSharedPtr<IPropertyHandle> BlendModeProperty = BasePropertyOverridePropery->GetChildHandle("BlendMode");
	TSharedPtr<IPropertyHandle> ShadingModelProperty = BasePropertyOverridePropery->GetChildHandle("ShadingModel");
	TSharedPtr<IPropertyHandle> TwoSidedProperty = BasePropertyOverridePropery->GetChildHandle("TwoSided");
	TSharedPtr<IPropertyHandle> DitheredLODTransitionProperty = BasePropertyOverridePropery->GetChildHandle("DitheredLODTransition");

	IDetailPropertyRow& OpacityClipMaskValuePropertyRow = BasePropertyOverrideGroup.AddPropertyRow(OpacityClipMaskValueProperty.ToSharedRef());
	OpacityClipMaskValuePropertyRow
		.DisplayName(OpacityClipMaskValueProperty->GetPropertyDisplayName())
		.ToolTip(OpacityClipMaskValueProperty->GetToolTipText())
		.EditCondition(IsOverrideOpacityClipMaskValueEnabled, FOnBooleanValueChanged::CreateSP(this, &FMaterialInstanceParameterDetails::OnOverrideOpacityClipMaskValueChanged));

	IDetailPropertyRow& BlendModePropertyRow = BasePropertyOverrideGroup.AddPropertyRow(BlendModeProperty.ToSharedRef());
	BlendModePropertyRow
		.DisplayName(BlendModeProperty->GetPropertyDisplayName())
		.ToolTip(BlendModeProperty->GetToolTipText())
		.EditCondition(IsOverrideBlendModeEnabled, FOnBooleanValueChanged::CreateSP(this, &FMaterialInstanceParameterDetails::OnOverrideBlendModeChanged));

	IDetailPropertyRow& ShadingModelPropertyRow = BasePropertyOverrideGroup.AddPropertyRow(ShadingModelProperty.ToSharedRef());
	ShadingModelPropertyRow
		.DisplayName(ShadingModelProperty->GetPropertyDisplayName())
		.ToolTip(ShadingModelProperty->GetToolTipText())
		.EditCondition(IsOverrideShadingModelEnabled, FOnBooleanValueChanged::CreateSP(this, &FMaterialInstanceParameterDetails::OnOverrideShadingModelChanged));

	IDetailPropertyRow& TwoSidedPropertyRow = BasePropertyOverrideGroup.AddPropertyRow(TwoSidedProperty.ToSharedRef());
	TwoSidedPropertyRow
		.DisplayName(TwoSidedProperty->GetPropertyDisplayName())
		.ToolTip(TwoSidedProperty->GetToolTipText())
		.EditCondition(IsOverrideTwoSidedEnabled, FOnBooleanValueChanged::CreateSP(this, &FMaterialInstanceParameterDetails::OnOverrideTwoSidedChanged));

	IDetailPropertyRow& DitheredLODTransitionPropertyRow = BasePropertyOverrideGroup.AddPropertyRow(DitheredLODTransitionProperty.ToSharedRef());
	DitheredLODTransitionPropertyRow
		.DisplayName(DitheredLODTransitionProperty->GetPropertyDisplayName())
		.ToolTip(DitheredLODTransitionProperty->GetToolTipText())
		.EditCondition(IsOverrideDitheredLODTransitionEnabled, FOnBooleanValueChanged::CreateSP(this, &FMaterialInstanceParameterDetails::OnOverrideDitheredLODTransitionChanged));

	// NVCHANGE_BEGIN: Add VXGI
#define VXGI_OVERRIDE_ATTR(NAME, OVERRIDEPROP) { \
	TAttribute<bool> Attribute = TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FMaterialInstanceParameterDetails::NAME)); \
	TSharedPtr<IPropertyHandle> Property = BasePropertyOverridePropery->GetChildHandle(OVERRIDEPROP); \
	BasePropertyOverrideGroup.AddPropertyRow(Property.ToSharedRef()).DisplayName(Property->GetPropertyDisplayName()).ToolTip(Property->GetToolTipText()) \
		.EditCondition(Attribute, FOnBooleanValueChanged::CreateSP(this, &FMaterialInstanceParameterDetails::On##NAME)); \
	}

	VXGI_OVERRIDE_ATTR(OverrideIsVxgiConeTracingEnabled, "bVxgiConeTracingEnabled");
	VXGI_OVERRIDE_ATTR(OverrideIsUsedWithVxgiVoxelizationEnabled, "bUsedWithVxgiVoxelization");
	VXGI_OVERRIDE_ATTR(OverrideIsVxgiOmniDirectionalEnabled, "bVxgiOmniDirectional");
	VXGI_OVERRIDE_ATTR(OverrideIsVxgiProportionalEmittanceEnabled, "bVxgiProportionalEmittance");
	VXGI_OVERRIDE_ATTR(OverrideGetVxgiAllowTesselationDuringVoxelizationEnabled, "bVxgiAllowTesselationDuringVoxelization");
	VXGI_OVERRIDE_ATTR(OverrideGetVxgiVoxelizationThicknessEnabled, "VxgiVoxelizationThickness");
	VXGI_OVERRIDE_ATTR(OverrideGetVxgiOpacityNoiseScaleBiasEnabled, "VxgiOpacityNoiseScaleBias");
	VXGI_OVERRIDE_ATTR(OverrideGetVxgiCoverageSupersamplingEnabled, "bVxgiCoverageSupersampling");
	VXGI_OVERRIDE_ATTR(OverrideGetVxgiMaterialSamplingRateEnabled, "VxgiMaterialSamplingRate");

#undef VXGI_OVERRIDE_ATTR
	// NVCHANGE_END: Add VXGI
}

bool FMaterialInstanceParameterDetails::OverrideOpacityClipMaskValueEnabled() const
{
	return MaterialEditorInstance->BasePropertyOverrides.bOverride_OpacityMaskClipValue;
}

bool FMaterialInstanceParameterDetails::OverrideBlendModeEnabled() const
{
	return MaterialEditorInstance->BasePropertyOverrides.bOverride_BlendMode;
}

bool FMaterialInstanceParameterDetails::OverrideShadingModelEnabled() const
{
	return MaterialEditorInstance->BasePropertyOverrides.bOverride_ShadingModel;
}

bool FMaterialInstanceParameterDetails::OverrideTwoSidedEnabled() const
{
	return MaterialEditorInstance->BasePropertyOverrides.bOverride_TwoSided;
}

bool FMaterialInstanceParameterDetails::OverrideDitheredLODTransitionEnabled() const
{
	return MaterialEditorInstance->BasePropertyOverrides.bOverride_DitheredLODTransition;
}

void FMaterialInstanceParameterDetails::OnOverrideOpacityClipMaskValueChanged(bool NewValue)
{
	MaterialEditorInstance->BasePropertyOverrides.bOverride_OpacityMaskClipValue = NewValue;
	MaterialEditorInstance->PostEditChange();
	FEditorSupportDelegates::RedrawAllViewports.Broadcast();
}

void FMaterialInstanceParameterDetails::OnOverrideBlendModeChanged(bool NewValue)
{
	MaterialEditorInstance->BasePropertyOverrides.bOverride_BlendMode = NewValue;
	MaterialEditorInstance->PostEditChange();
	FEditorSupportDelegates::RedrawAllViewports.Broadcast();
}

void FMaterialInstanceParameterDetails::OnOverrideShadingModelChanged(bool NewValue)
{
	MaterialEditorInstance->BasePropertyOverrides.bOverride_ShadingModel = NewValue;
	MaterialEditorInstance->PostEditChange();
	FEditorSupportDelegates::RedrawAllViewports.Broadcast();
}

void FMaterialInstanceParameterDetails::OnOverrideTwoSidedChanged(bool NewValue)
{
	MaterialEditorInstance->BasePropertyOverrides.bOverride_TwoSided = NewValue;
	MaterialEditorInstance->PostEditChange();
	FEditorSupportDelegates::RedrawAllViewports.Broadcast();
}

void FMaterialInstanceParameterDetails::OnOverrideDitheredLODTransitionChanged(bool NewValue)
{
	MaterialEditorInstance->BasePropertyOverrides.bOverride_DitheredLODTransition = NewValue;
	MaterialEditorInstance->PostEditChange();
	FEditorSupportDelegates::RedrawAllViewports.Broadcast();
}

// NVCHANGE_BEGIN: Add VXGI
void FMaterialInstanceParameterDetails::OnOverrideIsVxgiConeTracingEnabled(bool NewValue)
{
	MaterialEditorInstance->BasePropertyOverrides.bOverride_VxgiConeTracingEnabled = NewValue;
	MaterialEditorInstance->PostEditChange();
	FEditorSupportDelegates::RedrawAllViewports.Broadcast();
}
void FMaterialInstanceParameterDetails::OnOverrideIsUsedWithVxgiVoxelizationEnabled(bool NewValue)
{
	MaterialEditorInstance->BasePropertyOverrides.bOverride_UsedWithVxgiVoxelization = NewValue;
	MaterialEditorInstance->PostEditChange();
	FEditorSupportDelegates::RedrawAllViewports.Broadcast();
}
void FMaterialInstanceParameterDetails::OnOverrideIsVxgiOmniDirectionalEnabled(bool NewValue)
{
	MaterialEditorInstance->BasePropertyOverrides.bOverride_VxgiOmniDirectional = NewValue;
	MaterialEditorInstance->PostEditChange();
	FEditorSupportDelegates::RedrawAllViewports.Broadcast();
}
void FMaterialInstanceParameterDetails::OnOverrideIsVxgiProportionalEmittanceEnabled(bool NewValue)
{
	MaterialEditorInstance->BasePropertyOverrides.bOverride_VxgiProportionalEmittance = NewValue;
	MaterialEditorInstance->PostEditChange();
	FEditorSupportDelegates::RedrawAllViewports.Broadcast();
}
void FMaterialInstanceParameterDetails::OnOverrideGetVxgiAllowTesselationDuringVoxelizationEnabled(bool NewValue)
{
	MaterialEditorInstance->BasePropertyOverrides.bOverride_VxgiAllowTesselationDuringVoxelization = NewValue;
	MaterialEditorInstance->PostEditChange();
	FEditorSupportDelegates::RedrawAllViewports.Broadcast();
}
void FMaterialInstanceParameterDetails::OnOverrideGetVxgiVoxelizationThicknessEnabled(bool NewValue)
{
	MaterialEditorInstance->BasePropertyOverrides.bOverride_VxgiVoxelizationThickness = NewValue;
	MaterialEditorInstance->PostEditChange();
	FEditorSupportDelegates::RedrawAllViewports.Broadcast();
}
void FMaterialInstanceParameterDetails::OnOverrideGetVxgiOpacityNoiseScaleBiasEnabled(bool NewValue)
{
	MaterialEditorInstance->BasePropertyOverrides.bOverride_VxgiOpacityNoiseScaleBias = NewValue;
	MaterialEditorInstance->PostEditChange();
	FEditorSupportDelegates::RedrawAllViewports.Broadcast();
}
void FMaterialInstanceParameterDetails::OnOverrideGetVxgiCoverageSupersamplingEnabled(bool NewValue)
{
	MaterialEditorInstance->BasePropertyOverrides.bOverride_VxgiCoverageSupersampling = NewValue;
	MaterialEditorInstance->PostEditChange();
	FEditorSupportDelegates::RedrawAllViewports.Broadcast();
}
void FMaterialInstanceParameterDetails::OnOverrideGetVxgiMaterialSamplingRateEnabled(bool NewValue)
{
	MaterialEditorInstance->BasePropertyOverrides.bOverride_VxgiMaterialSamplingRate = NewValue;
	MaterialEditorInstance->PostEditChange();
	FEditorSupportDelegates::RedrawAllViewports.Broadcast();
}
// NVCHANGE_END: Add VXGI

#undef LOCTEXT_NAMESPACE

