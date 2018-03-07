// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "Misc/Attribute.h"
#include "Animation/Skeleton.h"
#include "Styling/SlateColor.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "EditableSkeleton.h"
#include "AssetData.h"
#include "Engine/SkeletalMeshSocket.h"
#include "ISkeletonTree.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STreeView.h"
#include "Preferences/PersonaOptions.h"
#include "ISkeletonTreeBuilder.h"
#include "ISkeletonTreeItem.h"
#include "SkeletonTreeBuilder.h"
#include "SSearchBox.h"
#include "EditorUndoClient.h"
#include "GCObject.h"

class FMenuBuilder;
class FSkeletonTreeAttachedAssetItem;
class FSkeletonTreeBoneItem;
class FSkeletonTreeSocketItem;
class FSkeletonTreeVirtualBoneItem;
class FTextFilterExpressionEvaluator;
class FUICommandList;
class IPersonaPreviewScene;
class SBlendProfilePicker;
class SComboButton;
class UBlendProfile;
struct FNotificationInfo;

//////////////////////////////////////////////////////////////////////////
// SSkeletonTree

PRAGMA_DISABLE_DEPRECATION_WARNINGS
class SSkeletonTree : public ISkeletonTree, public FEditorUndoClient, public FGCObject
{
public:
	SLATE_BEGIN_ARGS( SSkeletonTree )
		: _IsEditable(true)
		{}

	SLATE_ATTRIBUTE( bool, IsEditable )

	SLATE_ARGUMENT( TSharedPtr<class ISkeletonTreeBuilder>, Builder )

	SLATE_END_ARGS()
public:
	SSkeletonTree()
		: bSelectingSocket(false)
		, bSelectingBone(false)
		
	{}

	~SSkeletonTree();

	void Construct(const FArguments& InArgs, const TSharedRef<FEditableSkeleton>& InEditableSkeleton, const FSkeletonTreeArgs& InSkeletonTreeArgs);

	/** ISkeletonTree interface */
	virtual void Refresh() override;
	virtual void RefreshFilter() override;
	virtual TSharedRef<class IEditableSkeleton> GetEditableSkeleton() const override { return EditableSkeleton.Pin().ToSharedRef(); }
	virtual TSharedPtr<class IPersonaPreviewScene> GetPreviewScene() const override { return PreviewScene.Pin(); }
	virtual void SetSkeletalMesh(USkeletalMesh* NewSkeletalMesh) override;
	virtual void SetSelectedSocket(const struct FSelectedSocketInfo& InSocketInfo) override;
	virtual void SetSelectedBone(const FName& InBoneName) override;
	virtual void DeselectAll() override;
	virtual TArray<TSharedPtr<ISkeletonTreeItem>> GetSelectedItems() const override { return SkeletonTreeView->GetSelectedItems(); }
	virtual void SelectItemsBy(TFunctionRef<bool(const TSharedRef<ISkeletonTreeItem>&, bool&)> Predicate) const override;
	virtual void DuplicateAndSelectSocket(const FSelectedSocketInfo& SocketInfoToDuplicate, const FName& NewParentBoneName = FName()) override;

	virtual void RegisterOnObjectSelected(const FOnObjectSelected& Delegate) override
	{
		OnObjectSelectedMulticast.Add(Delegate);
	}

	virtual void UnregisterOnObjectSelected(SWidget* Widget) override
	{
		OnObjectSelectedMulticast.RemoveAll(Widget);
	}

	virtual FDelegateHandle RegisterOnSelectionChanged(const FOnSkeletonTreeSelectionChanged& Delegate) override
	{
		return OnSelectionChangedMulticast.Add(Delegate);
	}

	virtual void UnregisterOnSelectionChanged(FDelegateHandle DelegateHandle) override
	{
		OnSelectionChangedMulticast.Remove(DelegateHandle);
	}

	virtual UBlendProfile* GetSelectedBlendProfile() override;
	virtual void AttachAssets(const TSharedRef<ISkeletonTreeItem>& TargetItem, const TArray<FAssetData>& AssetData) override;
	virtual TSharedPtr<SWidget> GetSearchWidget() const override { return NameFilterBox; }

	/** FEditorUndoClient interface */
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;

	/** FGCObject interface */
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;

	/** Creates the tree control and then populates */
	void CreateTreeColumns();

	/** Function to build the skeleton tree widgets from the source skeleton tree */
	void CreateFromSkeleton();

	/** Apply filtering to the tree */
	void ApplyFilter();

	/** Set the initial expansion state of the tree items */
	void SetInitialExpansionState();

	/** Utility function to print notifications to the user */
	void NotifyUser( FNotificationInfo& NotificationInfo );

	/** Callback when an item is scrolled into view, handling calls to rename items */
	void OnItemScrolledIntoView( TSharedPtr<ISkeletonTreeItem> InItem, const TSharedPtr<ITableRow>& InWidget);

	/** Callback for when the user double-clicks on an item in the tree */
	void OnTreeDoubleClick( TSharedPtr<ISkeletonTreeItem> InItem );

	/** Handle recursive expansion/contraction of the tree */
	void SetTreeItemExpansionRecursive(TSharedPtr< ISkeletonTreeItem > TreeItem, bool bInExpansionState) const;

	/** Set Bone Translation Retargeting Mode for bone selection, and their children. */
	void SetBoneTranslationRetargetingModeRecursive(EBoneTranslationRetargetingMode::Type NewRetargetingMode);

	/** Remove the selected bones from LOD of LODIndex when using simplygon **/
	void RemoveFromLOD(int32 LODIndex, bool bIncludeSelected, bool bIncludeBelowLODs);

	/** Called when the preview mesh is changed - simply rebuilds the skeleton tree for the new mesh */
	void OnLODSwitched();

	/** Get the name of the currently selected blend profile */
	FName GetSelectedBlendProfileName() const;

	/** Delegate handler for when the tree needs refreshing */
	void HandleTreeRefresh();

	/** Get a shared reference to the skeleton tree that owns us */
	TSharedRef<FEditableSkeleton> GetEditableSkeletonInternal() const { return EditableSkeleton.Pin().ToSharedRef(); }

	/** Update preview scene and tree after a socket rename */
	void PostRenameSocket(UObject* InAttachedObject, const FName& InOldName, const FName& InNewName);

	/** Update preview scene and tree after a socket duplication */
	void PostDuplicateSocket(UObject* InAttachedObject, const FName& InSocketName);

private:
	/** Binds the commands in FSkeletonTreeCommands to functions in this class */
	void BindCommands();

	/** Create a widget for an entry in the tree from an info */
	TSharedRef<ITableRow> MakeTreeRowWidget(TSharedPtr<ISkeletonTreeItem> InInfo, const TSharedRef<STableViewBase>& OwnerTable);

	/** Get all children for a given entry in the list */
	void GetFilteredChildren(TSharedPtr<ISkeletonTreeItem> InInfo, TArray< TSharedPtr<ISkeletonTreeItem> >& OutChildren);

	/** Called to display context menu when right clicking on the widget */
	TSharedPtr< SWidget > CreateContextMenu();

	/** Called to display the filter menu */
	TSharedRef< SWidget > CreateFilterMenu();

	/** Function to copy selected bone name to the clipboard */
	void OnCopyBoneNames();

	/** Function to reset the transforms of selected bones */
	void OnResetBoneTransforms();

	/** Function to copy selected sockets to the clipboard */
	void OnCopySockets() const;

	/** Function to serialize a single socket to a string */
	FString SerializeSocketToString( class USkeletalMeshSocket* Socket, ESocketParentType ParentType ) const;

	/** Function to paste selected sockets from the clipboard */
	void OnPasteSockets(bool bPasteToSelectedBone);

	/** Function to add a socket to the selected bone (skeleton, not mesh) */
	void OnAddSocket();

	/** Function to check if it is possible to rename the selected item */
	bool CanRenameSelected() const;

	/** Function to start renaming the selected item */
	void OnRenameSelected();

	/** Function to customize a socket - this essentially copies a socket from the skeleton to the mesh */
	void OnCustomizeSocket();

	/** Function to promote a socket - this essentially copies a socket from the mesh to the skeleton */
	void OnPromoteSocket();

	/** Create sub menu to allow users to pick a target bone for the new space switching bone(s) */
	void FillVirtualBoneSubmenu(FMenuBuilder& MenuBuilder, TArray<TSharedPtr<class ISkeletonTreeItem>> SourceBones);

	/** Handler for user picking a target bone */
	void OnVirtualTargetBonePicked(FName TargetBoneName, TArray<TSharedPtr<class ISkeletonTreeItem>> SourceBones);

	/** Create content picker sub menu to allow users to pick an asset to attach */
	void FillAttachAssetSubmenu(FMenuBuilder& MenuBuilder, const TSharedPtr<class ISkeletonTreeItem> TargetItem);

	/** Helper function for asset picker that handles users choice */
	void OnAssetSelectedFromPicker(const FAssetData& AssetData, const TSharedPtr<class ISkeletonTreeItem> TargetItem);

	/** Context menu function to remove all attached assets */
	void OnRemoveAllAssets();

	/** Context menu function to control enabled/disabled status of remove all assets menu item */
	bool CanRemoveAllAssets() const;

	/** Functions to copy sockets from the skeleton to the mesh */
	void OnCopySocketToMesh() {};

	/** Callback function to be called when selection changes in the tree view widget. */
	void OnSelectionChanged(TSharedPtr<class ISkeletonTreeItem> Selection, ESelectInfo::Type SelectInfo);

	/** Filters the SListView when the user changes the search text box (NameFilterBox)	*/
	void OnFilterTextChanged( const FText& SearchText );

	/** Sets which types of bone to show */
	void SetBoneFilter(EBoneFilter InBoneFilter );

	/** Queries the bone filter */
	bool IsBoneFilter(EBoneFilter InBoneFilter ) const;

	/** Sets which types of socket to show */
	void SetSocketFilter(ESocketFilter InSocketFilter );

	/** Queries the bone filter */
	bool IsSocketFilter(ESocketFilter InSocketFilter ) const;

	/** Returns the current text for the filter button - "All", "Mesh" or "Weighted" etc. */
	FText GetFilterMenuTitle() const;

	/** We can only add sockets in Active, Skeleton or All mode (otherwise they just disappear) */
	bool IsAddingSocketsAllowed() const;

	/** Handler for "Show Retargeting Options" check box IsChecked functionality */
	bool IsShowingAdvancedOptions() const;

	/**  Handler for when we change the "Show Retargeting Options" check box */
	void OnChangeShowingAdvancedOptions();

	/** This replicates the socket filter to the previewcomponent so that the viewport can use the same settings */
	void SetPreviewComponentSocketFilter() const;

	/** Override OnKeyDown */
	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent );

	/** Check whether we can delete all the selected sockets/assets */
	bool CanDeleteSelectedRows() const;

	/** Function to delete all the selected sockets/assets */
	void OnDeleteSelectedRows();

	/** Function to remove attached assets from the skeleton/mesh */
	void DeleteAttachedAssets(const TArray<TSharedPtr<class FSkeletonTreeAttachedAssetItem>>& InDisplayedAttachedAssetInfos );

	/** Function to remove sockets from the skeleton/mesh */
	void DeleteSockets(const TArray<TSharedPtr<class FSkeletonTreeSocketItem>>& InDisplayedSocketInfos );

	/** Function to remove virtual bones from the skeleton/mesh */
	void DeleteVirtualBones(const TArray<TSharedPtr<class FSkeletonTreeVirtualBoneItem>>& InDisplayedVirtualBonestInfos);

	/** Called when the user selects a blend profile to edit */
	void OnBlendProfileSelected(UBlendProfile* NewProfile);

	/** Sets the blend scale for the selected bones and all of their children */
	void RecursiveSetBlendProfileScales(float InScaleToSet);

	/** Submenu creator handler for the given skeleton */
	static void CreateMenuForBoneReduction(FMenuBuilder& MenuBuilder, SSkeletonTree* SkeletonTree, int32 LODIndex, bool bIncludeSelected);

	/** Vary the foreground color of the filter button based on hover state */
	FSlateColor GetFilterComboButtonForegroundColor() const;

	/** Handle focusing the camera on the current selection */
	void HandleFocusCamera();

	/** Handle filtering the tree  */
	ESkeletonTreeFilterResult HandleFilterSkeletonTreeItem(const FSkeletonTreeFilterArgs& InArgs, const TSharedPtr<class ISkeletonTreeItem>& InItem);

	// Called when bone tree queries reference skeleton
	const FReferenceSkeleton& OnGetReferenceSkeleton() const
	{
		return GetEditableSkeletonInternal()->GetSkeleton().GetReferenceSkeleton();
	}
private:
	/** Pointer back to the skeleton tree that owns us */
	TWeakPtr<FEditableSkeleton> EditableSkeleton;

	/** Link to a preview scene */
	TWeakPtr<IPersonaPreviewScene> PreviewScene;

	/** SSearchBox to filter the tree */
	TSharedPtr<SSearchBox>	NameFilterBox;

	/** The blend profile picker displaying the selected profile */
	TSharedPtr<SBlendProfilePicker> BlendProfilePicker;

	/** Widget user to hold the skeleton tree */
	TSharedPtr<SOverlay> TreeHolder;

	/** Widget used to display the skeleton hierarchy */
	TSharedPtr<STreeView<TSharedPtr<class ISkeletonTreeItem>>> SkeletonTreeView;

	/** A tree of unfiltered items */
	TArray<TSharedPtr<class ISkeletonTreeItem>> Items;

	/** A "mirror" of the tree as a flat array for easier searching */
	TArray<TSharedPtr<class ISkeletonTreeItem>> LinearItems;

	/** Filtered view of the skeleton tree. This is what is actually used in the tree widget */
	TArray<TSharedPtr<class ISkeletonTreeItem>> FilteredItems;

	/** Is this view editable */
	TAttribute<bool> IsEditable;

	/** Current text typed into NameFilterBox */
	FText FilterText;

	/** Commands that are bound to delegates*/
	TSharedPtr<FUICommandList> UICommandList;

	/** Current type of bones to show */
	EBoneFilter BoneFilter;

	/** Current type of sockets to show */
	ESocketFilter SocketFilter;

	bool bShowingAdvancedOptions;

	/** Points to an item that is being requested to be renamed */
	TSharedPtr<ISkeletonTreeItem> DeferredRenameRequest;

	/** Last Cached Preview Mesh Component LOD */
	int32 LastCachedLODForPreviewMeshComponent;

	/** Delegate for when an item is selected */
	FOnSkeletonTreeSelectionChangedMulticast OnSelectionChangedMulticast;

	DEPRECATED(4.17, "Please use OnSelectionChangedMulticast")
	FOnObjectSelectedMulticast OnObjectSelectedMulticast;

	/** Selection recursion guard flags */
	bool bSelectingSocket;
	bool bSelectingBone;
	bool bDeselectingAll;

	/** Hold onto the filter combo button to set its foreground color */
	TSharedPtr<SComboButton> FilterComboButton;

	/** Add virtual bones to the skeleton tree */
	void AddVirtualBones(const TArray<FVirtualBone>& VirtualBones);

	/** The builder we use to construct the tree */
	TSharedPtr<class ISkeletonTreeBuilder> Builder;

	/** Compiled filter search terms. */
	TSharedPtr<class FTextFilterExpressionEvaluator> TextFilterPtr;

	/** Proxy object used to display and edit bone transforms in details panels. Note this is only kept for backwards compatibility (used with OnObjectSelectedMulticast) */
	class UBoneProxy* BoneProxy;

	/** Whether to allow operations that modify the mesh */
	bool bAllowMeshOperations;

	/** Whether to allow operations that modify the mesh */
	bool bAllowSkeletonOperations;

	/** Extenders for menus */
	TSharedPtr<FExtender> Extenders;

	/** Delegate that allows custom filtering text to be shown on the filter button */
	FOnGetFilterText OnGetFilterText;

	/** The mode that this skeleton tree is in */
	ESkeletonTreeMode Mode;

	friend struct FScopedSavedSelection;
}; 
PRAGMA_ENABLE_DEPRECATION_WARNINGS