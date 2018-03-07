// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "Styling/SlateColor.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Animation/SmartName.h"
#include "IPersonaPreviewScene.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/SListView.h"
#include "Animation/AnimInstance.h"
#include "EditorObjectsTracker.h"
#include "PersonaDelegates.h"

class FUICommandList;
class IEditableSkeleton;
class SAnimCurveViewer;

//////////////////////////////////////////////////////////////////////////
// FDisplayedAnimCurveInfo

class FDisplayedAnimCurveInfo
{
public:
	FSmartName SmartName;
	float Weight;
	bool bAutoFillData;
	TWeakPtr<class IEditableSkeleton> EditableSkeleton;		// The skeleton we're associated with
	TSharedPtr<SInlineEditableTextBlock> EditableText;	// The editable text box in the list, used to focus from the context menu
	FName ContainerName;	// The container in the skeleton this name resides in
	class UEditorAnimCurveBoneLinks* EditorMirrorObject;

	/** Static function for creating a new item, but ensures that you can only have a TSharedRef to one */
	static TSharedRef<FDisplayedAnimCurveInfo> Make(TWeakPtr<class IEditableSkeleton> InEditableSkeleton, const FName& InContainerName, const FSmartName& InSmartName, class UEditorAnimCurveBoneLinks* InEditorMirrorObject)
	{
		return MakeShareable(new FDisplayedAnimCurveInfo(InEditableSkeleton, InContainerName, InSmartName, InEditorMirrorObject));
	}

protected:
	/** Hidden constructor, always use Make above */
	FDisplayedAnimCurveInfo(TWeakPtr<class IEditableSkeleton> InEditableSkeleton, const FName& InContainerName, const FSmartName& InSmartName, class UEditorAnimCurveBoneLinks* InEditorMirrorObject)
		: SmartName(InSmartName)
		, Weight( 0 )
		, bAutoFillData(true)
		, EditableSkeleton(InEditableSkeleton)
		, ContainerName(InContainerName)
		, EditorMirrorObject(InEditorMirrorObject)
	{}
};

typedef SListView< TSharedPtr<FDisplayedAnimCurveInfo> > SAnimCurveListType;

//////////////////////////////////////////////////////////////////////////
// SAnimCurveListRow

class SAnimCurveListRow : public SMultiColumnTableRow< TSharedPtr<FDisplayedAnimCurveInfo> >
{
public:

	SLATE_BEGIN_ARGS(SAnimCurveListRow) {}

		/** The item for this row **/
		SLATE_ARGUMENT(TSharedPtr<FDisplayedAnimCurveInfo>, Item)

		/* The SAnimCurveViewer that we push the morph target weights into */
		SLATE_ARGUMENT(class TWeakPtr<SAnimCurveViewer>, AnimCurveViewerPtr)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTableView, const TSharedRef<class IPersonaPreviewScene>& InPreviewScene);

	/** Overridden from SMultiColumnTableRow.  Generates a widget for this column of the tree row. */
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

private:

	/**
	* Called when the user changes the value of the SSpinBox
	*
	* @param NewWeight - The new number the SSpinBox is set to
	*
	*/
	void OnAnimCurveWeightChanged(float NewWeight);
	/**
	* Called when the user types the value and enters
	*
	* @param NewWeight - The new number the SSpinBox is set to
	*
	*/
	void OnAnimCurveWeightValueCommitted(float NewWeight, ETextCommit::Type CommitType);

	/** Auto fill check call back functions */
	void OnAnimCurveAutoFillChecked(ECheckBoxState InState);
	ECheckBoxState IsAnimCurveAutoFillChangedChecked() const;

	/* Curve Flag checks for morphtarget or material */
	void OnAnimCurveTypeBoxChecked(ECheckBoxState InState, bool bMorphTarget);
	ECheckBoxState IsAnimCurveTypeBoxChangedChecked(bool bMorphTarget) const;

	/** Returns the weight of this curve */
	float GetWeight() const;
	/** Returns name of this curve */
	FText GetItemName() const;
	/** Get text we are filtering for */
	FText GetFilterText() const;
	/** Return color for text of item */
	FSlateColor GetItemTextColor() const;

	/** Get current active weight. Returns false if not currently active */
	bool GetActiveWeight(float& OutWeight) const;

	/* The SAnimCurveViewer that we push the morph target weights into */
	TWeakPtr<SAnimCurveViewer> AnimCurveViewerPtr;

	/** The name and weight of the morph target */
	TSharedPtr<FDisplayedAnimCurveInfo>	Item;

	/** Preview scene used to update on scrub */
	TWeakPtr<class IPersonaPreviewScene> PreviewScenePtr;
	/** Returns curve type widget constructed */
	TSharedRef<SWidget> GetCurveTypeWidget();

	/** returns display text for number of connected joint setting */
	FText GetNumConntectedBones() const;
};

//////////////////////////////////////////////////////////////////////////
// SAnimCurveViewer

class SAnimCurveViewer : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SAnimCurveViewer )
	{}
	
	SLATE_END_ARGS()

	/**
	* Slate construction function
	*
	* @param InArgs - Arguments passed from Slate
	*
	*/
	void Construct( const FArguments& InArgs, const TSharedRef<class IEditableSkeleton>& InEditableSkeleton, const TSharedRef<class IPersonaPreviewScene>& InPreviewScene, FSimpleMulticastDelegate& InOnPostUndo, FOnObjectsSelected InOnObjectsSelected);

	/**
	* Destructor - resets the animation curve
	*
	*/
	virtual ~SAnimCurveViewer();

	/** SWidget interface */
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent);

	/**
	* Is registered with Persona to handle when its preview mesh is changed.
	*
	* @param NewPreviewMesh - The new preview mesh being used by Persona
	*
	*/
	void OnPreviewMeshChanged(class USkeletalMesh* OldPreviewMesh, class USkeletalMesh* NewPreviewMesh);

	/**
	* Is registered with Persona to handle when its preview asset is changed.
	*
	* Pose Asset will have to add curve manually
	*/
	void OnPreviewAssetChanged(class UAnimationAsset* NewPreviewAsset);

	/** Is registered with Persona to handle when curves change. */
	void OnCurvesChanged();

	/**
	* Filters the SListView when the user changes the search text box (NameFilterBox)
	*
	* @param SearchText - The text the user has typed
	*
	*/
	void OnFilterTextChanged( const FText& SearchText );




	/**
	* Filters the SListView when the user hits enter or clears the search box
	* Simply calls OnFilterTextChanged
	*
	* @param SearchText - The text the user has typed
	* @param CommitInfo - Not used
	*
	*/
	void OnFilterTextCommitted( const FText& SearchText, ETextCommit::Type CommitInfo );

	/**
	* Create a widget for an entry in the tree from an info
	*
	* @param InInfo - Shared pointer to the morph target we're generating a row for
	* @param OwnerTable - The table that owns this row
	*
	* @return A new Slate widget, containing the UI for this row
	*/
	TSharedRef<ITableRow> GenerateAnimCurveRow(TSharedPtr<FDisplayedAnimCurveInfo> InInfo, const TSharedRef<STableViewBase>& OwnerTable);

	/**
	* Adds a curve override or updates the weight for an existing one
	*
	* @param Name - Name of the curve we want to override
	* @param Weight - How much of this curve to apply (0.0 - 1.0)
	*
	*/
	void AddAnimCurveOverride( FName& Name, float Weight);

	/** Removes an existing curve override */
	void RemoveAnimCurveOverride(FName& Name);

	/** Is there currently an override for this curve, and if so, what is its weight */
	bool GetAnimCurveOverride(FName& Name, float& Weight);

	/**
	* Tells the AnimInstance to reset all of its morph target curves
	*
	*/
	void ResetAnimCurves();
	
	/**
	* Accessor so our rows can grab the filtertext for highlighting
	*
	*/
	FText& GetFilterText() { return FilterText; }

	/**
	 * Refreshes the morph target list after an undo
	 */
	void OnPostUndo();

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime);

	void RefreshCurveList();

	// When a name is committed after being edited in the list
	virtual void OnNameCommitted(const FText& NewName, ETextCommit::Type CommitType, TSharedPtr<FDisplayedAnimCurveInfo> Item);

private:

	void BindCommands();

	/** Handler for context menus */
	TSharedPtr<SWidget> OnGetContextMenuContent() const;
	void OnSelectionChanged(TSharedPtr<FDisplayedAnimCurveInfo> InItem, ESelectInfo::Type SelectInfo);

	/**
	* Clears and rebuilds the table, according to an optional search string
	*
	* @param SearchText - Optional search string
	*
	*/
	void CreateAnimCurveList( const FString& SearchText = FString() );
	void CreateAnimCurveTypeList(TSharedRef<SHorizontalBox> HorizontalBox);

	void ApplyCustomCurveOverride(UAnimInstance* AnimInstance) const;

	void OnDeleteNameClicked();
	bool CanDelete();

	void OnRenameClicked();
	bool CanRename();

	void OnAddClicked();

	ECheckBoxState IsShowingAllCurves() const;
	void OnToggleShowingAllCurves(ECheckBoxState NewState);

	bool IsCurveFilterEnabled() const;


	// Adds a new smartname entry to the skeleton in the container we are managing
	void CreateNewNameEntry(const FText& CommittedText, ETextCommit::Type CommitType);

	/** Handle smart name (i.e. curve) removal */
	void HandleSmartNamesChange(const FName& InContainerName);

	/** Get the SmartNameMapping for anim curves */
	const struct FSmartNameMapping* GetAnimCurveMapping();

	/** Get the anim instance we are viewing */
	UAnimInstance* GetAnimInstance() const;

	/** Pointer to the preview scene we are bound to */
	TWeakPtr<class IPersonaPreviewScene> PreviewScenePtr;

	/** Pointer to the editable skeleton */
	TWeakPtr<class IEditableSkeleton> EditableSkeletonPtr;

	/** Box to filter to a specific morph target name */
	TSharedPtr<SSearchBox>	NameFilterBox;

	/** A list of animation curve. Used by the AnimCurveListView. */
	TArray< TSharedPtr<FDisplayedAnimCurveInfo> > AnimCurveList;

	/** The skeletal mesh that we grab the animation curve from */
	UAnimInstance* CachedPreviewInstance;						

	/** Widget used to display the list of animation curve */
	TSharedPtr<SAnimCurveListType> AnimCurveListView;

	/** Name of the skeleton smart name container to display in the list */
	FName ContainerName;

	/** Current text typed into NameFilterBox */
	FText FilterText;

	int32 CurrentCurveFlag;

	bool bShowAllCurves;

	TMap<FName, float> OverrideCurves;

	/** Commands that are bound to delegates*/
	TSharedPtr<FUICommandList> UICommandList;

	friend class SAnimCurveListRow;
	friend class SAnimCurveTypeList;

	/** Tracks objects created for the details panel */
	FEditorObjectTracker EditorObjectTracker;

	/** Delegate called to select objects */
	FOnObjectsSelected OnObjectsSelected;

	/** apply curve bone links from editor mirror object to skeleton */
	void ApplyCurveBoneLinks(class UEditorAnimCurveBoneLinks* EditorObj);

	/** Delegate handle for HandleSmartNameRemoved callback */
	FDelegateHandle SmartNameChangedHandle;
};