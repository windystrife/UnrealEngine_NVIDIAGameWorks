// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;

namespace UnrealBuildTool
{
	/// <summary>
	/// Extension methods for hash sets
	/// </summary>
	public static class HashSetExtensions
	{
		/// <summary>
		/// Convert a sequence to a hashset
		/// </summary>
		/// <typeparam name="T">Type of elements in the sequence</typeparam>
		/// <param name="Sequence">Sequence to convert</param>
		/// <returns>HashSet constructed from the sequence</returns>
		public static HashSet<T> ToHashSet<T>(this IEnumerable<T> Sequence)
		{
			return new HashSet<T>(Sequence);
		}
	}
}