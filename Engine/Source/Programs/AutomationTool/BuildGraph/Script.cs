// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml;
using System.Xml.Linq;
using UnrealBuildTool;
using AutomationTool;
using System.Reflection;
using System.Diagnostics;
using System.Xml.Schema;
using System.Text.RegularExpressions;
using Tools.DotNETCommon;

namespace AutomationTool
{
	/// <summary>
	/// Implementation of XmlDocument which preserves line numbers for its elements
	/// </summary>
	class ScriptDocument : XmlDocument
	{
		/// <summary>
		/// The file being read
		/// </summary>
		FileReference File;

		/// <summary>
		/// Interface to the LineInfo on the active XmlReader
		/// </summary>
		IXmlLineInfo LineInfo;

		/// <summary>
		/// Set to true if the reader encounters an error
		/// </summary>
		bool bHasErrors;

		/// <summary>
		/// Private constructor. Use ScriptDocument.Load to read an XML document.
		/// </summary>
		ScriptDocument(FileReference InFile)
		{
			File = InFile;
		}

		/// <summary>
		/// Overrides XmlDocument.CreateElement() to construct ScriptElements rather than XmlElements
		/// </summary>
		public override XmlElement CreateElement(string Prefix, string LocalName, string NamespaceUri)
		{
			return new ScriptElement(File, LineInfo.LineNumber, Prefix, LocalName, NamespaceUri, this);
		}

		/// <summary>
		/// Loads a script document from the given file
		/// </summary>
		/// <param name="File">The file to load</param>
		/// <param name="Schema">The schema to validate against</param>
		/// <param name="OutDocument">If successful, the document that was read</param>
		/// <returns>True if the document could be read, false otherwise</returns>
		public static bool TryRead(FileReference File, ScriptSchema Schema, out ScriptDocument OutDocument)
		{
			ScriptDocument Document = new ScriptDocument(File);

			XmlReaderSettings Settings = new XmlReaderSettings();
			Settings.Schemas.Add(Schema.CompiledSchema);
			Settings.ValidationType = ValidationType.Schema;
			Settings.ValidationEventHandler += Document.ValidationEvent;

			using (XmlReader Reader = XmlReader.Create(File.FullName, Settings))
			{
				// Read the document
				Document.LineInfo = (IXmlLineInfo)Reader;
				try
				{
					Document.Load(Reader);
				}
				catch (XmlException Ex)
				{
					if (!Document.bHasErrors)
					{
						CommandUtils.LogError("{0}", Ex.Message);
						Document.bHasErrors = true;
					}
				}

				// If we hit any errors while parsing
				if (Document.bHasErrors)
				{
					OutDocument = null;
					return false;
				}

				// Check that the root element is valid. If not, we didn't actually validate against the schema.
				if (Document.DocumentElement.Name != ScriptSchema.RootElementName)
				{
					CommandUtils.LogError("Script does not have a root element called '{0}'", ScriptSchema.RootElementName);
					OutDocument = null;
					return false;
				}
				if (Document.DocumentElement.NamespaceURI != ScriptSchema.NamespaceURI)
				{
					CommandUtils.LogError("Script root element is not in the '{0}' namespace (add the xmlns=\"{0}\" attribute)", ScriptSchema.NamespaceURI);
					OutDocument = null;
					return false;
				}
			}

			OutDocument = Document;
			return true;
		}

		/// <summary>
		/// Callback for validation errors in the document
		/// </summary>
		/// <param name="Sender">Standard argument for ValidationEventHandler</param>
		/// <param name="Args">Standard argument for ValidationEventHandler</param>
		void ValidationEvent(object Sender, ValidationEventArgs Args)
		{
			if (Args.Severity == XmlSeverityType.Warning)
			{
				CommandUtils.LogWarning("{0}({1}): {2}", File.FullName, Args.Exception.LineNumber, Args.Message);
			}
			else
			{
				CommandUtils.LogError("{0}({1}): {2}", File.FullName, Args.Exception.LineNumber, Args.Message);
				bHasErrors = true;
			}
		}
	}

	/// <summary>
	/// Implementation of XmlElement which preserves line numbers
	/// </summary>
	class ScriptElement : XmlElement
	{
		/// <summary>
		/// The file containing this element
		/// </summary>
		public readonly FileReference File;

		/// <summary>
		/// The line number containing this element
		/// </summary>
		public readonly int LineNumber;

		/// <summary>
		/// Constructor
		/// </summary>
		public ScriptElement(FileReference InFile, int InLineNumber, string Prefix, string LocalName, string NamespaceUri, ScriptDocument Document)
			: base(Prefix, LocalName, NamespaceUri, Document)
		{
			File = InFile;
			LineNumber = InLineNumber;
		}
	}

	/// <summary>
	/// Reader for build graph definitions. Instanced to contain temporary state; public interface is through ScriptReader.TryRead().
	/// </summary>
	class ScriptReader
	{
		/// <summary>
		/// The current graph
		/// </summary>
		Graph Graph = new Graph();

		/// <summary>
		/// List of property name to value lookups. Modifications to properties are scoped to nodes and agents. EnterScope() pushes an empty dictionary onto the end of this list, and LeaveScope() removes one. 
		/// ExpandProperties() searches from last to first lookup when trying to resolve a property name, and takes the first it finds.
		/// </summary>
		List<Dictionary<string, string>> ScopedProperties = new List<Dictionary<string, string>>();

		/// <summary>
		/// When declaring a property in a nested scope, we enter its name into a set for each parent scope which prevents redeclaration in an OUTER scope later. Subsequent NESTED scopes can redeclare it.
		/// The former is likely a coding error, since it implies that the scope of the variable was meant to be further out, whereas the latter is common for temporary and loop variables.
		/// </summary>
		List<HashSet<string>> ShadowProperties = new List<HashSet<string>>();

		/// <summary>
		/// Schema for the script
		/// </summary>
		ScriptSchema Schema;

		/// <summary>
		/// The number of errors encountered during processing so far
		/// </summary>
		int NumErrors;

		/// <summary>
		/// Private constructor. Use ScriptReader.TryRead() to read a script file.
		/// </summary>
		/// <param name="DefaultProperties">Default properties available to the script</param>
		/// <param name="Schema">Schema for the script</param>
		private ScriptReader(IDictionary<string, string> DefaultProperties, ScriptSchema Schema)
		{
			this.Schema = Schema;

			EnterScope();

			foreach(KeyValuePair<string, string> Pair in DefaultProperties)
			{
				ScopedProperties[ScopedProperties.Count - 1].Add(Pair.Key, Pair.Value);
			}
		}

		/// <summary>
		/// Try to read a script file from the given file.
		/// </summary>
		/// <param name="File">File to read from</param>
		/// <param name="Arguments">Arguments passed in to the graph on the command line</param>
		/// <param name="DefaultProperties">Default properties available to the script</param>
		/// <param name="Schema">Schema for the script</param>
		/// <param name="Graph">If successful, the graph constructed from the given script</param>
		/// <returns>True if the graph was read, false if there were errors</returns>
		public static bool TryRead(FileReference File, Dictionary<string, string> Arguments, Dictionary<string, string> DefaultProperties, ScriptSchema Schema, out Graph Graph)
		{
			// Check the file exists before doing anything.
			if (!FileReference.Exists(File))
			{
				CommandUtils.LogError("Cannot open '{0}'", File.FullName);
				Graph = null;
				return false;
			}

			// Read the file and build the graph
			ScriptReader Reader = new ScriptReader(DefaultProperties, Schema);
			if (!Reader.TryRead(File, Arguments) || Reader.NumErrors > 0)
			{
				Graph = null;
				return false;
			}

			// Make sure all the arguments were valid
			bool bInvalidArgument = false;
			foreach(string InvalidArgumentName in Arguments.Keys.Except(Reader.Graph.Options.Select(x => x.Name), StringComparer.InvariantCultureIgnoreCase))
			{
				CommandUtils.LogWarning("Unknown argument '{0}' for '{1}'", InvalidArgumentName, File.FullName);
				bInvalidArgument = true;
			}
			if(bInvalidArgument)
			{
				Graph = null;
				return false;
			}

			// Return the constructed graph
			Graph = Reader.Graph;
			return true;
		}

		/// <summary>
		/// Read the script from the given file
		/// </summary>
		/// <param name="File">File to read from</param>
		/// <param name="Arguments">Arguments passed in to the graph on the command line</param>
		bool TryRead(FileReference File, Dictionary<string, string> Arguments)
		{
			// Read the document and validate it against the schema
			ScriptDocument Document;
			if (!ScriptDocument.TryRead(File, Schema, out Document))
			{
				NumErrors++;
				return false;
			}

			// Read the root BuildGraph element
			ReadGraphBody(Document.DocumentElement, File.Directory, Arguments);
			return true;
		}

		/// <summary>
		/// Reads the contents of a graph
		/// </summary>
		/// <param name="Element">The parent element to read from</param>
		/// <param name="BaseDirectory">Base directory to resolve includes against</param>
		/// <param name="Arguments">Arguments passed in to the graph on the command line</param>
		void ReadGraphBody(XmlElement Element, DirectoryReference BaseDirectory, Dictionary<string, string> Arguments)
		{
			foreach (ScriptElement ChildElement in Element.ChildNodes.OfType<ScriptElement>())
			{
				switch (ChildElement.Name)
				{
					case "Include":
						ReadInclude(ChildElement, BaseDirectory, Arguments);
						break;
					case "Option":
						ReadOption(ChildElement, Arguments);
						break;
					case "Property":
						ReadProperty(ChildElement);
						break;
					case "EnvVar":
						ReadEnvVar(ChildElement);
						break;
					case "Agent":
						ReadAgent(ChildElement, null);
						break;
					case "Aggregate":
						ReadAggregate(ChildElement);
						break;
					case "Report":
						ReadReport(ChildElement);
						break;
					case "Badge":
						ReadBadge(ChildElement);
						break;
					case "Notify":
						ReadNotifier(ChildElement);
						break;
					case "Trigger":
						ReadTrigger(ChildElement);
						break;
					case "Warning":
						ReadDiagnostic(ChildElement, LogEventType.Warning, null, null, null);
						break;
					case "Error":
						ReadDiagnostic(ChildElement, LogEventType.Error, null, null, null);
						break;
					case "Do":
						ReadBlock(ChildElement, x => ReadGraphBody(x, BaseDirectory, Arguments));
						break;
					case "Switch":
						ReadSwitch(ChildElement, x => ReadGraphBody(x, BaseDirectory, Arguments));
						break;
					case "ForEach":
						ReadForEach(ChildElement, x => ReadGraphBody(x, BaseDirectory, Arguments));
						break;
					default:
						LogError(ChildElement, "Invalid element '{0}'", ChildElement.Name);
						break;
				}
			}
		}

		/// <summary>
		/// Handles validation messages from validating the document against its schema
		/// </summary>
		/// <param name="Sender">The source of the event</param>
		/// <param name="Args">Event arguments</param>
		void ValidationHandler(object Sender, ValidationEventArgs Args)
		{
			if (Args.Severity == XmlSeverityType.Warning)
			{
				CommandUtils.LogWarning("Script: {0}", Args.Message);
			}
			else
			{
				CommandUtils.LogError("Script: {0}", Args.Message);
				NumErrors++;
			}
		}

		/// <summary>
		/// Push a new property scope onto the stack
		/// </summary>
		void EnterScope()
		{
			ScopedProperties.Add(new Dictionary<string, string>(StringComparer.InvariantCultureIgnoreCase));
			ShadowProperties.Add(new HashSet<string>(StringComparer.InvariantCultureIgnoreCase));
		}

		/// <summary>
		/// Pop a property scope from the stack
		/// </summary>
		void LeaveScope()
		{
			ScopedProperties.RemoveAt(ScopedProperties.Count - 1);
			ShadowProperties.RemoveAt(ShadowProperties.Count - 1);
		}

		/// <summary>
		/// Sets a property value in the current scope
		/// </summary>
		/// <param name="Element">Element containing the property assignment. Used for error messages if the property is shadowed in another scope.</param>
		/// <param name="Name">Name of the property</param>
		/// <param name="Value">Value for the property</param>
		void SetPropertyValue(ScriptElement Element, string Name, string Value)
		{
			// Find the scope containing this property, defaulting to the current scope
			int ScopeIdx = 0;
			while(ScopeIdx < ScopedProperties.Count - 1 && !ScopedProperties[ScopeIdx].ContainsKey(Name))
			{
				ScopeIdx++;
			}

			// Make sure this property name was not already used in a child scope; it likely indicates an error.
			if(ShadowProperties[ScopeIdx].Contains(Name))
			{
				LogError(Element, "Property '{0}' was already used in a child scope. Move this definition before the previous usage if they are intended to share scope, or use a different name.", Name);
			}
			else
			{
				// Make sure it's added to the shadow property list for every parent scope
				for(int Idx = 0; Idx < ScopeIdx; Idx++)
				{
					ShadowProperties[Idx].Add(Name);
				}
				ScopedProperties[ScopeIdx][Name] = Value;
			}
		}

		/// <summary>
		/// Tries to get the value of a property
		/// </summary>
		/// <param name="Name">Name of the property</param>
		/// <param name="Value">On success, contains the value of the property. Set to null otherwise.</param>
		/// <returns>True if the property was found, false otherwise</returns>
		bool TryGetPropertyValue(string Name, out string Value)
		{
			// Check each scope for the property
			for (int ScopeIdx = ScopedProperties.Count - 1; ScopeIdx >= 0; ScopeIdx--)
			{
				string ScopeValue;
				if (ScopedProperties[ScopeIdx].TryGetValue(Name, out ScopeValue))
				{
					Value = ScopeValue;
					return true;
				}
			}

			// If we didn't find it, return false.
			Value = null;
			return false;
		}

		/// <summary>
		/// Reads the definition for a trigger.
		/// </summary>
		/// <param name="Element">Xml element to read the definition from</param>
		void ReadTrigger(ScriptElement Element)
		{
			string[] QualifiedName;
			if (EvaluateCondition(Element) && TryReadQualifiedObjectName(Element, out QualifiedName))
			{
				// Validate all the parent triggers
				ManualTrigger ParentTrigger = null;
				for (int Idx = 0; Idx < QualifiedName.Length - 1; Idx++)
				{
					ManualTrigger NextTrigger;
					if (!Graph.NameToTrigger.TryGetValue(QualifiedName[Idx], out NextTrigger))
					{
						LogError(Element, "Unknown trigger '{0}'", QualifiedName[Idx]);
						return;
					}
					if (NextTrigger.Parent != ParentTrigger)
					{
						LogError(Element, "Qualified name of trigger '{0}' is '{1}'", NextTrigger.Name, NextTrigger.QualifiedName);
						return;
					}
					ParentTrigger = NextTrigger;
				}

				// Get the name of the new trigger
				string Name = QualifiedName[QualifiedName.Length - 1];

				// Create the new trigger
				ManualTrigger Trigger;
				if (!Graph.NameToTrigger.TryGetValue(Name, out Trigger))
				{
					Trigger = new ManualTrigger(ParentTrigger, Name);
					Graph.NameToTrigger.Add(Name, Trigger);
				}
				else if (Trigger.Parent != ParentTrigger)
				{
					LogError(Element, "Conflicting parent for '{0}' - previously declared as '{1}', now '{2}'", Name, Trigger.QualifiedName, new ManualTrigger(ParentTrigger, Name).QualifiedName);
					return;
				}

				// Read the child elements
				ReadTriggerBody(Element, Trigger);
			}
		}

		/// <summary>
		/// Reads the body of a trigger element
		/// </summary>
		/// <param name="Element">Xml element to read the definition from</param>
		/// <param name="Trigger">The enclosing trigger definition</param>
		void ReadTriggerBody(XmlElement Element, ManualTrigger Trigger)
		{
			EnterScope();
			foreach (ScriptElement ChildElement in Element.ChildNodes.OfType<ScriptElement>())
			{
				switch (ChildElement.Name)
				{
					case "Property":
						ReadProperty(ChildElement);
						break;
					case "Agent":
						ReadAgent(ChildElement, Trigger);
						break;
					case "Aggregate":
						ReadAggregate(ChildElement);
						break;
					case "Notifier":
						ReadNotifier(ChildElement);
						break;
					case "Warning":
						ReadDiagnostic(ChildElement, LogEventType.Warning, null, null, Trigger);
						break;
					case "Error":
						ReadDiagnostic(ChildElement, LogEventType.Error, null, null, Trigger);
						break;
					case "Do":
						ReadBlock(ChildElement, x => ReadTriggerBody(x, Trigger));
						break;
					case "Switch":
						ReadSwitch(ChildElement, x => ReadTriggerBody(x, Trigger));
						break;
					case "ForEach":
						ReadForEach(ChildElement, x => ReadTriggerBody(x, Trigger));
						break;
					default:
						LogError(ChildElement, "Invalid element '{0}'", ChildElement.Name);
						break;
				}
			}
			LeaveScope();
		}

		/// <summary>
		/// Read an include directive, and the contents of the target file
		/// </summary>
		/// <param name="Element">Xml element to read the definition from</param>
		/// <param name="BaseDir">Base directory to resolve relative include paths from </param>
		/// <param name="Arguments">Arguments passed in to the graph on the command line</param>
		void ReadInclude(ScriptElement Element, DirectoryReference BaseDir, Dictionary<string, string> Arguments)
		{
			if (EvaluateCondition(Element))
			{
				FileReference Script = FileReference.Combine(BaseDir, Element.GetAttribute("Script"));
				if (!FileReference.Exists(Script))
				{
					LogError(Element, "Cannot find included script '{0}'", Script.FullName);
				}
				else
				{
					TryRead(Script, Arguments);
				}
			}
		}

		/// <summary>
		/// Reads the definition of a graph option; a parameter which can be set by the user on the command-line or via an environment variable.
		/// </summary>
		/// <param name="Element">Xml element to read the definition from</param>
		/// <param name="Arguments">Arguments passed in to the graph on the command line</param>
		void ReadOption(ScriptElement Element, IDictionary<string, string> Arguments)
		{
			if (EvaluateCondition(Element))
			{
				string Name = ReadAttribute(Element, "Name");
				if (ValidateName(Element, Name))
				{
					// Make sure we're at global scope
					if(ScopedProperties.Count > 1)
					{
						throw new AutomationException("Incorrect scope depth for reading option settings");
					}

					// Check if the property already exists. If it does, we don't need to register it as an option.
					string ExistingValue;
					if(TryGetPropertyValue(Name, out ExistingValue))
					{
						// If there's a restriction on this definition, check it matches
						string Restrict = ReadAttribute(Element, "Restrict");
						if(!String.IsNullOrEmpty(Restrict) && !Regex.IsMatch(ExistingValue, "^" + Restrict + "$", RegexOptions.IgnoreCase))
						{
							LogError(Element, "'{0} is already set to '{1}', which does not match the given restriction ('{2}')", Name, ExistingValue, Restrict);
						}
					}
					else
					{
						// Create a new option object to store the settings
						string Description = ReadAttribute(Element, "Description");
						string DefaultValue = ReadAttribute(Element, "DefaultValue");
						GraphOption Option = new GraphOption(Name, Description, DefaultValue);
						Graph.Options.Add(Option);

						// Get the value of this property
						string Value;
						if(!Arguments.TryGetValue(Name, out Value))
						{
							Value = Option.DefaultValue;
						}
						SetPropertyValue(Element, Name, Value);

						// If there's a restriction on it, check it's valid
						string Restrict = ReadAttribute(Element, "Restrict");
						if(!String.IsNullOrEmpty(Restrict))
						{
							string Pattern = "^" + Restrict + "$";
							if(!Regex.IsMatch(Value, Pattern, RegexOptions.IgnoreCase))
							{
								LogError(Element, "'{0}' is not a valid value for '{1}' (required: '{2}')", Value, Name, Restrict);
							}
							if(Option.DefaultValue != Value && !Regex.IsMatch(Option.DefaultValue, Pattern, RegexOptions.IgnoreCase))
							{
								LogError(Element, "Default value '{0}' is not valid for '{1}' (required: '{2}')", Option.DefaultValue, Name, Restrict);
							}
						}
					}
				}
			}
		}

		/// <summary>
		/// Reads a property assignment.
		/// </summary>
		/// <param name="Element">Xml element to read the definition from</param>
		void ReadProperty(ScriptElement Element)
		{
			if (EvaluateCondition(Element))
			{
				string Name = ReadAttribute(Element, "Name");
				if (ValidateName(Element, Name))
				{
					string Value = ReadAttribute(Element, "Value");
					if(Element.HasChildNodes)
					{
						// Read the element content, and append each line to the value as a semicolon delimited list
						StringBuilder Builder = new StringBuilder(Value);
						foreach(string Line in Element.InnerText.Split('\n'))
						{
							string TrimLine = ExpandProperties(Element, Line.Trim());
							if(TrimLine.Length > 0)
							{
								if(Builder.Length > 0)
								{
									Builder.Append(";");
								}
								Builder.Append(TrimLine);
							}
						}
						Value = Builder.ToString();
					}
					SetPropertyValue(Element, Name, Value);
				}
			}
		}

		/// <summary>
		/// Reads a property assignment from an environment variable.
		/// </summary>
		/// <param name="Element">Xml element to read the definition from</param>
		void ReadEnvVar(ScriptElement Element)
		{
			if (EvaluateCondition(Element))
			{
				string Name = ReadAttribute(Element, "Name");
				if (ValidateName(Element, Name))
				{
					string Value = Environment.GetEnvironmentVariable(Name) ?? "";
					SetPropertyValue(Element, Name, Value);
				}
			}
		}

		/// <summary>
		/// Reads the definition for an agent.
		/// </summary>
		/// <param name="Element">Xml element to read the definition from</param>
		/// <param name="Trigger">The controlling trigger for nodes in this agent</param>
		void ReadAgent(ScriptElement Element, ManualTrigger Trigger)
		{
			string Name;
			if (EvaluateCondition(Element) && TryReadObjectName(Element, out Name))
			{
				// Read the valid agent types. This may be omitted if we're continuing an existing agent.
				string[] Types = ReadListAttribute(Element, "Type");

				// Create the agent object, or continue an existing one
				Agent Agent;
				if (Graph.NameToAgent.TryGetValue(Name, out Agent))
				{
					if (Types.Length > 0 && Agent.PossibleTypes.Length > 0)
					{
						string[] NewTypes = Agent.PossibleTypes.Intersect(Types, StringComparer.InvariantCultureIgnoreCase).ToArray();
						if (NewTypes.Length == 0)
						{
							LogError(Element, "No common agent types with previous agent definition");
						}
						Agent.PossibleTypes = NewTypes;
					}
				}
				else
				{
					if (Types.Length == 0)
					{
						LogError(Element, "Missing type for agent '{0}'", Name);
					}
					Agent = new Agent(Name, Types);
					Graph.NameToAgent.Add(Name, Agent);
					Graph.Agents.Add(Agent);
				}

				// Process all the child elements.
				ReadAgentBody(Element, Agent, Trigger);
			}
		}

		/// <summary>
		/// Read the contents of an agent definition
		/// </summary>
		/// <param name="Element">Xml element to read the definition from</param>
		/// <param name="ParentAgent">The agent to contain the definition</param>
		/// <param name="ControllingTrigger">The enclosing trigger</param>
		void ReadAgentBody(ScriptElement Element, Agent ParentAgent, ManualTrigger ControllingTrigger)
		{
			EnterScope();
			foreach (ScriptElement ChildElement in Element.ChildNodes.OfType<ScriptElement>())
			{
				switch (ChildElement.Name)
				{
					case "Property":
						ReadProperty(ChildElement);
						break;
					case "Node":
						ReadNode(ChildElement, ParentAgent, ControllingTrigger);
						break;
					case "Aggregate":
						ReadAggregate(ChildElement);
						break;
					case "Warning":
						ReadDiagnostic(ChildElement, LogEventType.Warning, null, ParentAgent, ControllingTrigger);
						break;
					case "Error":
						ReadDiagnostic(ChildElement, LogEventType.Error, null, ParentAgent, ControllingTrigger);
						break;
					case "Do":
						ReadBlock(ChildElement, x => ReadAgentBody(x, ParentAgent, ControllingTrigger));
						break;
					case "Switch":
						ReadSwitch(ChildElement, x => ReadAgentBody(x, ParentAgent, ControllingTrigger));
						break;
					case "ForEach":
						ReadForEach(ChildElement, x => ReadAgentBody(x, ParentAgent, ControllingTrigger));
						break;
					default:
						LogError(ChildElement, "Unexpected element type '{0}'", ChildElement.Name);
						break;
				}
			}
			LeaveScope();
		}

		/// <summary>
		/// Reads the definition for an aggregate
		/// </summary>
		/// <param name="Element">Xml element to read the definition from</param>
		void ReadAggregate(ScriptElement Element)
		{
			string Name;
			if (EvaluateCondition(Element) && TryReadObjectName(Element, out Name) && CheckNameIsUnique(Element, Name))
			{
				string[] RequiredNames = ReadListAttribute(Element, "Requires");
				Graph.AggregateNameToNodes.Add(Name, ResolveReferences(Element, RequiredNames).ToArray());
			}
		}

		/// <summary>
		/// Reads the definition for a report
		/// </summary>
		/// <param name="Element">Xml element to read the definition from</param>
		void ReadReport(ScriptElement Element)
		{
			string Name;
			if (EvaluateCondition(Element) && TryReadObjectName(Element, out Name) && CheckNameIsUnique(Element, Name))
			{
				string[] RequiredNames = ReadListAttribute(Element, "Requires");

				Report NewReport = new Report(Name);
				foreach (Node ReferencedNode in ResolveReferences(Element, RequiredNames))
				{
					NewReport.Nodes.Add(ReferencedNode);
					NewReport.Nodes.UnionWith(ReferencedNode.OrderDependencies);
				}
				Graph.NameToReport.Add(Name, NewReport);
			}
		}

		/// <summary>
		/// Reads the definition for a badge
		/// </summary>
		/// <param name="Element">Xml element to read the definition from</param>
		void ReadBadge(ScriptElement Element)
		{
			string Name;
			if (EvaluateCondition(Element) && TryReadObjectName(Element, out Name))
			{
				string[] RequiredNames = ReadListAttribute(Element, "Requires");
				string Project = ReadAttribute(Element, "Project");
				int Change = ReadIntegerAttribute(Element, "Change", 0);

				Badge NewBadge = new Badge(Name, Project, Change);
				foreach (Node ReferencedNode in ResolveReferences(Element, RequiredNames))
				{
					NewBadge.Nodes.Add(ReferencedNode);
				}
				Graph.Badges.Add(NewBadge);
			}
		}

		/// <summary>
		/// Reads the definition for a node, and adds it to the given agent
		/// </summary>
		/// <param name="Element">Xml element to read the definition from</param>
		/// <param name="ParentAgent">Agent for the node to be added to</param>
		/// <param name="ControllingTrigger">The controlling trigger for this node</param>
		void ReadNode(ScriptElement Element, Agent ParentAgent, ManualTrigger ControllingTrigger)
		{
			string Name;
			if (EvaluateCondition(Element) && TryReadObjectName(Element, out Name))
			{
				string[] RequiresNames = ReadListAttribute(Element, "Requires");
				string[] ProducesNames = ReadListAttribute(Element, "Produces");
				string[] AfterNames = ReadListAttribute(Element, "After");
				string[] TokenFileNames = ReadListAttribute(Element, "Token");
				bool bNotifyOnWarnings = ReadBooleanAttribute(Element, "NotifyOnWarnings", true);

				// Resolve all the inputs we depend on
				HashSet<NodeOutput> Inputs = ResolveInputReferences(Element, RequiresNames);

				// Gather up all the input dependencies, and check they're all upstream of the current node
				HashSet<Node> InputDependencies = new HashSet<Node>();
				foreach (Node InputDependency in Inputs.Select(x => x.ProducingNode).Distinct())
				{
					if (InputDependency.ControllingTrigger != null && InputDependency.ControllingTrigger != ControllingTrigger && !InputDependency.ControllingTrigger.IsUpstreamFrom(ControllingTrigger))
					{
						LogError(Element, "'{0}' is dependent on '{1}', which is behind a different controlling trigger ({2})", Name, InputDependency.Name, InputDependency.ControllingTrigger.QualifiedName);
					}
					else
					{
						InputDependencies.Add(InputDependency);
					}
				}

				// Remove all the lock names from the list of required names
				HashSet<FileReference> RequiredTokens = new HashSet<FileReference>(TokenFileNames.Select(x => FileReference.Combine(CommandUtils.RootDirectory, x)));

				// Recursively include all their dependencies too
				foreach (Node InputDependency in InputDependencies.ToArray())
				{
					RequiredTokens.UnionWith(InputDependency.RequiredTokens);
					InputDependencies.UnionWith(InputDependency.InputDependencies);
				}

				// Validate all the outputs
				List<string> ValidOutputNames = new List<string>();
				foreach (string ProducesName in ProducesNames)
				{
					NodeOutput ExistingOutput;
					if(Graph.TagNameToNodeOutput.TryGetValue(ProducesName, out ExistingOutput))
					{
						LogError(Element, "Output tag '{0}' is already generated by node '{1}'", ProducesName, ExistingOutput.ProducingNode.Name);
					}
					else if(Graph.LocalTagNames.Contains(ProducesName))
					{
						LogError(Element, "Output tag '{0}' is used elsewhere as a local tag name", ProducesName);
					}
					else if(!ProducesName.StartsWith("#"))
					{
						LogError(Element, "Output tag names must begin with a '#' character ('{0}')", ProducesName);
					}
					else
					{
						ValidOutputNames.Add(ProducesName);
					}
				}

				// Gather up all the order dependencies
				HashSet<Node> OrderDependencies = new HashSet<Node>(InputDependencies);
				OrderDependencies.UnionWith(ResolveReferences(Element, AfterNames));

				// Recursively include all their order dependencies too
				foreach (Node OrderDependency in OrderDependencies.ToArray())
				{
					OrderDependencies.UnionWith(OrderDependency.OrderDependencies);
				}

				// Check that we're not dependent on anything completing that is declared after the initial declaration of this agent.
				int AgentIdx = Graph.Agents.IndexOf(ParentAgent);
				for (int Idx = AgentIdx + 1; Idx < Graph.Agents.Count; Idx++)
				{
					foreach (Node Node in Graph.Agents[Idx].Nodes.Where(x => OrderDependencies.Contains(x)))
					{
						LogError(Element, "Node '{0}' has a dependency on '{1}', which was declared after the initial definition of '{2}'.", Name, Node.Name, ParentAgent.Name);
					}
				}

				// Construct and register the node
				if (CheckNameIsUnique(Element, Name))
				{
					// Add it to the node lookup
					Node NewNode = new Node(Name, Inputs.ToArray(), ValidOutputNames.ToArray(), InputDependencies.ToArray(), OrderDependencies.ToArray(), ControllingTrigger, RequiredTokens.ToArray());
					NewNode.bNotifyOnWarnings = bNotifyOnWarnings;
					Graph.NameToNode.Add(Name, NewNode);

					// Register all the output tags in the global name table.
					foreach(NodeOutput Output in NewNode.Outputs)
					{
						Graph.TagNameToNodeOutput.Add(Output.TagName, Output);
					}

					// Add all the tasks
					ReadNodeBody(Element, NewNode, ParentAgent, ControllingTrigger);

					// Add it to the current agent
					ParentAgent.Nodes.Add(NewNode);
				}
			}
		}

		/// <summary>
		/// Reads the contents of a node element
		/// </summary>
		/// <param name="Element">Xml element to read the definition from</param>
		/// <param name="NewNode">The new node that has been created</param>
		/// <param name="ParentAgent">Agent for the node to be added to</param>
		/// <param name="ControllingTrigger">The controlling trigger for this node</param>
		void ReadNodeBody(XmlElement Element, Node NewNode, Agent ParentAgent, ManualTrigger ControllingTrigger)
		{
			EnterScope();
			foreach (ScriptElement ChildElement in Element.ChildNodes.OfType<ScriptElement>())
			{
				switch (ChildElement.Name)
				{
					case "Property":
						ReadProperty(ChildElement);
						break;
					case "Warning":
						ReadDiagnostic(ChildElement, LogEventType.Warning, NewNode, ParentAgent, ControllingTrigger);
						break;
					case "Error":
						ReadDiagnostic(ChildElement, LogEventType.Error, NewNode, ParentAgent, ControllingTrigger);
						break;
					case "Do":
						ReadBlock(ChildElement, x => ReadNodeBody(x, NewNode, ParentAgent, ControllingTrigger));
						break;
					case "Switch":
						ReadSwitch(ChildElement, x => ReadNodeBody(x, NewNode, ParentAgent, ControllingTrigger));
						break;
					case "ForEach":
						ReadForEach(ChildElement, x => ReadNodeBody(x, NewNode, ParentAgent, ControllingTrigger));
						break;
					default:
						ReadTask(ChildElement, NewNode);
						break;
				}
			}
			LeaveScope();
		}

		/// <summary>
		/// Reads a block element
		/// </summary>
		/// <param name="Element">Xml element to read the definition from</param>
		/// <param name="ReadContents">Delegate to read the contents of the element, if the condition evaluates to true</param>
		void ReadBlock(ScriptElement Element, Action<ScriptElement> ReadContents)
		{
			if (EvaluateCondition(Element))
			{
				ReadContents(Element);
			}
		}

		/// <summary>
		/// Reads a "Switch" element 
		/// </summary>
		/// <param name="Element">Xml element to read the definition from</param>
		/// <param name="ReadContents">Delegate to read the contents of the element, if the condition evaluates to true</param>
		void ReadSwitch(ScriptElement Element, Action<ScriptElement> ReadContents)
		{
			foreach (ScriptElement ChildElement in Element.ChildNodes.OfType<ScriptElement>())
			{
				if (ChildElement.Name == "Default" || EvaluateCondition(ChildElement))
				{
					ReadContents(ChildElement);
					break;
				}
			}
		}

		/// <summary>
		/// Reads a "ForEach" element 
		/// </summary>
		/// <param name="Element">Xml element to read the definition from</param>
		/// <param name="ReadContents">Delegate to read the contents of the element, if the condition evaluates to true</param>
		void ReadForEach(ScriptElement Element, Action<ScriptElement> ReadContents)
		{
			EnterScope();
			if(EvaluateCondition(Element))
			{
				string Name = ReadAttribute(Element, "Name");
				if (ValidateName(Element, Name))
				{
					if(ScopedProperties.Any(x => x.ContainsKey(Name)))
					{
						LogError(Element, "Loop variable '{0}' already exists as a local property in an outer scope", Name);
					}
					else
					{
						// Loop through all the values
						string[] Values = ReadListAttribute(Element, "Values");
						foreach(string Value in Values)
						{
							ScopedProperties[ScopedProperties.Count - 1][Name] = Value;
							ReadContents(Element);
						}
					}
				}
			}
			LeaveScope();
		}

		/// <summary>
		/// Reads a task definition from the given element, and add it to the given list
		/// </summary>
		/// <param name="Element">Xml element to read the definition from</param>
		/// <param name="ParentNode">The node which owns this task</param>
		void ReadTask(ScriptElement Element, Node ParentNode)
		{
			if (EvaluateCondition(Element))
			{
				// Get the reflection info for this element
				ScriptTask Task;
				if (!Schema.TryGetTask(Element.Name, out Task))
				{
					LogError(Element, "Unknown task '{0}'", Element.Name);
					return;
				}

				// Check all the required parameters are present
				bool bHasRequiredAttributes = true;
				foreach (ScriptTaskParameter Parameter in Task.NameToParameter.Values)
				{
					if (!Parameter.bOptional && !Element.HasAttribute(Parameter.Name))
					{
						LogError(Element, "Missing required attribute - {0}", Parameter.Name);
						bHasRequiredAttributes = false;
					}
				}

				// Read all the attributes into a parameters object for this task
				object ParametersObject = Activator.CreateInstance(Task.ParametersClass);
				foreach (XmlAttribute Attribute in Element.Attributes)
				{
					if (String.Compare(Attribute.Name, "If", StringComparison.InvariantCultureIgnoreCase) != 0)
					{
						// Get the field that this attribute should be written to in the parameters object
						ScriptTaskParameter Parameter;
						if (!Task.NameToParameter.TryGetValue(Attribute.Name, out Parameter))
						{
							LogError(Element, "Unknown attribute '{0}'", Attribute.Name);
							continue;
						}

						// Expand variables in the value
						string ExpandedValue = ExpandProperties(Element, Attribute.Value);

						// Parse it and assign it to the parameters object
						object Value;
						if (Parameter.ValueType.IsEnum)
						{
							Value = Enum.Parse(Parameter.ValueType, ExpandedValue);
						}
						else if (Parameter.ValueType == typeof(Boolean))
						{
							Value = Condition.Evaluate(ExpandedValue);
						}
						else
						{
							Value = Convert.ChangeType(ExpandedValue, Parameter.ValueType);
						}
						Parameter.FieldInfo.SetValue(ParametersObject, Value);
					}
				}

				// Construct the task
				if (bHasRequiredAttributes)
				{
					// Add it to the list
					CustomTask NewTask = (CustomTask)Activator.CreateInstance(Task.TaskClass, ParametersObject);
					ParentNode.Tasks.Add(NewTask);

					// Set up the source location for diagnostics
					NewTask.SourceLocation = Tuple.Create(Element.File, Element.LineNumber);

					// Make sure all the read tags are local or listed as a dependency
					foreach(string ReadTagName in NewTask.FindConsumedTagNames())
					{
						NodeOutput Output;
						if(!Graph.TagNameToNodeOutput.TryGetValue(ReadTagName, out Output))
						{
							Graph.LocalTagNames.Add(ReadTagName);
						}
						else if(Output != null && Output.ProducingNode != ParentNode && !ParentNode.Inputs.Contains(Output))
						{
							LogError(Element, "The tag '{0}' is not a dependency of node '{1}'", ReadTagName, ParentNode.Name);
						}
					}

					// Make sure all the written tags are local or listed as an output
					foreach(string ModifiedTagName in NewTask.FindProducedTagNames())
					{
						NodeOutput Output;
						if(!Graph.TagNameToNodeOutput.TryGetValue(ModifiedTagName, out Output))
						{
							Graph.LocalTagNames.Add(ModifiedTagName);
						}
						else if(Output != null && !ParentNode.Outputs.Contains(Output))
						{
							LogError(Element, "The tag '{0}' is created by '{1}', and cannot be modified downstream", Output.TagName, Output.ProducingNode.Name);
						}
					}
				}
			}
		}

		/// <summary>
		/// Reads the definition for an email notifier
		/// </summary>
		/// <param name="Element">Xml element to read the definition from</param>
		void ReadNotifier(ScriptElement Element)
		{
			if (EvaluateCondition(Element))
			{
				string[] TargetNames = ReadListAttribute(Element, "Targets");
				string[] ExceptNames = ReadListAttribute(Element, "Except");
				string[] IndividualNodeNames = ReadListAttribute(Element, "Nodes");
				string[] TriggerNames = ReadListAttribute(Element, "Triggers");
				string[] ReportNames = ReadListAttribute(Element, "Reports");
				string[] Users = ReadListAttribute(Element, "Users");
				string[] Submitters = ReadListAttribute(Element, "Submitters");
				bool? bWarnings = Element.HasAttribute("Warnings") ? (bool?)ReadBooleanAttribute(Element, "Warnings", true) : null;
				bool bAbsolute = Element.HasAttribute("Absolute") ? ReadBooleanAttribute(Element, "Absolute", true) : false;

				// Find the list of targets which are included, and recurse through all their dependencies
				HashSet<Node> Nodes = new HashSet<Node>();
				if (TargetNames != null)
				{
					HashSet<Node> TargetNodes = ResolveReferences(Element, TargetNames);
					foreach (Node Node in TargetNodes)
					{
						Nodes.Add(Node);
						Nodes.UnionWith(Node.InputDependencies);
					}
				}

				// Add all the individually referenced nodes
				if (IndividualNodeNames != null)
				{
					HashSet<Node> IndividualNodes = ResolveReferences(Element, IndividualNodeNames);
					Nodes.UnionWith(IndividualNodes);
				}

				// Exclude all the exceptions
				if (ExceptNames != null)
				{
					HashSet<Node> ExceptNodes = ResolveReferences(Element, ExceptNames);
					Nodes.ExceptWith(ExceptNodes);
				}

				// Update all the referenced nodes with the settings
				foreach (Node Node in Nodes)
				{
					if (Users != null)
					{
						if (bAbsolute)
						{
							Node.NotifyUsers = new HashSet<string>(Users);
						}
						else
						{
							Node.NotifyUsers.UnionWith(Users);
						}
					}
					if (Submitters != null)
					{
						if (bAbsolute)
						{
							Node.NotifySubmitters = new HashSet<string>(Submitters);
						}
						else
						{
							Node.NotifySubmitters.UnionWith(Submitters);
						}
					}
					if (bWarnings.HasValue)
					{
						Node.bNotifyOnWarnings = bWarnings.Value;
					}
				}

				// Add the users to the list of triggers
				if (TriggerNames != null)
				{
					foreach (string TriggerName in TriggerNames)
					{
						ManualTrigger Trigger;
						if (Graph.NameToTrigger.TryGetValue(TriggerName, out Trigger))
						{
							Trigger.NotifyUsers.UnionWith(Users);
						}
						else
						{
							LogError(Element, "Trigger '{0}' has not been defined", TriggerName);
						}
					}
				}

				// Add the users to the list of reports
				if (ReportNames != null)
				{
					foreach (string ReportName in ReportNames)
					{
						Report Report;
						if (Graph.NameToReport.TryGetValue(ReportName, out Report))
						{
							Report.NotifyUsers.UnionWith(Users);
						}
						else
						{
							LogError(Element, "Report '{0}' has not been defined", ReportName);
						}
					}
				}
			}
		}

		/// <summary>
		/// Reads a warning from the given element, evaluates the condition on it, and writes it to the log if the condition passes.
		/// </summary>
		/// <param name="Element">Xml element to read the definition from</param>
		/// <param name="EventType">The diagnostic event type</param>
		/// <param name="EnclosingNode">The node that this diagnostic is declared in, or null</param>
		/// <param name="EnclosingAgent">The agent that this diagnostic is declared in, or null</param>
		/// <param name="EnclosingTrigger">The trigger that this diagnostic is declared in, or null</param>
		void ReadDiagnostic(ScriptElement Element, LogEventType EventType, Node EnclosingNode, Agent EnclosingAgent, ManualTrigger EnclosingTrigger)
		{
			if (EvaluateCondition(Element))
			{
				string Message = ReadAttribute(Element, "Message");

				GraphDiagnostic Diagnostic = new GraphDiagnostic();
				Diagnostic.EventType = EventType;
				Diagnostic.Message = String.Format("{0}({1}): {2}", Element.File.FullName, Element.LineNumber, Message);
				Diagnostic.EnclosingNode = EnclosingNode;
				Diagnostic.EnclosingAgent = EnclosingAgent;
				Diagnostic.EnclosingTrigger = EnclosingTrigger;
				Graph.Diagnostics.Add(Diagnostic);
			}
		}

		/// <summary>
		/// Checks that the given name does not already used to refer to a node, and print an error if it is.
		/// </summary>
		/// <param name="Element">Xml element to read from</param>
		/// <param name="Name">Name of the alias</param>
		/// <returns>True if the name was registered correctly, false otherwise.</returns>
		bool CheckNameIsUnique(ScriptElement Element, string Name)
		{
			// Get the nodes that it maps to
			if (Graph.ContainsName(Name))
			{
				LogError(Element, "'{0}' is already defined; cannot add a second time", Name);
				return false;
			}
			return true;
		}

		/// <summary>
		/// Resolve a list of references to a set of nodes
		/// </summary>
		/// <param name="Element">Element used to locate any errors</param>
		/// <param name="ReferenceNames">Sequence of names to look up</param>
		/// <returns>Hashset of all the nodes included by the given names</returns>
		HashSet<Node> ResolveReferences(ScriptElement Element, IEnumerable<string> ReferenceNames)
		{
			HashSet<Node> Nodes = new HashSet<Node>();
			foreach (string ReferenceName in ReferenceNames)
			{
				Node[] OtherNodes;
				if (Graph.TryResolveReference(ReferenceName, out OtherNodes))
				{
					Nodes.UnionWith(OtherNodes);
				}
				else if (!ReferenceName.StartsWith("#") && Graph.TagNameToNodeOutput.ContainsKey("#" + ReferenceName))
				{
					LogError(Element, "Reference to '{0}' cannot be resolved; did you mean '#{0}'?", ReferenceName);
				}
				else
				{
					LogError(Element, "Reference to '{0}' cannot be resolved; check it has been defined.", ReferenceName);
				}
			}
			return Nodes;
		}

		/// <summary>
		/// Resolve a list of references to a set of nodes
		/// </summary>
		/// <param name="Element">Element used to locate any errors</param>
		/// <param name="ReferenceNames">Sequence of names to look up</param>
		/// <returns>Set of all the nodes included by the given names</returns>
		HashSet<NodeOutput> ResolveInputReferences(ScriptElement Element, IEnumerable<string> ReferenceNames)
		{
			HashSet<NodeOutput> Inputs = new HashSet<NodeOutput>();
			foreach (string ReferenceName in ReferenceNames)
			{
				NodeOutput[] ReferenceInputs;
				if (Graph.TryResolveInputReference(ReferenceName, out ReferenceInputs))
				{
					Inputs.UnionWith(ReferenceInputs);
				}
				else if (!ReferenceName.StartsWith("#") && Graph.TagNameToNodeOutput.ContainsKey("#" + ReferenceName))
				{
					LogError(Element, "Reference to '{0}' cannot be resolved; did you mean '#{0}'?", ReferenceName);
				}
				else
				{
					LogError(Element, "Reference to '{0}' cannot be resolved; check it has been defined.", ReferenceName);
				}
			}
			return Inputs;
		}

		/// <summary>
		/// Reads an object name from its defining element. Outputs an error if the name is missing.
		/// </summary>
		/// <param name="Element">Element to read the name for</param>
		/// <param name="Name">Output variable to receive the name of the object</param>
		/// <returns>True if the object had a valid name (assigned to the Name variable), false if the name was invalid or missing.</returns>
		bool TryReadObjectName(ScriptElement Element, out string Name)
		{
			// Check the name attribute is present
			if (!Element.HasAttribute("Name"))
			{
				LogError(Element, "Missing 'Name' attribute");
				Name = null;
				return false;
			}

			// Get the value of it, strip any leading or trailing whitespace, and make sure it's not empty
			string Value = ReadAttribute(Element, "Name");
			if (!ValidateName(Element, Value))
			{
				Name = null;
				return false;
			}

			// Return it
			Name = Value;
			return true;
		}

		/// <summary>
		/// Reads an qualified object name from its defining element. Outputs an error if the name is missing.
		/// </summary>
		/// <param name="Element">Element to read the name for</param>
		/// <param name="QualifiedName">Output variable to receive the name of the object</param>
		/// <returns>True if the object had a valid name (assigned to the Name variable), false if the name was invalid or missing.</returns>
		bool TryReadQualifiedObjectName(ScriptElement Element, out string[] QualifiedName)
		{
			// Check the name attribute is present
			if (!Element.HasAttribute("Name"))
			{
				LogError(Element, "Missing 'Name' attribute");
				QualifiedName = null;
				return false;
			}

			// Get the value of it, strip any leading or trailing whitespace, and make sure it's not empty
			string[] Values = ReadAttribute(Element, "Name").Split('.');
			foreach (string Value in Values)
			{
				if (!ValidateName(Element, Value))
				{
					QualifiedName = null;
					return false;
				}
			}

			// Return it
			QualifiedName = Values;
			return true;
		}

		/// <summary>
		/// Checks that the given name is valid syntax
		/// </summary>
		/// <param name="Element">The element that contains the name</param>
		/// <param name="Name">The name to check</param>
		/// <returns>True if the name is valid</returns>
		bool ValidateName(ScriptElement Element, string Name)
		{
			// Check it's not empty
			if (Name.Length == 0)
			{
				LogError(Element, "Name is empty");
				return false;
			}

			// Check there are no invalid characters
			for (int Idx = 0; Idx < Name.Length; Idx++)
			{
				if (Idx > 0 && Name[Idx] == ' ' && Name[Idx - 1] == ' ')
				{
					LogError(Element, "Consecutive spaces in object name");
					return false;
				}
				if(Char.IsControl(Name[Idx]) || ScriptSchema.IllegalNameCharacters.IndexOf(Name[Idx]) != -1)
				{
					LogError(Element, "Invalid character in object name - '{0}'", Name[Idx]);
					return false;
				}
			}
			return true;
		}

		/// <summary>
		/// Expands any properties and reads an attribute.
		/// </summary>
		/// <param name="Element">Element to read the attribute from</param>
		/// <param name="Name">Name of the attribute</param>
		/// <returns>Array of names, with all leading and trailing whitespace removed</returns>
		string ReadAttribute(ScriptElement Element, string Name)
		{
			return ExpandProperties(Element, Element.GetAttribute(Name));
		}

		/// <summary>
		/// Expands any properties and reads a list of strings from an attribute, separated by semi-colon characters
		/// </summary>
		/// <param name="Element"></param>
		/// <param name="Name"></param>
		/// <returns>Array of names, with all leading and trailing whitespace removed</returns>
		string[] ReadListAttribute(ScriptElement Element, string Name)
		{
			string Value = ReadAttribute(Element, Name);
			return Value.Split(new char[] { ';' }).Select(x => x.Trim()).Where(x => x.Length > 0).ToArray();
		}

		/// <summary>
		/// Reads an attribute from the given XML element, expands any properties in it, and parses it as a boolean.
		/// </summary>
		/// <param name="Element">Element to read the attribute from</param>
		/// <param name="Name">Name of the attribute</param>
		/// <param name="bDefaultValue">Default value if the attribute is missing</param>
		/// <returns>The value of the attribute field</returns>
		bool ReadBooleanAttribute(ScriptElement Element, string Name, bool bDefaultValue)
		{
			bool bResult = bDefaultValue;
			if (Element.HasAttribute(Name))
			{
				string Value = ReadAttribute(Element, Name).Trim();
				if (Value.Equals("true", StringComparison.InvariantCultureIgnoreCase))
				{
					bResult = true;
				}
				else if (Value.Equals("false", StringComparison.InvariantCultureIgnoreCase))
				{
					bResult = false;
				}
				else
				{
					LogError(Element, "Invalid boolean value '{0}' - expected 'true' or 'false'", Value);
				}
			}
			return bResult;
		}

		/// <summary>
		/// Reads an attribute from the given XML element, expands any properties in it, and parses it as an integer.
		/// </summary>
		/// <param name="Element">Element to read the attribute from</param>
		/// <param name="Name">Name of the attribute</param>
		/// <param name="DefaultValue">Default value for the integer, if the attribute is missing</param>
		/// <returns>The value of the attribute field</returns>
		int ReadIntegerAttribute(ScriptElement Element, string Name, int DefaultValue)
		{
			int Result = DefaultValue;
			if (Element.HasAttribute(Name))
			{
				string Value = ReadAttribute(Element, Name).Trim();

				int IntValue;
				if (Int32.TryParse(Value, out IntValue))
				{
					Result = IntValue;
				}
				else
				{
					LogError(Element, "Invalid integer value '{0}'", Value);
				}
			}
			return Result;
		}

		/// <summary>
		/// Reads an attribute from the given XML element, expands any properties in it, and parses it as an enum of the given type.
		/// </summary>
		/// <typeparam name="T">The enum type to parse the attribute as</typeparam>
		/// <param name="Element">Element to read the attribute from</param>
		/// <param name="Name">Name of the attribute</param>
		/// <param name="DefaultValue">Default value for the enum, if the attribute is missing</param>
		/// <returns>The value of the attribute field</returns>
		T ReadEnumAttribute<T>(ScriptElement Element, string Name, T DefaultValue) where T : struct
		{
			T Result = DefaultValue;
			if (Element.HasAttribute(Name))
			{
				string Value = ReadAttribute(Element, Name).Trim();

				T EnumValue;
				if (Enum.TryParse(Value, true, out EnumValue))
				{
					Result = EnumValue;
				}
				else
				{
					LogError(Element, "Invalid value '{0}' - expected {1}", Value, String.Join("/", Enum.GetNames(typeof(T))));
				}
			}
			return Result;
		}

		/// <summary>
		/// Outputs an error message to the log and increments the number of errors, referencing the file and line number of the element that caused it.
		/// </summary>
		/// <param name="Element">The script element causing the error</param>
		/// <param name="Format">Standard String.Format()-style format string</param>
		/// <param name="Args">Optional arguments</param>
		void LogError(ScriptElement Element, string Format, params object[] Args)
		{
			CommandUtils.LogError("{0}({1}): {2}", Element.File.FullName, Element.LineNumber, String.Format(Format, Args));
			NumErrors++;
		}

		/// <summary>
		/// Outputs a warning message to the log and increments the number of errors, referencing the file and line number of the element that caused it.
		/// </summary>
		/// <param name="Element">The script element causing the error</param>
		/// <param name="Format">Standard String.Format()-style format string</param>
		/// <param name="Args">Optional arguments</param>
		void LogWarning(ScriptElement Element, string Format, params object[] Args)
		{
			CommandUtils.LogWarning("{0}({1}): {2}", Element.File.FullName, Element.LineNumber, String.Format(Format, Args));
		}

		/// <summary>
		/// Evaluates the (optional) conditional expression on a given XML element via the If="..." attribute, and returns true if the element is enabled.
		/// </summary>
		/// <param name="Element">The element to check</param>
		/// <returns>True if the element's condition evaluates to true (or doesn't have a conditional expression), false otherwise</returns>
		bool EvaluateCondition(ScriptElement Element)
		{
			// Check if the element has a conditional attribute
			const string AttributeName = "If";
			if (!Element.HasAttribute(AttributeName))
			{
				return true;
			}

			// If it does, try to evaluate it.
			try
			{
				string Text = ExpandProperties(Element, Element.GetAttribute("If"));
				return Condition.Evaluate(Text);
			}
			catch (ConditionException Ex)
			{
				LogError(Element, "Error in condition: {0}", Ex.Message);
				return false;
			}
		}

		/// <summary>
		/// Expand all the property references (of the form $(PropertyName)) in a string.
		/// </summary>
		/// <param name="Element">The element containing the string. Used for diagnostic messages.</param>
		/// <param name="Text">The input string to expand properties in</param>
		/// <returns>The expanded string</returns>
		string ExpandProperties(ScriptElement Element, string Text)
		{
			string Result = Text;
			for (int Idx = Result.IndexOf("$("); Idx != -1; Idx = Result.IndexOf("$(", Idx))
			{
				// Find the end of the variable name
				int EndIdx = Result.IndexOf(')', Idx + 2);
				if (EndIdx == -1)
				{
					break;
				}

				// Extract the variable name from the string
				string Name = Result.Substring(Idx + 2, EndIdx - (Idx + 2));

				// Find the value for it, either from the dictionary or the environment block
				string Value;
				if(!TryGetPropertyValue(Name, out Value))
				{
					LogWarning(Element, "Property '{0}' is not defined", Name);
					Value = "";
				}

				// Replace the variable, or skip past it
				Result = Result.Substring(0, Idx) + Value + Result.Substring(EndIdx + 1);

				// Make sure we skip over the expanded variable; we don't want to recurse on it.
				Idx += Value.Length;
			}
			return Result;
		}
	}
}
