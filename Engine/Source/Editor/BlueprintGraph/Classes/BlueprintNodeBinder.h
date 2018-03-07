// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/WeakObjectPtr.h"

class UEdGraphNode;

class IBlueprintNodeBinder
{
public:
	/** */
	typedef TSet< TWeakObjectPtr<UObject> > FBindingSet;

public:
	/**
	 * Checks to see if the specified object can be bound by this.
	 * 
	 * @param  BindingCandidate	The object you want to check for.
	 * @return True if BindingCandidate can be bound by this controller, false if not.
	 */
	virtual bool IsBindingCompatible(UObject const* BindingCandidate) const = 0;

	/**
	 * Determines if this will accept more than one binding (used to block multiple 
	 * bindings from being applied to nodes that can only have one).
	 * 
	 * @return True if this will accept multiple bindings, otherwise false.
	 */
	virtual bool CanBindMultipleObjects() const = 0;

	/**
	 * Attempts to bind all bindings to the supplied node.
	 * 
	 * @param  Node	 The node you want bound to.
	 * @return True if all bindings were successfully applied, false if any failed.
	 */
	bool ApplyBindings(UEdGraphNode* Node, FBindingSet const& Bindings) const
	{
		uint32 BindingCount = 0;
		for (TWeakObjectPtr<UObject> const& Binding : Bindings)
		{
			if (Binding.IsValid() && BindToNode(Node, Binding.Get()))
			{
				++BindingCount;
				if (!CanBindMultipleObjects())
				{
					break;
				}
			}
		}
		return (BindingCount == Bindings.Num());
	}

protected:
	/**
	 * Attempts to apply the specified binding to the supplied node.
	 * 
	 * @param  Node		The node you want bound.
	 * @param  Binding	The binding you want applied to Node.
	 * @return True if the binding was successful, false if not.
	 */
	virtual bool BindToNode(UEdGraphNode* Node, UObject* Binding) const = 0;
};
