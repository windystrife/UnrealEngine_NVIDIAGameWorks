// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/UnrealType.h"

class AActor;

namespace FMatineeUtils
{
	/**
 	 * Looks up the matching float property and returns a
	 * reference to the actual value.
	 */
	ENGINE_API float* GetInterpFloatPropertyRef( AActor* InActor, FName InPropName );	
	
	/**
	 * Looks up the matching boolean property and returns a reference to the actual value.
	 *
	 * @param   InName  The name of boolean property to retrieve a reference.
	 * @return  A pointer to the actual value; NULL if the property was not found.
	 */
	ENGINE_API uint32* GetInterpBoolPropertyRef( AActor* InActor, FName InPropName, UBoolProperty*& OutProperty );
	
	/**
 	 * Looks up the matching vector property and returns a
	 * reference to the actual value.
	 */
	ENGINE_API FVector* GetInterpVectorPropertyRef( AActor* InActor, FName InPropName );
	
	/**
	 * Looks up the matching color property and returns a
	 * reference to the actual value.
	 */
	ENGINE_API FColor* GetInterpColorPropertyRef( AActor* InActor, FName InPropName );
	
	/**
	 * Looks up the matching color property and returns a
	 * reference to the actual value.
	 */
	ENGINE_API FLinearColor* GetInterpLinearColorPropertyRef( AActor* InActor, FName InPropName );
		
	/** 
	 *	Get the names of any float properties of this Actor which are marked as 'interp'.
	 *	Will also look in components of this Actor, and makes the name in the form 'componentname.propertyname'.
	 */
	ENGINE_API void GetInterpFloatPropertyNames( AActor* InActor, TArray<FName> &OutNames );

	/** 
	*	Get the names of any boolean properties of this Actor which are marked as 'interp'.
	*	Will also look in components of this Actor, and makes the name in the form 'componentname.propertyname'.
	* 
	* @param	OutNames	The names of all the boolean properties marked as 'interp'.
	*/
	ENGINE_API void GetInterpBoolPropertyNames( AActor* InActor, TArray<FName>& OutNames );

	/** 
	*	Get the names of any vector properties of this Actor which are marked as 'interp'.
	*	Will also look in components of this Actor, and makes the name in the form 'componentname.propertyname'.
	*/
	ENGINE_API void GetInterpVectorPropertyNames( AActor* InActor, TArray<FName> &OutNames);

	/** 
	*	Get the names of any color properties of this Actor which are marked as 'interp'.
	*	Will also look in components of this Actor, and makes the name in the form 'componentname.propertyname'.
	*/
	ENGINE_API void GetInterpColorPropertyNames( AActor* InActor, TArray<FName> &OutNames);

	/** 
	*	Get the names of any linear color properties of this Actor which are marked as 'interp'.
	*	Will also look in components of this Actor, and makes the name in the form 'componentname.propertyname'.
	*/
	ENGINE_API void GetInterpLinearColorPropertyNames( AActor* InActor, TArray<FName> &OutNames);

	/** 
	* This utility for returning the Object and an offset within it for the given property name.
	* If the name contains no period, we assume the property is in InObject, so basically just look up the offset.
	* But if it does contain a period, we first see if the first part is a struct property.
	* If not we see if it is a component name and return component pointer instead of actor pointer.
	* You may (optionally) specify property class types (and names of structs where the class type contains a struct)
	*
	* @param   OutPropContainer	The 'container' the holds the property
	* @param   OutProperty		The property requested
	* @param   InPropName		The name of the required property
	* @param   InObject			The object to get the property from
	* @param   RequiredClasses	Class requirements of the property (NULL will match any property matching the given name will be returned)
	* @param   StructTypes		Names of the structs of the property if struct is contained in the list of class types (NULL will match any property that is a struct)
	* returns the passed object if a match was found otherwise returns NULL
	*/
	ENGINE_API UObject* FindObjectAndPropOffset( void*& OutPropContainer, UProperty*& OutProperty, UObject* InObject, FName InPropName, const TArray<UClass*>* RequiredClasses = NULL, const TArray<FName>* StructTypes = NULL );

	/**
	 * Check if a property/struct matches given class requirements
	 *
	 * @param   Prop			The property to check
	 * @param   RequiredClasses The type(s) of class to check the property against (NULL will always consider the property a match)
	 * @param   StructTypes		The names of the structs if a STRUCT property is specified in the class check parameter (E.G. NAME_Vector, NAME_Color etc)
	 * @return  true if the property matches the given criteria
	 */
	ENGINE_API bool PropertyMatchesClassRequirements( UProperty* Prop, const TArray<UClass*>* RequiredClasses, const TArray<FName>* StructTypes );
	
	/**
	* Gets the address (typed) of a property to modify
	*
	* @param BaseObject			The base object to search for properties in
	* @param InPropName			The name or path to the property
	* @param OutProperty		The UProperty that would be modified by changing the address
	* @param OutPropertyOwner	The owner of the UProperty. This is the actual UObject that is modified
	*
	* @return The typed address
	*/
	template<typename T>
	T* GetPropertyAddress( UObject* BaseObject, FName InPropName, UProperty*& OutProperty, UObject*& OutPropertyOwner )
	{
		void* PropContainer = NULL;
		UObject* PropObject = FindObjectAndPropOffset( PropContainer, OutProperty, BaseObject, InPropName );
		if (PropObject != NULL)
		{
			OutPropertyOwner = PropObject;
			return OutProperty->ContainerPtrToValuePtr<T>(PropContainer);
		}

		return NULL;
	}
	
};
