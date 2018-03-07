// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Attribute.h"
#include "Layout/Visibility.h"
#include "AnimGraphNode_SkeletalControlBase.h"
#include "BoneControllers/AnimNode_BoneDrivenController.h"
#include "AnimGraphNode_BoneDrivenController.generated.h"

class FCompilerResultsLog;
class FPrimitiveDrawInterface;
class IDetailCategoryBuilder;
class IDetailLayoutBuilder;
class IPropertyHandle;
class USkeletalMeshComponent;

/**
 * This is the 'source version' of a bone driven controller, which maps part of the state from one bone to another (e.g., 2 * source.x -> target.z)
 */
UCLASS()
class ANIMGRAPH_API UAnimGraphNode_BoneDrivenController : public UAnimGraphNode_SkeletalControlBase
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category = Settings)
	FAnimNode_BoneDrivenController Node;

public:
	// UObject interface
	virtual void Serialize(FArchive& Ar) override;
	// End of UObject interface

	// UEdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	// End of UEdGraphNode interface

	// UAnimGraphNode_Base interface
	virtual void ValidateAnimNodeDuringCompilation(USkeleton* ForSkeleton, FCompilerResultsLog& MessageLog) override;
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	// End of UAnimGraphNode_Base interface

	// UAnimGraphNode_SkeletalControlBase interface
	virtual void Draw(FPrimitiveDrawInterface* PDI, USkeletalMeshComponent* SkelMeshComp) const override;
	// End of UAnimGraphNode_SkeletalControlBase interface

protected:

	// UAnimGraphNode_SkeletalControlBase protected interface
	virtual FText GetControllerDescription() const override;
	virtual const FAnimNode_SkeletalControlBase* GetNode() const override { return &Node; }
	// End of UAnimGraphNode_SkeletalControlBase protected interface

	// Should non-curve mapping values be shown (multiplier, range)?
	static EVisibility AreNonCurveMappingValuesVisible(IDetailLayoutBuilder* DetailLayoutBuilder);
	static EVisibility AreRemappingValuesVisible(IDetailLayoutBuilder* DetailLayoutBuilder);

	// Should destination bone or morph target properties be visible
	static EVisibility AreTargetBonePropertiesVisible(IDetailLayoutBuilder* DetailLayoutBuilder);
	static EVisibility AreTargetCurvePropertiesVisible(IDetailLayoutBuilder* DetailLayoutBuilder);

	static void AddTripletPropertyRow(const FText& Name, const FText& Tooltip, IDetailCategoryBuilder& Category, TSharedRef<IPropertyHandle> PropertyHandle, const FName XPropertyName, const FName YPropertyName, const FName ZPropertyName, TAttribute<EVisibility> VisibilityAttribute);
	static void AddRangePropertyRow(const FText& Name, const FText& Tooltip, IDetailCategoryBuilder& Category, TSharedRef<IPropertyHandle> PropertyHandle, const FName MinPropertyName, const FName MaxPropertyName, TAttribute<EVisibility> VisibilityAttribute);
	static FText ComponentTypeToText(EComponentType::Type Component);
};
