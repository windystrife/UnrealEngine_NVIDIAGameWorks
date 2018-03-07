// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

namespace Algo
{
	/**
	 * Rearranges the elements so that all the elements for which Predicate returns true precede all those for which it returns false.  (not stable)
	 *
	 * @param	First		pointer to the first element
	 * @param	Num			the number of items
	 * @param	Predicate	unary predicate class
	 * @return	index of the first element in the second group
	 */
	template<class T, class UnaryPredicate>
	int32 Partition(T* Elements, const int32 Num, const UnaryPredicate& Predicate)
	{
		T* First = Elements;
		T* Last = Elements + Num;
		
		while (First != Last) 
		{
			while (Predicate(*First)) 
			{
				++First;
				if (First == Last) 
				{	
					return First - Elements;
				}
			}
		
			do 
			{
				--Last;
				if (First == Last)
				{
					return First - Elements;
				}
			} while (!Predicate(*Last));
		
			Exchange(*First, *Last);
			++First;
		}
	
		return First - Elements;
	}
} //namespace Algo