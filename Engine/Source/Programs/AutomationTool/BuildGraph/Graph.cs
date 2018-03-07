// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using AutomationTool;
using UnrealBuildTool;
using System.Reflection;
using System.Xml;
using System.Linq;
using System.Diagnostics;
using Tools.DotNETCommon;

namespace AutomationTool
{
	/// <summary>
	/// Options for how the graph should be printed
	/// </summary>
	enum GraphPrintOptions
	{
		/// <summary>
		/// Includes a list of the graph options
		/// </summary>
		ShowCommandLineOptions = 0x1,

		/// <summary>
		/// Includes the list of dependencies for each node
		/// </summary>
		ShowDependencies = 0x2,

		/// <summary>
		/// Includes the list of notifiers for each node
		/// </summary>
		ShowNotifications = 0x4,
	}

	/// <summary>
	/// Diagnostic message from the graph script. These messages are parsed at startup, then culled along with the rest of the graph nodes before output. Doing so
	/// allows errors and warnings which are only output if a node is part of the graph being executed.
	/// </summary>
	class GraphDiagnostic
	{
		/// <summary>
		/// The diagnostic event type
		/// </summary>
		public LogEventType EventType;

		/// <summary>
		/// The message to display
		/// </summary>
		public string Message;

		/// <summary>
		/// The node which this diagnostic is declared in. If the node is culled from the graph, the message will not be displayed.
		/// </summary>
		public Node EnclosingNode;

		/// <summary>
		/// The agent that this diagnostic is declared in. If the entire agent is culled from the graph, the message will not be displayed.
		/// </summary>
		public Agent EnclosingAgent;

		/// <summary>
		/// The trigger that this diagnostic is declared in. If this trigger is not being run, the message will not be displayed.
		/// </summary>
		public ManualTrigger EnclosingTrigger;
	}

	/// <summary>
	/// Represents a graph option. These are expanded during preprocessing, but are retained in order to display help messages.
	/// </summary>
	class GraphOption
	{
		/// <summary>
		/// Name of this option
		/// </summary>
		public string Name;

		/// <summary>
		/// Description for this option
		/// </summary>
		public string Description;

		/// <summary>
		/// Default value for this option
		/// </summary>
		public string DefaultValue;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Name">The name of this option</param>
		/// <param name="Description">Description of the option, for display on help pages</param>
		/// <param name="DefaultValue">Default value for the option</param>
		public GraphOption(string Name, string Description, string DefaultValue)
		{
			this.Name = Name;
			this.Description = Description;
			this.DefaultValue = DefaultValue;
		}

		/// <summary>
		/// Returns a name of this option for debugging
		/// </summary>
		/// <returns>Name of the option</returns>
		public override string ToString()
		{
			return Name;
		}
	}

	/// <summary>
	/// Definition of a graph.
	/// </summary>
	class Graph
	{
		/// <summary>
		/// List of options, in the order they were specified
		/// </summary>
		public List<GraphOption> Options = new List<GraphOption>();

		/// <summary>
		/// List of agents containing nodes to execute
		/// </summary>
		public List<Agent> Agents = new List<Agent>();

		/// <summary>
		/// All manual triggers that are part of this graph
		/// </summary>
		public Dictionary<string, ManualTrigger> NameToTrigger = new Dictionary<string, ManualTrigger>(StringComparer.InvariantCultureIgnoreCase);

		/// <summary>
		/// Mapping from name to agent
		/// </summary>
		public Dictionary<string, Agent> NameToAgent = new Dictionary<string, Agent>(StringComparer.InvariantCultureIgnoreCase);

		/// <summary>
		/// Mapping of names to the corresponding node.
		/// </summary>
		public Dictionary<string, Node> NameToNode = new Dictionary<string,Node>(StringComparer.InvariantCultureIgnoreCase);

		/// <summary>
		/// Mapping of names to the corresponding report.
		/// </summary>
		public Dictionary<string, Report> NameToReport = new Dictionary<string, Report>(StringComparer.InvariantCultureIgnoreCase);

		/// <summary>
		/// Mapping of names to their corresponding node output.
		/// </summary>
		public HashSet<string> LocalTagNames = new HashSet<string>(StringComparer.InvariantCultureIgnoreCase);

		/// <summary>
		/// Mapping of names to their corresponding node output.
		/// </summary>
		public Dictionary<string, NodeOutput> TagNameToNodeOutput = new Dictionary<string,NodeOutput>(StringComparer.InvariantCultureIgnoreCase);

		/// <summary>
		/// Mapping of aggregate names to their respective nodes
		/// </summary>
		public Dictionary<string, Node[]> AggregateNameToNodes = new Dictionary<string,Node[]>(StringComparer.InvariantCultureIgnoreCase);

		/// <summary>
		/// List of badges that can be displayed for this build
		/// </summary>
		public List<Badge> Badges = new List<Badge>();

		/// <summary>
		/// Diagnostic messages for this graph
		/// </summary>
		public List<GraphDiagnostic> Diagnostics = new List<GraphDiagnostic>();

		/// <summary>
		/// Default constructor
		/// </summary>
		public Graph()
		{
		}

		/// <summary>
		/// Checks whether a given name already exists
		/// </summary>
		/// <param name="Name">The name to check.</param>
		/// <returns>True if the name exists, false otherwise.</returns>
		public bool ContainsName(string Name)
		{
			return NameToNode.ContainsKey(Name) || NameToReport.ContainsKey(Name) || AggregateNameToNodes.ContainsKey(Name);
		}

		/// <summary>
		/// Tries to resolve the given name to one or more nodes. Checks for aggregates, and actual nodes.
		/// </summary>
		/// <param name="Name">The name to search for</param>
		/// <param name="OutNodes">If the name is a match, receives an array of nodes and their output names</param>
		/// <returns>True if the name was found, false otherwise.</returns>
		public bool TryResolveReference(string Name, out Node[] OutNodes)
		{
			// Check if it's a tag reference or node reference
			if(Name.StartsWith("#"))
			{
				// Check if it's a regular node or output name
				NodeOutput Output;
				if(TagNameToNodeOutput.TryGetValue(Name, out Output))
				{
					OutNodes = new Node[]{ Output.ProducingNode };
					return true;
				}
			}
			else
			{
				// Check if it's a regular node or output name
				Node Node;
				if(NameToNode.TryGetValue(Name, out Node))
				{
					OutNodes = new Node[]{ Node };
					return true;
				}

				// Check if it's an aggregate name
				Node[] Nodes;
				if(AggregateNameToNodes.TryGetValue(Name, out Nodes))
				{
					OutNodes = Nodes;
					return true;
				}

				// Check if it's a group name
				Agent Agent;
				if(NameToAgent.TryGetValue(Name, out Agent))
				{
					OutNodes = Agent.Nodes.ToArray();
					return true;
				}
			}

			// Otherwise fail
			OutNodes = null;
			return false;
		}

		/// <summary>
		/// Tries to resolve the given name to one or more node outputs. Checks for aggregates, and actual nodes.
		/// </summary>
		/// <param name="Name">The name to search for</param>
		/// <param name="OutOutputs">If the name is a match, receives an array of nodes and their output names</param>
		/// <returns>True if the name was found, false otherwise.</returns>
		public bool TryResolveInputReference(string Name, out NodeOutput[] OutOutputs)
		{
			// Check if it's a tag reference or node reference
			if(Name.StartsWith("#"))
			{
				// Check if it's a regular node or output name
				NodeOutput Output;
				if(TagNameToNodeOutput.TryGetValue(Name, out Output))
				{
					OutOutputs = new NodeOutput[]{ Output };
					return true;
				}
			}
			else
			{
				// Check if it's a regular node or output name
				Node Node;
				if(NameToNode.TryGetValue(Name, out Node))
				{
					OutOutputs = Node.Outputs.Union(Node.Inputs).ToArray();
					return true;
				}

				// Check if it's an aggregate name
				Node[] Nodes;
				if(AggregateNameToNodes.TryGetValue(Name, out Nodes))
				{
					OutOutputs = Nodes.SelectMany(x => x.Outputs.Union(x.Inputs)).Distinct().ToArray();
					return true;
				}
			}

			// Otherwise fail
			OutOutputs = null;
			return false;
		}

		/// <summary>
		/// Cull the graph to only include the given nodes and their dependencies
		/// </summary>
		/// <param name="TargetNodes">A set of target nodes to build</param>
		public void Select(IEnumerable<Node> TargetNodes)
		{
			// Find this node and all its dependencies
			HashSet<Node> RetainNodes = new HashSet<Node>(TargetNodes);
			foreach(Node TargetNode in TargetNodes)
			{
				RetainNodes.UnionWith(TargetNode.InputDependencies);
			}

			// Remove all the nodes which are not marked to be kept
			foreach(Agent Agent in Agents)
			{
				Agent.Nodes = Agent.Nodes.Where(x => RetainNodes.Contains(x)).ToList();
			}

			// Remove all the empty agents
			Agents.RemoveAll(x => x.Nodes.Count == 0);

			// Trim down the list of nodes for each report to the ones that are being built
			foreach (Report Report in NameToReport.Values)
			{
				Report.Nodes.RemoveWhere(x => !RetainNodes.Contains(x));
			}

			// Remove all the empty reports
			NameToReport = NameToReport.Where(x => x.Value.Nodes.Count > 0).ToDictionary(Pair => Pair.Key, Pair => Pair.Value, StringComparer.InvariantCultureIgnoreCase);

			// Remove all the order dependencies which are no longer part of the graph. Since we don't need to build them, we don't need to wait for them
			foreach(Node Node in RetainNodes)
			{
				Node.OrderDependencies = Node.OrderDependencies.Where(x => RetainNodes.Contains(x)).ToArray();
			}

			// Create a new list of triggers from all the nodes which are left
			NameToTrigger = RetainNodes.Where(x => x.ControllingTrigger != null).Select(x => x.ControllingTrigger).Distinct().ToDictionary(x => x.Name, x => x, StringComparer.InvariantCultureIgnoreCase);

			// Create a new list of aggregates for everything that's left
			AggregateNameToNodes = AggregateNameToNodes.Where(x => x.Value.All(y => RetainNodes.Contains(y))).ToDictionary(Pair => Pair.Key, Pair => Pair.Value, StringComparer.InvariantCultureIgnoreCase);

			// Remove any badges which do not have all their dependencies
			Badges.RemoveAll(x => x.Nodes.Any(y => !RetainNodes.Contains(y)));

			// Remove any diagnostics which are no longer part of the graph
			Diagnostics.RemoveAll(x => (x.EnclosingNode != null && !RetainNodes.Contains(x.EnclosingNode)) || (x.EnclosingAgent != null && !Agents.Contains(x.EnclosingAgent)));
		}

		/// <summary>
		/// Skips the given triggers, collapsing everything inside them into their parent trigger.
		/// </summary>
		/// <param name="Triggers">Set of triggers to skip</param>
		public void SkipTriggers(HashSet<ManualTrigger> Triggers)
		{
			foreach(ManualTrigger Trigger in Triggers)
			{
				NameToTrigger.Remove(Trigger.Name);
			}
			foreach(Node Node in NameToNode.Values)
			{
				while(Triggers.Contains(Node.ControllingTrigger))
				{
					Node.ControllingTrigger = Node.ControllingTrigger.Parent;
				}
			}
			foreach(GraphDiagnostic Diagnostic in Diagnostics)
			{
				while(Triggers.Contains(Diagnostic.EnclosingTrigger))
				{
					Diagnostic.EnclosingTrigger = Diagnostic.EnclosingTrigger.Parent;
				}
			}
		}

		/// <summary>
		/// Writes a preprocessed build graph to a script file
		/// </summary>
		/// <param name="File">The file to load</param>
		/// <param name="SchemaFile">Schema file for validation</param>
		public void Write(FileReference File, FileReference SchemaFile)
		{
			XmlWriterSettings Settings = new XmlWriterSettings();
			Settings.Indent = true;
			Settings.IndentChars = "\t";

			using (XmlWriter Writer = XmlWriter.Create(File.FullName, Settings))
			{
				Writer.WriteStartElement("BuildGraph", "http://www.epicgames.com/BuildGraph");

				if (SchemaFile != null)
				{
					Writer.WriteAttributeString("schemaLocation", "http://www.w3.org/2001/XMLSchema-instance", "http://www.epicgames.com/BuildGraph " + SchemaFile.MakeRelativeTo(File.Directory));
				}

				foreach (Agent Agent in Agents)
				{
					Agent.Write(Writer, null);
				}

				foreach (ManualTrigger ControllingTrigger in Agents.SelectMany(x => x.Nodes).Where(x => x.ControllingTrigger != null).Select(x => x.ControllingTrigger).Distinct())
				{
					Writer.WriteStartElement("Trigger");
					Writer.WriteAttributeString("Name", ControllingTrigger.QualifiedName);
					foreach (Agent Agent in Agents)
					{
						Agent.Write(Writer, ControllingTrigger);
					}
					Writer.WriteEndElement();
				}

				foreach (KeyValuePair<string, Node[]> Aggregate in AggregateNameToNodes)
				{
					Writer.WriteStartElement("Aggregate");
					Writer.WriteAttributeString("Name", Aggregate.Key);
					Writer.WriteAttributeString("Requires", String.Join(";", Aggregate.Value.Select(x => x.Name)));
					Writer.WriteEndElement();
				}

				foreach (Report Report in NameToReport.Values)
				{
					Writer.WriteStartElement("Report");
					Writer.WriteAttributeString("Name", Report.Name);
					Writer.WriteAttributeString("Requires", String.Join(";", Report.Nodes.Select(x => x.Name)));
					Writer.WriteEndElement();
				}

				foreach (Badge Badge in Badges)
				{
					Writer.WriteStartElement("Badge");
					Writer.WriteAttributeString("Name", Badge.Name);
					if (Badge.Project != null)
					{
						Writer.WriteAttributeString("Project", Badge.Project);
					}
					if (Badge.Change != 0)
					{
						Writer.WriteAttributeString("Change", Badge.Change.ToString());
					}
					Writer.WriteAttributeString("Requires", String.Join(";", Badge.Nodes.Select(x => x.Name)));
					Writer.WriteEndElement();
				}

				Writer.WriteEndElement();
			}
		}

		/// <summary>
		/// Export the build graph to a Json file, for parallel execution by the build system
		/// </summary>
		/// <param name="File">Output file to write</param>
		/// <param name="Trigger">The trigger whose nodes to run. Null for the default nodes.</param>
		/// <param name="CompletedNodes">Set of nodes which have been completed</param>
		public void Export(FileReference File, ManualTrigger Trigger, HashSet<Node> CompletedNodes)
		{
			// Find all the nodes which we're actually going to execute. We'll use this to filter the graph.
			HashSet<Node> NodesToExecute = new HashSet<Node>();
			foreach(Node Node in Agents.SelectMany(x => x.Nodes))
			{
				if(!CompletedNodes.Contains(Node) && Node.IsBehind(Trigger))
				{
					NodesToExecute.Add(Node);
				}
			}

			// Open the output file
			using(JsonWriter JsonWriter = new JsonWriter(File.FullName))
			{
				JsonWriter.WriteObjectStart();

				// Write all the agents
				JsonWriter.WriteArrayStart("Groups");
				foreach(Agent Agent in Agents)
				{
					Node[] Nodes = Agent.Nodes.Where(x => NodesToExecute.Contains(x) && x.ControllingTrigger == Trigger).ToArray();
					if(Nodes.Length > 0)
					{
						JsonWriter.WriteObjectStart();
						JsonWriter.WriteValue("Name", Agent.Name);
						JsonWriter.WriteArrayStart("Agent Types");
						foreach(string AgentType in Agent.PossibleTypes)
						{
							JsonWriter.WriteValue(AgentType);
						}
						JsonWriter.WriteArrayEnd();
						JsonWriter.WriteArrayStart("Nodes");
						foreach(Node Node in Nodes)
						{
							JsonWriter.WriteObjectStart();
							JsonWriter.WriteValue("Name", Node.Name);
							JsonWriter.WriteValue("DependsOn", String.Join(";", Node.GetDirectOrderDependencies().Where(x => NodesToExecute.Contains(x) && x.ControllingTrigger == Trigger)));
							JsonWriter.WriteObjectStart("Notify");
							JsonWriter.WriteValue("Default", String.Join(";", Node.NotifyUsers));
							JsonWriter.WriteValue("Submitters", String.Join(";", Node.NotifySubmitters));
							JsonWriter.WriteValue("Warnings", Node.bNotifyOnWarnings);
							JsonWriter.WriteObjectEnd();
							JsonWriter.WriteObjectEnd();
						}
						JsonWriter.WriteArrayEnd();
						JsonWriter.WriteObjectEnd();
					}
				}
				JsonWriter.WriteArrayEnd();

				// Write all the badges
				JsonWriter.WriteArrayStart("Badges");
				foreach (Badge Badge in Badges)
				{
					Node[] Dependencies = Badge.Nodes.Where(x => NodesToExecute.Contains(x) && x.ControllingTrigger == Trigger).ToArray();
					if (Dependencies.Length > 0)
					{
						// Reduce that list to the smallest subset of direct dependencies
						HashSet<Node> DirectDependencies = new HashSet<Node>(Dependencies);
						foreach (Node Dependency in Dependencies)
						{
							DirectDependencies.ExceptWith(Dependency.OrderDependencies);
						}

						JsonWriter.WriteObjectStart();
						JsonWriter.WriteValue("Name", Badge.Name);
						if (!String.IsNullOrEmpty(Badge.Project))
						{
							JsonWriter.WriteValue("Project", Badge.Project);
						}
						if(Badge.Change != 0)
						{
							JsonWriter.WriteValue("Change", Badge.Change);
						}
						JsonWriter.WriteValue("AllDependencies", String.Join(";", Agents.SelectMany(x => x.Nodes).Where(x => Dependencies.Contains(x)).Select(x => x.Name)));
						JsonWriter.WriteValue("DirectDependencies", String.Join(";", DirectDependencies.Select(x => x.Name)));
						JsonWriter.WriteObjectEnd();
					}
				}
				JsonWriter.WriteArrayEnd();

				// Write all the triggers and reports. 
				JsonWriter.WriteArrayStart("Reports");
				foreach (Report Report in NameToReport.Values)
				{
					Node[] Dependencies = Report.Nodes.Where(x => NodesToExecute.Contains(x) && x.ControllingTrigger == Trigger).ToArray();
					if (Dependencies.Length > 0)
					{
						// Reduce that list to the smallest subset of direct dependencies
						HashSet<Node> DirectDependencies = new HashSet<Node>(Dependencies);
						foreach (Node Dependency in Dependencies)
						{
							DirectDependencies.ExceptWith(Dependency.OrderDependencies);
						}

						JsonWriter.WriteObjectStart();
						JsonWriter.WriteValue("Name", Report.Name);
						JsonWriter.WriteValue("AllDependencies", String.Join(";", Agents.SelectMany(x => x.Nodes).Where(x => Dependencies.Contains(x)).Select(x => x.Name)));
						JsonWriter.WriteValue("DirectDependencies", String.Join(";", DirectDependencies.Select(x => x.Name)));
						JsonWriter.WriteValue("Notify", String.Join(";", Report.NotifyUsers));
						JsonWriter.WriteValue("IsTrigger", false);
						JsonWriter.WriteObjectEnd();
					}
				}
				foreach (ManualTrigger DownstreamTrigger in NameToTrigger.Values)
				{
					if(DownstreamTrigger.Parent == Trigger)
					{
						// Find all the nodes that this trigger is dependent on
						HashSet<Node> Dependencies = new HashSet<Node>();
						foreach(Node NodeToExecute in NodesToExecute)
						{
							if(NodeToExecute.IsBehind(DownstreamTrigger))
							{
								Dependencies.UnionWith(NodeToExecute.OrderDependencies.Where(x => x.ControllingTrigger == Trigger));
							}
						}

						// Reduce that list to the smallest subset of direct dependencies
						HashSet<Node> DirectDependencies = new HashSet<Node>(Dependencies);
						foreach(Node Dependency in Dependencies)
						{
							DirectDependencies.ExceptWith(Dependency.OrderDependencies);
						}

						// Write out the object
						JsonWriter.WriteObjectStart();
						JsonWriter.WriteValue("Name", DownstreamTrigger.Name);
						JsonWriter.WriteValue("AllDependencies", String.Join(";", Agents.SelectMany(x => x.Nodes).Where(x => Dependencies.Contains(x)).Select(x => x.Name)));
						JsonWriter.WriteValue("DirectDependencies", String.Join(";", Dependencies.Where(x => DirectDependencies.Contains(x)).Select(x => x.Name)));
						JsonWriter.WriteValue("Notify", String.Join(";", DownstreamTrigger.NotifyUsers));
						JsonWriter.WriteValue("IsTrigger", true);
						JsonWriter.WriteObjectEnd();
					}
				}
				JsonWriter.WriteArrayEnd();

				JsonWriter.WriteObjectEnd();
			}
		}

		/// <summary>
		/// Print the contents of the graph
		/// </summary>
		/// <param name="CompletedNodes">Set of nodes which are already complete</param>
		/// <param name="PrintOptions">Options for how to print the graph</param>
		public void Print(HashSet<Node> CompletedNodes, GraphPrintOptions PrintOptions)
		{
			// Print the options
			if((PrintOptions & GraphPrintOptions.ShowCommandLineOptions) != 0)
			{
				// Get the list of messages
				List<string> Messages = new List<string>();
				foreach(GraphOption Option in Options)
				{
					StringBuilder Message = new StringBuilder();
					Message.AppendFormat("-set:{0}=... {1}", Option.Name, Option.Description);
					if(!String.IsNullOrEmpty(Option.DefaultValue))
					{
						Message.AppendFormat(" (Default: {0})", Option.DefaultValue);
					}
					Messages.Add(Message.ToString());
				}

				// Format them to the log
				if(Messages.Count > 0)
				{
					CommandUtils.Log("");
					CommandUtils.Log("Options:");
					CommandUtils.Log("");
					foreach(string Line in CommandUtils.FormatParams(Messages, 4, 24))
					{
						CommandUtils.Log(Line);
					}
				}
			}

			// Get a list of all the triggers, including the null global one
			List<ManualTrigger> AllTriggers = new List<ManualTrigger>();
			AllTriggers.Add(null);
			AllTriggers.AddRange(NameToTrigger.Values.OrderBy(x => x.QualifiedName));

			// Output all the triggers in order
			CommandUtils.Log("");
			CommandUtils.Log("Graph:");
			foreach(ManualTrigger Trigger in AllTriggers)
			{
				// Filter everything by this trigger
				Dictionary<Agent, Node[]> FilteredAgentToNodes = new Dictionary<Agent,Node[]>();
				foreach(Agent Agent in Agents)
				{
					Node[] Nodes = Agent.Nodes.Where(x => x.ControllingTrigger == Trigger).ToArray();
					if(Nodes.Length > 0)
					{
						FilteredAgentToNodes[Agent] = Nodes;
					}
				}

				// Skip this trigger if there's nothing to display
				if(FilteredAgentToNodes.Count == 0)
				{
					continue;
				}

				// Print the trigger name
				CommandUtils.Log("    Trigger: {0}", (Trigger == null)? "None" : Trigger.QualifiedName);
				if(Trigger != null && PrintOptions.HasFlag(GraphPrintOptions.ShowNotifications))
				{
					foreach(string User in Trigger.NotifyUsers)
					{
						CommandUtils.Log("            notify> {0}", User);
					}
				}

				// Output all the agents for this trigger
				foreach(Agent Agent in Agents)
				{
					Node[] Nodes;
					if(FilteredAgentToNodes.TryGetValue(Agent, out Nodes))
					{
						CommandUtils.Log("        Agent: {0} ({1})", Agent.Name, String.Join(";", Agent.PossibleTypes));
						foreach(Node Node in Nodes)
						{
							CommandUtils.Log("            Node: {0}{1}", Node.Name, CompletedNodes.Contains(Node)? " (completed)" : "");
							if(PrintOptions.HasFlag(GraphPrintOptions.ShowDependencies))
							{
								HashSet<Node> InputDependencies = new HashSet<Node>(Node.GetDirectInputDependencies());
								foreach(Node InputDependency in InputDependencies)
								{
									CommandUtils.Log("                input> {0}", InputDependency.Name);
								}
								HashSet<Node> OrderDependencies = new HashSet<Node>(Node.GetDirectOrderDependencies());
								foreach(Node OrderDependency in OrderDependencies.Except(InputDependencies))
								{
									CommandUtils.Log("                after> {0}", OrderDependency.Name);
								}
							}
							if(PrintOptions.HasFlag(GraphPrintOptions.ShowNotifications))
							{
								string Label = Node.bNotifyOnWarnings? "warnings" : "errors";
								foreach(string User in Node.NotifyUsers)
								{
									CommandUtils.Log("                {0}> {1}", Label, User);
								}
								foreach(string Submitter in Node.NotifySubmitters)
								{
									CommandUtils.Log("                {0}> submitters to {1}", Label, Submitter);
								}
							}
						}
					}
				}
			}
			CommandUtils.Log("");

			// Print out all the aggregates
			string[] AggregateNames = AggregateNameToNodes.Keys.OrderBy(x => x).ToArray();
			if(AggregateNames.Length > 0)
			{
				CommandUtils.Log("Aggregates:");
				foreach(string AggregateName in AggregateNames)
				{
					CommandUtils.Log("    {0}", AggregateName);
				}
				CommandUtils.Log("");
			}
		}
	}
}
