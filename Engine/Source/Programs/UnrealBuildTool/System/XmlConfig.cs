using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;
using System.Xml;
using System.Xml.Schema;
using System.Xml.Serialization;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Functions for manipulating the XML config cache
	/// </summary>
	static class XmlConfig
	{
		/// <summary>
		/// An input config file
		/// </summary>
		public class InputFile
		{
			/// <summary>
			/// Location of the file
			/// </summary>
			public FileReference Location;

			/// <summary>
			/// Which folder to display the config file under in the generated project files
			/// </summary>
			public string FolderName;
		}

		/// <summary>
		/// Parsed config values
		/// </summary>
		static XmlConfigData Values;

		/// <summary>
		/// Cached serializer for the XML schema
		/// </summary>
		static XmlSerializer CachedSchemaSerializer;

		/// <summary>
		/// Initialize the config system with the given types
		/// </summary>
		public static bool ReadConfigFiles()
		{
			// Find all the configurable types
			List<Type> ConfigTypes = FindConfigurableTypes();

			// Find all the input files
			FileReference[] InputFiles = FindInputFiles().Select(x => x.Location).ToArray();

			// Get the path to the cache file
			FileReference CacheFile = FileReference.Combine(UnrealBuildTool.EngineDirectory, "Intermediate", "Build", "XmlConfigCache.bin");
			FileReference SchemaFile = GetSchemaLocation();

			// Try to read the existing cache from disk
			XmlConfigData CachedValues;
			if(IsCacheUpToDate(CacheFile, InputFiles) && FileReference.Exists(SchemaFile))
			{
				if(XmlConfigData.TryRead(CacheFile, ConfigTypes, out CachedValues) && Enumerable.SequenceEqual(InputFiles, CachedValues.InputFiles))
				{
					Values = CachedValues;
				}
			}

			// If that failed, regenerate it
			if(Values == null)
			{
				// Find all the configurable fields from the given types
				Dictionary<string, Dictionary<string, FieldInfo>> CategoryToFields = new Dictionary<string, Dictionary<string, FieldInfo>>();
				FindConfigurableFields(ConfigTypes, CategoryToFields);

				// Create a schema for the config files
				XmlSchema Schema = CreateSchema(CategoryToFields);
				WriteSchema(Schema, SchemaFile);

				// Read all the XML files and validate them against the schema
				Dictionary<Type, Dictionary<FieldInfo, object>> TypeToValues = new Dictionary<Type, Dictionary<FieldInfo, object>>();
				foreach(FileReference InputFile in InputFiles)
				{
					if(!TryReadFile(InputFile, CategoryToFields, TypeToValues, Schema))
					{
						return false;
					}
				}

				// Create the new cache
				Values = new XmlConfigData(InputFiles, TypeToValues.ToDictionary(x => x.Key, x => x.Value.ToArray()));
				Values.Write(CacheFile);
			}

			// Apply all the static field values
			foreach(KeyValuePair<Type, KeyValuePair<FieldInfo, object>[]> TypeValuesPair in Values.TypeToValues)
			{
				foreach(KeyValuePair<FieldInfo, object> FieldValuePair in TypeValuesPair.Value)
				{
					if(FieldValuePair.Key.IsStatic)
					{
						object Value = InstanceValue(FieldValuePair.Value, FieldValuePair.Key.FieldType);
						FieldValuePair.Key.SetValue(null, Value);
					}
				}
			}
			return true;
		}

		/// <summary>
		/// Find all the configurable types in the current assembly
		/// </summary>
		/// <returns>List of configurable types</returns>
		static List<Type> FindConfigurableTypes()
		{
			List<Type> ConfigTypes = new List<Type>();
			foreach(Type ConfigType in Assembly.GetExecutingAssembly().GetTypes())
			{
				if(HasXmlConfigFileAttribute(ConfigType))
				{
					ConfigTypes.Add(ConfigType);
				}
			}
			return ConfigTypes;
		}

		/// <summary>
		/// Determines whether the given type has a field with an XmlConfigFile attribute
		/// </summary>
		/// <param name="Type">The type to check</param>
		/// <returns>True if the type has a field with the XmlConfigFile attribute</returns>
		static bool HasXmlConfigFileAttribute(Type Type)
		{
			foreach(FieldInfo Field in Type.GetFields(BindingFlags.Instance | BindingFlags.Static | BindingFlags.Public | BindingFlags.NonPublic))
			{
				foreach(CustomAttributeData CustomAttribute in Field.CustomAttributes)
				{
					if(CustomAttribute.AttributeType == typeof(XmlConfigFileAttribute))
					{
						return true;
					}
				}
			}
			return false;
		}

		/// <summary>
		/// Find the location of the XML config schema
		/// </summary>
		/// <returns>The location of the schema file</returns>
		public static FileReference GetSchemaLocation()
		{
			return FileReference.Combine(UnrealBuildTool.EngineDirectory, "Saved", "UnrealBuildTool", "BuildConfiguration.Schema.xsd");
		}

		/// <summary>
		/// Initialize the list of input files
		/// </summary>
		public static List<InputFile> FindInputFiles()
		{
			// Find all the input file locations
			List<InputFile> InputFiles = new List<InputFile>();

			// Check for the config file under /Engine/Programs/NotForLicensees/UnrealBuildTool
			FileReference NotForLicenseesConfigLocation = FileReference.Combine(UnrealBuildTool.EngineDirectory, "Programs", "NotForLicensees", "UnrealBuildTool", "BuildConfiguration.xml");
			if(FileReference.Exists(NotForLicenseesConfigLocation))
			{
				InputFiles.Add(new InputFile { Location = NotForLicenseesConfigLocation, FolderName = "NotForLicensees" });
			}

			// Check for the user config file under /Engine/Programs/NotForLicensees/UnrealBuildTool
			FileReference UserConfigLocation = FileReference.Combine(UnrealBuildTool.EngineDirectory, "Saved", "UnrealBuildTool", "BuildConfiguration.xml");
			if(!FileReference.Exists(UserConfigLocation))
			{
				CreateDefaultConfigFile(UserConfigLocation);
			}
			InputFiles.Add(new InputFile { Location = UserConfigLocation, FolderName = "User" });

			// Check for the global config file under AppData/Unreal Engine/UnrealBuildTool
			string AppDataFolder = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData);
			if(!String.IsNullOrEmpty(AppDataFolder))
			{
				FileReference AppDataConfigLocation = FileReference.Combine(new DirectoryReference(AppDataFolder), "Unreal Engine", "UnrealBuildTool", "BuildConfiguration.xml");
				if(!FileReference.Exists(AppDataConfigLocation))
				{
					CreateDefaultConfigFile(AppDataConfigLocation);
				}
				InputFiles.Add(new InputFile { Location = AppDataConfigLocation, FolderName = "Global (AppData)" });
			}

			// Check for the global config file under My Documents/Unreal Engine/UnrealBuildTool
			string PersonalFolder = Environment.GetFolderPath(Environment.SpecialFolder.Personal);
			if(!String.IsNullOrEmpty(PersonalFolder))
			{
				FileReference PersonalConfigLocation = FileReference.Combine(new DirectoryReference(PersonalFolder), "Unreal Engine", "UnrealBuildTool", "BuildConfiguration.xml");
				if(FileReference.Exists(PersonalConfigLocation))
				{
					InputFiles.Add(new InputFile { Location = PersonalConfigLocation, FolderName = "Global (Documents)" });
				}
			}

			return InputFiles;
		}

		/// <summary>
		/// Create a default config file at the given location
		/// </summary>
		/// <param name="Location">Location to read from</param>
		static void CreateDefaultConfigFile(FileReference Location)
		{
			DirectoryReference.CreateDirectory(Location.Directory);
			using (StreamWriter Writer = new StreamWriter(Location.FullName))
			{
				Writer.WriteLine("<?xml version=\"1.0\" encoding=\"utf-8\" ?>");
				Writer.WriteLine("<Configuration xmlns=\"{0}\">", XmlConfigFile.SchemaNamespaceURI);
				Writer.WriteLine("</Configuration>");
			}
		}

		/// <summary>
		/// Applies config values to the given object
		/// </summary>
		/// <param name="TargetObject">The object instance to be configured</param>
		public static void ApplyTo(object TargetObject)
		{
			for(Type TargetType = TargetObject.GetType(); TargetType != null; TargetType = TargetType.BaseType)
			{
				KeyValuePair<FieldInfo, object>[] FieldValues;
				if(Values.TypeToValues.TryGetValue(TargetType, out FieldValues))
				{
					foreach(KeyValuePair<FieldInfo, object> FieldValuePair in FieldValues)
					{
						if(!FieldValuePair.Key.IsStatic)
						{
							object ValueInstance = InstanceValue(FieldValuePair.Value, FieldValuePair.Key.FieldType);
							FieldValuePair.Key.SetValue(TargetObject, ValueInstance);
						}
					}
				}
			}
		}

		/// <summary>
		/// Instances a value for assignment to a target object
		/// </summary>
		/// <param name="Value">The value to instance</param>
		/// <param name="ValueType">The type of value</param>
		/// <returns>New instance of the given value, if necessary</returns>
		static object InstanceValue(object Value, Type ValueType)
		{
			if(ValueType == typeof(string[]))
			{
				return ((string[])Value).Clone();
			}
			else
			{
				return Value;
			}
		}

		/// <summary>
		/// Gets a config value for a single value, without writing it to an instance of that class
		/// </summary>
		/// <param name="TargetType">Type to find config values for</param>
		/// <param name="Name">Name of the field to receive</param>
		/// <param name="Value">On success, receives the value of the field</param>
		/// <returns>True if the value was read, false otherwise</returns>
		public static bool TryGetValue(Type TargetType, string Name, out object Value)
		{
			// Find all the config values for this type
			KeyValuePair<FieldInfo, object>[] FieldValues;
			if(!Values.TypeToValues.TryGetValue(TargetType, out FieldValues))
			{
				Value = null;
				return false;
			}

			// Find the value with the matching name
			foreach(KeyValuePair<FieldInfo, object> FieldPair in FieldValues)
			{
				if(FieldPair.Key.Name == Name)
				{
					Value = FieldPair.Value;
					return true;
				}
			}

			// Not found
			Value = null;
			return false;
		}

		/// <summary>
		/// Find all the configurable fields in the given types by searching for XmlConfigFile attributes.
		/// </summary>
		/// <param name="ConfigTypes">Array of types to search</param>
		/// <param name="CategoryToFields">Dictionaries populated with category -> name -> field mappings on return</param>
		static void FindConfigurableFields(IEnumerable<Type> ConfigTypes, Dictionary<string, Dictionary<string, FieldInfo>> CategoryToFields)
		{
			foreach(Type ConfigType in ConfigTypes)
			{
				foreach(FieldInfo FieldInfo in ConfigType.GetFields(BindingFlags.Instance | BindingFlags.Static | BindingFlags.GetField | BindingFlags.Public | BindingFlags.NonPublic))
				{
					IEnumerable<XmlConfigFileAttribute> Attributes = FieldInfo.GetCustomAttributes<XmlConfigFileAttribute>();
					foreach(XmlConfigFileAttribute Attribute in Attributes)
					{
						string CategoryName = Attribute.Category ?? ConfigType.Name;

						Dictionary<string, FieldInfo> NameToField;
						if(!CategoryToFields.TryGetValue(CategoryName, out NameToField))
						{
							NameToField = new Dictionary<string, FieldInfo>();
							CategoryToFields.Add(CategoryName, NameToField);
						}

						NameToField[Attribute.Name ?? FieldInfo.Name] = FieldInfo;
					}
				}
			}
		}

		/// <summary>
		/// Creates a schema from attributes in the given types
		/// </summary>
		/// <param name="CategoryToFields">Lookup for all field settings</param>
		/// <returns>New schema instance</returns>
		static XmlSchema CreateSchema(Dictionary<string, Dictionary<string, FieldInfo>> CategoryToFields)
		{
			// Create elements for all the categories
			XmlSchemaAll RootAll = new XmlSchemaAll();
			foreach(KeyValuePair<string, Dictionary<string, FieldInfo>> CategoryPair in CategoryToFields)
			{
				string CategoryName = CategoryPair.Key;

				XmlSchemaAll CategoryAll = new XmlSchemaAll();
				foreach (KeyValuePair<string, FieldInfo> FieldPair in CategoryPair.Value)
				{
					XmlSchemaElement Element = CreateSchemaFieldElement(FieldPair.Key, FieldPair.Value.FieldType);
					CategoryAll.Items.Add(Element);
				}

				XmlSchemaComplexType CategoryType = new XmlSchemaComplexType();
				CategoryType.Particle = CategoryAll;

				XmlSchemaElement CategoryElement = new XmlSchemaElement();
				CategoryElement.Name = CategoryName;
				CategoryElement.SchemaType = CategoryType;
				CategoryElement.MinOccurs = 0;
				CategoryElement.MaxOccurs = 1;

				RootAll.Items.Add(CategoryElement);
			}

			// Create the root element and schema object
			XmlSchemaComplexType RootType = new XmlSchemaComplexType();
			RootType.Particle = RootAll;

			XmlSchemaElement RootElement = new XmlSchemaElement();
			RootElement.Name = XmlConfigFile.RootElementName;
			RootElement.SchemaType = RootType;

			XmlSchema Schema = new XmlSchema();
			Schema.TargetNamespace = XmlConfigFile.SchemaNamespaceURI;
			Schema.ElementFormDefault = XmlSchemaForm.Qualified;
			Schema.Items.Add(RootElement);

			// Finally compile it
			XmlSchemaSet SchemaSet = new XmlSchemaSet();
			SchemaSet.Add(Schema);
			SchemaSet.Compile();
			return SchemaSet.Schemas().OfType<XmlSchema>().First();
		}

		/// <summary>
		/// Creates an XML schema element for reading a value of the given type
		/// </summary>
		/// <param name="Name">Name of the field</param>
		/// <param name="Type">Type of the field</param>
		/// <returns>New schema element representing the field</returns>
		static XmlSchemaElement CreateSchemaFieldElement(string Name, Type Type)
		{
			XmlSchemaElement Element = new XmlSchemaElement();
			Element.Name = Name;
			Element.MinOccurs = 0;
			Element.MaxOccurs = 1;

			if(Type == typeof(string))
			{
				Element.SchemaTypeName = XmlSchemaType.GetBuiltInSimpleType(XmlTypeCode.String).QualifiedName;
			}
			else if(Type == typeof(bool))
			{
				Element.SchemaTypeName = XmlSchemaType.GetBuiltInSimpleType(XmlTypeCode.Boolean).QualifiedName;
			}
			else if(Type == typeof(int))
			{
				Element.SchemaTypeName = XmlSchemaType.GetBuiltInSimpleType(XmlTypeCode.Int).QualifiedName;
			}
			else if(Type == typeof(float))
			{
				Element.SchemaTypeName = XmlSchemaType.GetBuiltInSimpleType(XmlTypeCode.Float).QualifiedName;
			}
			else if(Type == typeof(double))
			{
				Element.SchemaTypeName = XmlSchemaType.GetBuiltInSimpleType(XmlTypeCode.Double).QualifiedName;
			}
			else if(Type.IsEnum)
			{
				XmlSchemaSimpleTypeRestriction Restriction = new XmlSchemaSimpleTypeRestriction();
				Restriction.BaseTypeName = XmlSchemaType.GetBuiltInSimpleType(XmlTypeCode.String).QualifiedName;

				foreach(string EnumName in Enum.GetNames(Type))
				{
					XmlSchemaEnumerationFacet Facet = new XmlSchemaEnumerationFacet();
					Facet.Value = EnumName;
					Restriction.Facets.Add(Facet);
				}

				XmlSchemaSimpleType EnumType = new XmlSchemaSimpleType();
				EnumType.Content = Restriction;
				Element.SchemaType = EnumType;
			}
			else if(Type == typeof(string[]))
			{
				XmlSchemaElement ItemElement = new XmlSchemaElement();
				ItemElement.Name = "Item";
				ItemElement.SchemaTypeName = XmlSchemaType.GetBuiltInSimpleType(XmlTypeCode.String).QualifiedName;
				ItemElement.MinOccurs = 0;
				ItemElement.MaxOccursString = "unbounded";

				XmlSchemaSequence Sequence = new XmlSchemaSequence();
				Sequence.Items.Add(ItemElement);

				XmlSchemaComplexType ArrayType = new XmlSchemaComplexType();
				ArrayType.Particle = Sequence;
				Element.SchemaType = ArrayType;
			}
			else
			{
				throw new Exception("Unsupported field type for XmlConfigFile attribute");
			}
			return Element;
		}

		/// <summary>
		/// Writes a schema to the given location. Avoids writing it if the file is identical.
		/// </summary>
		/// <param name="Schema">The schema to be written</param>
		/// <param name="Location">Location to write to</param>
		static void WriteSchema(XmlSchema Schema, FileReference Location)
		{
			XmlWriterSettings Settings = new XmlWriterSettings();
			Settings.Indent = true;
			Settings.IndentChars = "\t";
			Settings.NewLineChars = Environment.NewLine;
			Settings.OmitXmlDeclaration = true;

			if(CachedSchemaSerializer == null)
			{
				CachedSchemaSerializer = XmlSerializer.FromTypes(new Type[] { typeof(XmlSchema) })[0];
			}

			StringBuilder Output = new StringBuilder();
			Output.AppendLine("<?xml version=\"1.0\" encoding=\"utf-8\"?>");
			using(XmlWriter Writer = XmlWriter.Create(Output, Settings))
			{
				XmlSerializerNamespaces Namespaces = new XmlSerializerNamespaces();
				Namespaces.Add("", "http://www.w3.org/2001/XMLSchema");
				CachedSchemaSerializer.Serialize(Writer, Schema, Namespaces);
			}

			string OutputText = Output.ToString();
			if(!FileReference.Exists(Location) || File.ReadAllText(Location.FullName) != OutputText)
			{
				DirectoryReference.CreateDirectory(Location.Directory);
				File.WriteAllText(Location.FullName, OutputText);
			}
		}
		
		/// <summary>
		/// Reads an XML config file and merges it to the given cache
		/// </summary>
		/// <param name="Location">Location to read from</param>
		/// <param name="CategoryToFields">Lookup for configurable fields by category</param>
		/// <param name="TypeToValues">Map of types to fields and their associated values</param>
		/// <param name="Schema">Schema to validate against</param>
		/// <returns>True if the file was read successfully</returns>
		static bool TryReadFile(FileReference Location, Dictionary<string, Dictionary<string, FieldInfo>> CategoryToFields, Dictionary<Type, Dictionary<FieldInfo, object>> TypeToValues, XmlSchema Schema)
		{
			// Read the XML file, and validate it against the schema
			XmlConfigFile ConfigFile;
			if(!XmlConfigFile.TryRead(Location, Schema, out ConfigFile))
			{
				return false;
			}

			// Parse the document
			foreach(XmlElement CategoryElement in ConfigFile.DocumentElement.ChildNodes.OfType<XmlElement>())
			{
				Dictionary<string, FieldInfo> NameToField = CategoryToFields[CategoryElement.Name];
				foreach(XmlElement KeyElement in CategoryElement.ChildNodes.OfType<XmlElement>())
				{
					FieldInfo Field;
					if(NameToField.TryGetValue(KeyElement.Name, out Field))
					{
						// Parse the corresponding value
						object Value;
						if(Field.FieldType == typeof(string[]))
						{
							Value = KeyElement.ChildNodes.OfType<XmlElement>().Where(x => x.Name == "Item").Select(x => x.InnerText).ToArray();
						}
						else
						{
							Value = ParseValue(Field.FieldType, KeyElement.InnerText);
						}

						// Add it to the set of values for the type containing this field
						Dictionary<FieldInfo, object> FieldToValue;
						if(!TypeToValues.TryGetValue(Field.DeclaringType, out FieldToValue))
						{
							FieldToValue = new Dictionary<FieldInfo, object>();
							TypeToValues.Add(Field.DeclaringType, FieldToValue);
						}
						FieldToValue[Field] = Value;
					}
				}
			}
			return true;
		}

		/// <summary>
		/// Parse the value for a field from its text based representation in an XML file
		/// </summary>
		/// <param name="FieldType">The type of field being read</param>
		/// <param name="Text">Text to parse</param>
		/// <returns>The object that was parsed</returns>
		static object ParseValue(Type FieldType, string Text)
		{
			if(FieldType == typeof(string))
			{
				return Text;
			}
			else if(FieldType == typeof(bool))
			{
				return (Text == "1" || Text.Equals("true", StringComparison.InvariantCultureIgnoreCase));
			}
			else if(FieldType == typeof(int))
			{
				return Int32.Parse(Text);
			}
			else if(FieldType == typeof(float))
			{
				return Single.Parse(Text);
			}
			else if(FieldType == typeof(double))
			{
				return Double.Parse(Text);
			}
			else if(FieldType.IsEnum)
			{
				return Enum.Parse(FieldType, Text);
			}
			else
			{
				throw new Exception(String.Format("Unsupported config type '{0}'", FieldType.Name));
			}
		}

		/// <summary>
		/// Checks that the given cache file exists and is newer than the given input files, and attempts to read it. Verifies that the resulting cache was created
		/// from the same input files in the same order.
		/// </summary>
		/// <param name="CacheFile">Path to the config cache file</param>
		/// <param name="InputFiles">The expected set of input files in the cache</param>
		/// <returns>True if the cache was valid and could be read, false otherwise.</returns>
		static bool IsCacheUpToDate(FileReference CacheFile, FileReference[] InputFiles)
		{
			// Always rebuild if the cache doesn't exist
			if(!FileReference.Exists(CacheFile))
			{
				return false;
			}

			// Get the timestamp for the cache
			DateTime CacheWriteTime = File.GetLastWriteTimeUtc(CacheFile.FullName);

			// Always rebuild if this executable is newer
			if(File.GetLastWriteTimeUtc(Assembly.GetExecutingAssembly().Location) > CacheWriteTime)
			{
				return false;
			}

			// Check if any of the input files are newer than the cache
			foreach(FileReference InputFile in InputFiles)
			{
				if(File.GetLastWriteTimeUtc(InputFile.FullName) > CacheWriteTime)
				{
					return false;
				}
			}

			// Otherwise, it's up to date
			return true;
		}

		/// <summary>
		/// Generates documentation files for the available settings, by merging the XML documentation from the compiler.
		/// </summary>
		/// <param name="OutputFile">The documentation file to write</param>
		public static void WriteDocumentation(FileReference OutputFile)
		{
			// Find all the configurable types
			List<Type> ConfigTypes = FindConfigurableTypes();

			// Find all the configurable fields from the given types
			Dictionary<string, Dictionary<string, FieldInfo>> CategoryToFields = new Dictionary<string, Dictionary<string, FieldInfo>>();
			FindConfigurableFields(ConfigTypes, CategoryToFields);

			// Get the path to the XML documentation
			FileReference InputDocumentationFile = new FileReference(Assembly.GetExecutingAssembly().Location).ChangeExtension(".xml");
			if(!FileReference.Exists(InputDocumentationFile))
			{
				throw new BuildException("Generated assembly documentation not found at {0}.", InputDocumentationFile);
			}

			// Get the current engine version for versioning the page
			BuildVersion Version;
			if(!BuildVersion.TryRead(BuildVersion.GetDefaultFileName(), out Version))
			{
				throw new BuildException("Unable to read the current build version");
			}

			// Read the documentation
			XmlDocument InputDocumentation = new XmlDocument();
			InputDocumentation.Load(InputDocumentationFile.FullName);

			// Make sure we can write to the output file
			FileReference.MakeWriteable(OutputFile);

			// Generate the UDN documentation file
			using (StreamWriter Writer = new StreamWriter(OutputFile.FullName))
			{
				Writer.WriteLine("Availability: NoPublish");
				Writer.WriteLine("Title: Build Configuration Properties Page");
				Writer.WriteLine("Crumbs:");
				Writer.WriteLine("Description: This is a procedurally generated markdown page.");
				Writer.WriteLine("Version: {0}.{1}", Version.MajorVersion, Version.MinorVersion);
				Writer.WriteLine("");

				foreach(KeyValuePair<string, Dictionary<string, FieldInfo>> CategoryPair in CategoryToFields)
				{
					string CategoryName = CategoryPair.Key;
					Writer.WriteLine("### {0}", CategoryName);
					Writer.WriteLine();

					Dictionary<string, FieldInfo> Fields = CategoryPair.Value;
					foreach(KeyValuePair<string, FieldInfo> FieldPair in Fields)
					{
						string FieldName = FieldPair.Key;

						FieldInfo Field = FieldPair.Value;
						XmlNode Node = InputDocumentation.SelectSingleNode(String.Format("//member[@name='F:{0}.{1}']/summary", Field.DeclaringType.FullName, Field.Name));
						if(Node != null)
						{
							// Reflow the comments into paragraphs, assuming that each paragraph will be separated by a blank line
							List<string> Lines = new List<string>(Node.InnerText.Trim().Split('\n').Select(x => x.Trim()));
							for(int Idx = Lines.Count - 1; Idx > 0; Idx--)
							{
								if(Lines[Idx - 1].Length > 0 && !Lines[Idx].StartsWith("*") && !Lines[Idx].StartsWith("-"))
								{
									Lines[Idx - 1] += " " + Lines[Idx];
									Lines.RemoveAt(Idx);
								}
							}

							// Write the result to the .udn file
							if(Lines.Count > 0)
							{
								Writer.WriteLine("$ {0} : {1}", FieldName, Lines[0]);
								for(int Idx = 1; Idx < Lines.Count; Idx++)
								{
									if(Lines[Idx].StartsWith("*") || Lines[Idx].StartsWith("-"))
									{
										Writer.WriteLine("        * {0}", Lines[Idx].Substring(1).TrimStart());
									}
									else
									{
										Writer.WriteLine("    * {0}", Lines[Idx]);
									}
								}
								Writer.WriteLine();
							}
						}
					}
				}
			}

			// Success!
			Log.TraceInformation("Written documentation to {0}.", OutputFile);
		}
	}
}
