// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "NiagaraEmitterDetailsCustomization.h"
#include "NiagaraCommon.h"
#include "NiagaraEmitter.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraComponent.h"
#include "NiagaraEditorStyle.h"

#include "DetailWidgetRow.h"
#include "PropertyHandle.h"
#include "DetailLayoutBuilder.h"
#include "IDetailGroup.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailPropertyRow.h"
#include "DetailCategoryBuilder.h"
#include "PropertyCustomizationHelpers.h"
#include "ScopedTransaction.h"

#include "SBorder.h"
#include "STextBlock.h"
#include "SButton.h"


#define LOCTEXT_NAMESPACE "NiagaraSystemEditor"
#if 0
class SMaterialUsageWarning : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMaterialUsageWarning)	{}
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, UNiagaraEmitter* InEmitterProperties)
	{
		EmitterProperties = InEmitterProperties;
		ChildSlot
		[
			SNew(SBorder)
			.BorderImage(FNiagaraEditorStyle::Get().GetBrush("NiagaraEditor.MaterialWarningBorder"))
			.BorderBackgroundColor(FLinearColor(.6f, 0.0f, 0.0f, 1.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding(3, 0, 0, 0)
				[
					SNew(STextBlock)
					.Text(this, &SMaterialUsageWarning::GetInvalidMessage)
					.AutoWrapText(true)
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				.Padding(5, 0, 0, 0)
				.AutoWidth()
				[
					SNew(SButton)
					.OnClicked(this, &SMaterialUsageWarning::OnFixNowClicked)
					.ToolTipText(NSLOCTEXT("MaterialUsageWarning", "FixButtonLabelToolTip", "Fix the material issue now by modifying the material asset."))
					.Content()
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("MaterialUsageWarning", "FixButtonLabel", "Fix Now"))
					]
				]
			]
		];
	}

	EVisibility GetVisibility() const
	{
		bool bMaterialIsValid =
			EmitterProperties->RendererProperties == nullptr ||
			EmitterProperties->Material == nullptr ||
			EmitterProperties->RendererProperties->IsMaterialValidForRenderer(EmitterProperties->Material, InvalidMessage);

		return bMaterialIsValid
			? EVisibility::Collapsed
			: EVisibility::Visible;
	}

private:
	FText GetInvalidMessage() const
	{
		return InvalidMessage;
	}

	FReply OnFixNowClicked()
	{
		FScopedTransaction ScopedTransaction(NSLOCTEXT("EmitterDetailsCustomization", "FixMaterialTransaction", "Fix material for emitter"));
		EmitterProperties->RendererProperties->FixMaterial(EmitterProperties->Material);
		return FReply::Handled();
	}

private:
	UNiagaraEmitter* EmitterProperties;
	mutable FText InvalidMessage;
};
#endif

TSharedRef<IDetailCustomization> FNiagaraEmitterDetails::MakeInstance(TWeakObjectPtr<UNiagaraEmitter> EmitterProperties)
{
	return MakeShareable(new FNiagaraEmitterDetails(EmitterProperties));
}

FNiagaraEmitterDetails::FNiagaraEmitterDetails(TWeakObjectPtr<UNiagaraEmitter> EmitterProperties)
: EmitterProps(EmitterProperties)
{
}

void FNiagaraEmitterDetails::CustomizeDetails(IDetailLayoutBuilder& InDetailLayout)
{
	DetailLayout = &InDetailLayout;
	FName EmitterCategoryName = TEXT("Emitter");
	IDetailCategoryBuilder& EmitterCategory = DetailLayout->EditCategory(EmitterCategoryName);
	
	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
	InDetailLayout.GetObjectsBeingCustomized(ObjectsBeingCustomized);
	UNiagaraEmitter* EmitterProperties = CastChecked<UNiagaraEmitter>(ObjectsBeingCustomized[0].Get());

#if 0
	IDetailCategoryBuilder& RenderCategory = DetailLayout->EditCategory(TEXT("Render"));
	TSharedRef<SMaterialUsageWarning> MaterialWarning = SNew(SMaterialUsageWarning, CastChecked<UNiagaraEmitter>(EmitterProperties));
	RenderCategory.AddCustomRow(FText())
		.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(MaterialWarning, &SMaterialUsageWarning::GetVisibility)))
		[
			MaterialWarning
		];
#endif
	/*
	TSharedRef<IPropertyHandle> UpdateScriptProps = DetailLayout->GetProperty(GET_MEMBER_NAME_CHECKED(UNiagaraEmitter, UpdateScriptProps));
	TSharedRef<IPropertyHandle> SpawnScriptProps = DetailLayout->GetProperty(GET_MEMBER_NAME_CHECKED(UNiagaraEmitter, SpawnScriptProps));

	BuildScriptProperties(SpawnScriptProps, TEXT("Spawn Script"), LOCTEXT("SpawnScriptDisplayName", "Spawn Script"));
	BuildScriptProperties(UpdateScriptProps, TEXT("Update Script"), LOCTEXT("UpdateScriptDisplayName", "Update Script"));
	*/
}

void FNiagaraEmitterDetails::BuildScriptProperties(TSharedRef<IPropertyHandle> ScriptPropsHandle, FName Name, FText DisplayName)
{
//	/*
//	DetailLayout->HideProperty(ScriptPropsHandle);
//
//	IDetailCategoryBuilder& ScriptCategory = DetailLayout->EditCategory(Name, DisplayName);
//
//	//Script
//	TSharedPtr<IPropertyHandle> ScriptHandle = ScriptPropsHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FNiagaraEmitterScriptProperties, Script));
//	ScriptCategory.AddProperty(ScriptHandle);
//
//	//Constants
//	/*
//	bool bGenerateHeader = false;
//	bool bDisplayResetToDefault = false;
//	TSharedPtr<IPropertyHandle> Constants = ScriptPropsHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FNiagaraEmitterScriptProperties, ExternalConstants));
//	DetailLayout->HideProperty(Constants);
//
//	TSharedPtr<IPropertyHandle> Uniforms = Constants->GetChildHandle(GET_MEMBER_NAME_CHECKED(FNiagaraConstants, Uniforms));
//	TSharedRef<FDetailArrayBuilder> ConstantsBuilder = MakeShareable(new FDetailArrayBuilder(Uniforms.ToSharedRef(), bGenerateHeader, bDisplayResetToDefault));
//	ConstantsBuilder->OnGenerateArrayElementWidget(FOnGenerateArrayElementWidget::CreateSP(this, &FNiagaraEmitterDetails::OnGenerateConstantEntry));
//	ScriptCategory.AddCustomBuilder(ConstantsBuilder);
//	*/
//
//	//Scalar Constants.
//	/*
//	TSharedPtr<IPropertyHandle> ScalarConstants = Constants->GetChildHandle(GET_MEMBER_NAME_CHECKED(FNiagaraConstants, ScalarConstants));
//	TSharedRef<FDetailArrayBuilder> ScalarConstantsBuilder = MakeShareable(new FDetailArrayBuilder(ScalarConstants.ToSharedRef(), bGenerateHeader, bDisplayResetToDefault));
//	ScalarConstantsBuilder->OnGenerateArrayElementWidget(FOnGenerateArrayElementWidget::CreateSP(this, &FNiagaraEmitterDetails::OnGenerateScalarConstantEntry));
//	ScriptCategory.AddCustomBuilder(ScalarConstantsBuilder);
//	//Vector Constants.
//	TSharedPtr<IPropertyHandle> VectorConstants = Constants->GetChildHandle(GET_MEMBER_NAME_CHECKED(FNiagaraConstants, VectorConstants));
//	TSharedRef<FDetailArrayBuilder> VectorConstantsBuilder = MakeShareable(new FDetailArrayBuilder(VectorConstants.ToSharedRef(), bGenerateHeader, bDisplayResetToDefault));
//	VectorConstantsBuilder->OnGenerateArrayElementWidget(FOnGenerateArrayElementWidget::CreateSP(this, &FNiagaraEmitterDetails::OnGenerateVectorConstantEntry));
//	ScriptCategory.AddCustomBuilder(VectorConstantsBuilder);
//	*/
//	//Events
//	//bGenerateHeader = true;
//	//Generators
//// 	TSharedPtr<IPropertyHandle> EventGenerators = ScriptPropsHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FNiagaraEmitterScriptProperties, EventGenerators));
//// 	TSharedRef<FDetailArrayBuilder> EventGeneratorsBuilder = MakeShareable(new FDetailArrayBuilder(EventGenerators.ToSharedRef(), bGenerateHeader, bDisplayResetToDefault));
//// 	EventGeneratorsBuilder->OnGenerateArrayElementWidget(FOnGenerateArrayElementWidget::CreateSP(this, &FNiagaraEmitterDetails::OnGenerateEventGeneratorEntry));
//// 	ScriptCategory.AddCustomBuilder(EventGeneratorsBuilder);
//// 	//Receivers
//// 	TSharedPtr<IPropertyHandle> EventReceivers = ScriptPropsHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FNiagaraEmitterScriptProperties, EventReceivers));
//// 	TSharedRef<FDetailArrayBuilder> EventReceiversBuilder = MakeShareable(new FDetailArrayBuilder(EventReceivers.ToSharedRef(), bGenerateHeader, bDisplayResetToDefault));
//// 	EventReceiversBuilder->OnGenerateArrayElementWidget(FOnGenerateArrayElementWidget::CreateSP(this, &FNiagaraEmitterDetails::OnGenerateEventReceiverEntry));
//// 	ScriptCategory.AddCustomBuilder(EventReceiversBuilder);
//	
}



void FNiagaraEmitterDetails::OnGenerateConstantEntry(TSharedRef<IPropertyHandle> ElementProperty, int32 ElementIndex, IDetailChildrenBuilder& ChildrenBuilder)
{
// 	TSharedPtr<IPropertyHandle> ValueProperty = ElementProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FNiagaraVariable, VarData));
// 	TSharedPtr<IPropertyHandle> NameProperty = ElementProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FNiagaraVariable, Name));
 	
// 	FName DisplayName;
// 	NameProperty->GetValue(DisplayName);

	//STOVEY. Not sure this is needed. System constants don't show up in the param UI anyway.
	//Don't display system constants
// 	if (UNiagaraComponent::FindSystemConstant(Var) == nullptr)
// 	{
//		ChildrenBuilder.AddChildProperty(ValueProperty.ToSharedRef()).DisplayName(FText::FromName(DisplayName));
//	}
}


void FNiagaraEmitterDetails::OnGenerateEventGeneratorEntry(TSharedRef<IPropertyHandle> ElementProperty, int32 ElementIndex, IDetailChildrenBuilder& ChildrenBuilder)
{
	TSharedPtr<IPropertyHandle> IDProperty = ElementProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FNiagaraEventGeneratorProperties, ID));
	TSharedPtr<IPropertyHandle> NameProperty = IDProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FNiagaraDataSetID, Name));

	FName DisplayName;
	NameProperty->GetValue(DisplayName);
	//IDetailPropertyRow& Row = ChildrenBuilder.AddChildProperty(ElementProperty).DisplayName(FText::FromName(DisplayName));

	IDetailGroup& GenGroup = ChildrenBuilder.AddGroup(DisplayName, FText::FromName(DisplayName));
	uint32 NumChildren = 0;
	if (ElementProperty->GetNumChildren(NumChildren) == FPropertyAccess::Success)
	{
		for (uint32 i = 0; i < NumChildren; ++i)
		{
			TSharedPtr<IPropertyHandle> Child = ElementProperty->GetChildHandle(i);
			//Dont add the ID. We just grab it's name for the name region of this property.
			if (Child.IsValid() && Child->GetProperty()->GetName() != GET_MEMBER_NAME_CHECKED(FNiagaraEventGeneratorProperties, ID).ToString())
			{
				GenGroup.AddPropertyRow(Child.ToSharedRef());
			}
		}
	}
}

void FNiagaraEmitterDetails::OnGenerateEventReceiverEntry(TSharedRef<IPropertyHandle> ElementProperty, int32 ElementIndex, IDetailChildrenBuilder& ChildrenBuilder)
{
	TSharedPtr<IPropertyHandle> NameProperty = ElementProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FNiagaraEventReceiverProperties, Name));

 	FName DisplayName;
 	NameProperty->GetValue(DisplayName);
// 	ChildrenBuilder.AddChildProperty(ElementProperty).DisplayName(FText::FromName(DisplayName));

	IDetailGroup& Group = ChildrenBuilder.AddGroup(DisplayName, FText::FromName(DisplayName));
	uint32 NumChildren = 0;
	if (ElementProperty->GetNumChildren(NumChildren) == FPropertyAccess::Success)
	{
		for (uint32 i = 0; i < NumChildren; ++i)
		{
			TSharedPtr<IPropertyHandle> Child = ElementProperty->GetChildHandle(i);
			//Dont add the ID. We just grab it's name for the name region of this property.
			if (Child.IsValid() && Child->GetProperty()->GetName() != GET_MEMBER_NAME_CHECKED(FNiagaraEventReceiverProperties, Name).ToString())
			{
				TSharedPtr<SWidget> NameWidget;
				TSharedPtr<SWidget> ValueWidget;
				FDetailWidgetRow DefaultDetailRow;
			
				IDetailPropertyRow& Row = Group.AddPropertyRow(Child.ToSharedRef());
				Row.GetDefaultWidgets(NameWidget, ValueWidget, DefaultDetailRow);
				Row.CustomWidget(true)
					.NameContent()
					[
						NameWidget.ToSharedRef()
					]
					.ValueContent()
					[
						ValueWidget.ToSharedRef()
					];
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE

