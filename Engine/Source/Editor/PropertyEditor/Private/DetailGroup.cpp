// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DetailGroup.h"
#include "Widgets/Input/SButton.h"
#include "PropertyHandleImpl.h"
#include "DetailPropertyRow.h"
#include "SImage.h"

#define LOCTEXT_NAMESPACE "FDetailGroup"

FDetailGroup::FDetailGroup( const FName InGroupName, TSharedRef<FDetailCategoryImpl> InParentCategory, const FText& InLocalizedDisplayName, const bool bInStartExpanded )
	: ParentCategory( InParentCategory )
	, LocalizedDisplayName( InLocalizedDisplayName )
	, GroupName( InGroupName )
	, bStartExpanded( bInStartExpanded )
	, ResetEnabled(false)
{

}

FDetailWidgetRow& FDetailGroup::HeaderRow()
{
	HeaderCustomization = MakeShareable( new FDetailLayoutCustomization );
	HeaderCustomization->WidgetDecl = MakeShareable( new FDetailWidgetRow );

	return *HeaderCustomization->WidgetDecl;
}

IDetailPropertyRow& FDetailGroup::HeaderProperty( TSharedRef<IPropertyHandle> PropertyHandle )
{
	check( PropertyHandle->IsValidHandle() );

	PropertyHandle->MarkHiddenByCustomization();

	HeaderCustomization = MakeShareable( new FDetailLayoutCustomization );
	HeaderCustomization->PropertyRow = MakeShareable( new FDetailPropertyRow( StaticCastSharedRef<FPropertyHandleBase>( PropertyHandle )->GetPropertyNode(), ParentCategory.Pin().ToSharedRef() ) );

	return *HeaderCustomization->PropertyRow;
}

FDetailWidgetRow& FDetailGroup::AddWidgetRow()
{
	FDetailLayoutCustomization NewCustomization;
	NewCustomization.WidgetDecl = MakeShareable( new FDetailWidgetRow );

	GroupChildren.Add( NewCustomization );

	return *NewCustomization.WidgetDecl;
}

IDetailPropertyRow& FDetailGroup::AddPropertyRow( TSharedRef<IPropertyHandle> PropertyHandle )
{
	check( PropertyHandle->IsValidHandle() );

	PropertyHandle->MarkHiddenByCustomization();

	FDetailLayoutCustomization NewCustomization;
	NewCustomization.PropertyRow = MakeShareable( new FDetailPropertyRow( StaticCastSharedRef<FPropertyHandleBase>( PropertyHandle )->GetPropertyNode(), ParentCategory.Pin().ToSharedRef() ) );

	GroupChildren.Add( NewCustomization );

	return *NewCustomization.PropertyRow;
}

TSharedPtr<IDetailPropertyRow> FDetailGroup::FindPropertyRow(TSharedRef<IPropertyHandle> PropertyHandle) const
{
	for (const FDetailLayoutCustomization& Customization : GroupChildren)
	{
		if (Customization.PropertyRow.IsValid() && Customization.PropertyRow->GetPropertyHandle() == PropertyHandle)
		{
			return Customization.PropertyRow;
		}
	}

	return nullptr;
}

IDetailGroup& FDetailGroup::AddGroup(FName NewGroupName, const FText& InLocalizedDisplayName, bool bInStartExpanded)
{
	FDetailLayoutCustomization NewCustomization;
	NewCustomization.DetailGroup = MakeShareable(new FDetailGroup(NewGroupName, ParentCategory.Pin().ToSharedRef(), InLocalizedDisplayName, bStartExpanded));

	GroupChildren.Add(NewCustomization);

	return *NewCustomization.DetailGroup;
}

void FDetailGroup::ToggleExpansion( bool bExpand )
{
	if( ParentCategory.IsValid() && OwnerTreeNode.IsValid() )
	{
		ParentCategory.Pin()->RequestItemExpanded( OwnerTreeNode.Pin().ToSharedRef(), bExpand );
	}
}

bool FDetailGroup::GetExpansionState() const
{
	if (ParentCategory.IsValid() && OwnerTreeNode.IsValid())
	{
		return ParentCategory.Pin()->GetSavedExpansionState(*OwnerTreeNode.Pin().Get());
	}
	return false;
}

TSharedPtr<FDetailPropertyRow> FDetailGroup::GetHeaderPropertyRow() const
{
	return HeaderCustomization.IsValid() ? HeaderCustomization->PropertyRow : nullptr;
}

TSharedPtr<FPropertyNode> FDetailGroup::GetHeaderPropertyNode() const
{
	return HeaderCustomization.IsValid() ? HeaderCustomization->GetPropertyNode() : nullptr;
}

bool FDetailGroup::HasColumns() const
{
	if( HeaderCustomization.IsValid() && HeaderCustomization->HasPropertyNode() && HeaderCustomization->PropertyRow->HasColumns() )
	{
		return HeaderCustomization->PropertyRow->HasColumns();
	}
	else if( HeaderCustomization.IsValid() && HeaderCustomization->HasCustomWidget() )
	{
		return HeaderCustomization->WidgetDecl->HasColumns();
	}
	
	return true;
}

bool FDetailGroup::RequiresTick() const
{
	bool bRequiresTick = false;
	if( HeaderCustomization.IsValid() && HeaderCustomization->HasPropertyNode() )
	{
		bRequiresTick = HeaderCustomization->PropertyRow->RequiresTick();
	}
	else if ( HeaderCustomization.IsValid() && HeaderCustomization->HasCustomWidget() )
	{
		bRequiresTick = HeaderCustomization->WidgetDecl->VisibilityAttr.IsBound();
	}

	return bRequiresTick;
}

EVisibility FDetailGroup::GetGroupVisibility() const
{
	EVisibility Visibility = EVisibility::Visible;
	if( HeaderCustomization.IsValid() && HeaderCustomization->HasPropertyNode() )
	{
		Visibility = HeaderCustomization->PropertyRow->GetPropertyVisibility();
	}
	else if ( HeaderCustomization.IsValid() && HeaderCustomization->HasCustomWidget() )
	{
		Visibility = HeaderCustomization->WidgetDecl->VisibilityAttr.Get();
	}

	return Visibility;
}

FDetailWidgetRow FDetailGroup::GetWidgetRow()
{
	if( HeaderCustomization.IsValid() && HeaderCustomization->HasPropertyNode() )
	{
		return HeaderCustomization->PropertyRow->GetWidgetRow();
	}
	else if ( HeaderCustomization.IsValid() && HeaderCustomization->HasCustomWidget() )
	{
		return *HeaderCustomization->WidgetDecl;
	}
	else
	{
		FDetailWidgetRow Row;

		Row.NameContent()
		[
			MakeNameWidget()
		];
		Row.ValueContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SButton)
				.OnClicked(this, &FDetailGroup::OnResetClicked)
				.Visibility(this, &FDetailGroup::GetResetVisibility)
				.ContentPadding(FMargin(5.f, 0.f))
				.ToolTipText(LOCTEXT("ResetToDefaultToolTip", "Reset to Default"))
				.ButtonStyle(FEditorStyle::Get(), "NoBorder")
				.Content()
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
				]
			]
		];

		return Row;
	}
}

void FDetailGroup::OnItemNodeInitialized( TSharedRef<FDetailItemNode> InTreeNode, TSharedRef<FDetailCategoryImpl> InParentCategory, const TAttribute<bool>& InIsParentEnabled )
{
	OwnerTreeNode = InTreeNode;
	ParentCategory = InParentCategory;
	IsParentEnabled = InIsParentEnabled;

	if( HeaderCustomization.IsValid() && HeaderCustomization->HasPropertyNode() )
	{
		HeaderCustomization->PropertyRow->OnItemNodeInitialized( InParentCategory, InIsParentEnabled, AsShared() );
	}
}

void FDetailGroup::OnGenerateChildren( FDetailNodeList& OutChildren )
{
	for( int32 ChildIndex = 0; ChildIndex < GroupChildren.Num(); ++ChildIndex )
	{
		TSharedRef<FDetailItemNode> NewNode = MakeShareable( new FDetailItemNode( GroupChildren[ChildIndex], ParentCategory.Pin().ToSharedRef(), IsParentEnabled, AsShared()) );
		NewNode->Initialize();
		OutChildren.Add( NewNode );
	}
}

FReply FDetailGroup::OnNameClicked()
{
	OwnerTreeNode.Pin()->ToggleExpansion();

	return FReply::Handled();
}

TSharedRef<SWidget> FDetailGroup::MakeNameWidget()
{
	return
		SNew( SButton )
		.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
		.ContentPadding(FMargin(0,2,2,2))
		.OnClicked( this, &FDetailGroup::OnNameClicked )				
		.ForegroundColor( FSlateColor::UseForeground() )
		.Content()
		[
			SNew( STextBlock )
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text( LocalizedDisplayName )
		];
}

FReply FDetailGroup::OnResetClicked()
{
	if (ResetEnabled)
	{
		TArray<TSharedPtr<IPropertyHandle>> PropertyHandles;

		if (GetAllChildrenPropertyHandles(PropertyHandles))
		{
			for (TSharedPtr<IPropertyHandle> PropertyHandle : PropertyHandles)
			{
				PropertyHandle->ResetToDefault();
			}

			OnDetailGroupReset.Broadcast();
		}
	}

	return FReply::Handled();
}

EVisibility FDetailGroup::GetResetVisibility() const
{
	if (ResetEnabled)
	{
		TArray<TSharedPtr<IPropertyHandle>> PropertyHandles;

		if (GetAllChildrenPropertyHandles(PropertyHandles))
		{
			for (TSharedPtr<IPropertyHandle> PropertyHandle : PropertyHandles)
			{
				if (PropertyHandle->DiffersFromDefault())
				{
					return EVisibility::Visible;
				}
			}
		}
	}

	return EVisibility::Hidden;
}

void FDetailGroup::EnableReset(bool InValue)
{
	ResetEnabled = InValue;
}

bool FDetailGroup::GetAllChildrenPropertyHandles(TArray<TSharedPtr<IPropertyHandle>>& PropertyHandles) const
{
	PropertyHandles.Reserve(GroupChildren.Num());
	return GetAllChildrenPropertyHandlesRecursive(this, PropertyHandles);
}

bool FDetailGroup::GetAllChildrenPropertyHandlesRecursive(const FDetailGroup* CurrentDetailGroup, TArray<TSharedPtr<IPropertyHandle>>& PropertyHandles) const
{
	bool Result = false;

	for (const FDetailLayoutCustomization& Customization : CurrentDetailGroup->GroupChildren)
	{
		if (Customization.HasPropertyNode())
		{
			PropertyHandles.Add(Customization.PropertyRow->GetPropertyHandle());
			Result = true;
		}
		else if (Customization.HasGroup())
		{
			Result &= GetAllChildrenPropertyHandlesRecursive(Customization.DetailGroup.Get(), PropertyHandles);
		}
		else if (Customization.HasCustomWidget())
		{
			PropertyHandles.Append(Customization.WidgetDecl->GetPropertyHandles());
			Result = true;
		}
	}

	return Result;
}

#undef LOCTEXT_NAMESPACE