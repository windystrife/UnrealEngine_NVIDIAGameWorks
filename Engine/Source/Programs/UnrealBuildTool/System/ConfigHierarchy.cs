using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Types of config file hierarchy
	/// </summary>
	public enum ConfigHierarchyType
	{
		/// <summary>
		/// BaseGame.ini, DefaultGame.ini, etc...
		/// </summary>
		Game,

		/// <summary>
		/// BaseEngine.ini, DefaultEngine.ini, etc...
		/// </summary>
		Engine,

		/// <summary>
		/// BaseEditorPerProjectUserSettings.ini, DefaultEditorPerProjectUserSettings.ini, etc..
		/// </summary>
		EditorPerProjectUserSettings,

		/// <summary>
		/// BaseEncryption.ini, DefaultEncryption.ini, etc..
		/// </summary>
		Encryption,

		/// <summary>
		/// BaseEditorSettings.ini, DefaultEditorSettings.ini, etc...
		/// </summary>
		EditorSettings,
	}

	/// <summary>
	/// Stores a set of merged key/value pairs for a config section
	/// </summary>
	public class ConfigHierarchySection
	{
		/// <summary>
		/// Map of key names to their values
		/// </summary>
		Dictionary<string, List<string>> KeyToValue = new Dictionary<string, List<string>>(StringComparer.InvariantCultureIgnoreCase);

		/// <summary>
		/// Construct a merged config section from the given per-file config sections
		/// </summary>
		/// <param name="FileSections">Config sections from individual files</param>
		internal ConfigHierarchySection(IEnumerable<ConfigFileSection> FileSections)
		{
			foreach(ConfigFileSection FileSection in FileSections)
			{
				foreach(ConfigLine Line in FileSection.Lines)
				{
					// Find or create the values for this key
					List<string> Values;
					if(KeyToValue.TryGetValue(Line.Key, out Values))
					{
						// Update the existing list
						if(Line.Action == ConfigLineAction.Set)
						{
							Values.Clear();
							Values.Add(Line.Value);
						}
						else if(Line.Action == ConfigLineAction.Add)
						{
							Values.Add(Line.Value);
						}
						else if(Line.Action == ConfigLineAction.Remove)
						{
							Values.RemoveAll(x => x.Equals(Line.Value, StringComparison.InvariantCultureIgnoreCase));
						}
					}
					else
					{
						// If it's a set or add action, create and add a new list
						if(Line.Action == ConfigLineAction.Set || Line.Action == ConfigLineAction.Add)
						{
							Values = new List<string>();
							Values.Add(Line.Value);
							KeyToValue.Add(Line.Key, Values);
						}
					}
				}
			}
		}

		/// <summary>
		/// Returns a list of key names
		/// </summary>
		public IEnumerable<string> KeyNames
		{
			get { return KeyToValue.Keys; }
		}

		/// <summary>
		/// Tries to find the value for a given key
		/// </summary>
		/// <param name="KeyName">The key name to search for</param>
		/// <param name="Value">On success, receives the corresponding value</param>
		/// <returns>True if the key was found, false otherwise</returns>
		public bool TryGetValue(string KeyName, out string Value)
		{
			List<string> ValuesList;
			if(KeyToValue.TryGetValue(KeyName, out ValuesList) && ValuesList.Count > 0)
			{
				Value = ValuesList[0];
				return true;
			}
			else
			{
				Value = null;
				return false;
			}
		}

		/// <summary>
		/// Tries to find the values for a given key
		/// </summary>
		/// <param name="KeyName">The key name to search for</param>
		/// <param name="Values">On success, receives a list of the corresponding values</param>
		/// <returns>True if the key was found, false otherwise</returns>
		public bool TryGetValues(string KeyName, out IEnumerable<string> Values)
		{
			List<string> ValuesList;
			if(KeyToValue.TryGetValue(KeyName, out ValuesList))
			{
				Values = ValuesList;
				return true;
			}
			else
			{
				Values = null;
				return false;
			}
		}
	}

	/// <summary>
	/// Encapsulates a hierarchy of config files, merging sections from them together on request 
	/// </summary>
	public class ConfigHierarchy
	{
		/// <summary>
		/// Array of 
		/// </summary>
		ConfigFile[] Files;

		/// <summary>
		/// Cache of requested config sections
		/// </summary>
		Dictionary<string, ConfigHierarchySection> NameToSection = new Dictionary<string, ConfigHierarchySection>(StringComparer.InvariantCultureIgnoreCase);

		/// <summary>
		/// Construct a config hierarchy from the given files
		/// </summary>
		/// <param name="Files">Set of files to include (in order)</param>
		public ConfigHierarchy(IEnumerable<ConfigFile> Files)
		{
			this.Files = Files.ToArray();
		}

		/// <summary>
		/// Finds a config section with the given name
		/// </summary>
		/// <param name="SectionName">Name of the section to look for</param>
		/// <returns>The merged config section</returns>
		public ConfigHierarchySection FindSection(string SectionName)
		{
			ConfigHierarchySection Section;
			if(!NameToSection.TryGetValue(SectionName, out Section))
			{
				// Find all the raw sections from the file hierarchy
				List<ConfigFileSection> RawSections = new List<ConfigFileSection>();
				foreach(ConfigFile File in Files)
				{
					ConfigFileSection RawSection;
					if(File.TryGetSection(SectionName, out RawSection))
					{
						RawSections.Add(RawSection);
					}
				}

				// Merge them together and add it to the cache
				Section = new ConfigHierarchySection(RawSections);
				NameToSection.Add(SectionName, Section);
			}
			return Section;
		}

		/// <summary>
		/// Legacy function for ease of transition from ConfigCacheIni to ConfigHierarchy. Gets a bool with the given key name.
		/// </summary>
		/// <param name="SectionName">Section name</param>
		/// <param name="KeyName">Key name</param>
		/// <param name="Value">Value associated with the specified key. If the key has more than one value, only the first one is returned</param>
		/// <returns>True if the key exists</returns>
		public bool GetBool(string SectionName, string KeyName, out bool Value)
		{
			return TryGetValue(SectionName, KeyName, out Value);
		}

		/// <summary>
		/// Legacy function for ease of transition from ConfigCacheIni to ConfigHierarchy. Gets a string with the given key name, returning an empty string on failure.
		/// </summary>
		/// <param name="SectionName">Section name</param>
		/// <param name="KeyName">Key name</param>
		/// <param name="Values">Value associated with the specified key. If the key has more than one value, only the first one is returned</param>
		/// <returns>True if the key exists</returns>
		public bool GetArray(string SectionName, string KeyName, out List<string> Values)
		{
			IEnumerable<string> ValuesEnumerable;
			if(TryGetValues(SectionName, KeyName, out ValuesEnumerable))
			{
				Values = ValuesEnumerable.ToList();
				return true;
			}
			else
			{
				Values = null;
				return false;
			}
		}

		/// <summary>
		/// Legacy function for ease of transition from ConfigCacheIni to ConfigHierarchy. Gets a string with the given key name, returning an empty string on failure.
		/// </summary>
		/// <param name="SectionName">Section name</param>
		/// <param name="KeyName">Key name</param>
		/// <param name="Value">Value associated with the specified key. If the key has more than one value, only the first one is returned</param>
		/// <returns>True if the key exists</returns>
		public bool GetString(string SectionName, string KeyName, out string Value)
		{
			if(TryGetValue(SectionName, KeyName, out Value))
			{
				return true;
			}
			else
			{
				Value = "";
				return false;
			}
		}

		/// <summary>
		/// Legacy function for ease of transition from ConfigCacheIni to ConfigHierarchy. Gets an int with the given key name.
		/// </summary>
		/// <param name="SectionName">Section name</param>
		/// <param name="KeyName">Key name</param>
		/// <param name="Value">Value associated with the specified key. If the key has more than one value, only the first one is returned</param>
		/// <returns>True if the key exists</returns>
		public bool GetInt32(string SectionName, string KeyName, out int Value)
		{
			return TryGetValue(SectionName, KeyName, out Value);
		}

		/// <summary>
		/// Gets a single string value associated with the specified key.
		/// </summary>
		/// <param name="SectionName">Section name</param>
		/// <param name="KeyName">Key name</param>
		/// <param name="Value">Value associated with the specified key. If the key has more than one value, only the first one is returned</param>
		/// <returns>True if the key exists</returns>
		public bool TryGetValue(string SectionName, string KeyName, out string Value)
		{
			return FindSection(SectionName).TryGetValue(KeyName, out Value);
		}

		/// <summary>
		/// Gets a single bool value associated with the specified key.
		/// </summary>
		/// <param name="SectionName">Section name</param>
		/// <param name="KeyName">Key name</param>
		/// <param name="Value">Value associated with the specified key. If the key has more than one value, only the first one is returned</param>
		/// <returns>True if the key exists</returns>
		public bool TryGetValue(string SectionName, string KeyName, out bool Value)
		{
			string Text;
			if(!TryGetValue(SectionName, KeyName, out Text))
			{
				Value = false;
				return false;
			}
			return TryParse(Text, out Value);
		}

		/// <summary>
		/// Gets a single Int32 value associated with the specified key.
		/// </summary>
		/// <param name="SectionName">Section name</param>
		/// <param name="KeyName">Key name</param>
		/// <param name="Value">Value associated with the specified key. If the key has more than one value, only the first one is returned</param>
		/// <returns>True if the key exists</returns>
		public bool TryGetValue(string SectionName, string KeyName, out int Value)
		{
			string Text;
			if(!TryGetValue(SectionName, KeyName, out Text))
			{
				Value = 0;
				return false;
			}
			return TryParse(Text, out Value);
		}

		/// <summary>
		/// Gets a single GUID value associated with the specified key.
		/// </summary>
		/// <param name="SectionName">Section name</param>
		/// <param name="KeyName">Key name</param>
		/// <param name="Value">Value associated with the specified key. If the key has more than one value, only the first one is returned</param>
		/// <returns>True if the key exists</returns>
		public bool TryGetValue(string SectionName, string KeyName, out Guid Value)
		{
			string Text;
			if(!TryGetValue(SectionName, KeyName, out Text))
			{
				Value = Guid.Empty;
				return false;
			}
			return TryParse(Text, out Value);
		}

		/// <summary>
		/// Gets a single-precision floating point value associated with the specified key.
		/// </summary>
		/// <param name="SectionName">Section name</param>
		/// <param name="KeyName">Key name</param>
		/// <param name="Value">Value associated with the specified key. If the key has more than one value, only the first one is returned</param>
		/// <returns>True if the key exists</returns>
		public bool TryGetValue(string SectionName, string KeyName, out float Value)
		{
			string Text;
			if(!TryGetValue(SectionName, KeyName, out Text))
			{
				Value = 0;
				return false;
			}
			return TryParse(Text, out Value);
		}

		/// <summary>
		/// Gets a double-precision floating point value associated with the specified key.
		/// </summary>
		/// <param name="SectionName">Section name</param>
		/// <param name="KeyName">Key name</param>
		/// <param name="Value">Value associated with the specified key. If the key has more than one value, only the first one is returned</param>
		/// <returns>True if the key exists</returns>
		public bool TryGetValue(string SectionName, string KeyName, out double Value)
		{
			string Text;
			if(!TryGetValue(SectionName, KeyName, out Text))
			{
				Value = 0;
				return false;
			}
			return TryParse(Text, out Value);
		}

		/// <summary>
		/// Gets all values associated with the specified key
		/// </summary>
		/// <param name="SectionName">Section where the key is located</param>
		/// <param name="KeyName">Key name</param>
		/// <param name="Values">Copy of the list containing all values associated with the specified key</param>
		/// <returns>True if the key exists</returns>
		public bool TryGetValues(string SectionName, string KeyName, out IEnumerable<string> Values)
		{
			return FindSection(SectionName).TryGetValues(KeyName, out Values);
		}

		/// <summary>
		/// Parse a string as a boolean value
		/// </summary>
		/// <param name="Text">The text to parse</param>
		/// <param name="Value">The parsed value, if successful</param>
		/// <returns>True if the text was parsed, false otherwise</returns>
		static public bool TryParse(string Text, out bool Value)
		{
			// C# Boolean type expects "False" or "True" but since we're not case sensitive, we need to suppor that manually
			if (Text == "1" || Text.Equals("true", StringComparison.InvariantCultureIgnoreCase))
			{
				Value = true;
				return true;
			}
			else if (Text == "0" || Text.Equals("false", StringComparison.InvariantCultureIgnoreCase))
			{
				Value = false;
				return true;
			}
			else
			{
				Value = false;
				return false;
			}
		}

		/// <summary>
		/// Parse a string as an integer value
		/// </summary>
		/// <param name="Text">The text to parse</param>
		/// <param name="Value">The parsed value, if successful</param>
		/// <returns>True if the text was parsed, false otherwise</returns>
		static public bool TryParse(string Text, out int Value)
		{
			return Int32.TryParse(Text, out Value);
		}

		/// <summary>
		/// Parse a string as a GUID value
		/// </summary>
		/// <param name="Text">The text to parse</param>
		/// <param name="Value">The parsed value, if successful</param>
		/// <returns>True if the text was parsed, false otherwise</returns>
		public static bool TryParse(string Text, out Guid Value)
		{
			if (Text.Contains("A=") && Text.Contains("B=") && Text.Contains("C=") && Text.Contains("D="))
			{
				char[] Separators = new char[] { '(', ')', '=', ',', ' ', 'A', 'B', 'C', 'D' };
				string[] ComponentValues = Text.Split(Separators, StringSplitOptions.RemoveEmptyEntries);
				if (ComponentValues.Length == 4)
				{
					StringBuilder HexString = new StringBuilder();
					for (int ComponentIndex = 0; ComponentIndex < 4; ComponentIndex++)
					{
						int IntegerValue;
						if(!Int32.TryParse(ComponentValues[ComponentIndex], out IntegerValue))
						{
							Value = Guid.Empty;
							return false;
						}
						HexString.Append(IntegerValue.ToString("X8"));
					}
					Text = HexString.ToString();
				}
			}
			return Guid.TryParseExact(Text, "N", out Value);
		}

		/// <summary>
		/// Parse a string as a single-precision floating point value
		/// </summary>
		/// <param name="Text">The text to parse</param>
		/// <param name="Value">The parsed value, if successful</param>
		/// <returns>True if the text was parsed, false otherwise</returns>
		public static bool TryParse(string Text, out float Value)
		{
			if(Text.EndsWith("f"))
			{
				return Single.TryParse(Text.Substring(0, Text.Length - 1), out Value);
			}
			else
			{
				return Single.TryParse(Text, out Value);
			}
		}

		/// <summary>
		/// Parse a string as a double-precision floating point value
		/// </summary>
		/// <param name="Text">The text to parse</param>
		/// <param name="Value">The parsed value, if successful</param>
		/// <returns>True if the text was parsed, false otherwise</returns>
		public static bool TryParse(string Text, out double Value)
		{
			if(Text.EndsWith("f"))
			{
				return Double.TryParse(Text.Substring(0, Text.Length - 1), out Value);
			}
			else
			{
				return Double.TryParse(Text, out Value);
			}
		}

		/// <summary>
		/// Attempts to parse the given line as a UE4 config object (eg. (Name="Foo",Number=1234)).
		/// </summary>
		/// <param name="Line">Line of text to parse</param>
		/// <param name="Properties">Receives key/value pairs for the config object</param>
		/// <returns>True if an object was parsed, false otherwise</returns>
		public static bool TryParse(string Line, out Dictionary<string, string> Properties)
		{
			// Convert the string to a zero-terminated array, to make parsing easier.
			char[] Chars = new char[Line.Length + 1];
			Line.CopyTo(0, Chars, 0, Line.Length);

			// Get the opening paren
			int Idx = 0;
			while(Char.IsWhiteSpace(Chars[Idx]))
			{
				Idx++;
			}
			if(Chars[Idx] != '(')
			{
				Properties = null;
				return false;
			}

			// Read to the next token
			Idx++;
			while(Char.IsWhiteSpace(Chars[Idx]))
			{
				Idx++;
			}

			// Create the dictionary to receive the new properties
			Dictionary<string, string> NewProperties = new Dictionary<string, string>();

			// Read a sequence of key/value pairs
			StringBuilder Value = new StringBuilder();
			if(Chars[Idx] != ')')
			{
				for (;;)
				{
					// Find the end of the name
					int NameIdx = Idx;
					while(Char.IsLetterOrDigit(Chars[Idx]) || Chars[Idx] == '_')
					{
						Idx++;
					}
					if(Idx == NameIdx)
					{
						Properties = null;
						return false;
					}

					// Extract the key string, and make sure it hasn't already been added
					string Key = new string(Chars, NameIdx, Idx - NameIdx);
					if(NewProperties.ContainsKey(Key))
					{
						Properties = null;
						return false;
					}

					// Consume the equals character
					while(Char.IsWhiteSpace(Chars[Idx]))
					{
						Idx++;
					}
					if(Chars[Idx] != '=')
					{
						Properties = null;
						return false;
					}

					// Move to the value
					Idx++;
					while (Char.IsWhiteSpace(Chars[Idx]))
					{
						Idx++;
					}

					// Parse the value
					Value.Clear();
					if (Char.IsLetterOrDigit(Chars[Idx]) || Chars[Idx] == '_')
					{
						while (Char.IsLetterOrDigit(Chars[Idx]) || Chars[Idx] == '_' || Chars[Idx] == '.')
						{
							Value.Append(Chars[Idx]);
							Idx++;
						}
					}
					else if (Chars[Idx] == '\"')
					{
						Idx++;
						for(; Chars[Idx] != '\"'; Idx++)
						{
							if (Chars[Idx] == '\0')
							{
								Properties = null;
								return false;
							}
							else
							{
								Value.Append(Chars[Idx]);
							}
						}
						Idx++;
					}
					else if (Chars[Idx] == '(')
					{
						Value.Append(Chars[Idx++]);

						bool bInQuotes = false;
						for (int Nesting = 1; Nesting > 0; Idx++)
						{
							if (Chars[Idx] == '\0')
							{
								Properties = null;
								return false;
							}
							else if (Chars[Idx] == '(' && !bInQuotes)
							{
								Nesting++;
							}
							else if (Chars[Idx] == ')' && !bInQuotes)
							{
								Nesting--;
							}
							else if (Chars[Idx] == '\"' || Chars[Idx] == '\'')
							{
								bInQuotes ^= true;
							}
							Value.Append(Chars[Idx]);
						}
					}
					else
					{
						Properties = null;
						return false;
					}

					// Extract the value string
					NewProperties[Key] = Value.ToString();

					// Move to the separator
					while(Char.IsWhiteSpace(Chars[Idx]))
					{
						Idx++;
					}
					if(Chars[Idx] == ')')
					{
						break;
					}
					if(Chars[Idx] != ',')
					{
						Properties = null;
						return false;
					}

					// Move to the next field
					Idx++;
					while (Char.IsWhiteSpace(Chars[Idx]))
					{
						Idx++;
					}
				}
			}

			// Make sure we're at the end of the string
			Idx++;
			while(Char.IsWhiteSpace(Chars[Idx]))
			{
				Idx++;
			}
			if(Chars[Idx] != '\0')
			{
				Properties = null;
				return false;
			}

			Properties = NewProperties;
			return true;
		}

		/// <summary>
		/// Returns a list of INI filenames for the given project
		/// </summary>
		public static IEnumerable<FileReference> EnumerateConfigFileLocations(ConfigHierarchyType Type, DirectoryReference ProjectDir, UnrealTargetPlatform Platform)
		{
			string BaseIniName = Enum.GetName(typeof(ConfigHierarchyType), Type);
			string PlatformName = GetIniPlatformName(Platform);

			// Engine/Config/Base.ini (included in every ini type, required)
			yield return FileReference.Combine(UnrealBuildTool.EngineDirectory, "Config", "Base.ini");

			// Engine/Config/Base* ini
			yield return FileReference.Combine(UnrealBuildTool.EngineDirectory, "Config", "Base" + BaseIniName + ".ini");

			if (Platform != UnrealTargetPlatform.Unknown)
			{
				// Engine/Config/Platform/BasePlatform* ini
				yield return FileReference.Combine(UnrealBuildTool.EngineDirectory, "Config", PlatformName, "Base" + PlatformName + BaseIniName + ".ini");
			}

			// Engine/Config/NotForLicensees/Base* ini
			yield return FileReference.Combine(UnrealBuildTool.EngineDirectory, "Config", "NotForLicensees", "Base" + BaseIniName + ".ini");

			// NOTE: 4.7: See comment in GetSourceIniHierarchyFilenames()
			// Engine/Config/NoRedist/Base* ini
			// yield return Path.Combine(EngineDirectory, "Config", "NoRedist", "Base" + BaseIniName + ".ini");

			if (ProjectDir != null)
			{
				// Game/Config/Default* ini
				yield return FileReference.Combine(ProjectDir, "Config", "Default" + BaseIniName + ".ini");

				// Game/Config/NotForLicensees/Default* ini
				yield return FileReference.Combine(ProjectDir, "Config", "NotForLicensees", "Default" + BaseIniName + ".ini");

				// Game/Config/NoRedist/Default* ini
				yield return FileReference.Combine(ProjectDir, "Config", "NoRedist", "Default" + BaseIniName + ".ini");
			}

			if (Platform != UnrealTargetPlatform.Unknown)
			{
				// Engine/Config/Platform/Platform* ini
				yield return FileReference.Combine(UnrealBuildTool.EngineDirectory, "Config", PlatformName, PlatformName + BaseIniName + ".ini");

				// Engine/Config/NotForLicensees/Platform/Platform* ini
				yield return FileReference.Combine(UnrealBuildTool.EngineDirectory, "Config", "NotForLicensees", PlatformName, PlatformName + BaseIniName + ".ini");

				// Engine/Config/NoRedist/Platform/Platform* ini
				yield return FileReference.Combine(UnrealBuildTool.EngineDirectory, "Config", "NoRedist", PlatformName, PlatformName + BaseIniName + ".ini");

				if (ProjectDir != null)
				{
					// Game/Config/Platform/Platform* ini
					yield return FileReference.Combine(ProjectDir, "Config", PlatformName, PlatformName + BaseIniName + ".ini");

					// Engine/Config/NotForLicensees/Platform/Platform* ini
					yield return FileReference.Combine(ProjectDir, "Config", "NotForLicensees", PlatformName, PlatformName + BaseIniName + ".ini");

					// Engine/Config/NoRedist/Platform/Platform* ini
					yield return FileReference.Combine(ProjectDir, "Config", "NoRedist", PlatformName, PlatformName + BaseIniName + ".ini");
				}
			}

			DirectoryReference UserSettingsFolder = Utils.GetUserSettingDirectory(); // Match FPlatformProcess::UserSettingsDir()
			if(UserSettingsFolder != null)
			{
				// <AppData>/UE4/EngineConfig/User* ini
				yield return FileReference.Combine(UserSettingsFolder, "Unreal Engine", "Engine", "Config", "User" + BaseIniName + ".ini");
			}

			// Some user accounts (eg. SYSTEM on Windows) don't have a home directory. Ignore them if Environment.GetFolderPath() returns an empty string.
			string PersonalFolder = Environment.GetFolderPath(Environment.SpecialFolder.Personal);
			if (!String.IsNullOrEmpty(PersonalFolder))
			{
				DirectoryReference PersonalConfigFolder; // Match FPlatformProcess::UserDir()
				if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac)
				{
					PersonalConfigFolder = DirectoryReference.Combine(new DirectoryReference(PersonalFolder), "Documents");
				}
				else if (Environment.OSVersion.Platform == PlatformID.Unix)
				{
					PersonalConfigFolder = DirectoryReference.Combine(new DirectoryReference(PersonalFolder), "Documents");
				}
				else
				{
					PersonalConfigFolder = new DirectoryReference(PersonalFolder);
				}

				// <Documents>/UE4/EngineConfig/User* ini
				yield return FileReference.Combine(PersonalConfigFolder, "Unreal Engine", "Engine", "Config", "User" + BaseIniName + ".ini");
			}

			// Game/Config/User* ini
			if (ProjectDir != null)
			{
				yield return FileReference.Combine(ProjectDir, "Config", "User" + BaseIniName + ".ini");
			}
		}

		/// <summary>
		/// Returns the platform name to use as part of platform-specific config files
		/// </summary>
		public static string GetIniPlatformName(UnrealTargetPlatform TargetPlatform)
		{
			if (TargetPlatform == UnrealTargetPlatform.Win32 || TargetPlatform == UnrealTargetPlatform.Win64)
			{
				return "Windows";
			}
			else
			{
				return Enum.GetName(typeof(UnrealTargetPlatform), TargetPlatform);
			}
		}
	}
}
