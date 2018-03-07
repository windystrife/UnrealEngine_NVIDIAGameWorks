// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Forward Decl for implementation */
class FXmlFile;

class XMLPARSER_API FXmlAttribute
{
public:
	FXmlAttribute(const FString& InTag, const FString& InValue)
		: Tag(InTag)
		, Value(InValue)
	{
	}

	/** Gets the tag of the attribute */
	const FString& GetTag() const;

	/** Gets the value of the attribute */
	const FString& GetValue() const;

private:
	/** The tag string */
	FString Tag;

	/** The value string */
	FString Value;
};

/** Xml Node representing a line in an xml file */
class XMLPARSER_API FXmlNode
{
	friend class FXmlFile;

private:

	/** Default ctor, private for FXmlFile use only */
	FXmlNode() : NextNode(nullptr) {}
	/** No copy ctor allowed */
	FXmlNode(const FXmlNode& rhs) {}
	/** dtor */
	~FXmlNode() { Delete(); }

private:

	/** Recursively deletes the nodes for cleanup */
	void Delete();

public:

	/** Gets the next node in a list of nodes */
	const FXmlNode* GetNextNode() const;
	/** Gets a list of children nodes */
	const TArray<FXmlNode*>& GetChildrenNodes() const;
	/** Gets the first child of this node which can be iterated into with GetNextNode */
	const FXmlNode* GetFirstChildNode() const;
	/** Finds the first child node that contains the specified tag */
	const FXmlNode* FindChildNode(const FString& InTag) const;
	/** Finds the first child node that contains the specified tag */
	FXmlNode* FindChildNode(const FString& InTag);
	/** Gets the tag of the node */
	const FString& GetTag() const;
	/** Gets the value of the node */
	const FString& GetContent() const;
	/** Sets the new value of the node */
	void SetContent(const FString& InContent);

	/**
	 * Gets all of the attributes in this node
	 *
	 * @return	List of attributes in this node
	 */
	const TArray<FXmlAttribute>& GetAttributes() const
	{
		return Attributes;
	}

	/** Gets an attribute that corresponds with the passed-in tag */
	FString GetAttribute(const FString& InTag) const;
	/** Adds a simple tag with content to the current node */
	void AppendChildNode(const FString& InTag, const FString& InContent);
private:

	/** The list of children nodes */
	TArray<FXmlNode*> Children;
	/** Attributes of this node */
	TArray<FXmlAttribute> Attributes;
	/** Tag of the node */
	FString Tag;
	/** Content of the node */
	FString Content;
	/** Next pointer */
	FXmlNode* NextNode;

};
