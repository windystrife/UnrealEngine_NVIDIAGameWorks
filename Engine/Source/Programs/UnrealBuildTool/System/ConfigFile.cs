using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Tools.DotNETCommon;
using UnrealBuildTool;

namespace UnrealBuildTool
{
	/// <summary>
	/// Specifies the action to take for a config line, as denoted by its prefix.
	/// </summary>
	public enum ConfigLineAction
	{
		/// <summary>
		/// Assign the value to the key
		/// </summary>
		Set,

		/// <summary>
		/// Add the value to the key (denoted with +X=Y in config files)
		/// </summary>
		Add,

		/// <summary>
		/// Remove the value to the key (denoted with -X=Y in config files)
		/// </summary>
		Remove,
	}

	/// <summary>
	/// Contains a pre-parsed raw config line, consisting of action, key and value components.
	/// </summary>
	public class ConfigLine
	{
		/// <summary>
		/// The action to take when merging this key/value pair with an existing value
		/// </summary>
		public ConfigLineAction Action;

		/// <summary>
		/// Name of the key to modify
		/// </summary>
		public string Key;

		/// <summary>
		/// Value to assign to the key
		/// </summary>
		public string Value;

		/// <summary>
		/// Constructor.
		/// </summary>
		/// <param name="Action">Action to take when merging this key/value pair with an existing value</param>
		/// <param name="Key">Name of the key to modify</param>
		/// <param name="Value">Value to assign</param>
		public ConfigLine(ConfigLineAction Action, string Key, string Value)
		{
			this.Action = Action;
			this.Key = Key;
			this.Value = Value;
		}

		/// <summary>
		/// Formats this object for the debugger
		/// </summary>
		/// <returns>The original config line</returns>
		public override string ToString()
		{
			string Prefix = (Action == ConfigLineAction.Add)? "+" : (Action == ConfigLineAction.Remove)? "-" : "";
			return String.Format("{0}{1}={2}", Prefix, Key, Value);
		}
	}

	/// <summary>
	/// Contains the lines which appeared in a config section, with all comments and whitespace removed
	/// </summary>
	public class ConfigFileSection
	{
		/// <summary>
		/// Name of the section
		/// </summary>
		public string Name;

		/// <summary>
		/// Lines which appeared in the config section
		/// </summary>
		public List<ConfigLine> Lines = new List<ConfigLine>();

		/// <summary>
		/// Construct an empty config section with the given name
		/// </summary>
		/// <param name="Name">Name of the config section</param>
		public ConfigFileSection(string Name)
		{
			this.Name = Name;
		}
	}

	/// <summary>
	/// Represents a single config file as stored on disk. 
	/// </summary>
	public class ConfigFile
	{
		/// <summary>
		/// Maps names to config sections
		/// </summary>
		Dictionary<string, ConfigFileSection> Sections = new Dictionary<string, ConfigFileSection>(StringComparer.InvariantCultureIgnoreCase);

		/// <summary>
		/// Reads config data from the given file.
		/// </summary>
		/// <param name="Location">File to read from</param>
		/// <param name="DefaultAction">The default action to take when encountering arrays without a '+' prefix</param>
		public ConfigFile(FileReference Location, ConfigLineAction DefaultAction = ConfigLineAction.Set)
		{
			using (StreamReader Reader = new StreamReader(Location.FullName))
			{
				ConfigFileSection CurrentSection = null;
				while(!Reader.EndOfStream)
				{
					// Find the first non-whitespace character
					string Line = Reader.ReadLine();
					for(int StartIdx = 0; StartIdx < Line.Length; StartIdx++)
					{
						if (Line[StartIdx] != ' ' && Line[StartIdx] != '\t')
						{
							// Find the last non-whitespace character. If it's an escaped newline, merge the following line with it.
							int EndIdx = Line.Length;
							for(; EndIdx > StartIdx; EndIdx--)
							{
								if(Line[EndIdx - 1] == '\\')
								{
									string NextLine = Reader.ReadLine();
									if(NextLine == null)
									{
										break;
									}
									Line += NextLine;
									EndIdx = Line.Length;
								}
								if(Line[EndIdx - 1] != ' ' && Line[EndIdx - 1] != '\t')
								{
									break;
								}
							}

							// Break out if we've got a comment
							if(Line[StartIdx] == ';')
							{
								break;
							}
							if(Line[StartIdx] == '/' && StartIdx + 1 < Line.Length && Line[StartIdx + 1] == '/')
							{
								break;
							}

							// Check if it's the start of a new section
							if(Line[StartIdx] == '[')
							{
								CurrentSection = null;
								if(Line[EndIdx - 1] == ']')
								{
									string SectionName = Line.Substring(StartIdx + 1, EndIdx - StartIdx - 2);
									if(!Sections.TryGetValue(SectionName, out CurrentSection))
									{
										CurrentSection = new ConfigFileSection(SectionName);
										Sections.Add(SectionName, CurrentSection);
									}
								}
								break;
							}

							// Otherwise add it to the current section or add a new one
							if(CurrentSection != null)
							{
								if(!TryAddConfigLine(CurrentSection, Line, StartIdx, EndIdx, DefaultAction))
								{
									Console.WriteLine("Couldn't parse '{0}' in {1} of {2}", Line, CurrentSection, Location.FullName);
								}
								break;
							}

							// Otherwise just ignore it
							break;
						}
					}
				}
			}
		}

		/// <summary>
		/// Reads config data from the given string.
		/// </summary>
		/// <param name="IniText">Single line string of config settings in the format [Section1]:Key1=Value1,[Section2]:Key2=Value2</param>
		/// <param name="DefaultAction">The default action to take when encountering arrays without a '+' prefix</param>
		public ConfigFile(string IniText, ConfigLineAction DefaultAction = ConfigLineAction.Set)
		{

			// Break into individual settings of the form [Section]:Key=Value
			string[] SettingLines = IniText.Split(new char[] { ',' });
			foreach (string Setting in SettingLines)
			{
				// Locate and break off the section name
				string SectionName = Setting.Remove(Setting.IndexOf(':')).Trim(new char[] { '[', ']' });
				if (SectionName.Length > 0)
				{
					ConfigFileSection CurrentSection = null;
					if (!Sections.TryGetValue(SectionName, out CurrentSection))
					{
						CurrentSection = new ConfigFileSection(SectionName);
						Sections.Add(SectionName, CurrentSection);
					}

					if (CurrentSection != null)
					{
						string IniKeyValue = Setting.Substring(Setting.IndexOf(':') + 1);
						if (!TryAddConfigLine(CurrentSection, IniKeyValue, 0, IniKeyValue.Length, DefaultAction))
						{
							Console.WriteLine("Couldn't parse '{0}'", IniKeyValue);
						}
					}
				}
			}
		}

		/// <summary>
		/// Try to parse a key/value pair from the given line, and add it to a config section
		/// </summary>
		/// <param name="Section">The section to receive the parsed config line</param>
		/// <param name="Line">Text to parse</param>
		/// <param name="StartIdx">Index of the first non-whitespace character in this line</param>
		/// <param name="EndIdx">Index of the last (exclusive) non-whitespace character in this line</param>
		/// <param name="DefaultAction">The default action to take if '+' or '-' is not specified on a config line</param>
		/// <returns>True if the line was parsed correctly, false otherwise</returns>
		static bool TryAddConfigLine(ConfigFileSection Section, string Line, int StartIdx, int EndIdx, ConfigLineAction DefaultAction)
		{
			// Find the '=' character separating key and value
			int EqualsIdx = Line.IndexOf('=', StartIdx, EndIdx - StartIdx);
			if(EqualsIdx == -1)
			{
				return false;
			}

			// Keep track of the start of the key name
			int KeyStartIdx = StartIdx;

			// Remove the '+' or '-' prefix, if present
			ConfigLineAction Action = DefaultAction;
			if(Line[KeyStartIdx] == '+' || Line[KeyStartIdx] == '-')
			{
				Action = (Line[KeyStartIdx] == '+')? ConfigLineAction.Add : ConfigLineAction.Remove;
				KeyStartIdx++;
				while(Line[KeyStartIdx] == ' ' || Line[KeyStartIdx] == '\t')
				{
					KeyStartIdx++;
				}
			}

			// Remove trailing spaces after the name of the key
			int KeyEndIdx = EqualsIdx;
			for(; KeyEndIdx > KeyStartIdx; KeyEndIdx--)
			{
				if(Line[KeyEndIdx - 1] != ' ' && Line[KeyEndIdx - 1] != '\t')
				{
					break;
				}
			}

			// Make sure there's a non-empty key name
			if(KeyStartIdx == EqualsIdx)
			{
				return false;
			}

			// Skip whitespace between the '=' and the start of the value
			int ValueStartIdx = EqualsIdx + 1;
			for(; ValueStartIdx < EndIdx; ValueStartIdx++)
			{
				if(Line[ValueStartIdx] != ' ' && Line[ValueStartIdx] != '\t')
				{
					break;
				}
			}

			// Strip quotes around the value if present
			int ValueEndIdx = EndIdx;
			if(ValueEndIdx >= ValueStartIdx + 2 && Line[ValueStartIdx] == '"' && Line[ValueEndIdx - 1] == '"')
			{
				ValueStartIdx++;
				ValueEndIdx--;
			}

			// Add it to the config section
			string Key = Line.Substring(KeyStartIdx, KeyEndIdx - KeyStartIdx);
			string Value = Line.Substring(ValueStartIdx, ValueEndIdx - ValueStartIdx);
			Section.Lines.Add(new ConfigLine(Action, Key, Value));
			return true;
		}

		/// <summary>
		/// Names of sections in this file
		/// </summary>
		public IEnumerable<string> SectionNames
		{
			get { return Sections.Keys; }
		}

		/// <summary>
		/// Tries to get a config section by name
		/// </summary>
		/// <param name="SectionName">Name of the section to look for</param>
		/// <param name="RawSection">On success, the config section that was found</param>
		/// <returns>True if the section was found, false otherwise</returns>
		public bool TryGetSection(string SectionName, out ConfigFileSection RawSection)
		{
			return Sections.TryGetValue(SectionName, out RawSection);
		}

		/// <summary>
		/// Write the config file out to the given location. Useful for debugging.
		/// </summary>
		/// <param name="Location">The file to write</param>
		public void Write(FileReference Location)
		{
			using (StreamWriter Writer = new StreamWriter(Location.FullName))
			{
				foreach (ConfigFileSection Section in Sections.Values)
				{
					Writer.WriteLine("[{0}]", Section.Name);
					foreach (ConfigLine Line in Section.Lines)
					{
						Writer.WriteLine("{0}", Line.ToString());
					}
					Writer.WriteLine();
				}
			}
		}
	}
}
