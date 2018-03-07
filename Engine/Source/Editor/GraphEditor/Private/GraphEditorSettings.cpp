// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GraphEditorSettings.h"
#include "UObject/UnrealType.h"
#include "EditorStyleSet.h"


/* UGraphEditorSettings structors
 *****************************************************************************/

UGraphEditorSettings::UGraphEditorSettings( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
	, PanningMouseButton(EGraphPanningMouseButton::Right)
	, PaddingAbovePin(4.0f)
	, PaddingBelowPin(4.0f)
	, PaddingRightOfInput(10.0f)
	, PaddingLeftOfOutput(10.0f)
	, PaddingTowardsNodeEdge(10.0f)
	, bTreatSplinesLikePins(true)
	, SplineHoverTolerance(2.0f)
	, ForwardSplineHorizontalDeltaRange(1000.0f)
	, ForwardSplineVerticalDeltaRange(1000.0f)
	, ForwardSplineTangentFromHorizontalDelta(1.0f, 0.0f)
	, ForwardSplineTangentFromVerticalDelta(1.0f, 0.0f)
	, BackwardSplineHorizontalDeltaRange(200.0f)
	, BackwardSplineVerticalDeltaRange(200.0f)
	, BackwardSplineTangentFromHorizontalDelta(3.0f, 0.0f)
	, BackwardSplineTangentFromVerticalDelta(1.5f, 0.0f)
{
	DataPinStyle = BPST_VariantA;

	// graph node pin type colors
	DefaultPinTypeColor = FLinearColor(0.750000f, 0.6f, 0.4f, 1.0f);			// light brown
	ExecutionPinTypeColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);				// white
	BooleanPinTypeColor = FLinearColor(0.300000f, 0.0f, 0.0f, 1.0f);			// maroon
	BytePinTypeColor = FLinearColor(0.0f, 0.160000f, 0.131270f, 1.0f);			// dark green
	ClassPinTypeColor = FLinearColor(0.1f, 0.0f, 0.5f, 1.0f);					// deep purple (violet)
	IntPinTypeColor = FLinearColor(0.013575f, 0.770000f, 0.429609f, 1.0f);		// green-blue
	FloatPinTypeColor = FLinearColor(0.357667f, 1.0f, 0.060000f, 1.0f);			// bright green
	NamePinTypeColor = FLinearColor(0.607717f, 0.224984f, 1.0f, 1.0f);			// lilac
	DelegatePinTypeColor = FLinearColor(1.0f, 0.04f, 0.04f, 1.0f);				// bright red
	ObjectPinTypeColor = FLinearColor(0.0f, 0.4f, 0.910000f, 1.0f);				// sharp blue
	SoftObjectPinTypeColor = FLinearColor(0.3f, 1.0f, 1.0f, 1.0f);					
	SoftClassPinTypeColor = FLinearColor(1.0f, 0.3f, 1.0f, 1.0f);				
	InterfacePinTypeColor = FLinearColor(0.8784f, 1.0f, 0.4f, 1.0f);			// pale green
	StringPinTypeColor = FLinearColor(1.0f, 0.0f, 0.660537f, 1.0f);				// bright pink
	TextPinTypeColor = FLinearColor(0.8f, 0.2f, 0.4f, 1.0f);					// salmon (light pink)
	StructPinTypeColor = FLinearColor(0.0f, 0.1f, 0.6f, 1.0f);					// deep blue
	WildcardPinTypeColor = FLinearColor(0.220000f, 0.195800f, 0.195800f, 1.0f);	// dark gray
	VectorPinTypeColor = FLinearColor(1.0f, 0.591255f, 0.016512f, 1.0f);		// yellow
	RotatorPinTypeColor = FLinearColor(0.353393f, 0.454175f, 1.0f, 1.0f);		// periwinkle 
	TransformPinTypeColor = FLinearColor(1.0f, 0.172585f, 0.0f, 1.0f);			// orange
	IndexPinTypeColor = FLinearColor(0.013575f, 0.770000f, 0.429609f, 1.0f);	// green-blue

	DefaultDataWireThickness = 1.0f;
	DefaultExecutionWireThickness = 3.0f;

	// graph debugging visuals
	TraceAttackColor = FLinearColor(1.0f, 0.05f, 0.0f, 1.0f);
	TraceAttackWireThickness = 12.0f;
	TraceSustainColor = FLinearColor(1.0f, 0.7f, 0.4f, 1.0f);
	TraceSustainWireThickness = 8.0f;
	TraceReleaseColor = FLinearColor(0.5f, 0.5f, 0.5f, 1.0f);
	TraceReleaseWireThickness = 2.0f;

	// graph debugging exec curve constants
	TracePositionBonusPeriod = 0.5f;
	TracePositionExponent = 5.0f;
	TraceAttackHoldPeriod = 0.3f;
	TraceDecayPeriod = 0.4f;
	TraceDecayExponent = 1.8f;
	TraceSustainHoldPeriod = 0.4f;
	TraceReleasePeriod = 1.5f;
	TraceReleaseExponent = 1.4f;

	// Blueprint editor graph node title colors
	EventNodeTitleColor = FLinearColor(1.f, 0.0f, 0.0f, 1.0f);
	FunctionCallNodeTitleColor = FLinearColor(0.190525f, 0.583898f, 1.0f, 1.0f);
	PureFunctionCallNodeTitleColor = FLinearColor(0.4f, 0.850000f, 0.350000f, 1.0f);
	ParentFunctionCallNodeTitleColor = FLinearColor(1.0f, 0.170000f, 0.0f, 1.0f);
	FunctionTerminatorNodeTitleColor = FLinearColor(0.6f, 0.0f, 1.0f, 1.0f);
	ExecBranchNodeTitleColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
	ExecSequenceNodeTitleColor = FLinearColor(0.8f, 0.4f, 0.4f, 1.0f);
	ResultNodeTitleColor = FLinearColor(1.0f, 0.65f, 0.4f, 1.0f);
	DefaultCommentNodeTitleColor = FLinearColor::White;
}

#if WITH_EDITOR
void UGraphEditorSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	if ((PropertyName == GET_MEMBER_NAME_CHECKED(UGraphEditorSettings, DataPinStyle)) || (PropertyName == GET_MEMBER_NAME_CHECKED(UGraphEditorSettings, PaddingTowardsNodeEdge)))
	{
		// Invalidate all node graph editors
		//@TODO: That thing I said
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

FMargin UGraphEditorSettings::GetInputPinPadding() const
{
	const float HPad = FMath::Max<float>(PaddingTowardsNodeEdge, 0.0f);
	return FMargin(HPad, PaddingAbovePin, PaddingRightOfInput, PaddingBelowPin);
}

FMargin UGraphEditorSettings::GetOutputPinPadding() const
{
	const float HPad = FMath::Max<float>(PaddingTowardsNodeEdge, 0.0f);
	return FMargin(PaddingLeftOfOutput, PaddingAbovePin, HPad, PaddingBelowPin);
}

FMargin UGraphEditorSettings::GetNonPinNodeBodyPadding() const
{
	const float NegativeHPad = FMath::Max<float>(-PaddingTowardsNodeEdge, 0.0f);
	return FMargin(NegativeHPad, 0.0f, NegativeHPad, 0.0f);
}

FVector2D UGraphEditorSettings::GetShadowDeltaSize() const
{
	FVector2D ShadowSize = FEditorStyle::GetVector(TEXT("Graph.Node.ShadowSize"));
	ShadowSize.X += FMath::Min<float>(PaddingTowardsNodeEdge, 0.0f);
	return ShadowSize;
}

FVector2D UGraphEditorSettings::ComputeSplineTangent(const FVector2D& Start, const FVector2D& End) const
{
	const FVector2D DeltaPos = End - Start;
	const bool bGoingForward = DeltaPos.X >= 0.0f;

	const float ClampedTensionX = FMath::Min<float>(FMath::Abs<float>(DeltaPos.X), bGoingForward ? ForwardSplineHorizontalDeltaRange : BackwardSplineHorizontalDeltaRange);
	const float ClampedTensionY = FMath::Min<float>(FMath::Abs<float>(DeltaPos.Y), bGoingForward ? ForwardSplineVerticalDeltaRange : BackwardSplineVerticalDeltaRange);

	if (bGoingForward)
	{
		return (ClampedTensionX * ForwardSplineTangentFromHorizontalDelta) + (ClampedTensionY * ForwardSplineTangentFromVerticalDelta);
	}
	else
	{
		return (ClampedTensionX * BackwardSplineTangentFromHorizontalDelta) + (ClampedTensionY * BackwardSplineTangentFromVerticalDelta);
	}
}
