// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "XmlNode.h"


const FString& FXmlAttribute::GetTag() const
{
	return Tag;
}

const FString& FXmlAttribute::GetValue() const
{
	return Value;
}

void FXmlNode::Delete()
{
	const int32 ChildCount = Children.Num();
	for(int32 ChildIndex = 0; ChildIndex < ChildCount; ++ChildIndex)
	{
		check(Children[ChildIndex] != nullptr);
		Children[ChildIndex]->Delete();
		delete Children[ChildIndex];
	}
	Children.Empty();
}

const FXmlNode* FXmlNode::GetNextNode() const
{
	return NextNode;
}

const TArray<FXmlNode*>& FXmlNode::GetChildrenNodes() const
{
	return Children;
}

const FXmlNode* FXmlNode::GetFirstChildNode() const
{
	if(Children.Num() > 0)
	{
		return Children[0];
	}
	else
	{
		return nullptr;
	}
}

const FXmlNode* FXmlNode::FindChildNode(const FString& InTag) const
{
	const int32 ChildCount = Children.Num();
	for(int32 ChildIndex = 0; ChildIndex < ChildCount; ++ChildIndex)
	{
		if(Children[ChildIndex] != nullptr && Children[ChildIndex]->GetTag() == InTag)
		{
			return Children[ChildIndex];
		}
	}

	return nullptr;
}

FXmlNode* FXmlNode::FindChildNode(const FString& InTag)
{
	return const_cast<FXmlNode*>(const_cast<const FXmlNode*>(this)->FindChildNode(InTag));
}

const FString& FXmlNode::GetTag() const
{
	return Tag;
}

const FString& FXmlNode::GetContent() const
{
	return Content;
}


void FXmlNode::SetContent( const FString& InContent )
{
	Content = InContent;
}

FString FXmlNode::GetAttribute(const FString& InTag) const
{
	for(auto Iter(Attributes.CreateConstIterator()); Iter; Iter++)
	{
		if(Iter->GetTag() == InTag)
		{
			return Iter->GetValue();
		}
	}
	return FString();
}

void FXmlNode::AppendChildNode(const FString& InTag, const FString& InContent)
{
	auto NewNode = new FXmlNode;
	NewNode->Tag = InTag;
	NewNode->Content = InContent;

	auto NumChildren = Children.Num();
	if (NumChildren != 0)
	{
		Children[NumChildren - 1]->NextNode = NewNode;
	}
	Children.Push(NewNode);
}
