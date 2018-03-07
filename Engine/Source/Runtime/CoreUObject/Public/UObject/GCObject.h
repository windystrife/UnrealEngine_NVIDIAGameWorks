// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GCObject.h: Abstract base class to allow non-UObject objects reference
				UObject instances with proper handling of them by the
				Garbage Collector.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"

class FGCObject;

class COREUOBJECT_API FGCObject;

/**
 * This nested class is used to provide a UObject interface between non
 * UObject classes and the UObject system. It handles forwarding all
 * calls of AddReferencedObjects() to objects/ classes that register with it.
 */
class COREUOBJECT_API UGCObjectReferencer : public UObject
{
	/**
	 * This is the list of objects that are referenced
	 */
	TArray<FGCObject*> ReferencedObjects;
	/** Critical section used when adding and removing objects */
	FCriticalSection ReferencedObjectsCritical;

public:
	DECLARE_CASTED_CLASS_INTRINSIC_WITH_API(UGCObjectReferencer, UObject, CLASS_Transient, TEXT("/Script/CoreUObject"), CASTCLASS_None, NO_API);

	/**
	 * Adds an object to the referencer list
	 *
	 * @param Object The object to add to the list
	 */
	void AddObject(FGCObject* Object);

	/**
	 * Removes a window from the list so it won't receive serialization events
	 *
	 * @param Object The object to remove from the list
	 */
	void RemoveObject(FGCObject* Object);

	/**
	 * Forwards this call to all registered objects so they can reference
	 * any UObjects they depend upon
	 *
	 * @param InThis This UGCObjectReferencer object.
	 * @param Collector The collector of referenced objects.
	 */
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	
	/**
	 * Destroy function that gets called before the object is freed. This might
	 * be as late as from the destructor.
	 */
	virtual void FinishDestroy() override;
};


/**
 * This class provides common registration for garbage collection for
 * non-UObject classes. It is an abstract base class requiring you to implement
 * the AddReferencedObjects() method.
 */
class COREUOBJECT_API FGCObject
{
	void Init()
	{
		// Some objects can get created after the engine started shutting down (lazy init of singletons etc).
		if (!GIsRequestingExit)
		{
			StaticInit();
			check(GGCObjectReferencer);
			// Add this instance to the referencer's list
			GGCObjectReferencer->AddObject(this);
		}
	}

public:
	/**
	 * The static object referencer object that is shared across all
	 * garbage collectible non-UObject objects.
	 */
	static UGCObjectReferencer* GGCObjectReferencer;

	/**
	 * Initializes the global object referencer and adds it to the root set.
	 */
	static void StaticInit(void)
	{
		if (GGCObjectReferencer == NULL)
		{
			GGCObjectReferencer = NewObject<UGCObjectReferencer>();
			GGCObjectReferencer->AddToRoot();
		}
	}

	/**
	 * Tells the global object that forwards AddReferencedObjects calls on to objects
	 * that a new object is requiring AddReferencedObjects call.
	 */
	FGCObject(void)
	{
		Init();
	}

	/** Copy constructor */
	FGCObject(FGCObject const&)
	{
		Init();
	}

	/** Move constructor */
	FGCObject(FGCObject&&)
	{
		Init();
	}

	/**
	 * Removes this instance from the global referencer's list
	 */
	virtual ~FGCObject(void)
	{
		// GObjectSerializer will be NULL if this object gets destroyed after the exit purge.
		if( GGCObjectReferencer )
		{
			// Remove this instance from the referencer's list
			GGCObjectReferencer->RemoveObject(this);
		}
	}

	/**
	 * Pure virtual that must be overloaded by the inheriting class. Use this
	 * method to serialize any UObjects contained that you wish to keep around.
	 *
	 * @param Collector The collector of referenced objects.
	 */
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) = 0;
};

