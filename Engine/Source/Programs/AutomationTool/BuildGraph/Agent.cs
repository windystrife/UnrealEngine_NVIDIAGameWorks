// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml;

namespace AutomationTool
{
	/// <summary>
	/// Stores a list of nodes which can be executed on a single agent
	/// </summary>
	[DebuggerDisplay("{Name}")]
	class Agent
	{
		/// <summary>
		/// Name of this agent. Used for display purposes in a build system.
		/// </summary>
		public string Name;

		/// <summary>
		/// Array of valid agent types that these nodes may run on. When running in the build system, this determines the class of machine that should
		/// be selected to run these nodes. The first defined agent type for this branch will be used.
		/// </summary>
		public string[] PossibleTypes;

		/// <summary>
		/// List of nodes in this agent group.
		/// </summary>
		public List<Node> Nodes = new List<Node>();

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InName">Name of this agent group</param>
		/// <param name="InPossibleTypes">Array of valid agent types. See comment for AgentTypes member.</param>
		public Agent(string InName, string[] InPossibleTypes)
		{
			Name = InName;
			PossibleTypes = InPossibleTypes;
		}

		/// <summary>
		/// Writes this agent group out to a file, filtering nodes by a controlling trigger
		/// </summary>
		/// <param name="Writer">The XML writer to output to</param>
		/// <param name="ControllingTrigger">The controlling trigger to filter by</param>
		public void Write(XmlWriter Writer, ManualTrigger ControllingTrigger)
		{
			if (Nodes.Any(x => x.ControllingTrigger == ControllingTrigger))
			{
				Writer.WriteStartElement("Agent");
				Writer.WriteAttributeString("Name", Name);
				Writer.WriteAttributeString("Type", String.Join(";", PossibleTypes));
				foreach (Node Node in Nodes)
				{
					if (Node.ControllingTrigger == ControllingTrigger)
					{
						Node.Write(Writer);
					}
				}
				Writer.WriteEndElement();
			}
		}
	}
}
