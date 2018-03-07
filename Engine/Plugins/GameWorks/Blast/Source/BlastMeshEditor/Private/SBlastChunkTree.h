// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "STreeView.h"

#include "IBlastMeshEditor.h"

class IBlastMeshEditor;

class SBlastChunkTree : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBlastChunkTree)
	{}
	
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TWeakPtr <IBlastMeshEditor> InBlastMeshEditor);

	void Refresh();
	void UpdateSelection();
	void UpdateExpansion();
	TArray<FBlastChunkEditorModelPtr>& GetRootChunks()
	{
		return RootChunks;
	}

private:

	void OnDepthFilterChanged(int32 NewDepth);
	
	FReply ApplySelectionFilter();
	FReply ClearSelectionFilter();

	TSharedRef<ITableRow> OnGenerateRowForTree(FBlastChunkEditorModelPtr Item, const TSharedRef<STableViewBase>& OwnerTable);
	void OnGetChildrenForTree(FBlastChunkEditorModelPtr Parent, TArray<FBlastChunkEditorModelPtr>& OutChildren);
	void OnTreeSelectionChanged(FBlastChunkEditorModelPtr TreeElem, ESelectInfo::Type SelectInfo);
	TSharedPtr<SWidget> ConstructContextMenu() const;

	/**
	Context menu callbacks
	*/
	void Visibility(bool isShow);
	void SetStatic(bool isStatic);
	void RemoveChildren();
private:
	
	TSharedPtr<class SBlastDepthFilter>					DepthFilter;
	TSharedPtr<class SCheckBox>							SupportChunkFilter;
	TSharedPtr<class SCheckBox>							StaticChunkFilter;

	TSharedPtr< STreeView<FBlastChunkEditorModelPtr> >	ChunkHierarchy;
	
	TArray<FBlastChunkEditorModelPtr>					RootChunks;

	bool InsideSelectionChanged;

	TWeakPtr <IBlastMeshEditor>							BlastMeshEditorPtr;
};
