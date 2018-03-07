using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml;

namespace Tools.DotNETCommon
{
	/// <summary>
	/// Basic information from a preprocessed C# project file. Supports reading a project file, expanding simple conditions in it, parsing property values, assembly references and references to other projects.
	/// </summary>
	public class CsProjectInfo
	{
		/// <summary>
		/// Evaluated properties from the project file
		/// </summary>
		public Dictionary<string, string> Properties;

		/// <summary>
		/// Mapping of referenced assemblies to their 'CopyLocal' (aka 'Private') setting.
		/// </summary>
		public Dictionary<FileReference, bool> References = new Dictionary<FileReference, bool>();

		/// <summary>
		/// Mapping of referenced projects to their 'CopyLocal' (aka 'Private') setting.
		/// </summary>
		public Dictionary<FileReference, bool> ProjectReferences = new Dictionary<FileReference, bool>();

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InProperties">Initial mapping of property names to values</param>
		CsProjectInfo(Dictionary<string, string> InProperties)
		{
			Properties = new Dictionary<string, string>(InProperties);
		}

		/// <summary>
		/// Resolve the project's output directory
		/// </summary>
		/// <param name="BaseDirectory">Base directory to resolve relative paths to</param>
		/// <returns>The configured output directory</returns>
		public DirectoryReference GetOutputDir(DirectoryReference BaseDirectory)
		{
			string OutputPath;
			if (Properties.TryGetValue("OutputPath", out OutputPath))
			{
				return DirectoryReference.Combine(BaseDirectory, OutputPath);
			}
			else
			{
				return BaseDirectory;
			}
		}

		/// <summary>
		/// Adds build products from the project to the given set.
		/// </summary>
		/// <param name="OutputDir">Output directory for the build products. May be different to the project's output directory in the case that we're copying local to another project.</param>
		/// <param name="BuildProducts">Set to receive the list of build products</param>
		public bool AddBuildProducts(DirectoryReference OutputDir, HashSet<FileReference> BuildProducts)
		{
			string OutputType, AssemblyName;
			if (Properties.TryGetValue("OutputType", out OutputType) && Properties.TryGetValue("AssemblyName", out AssemblyName))
			{
				// DotNET Core framework doesn't produce .exe files, it produces DLLs in all cases
				if (IsDotNETCoreProject())
				{
					OutputType = "Library";
				}

				switch (OutputType)
				{
					case "Exe":
					case "WinExe":
						BuildProducts.Add(FileReference.Combine(OutputDir, AssemblyName + ".exe"));
						AddOptionalBuildProduct(FileReference.Combine(OutputDir, AssemblyName + ".pdb"), BuildProducts);
						AddOptionalBuildProduct(FileReference.Combine(OutputDir, AssemblyName + ".exe.config"), BuildProducts);
						AddOptionalBuildProduct(FileReference.Combine(OutputDir, AssemblyName + ".exe.mdb"), BuildProducts);
						return true;
					case "Library":
						BuildProducts.Add(FileReference.Combine(OutputDir, AssemblyName + ".dll"));
						AddOptionalBuildProduct(FileReference.Combine(OutputDir, AssemblyName + ".pdb"), BuildProducts);
						AddOptionalBuildProduct(FileReference.Combine(OutputDir, AssemblyName + ".dll.config"), BuildProducts);
						AddOptionalBuildProduct(FileReference.Combine(OutputDir, AssemblyName + ".dll.mdb"), BuildProducts);
						return true;
				}
			}
			return false;
		}

		public bool IsDotNETCoreProject()
		{
			bool bIsDotNetCoreProject = false;

			string TargetFramework;
			if (Properties.TryGetValue("TargetFramework", out TargetFramework))
			{
				bIsDotNetCoreProject = TargetFramework.ToLower().Contains("netstandard") || TargetFramework.ToLower().Contains("netcoreapp");
			}

			return bIsDotNetCoreProject;
		}

		/// <summary>
		/// Adds a build product to the output list if it exists
		/// </summary>
		/// <param name="BuildProduct">The build product to add</param>
		/// <param name="BuildProducts">List of output build products</param>
		public static void AddOptionalBuildProduct(FileReference BuildProduct, HashSet<FileReference> BuildProducts)
		{
			if (FileReference.Exists(BuildProduct))
			{
				BuildProducts.Add(BuildProduct);
			}
		}

		/// <summary>
		/// Attempts to read project information for the given file.
		/// </summary>
		/// <param name="File">The project file to read</param>
		/// <param name="Properties">Initial set of property values</param>
		/// <param name="OutProjectInfo">If successful, the parsed project info</param>
		/// <returns>True if the project was read successfully, false otherwise</returns>
		public static bool TryRead(FileReference File, Dictionary<string, string> Properties, out CsProjectInfo OutProjectInfo)
		{
			// Read the project file
			XmlDocument Document = new XmlDocument();
			Document.Load(File.FullName);

			// Check the root element is the right type
			//			HashSet<FileReference> ProjectBuildProducts = new HashSet<FileReference>();
			if (Document.DocumentElement.Name != "Project")
			{
				OutProjectInfo = null;
				return false;
			}

			// Parse the basic structure of the document, updating properties and recursing into other referenced projects as we go
			CsProjectInfo ProjectInfo = new CsProjectInfo(Properties);
			foreach (XmlElement Element in Document.DocumentElement.ChildNodes.OfType<XmlElement>())
			{
				switch (Element.Name)
				{
					case "PropertyGroup":
						if (EvaluateCondition(Element, ProjectInfo.Properties))
						{
							ParsePropertyGroup(Element, ProjectInfo.Properties);
						}
						break;
					case "ItemGroup":
						if (EvaluateCondition(Element, ProjectInfo.Properties))
						{
							ParseItemGroup(File.Directory, Element, ProjectInfo);
						}
						break;
				}
			}

			// Return the complete project
			OutProjectInfo = ProjectInfo;
			return true;
		}

		/// <summary>
		/// Parses a 'PropertyGroup' element.
		/// </summary>
		/// <param name="ParentElement">The parent 'PropertyGroup' element</param>
		/// <param name="Properties">Dictionary mapping property names to values</param>
		static void ParsePropertyGroup(XmlElement ParentElement, Dictionary<string, string> Properties)
		{
			// We need to know the overridden output type and output path for the selected configuration.
			foreach (XmlElement Element in ParentElement.ChildNodes.OfType<XmlElement>())
			{
				if (EvaluateCondition(Element, Properties))
				{
					Properties[Element.Name] = ExpandProperties(Element.InnerText, Properties);
				}
			}
		}

		/// <summary>
		/// Parses an 'ItemGroup' element.
		/// </summary>
		/// <param name="BaseDirectory">Base directory to resolve relative paths against</param>
		/// <param name="ParentElement">The parent 'ItemGroup' element</param>
		/// <param name="ProjectInfo">Project info object to be updated</param>
		static void ParseItemGroup(DirectoryReference BaseDirectory, XmlElement ParentElement, CsProjectInfo ProjectInfo)
		{
			// Parse any external assembly references
			foreach (XmlElement ItemElement in ParentElement.ChildNodes.OfType<XmlElement>())
			{
				switch (ItemElement.Name)
				{
					case "Reference":
						// Reference to an external assembly
						if (EvaluateCondition(ItemElement, ProjectInfo.Properties))
						{
							ParseReference(BaseDirectory, ItemElement, ProjectInfo.References);
						}
						break;
					case "ProjectReference":
						// Reference to another project
						if (EvaluateCondition(ItemElement, ProjectInfo.Properties))
						{
							ParseProjectReference(BaseDirectory, ItemElement, ProjectInfo.ProjectReferences);
						}
						break;
				}
			}
		}

		/// <summary>
		/// Parses an assembly reference from a given 'Reference' element
		/// </summary>
		/// <param name="BaseDirectory">Directory to resolve relative paths against</param>
		/// <param name="ParentElement">The parent 'Reference' element</param>
		/// <param name="References">Dictionary of project files to a bool indicating whether the assembly should be copied locally to the referencing project.</param>
		static void ParseReference(DirectoryReference BaseDirectory, XmlElement ParentElement, Dictionary<FileReference, bool> References)
		{
			string HintPath = GetChildElementString(ParentElement, "HintPath", null);
			if (!String.IsNullOrEmpty(HintPath))
			{
				FileReference AssemblyFile = FileReference.Combine(BaseDirectory, HintPath);
				bool bPrivate = GetChildElementBoolean(ParentElement, "Private", true);
				References.Add(AssemblyFile, bPrivate);
			}
		}

		/// <summary>
		/// Parses a project reference from a given 'ProjectReference' element
		/// </summary>
		/// <param name="BaseDirectory">Directory to resolve relative paths against</param>
		/// <param name="ParentElement">The parent 'ProjectReference' element</param>
		/// <param name="ProjectReferences">Dictionary of project files to a bool indicating whether the outputs of the project should be copied locally to the referencing project.</param>
		static void ParseProjectReference(DirectoryReference BaseDirectory, XmlElement ParentElement, Dictionary<FileReference, bool> ProjectReferences)
		{
			string IncludePath = ParentElement.GetAttribute("Include");
			if (!String.IsNullOrEmpty(IncludePath))
			{
				FileReference ProjectFile = FileReference.Combine(BaseDirectory, IncludePath);
				bool bPrivate = GetChildElementBoolean(ParentElement, "Private", true);
				ProjectReferences[ProjectFile] = bPrivate;
			}
		}

		/// <summary>
		/// Reads the inner text of a child XML element
		/// </summary>
		/// <param name="ParentElement">The parent element to check</param>
		/// <param name="Name">Name of the child element</param>
		/// <param name="DefaultValue">Default value to return if the child element is missing</param>
		/// <returns>The contents of the child element, or default value if it's not present</returns>
		static string GetChildElementString(XmlElement ParentElement, string Name, string DefaultValue)
		{
			XmlElement ChildElement = ParentElement.ChildNodes.OfType<XmlElement>().FirstOrDefault(x => x.Name == Name);
			if (ChildElement == null)
			{
				return DefaultValue;
			}
			else
			{
				return ChildElement.InnerText ?? DefaultValue;
			}
		}

		/// <summary>
		/// Read a child XML element with the given name, and parse it as a boolean.
		/// </summary>
		/// <param name="ParentElement">Parent element to check</param>
		/// <param name="Name">Name of the child element to look for</param>
		/// <param name="DefaultValue">Default value to return if the element is missing or not a valid bool</param>
		/// <returns>The parsed boolean, or the default value</returns>
		static bool GetChildElementBoolean(XmlElement ParentElement, string Name, bool DefaultValue)
		{
			string Value = GetChildElementString(ParentElement, Name, null);
			if (Value == null)
			{
				return DefaultValue;
			}
			else if (Value.Equals("True", StringComparison.InvariantCultureIgnoreCase))
			{
				return true;
			}
			else if (Value.Equals("False", StringComparison.InvariantCultureIgnoreCase))
			{
				return false;
			}
			else
			{
				return DefaultValue;
			}
		}

		/// <summary>
		/// Evaluate whether the optional MSBuild condition on an XML element evaluates to true. Currently only supports 'ABC' == 'DEF' style expressions, but can be expanded as needed.
		/// </summary>
		/// <param name="Element">The XML element to check</param>
		/// <param name="Properties">Dictionary mapping from property names to values.</param>
		/// <returns></returns>
		static bool EvaluateCondition(XmlElement Element, Dictionary<string, string> Properties)
		{
			// Read the condition attribute. If it's not present, assume it evaluates to true.
			string Condition = Element.GetAttribute("Condition");
			if (String.IsNullOrEmpty(Condition))
			{
				return true;
			}

			// Expand all the properties
			Condition = ExpandProperties(Condition, Properties);

			// Tokenize the condition
			string[] Tokens = Tokenize(Condition);

			// Try to evaluate it. We only support a very limited class of condition expressions at the moment, but it's enough to parse standard projects
			bool bResult;
			if (Tokens.Length == 3 && Tokens[0].StartsWith("'") && Tokens[1] == "==" && Tokens[2].StartsWith("'"))
			{
				bResult = String.Compare(Tokens[0], Tokens[2], StringComparison.InvariantCultureIgnoreCase) == 0;
			}
			else
			{
				throw new Exception("Couldn't parse condition in project file");
			}
			return bResult;
		}

		/// <summary>
		/// Expand MSBuild properties within a string. If referenced properties are not in this dictionary, the process' environment variables are expanded. Unknown properties are expanded to an empty string.
		/// </summary>
		/// <param name="Text">The input string to expand</param>
		/// <param name="Properties">Dictionary mapping from property names to values.</param>
		/// <returns>String with all properties expanded.</returns>
		static string ExpandProperties(string Text, Dictionary<string, string> Properties)
		{
			string NewText = Text;
			for (int Idx = NewText.IndexOf("$("); Idx != -1; Idx = NewText.IndexOf("$(", Idx))
			{
				// Find the end of the variable name
				int EndIdx = NewText.IndexOf(')', Idx + 2);
				if (EndIdx != -1)
				{
					// Extract the variable name from the string
					string Name = NewText.Substring(Idx + 2, EndIdx - (Idx + 2));

					// Find the value for it, either from the dictionary or the environment block
					string Value;
					if (!Properties.TryGetValue(Name, out Value))
					{
						Value = Environment.GetEnvironmentVariable(Name) ?? "";
					}

					// Replace the variable, or skip past it
					NewText = NewText.Substring(0, Idx) + Value + NewText.Substring(EndIdx + 1);

					// Make sure we skip over the expanded variable; we don't want to recurse on it.
					Idx += Value.Length;
				}
			}
			return NewText;
		}

		/// <summary>
		/// Split an MSBuild condition into tokens
		/// </summary>
		/// <param name="Condition">The condition expression</param>
		/// <returns>Array of the parsed tokens</returns>
		static string[] Tokenize(string Condition)
		{
			List<string> Tokens = new List<string>();
			for (int Idx = 0; Idx < Condition.Length; Idx++)
			{
				if (Idx + 1 < Condition.Length && Condition[Idx] == '=' && Condition[Idx + 1] == '=')
				{
					// "==" operator
					Idx++;
					Tokens.Add("==");
				}
				else if (Condition[Idx] == '\'')
				{
					// Quoted string
					int StartIdx = Idx++;
					while (Idx + 1 < Condition.Length && Condition[Idx] != '\'')
					{
						Idx++;
					}
					Tokens.Add(Condition.Substring(StartIdx, Idx - StartIdx));
				}
				else if (!Char.IsWhiteSpace(Condition[Idx]))
				{
					// Other token; assume a single character.
					string Token = Condition.Substring(Idx, 1);
					Tokens.Add(Token);
				}
			}
			return Tokens.ToArray();
		}
	}
}
