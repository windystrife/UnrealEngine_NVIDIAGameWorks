// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "StructurePropertyNode.h"
#include "ItemPropertyNode.h"

void FStructurePropertyNode::InitChildNodes()
{
	const bool bShouldShowHiddenProperties = !!HasNodeFlags(EPropertyNodeFlags::ShouldShowHiddenProperties);
	const bool bShouldShowDisableEditOnInstance = !!HasNodeFlags(EPropertyNodeFlags::ShouldShowDisableEditOnInstance);

	const UStruct* Struct = StructData.IsValid() ? StructData->GetStruct() : NULL;

	for (TFieldIterator<UProperty> It(Struct); It; ++It)
	{
		UProperty* StructMember = *It;

		if (StructMember)
		{
			static const FName Name_InlineEditConditionToggle("InlineEditConditionToggle");
			const bool bOnlyShowAsInlineEditCondition = StructMember->HasMetaData(Name_InlineEditConditionToggle);
			const bool bShowIfEditableProperty = StructMember->HasAnyPropertyFlags(CPF_Edit);
			const bool bShowIfDisableEditOnInstance = !StructMember->HasAnyPropertyFlags(CPF_DisableEditOnInstance) || bShouldShowDisableEditOnInstance;

			if (bShouldShowHiddenProperties || (bShowIfEditableProperty && !bOnlyShowAsInlineEditCondition && bShowIfDisableEditOnInstance))
			{
				TSharedPtr<FItemPropertyNode> NewItemNode(new FItemPropertyNode);//;//CreatePropertyItem(StructMember,INDEX_NONE,this);

				FPropertyNodeInitParams InitParams;
				InitParams.ParentNode = SharedThis(this);
				InitParams.Property = StructMember;
				InitParams.ArrayOffset = 0;
				InitParams.ArrayIndex = INDEX_NONE;
				InitParams.bAllowChildren = true;
				InitParams.bForceHiddenPropertyVisibility = bShouldShowHiddenProperties;
				InitParams.bCreateDisableEditOnInstanceNodes = bShouldShowDisableEditOnInstance;
				InitParams.bCreateCategoryNodes = false;

				NewItemNode->InitNode(InitParams);
				AddChildNode(NewItemNode);
			}
		}
	}
}
