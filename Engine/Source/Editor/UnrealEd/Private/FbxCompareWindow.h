// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWindow.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STreeView.h"
#include "Widgets/SNullWidget.h"
#include "EditorStyleSet.h"

class FFbxSceneInfo;

enum EFBXCompareSection
{
	EFBXCompareSection_General = 0,
	EFBXCompareSection_Materials,
	EFBXCompareSection_Skeleton,
	EFBXCompareSection_Count
};

class FCompMaterial
{
public:
	FCompMaterial(FName InMaterialSlotName, FName InImportedMaterialSlotName)
		: MaterialSlotName(InMaterialSlotName)
		, ImportedMaterialSlotName(InImportedMaterialSlotName)
	{}
	FName MaterialSlotName;
	FName ImportedMaterialSlotName;
};

class FCompSection
{
public:
	FCompSection()
		: MaterialIndex(INDEX_NONE)
	{}

	int32 MaterialIndex;
};

class FCompLOD
{
public:
	~FCompLOD()
	{
		Sections.Empty();
	}

	TArray<FCompSection> Sections;
};

class FCompJoint
{
public:
	FCompJoint()
		: Name(NAME_None)
		, ParentIndex(INDEX_NONE)
	{ }

	~FCompJoint()
	{
		ChildIndexes.Empty();
	}

	FName Name;
	int32 ParentIndex;
	TArray<int32> ChildIndexes;
};

class FCompSkeleton
{
public:
	FCompSkeleton()
		: bSkeletonFitMesh(true)
	{}

	~FCompSkeleton()
	{
		Joints.Empty();
	}

	TArray<FCompJoint> Joints;
	bool bSkeletonFitMesh;
};

class FCompMesh
{
public:
	~FCompMesh()
	{
		CompMaterials.Empty();
		CompLods.Empty();
	}

	TArray<FCompMaterial> CompMaterials;
	TArray<FCompLOD> CompLods;
	FCompSkeleton CompSkeleton;
	TArray<FString> UVSetsName;
	int32 LightMapUvIndex;

	TArray<FString> ErrorMessages;
	TArray<FString> WarningMessages;
};

class FGeneralFbxFileInfo
{
public:
	FString ApplicationCreator;
	FString UE4SdkVersion;
	FString FileVersion;
	FString AxisSystem;
	FString UnitSystem;
	FString CreationDate;
};

class FSkeletonCompareData : public TSharedFromThis<FSkeletonCompareData>
{
public:
	FSkeletonCompareData()
		: CurrentJointIndex(INDEX_NONE)
		, FbxJointIndex(INDEX_NONE)
		, JointName(NAME_None)
		, ParentJoint(nullptr)
		, bMatchJoint(false)
		, bChildConflict(false)
	{}
	int32 CurrentJointIndex;
	int32 FbxJointIndex;
	FName JointName;
	TSharedPtr<FSkeletonCompareData> ParentJoint;
	bool bMatchJoint;
	bool bChildConflict;
	TArray<int32> ChildJointIndexes;
	TArray<TSharedPtr<FSkeletonCompareData>> ChildJoints;
};

class FCompareRowData : public TSharedFromThis<FCompareRowData>
{
public:
	FCompareRowData()
		: RowIndex(INDEX_NONE)
		, CurrentData(nullptr)
		, FbxData(nullptr)
	{}
	virtual ~FCompareRowData() {}

	int32 RowIndex;
	FCompMesh *CurrentData;
	FCompMesh *FbxData;

	virtual TSharedRef<SWidget> ConstructCellCurrent()
	{
		return SNullWidget::NullWidget;
	};
	virtual TSharedRef<SWidget> ConstructCellFbx()
	{
		return SNullWidget::NullWidget;
	};
};

class FMaterialCompareData : public FCompareRowData
{
public:
	enum EMaterialCompareDisplayOption
	{
		NoMatch = 0,
		IndexChanged,
		SkinxxError,
		All,
		MaxOptionEnum
	};

	FMaterialCompareData()
		: CurrentMaterialIndex(INDEX_NONE)
		, FbxMaterialIndex(INDEX_NONE)
		, CurrentMaterialMatch(INDEX_NONE)
		, FbxMaterialMatch(INDEX_NONE)
		, bCurrentSkinxxDuplicate(false)
		, bCurrentSkinxxMissing(false)
		, bFbxSkinxxDuplicate(false)
		, bFbxSkinxxMissing(false)
		, CompareDisplayOption(EMaterialCompareDisplayOption::All)
	{}
	
	virtual ~FMaterialCompareData() {}

	int32 CurrentMaterialIndex;
	int32 FbxMaterialIndex;
	int32 CurrentMaterialMatch;
	int32 FbxMaterialMatch;
	bool bCurrentSkinxxDuplicate;
	bool bCurrentSkinxxMissing;
	bool bFbxSkinxxDuplicate;
	bool bFbxSkinxxMissing;
	EMaterialCompareDisplayOption CompareDisplayOption;
	
	FSlateColor GetCellColor(FCompMesh *DataA, int32 MaterialIndexA, int32 MaterialMatchA, FCompMesh *DataB, int32 MaterialIndexB, bool bSkinxxError) const;
	FSlateColor GetCurrentCellColor() const;
	FSlateColor GetFbxCellColor() const;

	TSharedRef<SWidget> ConstructCell(FCompMesh *MeshData, int32 MeshMaterialIndex, bool bSkinxxDuplicate, bool bSkinxxMissing);
	virtual TSharedRef<SWidget> ConstructCellCurrent() override;
	virtual TSharedRef<SWidget> ConstructCellFbx() override;
};

class SCompareRowDataTableListViewRow : public SMultiColumnTableRow<TSharedPtr<FCompareRowData>>
{
public:

	SLATE_BEGIN_ARGS(SCompareRowDataTableListViewRow)
		: _CompareRowData(nullptr)
	{}

	/** The item content. */
	SLATE_ARGUMENT(TSharedPtr<FCompareRowData>, CompareRowData)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
	{
		CompareRowData = InArgs._CompareRowData;

		//This is suppose to always be valid
		check(CompareRowData.IsValid());

		SMultiColumnTableRow<TSharedPtr<FCompareRowData>>::Construct(
			FSuperRowType::FArguments()
			.Style(FEditorStyle::Get(), "DataTableEditor.CellListViewRow"),
			InOwnerTableView
			);
	}

	/** Overridden from SMultiColumnTableRow.  Generates a widget for this column of the list view. */
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
	{
		if (ColumnName == FName(TEXT("RowIndex")))
		{
			return SNew(SBox)
				.Padding(FMargin(5.0f, 0.0f, 0.0f, 0.0f))
				[
					SNew(STextBlock)
					.Text(FText::FromString(FString::FromInt(CompareRowData->RowIndex)))
				];
		}
		else if (ColumnName == FName(TEXT("Current")))
		{
			return CompareRowData->ConstructCellCurrent();
		}
		else if (ColumnName == FName(TEXT("Fbx")))
		{
			return CompareRowData->ConstructCellFbx();
		}
		return SNullWidget::NullWidget;
	}
private:

	/** The node info to build the tree view row from. */
	TSharedPtr<FCompareRowData> CompareRowData;
};

class SFbxCompareWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SFbxCompareWindow)
		: _WidgetWindow()
		, _FullFbxPath()
		, _FbxSceneInfo()
		, _FbxGeneralInfo()
		, _AssetReferencingSkeleton(nullptr)
		, _CurrentMeshData(nullptr)
		, _FbxMeshData(nullptr)
		, _PreviewObject(nullptr)
		{}

		SLATE_ARGUMENT( TSharedPtr<SWindow>, WidgetWindow )
		SLATE_ARGUMENT( FText, FullFbxPath )
		SLATE_ARGUMENT( TSharedPtr<FFbxSceneInfo>, FbxSceneInfo )
		SLATE_ARGUMENT( FGeneralFbxFileInfo, FbxGeneralInfo )
		SLATE_ARGUMENT( TArray<TSharedPtr<FString>>*, AssetReferencingSkeleton)
		SLATE_ARGUMENT( FCompMesh*, CurrentMeshData )
		SLATE_ARGUMENT( FCompMesh*, FbxMeshData )
		SLATE_ARGUMENT( UObject*, PreviewObject )
			
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs);
	virtual bool SupportsKeyboardFocus() const override { return true; }

	FReply OnDone()
	{
		if ( WidgetWindow.IsValid() )
		{
			WidgetWindow.Pin()->RequestDestroyWindow();
		}
		return FReply::Handled();
	}

	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override
	{
		if( InKeyEvent.GetKey() == EKeys::Escape )
		{
			return OnDone();
		}

		return FReply::Unhandled();
	}

	SFbxCompareWindow()
		: FullFbxPath(TEXT(""))
		, FbxSceneInfo()
		
	{}
		
private:
	TWeakPtr< SWindow > WidgetWindow;
	FString FullFbxPath;

	//Preview Mesh
	UObject *PreviewObject;

	//////////////////////////////////////////////////////////////////////////
	//Collapse generic
	bool bShowSectionFlag[EFBXCompareSection_Count];
	FReply SetSectionVisible(EFBXCompareSection SectionIndex);
	EVisibility IsSectionVisible(EFBXCompareSection SectionIndex);
	const FSlateBrush* GetCollapsableArrow(EFBXCompareSection SectionIndex) const;
	//////////////////////////////////////////////////////////////////////////
	
	//////////////////////////////////////////////////////////////////////////
	// General fbx data
	FGeneralFbxFileInfo FbxGeneralInfo;
	TSharedPtr<FFbxSceneInfo> FbxSceneInfo;
	TArray<TSharedPtr<FString>> GeneralListItem;
	
	void FillGeneralListItem();
	//Construct slate
	TSharedPtr<SWidget> ConstructGeneralInfo();
	//Slate events
	TSharedRef<ITableRow> OnGenerateRowGeneralFbxInfo(TSharedPtr<FString> InItem, const TSharedRef<STableViewBase>& OwnerTable);
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Compare data
	FCompMesh *CurrentMeshData;
	FCompMesh *FbxMeshData;
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Material Data
	TArray<TSharedPtr<FMaterialCompareData>> CompareMaterialListItem;
	FMaterialCompareData::EMaterialCompareDisplayOption CurrentDisplayOption;
	
	void FillMaterialListItem();
	bool FindSkinxxErrors(FCompMesh *MeshData, TArray<bool> &DuplicateSkinxxMaterialNames, TArray<bool> &MissingSkinxxSuffixeMaterialNames);
	//Construct slate
	TSharedPtr<SWidget> ConstructMaterialComparison();
	//Slate events
	TSharedRef<ITableRow> OnGenerateRowForCompareMaterialList(TSharedPtr<FMaterialCompareData> RowData, const TSharedRef<STableViewBase>& Table);
	void ToggleMaterialDisplay(ECheckBoxState InNewValue, FMaterialCompareData::EMaterialCompareDisplayOption InDisplayOption);
	ECheckBoxState IsToggleMaterialDisplayChecked(FMaterialCompareData::EMaterialCompareDisplayOption InDisplayOption) const;
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Skeleton Data
	TSharedPtr<STreeView<TSharedPtr<FSkeletonCompareData>>> CompareTree;
	TArray<TSharedPtr<FSkeletonCompareData>> DisplaySkeletonTreeItem;

	TArray<TSharedPtr<FSkeletonCompareData>> CurrentSkeletonTreeItem;
	TArray<TSharedPtr<FSkeletonCompareData>> FbxSkeletonTreeItem;

	TArray<TSharedPtr<FString>> AssetReferencingSkeleton;

	void FilSkeletonTreeItem();
	void RecursiveMatchJointInfo(TSharedPtr<FSkeletonCompareData> CurrentItem);
	void SetMatchJointInfo();
	//Construct slate
	TSharedPtr<SWidget> ConstructSkeletonComparison();
	//Slate events
	TSharedRef<ITableRow> OnGenerateRowCompareTreeView(TSharedPtr<FSkeletonCompareData> RowData, const TSharedRef<STableViewBase>& Table);
	void OnGetChildrenRowCompareTreeView(TSharedPtr<FSkeletonCompareData> InParent, TArray< TSharedPtr<FSkeletonCompareData> >& OutChildren);
	TSharedRef<ITableRow> OnGenerateRowAssetReferencingSkeleton(TSharedPtr<FString> InItem, const TSharedRef<STableViewBase>& OwnerTable);
	//////////////////////////////////////////////////////////////////////////
};
