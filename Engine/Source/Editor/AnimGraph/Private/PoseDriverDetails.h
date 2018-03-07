// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "IDetailCustomization.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Curves/CurveOwnerInterface.h"

class IDetailLayoutBuilder;
class UAnimGraphNode_PoseDriver;
class IPropertyHandle;
class FPoseDriverDetails;
class SExpandableArea;
class SCurveEditor;
class SComboButton;
struct FPoseDriverTarget;

/** Entry in backing list for target list widget */
struct FPDD_TargetInfo 
{
	int32 TargetIndex;

	/** Static function for creating a new item, but ensures that you can only have a TSharedRef to one */
	static TSharedRef<FPDD_TargetInfo> Make(int32 InTargetIndex)
	{
		return MakeShareable(new FPDD_TargetInfo(InTargetIndex));
	}

	/** Executed when we want to expand this target info UI */
	FSimpleMulticastDelegate ExpandTargetDelegate;

protected:
	/** Hidden constructor, always use Make above */
	FPDD_TargetInfo(int32 InTargetIndex)
		: TargetIndex(InTargetIndex)
	{}
};

/** Type of target list widget */
typedef SListView< TSharedPtr<FPDD_TargetInfo> > SPDD_TargetListType;


/** Widget for displaying info on a paricular target */
class SPDD_TargetRow : public SMultiColumnTableRow< TSharedPtr<FPDD_TargetInfo> > ,
	public FCurveOwnerInterface
{
public:

	SLATE_BEGIN_ARGS(SPDD_TargetRow) {}
		SLATE_ARGUMENT(TWeakPtr<FPDD_TargetInfo>, TargetInfo)
		SLATE_ARGUMENT(TWeakPtr<FPoseDriverDetails>, PoseDriverDetails)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTableView);

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

	/** Return underlying FPoseDriverTarget this widget represents */
	FPoseDriverTarget* GetTarget() const;
	/** Get the pose drive node we are editing */
	UAnimGraphNode_PoseDriver* GetPoseDriverGraphNode() const;
	/** If we are editing rotation or translation */
	bool IsEditingRotation() const;
	/** Call when target is modified, so change is propagated to preview instance */
	void NotifyTargetChanged();
	/** Get current weight of this target in preview */
	float GetTargetWeight() const;
	/** Get index of target this represents on pose driver */
	int32 GetTargetIndex() const;

	int32 GetTransRotWidgetIndex() const;
	TOptional<float> GetTranslation(int32 BoneIndex, EAxis::Type Axis) const;
	TOptional<float> GetRotation(int32 BoneIndex, EAxis::Type Axis) const;
	TOptional<float> GetScale() const;
	FText GetTargetTitleText() const;
	FText GetTargetWeightText() const;
	FSlateColor GetWeightBarColor() const;

	void SetTranslation(float NewTrans, int32 BoneIndex, EAxis::Type Axis);
	void SetRotation(float NewRot, int32 BoneIndex, EAxis::Type Axis);
	void SetScale(float NewScale);
	void SetDrivenNameText(const FText& NewText, ETextCommit::Type CommitType);

	void ExpandTargetInfo();
	void OnTargetExpansionChanged(bool bExpanded);
	FText GetDrivenNameText() const;
	void OnDrivenNameChanged(TSharedPtr<FName> NewName, ESelectInfo::Type SelectInfo);
	TSharedRef<SWidget> MakeDrivenNameWidget(TSharedPtr<FName> InItem);

	bool IsCustomCurveEnabled() const;
	void OnApplyCustomCurveChanged(const ECheckBoxState NewCheckState);

	/** Remove this target from  */
	void RemoveTarget();

	/** FCurveOwnerInterface interface */
	virtual TArray<FRichCurveEditInfoConst> GetCurves() const override;
	virtual TArray<FRichCurveEditInfo> GetCurves() override;
	virtual void ModifyOwner() override;
	virtual TArray<const UObject*> GetOwners() const override;
	virtual void MakeTransactional() override;
	virtual void OnCurveChanged(const TArray<FRichCurveEditInfo>& ChangedCurveEditInfos) override;
	virtual bool IsValidCurve(FRichCurveEditInfo CurveInfo) override;

	/** Expandable area used for this widget */
	TSharedPtr<SExpandableArea> ExpandArea;

	/** Curve editor for custom curves */
	TSharedPtr<SCurveEditor> CurveEditor;

	/** Pointer back to owning customization */
	TWeakPtr<FPoseDriverDetails> PoseDriverDetailsPtr;

	/** Info that this widget represents */
	TWeakPtr<FPDD_TargetInfo> TargetInfoPtr;
};


/** Details customization for PoseDriver node */
class FPoseDriverDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	// Begin IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	// End IDetailCustomization interface

	void ClickedOnCopyFromPoseAsset();
	bool CopyFromPoseAssetIsEnabled() const;
	void ClickedOnAutoScaleFactors();
	bool AutoScaleFactorsIsEnabled() const;
	FReply ClickedAddTarget();
	TSharedRef<ITableRow> GenerateTargetRow(TSharedPtr<FPDD_TargetInfo> InInfo, const TSharedRef<STableViewBase>& OwnerTable);
	void OnTargetSelectionChanged(TSharedPtr<FPDD_TargetInfo> InInfo, ESelectInfo::Type SelectInfo);
	void OnPoseAssetChanged();
	void OnSourceBonesChanged();
	void SelectedTargetChanged();

	/** Get tools popup menu content */
	TSharedRef<SWidget> GetToolsMenuContent();
	FSlateColor GetToolsForegroundColor() const;

	/** Remove a target from node */
	void RemoveTarget(int32 TargetIndex);

	/** Set the currently selected Target */
	void SelectTarget(int32 TargetIndex, bool bExpandTarget);

	/** Util to get the first selected PoseDriver node */
	UAnimGraphNode_PoseDriver* GetFirstSelectedPoseDriver() const;

	/**  Refresh list of TargetInfos, mirroring PoseTargets list on node */
	void UpdateTargetInfosList();

	/** Update list of options for targets to drive (use by combo box) */
	void UpdateDrivenNameOptions();


	/** Cached array of selected objects */
	TArray< TWeakObjectPtr<UObject> > SelectedObjectsList;
	/** Array source for target list */
	TArray< TSharedPtr<FPDD_TargetInfo> > TargetInfos;
	/** List of things a target can drive (curves or morphs), used by combo box */
	TArray< TSharedPtr<FName> > DrivenNameOptions;
	/** Target list widget */
	TSharedPtr<SPDD_TargetListType> TargetListWidget;
	/** Property handle to node */
	TSharedPtr<IPropertyHandle> NodePropHandle;
	/** Pointer to Tools menu button */
	TSharedPtr<SComboButton> ToolsButton;
};
