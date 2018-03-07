// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SDetailSingleItemRow.h"
#include "ObjectPropertyNode.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Settings/EditorExperimentalSettings.h"
#include "DetailWidgetRow.h"
#include "IDetailKeyframeHandler.h"
#include "IDetailPropertyExtensionHandler.h"
#include "DetailPropertyRow.h"
#include "DetailGroup.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Editor.h"


void SConstrainedBox::Construct(const FArguments& InArgs)
{
	MinWidth = InArgs._MinWidth;
	MaxWidth = InArgs._MaxWidth;

	ChildSlot
	[
		InArgs._Content.Widget
	];
}

FVector2D SConstrainedBox::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	const float MinWidthVal = MinWidth.Get().Get(0.0f);
	const float MaxWidthVal = MaxWidth.Get().Get(0.0f);

	if (MinWidthVal == 0.0f && MaxWidthVal == 0.0f)
	{
		return SCompoundWidget::ComputeDesiredSize(LayoutScaleMultiplier);
	}
	else
	{
		FVector2D ChildSize = ChildSlot.GetWidget()->GetDesiredSize();

		float XVal = FMath::Max(MinWidthVal, ChildSize.X);
		if (MaxWidthVal >= MinWidthVal)
		{
			XVal = FMath::Min(MaxWidthVal, XVal);
		}

		return FVector2D(XVal, ChildSize.Y);
	}
}

namespace DetailWidgetConstants
{
	const FMargin LeftRowPadding( 0.0f, 2.5f, 2.0f, 2.5f );
	const FMargin RightRowPadding( 3.0f, 2.5f, 2.0f, 2.5f );
}

namespace SDetailSingleItemRow_Helper
{
	//Get the node item number in case it is expand we have to recursively count all expanded children
	void RecursivelyGetItemShow(TSharedRef<FDetailTreeNode> ParentItem, int32 &ItemShowNum)
	{
		if (ParentItem->GetVisibility() == ENodeVisibility::Visible)
		{
			ItemShowNum++;
		}

		if (ParentItem->ShouldBeExpanded())
		{
			TArray< TSharedRef<FDetailTreeNode> > Childrens;
			ParentItem->GetChildren(Childrens);
			for (TSharedRef<FDetailTreeNode> ItemChild : Childrens)
			{
				RecursivelyGetItemShow(ItemChild, ItemShowNum);
			}
		}
	}
}

FReply SDetailSingleItemRow::OnFavoriteToggle()
{
	if (Customization->GetPropertyNode().IsValid() && Customization->GetPropertyNode()->CanDisplayFavorite())
	{
		bool toggle = !Customization->GetPropertyNode()->IsFavorite();
		Customization->GetPropertyNode()->SetFavorite(toggle);
		if (OwnerTreeNode.IsValid())
		{
			//////////////////////////////////////////////////////////////////////////
			// Calculate properly the scrolling offset (by item) to make sure the mouse stay over the same property

			//Get the node item number in case it is expand we have to recursively count all childrens
			int32 ExpandSize = 0;
			if (OwnerTreeNode.Pin()->ShouldBeExpanded())
			{
				SDetailSingleItemRow_Helper::RecursivelyGetItemShow(OwnerTreeNode.Pin().ToSharedRef(), ExpandSize);
			}
			else
			{
				//if the item is not expand count is 1
				ExpandSize = 1;
			}
			
			//Get the number of favorite child (simple and advance) to know if the favorite category will be create or remove
			FString CategoryFavoritesName = TEXT("Favorites");
			FName CatFavName = *CategoryFavoritesName;
			int32 SimplePropertiesNum = 0;
			int32 AdvancePropertiesNum = 0;

			FDetailLayoutBuilderImpl& DetailLayout = OwnerTreeNode.Pin()->GetParentCategory()->GetParentLayoutImpl();

			bool HasCategoryFavorite = DetailLayout.HasCategory(CatFavName);
			if(HasCategoryFavorite)
			{
				DetailLayout.DefaultCategory(CatFavName).GetCategoryInformation(SimplePropertiesNum, AdvancePropertiesNum);
			}

			//Check if the property we toggle is an advance property
			bool IsAdvanceProperty = Customization->GetPropertyNode()->HasNodeFlags(EPropertyNodeFlags::IsAdvanced) == 0 ? false : true;

			//Compute the scrolling offset by item
			int32 ScrollingOffsetAdd = ExpandSize;
			int32 ScrollingOffsetRemove = -ExpandSize;
			if (HasCategoryFavorite)
			{
				//Adding the advance button in a category add 1 item
				ScrollingOffsetAdd += (IsAdvanceProperty && AdvancePropertiesNum == 0) ? 1 : 0;

				if (IsAdvanceProperty && AdvancePropertiesNum == 1)
				{
					//Removing the advance button count as 1 item
					ScrollingOffsetRemove -= 1;
				}
				if (AdvancePropertiesNum + SimplePropertiesNum == 1)
				{
					//Removing a full category count as 2 items
					ScrollingOffsetRemove -= 2;
				}
			}
			else
			{
				//Adding new category (2 items) adding advance button (1 item)
				ScrollingOffsetAdd += IsAdvanceProperty ? 3 : 2;
				
				//We should never remove an item from favorite if there is no favorite category
				//Set the remove offset to 0
				ScrollingOffsetRemove = 0;
			}

			//Apply the calculated offset
			OwnerTreeNode.Pin()->GetDetailsView()->MoveScrollOffset(toggle ? ScrollingOffsetAdd : ScrollingOffsetRemove);

			//Refresh the tree
			OwnerTreeNode.Pin()->GetDetailsView()->ForceRefresh();
		}
	}
	return FReply::Handled();
}

void SDetailSingleItemRow::OnArrayDragEnter(const FDragDropEvent& DragDropEvent)
{
	bIsHoveredDragTarget = true;
}

void SDetailSingleItemRow::OnArrayDragLeave(const FDragDropEvent& DragDropEvent)
{
	bIsHoveredDragTarget = false;
}

FReply SDetailSingleItemRow::OnArrayDrop(const FDragDropEvent& DragDropEvent)
{
	bIsHoveredDragTarget = false;
	TSharedPtr<FArrayRowDragDropOp> ArrayDropOp = DragDropEvent.GetOperationAs< FArrayRowDragDropOp >();
	TSharedPtr<SDetailSingleItemRow> RowPtr = nullptr;
	if (ArrayDropOp.IsValid() && ArrayDropOp->Row.IsValid())
	{
		RowPtr = ArrayDropOp->Row.Pin();
	}
	if (!RowPtr.IsValid())
	{
		return FReply::Unhandled();
	}
	TSharedPtr<FPropertyNode> SwappingPropertyNode = RowPtr->SwappablePropertyNode;
	if (SwappingPropertyNode.IsValid() && SwappablePropertyNode.IsValid())
	{
		if (SwappingPropertyNode != SwappablePropertyNode)
		{
			int32 OriginalIndex = SwappingPropertyNode->GetArrayIndex();
			int32 NewIndex = SwappablePropertyNode->GetArrayIndex();
			if (NewIndex > OriginalIndex)
			{
				NewIndex += 1;
			}
			TSharedPtr<IPropertyHandle> SwappingHandle = PropertyEditorHelpers::GetPropertyHandle(SwappingPropertyNode.ToSharedRef(), OwnerTreeNode.Pin()->GetDetailsView()->GetNotifyHook(), OwnerTreeNode.Pin()->GetDetailsView()->GetPropertyUtilities());
			TSharedPtr<IPropertyHandleArray> ParentHandle = SwappingHandle->GetParentHandle()->AsArray();
			if (ParentHandle.IsValid() && SwappablePropertyNode->GetParentNode() == SwappingPropertyNode->GetParentNode())
			{
				// Need to swap the moving and target expansion states before saving
				bool bOriginalSwappableExpansion = SwappablePropertyNode->HasNodeFlags(EPropertyNodeFlags::Expanded) != 0;
				bool bOriginalSwappingExpansion = SwappingPropertyNode->HasNodeFlags(EPropertyNodeFlags::Expanded) != 0;
				SwappablePropertyNode->SetNodeFlags(EPropertyNodeFlags::Expanded, bOriginalSwappingExpansion);
				SwappingPropertyNode->SetNodeFlags(EPropertyNodeFlags::Expanded, bOriginalSwappableExpansion);
				OwnerTreeNode.Pin()->GetDetailsView()->SaveExpandedItems(SwappablePropertyNode->GetParentNodeSharedPtr().ToSharedRef());
				ParentHandle->MoveElementTo(OriginalIndex, NewIndex);
			}
		}
	}
	return FReply::Handled();
}

const FSlateBrush* SDetailSingleItemRow::GetFavoriteButtonBrush() const
{
	if (Customization->GetPropertyNode().IsValid() && Customization->GetPropertyNode()->CanDisplayFavorite())
	{
		return FEditorStyle::GetBrush(Customization->GetPropertyNode()->IsFavorite() ? TEXT("DetailsView.PropertyIsFavorite") : IsHovered() ? TEXT("DetailsView.PropertyIsNotFavorite") : TEXT("DetailsView.NoFavoritesSystem"));
	}
	//Adding a transparent brush make sure all property are left align correctly
	return FEditorStyle::GetBrush(TEXT("DetailsView.NoFavoritesSystem"));
}

void SDetailSingleItemRow::Construct( const FArguments& InArgs, FDetailLayoutCustomization* InCustomization, bool bHasMultipleColumns, TSharedRef<FDetailTreeNode> InOwnerTreeNode, const TSharedRef<STableViewBase>& InOwnerTableView )
{
	OwnerTreeNode = InOwnerTreeNode;
	bAllowFavoriteSystem = InArgs._AllowFavoriteSystem;

	ColumnSizeData = InArgs._ColumnSizeData;

	TSharedRef<SWidget> Widget = SNullWidget::NullWidget;
	Customization = InCustomization;

	EHorizontalAlignment HorizontalAlignment = HAlign_Fill;
	EVerticalAlignment VerticalAlignment = VAlign_Fill;

	TAttribute<bool> NameWidgetEnabled;

	FOnTableRowDragEnter ArrayDragDelegate;
	FOnTableRowDragLeave ArrayDragLeaveDelegate;
	FOnTableRowDrop ArrayDropDelegate;

	const bool bIsValidTreeNode = InOwnerTreeNode->GetParentCategory().IsValid() && InOwnerTreeNode->GetParentCategory()->IsParentLayoutValid();
	if(bIsValidTreeNode)
	{
		if(InCustomization->IsValidCustomization())
		{
			FDetailWidgetRow Row = InCustomization->GetWidgetRow();

			TSharedPtr<SWidget> NameWidget;
			TSharedPtr<SWidget> ValueWidget;

			NameWidget = Row.NameWidget.Widget;
			if(Row.IsEnabledAttr.IsBound())
			{
				NameWidgetEnabled = Row.IsEnabledAttr;
				NameWidget->SetEnabled(Row.IsEnabledAttr);
			}

			ValueWidget =
				SNew(SConstrainedBox)
				.MinWidth(Row.ValueWidget.MinWidth)
				.MaxWidth(Row.ValueWidget.MaxWidth)
				[
					Row.ValueWidget.Widget
				];

			ValueWidget = CreateExtensionWidget(ValueWidget.ToSharedRef(), *Customization, InOwnerTreeNode);

			if(Row.IsEnabledAttr.IsBound())
			{
				ValueWidget->SetEnabled(Row.IsEnabledAttr);
			}

			TSharedRef<SWidget> KeyFrameButton = CreateKeyframeButton(*Customization, InOwnerTreeNode);
			TAttribute<bool> IsPropertyEditingEnabled = InOwnerTreeNode->IsPropertyEditingEnabled();

			bool const bEnableFavoriteSystem = GIsRequestingExit ? false : (GetDefault<UEditorExperimentalSettings>()->bEnableFavoriteSystem && bAllowFavoriteSystem);

			TSharedRef<SHorizontalBox> InternalLeftColumnRowBox =
				SNew(SHorizontalBox)
				.Clipping(EWidgetClipping::OnDemand);

			if(bEnableFavoriteSystem)
			{
				InternalLeftColumnRowBox->AddSlot()
					.Padding(0.0f, 0.0f)
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.IsFocusable(false)
						.ButtonStyle(FEditorStyle::Get(), "NoBorder")
						.OnClicked(this, &SDetailSingleItemRow::OnFavoriteToggle)
						[
							SNew(SImage).Image(this, &SDetailSingleItemRow::GetFavoriteButtonBrush)
						]
					];
			}
			TSharedPtr<SOverlay> LeftSideOverlay;
			LeftSideOverlay = SNew(SOverlay);
			LeftSideOverlay->AddSlot()
				.Padding(3.0f, 0.0f)
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					SNew(SExpanderArrow, SharedThis(this))
					.BaseIndentLevel(1)
				];

			TSharedPtr<FPropertyNode> PropertyNode = Customization->GetPropertyNode();
			if (PropertyNode.IsValid() && PropertyNode->IsReorderable())
			{
				TSharedPtr<SDetailSingleItemRow> InRow = SharedThis(this);
				TSharedRef<SWidget> Handle = PropertyEditorHelpers::MakePropertyReorderHandle(PropertyNode.ToSharedRef(), InRow);
				Handle->SetEnabled(IsPropertyEditingEnabled);
				LeftSideOverlay->AddSlot()
					.Padding(0.0f, 0.0f, 10.0f, 0.0f)
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					[
						Handle
					];
				ArrayDragDelegate = FOnTableRowDragEnter::CreateSP(this, &SDetailSingleItemRow::OnArrayDragEnter);
				ArrayDragLeaveDelegate = FOnTableRowDragLeave::CreateSP(this, &SDetailSingleItemRow::OnArrayDragLeave);
				ArrayDropDelegate = FOnTableRowDrop::CreateSP(this, &SDetailSingleItemRow::OnArrayDrop);
				SwappablePropertyNode = PropertyNode;
			}

			InternalLeftColumnRowBox->AddSlot()
				.Padding(0.0f, 0.0f)
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					LeftSideOverlay.ToSharedRef()
				];

			if(bHasMultipleColumns)
			{
				// If the NameWidget has already been disabled, don't re-enable it if IsPropertyEditingEnabled is true.
				NameWidget->SetEnabled(NameWidgetEnabled.IsBound() ?
					TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda(
						[NameWidgetEnabled, IsPropertyEditingEnabled]()
						{
							return NameWidgetEnabled.Get() && IsPropertyEditingEnabled.Get();
						}))
					: IsPropertyEditingEnabled);

				TSharedPtr<SHorizontalBox> HBox;

				InternalLeftColumnRowBox->AddSlot()
					.HAlign(Row.NameWidget.HorizontalAlignment)
					.VAlign(Row.NameWidget.VerticalAlignment)
					.Padding(DetailWidgetConstants::LeftRowPadding)
					[
						NameWidget.ToSharedRef()
					];
				InternalLeftColumnRowBox->AddSlot()
					.Padding(3.0f, 0.0f)
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						KeyFrameButton
					];

				TSharedRef<SSplitter> Splitter =
					SNew(SSplitter)
					.Style(FEditorStyle::Get(), "DetailsView.Splitter")
					.PhysicalSplitterHandleSize(1.0f)
					.HitDetectionSplitterHandleSize(5.0f)

					+ SSplitter::Slot()
					.Value(ColumnSizeData.LeftColumnWidth)
					.OnSlotResized(SSplitter::FOnSlotResized::CreateSP(this, &SDetailSingleItemRow::OnLeftColumnResized))
					[
						InternalLeftColumnRowBox
					]

					+ SSplitter::Slot()
					.Value(ColumnSizeData.RightColumnWidth)
					.OnSlotResized(ColumnSizeData.OnWidthChanged)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						[
							SAssignNew(HBox, SHorizontalBox)
							+ SHorizontalBox::Slot()
							.Padding(DetailWidgetConstants::RightRowPadding)
							.HAlign(Row.ValueWidget.HorizontalAlignment)
							.VAlign(Row.ValueWidget.VerticalAlignment)
							[
								SNew(SBox)
								.IsEnabled(IsPropertyEditingEnabled)
								[
									ValueWidget.ToSharedRef()
								]
							]
						]
					];
				Widget = Splitter;
			}
			else
			{
				Row.WholeRowWidget.Widget->SetEnabled(IsPropertyEditingEnabled);
				InternalLeftColumnRowBox->AddSlot()
					.HAlign(Row.WholeRowWidget.HorizontalAlignment)
					.VAlign(Row.WholeRowWidget.VerticalAlignment)
					.Padding(DetailWidgetConstants::LeftRowPadding)
					[
						Row.WholeRowWidget.Widget
					];
				InternalLeftColumnRowBox->AddSlot()
					.Padding(3.0f, 0.0f)
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					[
						KeyFrameButton
					];
				Widget = InternalLeftColumnRowBox;
			}
		}
	}
	else
	{
		// details panel layout became invalid.  This is probably a scenario where a widget is coming into view in the parent tree but some external event previous in the frame has invalidated the contents of the details panel.
		// The next frame update of the details panel will fix it
		Widget = SNew(SSpacer);
	}

	this->ChildSlot
	[	
		SNew( SBorder )
		.BorderImage( this, &SDetailSingleItemRow::GetBorderImage )
		.Padding( FMargin( 0.0f, 0.0f, SDetailTableRowBase::ScrollbarPaddingSize, 0.0f ) )
		[
			Widget
		]
	];

	STableRow< TSharedPtr< FDetailTreeNode > >::ConstructInternal(
		STableRow::FArguments()
			.Style(FEditorStyle::Get(), "DetailsView.TreeView.TableRow")
			.ShowSelection(false)
			.OnDragEnter(ArrayDragDelegate)
			.OnDragLeave(ArrayDragLeaveDelegate)
			.OnDrop(ArrayDropDelegate),
		InOwnerTableView
	);
}


bool SDetailSingleItemRow::OnContextMenuOpening(FMenuBuilder& MenuBuilder)
{
	const bool bIsCopyPasteBound = Customization->GetWidgetRow().IsCopyPasteBound();

	FUIAction CopyAction;
	FUIAction PasteAction;

	if(bIsCopyPasteBound)
	{
		CopyAction = Customization->GetWidgetRow().CopyMenuAction;
		PasteAction = Customization->GetWidgetRow().PasteMenuAction;
	}
	else
	{
		TSharedPtr<FPropertyNode> PropertyNode = Customization->GetPropertyNode();
		if (!PropertyNode.IsValid() && Customization->DetailGroup.IsValid())
		{
			PropertyNode = Customization->DetailGroup->GetHeaderPropertyNode();
		}

		static const FName DisableCopyPasteMetaDataName("DisableCopyPaste");
		if (PropertyNode.IsValid() && !PropertyNode->ParentOrSelfHasMetaData(DisableCopyPasteMetaDataName))
		{
			CopyAction.ExecuteAction = FExecuteAction::CreateSP(this, &SDetailSingleItemRow::OnCopyProperty);
			PasteAction.ExecuteAction = FExecuteAction::CreateSP(this, &SDetailSingleItemRow::OnPasteProperty);
			PasteAction.CanExecuteAction = FCanExecuteAction::CreateSP(this, &SDetailSingleItemRow::CanPasteProperty);
		}
	}

	if (CopyAction.IsBound() && PasteAction.IsBound())
	{
		MenuBuilder.AddMenuSeparator();

		MenuBuilder.AddMenuEntry(
			NSLOCTEXT("PropertyView", "CopyProperty", "Copy"),
			NSLOCTEXT("PropertyView", "CopyProperty_ToolTip", "Copy this property value"),
			FSlateIcon(),
			CopyAction);

		MenuBuilder.AddMenuEntry(
			NSLOCTEXT("PropertyView", "PasteProperty", "Paste"),
			NSLOCTEXT("PropertyView", "PasteProperty_ToolTip", "Paste the copied value here"),
			FSlateIcon(),
			PasteAction);

		return true;
	}

	return false;
}

void SDetailSingleItemRow::OnLeftColumnResized( float InNewWidth )
{
	// This has to be bound or the splitter will take it upon itself to determine the size
	// We do nothing here because it is handled by the column size data
}

void SDetailSingleItemRow::OnCopyProperty()
{
	if (OwnerTreeNode.IsValid())
	{
		TSharedPtr<FPropertyNode> PropertyNode = Customization->GetPropertyNode();
		if (!PropertyNode.IsValid() && Customization->DetailGroup.IsValid())
		{
			PropertyNode = Customization->DetailGroup->GetHeaderPropertyNode();
		}
		if (PropertyNode.IsValid())
		{
			TSharedPtr<IPropertyHandle> Handle = PropertyEditorHelpers::GetPropertyHandle(PropertyNode.ToSharedRef(), OwnerTreeNode.Pin()->GetDetailsView()->GetNotifyHook(), OwnerTreeNode.Pin()->GetDetailsView()->GetPropertyUtilities());

			FString Value;
			if (Handle->GetValueAsFormattedString(Value, PPF_Copy) == FPropertyAccess::Success)
			{
				FPlatformApplicationMisc::ClipboardCopy(*Value);
			}
		}
	}
}

void SDetailSingleItemRow::OnPasteProperty()
{
	FString ClipboardContent;
	FPlatformApplicationMisc::ClipboardPaste(ClipboardContent);

	if (!ClipboardContent.IsEmpty() && OwnerTreeNode.IsValid())
	{
		TSharedPtr<FPropertyNode> PropertyNode = Customization->GetPropertyNode();
		if (!PropertyNode.IsValid() && Customization->DetailGroup.IsValid())
		{
			PropertyNode = Customization->DetailGroup->GetHeaderPropertyNode();
		}
		if (PropertyNode.IsValid())
		{
			TSharedPtr<IPropertyHandle> Handle = PropertyEditorHelpers::GetPropertyHandle(PropertyNode.ToSharedRef(), OwnerTreeNode.Pin()->GetDetailsView()->GetNotifyHook(), OwnerTreeNode.Pin()->GetDetailsView()->GetPropertyUtilities());

			Handle->SetValueFromFormattedString(ClipboardContent);
		}
	}
}

bool SDetailSingleItemRow::CanPasteProperty() const
{
	// Prevent paste from working if the property's edit condition is not met.
	TSharedPtr<FDetailPropertyRow> PropertyRow = Customization->PropertyRow;
	if (!PropertyRow.IsValid() && Customization->DetailGroup.IsValid())
	{
		PropertyRow = Customization->DetailGroup->GetHeaderPropertyRow();
	}

	if (PropertyRow.IsValid())
	{
		FPropertyEditor* PropertyEditor = PropertyRow->GetPropertyEditor().Get();
		if (PropertyEditor)
		{
			return !PropertyEditor->IsEditConst() && (!PropertyEditor->HasEditCondition() || PropertyEditor->IsEditConditionMet());
		}
	}

	FString ClipboardContent;
	if( OwnerTreeNode.IsValid() )
	{
		FPlatformApplicationMisc::ClipboardPaste(ClipboardContent);
	}

	return !ClipboardContent.IsEmpty();
}

const FSlateBrush* SDetailSingleItemRow::GetBorderImage() const
{
	if( IsHighlighted() )
	{
		return FEditorStyle::GetBrush("DetailsView.CategoryMiddle_Highlighted");
	}
	else if (IsHovered() && !bIsHoveredDragTarget)
	{
		return FEditorStyle::GetBrush("DetailsView.CategoryMiddle_Hovered");
	}
	else if (bIsHoveredDragTarget)
	{
		return FEditorStyle::GetBrush("DetailsView.CategoryMiddle_Highlighted");
	}
	else
	{
		return FEditorStyle::GetBrush("DetailsView.CategoryMiddle");
	}
}

TSharedRef<SWidget> SDetailSingleItemRow::CreateExtensionWidget(TSharedRef<SWidget> ValueWidget, FDetailLayoutCustomization& InCustomization, TSharedRef<FDetailTreeNode> InTreeNode)
{
	if(InTreeNode->GetParentCategory().IsValid())
	{
		IDetailsViewPrivate* DetailsView = InTreeNode->GetDetailsView();
		TSharedPtr<IDetailPropertyExtensionHandler> ExtensionHandler = DetailsView->GetExtensionHandler();

		if(ExtensionHandler.IsValid() && InCustomization.HasPropertyNode())
		{
			TSharedPtr<IPropertyHandle> Handle = PropertyEditorHelpers::GetPropertyHandle(InCustomization.GetPropertyNode().ToSharedRef(), nullptr, nullptr);

			UClass* ObjectClass = InCustomization.GetPropertyNode()->FindObjectItemParent()->GetObjectBaseClass();
			if(Handle->IsValidHandle() && ExtensionHandler->IsPropertyExtendable(ObjectClass, *Handle))
			{
				ValueWidget = SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						ValueWidget
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						ExtensionHandler->GenerateExtensionWidget(ObjectClass, Handle)
					];
			}
		}
	}

	return ValueWidget;
}

TSharedRef<SWidget> SDetailSingleItemRow::CreateKeyframeButton( FDetailLayoutCustomization& InCustomization, TSharedRef<FDetailTreeNode> InTreeNode )
{
	IDetailsViewPrivate* DetailsView = InTreeNode->GetDetailsView();

	KeyframeHandler = DetailsView->GetKeyframeHandler();

	EVisibility SetKeyVisibility = EVisibility::Collapsed;

	if (InCustomization.HasPropertyNode() && KeyframeHandler.IsValid() )
	{
		TSharedPtr<IPropertyHandle> Handle = PropertyEditorHelpers::GetPropertyHandle(InCustomization.GetPropertyNode().ToSharedRef(), nullptr, nullptr);

		UClass* ObjectClass = InCustomization.GetPropertyNode()->FindObjectItemParent()->GetObjectBaseClass();
		SetKeyVisibility = KeyframeHandler.Pin()->IsPropertyKeyable(ObjectClass, *Handle) ? EVisibility::Visible : EVisibility::Collapsed;
		
	}

	return
		SNew(SButton)
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.ContentPadding(0.0f)
		.ButtonStyle(FEditorStyle::Get(), "Sequencer.AddKey.Details")
		.Visibility(SetKeyVisibility)
		.IsEnabled( this, &SDetailSingleItemRow::IsKeyframeButtonEnabled, InTreeNode )
		.ToolTipText( NSLOCTEXT("PropertyView", "AddKeyframeButton_ToolTip", "Adds a keyframe for this property to the current animation") )
		.OnClicked( this, &SDetailSingleItemRow::OnAddKeyframeClicked );
}

bool SDetailSingleItemRow::IsKeyframeButtonEnabled(TSharedRef<FDetailTreeNode> InTreeNode) const
{
	return
		InTreeNode->IsPropertyEditingEnabled().Get() &&
		KeyframeHandler.IsValid() &&
		KeyframeHandler.Pin()->IsPropertyKeyingEnabled();
}

FReply SDetailSingleItemRow::OnAddKeyframeClicked()
{	
	if( KeyframeHandler.IsValid() )
	{
		TSharedPtr<IPropertyHandle> Handle = PropertyEditorHelpers::GetPropertyHandle(Customization->GetPropertyNode().ToSharedRef(), nullptr, nullptr);

		KeyframeHandler.Pin()->OnKeyPropertyClicked(*Handle);
	}

	return FReply::Handled();
}

bool SDetailSingleItemRow::IsHighlighted() const
{
	return OwnerTreeNode.Pin()->IsHighlighted();
}

void SArrayRowHandle::Construct(const FArguments& InArgs)
{
	ParentRow = InArgs._ParentRow;

	ChildSlot
	[
		InArgs._Content.Widget
	];
}

FReply SArrayRowHandle::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		{	
			TSharedPtr<FDragDropOperation> DragDropOp = CreateDragDropOperation(ParentRow.Pin());
			if (DragDropOp.IsValid())
			{
				return FReply::Handled().BeginDragDrop(DragDropOp.ToSharedRef());
			}
		}
	}

	return FReply::Unhandled();

}

TSharedPtr<FArrayRowDragDropOp> SArrayRowHandle::CreateDragDropOperation(TSharedPtr<SDetailSingleItemRow> InRow)
{
	TSharedPtr<FArrayRowDragDropOp> Operation = MakeShareable(new FArrayRowDragDropOp(InRow));

	return Operation;
}
