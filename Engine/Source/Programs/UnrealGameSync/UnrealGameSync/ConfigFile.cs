// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace UnrealGameSync
{
	class ConfigObject
	{
		const string ConfigSeparatorCharacters = "(),= \t\"";

		public List<KeyValuePair<string, string>> Pairs;

		public ConfigObject()
		{
			Pairs = new List<KeyValuePair<string,string>>();
		}

		public ConfigObject(ConfigObject Other)
		{
			Pairs = new List<KeyValuePair<string,string>>(Other.Pairs);
		}

		public ConfigObject(string Text)
		{
			Pairs = new List<KeyValuePair<string,string>>();
			ParseConfigString(Text);
		}

		public ConfigObject(ConfigObject BaseObject, string Text)
		{
			Pairs = new List<KeyValuePair<string,string>>(BaseObject.Pairs);
			ParseConfigString(Text);
		}

		void ParseConfigString(string Text)
		{
			int Idx = 0;
			if(ParseConfigToken(Text, ref Idx) == "(")
			{
				while(Idx < Text.Length)
				{
					// Read the next key/value pair
					string Key = ParseConfigToken(Text, ref Idx);
					if(ParseConfigToken(Text, ref Idx) == "=")
					{
						string Value = ParseConfigToken(Text, ref Idx);
						SetValue(Key, Value);
					}

					// Check for the end of the list, or a comma before the next pair
					for(;;)
					{
						string Token = ParseConfigToken(Text, ref Idx);
						if(Token == ",")
						{
							break;
						}
						if(Token == ")" || Token == null)
						{
							return;
						}
					}
				}
			}
		}

		static string ParseConfigToken(string Text, ref int Idx)
		{
			// Skip whitespace
			while(Idx < Text.Length && Char.IsWhiteSpace(Text[Idx]))
			{
				Idx++;
			}
			if(Idx == Text.Length)
			{
				return null;
			}

			// Read the token
			if(Text[Idx] == '\"')
			{
				StringBuilder Token = new StringBuilder();
				while(++Idx < Text.Length)
				{
					if(Text[Idx] == '\"')
					{
						Idx++;
						break;
					}
					if(Text[Idx] == '\\' && Idx + 1 < Text.Length)
					{
						Idx++;
					}
					Token.Append(Text[Idx]);
				}
				return Token.ToString();
			}
			else if(ConfigSeparatorCharacters.IndexOf(Text[Idx]) != -1)
			{
				return Text[Idx++].ToString();
			}
			else
			{
				int StartIdx = Idx;
				while(Idx < Text.Length && ConfigSeparatorCharacters.IndexOf(Text[Idx]) == -1)
				{
					Idx++;
				}
				return Text.Substring(StartIdx, Idx - StartIdx);
			}
		}

		public string GetValue(string Key, string DefaultValue = null)
		{
			for(int Idx = 0; Idx < Pairs.Count; Idx++)
			{
				if(Pairs[Idx].Key.Equals(Key, StringComparison.InvariantCultureIgnoreCase))
				{
					return Pairs[Idx].Value;
				}
			}
			return DefaultValue;
		}

		public Guid GetValue(string Key, Guid DefaultValue)
		{
			string StringValue = GetValue(Key);
			if(StringValue != null)
			{
				Guid Value;
				if(Guid.TryParse(StringValue, out Value))
				{
					return Value;
				}
			}
			return DefaultValue;
		}

		public int GetValue(string Key, int DefaultValue)
		{
			string StringValue = GetValue(Key);
			if(StringValue != null)
			{
				int Value;
				if(int.TryParse(StringValue, out Value))
				{
					return Value;
				}
			}
			return DefaultValue;
		}

		public bool GetValue(string Key, bool DefaultValue)
		{
			string StringValue = GetValue(Key);
			if(StringValue != null)
			{
				bool Value;
				if(bool.TryParse(StringValue, out Value))
				{
					return Value;
				}
			}
			return DefaultValue;
		}

		public void SetValue(string Key, string Value)
		{
			for(int Idx = 0; Idx < Pairs.Count; Idx++)
			{
				if(Pairs[Idx].Key.Equals(Key, StringComparison.InvariantCultureIgnoreCase))
				{
					Pairs[Idx] = new KeyValuePair<string,string>(Key, Value);
					return;
				}
			}
			Pairs.Add(new KeyValuePair<string,string>(Key, Value));
		}

		public void SetValue(string Key, Guid Value)
		{
			SetValue(Key, Value.ToString());
		}

		public void SetValue(string Key, int Value)
		{
			SetValue(Key, Value.ToString());
		}

		public void SetValue(string Key, bool Value)
		{
			SetValue(Key, Value.ToString());
		}

		public string this[string Key]
		{
			get { return GetValue(Key); }
			set { SetValue(Key, value); }
		}

		public void SetDefaults(ConfigObject Other)
		{
			foreach(KeyValuePair<string, string> Pair in Other.Pairs)
			{
				if(GetValue(Pair.Key) == null)
				{
					SetValue(Pair.Key, Pair.Value);
				}
			}
		}

		public void AddOverrides(ConfigObject Object, ConfigObject DefaultObject)
		{
			foreach(KeyValuePair<string, string> Pair in Object.Pairs)
			{
				if(DefaultObject == null || DefaultObject.GetValue(Pair.Key) != Pair.Value)
				{
					SetValue(Pair.Key, Pair.Value);
				}
			}
		}

		public string ToString(ConfigObject BaseObject)
		{
			StringBuilder Result = new StringBuilder();
			Result.Append("(");
			foreach(KeyValuePair<string, string> Pair in Pairs)
			{
				if(BaseObject == null || BaseObject.GetValue(Pair.Key) != Pair.Value)
				{
					if(Result.Length > 1)
					{
						Result.Append(", ");
					}
					Result.Append(Pair.Key);
					Result.Append("=");
					if(Pair.Value == null)
					{
						Result.Append("\"\"");
					}
					else
					{
						Result.AppendFormat("\"{0}\"", Pair.Value.Replace("\\", "\\\\").Replace("\"", "\\\""));
					}
				}
			}
			Result.Append(")");
			return Result.ToString();
		}
 
		public override string ToString()
		{
			return ToString(null);
		}
	}

	[DebuggerDisplay("{Name}")]
	class ConfigSection
	{
		public string Name;
		public Dictionary<string, string> Pairs = new Dictionary<string,string>();

		public ConfigSection(string InName)
		{
			Name = InName;
		}

		public void Clear()
		{
			Pairs.Clear();
		}

		public void SetValue(string Key, int Value)
		{
			Pairs[Key] = Value.ToString();
		}

		public void SetValue(string Key, bool Value)
		{
			Pairs[Key] = Value? "1" : "0";
		}

		public void SetValue(string Key, string Value)
		{
			if(Value == null)
			{
				RemoveValue(Key);
			}
			else
			{
				Pairs[Key] = Value;
			}
		}

		public void SetValues(string Key, string[] Values)
		{
			if(Values == null)
			{
				RemoveValue(Key);
			}
			else
			{
				Pairs[Key] = String.Join("\n", Values);
			}
		}

		public void SetValues(string Key, Guid[] Values)
		{
			if(Values == null)
			{
				RemoveValue(Key);
			}
			else
			{
				Pairs[Key] = String.Join("\n", Values.Select(x => x.ToString()));
			}
		}

		public void AppendValue(string Key, string Value)
		{
			string CurrentValue;
			if(Pairs.TryGetValue(Key, out CurrentValue))
			{
				Pairs[Key] = CurrentValue + "\n" + Value;
			}
			else
			{
				Pairs[Key] = Value;
			}
		}

		public void RemoveValue(string Key)
		{
			Pairs.Remove(Key);
		}

		public int GetValue(string Key, int DefaultValue)
		{
			string ValueString = GetValue(Key);
			if(ValueString != null)
			{
				int Value;
				if(int.TryParse(ValueString, out Value))
				{
					return Value;
				}
			}
			return DefaultValue;
		}

		public bool GetValue(string Key, bool DefaultValue)
		{
			return GetValue(Key, DefaultValue? 1 : 0) != 0;
		}

		public string GetValue(string Key, string DefaultValue = null)
		{
			string Value;
			if(!Pairs.TryGetValue(Key, out Value))
			{
				Value = DefaultValue;
			}
			return Value;
		}

		public string[] GetValues(string Key, string[] DefaultValue = null)
		{
			string Value = GetValue(Key, null);
			if(Value == null)
			{
				return DefaultValue;
			}
			else
			{
				return Value.Split('\n');
			}
		}

		public Guid[] GetValues(string Key, Guid[] DefaultValue = null)
		{
			string[] StringValues = GetValues(Key, (string[])null);
			if(StringValues == null)
			{
				return DefaultValue;
			}

			List<Guid> GuidValues = new List<Guid>();
			foreach(string StringValue in StringValues)
			{
				Guid GuidValue;
				if(Guid.TryParse(StringValue, out GuidValue))
				{
					GuidValues.Add(GuidValue);
				}
			}

			return GuidValues.ToArray();
		}
	}

	class ConfigFile
	{
		List<ConfigSection> Sections = new List<ConfigSection>();

		public ConfigFile()
		{
		}

		public void Load(string FileName)
		{
			Parse(File.ReadAllLines(FileName));
		}

		public void Parse(string[] Lines)
		{
			ConfigSection CurrentSection = null;
			foreach(string Line in Lines)
			{
				string TrimLine = Line.Trim();
				if(!TrimLine.StartsWith(";"))
				{
					if(TrimLine.StartsWith("[") && TrimLine.EndsWith("]"))
					{
						string SectionName = TrimLine.Substring(1, TrimLine.Length - 2).Trim();
						CurrentSection = FindOrAddSection(SectionName);
					}
					else if(CurrentSection != null)
					{
						int EqualsIdx = TrimLine.IndexOf('=');
						if(EqualsIdx != -1)
						{
							string Value = Line.Substring(EqualsIdx + 1).TrimStart();
							if(TrimLine.StartsWith("+"))
							{
								CurrentSection.AppendValue(TrimLine.Substring(1, EqualsIdx - 1).Trim(), Value);
							}
							else
							{
								CurrentSection.SetValue(TrimLine.Substring(0, EqualsIdx).TrimEnd(), Value);
							}
						}
					}
				}
			}
		}

		public void Save(string FileName)
		{
			using(StreamWriter Writer = new StreamWriter(FileName))
			{
				for(int Idx = 0; Idx < Sections.Count; Idx++)
				{
					Writer.WriteLine("[{0}]", Sections[Idx].Name);
					foreach(KeyValuePair<string, string> Pair in Sections[Idx].Pairs)
					{
						if(Pair.Value.Contains('\n'))
						{
							foreach(string Line in Pair.Value.Split('\n'))
							{
								Writer.WriteLine("+{0}={1}", Pair.Key, Line);
							}
						}
						else
						{
							Writer.WriteLine("{0}={1}", Pair.Key, Pair.Value);
						}
					}
					if(Idx < Sections.Count - 1)
					{
						Writer.WriteLine();
					}
				}
			}
		}

		public ConfigSection FindSection(string Name)
		{
			return Sections.FirstOrDefault(x => String.Compare(x.Name, Name, true) == 0);
		}

		public ConfigSection FindOrAddSection(string Name)
		{
			ConfigSection Section = FindSection(Name);
			if(Section == null)
			{
				Section = new ConfigSection(Name);
				Sections.Add(Section);
			}
			return Section;
		}

		public void SetValue(string Key, int Value)
		{
			int DotIdx = Key.IndexOf('.');
			ConfigSection Section = FindOrAddSection(Key.Substring(0, DotIdx));
			Section.SetValue(Key.Substring(DotIdx + 1), Value);
		}

		public void SetValue(string Key, bool Value)
		{
			int DotIdx = Key.IndexOf('.');
			ConfigSection Section = FindOrAddSection(Key.Substring(0, DotIdx));
			Section.SetValue(Key.Substring(DotIdx + 1), Value);
		}

		public void SetValue(string Key, string Value)
		{
			int DotIdx = Key.IndexOf('.');
			ConfigSection Section = FindOrAddSection(Key.Substring(0, DotIdx));
			Section.SetValue(Key.Substring(DotIdx + 1), Value);
		}

		public void SetValues(string Key, string[] Values)
		{
			int DotIdx = Key.IndexOf('.');
			ConfigSection Section = FindOrAddSection(Key.Substring(0, DotIdx));
			Section.SetValues(Key.Substring(DotIdx + 1), Values);
		}

		public bool GetValue(string Key, bool DefaultValue)
		{
			int DotIdx = Key.IndexOf('.');
			ConfigSection Section = FindSection(Key.Substring(0, DotIdx));
			return (Section == null)? DefaultValue : Section.GetValue(Key.Substring(DotIdx + 1), DefaultValue);
		}

		public string GetValue(string Key, string DefaultValue)
		{
			int DotIdx = Key.IndexOf('.');
			ConfigSection Section = FindSection(Key.Substring(0, DotIdx));
			return (Section == null)? DefaultValue : Section.GetValue(Key.Substring(DotIdx + 1), DefaultValue);
		}

		public string[] GetValues(string Key, string[] DefaultValue)
		{
			int DotIdx = Key.IndexOf('.');
			ConfigSection Section = FindSection(Key.Substring(0, DotIdx));
			return (Section == null)? DefaultValue : Section.GetValues(Key.Substring(DotIdx + 1), DefaultValue);
		}

		public Guid[] GetGuidValues(string Key, Guid[] DefaultValue)
		{
			int DotIdx = Key.IndexOf('.');
			ConfigSection Section = FindSection(Key.Substring(0, DotIdx));
			return (Section == null)? DefaultValue : Section.GetValues(Key.Substring(DotIdx + 1), DefaultValue);
		}
	}
}
