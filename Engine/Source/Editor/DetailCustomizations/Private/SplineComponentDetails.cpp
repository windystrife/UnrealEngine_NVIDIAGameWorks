// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SplineComponentDetails.h"
#include "Misc/MessageDialog.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"
#include "Layout/Visibility.h"
#include "Misc/Attribute.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SComboBox.h"
#include "ComponentVisualizer.h"
#include "Editor/UnrealEdEngine.h"
#include "UnrealEdGlobals.h"
#include "IDetailCustomNodeBuilder.h"
#include "IDetailChildrenBuilder.h"
#include "DetailWidgetRow.h"
#include "DetailLayoutBuilder.h"
#include "Components/SplineComponent.h"
#include "DetailCategoryBuilder.h"
#include "SplineComponentVisualizer.h"
#include "Widgets/Input/SVectorInputBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "SplineComponentDetails"

class FSplinePointDetails : public IDetailCustomNodeBuilder, public TSharedFromThis<FSplinePointDetails>
{
public:
	FSplinePointDetails();

	//~ Begin IDetailCustomNodeBuilder interface
	virtual void SetOnRebuildChildren(FSimpleDelegate InOnRegenerateChildren) override;
	virtual void GenerateHeaderRowContent(FDetailWidgetRow& NodeRow) override;
	virtual void GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder) override;
	virtual void Tick(float DeltaTime) override;
	virtual bool RequiresTick() const override { return true; }
	virtual bool InitiallyCollapsed() const override { return false; }
	virtual FName GetName() const override;
	//~ End IDetailCustomNodeBuilder interface

private:

	template <typename T>
	struct TSharedValue
	{
		TSharedValue() : bInitialized(false) {}

		void Reset()
		{
			bInitialized = false;
		}

		void Add(T InValue)
		{
			if (!bInitialized)
			{
				Value = InValue;
				bInitialized = true;
			}
			else
			{
				if (Value.IsSet() && InValue != Value.GetValue()) { Value.Reset(); }
			}
		}

		TOptional<T> Value;
		bool bInitialized;
	};

	struct FSharedVectorValue
	{
		FSharedVectorValue() : bInitialized(false) {}

		void Reset()
		{
			bInitialized = false;
		}

		bool IsValid() const { return bInitialized; }

		void Add(const FVector& V)
		{
			if (!bInitialized)
			{
				X = V.X;
				Y = V.Y;
				Z = V.Z;
				bInitialized = true;
			}
			else
			{
				if (X.IsSet() && V.X != X.GetValue()) { X.Reset(); }
				if (Y.IsSet() && V.Y != Y.GetValue()) { Y.Reset(); }
				if (Z.IsSet() && V.Z != Z.GetValue()) { Z.Reset(); }
			}
		}

		TOptional<float> X;
		TOptional<float> Y;
		TOptional<float> Z;
		bool bInitialized;
	};

	struct FSharedRotatorValue
	{
		FSharedRotatorValue() : bInitialized(false) {}

		void Reset()
		{
			bInitialized = false;
		}

		bool IsValid() const { return bInitialized; }

		void Add(const FRotator& R)
		{
			if (!bInitialized)
			{
				Roll = R.Roll;
				Pitch = R.Pitch;
				Yaw = R.Yaw;
				bInitialized = true;
			}
			else
			{
				if (Roll.IsSet() && R.Roll != Roll.GetValue()) { Roll.Reset(); }
				if (Pitch.IsSet() && R.Pitch != Pitch.GetValue()) { Pitch.Reset(); }
				if (Yaw.IsSet() && R.Yaw != Yaw.GetValue()) { Yaw.Reset(); }
			}
		}

		TOptional<float> Roll;
		TOptional<float> Pitch;
		TOptional<float> Yaw;
		bool bInitialized;
	};

	EVisibility IsEnabled() const { return (SelectedKeys.Num() > 0) ? EVisibility::Visible : EVisibility::Collapsed; }
	EVisibility IsDisabled() const { return (SelectedKeys.Num() == 0) ? EVisibility::Visible : EVisibility::Collapsed; }
	bool IsOnePointSelected() const { return SelectedKeys.Num() == 1; }
	TOptional<float> GetInputKey() const { return InputKey.Value; }
	TOptional<float> GetPositionX() const { return Position.X; }
	TOptional<float> GetPositionY() const { return Position.Y; }
	TOptional<float> GetPositionZ() const { return Position.Z; }
	TOptional<float> GetArriveTangentX() const { return ArriveTangent.X; }
	TOptional<float> GetArriveTangentY() const { return ArriveTangent.Y; }
	TOptional<float> GetArriveTangentZ() const { return ArriveTangent.Z; }
	TOptional<float> GetLeaveTangentX() const { return LeaveTangent.X; }
	TOptional<float> GetLeaveTangentY() const { return LeaveTangent.Y; }
	TOptional<float> GetLeaveTangentZ() const { return LeaveTangent.Z; }
	TOptional<float> GetRotationRoll() const { return Rotation.Roll; }
	TOptional<float> GetRotationPitch() const { return Rotation.Pitch; }
	TOptional<float> GetRotationYaw() const { return Rotation.Yaw; }
	TOptional<float> GetScaleX() const { return Scale.X; }
	TOptional<float> GetScaleY() const { return Scale.Y; }
	TOptional<float> GetScaleZ() const { return Scale.Z; }
	void OnSetInputKey(float NewValue, ETextCommit::Type CommitInfo);
	void OnSetPosition(float NewValue, ETextCommit::Type CommitInfo, int32 Axis);
	void OnSetArriveTangent(float NewValue, ETextCommit::Type CommitInfo, int32 Axis);
	void OnSetLeaveTangent(float NewValue, ETextCommit::Type CommitInfo, int32 Axis);
	void OnSetRotation(float NewValue, ETextCommit::Type CommitInfo, int32 Axis);
	void OnSetScale(float NewValue, ETextCommit::Type CommitInfo, int32 Axis);
	FText GetPointType() const;
	void OnSplinePointTypeChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo);
	TSharedRef<SWidget> OnGenerateComboWidget(TSharedPtr<FString> InComboString);

	void UpdateValues();

	USplineComponent* SplineComp;
	TSet<int32> SelectedKeys;

	TSharedValue<float> InputKey;
	FSharedVectorValue Position;
	FSharedVectorValue ArriveTangent;
	FSharedVectorValue LeaveTangent;
	FSharedVectorValue Scale;
	FSharedRotatorValue Rotation;
	TSharedValue<ESplinePointType::Type> PointType;

	FSplineComponentVisualizer* SplineVisualizer;
	UProperty* SplineCurvesProperty;
	TArray<TSharedPtr<FString>> SplinePointTypes;
};

FSplinePointDetails::FSplinePointDetails()
	: SplineComp(nullptr)
{
	TSharedPtr<FComponentVisualizer> Visualizer = GUnrealEd->FindComponentVisualizer(USplineComponent::StaticClass());
	SplineVisualizer = (FSplineComponentVisualizer*)Visualizer.Get();
	check(SplineVisualizer);

	SplineCurvesProperty = FindField<UProperty>(USplineComponent::StaticClass(), GET_MEMBER_NAME_CHECKED(USplineComponent, SplineCurves));

	UEnum* SplinePointTypeEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("ESplinePointType"));
	check(SplinePointTypeEnum);
	for (int32 EnumIndex = 0; EnumIndex < SplinePointTypeEnum->NumEnums() - 1; ++EnumIndex)
	{
		SplinePointTypes.Add(MakeShareable(new FString(SplinePointTypeEnum->GetNameStringByIndex(EnumIndex))));
	}
}

void FSplinePointDetails::SetOnRebuildChildren(FSimpleDelegate InOnRegenerateChildren)
{
}

void FSplinePointDetails::GenerateHeaderRowContent(FDetailWidgetRow& NodeRow)
{
}
									  									  

void FSplinePointDetails::GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder)
{
	// Message which is shown when no points are selected
	ChildrenBuilder.AddCustomRow(LOCTEXT("NoneSelected", "None selected"))
	.Visibility(TAttribute<EVisibility>(this, &FSplinePointDetails::IsDisabled))
	[
		SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NoPointsSelected", "No spline points are selected."))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
	];

	// Input key
	ChildrenBuilder.AddCustomRow(LOCTEXT("InputKey", "Input Key"))
	.Visibility(TAttribute<EVisibility>(this, &FSplinePointDetails::IsEnabled))
	.NameContent()
	.HAlign(HAlign_Left)
	.VAlign(VAlign_Center)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("InputKey", "Input Key"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MinDesiredWidth(125.0f)
	.MaxDesiredWidth(125.0f)
	[
		SNew(SNumericEntryBox<float>)
		.IsEnabled(TAttribute<bool>(this, &FSplinePointDetails::IsOnePointSelected))
		.Value(this, &FSplinePointDetails::GetInputKey)
		.UndeterminedString(LOCTEXT("Multiple", "Multiple"))
		.OnValueCommitted(this, &FSplinePointDetails::OnSetInputKey)
		.Font(IDetailLayoutBuilder::GetDetailFont())
	];

	// Position
	ChildrenBuilder.AddCustomRow(LOCTEXT("Position", "Position"))
	.Visibility(TAttribute<EVisibility>(this, &FSplinePointDetails::IsEnabled))
	.NameContent()
	.HAlign(HAlign_Left)
	.VAlign(VAlign_Center)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("Position", "Position"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MinDesiredWidth(375.0f)
	.MaxDesiredWidth(375.0f)
	[
		SNew(SVectorInputBox)
		.X(this, &FSplinePointDetails::GetPositionX)
		.Y(this, &FSplinePointDetails::GetPositionY)
		.Z(this, &FSplinePointDetails::GetPositionZ)
		.AllowResponsiveLayout(true)
		.OnXCommitted(this, &FSplinePointDetails::OnSetPosition, 0)
		.OnYCommitted(this, &FSplinePointDetails::OnSetPosition, 1)
		.OnZCommitted(this, &FSplinePointDetails::OnSetPosition, 2)
		.Font(IDetailLayoutBuilder::GetDetailFont())
	];

	// ArriveTangent
	ChildrenBuilder.AddCustomRow(LOCTEXT("ArriveTangent", "Arrive Tangent"))
	.Visibility(TAttribute<EVisibility>(this, &FSplinePointDetails::IsEnabled))
	.NameContent()
	.HAlign(HAlign_Left)
	.VAlign(VAlign_Center)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("ArriveTangent", "Arrive Tangent"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MinDesiredWidth(375.0f)
	.MaxDesiredWidth(375.0f)
	[
		SNew(SVectorInputBox)
		.X(this, &FSplinePointDetails::GetArriveTangentX)
		.Y(this, &FSplinePointDetails::GetArriveTangentY)
		.Z(this, &FSplinePointDetails::GetArriveTangentZ)
		.AllowResponsiveLayout(true)
		.OnXCommitted(this, &FSplinePointDetails::OnSetArriveTangent, 0)
		.OnYCommitted(this, &FSplinePointDetails::OnSetArriveTangent, 1)
		.OnZCommitted(this, &FSplinePointDetails::OnSetArriveTangent, 2)
		.Font(IDetailLayoutBuilder::GetDetailFont())
	];

	// LeaveTangent
	ChildrenBuilder.AddCustomRow(LOCTEXT("LeaveTangent", "Leave Tangent"))
	.Visibility(TAttribute<EVisibility>(this, &FSplinePointDetails::IsEnabled))
	.NameContent()
	.HAlign(HAlign_Left)
	.VAlign(VAlign_Center)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("LeaveTangent", "Leave Tangent"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MinDesiredWidth(375.0f)
	.MaxDesiredWidth(375.0f)
	[
		SNew(SVectorInputBox)
		.X(this, &FSplinePointDetails::GetLeaveTangentX)
		.Y(this, &FSplinePointDetails::GetLeaveTangentY)
		.Z(this, &FSplinePointDetails::GetLeaveTangentZ)
		.AllowResponsiveLayout(true)
		.OnXCommitted(this, &FSplinePointDetails::OnSetLeaveTangent, 0)
		.OnYCommitted(this, &FSplinePointDetails::OnSetLeaveTangent, 1)
		.OnZCommitted(this, &FSplinePointDetails::OnSetLeaveTangent, 2)
		.Font(IDetailLayoutBuilder::GetDetailFont())
	];

	// Rotation
	ChildrenBuilder.AddCustomRow(LOCTEXT("Rotation", "Rotation"))
	.Visibility(TAttribute<EVisibility>(this, &FSplinePointDetails::IsEnabled))
	.NameContent()
	.HAlign(HAlign_Left)
	.VAlign(VAlign_Center)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("Rotation", "Rotation"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MinDesiredWidth(375.0f)
	.MaxDesiredWidth(375.0f)
	[
		SNew(SVectorInputBox)
		.X(this, &FSplinePointDetails::GetRotationRoll)
		.Y(this, &FSplinePointDetails::GetRotationPitch)
		.Z(this, &FSplinePointDetails::GetRotationYaw)
		.AllowResponsiveLayout(true)
		.OnXCommitted(this, &FSplinePointDetails::OnSetRotation, 0)
		.OnYCommitted(this, &FSplinePointDetails::OnSetRotation, 1)
		.OnZCommitted(this, &FSplinePointDetails::OnSetRotation, 2)
		.Font(IDetailLayoutBuilder::GetDetailFont())
	];

	// Scale
	ChildrenBuilder.AddCustomRow(LOCTEXT("Scale", "Scale"))
	.Visibility(TAttribute<EVisibility>(this, &FSplinePointDetails::IsEnabled))
	.NameContent()
	.HAlign(HAlign_Left)
	.VAlign(VAlign_Center)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("Scale", "Scale"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MinDesiredWidth(375.0f)
	.MaxDesiredWidth(375.0f)
	[
		SNew(SVectorInputBox)
		.X(this, &FSplinePointDetails::GetScaleX)
		.Y(this, &FSplinePointDetails::GetScaleY)
		.Z(this, &FSplinePointDetails::GetScaleZ)
		.AllowResponsiveLayout(true)
		.OnXCommitted(this, &FSplinePointDetails::OnSetScale, 0)
		.OnYCommitted(this, &FSplinePointDetails::OnSetScale, 1)
		.OnZCommitted(this, &FSplinePointDetails::OnSetScale, 2)
		.Font(IDetailLayoutBuilder::GetDetailFont())
	];

	// Type
	ChildrenBuilder.AddCustomRow(LOCTEXT("Type", "Type"))
	.Visibility(TAttribute<EVisibility>(this, &FSplinePointDetails::IsEnabled))
	.NameContent()
	.HAlign(HAlign_Left)
	.VAlign(VAlign_Center)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("Type", "Type"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MinDesiredWidth(125.0f)
	.MaxDesiredWidth(125.0f)
	[
		SNew(SComboBox<TSharedPtr<FString>>)
		.OptionsSource(&SplinePointTypes)
		.OnGenerateWidget(this, &FSplinePointDetails::OnGenerateComboWidget)
		.OnSelectionChanged(this, &FSplinePointDetails::OnSplinePointTypeChanged)
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(this, &FSplinePointDetails::GetPointType)
		]
	];
}

void FSplinePointDetails::Tick(float DeltaTime)
{
	UpdateValues();
}

void FSplinePointDetails::UpdateValues()
{
	SplineComp = SplineVisualizer->GetEditedSplineComponent();
	SelectedKeys = SplineVisualizer->GetSelectedKeys();

	// Cache values to be shown by the details customization.
	// An unset optional value represents 'multiple values' (in the case where multiple points are selected).
	InputKey.Reset();
	Position.Reset();
	ArriveTangent.Reset();
	LeaveTangent.Reset();
	Rotation.Reset();
	Scale.Reset();
	PointType.Reset();

	if (SplineComp)
	{
		for (int32 Index : SelectedKeys)
		{
			InputKey.Add(SplineComp->GetSplinePointsPosition().Points[Index].InVal);
			Position.Add(SplineComp->GetSplinePointsPosition().Points[Index].OutVal);
			ArriveTangent.Add(SplineComp->GetSplinePointsPosition().Points[Index].ArriveTangent);
			LeaveTangent.Add(SplineComp->GetSplinePointsPosition().Points[Index].LeaveTangent);
			Rotation.Add(SplineComp->GetSplinePointsRotation().Points[Index].OutVal.Rotator());
			Scale.Add(SplineComp->GetSplinePointsScale().Points[Index].OutVal);
			PointType.Add(ConvertInterpCurveModeToSplinePointType(SplineComp->GetSplinePointsPosition().Points[Index].InterpMode));
		}
	}
}

FName FSplinePointDetails::GetName() const
{
	static const FName Name("SplinePointDetails");
	return Name;
}

void FSplinePointDetails::OnSetInputKey(float NewValue, ETextCommit::Type CommitInfo)
{
	if ((CommitInfo != ETextCommit::OnEnter && CommitInfo != ETextCommit::OnUserMovedFocus) || !SplineComp)
	{
		return;
	}

	check(SelectedKeys.Num() == 1);
	const int32 Index = *SelectedKeys.CreateConstIterator();
	TArray<FInterpCurvePoint<FVector>>& Positions = SplineComp->GetSplinePointsPosition().Points;

	const int32 NumPoints = Positions.Num();

	bool bModifyOtherPoints = false;
	if ((Index > 0 && NewValue <= Positions[Index - 1].InVal) ||
		(Index < NumPoints - 1 && NewValue >= Positions[Index + 1].InVal))
	{
		const FText Title(LOCTEXT("InputKeyTitle", "Input key out of range"));
		const FText Message(LOCTEXT("InputKeyMessage", "Spline input keys must be numerically ascending. Would you like to modify other input keys in the spline in order to be able to set this value?"));

		// Ensure input keys remain ascending
		if (FMessageDialog::Open(EAppMsgType::YesNo, Message, &Title) == EAppReturnType::No)
		{
			return;
		}

		bModifyOtherPoints = true;
	}

	const FScopedTransaction Transaction(LOCTEXT("SetSplinePointInputKey", "Set spline point input key"));
	SplineComp->Modify();

	TArray<FInterpCurvePoint<FQuat>>& Rotations = SplineComp->GetSplinePointsRotation().Points;
	TArray<FInterpCurvePoint<FVector>>& Scales = SplineComp->GetSplinePointsScale().Points;

	if (bModifyOtherPoints)
	{
		// Shuffle the previous or next input keys down or up so the input value remains in sequence
		if (Index > 0 && NewValue <= Positions[Index - 1].InVal)
		{
			float Delta = (NewValue - Positions[Index].InVal);
			for (int32 PrevIndex = 0; PrevIndex < Index; PrevIndex++)
			{
				Positions[PrevIndex].InVal += Delta;
				Rotations[PrevIndex].InVal += Delta;
				Scales[PrevIndex].InVal += Delta;
			}
		}
		else if (Index < NumPoints - 1 && NewValue >= Positions[Index + 1].InVal)
		{
			float Delta = (NewValue - Positions[Index].InVal);
			for (int32 NextIndex = Index + 1; NextIndex < NumPoints; NextIndex++)
			{
				Positions[NextIndex].InVal += Delta;
				Rotations[NextIndex].InVal += Delta;
				Scales[NextIndex].InVal += Delta;
			}
		}
	}

	Positions[Index].InVal = NewValue;
	Rotations[Index].InVal = NewValue;
	Scales[Index].InVal = NewValue;

	SplineComp->UpdateSpline();
	SplineComp->bSplineHasBeenEdited = true;
	FComponentVisualizer::NotifyPropertyModified(SplineComp, SplineCurvesProperty);
	UpdateValues();
}

void FSplinePointDetails::OnSetPosition(float NewValue, ETextCommit::Type CommitInfo, int32 Axis)
{
	if (!SplineComp)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("SetSplinePointPosition", "Set spline point position"));
	SplineComp->Modify();

	for (int32 Index : SelectedKeys)
	{
		FVector PointPosition = SplineComp->GetSplinePointsPosition().Points[Index].OutVal;
		PointPosition.Component(Axis) = NewValue;
		SplineComp->GetSplinePointsPosition().Points[Index].OutVal = PointPosition;
	}

	SplineComp->UpdateSpline();
	SplineComp->bSplineHasBeenEdited = true;
	FComponentVisualizer::NotifyPropertyModified(SplineComp, SplineCurvesProperty);
	UpdateValues();
}

void FSplinePointDetails::OnSetArriveTangent(float NewValue, ETextCommit::Type CommitInfo, int32 Axis)
{
	if (!SplineComp)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("SetSplinePointTangent", "Set spline point tangent"));
	SplineComp->Modify();

	for (int32 Index : SelectedKeys)
	{
		FVector PointTangent = SplineComp->GetSplinePointsPosition().Points[Index].ArriveTangent;
		PointTangent.Component(Axis) = NewValue;
		SplineComp->GetSplinePointsPosition().Points[Index].ArriveTangent = PointTangent;
		SplineComp->GetSplinePointsPosition().Points[Index].InterpMode = CIM_CurveUser;
	}

	SplineComp->UpdateSpline();
	SplineComp->bSplineHasBeenEdited = true;
	FComponentVisualizer::NotifyPropertyModified(SplineComp, SplineCurvesProperty);
	UpdateValues();
}

void FSplinePointDetails::OnSetLeaveTangent(float NewValue, ETextCommit::Type CommitInfo, int32 Axis)
{
	if (!SplineComp)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("SetSplinePointTangent", "Set spline point tangent"));
	SplineComp->Modify();

	for (int32 Index : SelectedKeys)
	{
		FVector PointTangent = SplineComp->GetSplinePointsPosition().Points[Index].LeaveTangent;
		PointTangent.Component(Axis) = NewValue;
		SplineComp->GetSplinePointsPosition().Points[Index].LeaveTangent = PointTangent;
		SplineComp->GetSplinePointsPosition().Points[Index].InterpMode = CIM_CurveUser;
	}

	SplineComp->UpdateSpline();
	SplineComp->bSplineHasBeenEdited = true;
	FComponentVisualizer::NotifyPropertyModified(SplineComp, SplineCurvesProperty);
	UpdateValues();
}

void FSplinePointDetails::OnSetRotation(float NewValue, ETextCommit::Type CommitInfo, int32 Axis)
{
	if (!SplineComp)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("SetSplinePointRotation", "Set spline point rotation"));
	SplineComp->Modify();

	for (int32 Index : SelectedKeys)
	{
		FRotator PointRotation = SplineComp->GetSplinePointsRotation().Points[Index].OutVal.Rotator();

		switch (Axis)
		{
			case 0: PointRotation.Roll = NewValue; break;
			case 1: PointRotation.Pitch = NewValue; break;
			case 2: PointRotation.Yaw = NewValue; break;
		}

		SplineComp->GetSplinePointsRotation().Points[Index].OutVal = PointRotation.Quaternion();
	}

	SplineComp->UpdateSpline();
	SplineComp->bSplineHasBeenEdited = true;
	FComponentVisualizer::NotifyPropertyModified(SplineComp, SplineCurvesProperty);
	UpdateValues();
}

void FSplinePointDetails::OnSetScale(float NewValue, ETextCommit::Type CommitInfo, int32 Axis)
{
	if (!SplineComp)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("SetSplinePointScale", "Set spline point scale"));
	SplineComp->Modify();

	for (int32 Index : SelectedKeys)
	{
		FVector PointScale = SplineComp->GetSplinePointsScale().Points[Index].OutVal;
		PointScale.Component(Axis) = NewValue;
		SplineComp->GetSplinePointsScale().Points[Index].OutVal = PointScale;
	}

	SplineComp->UpdateSpline();
	SplineComp->bSplineHasBeenEdited = true;
	FComponentVisualizer::NotifyPropertyModified(SplineComp, SplineCurvesProperty);
	UpdateValues();
}

FText FSplinePointDetails::GetPointType() const
{
	if (PointType.Value.IsSet())
	{
		return FText::FromString(*SplinePointTypes[PointType.Value.GetValue()]);
	}

	return LOCTEXT("MultipleTypes", "Multiple Types");
}

void FSplinePointDetails::OnSplinePointTypeChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo)
{
	if (!SplineComp)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("SetSplinePointType", "Set spline point type"));
	SplineComp->Modify();

	EInterpCurveMode Mode = ConvertSplinePointTypeToInterpCurveMode((ESplinePointType::Type)SplinePointTypes.Find(NewValue));

	for (int32 Index : SelectedKeys)
	{
		SplineComp->GetSplinePointsPosition().Points[Index].InterpMode = Mode;
	}

	SplineComp->UpdateSpline();
	SplineComp->bSplineHasBeenEdited = true;
	FComponentVisualizer::NotifyPropertyModified(SplineComp, SplineCurvesProperty);
	UpdateValues();
}

TSharedRef<SWidget> FSplinePointDetails::OnGenerateComboWidget(TSharedPtr<FString> InComboString)
{
	return SNew(STextBlock)
		.Text(FText::FromString(*InComboString))
		.Font(IDetailLayoutBuilder::GetDetailFont());
}

////////////////////////////////////

TSharedRef<IDetailCustomization> FSplineComponentDetails::MakeInstance()
{
	return MakeShareable(new FSplineComponentDetails);
}

void FSplineComponentDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// Hide the SplineCurves property
	TSharedPtr<IPropertyHandle> SplineCurvesProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(USplineComponent, SplineCurves));
	SplineCurvesProperty->MarkHiddenByCustomization();

	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory("Selected Points");
	TSharedRef<FSplinePointDetails> SplinePointDetails = MakeShareable(new FSplinePointDetails);
	Category.AddCustomBuilder(SplinePointDetails);
}

#undef LOCTEXT_NAMESPACE
