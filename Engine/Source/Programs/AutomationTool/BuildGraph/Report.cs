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
	/// Defines a report to be generated as part of the build.
	/// </summary>
	class Report
	{
		/// <summary>
		/// Name of this trigger
		/// </summary>
		public readonly string Name;

		/// <summary>
		/// Set of nodes to include in the report
		/// </summary>
		public HashSet<Node> Nodes = new HashSet<Node>();

		/// <summary>
		/// List of users to notify with this report
		/// </summary>
		public HashSet<string> NotifyUsers = new HashSet<string>(StringComparer.InvariantCultureIgnoreCase);

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InName">Name of this report</param>
		public Report(string InName)
		{
			Name = InName;
		}

		/// <summary>
		/// Get the name of this report
		/// </summary>
		/// <returns>The name of this report</returns>
		public override string ToString()
		{
			return Name;
		}
	}
}
