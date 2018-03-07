// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "LeapEnums.h"
#include "LeapForwardDeclaration.h"
#include "LeapFinger.h"
#include "LeapFingerList.h"
#include "LeapArm.h"
#include "LeapHand.h"
#include "LeapHandList.h"
#include "LeapGestureList.h"
#include "LeapCircleGesture.h"
#include "LeapKeyTapGesture.h"
#include "LeapScreenTapGesture.h"
#include "LeapSwipeGesture.h"
#include "LeapPointable.h"
#include "LeapPointableList.h"
#include "LeapToolList.generated.h"

/**
* The ToolList class represents a list of Tool objects.
*
* Leap API reference: https://developer.leapmotion.com/documentation/cpp/api/Leap.ToolList.html
*/
UCLASS(BlueprintType)
class LEAPMOTION_API ULeapToolList : public UObject
{
	GENERATED_UCLASS_BODY()
public:
	~ULeapToolList();
	
	/**
	* Appends the members of the specified ToolList to this ToolList.
	*
	* @param	List	A ToolList object containing Tool objects to append to the end of this ToolList.
	* @return	A ToolList object with the current and appended list
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "append", CompactNodeTitle = "", Keywords = "append"), Category = "Leap Tool List")
	ULeapToolList *Append(const ULeapToolList *List);

	/**
	* Returns the number of tools in this list.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "count", CompactNodeTitle = "", Keywords = "count"), Category = "Leap Tool List")
	int32 Count() const;

	/**
	* The member of the list that is farthest to the front within the standard Leap Motion
	* frame of reference(i.e has the largest X coordinate).
	*
	* @return	The frontmost tool, or invalid if list is empty.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "frontmost", CompactNodeTitle = "", Keywords = "frontmost"), Category = "Leap Tool List")
	class ULeapTool *Frontmost();

	/**
	* Reports whether the list is empty.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "isEmpty", CompactNodeTitle = "", Keywords = "is empty"), Category = "Leap Tool List")
	bool IsEmpty() const;

	/**
	* The member of the list that is farthest to the front within the standard Leap Motion
	* frame of reference(i.e has the smallest Y coordinate).
	*
	* @return	The leftmost tool, or invalid if list is empty.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "leftmost", CompactNodeTitle = "", Keywords = "leftmost"), Category = "Leap Tool List")
	class ULeapTool *Leftmost();

	/**
	* Access a list member by its position in the list.
	*
	* @param	Index	position in the list.
	* @return The Pointable object at the specified index.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "getPointableById", CompactNodeTitle = "[]", Keywords = "get pointable by id"), Category = "Leap Tool List")
	class ULeapPointable *GetPointableByIndex(int32 Index);

	/**
	* The member of the list that is farthest to the front within the standard Leap Motion
	* frame of reference(i.e has the largest Y coordinate).
	*
	* @return	The rightmost tool, or invalid if list is empty.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "rightmost", CompactNodeTitle = "", Keywords = "rightmost"), Category = "Leap Tool List")
	class ULeapTool *Rightmost();

	void SetToolList(const class Leap::ToolList &Tools);
	const Leap::ToolList* ToolList();

private:
	class FPrivateToolList* Private;

	UPROPERTY()
	ULeapTool* PLeftmost = nullptr;
	UPROPERTY()
	ULeapTool* PRightmost = nullptr;
	UPROPERTY()
	ULeapTool* PFrontmost = nullptr;
	UPROPERTY()
	ULeapTool* PPointableById = nullptr;
	UPROPERTY()
	ULeapToolList* PAppendedList = nullptr;
};