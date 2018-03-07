// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SceneOutlinerFwd.h"
#include "Layout/Visibility.h"
#include "Input/DragAndDrop.h"
#include "DragAndDrop/DecoratedDragDropOp.h"
#include "DragAndDrop/ActorDragDropGraphEdOp.h"

namespace SceneOutliner
{
	/** Consilidated drag/drop information parsed for the scene outliner */
	struct FDragDropPayload
	{
		/** Default constructor, resulting in unset contents */
		FDragDropPayload();

		/** Populate this payload from an array of tree items */
		template<typename TItem>
		FDragDropPayload(const TArray<TItem>& InDraggedItems)
		{
			for (const auto& Item : InDraggedItems)
			{
				Item->PopulateDragDropPayload(*this);
			}
		}

		/** Optional array of dragged folders */
		TOptional<FFolderPaths> Folders;

		/** OPtional array of dragged actors */
		TOptional<FActorArray> Actors;

		/**
		 *	Parse a drag operation into our list of actors and folders
		 *	@return true if the operation is viable for the sceneoutliner to process, false otherwise
		 */
		bool ParseDrag(const FDragDropOperation& Operation);
	};
	
	/** Construct a new Drag and drop operation for a scene outliner */
	TSharedPtr<FDragDropOperation> CreateDragDropOperation(const TArray<FTreeItemPtr>& InTreeItems);

	/** Struct used for validation of a drag/drop operation in the scene outliner */
	struct FDragValidationInfo
	{
		/** The tooltip type to display on the operation */
		FActorDragDropGraphEdOp::ToolTipTextType TooltipType;

		/** The tooltip text to display on the operation */
		FText ValidationText;

		/** Construct this validation information out of a tootip type and some text */
		FDragValidationInfo(const FActorDragDropGraphEdOp::ToolTipTextType InTooltipType, const FText InValidationText)
			: TooltipType(InTooltipType)
			, ValidationText(InValidationText)
		{}

		/** Return a generic invalid result */
		static FDragValidationInfo Invalid()
		{
			return FDragValidationInfo(FActorDragDropGraphEdOp::ToolTip_IncompatibleGeneric, FText());
		}
		
		/** @return true if this operation is valid, false otheriwse */ 
		bool IsValid() const
		{
			switch(TooltipType)
			{
			case FActorDragDropGraphEdOp::ToolTip_Compatible:
			case FActorDragDropGraphEdOp::ToolTip_CompatibleAttach:
			case FActorDragDropGraphEdOp::ToolTip_CompatibleGeneric:
			case FActorDragDropGraphEdOp::ToolTip_CompatibleMultipleAttach:
			case FActorDragDropGraphEdOp::ToolTip_CompatibleDetach:
			case FActorDragDropGraphEdOp::ToolTip_CompatibleMultipleDetach:
				return true;
			default:
				return false;
			}
		}
	};

	/* A drag/drop operation when dragging folders in the scene outliner */
	struct FFolderDragDropOp: public FDecoratedDragDropOp
	{
		DRAG_DROP_OPERATOR_TYPE(FFolderDragDropOp, FDecoratedDragDropOp)

		/** Array of folders that we are dragging */
		FFolderPaths Folders;

		void Init(FFolderPaths InFolders);
	};

	/** A drag/drop operation that was started from the scene outliner */
	struct FSceneOutlinerDragDropOp : public FDragDropOperation
	{
		DRAG_DROP_OPERATOR_TYPE(FSceneOutlinerDragDropOp, FDragDropOperation);

		FSceneOutlinerDragDropOp(const FDragDropPayload& DraggedObjects);

		using FDragDropOperation::Construct;

		/** Actor drag operation */
		TSharedPtr<FActorDragDropOp>	ActorOp;

		/** Actor drag operation */
		TSharedPtr<FFolderDragDropOp>	FolderOp;

		void ResetTooltip()
		{
			OverrideText = FText();
			OverrideIcon = nullptr;
		}

		void SetTooltip(FText InOverrideText, const FSlateBrush* InOverrideIcon)
		{
			OverrideText = InOverrideText;
			OverrideIcon = InOverrideIcon;
		}

		virtual TSharedPtr<SWidget> GetDefaultDecorator() const override;

	private:

		EVisibility GetOverrideVisibility() const;
		EVisibility GetDefaultVisibility() const;

		FText OverrideText;
		FText GetOverrideText() const { return OverrideText; }

		const FSlateBrush* OverrideIcon;
		const FSlateBrush* GetOverrideIcon() const { return OverrideIcon; }
	};

}
