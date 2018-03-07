// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace AutomationTool
{
	/// <summary>
	/// Defines a badge which gives an at-a-glance summary of part of the build, and can be displayed in UGS
	/// </summary>
	class Badge
	{
		/// <summary>
		/// Name of this badge
		/// </summary>
		public readonly string Name;

		/// <summary>
		/// Depot path to the project that this badge applies to. Used for filtering in UGS.
		/// </summary>
		public readonly string Project;

		/// <summary>
		/// The changelist to post the badge for
		/// </summary>
		public readonly int Change;

		/// <summary>
		/// Set of nodes that this badge reports the status of
		/// </summary>
		public HashSet<Node> Nodes = new HashSet<Node>();

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InName">Name of this report</param>
		/// <param name="InProject">Depot path to the project that this badge applies to</param>
		/// <param name="InChange">The changelist to post the badge for</param>
		public Badge(string InName, string InProject, int InChange)
		{
			Name = InName;
			Project = InProject;
			Change = InChange;
		}

		/// <summary>
		/// Get the name of this badge
		/// </summary>
		/// <returns>The name of this badge</returns>
		public override string ToString()
		{
			return Name;
		}
	}
}
