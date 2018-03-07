// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphNodeDetails.h"
#include "Modules/ModuleManager.h"
#include "UObject/UnrealType.h"
#include "Widgets/Text/STextBlock.h"
#include "BoneContainer.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimationAsset.h"
#include "Widgets/Layout/SSpacer.h"
#include "DetailWidgetRow.h"
#include "IDetailPropertyRow.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "IDetailsView.h"
#include "PropertyCustomizationHelpers.h"
#include "SlateOptMacros.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Animation/AnimInstance.h"
#include "Animation/EditorParentPlayerListObj.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Widgets/SToolTip.h"
#include "IDocumentation.h"
#include "ObjectEditorUtils.h"
#include "AnimGraphNode_Base.h"
#include "Widgets/Views/STreeView.h"
#include "BoneSelectionWidget.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Animation/BlendProfile.h"
#include "AnimGraphNode_AssetPlayerBase.h"
#include "BlendProfilePicker.h"
#include "ISkeletonEditorModule.h"
#include "EdGraph/EdGraph.h"
#include "BlueprintEditor.h"
#include "Animation/EditorAnimCurveBoneLinks.h"
#include "IEditableSkeleton.h"
#include "IDetailChildrenBuilder.h"
#include "SNumericEntryBox.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "Engine/SkeletalMeshSocket.h"

#define LOCTEXT_NAMESPACE "KismetNodeWithOptionalPinsDetails"

/////////////////////////////////////////////////////
// FAnimGraphNodeDetails 

TSharedRef<IDetailCustomization> FAnimGraphNodeDetails::MakeInstance()
{
	return MakeShareable(new FAnimGraphNodeDetails());
}

void FAnimGraphNodeDetails::CustomizeDetails(class IDetailLayoutBuilder& DetailBuilder)
{
	TArray< TWeakObjectPtr<UObject> > SelectedObjectsList = DetailBuilder.GetSelectedObjects();

	// Hide the pin options property; it's represented inline per-property instead
	IDetailCategoryBuilder& PinOptionsCategory = DetailBuilder.EditCategory("PinOptions");
	TSharedRef<IPropertyHandle> AvailablePins = DetailBuilder.GetProperty("ShowPinForProperties");
	DetailBuilder.HideProperty(AvailablePins);

	// get first animgraph nodes
	UAnimGraphNode_Base* AnimGraphNode = Cast<UAnimGraphNode_Base>(SelectedObjectsList[0].Get());
	if (AnimGraphNode == NULL)
	{
		return;
	}

	TargetSkeleton = AnimGraphNode->GetAnimBlueprint()->TargetSkeleton;
	TargetSkeletonName = FString::Printf(TEXT("%s'%s'"), *TargetSkeleton->GetClass()->GetName(), *TargetSkeleton->GetPathName());

	// Get the node property
	const UStructProperty* NodeProperty = AnimGraphNode->GetFNodeProperty();
	if (NodeProperty == NULL)
	{
		return;
	}

	// customize anim graph node's own details if needed
	AnimGraphNode->CustomizeDetails(DetailBuilder);

	// Hide the Node property as we are going to be adding its inner properties below
	TSharedRef<IPropertyHandle> NodePropertyHandle = DetailBuilder.GetProperty(NodeProperty->GetFName(), AnimGraphNode->GetClass());
	DetailBuilder.HideProperty(NodePropertyHandle);

	// Now customize each property in the pins array
	for (int CustomPinIndex = 0; CustomPinIndex < AnimGraphNode->ShowPinForProperties.Num(); ++CustomPinIndex)
	{
		const FOptionalPinFromProperty& OptionalPin = AnimGraphNode->ShowPinForProperties[CustomPinIndex];

		UProperty* TargetProperty = FindField<UProperty>(NodeProperty->Struct, OptionalPin.PropertyName);

		IDetailCategoryBuilder& CurrentCategory = DetailBuilder.EditCategory(FObjectEditorUtils::GetCategoryFName(TargetProperty));

		const FName TargetPropertyPath(*FString::Printf(TEXT("%s.%s"), *NodeProperty->GetName(), *TargetProperty->GetName()));
		TSharedRef<IPropertyHandle> TargetPropertyHandle = DetailBuilder.GetProperty(TargetPropertyPath, AnimGraphNode->GetClass() );

		// Not optional
		if (!OptionalPin.bCanToggleVisibility && OptionalPin.bShowPin)
		{
			// Always displayed as a pin, so hide the property
			DetailBuilder.HideProperty(TargetPropertyHandle);
			continue;
		}

		if(!TargetPropertyHandle->GetProperty())
		{
			continue;
		}

		// if customized, do not do anything
		if(TargetPropertyHandle->IsCustomized())
		{
			continue;
		}
		
		// sometimes because of order of customization
		// this gets called first for the node you'd like to customize
		// then the above statement won't work
		// so you can mark certain property to have meta data "CustomizeProperty"
		// which will trigger below statement
		if (OptionalPin.bPropertyIsCustomized)
		{
			continue;
		}

		IDetailPropertyRow& PropertyRow = CurrentCategory.AddProperty( TargetPropertyHandle );

		if (OptionalPin.bCanToggleVisibility)
		{
			TSharedPtr<SWidget> NameWidget; 
			TSharedPtr<SWidget> ValueWidget;
			FDetailWidgetRow Row;
			PropertyRow.GetDefaultWidgets( NameWidget, ValueWidget, Row );

			TSharedRef<SWidget> TempWidget = CreatePropertyWidget(TargetProperty, TargetPropertyHandle, AnimGraphNode->GetClass());
			ValueWidget = (TempWidget == SNullWidget::NullWidget) ? ValueWidget : TempWidget;

			const FName OptionalPinArrayEntryName(*FString::Printf(TEXT("ShowPinForProperties[%d].bShowPin"), CustomPinIndex));
			TSharedRef<IPropertyHandle> ShowHidePropertyHandle = DetailBuilder.GetProperty(OptionalPinArrayEntryName);

			ShowHidePropertyHandle->MarkHiddenByCustomization();

			const FText AsPinTooltip = LOCTEXT("AsPinTooltip", "Show this property as a pin on the node");

			TSharedRef<SWidget> ShowHidePropertyWidget = ShowHidePropertyHandle->CreatePropertyValueWidget();
			ShowHidePropertyWidget->SetToolTipText(AsPinTooltip);

			ValueWidget->SetVisibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FAnimGraphNodeDetails::GetVisibilityOfProperty, ShowHidePropertyHandle)));

			NameWidget = SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Fill)
				.FillWidth(1)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					[
						ShowHidePropertyWidget
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("AsPin", " (As pin) "))
						.Font(DetailBuilder.IDetailLayoutBuilder::GetDetailFont())
						.ToolTipText(AsPinTooltip)
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SSpacer)
					]
					+SHorizontalBox::Slot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					.FillWidth(1)
					.Padding(FMargin(0,0,4,0))
					[
						NameWidget.ToSharedRef()
					]
				];

			const bool bShowChildren = true;
			PropertyRow.CustomWidget(bShowChildren)
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
				ValueWidget.ToSharedRef()
			];
		}
	}
}

TSharedRef<SWidget> FAnimGraphNodeDetails::CreatePropertyWidget(UProperty* TargetProperty, TSharedRef<IPropertyHandle> TargetPropertyHandle, UClass* NodeClass)
{
	if(const UObjectPropertyBase* ObjectProperty = Cast<const UObjectPropertyBase>( TargetProperty ))
	{
		if(ObjectProperty->PropertyClass->IsChildOf(UAnimationAsset::StaticClass()))
		{
			bool bAllowClear = !(ObjectProperty->PropertyFlags & CPF_NoClear);

			return SNew(SObjectPropertyEntryBox)
				.PropertyHandle(TargetPropertyHandle)
				.AllowedClass(ObjectProperty->PropertyClass)
				.AllowClear(bAllowClear)
				.OnShouldFilterAsset(FOnShouldFilterAsset::CreateSP(this, &FAnimGraphNodeDetails::OnShouldFilterAnimAsset, NodeClass));
		}
		else if(ObjectProperty->PropertyClass->IsChildOf(UBlendProfile::StaticClass()) && TargetSkeleton)
		{
			TSharedPtr<IPropertyHandle> PropertyPtr(TargetPropertyHandle);

			UObject* PropertyValue = nullptr;
			TargetPropertyHandle->GetValue(PropertyValue);

			UBlendProfile* CurrentProfile = Cast<UBlendProfile>(PropertyValue);

			FBlendProfilePickerArgs Args;
			Args.bAllowNew = false;
			Args.OnBlendProfileSelected = FOnBlendProfileSelected::CreateSP(this, &FAnimGraphNodeDetails::OnBlendProfileChanged, PropertyPtr);
			Args.InitialProfile = CurrentProfile;

			ISkeletonEditorModule& SkeletonEditorModule = FModuleManager::Get().LoadModuleChecked<ISkeletonEditorModule>("SkeletonEditor");
			return SkeletonEditorModule.CreateBlendProfilePicker(this->TargetSkeleton, Args);

		}
	}

	return SNullWidget::NullWidget;
}

bool FAnimGraphNodeDetails::OnShouldFilterAnimAsset( const FAssetData& AssetData, UClass* NodeToFilterFor ) const
{
	const FString* SkeletonName = AssetData.TagsAndValues.Find(TEXT("Skeleton"));
	if ((SkeletonName != nullptr) && (*SkeletonName == TargetSkeletonName))
	{
		const UClass* AssetClass = AssetData.GetClass();
		// If node is an 'asset player', only let you select the right kind of asset for it
		if (!NodeToFilterFor->IsChildOf(UAnimGraphNode_AssetPlayerBase::StaticClass()) || SupportNodeClassForAsset(AssetClass, NodeToFilterFor))
		{
			return false;
		}
	}
	return true;
}

EVisibility FAnimGraphNodeDetails::GetVisibilityOfProperty(TSharedRef<IPropertyHandle> Handle) const
{
	bool bShowAsPin;
	if (FPropertyAccess::Success == Handle->GetValue(/*out*/ bShowAsPin))
	{
		return bShowAsPin ? EVisibility::Hidden : EVisibility::Visible;
	}
	else
	{
		return EVisibility::Visible;
	}
}

void FAnimGraphNodeDetails::OnBlendProfileChanged(UBlendProfile* NewProfile, TSharedPtr<IPropertyHandle> PropertyHandle)
{
	if(PropertyHandle.IsValid())
	{
		PropertyHandle->SetValue(NewProfile);
	}
}


TSharedRef<IPropertyTypeCustomization> FInputScaleBiasCustomization::MakeInstance() 
{
	return MakeShareable(new FInputScaleBiasCustomization());
}

float GetMinValue(float Scale, float Bias)
{
	return Scale != 0.0f ? (FMath::Abs(Bias) < SMALL_NUMBER ? 0.0f : -Bias) / Scale : 0.0f; // to avoid displaying of - in front of 0
}

float GetMaxValue(float Scale, float Bias)
{
	return Scale != 0.0f ? (1.0f - Bias) / Scale : 0.0f;
}

void UpdateInputScaleBiasWithMinValue(float MinValue, TSharedRef<class IPropertyHandle> InputBiasScaleStructPropertyHandle)
{
	InputBiasScaleStructPropertyHandle->NotifyPreChange();

	TSharedRef<class IPropertyHandle> BiasProperty = InputBiasScaleStructPropertyHandle->GetChildHandle("Bias").ToSharedRef();
	TSharedRef<class IPropertyHandle> ScaleProperty = InputBiasScaleStructPropertyHandle->GetChildHandle("Scale").ToSharedRef();
	TArray<void*> BiasDataArray;
	TArray<void*> ScaleDataArray;
	BiasProperty->AccessRawData(BiasDataArray);
	ScaleProperty->AccessRawData(ScaleDataArray);
	check(BiasDataArray.Num() == ScaleDataArray.Num());
	for(int32 DataIndex = 0; DataIndex < BiasDataArray.Num(); ++DataIndex)
	{
		float* BiasPtr = (float*)BiasDataArray[DataIndex];
		float* ScalePtr = (float*)ScaleDataArray[DataIndex];
		check(BiasPtr);
		check(ScalePtr);

		const float MaxValue = GetMaxValue(*ScalePtr, *BiasPtr);
		const float Difference = MaxValue - MinValue;
		*ScalePtr = Difference != 0.0f? 1.0f / Difference : 0.0f;
		*BiasPtr = -MinValue * *ScalePtr;
	}

	InputBiasScaleStructPropertyHandle->NotifyPostChange();
}

void UpdateInputScaleBiasWithMaxValue(float MaxValue, TSharedRef<class IPropertyHandle> InputBiasScaleStructPropertyHandle)
{
	InputBiasScaleStructPropertyHandle->NotifyPreChange();

	TSharedRef<class IPropertyHandle> BiasProperty = InputBiasScaleStructPropertyHandle->GetChildHandle("Bias").ToSharedRef();
	TSharedRef<class IPropertyHandle> ScaleProperty = InputBiasScaleStructPropertyHandle->GetChildHandle("Scale").ToSharedRef();
	TArray<void*> BiasDataArray;
	TArray<void*> ScaleDataArray;
	BiasProperty->AccessRawData(BiasDataArray);
	ScaleProperty->AccessRawData(ScaleDataArray);
	check(BiasDataArray.Num() == ScaleDataArray.Num());
	for(int32 DataIndex = 0; DataIndex < BiasDataArray.Num(); ++DataIndex)
	{
		float* BiasPtr = (float*)BiasDataArray[DataIndex];
		float* ScalePtr = (float*)ScaleDataArray[DataIndex];
		check(BiasPtr);
		check(ScalePtr);

		const float MinValue = GetMinValue(*ScalePtr, *BiasPtr);
		const float Difference = MaxValue - MinValue;
		*ScalePtr = Difference != 0.0f ? 1.0f / Difference : 0.0f;
		*BiasPtr = -MinValue * *ScalePtr;
	}

	InputBiasScaleStructPropertyHandle->NotifyPostChange();
}

TOptional<float> GetMinValueInputScaleBias(TSharedRef<class IPropertyHandle> InputBiasScaleStructPropertyHandle)
{
	TSharedRef<class IPropertyHandle> BiasProperty = InputBiasScaleStructPropertyHandle->GetChildHandle("Bias").ToSharedRef();
	TSharedRef<class IPropertyHandle> ScaleProperty = InputBiasScaleStructPropertyHandle->GetChildHandle("Scale").ToSharedRef();
	float Scale = 1.0f;
	float Bias = 0.0f;
	if(ScaleProperty->GetValue(Scale) == FPropertyAccess::Success && BiasProperty->GetValue(Bias) == FPropertyAccess::Success)
	{
		return GetMinValue(Scale, Bias);
	}

	return TOptional<float>();
}

TOptional<float> GetMaxValueInputScaleBias(TSharedRef<class IPropertyHandle> InputBiasScaleStructPropertyHandle)
{
	TSharedRef<class IPropertyHandle> BiasProperty = InputBiasScaleStructPropertyHandle->GetChildHandle("Bias").ToSharedRef();
	TSharedRef<class IPropertyHandle> ScaleProperty = InputBiasScaleStructPropertyHandle->GetChildHandle("Scale").ToSharedRef();
	float Scale = 1.0f;
	float Bias = 0.0f;
	if(ScaleProperty->GetValue(Scale) == FPropertyAccess::Success && BiasProperty->GetValue(Bias) == FPropertyAccess::Success)
	{
		return GetMaxValue(Scale, Bias);
	}

	return TOptional<float>();
}


void FInputScaleBiasCustomization::CustomizeHeader(TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{

}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void FInputScaleBiasCustomization::CustomizeChildren( TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	TWeakPtr<IPropertyHandle> WeakStructPropertyHandle = StructPropertyHandle;

	StructBuilder
	.AddProperty(StructPropertyHandle)
	.CustomWidget()
	.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(250.0f)
	.MaxDesiredWidth(250.0f)
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.Padding(FMargin(0.0f, 2.0f, 3.0f, 2.0f))
		[
			SNew(SNumericEntryBox<float>)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.ToolTipText(LOCTEXT("MinInputScaleBias", "Minimum input value"))
			.AllowSpin(true)
			.MinSliderValue(0.0f)
			.MaxSliderValue(2.0f)
			.Value_Lambda([WeakStructPropertyHandle]()
			{
				return GetMinValueInputScaleBias(WeakStructPropertyHandle.Pin().ToSharedRef());
			})
			.OnValueChanged_Lambda([WeakStructPropertyHandle](float InValue)
			{
				UpdateInputScaleBiasWithMinValue(InValue, WeakStructPropertyHandle.Pin().ToSharedRef());
			})
		]
		+SHorizontalBox::Slot()
		.Padding(FMargin(0.0f, 2.0f, 0.0f, 2.0f))
		[
			SNew(SNumericEntryBox<float>)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.ToolTipText(LOCTEXT("MaxInputScaleBias", "Maximum input value"))
			.AllowSpin(true)
			.MinSliderValue(0.0f)
			.MaxSliderValue(2.0f)
			.Value_Lambda([WeakStructPropertyHandle]()
			{
				return GetMaxValueInputScaleBias(WeakStructPropertyHandle.Pin().ToSharedRef());
			})
			.OnValueChanged_Lambda([WeakStructPropertyHandle](float InValue)
			{
				UpdateInputScaleBiasWithMaxValue(InValue, WeakStructPropertyHandle.Pin().ToSharedRef());
			})
		]
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

/////////////////////////////////////////////////////////////////////////////////////////////
//  FBoneReferenceCustomization
/////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<IPropertyTypeCustomization> FBoneReferenceCustomization::MakeInstance()
{
	return MakeShareable(new FBoneReferenceCustomization());
}

void FBoneReferenceCustomization::CustomizeHeader( TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	// set property handle 
	SetPropertyHandle(StructPropertyHandle);
	// set editable skeleton info from struct
	SetEditableSkeleton(StructPropertyHandle);
	if (TargetEditableSkeleton.IsValid() && BoneNameProperty->IsValidHandle())
	{
		HeaderRow
		.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(200.f)
		[
			SNew(SBoneSelectionWidget)
			.ToolTipText(StructPropertyHandle->GetToolTipText())
			.OnBoneSelectionChanged(this, &FBoneReferenceCustomization::OnBoneSelectionChanged)
			.OnGetSelectedBone(this, &FBoneReferenceCustomization::GetSelectedBone)
			.OnGetReferenceSkeleton(this, &FBoneReferenceCustomization::GetReferenceSkeleton)
		];
	}
	else
	{
		// if this FBoneReference is used by some other Outers, this will fail	
		// should warn programmers instead of silent fail
		ensureAlways(false);
	}
}

void FBoneReferenceCustomization::SetEditableSkeleton(TSharedRef<IPropertyHandle> StructPropertyHandle) 
{
	TArray<UObject*> Objects;
	StructPropertyHandle->GetOuterObjects(Objects);
	UAnimGraphNode_Base* AnimGraphNode = nullptr;
	USkeletalMesh* SkeletalMesh = nullptr;
	UAnimationAsset * AnimationAsset = nullptr;

	USkeleton* TargetSkeleton = nullptr;
	TSharedPtr<IEditableSkeleton> EditableSkeleton;

	for (auto OuterIter = Objects.CreateIterator(); OuterIter; ++OuterIter)
	{
		AnimGraphNode = Cast<UAnimGraphNode_Base>(*OuterIter);
		if (AnimGraphNode)
		{
			TargetSkeleton = AnimGraphNode->GetAnimBlueprint()->TargetSkeleton;
			break;
		}
		SkeletalMesh = Cast<USkeletalMesh>(*OuterIter);
		if (SkeletalMesh)
		{
			TargetSkeleton = SkeletalMesh->Skeleton;
			break;
		}
		AnimationAsset = Cast<UAnimationAsset>(*OuterIter);
		if (AnimationAsset)
		{
			TargetSkeleton = AnimationAsset->GetSkeleton();
			break;
		}

		if (UAnimInstance* AnimInstance = Cast<UAnimInstance>(*OuterIter))
		{
			if (AnimInstance->CurrentSkeleton)
			{
				TargetSkeleton = AnimInstance->CurrentSkeleton;
				break;
			}
			else if (UAnimBlueprintGeneratedClass* AnimBPClass = Cast<UAnimBlueprintGeneratedClass>(AnimInstance->GetClass()))
			{
				TargetSkeleton = AnimBPClass->TargetSkeleton;
				break;
			}
		}

		// editor animation curve bone links are responsible for linking joints to curve
		// this is editor object that only exists for editor
		if (UEditorAnimCurveBoneLinks* AnimCurveObj = Cast<UEditorAnimCurveBoneLinks>(*OuterIter))
		{
			EditableSkeleton = AnimCurveObj->EditableSkeleton.Pin();
		}
	}

	if (TargetSkeleton != nullptr)
	{
		ISkeletonEditorModule& SkeletonEditorModule = FModuleManager::LoadModuleChecked<ISkeletonEditorModule>("SkeletonEditor");
		EditableSkeleton = SkeletonEditorModule.CreateEditableSkeleton(TargetSkeleton);
	}

	TargetEditableSkeleton = EditableSkeleton;
}

TSharedPtr<IPropertyHandle> FBoneReferenceCustomization::FindStructMemberProperty(TSharedRef<IPropertyHandle> PropertyHandle, const FName& PropertyName)
{
	uint32 NumChildren = 0;
	PropertyHandle->GetNumChildren(NumChildren);
	for (uint32 ChildIdx = 0; ChildIdx < NumChildren; ++ChildIdx)
	{
		TSharedPtr<IPropertyHandle> ChildHandle = PropertyHandle->GetChildHandle(ChildIdx);
		if (ChildHandle->GetProperty()->GetFName() == PropertyName)
		{
			return ChildHandle;
		}
	}

	return TSharedPtr<IPropertyHandle>();
}

void FBoneReferenceCustomization::SetPropertyHandle(TSharedRef<IPropertyHandle> StructPropertyHandle)
{
	BoneNameProperty = FindStructMemberProperty(StructPropertyHandle, GET_MEMBER_NAME_CHECKED(FBoneReference, BoneName));
	check(BoneNameProperty->IsValidHandle());
}

void FBoneReferenceCustomization::CustomizeChildren( TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	
}

void FBoneReferenceCustomization::OnBoneSelectionChanged(FName Name)
{
	BoneNameProperty->SetValue(Name);
}

FName FBoneReferenceCustomization::GetSelectedBone(bool& bMultipleValues) const
{
	FString OutText;
	
	FPropertyAccess::Result Result = BoneNameProperty->GetValueAsFormattedString(OutText);
	bMultipleValues = (Result == FPropertyAccess::MultipleValues);

	return FName(*OutText);
}

const struct FReferenceSkeleton&  FBoneReferenceCustomization::GetReferenceSkeleton() const
{
	// retruning dummy skeleton if any reason, it is invalid
	static FReferenceSkeleton DummySkeleton;

	return (TargetEditableSkeleton.IsValid()) ? TargetEditableSkeleton.Get()->GetSkeleton().GetReferenceSkeleton() : DummySkeleton;
}

/////////////////////////////////////////////////////////////////////////////////////////////
//  FBoneSocketTargetCustomization
/////////////////////////////////////////////////////////////////////////////////////////////
TSharedRef<IPropertyTypeCustomization> FBoneSocketTargetCustomization::MakeInstance()
{
	return MakeShareable(new FBoneSocketTargetCustomization());
}

void FBoneSocketTargetCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	// set property handle 
 	SetPropertyHandle(StructPropertyHandle);
 	// set editable skeleton info from struct
 	SetEditableSkeleton(StructPropertyHandle);
 	Build(StructPropertyHandle, ChildBuilder);
}

void FBoneSocketTargetCustomization::SetPropertyHandle(TSharedRef<IPropertyHandle> StructPropertyHandle)
{
	TSharedPtr<IPropertyHandle> BoneReferenceProperty = FindStructMemberProperty(StructPropertyHandle, GET_MEMBER_NAME_CHECKED(FBoneSocketTarget, BoneReference));
	check(BoneReferenceProperty->IsValidHandle());
	BoneNameProperty = FindStructMemberProperty(BoneReferenceProperty.ToSharedRef(), GET_MEMBER_NAME_CHECKED(FBoneReference, BoneName));
	TSharedPtr<IPropertyHandle> SocketReferenceProperty = FindStructMemberProperty(StructPropertyHandle, GET_MEMBER_NAME_CHECKED(FBoneSocketTarget, SocketReference));
	check(SocketReferenceProperty->IsValidHandle());
	SocketNameProperty = FindStructMemberProperty(SocketReferenceProperty.ToSharedRef(), GET_MEMBER_NAME_CHECKED(FSocketReference, SocketName));
	UseSocketProperty = FindStructMemberProperty(StructPropertyHandle, GET_MEMBER_NAME_CHECKED(FBoneSocketTarget, bUseSocket));

	check(BoneNameProperty->IsValidHandle() && SocketNameProperty->IsValidHandle() && UseSocketProperty->IsValidHandle());
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void FBoneSocketTargetCustomization::Build(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& ChildBuilder)
{
	if (TargetEditableSkeleton.IsValid() && BoneNameProperty->IsValidHandle())
	{
		ChildBuilder
		.AddProperty(StructPropertyHandle)
		.CustomWidget()
		.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(200.f)
		[
			SNew(SBoneSelectionWidget)
			.ToolTipText(StructPropertyHandle->GetToolTipText())
			.bShowSocket(true)
			.OnBoneSelectionChanged(this, &FBoneSocketTargetCustomization::OnBoneSelectionChanged)
			.OnGetSelectedBone(this, &FBoneSocketTargetCustomization::GetSelectedBone)
			.OnGetReferenceSkeleton(this, &FBoneReferenceCustomization::GetReferenceSkeleton)
			.OnGetSocketList(this, &FBoneSocketTargetCustomization::GetSocketList)
		];
	}
	else
	{
		// if this FBoneSocketTarget is used by some other Outers, this will fail	
		// should warn programmers instead of silent fail
		ensureAlways(false);
	}
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

TSharedPtr<IPropertyHandle> FBoneSocketTargetCustomization::GetNameProperty() const
{
	bool bUseSocket = false;
	if (UseSocketProperty->GetValue(bUseSocket) == FPropertyAccess::Success)
	{
		if (bUseSocket)
		{
			return SocketNameProperty;
		}

		return BoneNameProperty;
	}

	return TSharedPtr<IPropertyHandle>();
}
void FBoneSocketTargetCustomization::OnBoneSelectionChanged(FName Name)
{
	// figure out if the name is BoneName or socket name
	if (TargetEditableSkeleton.IsValid())
	{
		bool bUseSocket = false;
		if (GetReferenceSkeleton().FindBoneIndex(Name) == INDEX_NONE)
		{
			// make sure socket exists
			const TArray<class USkeletalMeshSocket*>& Sockets = GetSocketList();
			for (int32 Idx = 0; Idx < Sockets.Num(); ++Idx)
			{
				if (Sockets[Idx]->SocketName == Name)
				{
					bUseSocket = true;
					break;
				}
			}

			// we should find one
			ensure(bUseSocket);
		}

		// set correct value
		UseSocketProperty->SetValue(bUseSocket);

		TSharedPtr<IPropertyHandle> NameProperty = GetNameProperty();
		if (ensureAlways(NameProperty.IsValid()))
		{
			NameProperty->SetValue(Name);
		}
	}
}

FName FBoneSocketTargetCustomization::GetSelectedBone(bool& bMultipleValues) const
{
	FString OutText;

	TSharedPtr<IPropertyHandle> NameProperty = GetNameProperty();
	if (NameProperty.IsValid())
	{
		FPropertyAccess::Result Result = NameProperty->GetValueAsFormattedString(OutText);
		bMultipleValues = (Result == FPropertyAccess::MultipleValues);
	}
	else
	{
		// there is no single value
		bMultipleValues = true;
		return NAME_None;
	}

	return FName(*OutText);
}

const TArray<class USkeletalMeshSocket*>& FBoneSocketTargetCustomization::GetSocketList() const
{
	if (TargetEditableSkeleton.IsValid())
	{
		return  TargetEditableSkeleton.Get()->GetSkeleton().Sockets;
	}

	static TArray<class USkeletalMeshSocket*> DummyList;
	return DummyList;
}

/////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<IDetailCustomization> FAnimGraphParentPlayerDetails::MakeInstance(TSharedRef<FBlueprintEditor> InBlueprintEditor)
{
	return MakeShareable(new FAnimGraphParentPlayerDetails(InBlueprintEditor));
}

void FAnimGraphParentPlayerDetails::CustomizeDetails(class IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> SelectedObjects;
	DetailBuilder.GetObjectsBeingCustomized(SelectedObjects);
	check(SelectedObjects.Num() == 1);

	EditorObject = Cast<UEditorParentPlayerListObj>(SelectedObjects[0].Get());
	check(EditorObject);
	
	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory("AnimGraphOverrides");
	DetailBuilder.HideProperty("Overrides");

	// Build a hierarchy of entires for a tree view in the form of Blueprint->Graph->Node
	for(FAnimParentNodeAssetOverride& Override : EditorObject->Overrides)
	{
		UAnimGraphNode_Base* Node = EditorObject->GetVisualNodeFromGuid(Override.ParentNodeGuid);
		TSharedPtr<FPlayerTreeViewEntry> NodeEntry = MakeShareable(new FPlayerTreeViewEntry(Node->GetNodeTitle(ENodeTitleType::ListView).ToString(), EPlayerTreeViewEntryType::Node, &Override));
		
		// Process blueprint entry
		TSharedPtr<FPlayerTreeViewEntry>* ExistingBPEntry = ListEntries.FindByPredicate([Node](const TSharedPtr<FPlayerTreeViewEntry>& Other)
		{
			return Node->GetBlueprint()->GetName() == Other->EntryName;
		});

		if(!ExistingBPEntry)
		{
			ListEntries.Add(MakeShareable(new FPlayerTreeViewEntry(Node->GetBlueprint()->GetName(), EPlayerTreeViewEntryType::Blueprint)));
			ExistingBPEntry = &ListEntries.Last();
		}

		// Process graph entry
		TSharedPtr<FPlayerTreeViewEntry>* ExistingGraphEntry = (*ExistingBPEntry)->Children.FindByPredicate([Node](const TSharedPtr<FPlayerTreeViewEntry>& Other)
		{
			return Node->GetGraph()->GetName() == Other->EntryName;
		});

		if(!ExistingGraphEntry)
		{
			(*ExistingBPEntry)->Children.Add(MakeShareable(new FPlayerTreeViewEntry(Node->GetGraph()->GetName(), EPlayerTreeViewEntryType::Graph)));
			ExistingGraphEntry = &(*ExistingBPEntry)->Children.Last();
		}

		// Process node entry
		(*ExistingGraphEntry)->Children.Add(NodeEntry);
	}

	FDetailWidgetRow& Row = Category.AddCustomRow(FText::GetEmpty());
	TSharedRef<STreeView<TSharedPtr<FPlayerTreeViewEntry>>> TreeView = SNew(STreeView<TSharedPtr<FPlayerTreeViewEntry>>)
		.SelectionMode(ESelectionMode::None)
		.OnGenerateRow(this, &FAnimGraphParentPlayerDetails::OnGenerateRow)
		.OnGetChildren(this, &FAnimGraphParentPlayerDetails::OnGetChildren)
		.TreeItemsSource(&ListEntries)
		.HeaderRow
		(
			SNew(SHeaderRow)
			+SHeaderRow::Column(FName("Name"))
			.FillWidth(0.5f)
			.DefaultLabel(LOCTEXT("ParentPlayer_NameCol", "Name"))

			+SHeaderRow::Column(FName("Asset"))
			.FillWidth(0.5f)
			.DefaultLabel(LOCTEXT("ParentPlayer_AssetCol", "Asset"))
		);

	// Expand top level (blueprint) entries so the panel seems less empty
	for(TSharedPtr<FPlayerTreeViewEntry> Entry : ListEntries)
	{
		TreeView->SetItemExpansion(Entry, true);
	}

	Row
	[
		TreeView->AsShared()
	];
}

TSharedRef<ITableRow> FAnimGraphParentPlayerDetails::OnGenerateRow(TSharedPtr<FPlayerTreeViewEntry> EntryPtr, const TSharedRef< STableViewBase >& OwnerTable)
{
	return SNew(SParentPlayerTreeRow, OwnerTable).Item(EntryPtr).OverrideObject(EditorObject).BlueprintEditor(BlueprintEditorPtr);
}

void FAnimGraphParentPlayerDetails::OnGetChildren(TSharedPtr<FPlayerTreeViewEntry> InParent, TArray< TSharedPtr<FPlayerTreeViewEntry> >& OutChildren)
{
	OutChildren.Append(InParent->Children);
}

void SParentPlayerTreeRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	Item = InArgs._Item;
	EditorObject = InArgs._OverrideObject;
	BlueprintEditor = InArgs._BlueprintEditor;

	if(Item->Override)
	{
		GraphNode = EditorObject->GetVisualNodeFromGuid(Item->Override->ParentNodeGuid);
	}
	else
	{
		GraphNode = NULL;
	}

	SMultiColumnTableRow<TSharedPtr<FAnimGraphParentPlayerDetails>>::Construct(FSuperRowType::FArguments(), InOwnerTableView);
}

TSharedRef<SWidget> SParentPlayerTreeRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	TSharedPtr<SHorizontalBox> HorizBox;
	SAssignNew(HorizBox, SHorizontalBox);

	if(ColumnName == "Name")
	{
		HorizBox->AddSlot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SExpanderArrow, SharedThis(this))
			];

		Item->GenerateNameWidget(HorizBox);
	}
	else if(Item->Override)
	{
		HorizBox->AddSlot()
			.Padding(2)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(FEditorStyle::Get(), "ToggleButton")
				.ToolTip(IDocumentation::Get()->CreateToolTip(LOCTEXT("FocusNodeButtonTip", "Open the graph that contains this node in read-only mode and focus on the node"), NULL, "Shared/Editors/Persona", "FocusNodeButton"))
				.OnClicked(FOnClicked::CreateSP(this, &SParentPlayerTreeRow::OnFocusNodeButtonClicked))
				.Content()
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("GenericViewButton"))
				]
				
			];
		
		TArray<const UClass*> AllowedClasses;
		AllowedClasses.Add(UAnimationAsset::StaticClass());
		HorizBox->AddSlot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SObjectPropertyEntryBox)
				.ObjectPath(this, &SParentPlayerTreeRow::GetCurrentAssetPath)
				.OnShouldFilterAsset(this, &SParentPlayerTreeRow::OnShouldFilterAsset)
				.OnObjectChanged(this, &SParentPlayerTreeRow::OnAssetSelected)
				.AllowedClass(GetCurrentAssetToUse()->GetClass())
			];

		HorizBox->AddSlot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(FEditorStyle::Get(), "NoBorder")
				.Visibility(this, &SParentPlayerTreeRow::GetResetToDefaultVisibility)
				.OnClicked(this, &SParentPlayerTreeRow::OnResetButtonClicked)
				.ToolTip(IDocumentation::Get()->CreateToolTip(LOCTEXT("ResetToParentButtonTip", "Undo the override, returning to the default asset for this node"), NULL, "Shared/Editors/Persona", "ResetToParentButton"))
				.Content()
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
				]
			];
	}

	return HorizBox.ToSharedRef();
}

bool SParentPlayerTreeRow::OnShouldFilterAsset(const FAssetData& AssetData)
{
	const FString SkeletonName = AssetData.GetTagValueRef<FString>("Skeleton");

	if(!SkeletonName.IsEmpty())
	{
		USkeleton* CurrentSkeleton = GraphNode->GetAnimBlueprint()->TargetSkeleton;
		if(SkeletonName == FString::Printf(TEXT("%s'%s'"), *CurrentSkeleton->GetClass()->GetName(), *CurrentSkeleton->GetPathName()))
		{
			return false;
		}
	}

	return true;
}

void SParentPlayerTreeRow::OnAssetSelected(const FAssetData& AssetData)
{
	Item->Override->NewAsset = Cast<UAnimationAsset>(AssetData.GetAsset());
	EditorObject->ApplyOverrideToBlueprint(*Item->Override);
}

FReply SParentPlayerTreeRow::OnFocusNodeButtonClicked()
{
	TSharedPtr<FBlueprintEditor> SharedBlueprintEditor = BlueprintEditor.Pin();
	if(SharedBlueprintEditor.IsValid())
	{
		if(GraphNode)
		{
			UEdGraph* EdGraph = GraphNode->GetGraph();
			TSharedPtr<SGraphEditor> GraphEditor = SharedBlueprintEditor->OpenGraphAndBringToFront(EdGraph);
			if (GraphEditor.IsValid())
			{
				GraphEditor->JumpToNode(GraphNode, false);
			}
		}
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

const UAnimationAsset* SParentPlayerTreeRow::GetCurrentAssetToUse() const
{
	if(Item->Override->NewAsset)
	{
		return Item->Override->NewAsset;
	}
	
	if(GraphNode)
	{
		return GraphNode->GetAnimationAsset();
	}

	return NULL;
}

EVisibility SParentPlayerTreeRow::GetResetToDefaultVisibility() const
{
	FAnimParentNodeAssetOverride* HierarchyOverride = EditorObject->GetBlueprint()->GetAssetOverrideForNode(Item->Override->ParentNodeGuid, true);

	if(HierarchyOverride)
	{
		return Item->Override->NewAsset != HierarchyOverride->NewAsset ? EVisibility::Visible : EVisibility::Hidden;
	}

	return Item->Override->NewAsset != GraphNode->GetAnimationAsset() ? EVisibility::Visible : EVisibility::Hidden;
}

FReply SParentPlayerTreeRow::OnResetButtonClicked()
{
	FAnimParentNodeAssetOverride* HierarchyOverride = EditorObject->GetBlueprint()->GetAssetOverrideForNode(Item->Override->ParentNodeGuid, true);
	
	Item->Override->NewAsset = HierarchyOverride ? HierarchyOverride->NewAsset : GraphNode->GetAnimationAsset();

	// Apply will remove the override from the object
	EditorObject->ApplyOverrideToBlueprint(*Item->Override);
	return FReply::Handled();
}

FString SParentPlayerTreeRow::GetCurrentAssetPath() const
{
	const UAnimationAsset* Asset = GetCurrentAssetToUse();
	return Asset ? Asset->GetPathName() : FString("");
}

FORCENOINLINE bool FPlayerTreeViewEntry::operator==(const FPlayerTreeViewEntry& Other)
{
	return EntryName == Other.EntryName;
}

void FPlayerTreeViewEntry::GenerateNameWidget(TSharedPtr<SHorizontalBox> Box)
{
	// Get an appropriate image icon for the row
	const FSlateBrush* EntryImageBrush = NULL;
	switch(EntryType)
	{
		case EPlayerTreeViewEntryType::Blueprint:
			EntryImageBrush = FEditorStyle::GetBrush("ClassIcon.Blueprint");
			break;
		case EPlayerTreeViewEntryType::Graph:
			EntryImageBrush = FEditorStyle::GetBrush("GraphEditor.EventGraph_16x");
			break;
		case EPlayerTreeViewEntryType::Node:
			EntryImageBrush = FEditorStyle::GetBrush("GraphEditor.Default_16x");
			break;
		default:
			break;
	}

	Box->AddSlot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SImage)
			.Image(EntryImageBrush)
		];

	Box->AddSlot()
		.VAlign(VAlign_Center)
		.Padding(FMargin(5.0f, 0.0f, 0.0f, 0.0f))
		.AutoWidth()
		[
			SNew(STextBlock)
			.Font(FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 10))
			.Text(FText::FromString(EntryName))
		];
}

#undef LOCTEXT_NAMESPACE
