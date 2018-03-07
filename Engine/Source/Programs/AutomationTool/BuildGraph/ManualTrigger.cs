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
	/// Defines a manual trigger; a fence behind which build nodes will only be built if explicitly activated in the job setup.
	/// </summary>
	class ManualTrigger
	{
		/// <summary>
		/// The parent trigger
		/// </summary>
		public readonly ManualTrigger Parent;

		/// <summary>
		/// Name of this trigger
		/// </summary>
		public readonly string Name;

		/// <summary>
		/// List of users to notify about this trigger
		/// </summary>
		public HashSet<string> NotifyUsers = new HashSet<string>(StringComparer.InvariantCultureIgnoreCase);

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InParent">The parent trigger</param>
		/// <param name="InName">Name of this trigger</param>
		public ManualTrigger(ManualTrigger InParent, string InName)
		{
			Parent = InParent;
			Name = InName;
		}

		/// <summary>
		/// Checks whether this trigger is upstream of another
		/// </summary>
		/// <param name="Other">The other trigger to check</param>
		/// <returns>True if this trigger is upstream of the given trigger</returns>
		public bool IsUpstreamFrom(ManualTrigger Other)
		{
			for(ManualTrigger Ancestor = Other; Ancestor != null; Ancestor = Ancestor.Parent)
			{
				if(Ancestor == this)
				{
					return true;
				}
			}
			return false;
		}

		/// <summary>
		/// Checks whether this trigger is downstream of another
		/// </summary>
		/// <param name="Other">The parent trigger to check</param>
		/// <returns>True if the trigger is downstream of the given trigger</returns>
		public bool IsDownstreamFrom(ManualTrigger Other)
		{
			for(ManualTrigger Ancestor = Parent; Ancestor != null; Ancestor = Ancestor.Parent)
			{
				if(Ancestor == Other)
				{
					return true;
				}
			}
			return false;
		}

		/// <summary>
		/// The qualified name of this trigger. For triggers which are nested more than one level deep, this consists of all the required triggers in order, separated by dot characters.
		/// </summary>
		public string QualifiedName
		{
			get { return (Parent == null)? Name : (Parent.QualifiedName + "." + Name); }
		}

		/// <summary>
		/// Get the name of this trigger
		/// </summary>
		/// <returns>The qualified name of this trigger</returns>
		public override string ToString()
		{
			return QualifiedName;
		}
	}
}
