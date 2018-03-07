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
	[DebuggerDisplay("{Number}: {Description}")]
	class PerforceChangeSummary
	{
		public int Number;
		public DateTime Date;
		public string User;
		public string Client;
		public string Description;
	}

	[DebuggerDisplay("#{Revision} - {ChangeNumber}: {Description}")]
	class PerforceFileChangeSummary
	{
		public int Revision;
		public int ChangeNumber;
		public string Action;
		public DateTime Date;
		public string User;
		public string Client;
		public string Type;
		public string Description;
	}

	[DebuggerDisplay("{Name}")]
	class PerforceClientRecord
	{
		public string Name;
		public string Owner;
		public string Host;
		public string Root;

		public PerforceClientRecord(Dictionary<string, string> Tags)
		{
			Tags.TryGetValue("client", out Name);
			Tags.TryGetValue("Owner", out Owner);
			Tags.TryGetValue("Host", out Host);
			Tags.TryGetValue("Root", out Root);
		}
	}

	[DebuggerDisplay("{DepotFile}")]
	class PerforceDescribeFileRecord
	{
		public string DepotFile;
		public string Action;
		public string Type;
		public int Revision;
		public int FileSize;
		public string Digest;
	}

	[DebuggerDisplay("{ChangeNumber}: {Description}")]
	class PerforceDescribeRecord
	{
		public int ChangeNumber;
		public string User;
		public string Client;
		public long Time;
		public string Description;
		public string Status;
		public string ChangeType;
		public string Path;
		public List<PerforceDescribeFileRecord> Files = new List<PerforceDescribeFileRecord>();

		public PerforceDescribeRecord(Dictionary<string, string> Tags)
		{
			string ChangeString;
			if(!Tags.TryGetValue("change", out ChangeString) || !int.TryParse(ChangeString, out ChangeNumber))
			{
				ChangeNumber = -1;
			}

			Tags.TryGetValue("user", out User);
			Tags.TryGetValue("client", out Client);

			string TimeString;
			if(!Tags.TryGetValue("time", out TimeString) || !long.TryParse(TimeString, out Time))
			{
				Time = -1;
			}

			Tags.TryGetValue("desc", out Description);
			Tags.TryGetValue("status", out Status);
			Tags.TryGetValue("changeType", out ChangeType);
			Tags.TryGetValue("path", out Path);

			for(int Idx = 0;;Idx++)
			{
				string Suffix = String.Format("{0}", Idx);

				PerforceDescribeFileRecord File = new PerforceDescribeFileRecord();
				if(!Tags.TryGetValue("depotFile" + Suffix, out File.DepotFile))
				{
					break;
				}

				Tags.TryGetValue("action" + Suffix, out File.Action);
				Tags.TryGetValue("type" + Suffix, out File.Type);

				string RevisionString;
				if(!Tags.TryGetValue("rev" + Suffix, out RevisionString) || !int.TryParse(RevisionString, out File.Revision))
				{
					File.Revision = -1;
				}

				string FileSizeString;
				if(!Tags.TryGetValue("fileSize" + Suffix, out FileSizeString) || !int.TryParse(FileSizeString, out File.FileSize))
				{
					File.FileSize = -1;
				}

				Tags.TryGetValue("digest" + Suffix, out File.Digest);
				Files.Add(File);
			}
		}
	}

	[DebuggerDisplay("{ServerAddress}")]
	class PerforceInfoRecord
	{
		public string UserName;
		public string HostName;
		public string ClientAddress;
		public string ServerAddress;
		public TimeSpan ServerTimeZone;

		public PerforceInfoRecord(Dictionary<string, string> Tags)
		{
			Tags.TryGetValue("userName", out UserName);
			Tags.TryGetValue("clientHost", out HostName);
			Tags.TryGetValue("clientAddress", out ClientAddress);
			Tags.TryGetValue("serverAddress", out ServerAddress);

			string ServerDateTime;
			if(Tags.TryGetValue("serverDate", out ServerDateTime))
			{
				string[] Fields = ServerDateTime.Split(new char[]{ ' ' }, StringSplitOptions.RemoveEmptyEntries);
				if(Fields.Length >= 3)
				{
					DateTimeOffset Offset;
					if(DateTimeOffset.TryParse(String.Join(" ", Fields.Take(3)), out Offset))
					{
						ServerTimeZone = Offset.Offset;
					}
				}
			}
		}
	}

	[DebuggerDisplay("{DepotPath}")]
	class PerforceFileRecord
	{
		public string DepotPath;
		public string ClientPath;
		public string Path;
		public string Action;
		public int Revision;
		public bool IsMapped;
		public bool Unmap;

		public PerforceFileRecord(Dictionary<string, string> Tags)
		{
			Tags.TryGetValue("depotFile", out DepotPath);
			Tags.TryGetValue("clientFile", out ClientPath);
			Tags.TryGetValue("path", out Path);
			if(!Tags.TryGetValue("action", out Action))
			{
				Tags.TryGetValue("headAction", out Action);
			}
			IsMapped = Tags.ContainsKey("isMapped");
			Unmap = Tags.ContainsKey("unmap");

			string RevisionString;
			if(Tags.TryGetValue("rev", out RevisionString))
			{
				int.TryParse(RevisionString, out Revision);
			}
		}
	}

	class PerforceTagRecordParser : IDisposable
	{
		Action<Dictionary<string, string>> OutputRecord;
		Dictionary<string, string> Tags = new Dictionary<string,string>();

		public PerforceTagRecordParser(Action<Dictionary<string, string>> InOutputRecord)
		{
			OutputRecord = InOutputRecord;
		}

		public void OutputLine(string Line)
		{
			int SpaceIdx = Line.IndexOf(' ');

			string Key = (SpaceIdx > 0)? Line.Substring(0, SpaceIdx) : Line;
			if(Tags.ContainsKey(Key))
			{
				OutputRecord(Tags);
				Tags = new Dictionary<string,string>();
			}

			Tags.Add(Key, (SpaceIdx > 0)? Line.Substring(SpaceIdx + 1) : "");
		}

		public void Dispose()
		{
			if(Tags.Count > 0)
			{
				OutputRecord(Tags);
				Tags = new Dictionary<string,string>();
			}
		}
	}

	[DebuggerDisplay("{Path}")]
	class PerforceWhereRecord
	{
		public string LocalPath;
		public string ClientPath;
		public string DepotPath;
	}

	class PerforceSyncOptions
	{
		public int NumRetries;
		public int NumThreads;
		public int TcpBufferSize;

		public PerforceSyncOptions Clone()
		{
			return (PerforceSyncOptions)MemberwiseClone();
		}
	}

	class PerforceSpec
	{
		public List<KeyValuePair<string, string>> Sections;

		/// <summary>
		/// Default constructor.
		/// </summary>
		public PerforceSpec()
		{
			Sections = new List<KeyValuePair<string,string>>();
		}

		/// <summary>
		/// Gets the current value of a field with the given name 
		/// </summary>
		/// <param name="Name">Name of the field to search for</param>
		/// <returns>The value of the field, or null if it does not exist</returns>
		public string GetField(string Name)
		{
			int Idx = Sections.FindIndex(x => x.Key == Name);
			return (Idx == -1)? null : Sections[Idx].Value;
		}

		/// <summary>
		/// Sets the value of an existing field, or adds a new one with the given name
		/// </summary>
		/// <param name="Name">Name of the field to set</param>
		/// <param name="Value">New value of the field</param>
		public void SetField(string Name, string Value)
		{
			int Idx = Sections.FindIndex(x => x.Key == Name);
			if(Idx == -1)
			{
				Sections.Add(new KeyValuePair<string,string>(Name, Value));
			}
			else
			{
				Sections[Idx] = new KeyValuePair<string,string>(Name, Value);
			}
		}

		/// <summary>
		/// Parses a spec (clientspec, branchspec, changespec) from an array of lines
		/// </summary>
		/// <param name="Lines">Text split into separate lines</param>
		/// <returns>Array of section names and values</returns>
		public static bool TryParse(List<string> Lines, out PerforceSpec Spec, TextWriter Log)
		{
			Spec = new PerforceSpec();
			for(int LineIdx = 0; LineIdx < Lines.Count; LineIdx++)
			{
				if(Lines[LineIdx].EndsWith("\r"))
				{
					Lines[LineIdx] = Lines[LineIdx].Substring(0, Lines[LineIdx].Length - 1);
				}
				if(!String.IsNullOrWhiteSpace(Lines[LineIdx]) && !Lines[LineIdx].StartsWith("#"))
				{
					// Read the section name
					int SeparatorIdx = Lines[LineIdx].IndexOf(':');
					if(SeparatorIdx == -1 || !Char.IsLetter(Lines[LineIdx][0]))
					{
						Log.WriteLine("Invalid spec format at line {0}: \"{1}\"", LineIdx, Lines[LineIdx]);
						return false;
					}

					// Get the section name
					string SectionName = Lines[LineIdx].Substring(0, SeparatorIdx);

					// Parse the section value
					StringBuilder Value = new StringBuilder(Lines[LineIdx].Substring(SeparatorIdx + 1).TrimStart());
					for(; LineIdx + 1 < Lines.Count; LineIdx++)
					{
						if(Lines[LineIdx + 1].Length == 0)
						{
							Value.AppendLine();
						}
						else if(Lines[LineIdx + 1][0] == '\t')
						{
							Value.AppendLine(Lines[LineIdx + 1].Substring(1));
						}
						else
						{
							break;
						}
					}
					Spec.Sections.Add(new KeyValuePair<string,string>(SectionName, Value.ToString().TrimEnd()));
				}
			}
			return true;
		}

		/// <summary>
		/// Formats a P4 specification as a block of text
		/// </summary>
		/// <returns></returns>
		public override string ToString()
		{
			StringBuilder Result = new StringBuilder();
			foreach(KeyValuePair<string, string> Section in Sections)
			{
				if(Section.Value.Contains('\n'))
				{
					Result.AppendLine(Section.Key + ":\n\t" + Section.Value.Replace("\n", "\n\t"));
				}
				else
				{
					Result.AppendLine(Section.Key + ":\t" + Section.Value);
				}
				Result.AppendLine();
			}
			return Result.ToString();
		}
	}

	enum PerforceOutputChannel
	{
		Unknown,
		Text,
		Info,
		TaggedInfo,
		Warning,
		Error,
		Exit
	}

	class PerforceOutputLine
	{
		public readonly PerforceOutputChannel Channel;
		public readonly string Text;

		public PerforceOutputLine(PerforceOutputChannel InChannel, string InText)
		{
			Channel = InChannel;
			Text = InText;
		}

		public override string ToString()
		{
			return String.Format("{0}: {1}", Channel, Text);
		}
	}

	class PerforceConnection
	{
		[Flags]
		enum CommandOptions
		{
			None = 0x0,
			NoClient = 0x1,
			NoFailOnErrors = 0x2,
			NoChannels = 0x4,
			IgnoreFilesUpToDateError = 0x8,
			IgnoreNoSuchFilesError = 0x10,
			IgnoreFilesNotOpenedOnThisClientError = 0x20,
			IgnoreExitCode = 0x40,
			IgnoreFilesNotInClientViewError = 0x80,
		}

		delegate bool HandleRecordDelegate(Dictionary<string, string> Tags);
		delegate bool HandleOutputDelegate(PerforceOutputLine Line);

		public readonly string ServerAndPort;
		public readonly string UserName;
		public readonly string ClientName;

		public PerforceConnection(string InUserName, string InClientName, string InServerAndPort)
		{
			ServerAndPort = InServerAndPort;
			UserName = InUserName;
			ClientName = InClientName;
		}

		public PerforceConnection OpenClient(string NewClientName)
		{
			return new PerforceConnection(UserName, NewClientName, ServerAndPort);
		}

		public bool Info(out PerforceInfoRecord Info, TextWriter Log)
		{
			List<Dictionary<string, string>> TagRecords;
			if(!RunCommand("info -s", out TagRecords, CommandOptions.NoClient, Log) || TagRecords.Count != 1)
			{
				Info = null;
				return false;
			}
			else
			{
				Info = new PerforceInfoRecord(TagRecords[0]);
				return true;
			}
		}

		public bool GetSetting(string Name, out string Value, TextWriter Log)
		{
			List<string> Lines;
			if(!RunCommand(String.Format("set {0}", Name), out Lines, CommandOptions.NoChannels, Log) || Lines.Count != 1)
			{
				Value = null;
				return false;
			}
			if(Lines[0].Length <= Name.Length || !Lines[0].StartsWith(Name, StringComparison.InvariantCultureIgnoreCase) || Lines[0][Name.Length] != '=')
			{
				Value = null;
				return false;
			}

			Value = Lines[0].Substring(Name.Length + 1);

			int EndIdx = Value.IndexOf(" (");
			if(EndIdx != -1)
			{
				Value = Value.Substring(0, EndIdx).Trim();
			}
			return true;
		}

		public bool FindClients(out List<PerforceClientRecord> Clients, TextWriter Log)
		{
			return RunCommand("clients", out Clients, CommandOptions.NoClient, Log);
		}

		public bool FindClients(string ForUserName, out List<string> ClientNames, TextWriter Log)
		{
			List<string> Lines;
			if(!RunCommand(String.Format("clients -u{0}", ForUserName), out Lines, CommandOptions.None, Log))
			{
				ClientNames = null;
				return false;
			}

			ClientNames = new List<string>();
			foreach(string Line in Lines)
			{
				string[] Tokens = Line.Split(new char[]{ ' ' }, StringSplitOptions.RemoveEmptyEntries);
				if(Tokens.Length < 5 || Tokens[0] != "Client" || Tokens[3] != "root")
				{
					Log.WriteLine("Couldn't parse client from line '{0}'", Line);
				}
				else
				{
					ClientNames.Add(Tokens[1]);
				}
			}
			return true;
		}

		public bool TryGetClientSpec(string ClientName, out PerforceSpec Spec, TextWriter Log)
		{
			List<string> Lines;
			if(!RunCommand(String.Format("client -o {0}", ClientName), out Lines, CommandOptions.None, Log))
			{
				Spec = null;
				return false;
			}
			if(!PerforceSpec.TryParse(Lines, out Spec, Log))
			{
				Spec = null;
				return false;
			}
			return true;
		}

		public bool TryGetStreamSpec(string StreamName, out PerforceSpec Spec, TextWriter Log)
		{
			List<string> Lines;
			if(!RunCommand(String.Format("stream -o {0}", StreamName), out Lines, CommandOptions.None, Log))
			{
				Spec = null;
				return false;
			}
			if(!PerforceSpec.TryParse(Lines, out Spec, Log))
			{
				Spec = null;
				return false;
			}
			return true;
		}

		public bool FindFiles(string Filter, out List<PerforceFileRecord> FileRecords, TextWriter Log)
		{
			return RunCommand(String.Format("fstat \"{0}\"", Filter), out FileRecords, CommandOptions.None, Log);
		}
		
		public bool Print(string DepotPath, out List<string> Lines, TextWriter Log)
		{
			string TempFileName = Path.GetTempFileName();
			try
			{
				if(!PrintToFile(DepotPath, TempFileName, Log))
				{
					Lines = null;
					return false;
				}

				try
				{
					Lines = new List<string>(File.ReadAllLines(TempFileName));
					return true;
				}
				catch
				{
					Lines = null;
					return false;
				}
			}
			finally
			{
				try
				{
					File.SetAttributes(TempFileName, FileAttributes.Normal);
					File.Delete(TempFileName);
				}
				catch
				{
				}
			}
		}
		
		public bool PrintToFile(string DepotPath, string OutputFileName, TextWriter Log)
		{
			return RunCommand(String.Format("print -q -o \"{0}\" \"{1}\"", OutputFileName, DepotPath), CommandOptions.None, Log);
		}

		public bool FileExists(string Filter, out bool bExists, TextWriter Log)
		{
			List<PerforceFileRecord> FileRecords;
			if(RunCommand(String.Format("fstat \"{0}\"", Filter), out FileRecords, CommandOptions.IgnoreNoSuchFilesError | CommandOptions.IgnoreFilesNotInClientViewError, Log))
			{
				bExists = (FileRecords.Count > 0);
				return true;
			}
			else
			{
				bExists = false;
				return false;
			}
		}

		public bool FindChanges(string Filter, int MaxResults, out List<PerforceChangeSummary> Changes, TextWriter Log)
		{
			return FindChanges(new string[]{ Filter }, MaxResults, out Changes, Log);
		}

		public bool FindChanges(IEnumerable<string> Filters, int MaxResults, out List<PerforceChangeSummary> Changes, TextWriter Log)
		{
			string Arguments = "changes -s submitted -t -L";
			if(MaxResults > 0)
			{
				Arguments += String.Format(" -m {0}", MaxResults);
			}
			foreach(string Filter in Filters)
			{
				Arguments += String.Format(" \"{0}\"", Filter);
			}

			List<string> Lines;
			if(!RunCommand(Arguments, out Lines, CommandOptions.None, Log))
			{
				Changes = null;
				return false;
			}

			Changes = new List<PerforceChangeSummary>();
			for(int Idx = 0; Idx < Lines.Count; Idx++)
			{
				PerforceChangeSummary Change = TryParseChangeSummary(Lines[Idx]);
				if(Change == null)
				{
					Log.WriteLine("Couldn't parse description from '{0}'", Lines[Idx]);
				}
				else
				{
					StringBuilder Description = new StringBuilder();
					for(; Idx + 1 < Lines.Count; Idx++)
					{
						if(Lines[Idx + 1].Length == 0)
						{
							Description.AppendLine();
						}
						else if(Lines[Idx + 1].StartsWith("\t"))
						{
							Description.AppendLine(Lines[Idx + 1].Substring(1));
						}
						else
						{
							break;
						}
					}
					Change.Description = Description.ToString().Trim();

					Changes.Add(Change);
				}
			}

			Changes = Changes.GroupBy(x => x.Number).Select(x => x.First()).OrderByDescending(x => x.Number).ToList();

			if(MaxResults >= 0 && MaxResults < Changes.Count)
			{
				Changes.RemoveRange(MaxResults, Changes.Count - MaxResults);
			}

			return true;
		}

		PerforceChangeSummary TryParseChangeSummary(string Line)
		{
			string[] Tokens = Line.Split(new char[]{ ' ' }, StringSplitOptions.RemoveEmptyEntries);
			if(Tokens.Length == 7 && Tokens[0] == "Change" && Tokens[2] == "on" && Tokens[5] == "by")
			{
				PerforceChangeSummary Change = new PerforceChangeSummary();
				if(int.TryParse(Tokens[1], out Change.Number) && DateTime.TryParse(Tokens[3] + " " + Tokens[4], out Change.Date))
				{
					int UserClientIdx = Tokens[6].IndexOf('@');
					if(UserClientIdx != -1)
					{
						Change.User = Tokens[6].Substring(0, UserClientIdx);
						Change.Client = Tokens[6].Substring(UserClientIdx + 1);
						return Change;
					}
				}
			}
			return null;
		}

		public bool FindFileChanges(string FilePath, int MaxResults, out List<PerforceFileChangeSummary> Changes, TextWriter Log)
		{
			string Arguments = "filelog -L -t";
			if(MaxResults > 0)
			{
				Arguments += String.Format(" -m {0}", MaxResults);
			}
			Arguments += String.Format(" \"{0}\"", FilePath);

			List<string> Lines;
			if(!RunCommand(Arguments, PerforceOutputChannel.TaggedInfo, out Lines, CommandOptions.None, Log))
			{
				Changes = null;
				return false;
			}

			Changes = new List<PerforceFileChangeSummary>();
			for(int Idx = 0; Idx < Lines.Count; Idx++)
			{
				PerforceFileChangeSummary Change;
				if(!TryParseFileChangeSummary(Lines, ref Idx, out Change))
				{
					Log.WriteLine("Couldn't parse description from '{0}'", Lines[Idx]);
				}
				else
				{
					Changes.Add(Change);
				}
			}

			return true;
		}

		bool TryParseFileChangeSummary(List<string> Lines, ref int LineIdx, out PerforceFileChangeSummary OutChange)
		{
			string[] Tokens = Lines[LineIdx].Split(new char[]{ ' ' }, StringSplitOptions.RemoveEmptyEntries);
			if(Tokens.Length != 10 || !Tokens[0].StartsWith("#") || Tokens[1] != "change" || Tokens[4] != "on" || Tokens[7] != "by")
			{
				OutChange = null;
				return false;
			}

			PerforceFileChangeSummary Change = new PerforceFileChangeSummary();
			if(!int.TryParse(Tokens[0].Substring(1), out Change.Revision) || !int.TryParse(Tokens[2], out Change.ChangeNumber) || !DateTime.TryParse(Tokens[5] + " " + Tokens[6], out Change.Date))
			{
				OutChange = null;
				return false;
			}

			int UserClientIdx = Tokens[8].IndexOf('@');
			if(UserClientIdx == -1)
			{
				OutChange = null;
				return false;
			}

			Change.Action = Tokens[3];
			Change.Type = Tokens[9].Trim('(', ')');
			Change.User = Tokens[8].Substring(0, UserClientIdx);
			Change.Client = Tokens[8].Substring(UserClientIdx + 1);

			StringBuilder Description = new StringBuilder();
			for(; LineIdx + 1 < Lines.Count; LineIdx++)
			{
				if(Lines[LineIdx + 1].Length == 0)
				{
					Description.AppendLine();
				}
				else if(Lines[LineIdx + 1].StartsWith("\t"))
				{
					Description.AppendLine(Lines[LineIdx + 1].Substring(1));
				}
				else
				{
					break;
				}
			}
			Change.Description = Description.ToString().Trim();

			OutChange = Change;
			return true;
		}

		public bool ConvertToClientPath(string FileName, out string ClientFileName, TextWriter Log)
		{
			PerforceWhereRecord WhereRecord;
			if(Where(FileName, out WhereRecord, Log))
			{
				ClientFileName = WhereRecord.ClientPath;
				return true;
			}
			else
			{
				ClientFileName = null;
				return false;
			}
		}

		public bool ConvertToDepotPath(string FileName, out string DepotFileName, TextWriter Log)
		{
			PerforceWhereRecord WhereRecord;
			if(Where(FileName, out WhereRecord, Log))
			{
				DepotFileName = WhereRecord.DepotPath;
				return true;
			}
			else
			{
				DepotFileName = null;
				return false;
			}
		}

		public bool ConvertToLocalPath(string FileName, out string LocalFileName, TextWriter Log)
		{
			PerforceWhereRecord WhereRecord;
			if(Where(FileName, out WhereRecord, Log))
			{
				LocalFileName = Utility.GetPathWithCorrectCase(new FileInfo(WhereRecord.LocalPath));
				return true;
			}
			else
			{
				LocalFileName = null;
				return false;
			}
		}

		public bool FindStreams(string Filter, out List<string> OutStreamNames, TextWriter Log)
		{
			List<string> Lines;
			if(!RunCommand(String.Format("streams -F \"Stream={0}\"", Filter), out Lines, CommandOptions.None, Log))
			{
				OutStreamNames = null;
				return false;
			}

			List<string> StreamNames = new List<string>();
			foreach(string Line in Lines)
			{
				string[] Tokens = Line.Split(new char[]{ ' ' }, StringSplitOptions.RemoveEmptyEntries);
				if(Tokens.Length < 2 || Tokens[0] != "Stream" || !Tokens[1].StartsWith("//"))
				{
					Log.WriteLine("Unexpected output from stream query: {0}", Line);
					OutStreamNames = null;
					return false;
				}
				StreamNames.Add(Tokens[1]);
			}
			OutStreamNames = StreamNames;

			return true;
		}

		public bool HasOpenFiles(TextWriter Log)
		{
			List<PerforceFileRecord> Records = new List<PerforceFileRecord>();
			bool bResult = RunCommand("opened -m 1", out Records, CommandOptions.None, Log);
			return bResult && Records.Count > 0;
		}

		public bool SwitchStream(string NewStream, TextWriter Log)
		{
			return RunCommand(String.Format("client -f -s -S \"{0}\" \"{1}\"", NewStream, ClientName), CommandOptions.None, Log);
		}

		public bool Describe(int ChangeNumber, out PerforceDescribeRecord Record, TextWriter Log)
		{
			string CommandLine = String.Format("describe -s {0}", ChangeNumber);

			List<Dictionary<string, string>> Records = new List<Dictionary<string,string>>();
			if(!RunCommandWithBinaryOutput(CommandLine, Records, CommandOptions.None, Log))
			{
				Record = null;
				return false;
			}
			if(Records.Count != 1)
			{
				Log.WriteLine("Expected 1 record from p4 {0}, got {1}", CommandLine, Records.Count);
				Record = null;
				return false;
			}

			string Code;
			if(!Records[0].TryGetValue("code", out Code) || Code != "stat")
			{
				Log.WriteLine("Unexpected response from p4 {0}: {1}", CommandLine, String.Join(", ", Records[0].Select(x => String.Format("( \"{0}\", \"{1}\" )", x.Key, x.Value))));
				Record = null;
				return false;
			}

			Record = new PerforceDescribeRecord(Records[0]);
			return true;
		}

		public bool Where(string Filter, out PerforceWhereRecord WhereRecord, TextWriter Log)
		{
			List<PerforceFileRecord> FileRecords;
			if(!RunCommand(String.Format("where \"{0}\"", Filter), out FileRecords, CommandOptions.None, Log))
			{
				WhereRecord = null;
				return false;
			}

			FileRecords.RemoveAll(x => x.Unmap);

			if(FileRecords.Count == 0)
			{
				Log.WriteLine("'{0}' is not mapped to workspace.", Filter);
				WhereRecord = null;
				return false;
			}
			else if(FileRecords.Count > 1)
			{
				Log.WriteLine("File is mapped to {0} locations: {1}", FileRecords.Count, String.Join(", ", FileRecords.Select(x => x.Path)));
				WhereRecord = null;
				return false;
			}

			WhereRecord = new PerforceWhereRecord();
			WhereRecord.LocalPath = FileRecords[0].Path;
			WhereRecord.DepotPath = FileRecords[0].DepotPath;
			WhereRecord.ClientPath = FileRecords[0].ClientPath;
			return true;
		}

		public bool Have(string Filter, out List<PerforceFileRecord> FileRecords, TextWriter Log)
		{
			return RunCommand(String.Format("have \"{0}\"", Filter), out FileRecords, CommandOptions.None, Log);
		}

		public bool Stat(string Filter, out List<PerforceFileRecord> FileRecords, TextWriter Log)
		{
			return RunCommand(String.Format("fstat \"{0}\"", Filter), out FileRecords, CommandOptions.None, Log);
		}

		public bool Sync(string Filter, TextWriter Log)
		{
			return RunCommand("sync " + Filter, CommandOptions.IgnoreFilesUpToDateError, Log);
		}

		public bool Sync(List<string> DepotPaths, int ChangeNumber, Action<PerforceFileRecord> SyncOutput, List<string> TamperedFiles, PerforceSyncOptions Options, TextWriter Log)
		{
			// Write all the files we want to sync to a temp file
			string TempFileName = Path.GetTempFileName();
			using(StreamWriter Writer = new StreamWriter(TempFileName))
			{
				foreach(string DepotPath in DepotPaths)
				{
					Writer.WriteLine("{0}@{1}", DepotPath, ChangeNumber);
				}
			}

			// Create a filter to strip all the sync records
			bool bResult;
			using(PerforceTagRecordParser Parser = new PerforceTagRecordParser(x => SyncOutput(new PerforceFileRecord(x))))
			{
				StringBuilder CommandLine = new StringBuilder();
				CommandLine.AppendFormat("-x \"{0}\" -z tag", TempFileName);
				if(Options != null && Options.NumRetries > 0)
				{
					CommandLine.AppendFormat(" -r {0}", Options.NumRetries);
				}
				if(Options != null && Options.TcpBufferSize > 0)
				{
					CommandLine.AppendFormat(" -v net.tcpsize={0}", Options.TcpBufferSize);
				}
				CommandLine.Append(" sync");
				if(Options != null && Options.NumThreads > 1)
				{
					CommandLine.AppendFormat(" --parallel=threads={0}", Options.NumThreads);
				}
				bResult = RunCommand(CommandLine.ToString(), null, Line => FilterSyncOutput(Line, Parser, TamperedFiles, Log), CommandOptions.NoFailOnErrors | CommandOptions.IgnoreFilesUpToDateError | CommandOptions.IgnoreExitCode, Log);
			}
			return bResult;
		}

		private static bool FilterSyncOutput(PerforceOutputLine Line, PerforceTagRecordParser Parser, List<string> TamperedFiles, TextWriter Log)
		{
			if(Line.Channel == PerforceOutputChannel.TaggedInfo)
			{
				Parser.OutputLine(Line.Text);
				return true;
			}

			Log.WriteLine(Line.Text);

			const string Prefix = "Can't clobber writable file ";
			if(Line.Channel == PerforceOutputChannel.Error && Line.Text.StartsWith(Prefix))
			{
				TamperedFiles.Add(Line.Text.Substring(Prefix.Length).Trim());
				return true;
			}

			return Line.Channel != PerforceOutputChannel.Error;
		}

		private void ParseTamperedFile(string Line, List<string> TamperedFiles)
		{
			const string Prefix = "Can't clobber writable file ";
			if(Line.StartsWith(Prefix))
			{
				TamperedFiles.Add(Line.Substring(Prefix.Length).Trim());
			}
		}

		public bool SyncPreview(string Filter, int ChangeNumber, bool bOnlyFilesInThisChange, out List<PerforceFileRecord> FileRecords, TextWriter Log)
		{
			return RunCommand(String.Format("sync -n {0}@{1}{2}", Filter, bOnlyFilesInThisChange? "=" : "", ChangeNumber), out FileRecords, CommandOptions.IgnoreFilesUpToDateError | CommandOptions.IgnoreNoSuchFilesError, Log);
		}

		public bool ForceSync(string Filter, int ChangeNumber, TextWriter Log)
		{
			return RunCommand(String.Format("sync -f \"{0}\"@{1}", Filter, ChangeNumber), CommandOptions.IgnoreFilesUpToDateError, Log);
		}

		public bool GetOpenFiles(string Filter, out List<PerforceFileRecord> FileRecords, TextWriter Log)
		{
			return RunCommand(String.Format("opened \"{0}\"", Filter), out FileRecords, CommandOptions.None, Log);
		}

		public bool GetUnresolvedFiles(string Filter, out List<PerforceFileRecord> FileRecords, TextWriter Log)
		{
			return RunCommand(String.Format("fstat -Ru \"{0}\"", Filter), out FileRecords, CommandOptions.IgnoreNoSuchFilesError | CommandOptions.IgnoreFilesNotOpenedOnThisClientError, Log);
		}

		public bool AutoResolveFile(string File, TextWriter Log)
		{
			return RunCommand(String.Format("resolve -am {0}", File), CommandOptions.None, Log);
		}

		public bool GetActiveStream(out string StreamName, TextWriter Log)
		{
			PerforceSpec ClientSpec;
			if(TryGetClientSpec(ClientName, out ClientSpec, Log))
			{
				StreamName = ClientSpec.GetField("Stream");
				return StreamName != null;
			}
			else
			{
				StreamName = null;
				return false;
			}
		}

		private bool RunCommand(string CommandLine, out List<PerforceFileRecord> FileRecords, CommandOptions Options, TextWriter Log)
		{
			List<Dictionary<string, string>> TagRecords;
			if(!RunCommand(CommandLine, out TagRecords, Options, Log))
			{
				FileRecords = null;
				return false;
			}
			else
			{
				FileRecords = TagRecords.Select(x => new PerforceFileRecord(x)).ToList();
				return true;
			}
		}

		private bool RunCommand(string CommandLine, out List<PerforceClientRecord> ClientRecords, CommandOptions Options, TextWriter Log)
		{
			List<Dictionary<string, string>> TagRecords;
			if(!RunCommand(CommandLine, out TagRecords, Options, Log))
			{
				ClientRecords = null;
				return false;
			}
			else
			{
				ClientRecords = TagRecords.Select(x => new PerforceClientRecord(x)).ToList();
				return true;
			}
		}

		private bool RunCommand(string CommandLine, out List<Dictionary<string, string>> TagRecords, CommandOptions Options, TextWriter Log)
		{
			List<string> Lines;
			if(!RunCommand("-ztag " + CommandLine, PerforceOutputChannel.TaggedInfo, out Lines, Options, Log))
			{
				TagRecords = null;
				return false;
			}

			List<Dictionary<string, string>> LocalOutput = new List<Dictionary<string, string>>();
			using(PerforceTagRecordParser Parser = new PerforceTagRecordParser(Record => LocalOutput.Add(Record)))
			{
				foreach(string Line in Lines)
				{
					Parser.OutputLine(Line);
				}
			}
			TagRecords = LocalOutput;

			return true;
		}

		private bool RunCommand(string CommandLine, CommandOptions Options, TextWriter Log)
		{
			List<string> Lines;
			return RunCommand(CommandLine, out Lines, Options, Log);
		}

		private bool RunCommand(string CommandLine, out List<string> Lines, CommandOptions Options, TextWriter Log)
		{
			return RunCommand(CommandLine, PerforceOutputChannel.Info, out Lines, Options, Log);
		}

		private bool RunCommand(string CommandLine, PerforceOutputChannel Channel, out List<string> Lines, CommandOptions Options, TextWriter Log)
		{
			string FullCommandLine = GetFullCommandLine(CommandLine, Options);
			Log.WriteLine("p4> p4.exe {0}", FullCommandLine);

			List<string> RawOutputLines;
			if(Utility.ExecuteProcess("p4.exe", FullCommandLine, null, out RawOutputLines) != 0 && !Options.HasFlag(CommandOptions.IgnoreExitCode))
			{
				Lines = null;
				return false;
			}

			bool bResult = true;
			if(Options.HasFlag(CommandOptions.NoChannels))
			{
				Lines = RawOutputLines;
			}
			else
			{
				List<string> LocalLines = new List<string>();
				foreach(string RawOutputLine in RawOutputLines)
				{
					bResult &= ParseCommandOutput(RawOutputLine, Line => { if(Line.Channel == Channel){ LocalLines.Add(Line.Text); return true; } else { Log.WriteLine(Line.Text); return Line.Channel != PerforceOutputChannel.Error; } }, Options);
				}
				Lines = LocalLines;
			}
			return bResult;
		}

		private bool RunCommand(string CommandLine, string Input, HandleOutputDelegate HandleOutput, CommandOptions Options, TextWriter Log)
		{
			string FullCommandLine = GetFullCommandLine(CommandLine, Options);

			bool bResult = true;
			Log.WriteLine("p4> p4.exe {0}", FullCommandLine);
			if(Utility.ExecuteProcess("p4.exe", FullCommandLine, Input, Line => { bResult &= ParseCommandOutput(Line, HandleOutput, Options); }) != 0 && !Options.HasFlag(CommandOptions.IgnoreExitCode))
			{
				bResult = false;
			}
			return bResult;
		}

		private string GetFullCommandLine(string CommandLine, CommandOptions Options)
		{
			StringBuilder FullCommandLine = new StringBuilder();
			if(ServerAndPort != null)
			{
				FullCommandLine.AppendFormat("-p{0} ", ServerAndPort);
			}
			if(UserName != null)
			{
				FullCommandLine.AppendFormat("-u{0} ", UserName);
			}
			if(!Options.HasFlag(CommandOptions.NoClient) && ClientName != null)
			{
				FullCommandLine.AppendFormat("-c{0} ", ClientName);
			}
			if(!Options.HasFlag(CommandOptions.NoChannels))
			{
				FullCommandLine.Append("-s ");
			}
			FullCommandLine.Append(CommandLine);

			return FullCommandLine.ToString();
		}

		private bool ParseCommandOutput(string Text, HandleOutputDelegate HandleOutput, CommandOptions Options)
		{
			if(Options.HasFlag(CommandOptions.NoChannels))
			{
				PerforceOutputLine Line = new PerforceOutputLine(PerforceOutputChannel.Unknown, Text);
				return HandleOutput(Line);
			}
			else if(!IgnoreCommandOutput(Text, Options))
			{
				PerforceOutputLine Line;
				if(Text.StartsWith("text: "))
				{
					Line = new PerforceOutputLine(PerforceOutputChannel.Text, Text.Substring(6));
				}
				else if(Text.StartsWith("info: "))
				{
					Line = new PerforceOutputLine(PerforceOutputChannel.Info, Text.Substring(6));
				}
				else if(Text.StartsWith("info1: "))
				{
					Line = new PerforceOutputLine(IsValidTag(Text, 7)? PerforceOutputChannel.TaggedInfo : PerforceOutputChannel.Info, Text.Substring(7));
				}
				else if(Text.StartsWith("warning: "))
				{
					Line = new PerforceOutputLine(PerforceOutputChannel.Warning, Text.Substring(9));
				}
				else if(Text.StartsWith("error: "))
				{
					Line = new PerforceOutputLine(PerforceOutputChannel.Error, Text.Substring(7));
				}
				else
				{
					Line = new PerforceOutputLine(PerforceOutputChannel.Unknown, Text);
				}
				return HandleOutput(Line) && (Line.Channel != PerforceOutputChannel.Error || Options.HasFlag(CommandOptions.NoFailOnErrors)) && Line.Channel != PerforceOutputChannel.Unknown;
			}
			return true;
		}

		private static bool IsValidTag(string Line, int StartIndex)
		{
			// Annoyingly, we sometimes get commentary with an info1: prefix. Since it typically starts with a depot or file path, we can pick it out.
			for(int Idx = StartIndex; Idx < Line.Length && Line[Idx] != ' '; Idx++)
			{
				if(Line[Idx] == '/' || Line[Idx] == '\\')
				{
					return false;
				}
			}
			return true;
		}

		private static bool IgnoreCommandOutput(string Text, CommandOptions Options)
		{
			if(Text.StartsWith("exit: ") || Text.StartsWith("info2: ") || Text.Length == 0)
			{
				return true;
			}
			else if(Options.HasFlag(CommandOptions.IgnoreFilesUpToDateError) && Text.StartsWith("error: ") && Text.EndsWith("- file(s) up-to-date."))
			{
				return true;
			}
			else if(Options.HasFlag(CommandOptions.IgnoreNoSuchFilesError) && Text.StartsWith("error: ") && Text.EndsWith(" - no such file(s)."))
			{
				return true;
			}
			else if(Options.HasFlag(CommandOptions.IgnoreFilesNotInClientViewError) && Text.StartsWith("error: ") && Text.EndsWith("- file(s) not in client view."))
			{
				return true;
			}
			else if(Options.HasFlag(CommandOptions.IgnoreFilesNotOpenedOnThisClientError) && Text.StartsWith("error: ") && Text.EndsWith(" - file(s) not opened on this client."))
			{
				return true;
			}
			return false;
		}

		/// <summary>
		/// Execute a Perforce command and parse the output as marshalled Python objects. This is more robustly defined than the text-based tagged output
		/// format, because it avoids ambiguity when returned fields can have newlines.
		/// </summary>
		/// <param name="CommandLine">Command line to execute Perforce with</param>
		/// <param name="TaggedOutput">List that receives the output records</param>
		/// <param name="WithClient">Whether to include client information on the command line</param>
		private bool RunCommandWithBinaryOutput(string CommandLine, List<Dictionary<string, string>> Records, CommandOptions Options, TextWriter Log)
		{
			return RunCommandWithBinaryOutput(CommandLine, Record => { Records.Add(Record); return true; }, Options, Log);
		}

		/// <summary>
		/// Execute a Perforce command and parse the output as marshalled Python objects. This is more robustly defined than the text-based tagged output
		/// format, because it avoids ambiguity when returned fields can have newlines.
		/// </summary>
		/// <param name="CommandLine">Command line to execute Perforce with</param>
		/// <param name="TaggedOutput">List that receives the output records</param>
		/// <param name="WithClient">Whether to include client information on the command line</param>
		private bool RunCommandWithBinaryOutput(string CommandLine, HandleRecordDelegate HandleOutput, CommandOptions Options, TextWriter Log)
		{
			// Execute Perforce, consuming the binary output into a memory stream
			MemoryStream MemoryStream = new MemoryStream();
			using (Process Process = new Process())
			{
				Process.StartInfo.FileName = "p4.exe";
				Process.StartInfo.Arguments = GetFullCommandLine("-G " + CommandLine, Options | CommandOptions.NoChannels);

				Process.StartInfo.RedirectStandardError = true;
				Process.StartInfo.RedirectStandardOutput = true;
				Process.StartInfo.RedirectStandardInput = false;
				Process.StartInfo.UseShellExecute = false;
				Process.StartInfo.CreateNoWindow = true;

				Process.Start();

				Process.StandardOutput.BaseStream.CopyTo(MemoryStream);
				Process.WaitForExit();
			}

			// Move back to the start of the memory stream
			MemoryStream.Position = 0;

			// Parse the records
			List<Dictionary<string, string>> Records = new List<Dictionary<string, string>>();
			using (BinaryReader Reader = new BinaryReader(MemoryStream, Encoding.UTF8))
			{
				while(Reader.BaseStream.Position < Reader.BaseStream.Length)
				{
					// Check that a dictionary follows
					byte Temp = Reader.ReadByte();
					if(Temp != '{')
					{
						Log.WriteLine("Unexpected data while parsing marshalled output - expected '{'");
						return false;
					}

					// Read all the fields in the record
					Dictionary<string, string> Record = new Dictionary<string, string>();
					for(;;)
					{
						// Read the next field type. Perforce only outputs string records. A '0' character indicates the end of the dictionary.
						byte KeyFieldType = Reader.ReadByte();
						if(KeyFieldType == '0')
						{
							break;
						}
						else if(KeyFieldType != 's')
						{
							Log.WriteLine("Unexpected key field type while parsing marshalled output ({0}) - expected 's'", (int)KeyFieldType);
							return false;
						}

						// Read the key
						int KeyLength = Reader.ReadInt32();
						string Key = Encoding.UTF8.GetString(Reader.ReadBytes(KeyLength));

						// Read the value type.
						byte ValueFieldType = Reader.ReadByte();
						if(ValueFieldType == 'i')
						{
							// An integer
							string Value = Reader.ReadInt32().ToString();
							Record.Add(Key, Value);
						}
						else if(ValueFieldType == 's')
						{
							// A string
							int ValueLength = Reader.ReadInt32();
							string Value = Encoding.UTF8.GetString(Reader.ReadBytes(ValueLength));
							Record.Add(Key, Value);
						}
						else
						{
							Log.WriteLine("Unexpected value field type while parsing marshalled output ({0}) - expected 's'", (int)ValueFieldType);
							return false;
						}
					}
					if(!HandleOutput(Record))
					{
						return false;
					}
				}
			}
			return true;
		}
	}

	static class PerforceUtils
	{
		static public string GetClientOrDepotDirectoryName(string ClientFile)
		{
			int Index = ClientFile.LastIndexOf('/');
			if(Index == -1)
			{
				return "";
			}
			else
			{
				return ClientFile.Substring(0, Index);
			}
		}
	}
}
