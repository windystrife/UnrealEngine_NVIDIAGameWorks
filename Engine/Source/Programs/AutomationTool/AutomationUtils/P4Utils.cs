// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Text;
using System.Linq;
using System.IO;
using System.ComponentModel;
using System.Diagnostics;
using System.Text.RegularExpressions;
using System.Reflection;
using System.Collections;

namespace AutomationTool
{
	public class RequireP4Attribute : Attribute
	{
	}

	public class DoesNotNeedP4CLAttribute : Attribute
	{
	}

	public class P4Exception : AutomationException
	{
		public P4Exception(string Msg)
			: base(Msg) { }
		public P4Exception(string Msg, Exception InnerException)
			: base(InnerException, Msg) { }

		public P4Exception(string Format, params object[] Args)
			: base(Format, Args) { }
	}

    public enum P4LineEnd
    {
        Local = 0,
        Unix = 1,
        Mac = 2,
        Win = 3,
        Share = 4,
    }

    [Flags]
    public enum P4SubmitOption
    {
        SubmitUnchanged = 1,
        RevertUnchanged = 2,
        LeaveUnchanged = 4,
		Reopen = 8
    }

    [Flags]
    public enum P4ClientOption
    {
        None = 0,
        NoAllWrite = 1,
        NoClobber = 2,
        NoCompress = 4,
        NoModTime = 8,
        NoRmDir = 16,
        Unlocked = 32,
        AllWrite = 64,
        Clobber = 128,
        Compress = 256,
        Locked = 512,
        ModTime = 1024,
        RmDir = 2048,
    }

	public class P4ClientInfo
	{
		public string Name;
		public string RootPath;
		public string Host;
		public string Owner;
		public string Stream;
		public DateTime Access;
        public P4LineEnd LineEnd;
        public P4ClientOption Options;
        public P4SubmitOption SubmitOptions = P4SubmitOption.SubmitUnchanged;
        public List<KeyValuePair<string, string>> View = new List<KeyValuePair<string, string>>();

		public bool Matches(P4ClientInfo Other)
		{
			return Name == Other.Name 
				&& RootPath == Other.RootPath 
				&& Host == Other.Host 
				&& Owner == Other.Owner 
				&& Stream == Other.Stream
				&& LineEnd == Other.LineEnd 
				&& Options == Other.Options 
				&& SubmitOptions == Other.SubmitOptions
				&& (!String.IsNullOrEmpty(Stream) || Enumerable.SequenceEqual(View, Other.View));
		}

		public override string ToString()
		{
			return Name;
		}
	}

	public enum P4FileType
	{
		[Description("unknown")]
		Unknown,
		[Description("text")]
		Text,
		[Description("binary")]
		Binary,
		[Description("resource")]
		Resource,
		[Description("tempobj")]
		Temp,
		[Description("symlink")]
		Symlink,
		[Description("apple")]
		Apple,
		[Description("unicode")]
		Unicode,
		[Description("utf16")]
		Utf16,
		[Description("utf8")]
		Utf8,
	}

	[Flags]
	public enum P4FileAttributes
	{
		[Description("")]
		None = 0,
		[Description("u")]
		Unicode = 1 << 0,
		[Description("x")]
		Executable = 1 << 1,
		[Description("w")]
		Writeable = 1 << 2,
		[Description("m")]
		LocalModTimes = 1 << 3,
		[Description("k")]
		RCS = 1 << 4,
		[Description("l")]
		Exclusive = 1 << 5,
		[Description("D")]
		DeltasPerRevision = 1 << 6,
		[Description("F")]
		Uncompressed = 1 << 7,
		[Description("C")]
		Compressed = 1 << 8,
		[Description("X")]
		Archive = 1 << 9,
		[Description("S")]
		Revisions = 1 << 10,
	}

	public enum P4Action
	{
		[Description("none")]
		None,
		[Description("add")]
		Add,
		[Description("edit")]
		Edit,
		[Description("delete")]
		Delete,
		[Description("branch")]
		Branch,
		[Description("move/add")]
		MoveAdd,
		[Description("move/delete")]
		MoveDelete,
		[Description("integrate")]
		Integrate,
		[Description("import")]
		Import,
		[Description("purge")]
		Purge,
		[Description("archive")]
		Archive,
		[Description("unknown")]
		Unknown,
	}

	public struct P4FileStat
	{
		public P4FileType Type;
		public P4FileAttributes Attributes;
		public P4Action Action;
		public string Change;
		public bool IsOldType;

		public P4FileStat(P4FileType Type, P4FileAttributes Attributes, P4Action Action)
		{
			this.Type = Type;
			this.Attributes = Attributes;
			this.Action = Action;
			this.Change = String.Empty;
			this.IsOldType = false;
		}

		public static readonly P4FileStat Invalid = new P4FileStat(P4FileType.Unknown, P4FileAttributes.None, P4Action.None);

		public bool IsValid { get { return Type != P4FileType.Unknown; } }
	}

	public class P4WhereRecord
	{
		public bool bUnmap;
		public string DepotFile;
		public string ClientFile;
		public string Path;
	}

	public class P4Spec
	{
		public List<KeyValuePair<string, string>> Sections;

		/// <summary>
		/// Default constructor.
		/// </summary>
		public P4Spec()
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
		public static P4Spec FromString(string Text)
		{
			P4Spec Spec = new P4Spec();

			string[] Lines = Text.Split('\n');
			for(int LineIdx = 0; LineIdx < Lines.Length; LineIdx++)
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
						throw new P4Exception("Invalid spec format at line {0}: \"{1}\"", LineIdx, Lines[LineIdx]);
					}

					// Get the section name
					string SectionName = Lines[LineIdx].Substring(0, SeparatorIdx);

					// Parse the section value
					StringBuilder Value = new StringBuilder(Lines[LineIdx].Substring(SeparatorIdx + 1));
					for(; LineIdx + 1 < Lines.Length; LineIdx++)
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

			return Spec;
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

	/// <summary>
	/// Describes the action performed by the user when resolving the integration
	/// </summary>
	public enum P4IntegrateAction
	{
		/// <summary>
		/// file did not previously exist; it was created as a copy of partner-file
		/// </summary>
		[Description("branch from")]
		BranchFrom,

		/// <summary>
		/// partner-file did not previously exist; it was created as a copy of file.
		/// </summary>
		[Description("branch into")]
		BranchInto,

		/// <summary>
		/// file was integrated from partner-file, accepting merge.
		/// </summary>
		[Description("merge from")]
		MergeFrom,

		/// <summary>
		/// file was integrated into partner-file, accepting merge.
		/// </summary>
		[Description("merge into")]
		MergeInto,

		/// <summary>
		/// file was integrated from partner-file, accepting theirs and deleting the original.
		/// </summary>
		[Description("moved from")]
		MovedFrom,

		/// <summary>
		/// file was integrated into partner-file, accepting theirs and creating partner-file if it did not previously exist.
		/// </summary>
		[Description("moved into")]
		MovedInto,

		/// <summary>
		/// file was integrated from partner-file, accepting theirs.
		/// </summary>
		[Description("copy from")]
		CopyFrom,

		/// <summary>
		/// file was integrated into partner-file, accepting theirs.
		/// </summary>
		[Description("copy into")]
		CopyInto,

		/// <summary>
		/// file was integrated from partner-file, accepting yours.
		/// </summary>
		[Description("ignored")]
		Ignored,

		/// <summary>
		/// file was integrated into partner-file, accepting yours.
		/// </summary>
		[Description("ignored by")]
		IgnoredBy,

		/// <summary>
		/// file was integrated from partner-file, and partner-file had been previously deleted.
		/// </summary>
		[Description("delete from")]
		DeleteFrom,

		/// <summary>
		/// file was integrated into partner-file, and file had been previously deleted.
		/// </summary>
		[Description("delete into")]
		DeleteInto,

		/// <summary>
		/// file was integrated from partner-file, and file was edited within the p4 resolve process.
		/// </summary>
		[Description("edit from")]
		EditFrom,

		/// <summary>
		/// file was integrated into partner-file, and partner-file was reopened for edit before submission.
		/// </summary>
		[Description("edit into")]
		EditInto,

		/// <summary>
		/// file was integrated from a deleted partner-file, and partner-file was reopened for add (that is, someone restored a deleted file by syncing back to a pre-deleted revision and adding the file).
		/// </summary>
		[Description("add from")]
		AddFrom,

		/// <summary>
		/// file was integrated into previously nonexistent partner-file, and partner-file was reopened for add before submission.
		/// </summary>
		[Description("add into")]
		AddInto,
	}

	/// <summary>
	/// Stores integration information for a file revision
	/// </summary>
	public class P4IntegrationRecord
	{
		/// <summary>
		/// The integration action performed for this file
		/// </summary>
		public readonly P4IntegrateAction Action;

		/// <summary>
		/// The partner file for this integration
		/// </summary>
		public readonly string OtherFile;

		/// <summary>
		/// Min revision of the partner file for this integration
		/// </summary>
		public readonly int StartRevisionNumber;

		/// <summary>
		/// Max revision of the partner file for this integration
		/// </summary>
		public readonly int EndRevisionNumber;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Action">The integration action</param>
		/// <param name="OtherFile">The partner file involved in the integration</param>
		/// <param name="StartRevisionNumber">Starting revision of the partner file for the integration (exclusive)</param>
		/// <param name="EndRevisionNumber">Ending revision of the partner file for the integration (inclusive)</param>
		public P4IntegrationRecord(P4IntegrateAction Action, string OtherFile, int StartRevisionNumber, int EndRevisionNumber)
		{
			this.Action = Action;
			this.OtherFile = OtherFile;
			this.StartRevisionNumber = StartRevisionNumber;
			this.EndRevisionNumber = EndRevisionNumber;
		}

		/// <summary>
		/// Summarize this record for display in the debugger
		/// </summary>
		/// <returns>Formatted integration record</returns>
		public override string ToString()
		{
			if(StartRevisionNumber + 1 == EndRevisionNumber)
			{
				return String.Format("{0} {1}#{2}", Action, OtherFile, EndRevisionNumber);
			}
			else
			{
				return String.Format("{0} {1}#{2},#{3}", Action, OtherFile, StartRevisionNumber + 1, EndRevisionNumber);
			}
		}
	}

	/// <summary>
	/// Stores a revision record for a file
	/// </summary>
	public class P4RevisionRecord
	{
		/// <summary>
		/// The revision number of this file
		/// </summary>
		public readonly int RevisionNumber;

		/// <summary>
		/// The changelist responsible for this revision of the file
		/// </summary>
		public readonly int ChangeNumber;

		/// <summary>
		/// Action performed to the file in this revision
		/// </summary>
		public readonly P4Action Action;

		/// <summary>
		/// Type of the file
		/// </summary>
		public readonly string Type;

		/// <summary>
		/// Timestamp of this modification
		/// </summary>
		public readonly DateTime DateTime;

		/// <summary>
		/// Author of the changelist
		/// </summary>
		public readonly string UserName;

		/// <summary>
		/// Client that submitted this changelist
		/// </summary>
		public readonly string ClientName;

		/// <summary>
		/// Size of the file, or -1 if not specified
		/// </summary>
		public readonly int FileSize;

		/// <summary>
		/// Digest of the file, or null if not specified
		/// </summary>
		public readonly string Digest;

		/// <summary>
		/// Description of this changelist
		/// </summary>
		public readonly string Description;

		/// <summary>
		/// Integration records for this revision
		/// </summary>
		public readonly P4IntegrationRecord[] Integrations;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="RevisionNumber">Revision number of the file</param>
		/// <param name="ChangeNumber">Number of the changelist that submitted this revision</param>
		/// <param name="Action">Action performed to the file in this changelist</param>
		/// <param name="Type">Type of the file</param>
		/// <param name="DateTime">Timestamp for the change</param>
		/// <param name="UserName">User that submitted the change</param>
		/// <param name="ClientName">Client that submitted the change</param>
		/// <param name="FileSize">Size of the file, or -1 if not specified</param>
		/// <param name="Digest">Digest of the file, or null if not specified</param>
		/// <param name="Description">Description of the changelist</param>
		/// <param name="Integrations">Integrations performed to the file</param>
		public P4RevisionRecord(int RevisionNumber, int ChangeNumber, P4Action Action, string Type, DateTime DateTime, string UserName, string ClientName, int FileSize, string Digest, string Description, P4IntegrationRecord[] Integrations)
		{
			this.RevisionNumber = RevisionNumber;
			this.ChangeNumber = ChangeNumber;
			this.Action = Action;
			this.Type = Type;
			this.DateTime = DateTime;
			this.UserName = UserName;
			this.ClientName = ClientName;
			this.Description = Description;
			this.FileSize = FileSize;
			this.Digest = Digest;
			this.Integrations = Integrations;
		}

		/// <summary>
		/// Format this record for display in the debugger
		/// </summary>
		/// <returns>Summary of this revision</returns>
		public override string ToString()
		{
			return String.Format("#{0} change {1} {2} on {3} by {4}@{5}", RevisionNumber, ChangeNumber, Action, DateTime, UserName, ClientName);
		}
	}

	/// <summary>
	/// Record output by the filelog command
	/// </summary>
	public class P4FileRecord
	{
		/// <summary>
		/// Path to the file in the depot
		/// </summary>
		public string DepotPath;

		/// <summary>
		/// Revisions of this file
		/// </summary>
		public P4RevisionRecord[] Revisions;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="DepotPath">The depot path of the file</param>
		/// <param name="Revisions">Revisions of the file</param>
		public P4FileRecord(string DepotPath, P4RevisionRecord[] Revisions)
		{
			this.DepotPath = DepotPath;
			this.Revisions = Revisions;
		}

		/// <summary>
		/// Return the depot path of the file for display in the debugger
		/// </summary>
		/// <returns>Path to the file</returns>
		public override string ToString()
		{
			return DepotPath;
		}
	}

	/// <summary>
	/// Options for the filelog command
	/// </summary>
	[Flags]
	public enum P4FileLogOptions
	{
		/// <summary>
		/// No options
		/// </summary>
		None = 0,

		/// <summary>
		/// Display file content history instead of file name history.
		/// </summary>
		ContentHistory = 1,

		/// <summary>
		/// Follow file history across branches.
		/// </summary>
		FollowAcrossBranches = 2,

		/// <summary>
		/// List long output, with the full text of each changelist description.
		/// </summary>
		FullDescriptions = 4,

		/// <summary>
		/// List long output, with the full text of each changelist description truncated at 250 characters.
		/// </summary>
		LongDescriptions = 8,

		/// <summary>
		/// When used with the ContentHistory option, do not follow content of promoted task streams. 
		/// </summary>
		DoNotFollowPromotedTaskStreams = 16,

		/// <summary>
		/// Display a shortened form of output by ignoring non-contributory integrations
		/// </summary>
		IgnoreNonContributoryIntegrations = 32,
	}

	/// <summary>
	/// Type of a Perforce stream
	/// </summary>
	public enum P4StreamType
	{
		/// <summary>
		/// A mainline stream
		/// </summary>
		Mainline,

		/// <summary>
		/// A development stream
		/// </summary>
		Development,

		/// <summary>
		/// A release stream
		/// </summary>
		Release,

		/// <summary>
		/// A virtual stream
		/// </summary>
		Virtual,

		/// <summary>
		/// A task stream
		/// </summary>
		Task,
	}

	/// <summary>
	/// Options for a stream definition
	/// </summary>
	[Flags]
	public enum P4StreamOptions
	{
		/// <summary>
		/// The stream is locked
		/// </summary>
		Locked = 1,

		/// <summary>
		/// Only the owner may submit to the stream
		/// </summary>
		OwnerSubmit = 4,

		/// <summary>
		/// Integrations from this stream to its parent are expected
		/// </summary>
		ToParent = 4,

		/// <summary>
		/// Integrations from this stream from its parent are expected
		/// </summary>
		FromParent = 8,

		/// <summary>
		/// Undocumented?
		/// </summary>
		MergeDown = 16,
	}

	/// <summary>
	/// Contains information about a stream, as returned by the 'p4 streams' command
	/// </summary>
	[DebuggerDisplay("{Stream}")]
	public class P4StreamRecord
	{
		/// <summary>
		/// Path to the stream
		/// </summary>
		public string Stream;

		/// <summary>
		/// Last time the stream definition was updated
		/// </summary>
		public DateTime Update;

		/// <summary>
		/// Last time the stream definition was accessed
		/// </summary>
		public DateTime Access;

		/// <summary>
		/// Owner of this stream
		/// </summary>
		public string Owner;

		/// <summary>
		/// Name of the stream. This may be modified after the stream is initially created, but it's underlying depot path will not change.
		/// </summary>
		public string Name;

		/// <summary>
		/// The parent stream
		/// </summary>
		public string Parent;

		/// <summary>
		/// Type of the stream
		/// </summary>
		public P4StreamType Type;

		/// <summary>
		/// User supplied description of the stream
		/// </summary>
		public string Description;

		/// <summary>
		/// Options for the stream definition
		/// </summary>
		public P4StreamOptions Options;

		/// <summary>
		/// Whether this stream is more stable than the parent stream
		/// </summary>
		public Nullable<bool> FirmerThanParent;

		/// <summary>
		/// Whether changes from this stream flow to the parent stream
		/// </summary>
		public bool ChangeFlowsToParent;

		/// <summary>
		/// Whether changes from this stream flow from the parent stream
		/// </summary>
		public bool ChangeFlowsFromParent;

		/// <summary>
		/// The mainline branch associated with this stream
		/// </summary>
		public string BaseParent;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Stream">Path to the stream</param>
		/// <param name="Update">Last time the stream definition was updated</param>
		/// <param name="Access">Last time the stream definition was accessed</param>
		/// <param name="Owner">Owner of this stream</param>
		/// <param name="Name">Name of the stream. This may be modified after the stream is initially created, but it's underlying depot path will not change.</param>
		/// <param name="Parent">The parent stream</param>
		/// <param name="Type">Type of the stream</param>
		/// <param name="Description">User supplied description of the stream</param>
		/// <param name="Options">Options for the stream definition</param>
		/// <param name="FirmerThanParent">Whether this stream is more stable than the parent stream</param>
		/// <param name="ChangeFlowsToParent">Whether changes from this stream flow to the parent stream</param>
		/// <param name="ChangeFlowsFromParent">Whether changes from this stream flow from the parent stream</param>
		/// <param name="BaseParent">The mainline branch associated with this stream</param>
		public P4StreamRecord(string Stream, DateTime Update, DateTime Access, string Owner, string Name, string Parent, P4StreamType Type, string Description, P4StreamOptions Options, Nullable<bool> FirmerThanParent, bool ChangeFlowsToParent, bool ChangeFlowsFromParent, string BaseParent)
		{
			this.Stream = Stream;
			this.Update = Update;
			this.Owner = Owner;
			this.Name = Name;
			this.Parent = Parent;
			this.Type = Type;
			this.Description = Description;
			this.Options = Options;
			this.FirmerThanParent = FirmerThanParent;
			this.ChangeFlowsToParent = ChangeFlowsToParent;
			this.ChangeFlowsFromParent = ChangeFlowsFromParent;
			this.BaseParent = BaseParent;
		}

		/// <summary>
		/// Return the path of this stream for display in the debugger
		/// </summary>
		/// <returns>Path to this stream</returns>
		public override string ToString()
		{
			return Stream;
		}
	}

	/// <summary>
	/// Error severity codes. Taken from the p4java documentation.
	/// </summary>
	public enum P4SeverityCode
	{
		Empty = 0,
		Info = 1,
		Warning = 2,
		Failed = 3,
		Fatal = 4,
	}

	/// <summary>
	/// Generic error codes that can be returned by the Perforce server. Taken from the p4java documentation.
	/// </summary>
	public enum P4GenericCode
	{
		None = 0,
		Usage = 1,
		Unknown = 2,
		Context = 3,
		Illegal = 4,
		NotYet = 5,
		Protect = 6,
		Empty = 17,
		Fault = 33,
		Client = 34,
		Admin = 35,
		Config = 36,
		Upgrade = 37,
		Comm = 38,
		TooBig = 39, 
	}

	/// <summary>
	/// Represents a error return value from Perforce.
	/// </summary>
	public class P4ReturnCode
	{
		/// <summary>
		/// The value of the "code" field returned by the server
		/// </summary>
		public string Code;

		/// <summary>
		/// The severity of this error
		/// </summary>
		public P4SeverityCode Severity;

		/// <summary>
		/// The generic error code associated with this message
		/// </summary>
		public P4GenericCode Generic;

		/// <summary>
		/// The message text
		/// </summary>
		public string Message;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Code">The value of the "code" field returned by the server</param>
		/// <param name="Severity">The severity of this error</param>
		/// <param name="Generic">The generic error code associated with this message</param>
		/// <param name="Message">The message text</param>
		public P4ReturnCode(string Code, P4SeverityCode Severity, P4GenericCode Generic, string Message)
		{
			this.Code = Code;
			this.Severity = Severity;
			this.Generic = Generic;
			this.Message = Message;
		}

		/// <summary>
		/// Formats this error for display in the debugger
		/// </summary>
		/// <returns>String representation of this object</returns>
		public override string ToString()
		{
			return String.Format("{0}: {1} (Generic={2})", Code, Message, Generic);
		}
	}

	public partial class CommandUtils
	{
		#region Environment Setup

		static private P4Connection PerforceConnection;
		static private P4Environment PerforceEnvironment;

		/// <summary>
		/// BuildEnvironment to use for this buildcommand. This is initialized by InitBuildEnvironment. As soon
		/// as the script execution in ExecuteBuild begins, the BuildEnv is set up and ready to use.
		/// </summary>
		static public P4Connection P4
		{
			get
			{
				if (PerforceConnection == null)
				{
					throw new AutomationException("Attempt to use P4 before it was initialized or P4 support is disabled.");
				}
				return PerforceConnection;
			}
		}

		/// <summary>
		/// BuildEnvironment to use for this buildcommand. This is initialized by InitBuildEnvironment. As soon
		/// as the script execution in ExecuteBuild begins, the BuildEnv is set up and ready to use.
		/// </summary>
		static public P4Environment P4Env
		{
			get
			{
				if (PerforceEnvironment == null)
				{
					throw new AutomationException("Attempt to use P4Environment before it was initialized or P4 support is disabled.");
				}
				return PerforceEnvironment;
			}
		}

		/// <summary>
		/// Initializes build environment. If the build command needs a specific env-var mapping or
		/// has an extended BuildEnvironment, it must implement this method accordingly.
		/// </summary>
		static internal void InitP4Environment()
		{
			// Temporary connection - will use only the currently set env vars to connect to P4
			PerforceEnvironment = new P4Environment(CmdEnv);
		}

		/// <summary>
		/// Initializes default source control connection.
		/// </summary>
		static internal void InitDefaultP4Connection()
		{
			PerforceConnection = new P4Connection(User: P4Env.User, Client: P4Env.Client, ServerAndPort: P4Env.ServerAndPort);
		}

		#endregion

		/// <summary>
		/// Check if P4 is supported.
		/// </summary>		
		public static bool P4Enabled
		{
			get
			{
				if (!bP4Enabled.HasValue)
				{
					throw new AutomationException("Trying to access P4Enabled property before it was initialized.");
				}
				return (bool)bP4Enabled;
			}
			private set
			{
				bP4Enabled = value;
			}
		}
		private static bool? bP4Enabled;

		/// <summary>
		/// Check if P4CL is required.
		/// </summary>		
		public static bool P4CLRequired
		{
			get
			{
				if (!bP4CLRequired.HasValue)
				{
					throw new AutomationException("Trying to access P4CLRequired property before it was initialized.");
				}
				return (bool)bP4CLRequired;
			}
			private set
			{
				bP4CLRequired = value;
			}
		}
		private static bool? bP4CLRequired;

		/// <summary>
		/// Checks whether commands are allowed to submit files into P4.
		/// </summary>
		public static bool AllowSubmit
		{
			get
			{
				if (!bAllowSubmit.HasValue)
				{
					throw new AutomationException("Trying to access AllowSubmit property before it was initialized.");
				}
				return (bool)bAllowSubmit;
			}
			private set
			{
				bAllowSubmit = value;
			}

		}
		private static bool? bAllowSubmit;

		/// <summary>
		/// Sets up P4Enabled, AllowSubmit properties. Note that this does not initialize P4 environment.
		/// </summary>
		/// <param name="CommandsToExecute">Commands to execute</param>
		/// <param name="Commands">Commands</param>
		internal static void InitP4Support(List<CommandInfo> CommandsToExecute, Dictionary<string, Type> Commands)
		{
			// Init AllowSubmit
			// If we do not specify on the commandline if submitting is allowed or not, this is 
			// depending on whether we run locally or on a build machine.
			LogVerbose("Initializing AllowSubmit.");
			if (GlobalCommandLine.Submit || GlobalCommandLine.NoSubmit)
			{
				AllowSubmit = GlobalCommandLine.Submit;
			}
			else
			{
				AllowSubmit = Automation.IsBuildMachine;
			}
			LogVerbose("AllowSubmit={0}", AllowSubmit);

			// Init P4Enabled
			LogVerbose("Initializing P4Enabled.");
			if (Automation.IsBuildMachine)
			{
				P4Enabled = !GlobalCommandLine.NoP4;
				P4CLRequired = P4Enabled;
			}
			else
			{
				bool bRequireP4;
				bool bRequireCL;
				CheckIfCommandsRequireP4(CommandsToExecute, Commands, out bRequireP4, out bRequireCL);

				P4Enabled = GlobalCommandLine.P4 || bRequireP4;
				P4CLRequired = GlobalCommandLine.P4 || bRequireCL;
			}
			LogVerbose("P4Enabled={0}", P4Enabled);
			LogVerbose("P4CLRequired={0}", P4CLRequired);
		}

		/// <summary>
		/// Checks if any of the commands to execute has [RequireP4] attribute.
		/// </summary>
		/// <param name="CommandsToExecute">List of commands to be executed.</param>
		/// <param name="Commands">Commands.</param>
		private static void CheckIfCommandsRequireP4(List<CommandInfo> CommandsToExecute, Dictionary<string, Type> Commands, out bool bRequireP4, out bool bRequireCL)
		{
			bRequireP4 = false;
			bRequireCL = false;

			foreach (var CommandInfo in CommandsToExecute)
			{
				Type Command;
				if (Commands.TryGetValue(CommandInfo.CommandName, out Command))
				{
					var RequireP4Attributes = Command.GetCustomAttributes(typeof(RequireP4Attribute), true);	
					if (!CommandUtils.IsNullOrEmpty(RequireP4Attributes))
					{
						if(!GlobalCommandLine.P4)
						{
							LogWarning("Command {0} requires P4 functionality.", Command.Name);
						}
						bRequireP4 = true;

						var DoesNotNeedP4CLAttributes = Command.GetCustomAttributes(typeof(DoesNotNeedP4CLAttribute), true);
						if (CommandUtils.IsNullOrEmpty(DoesNotNeedP4CLAttributes))
						{
							bRequireCL = true;
						}
					}
				}
			}
		}
	}

	/// <summary>
	/// Class that stores labels info.
	/// </summary>
	public class P4Label
	{
		// The name of the label.
		public string Name { get; private set; }

		// The date of the label.
		public DateTime Date { get; private set; }

		public P4Label(string Name, DateTime Date)
		{
			this.Name = Name;
			this.Date = Date;
		}
	}

	/// <summary>
	/// Perforce connection.
	/// </summary>
	public partial class P4Connection
	{
		/// <summary>
		/// List of global options for this connection (client/user)
		/// </summary>
		private string GlobalOptions;
        /// <summary>
        /// List of global options for this connection (client/user)
        /// </summary>
        private string GlobalOptionsWithoutClient;
		/// <summary>
		/// Path where this connection's log is to go to
		/// </summary>
		public string LogPath { get; private set; }

		/// <summary>
		/// Initializes P4 connection
		/// </summary>
		/// <param name="User">Username (can be null, in which case the environment variable default will be used)</param>
		/// <param name="Client">Workspace (can be null, in which case the environment variable default will be used)</param>
		/// <param name="ServerAndPort">Server:Port (can be null, in which case the environment variable default will be used)</param>
		/// <param name="P4LogPath">Log filename (can be null, in which case CmdEnv.LogFolder/p4.log will be used)</param>
		public P4Connection(string User, string Client, string ServerAndPort = null, string P4LogPath = null)
		{
			var UserOpts = String.IsNullOrEmpty(User) ? "" : ("-u" + User + " ");
			var ClientOpts = String.IsNullOrEmpty(Client) ? "" : ("-c" + Client + " ");
			var ServerOpts = String.IsNullOrEmpty(ServerAndPort) ? "" : ("-p" + ServerAndPort + " ");			
			GlobalOptions = UserOpts + ClientOpts + ServerOpts;
            GlobalOptionsWithoutClient = UserOpts + ServerOpts;

			if (P4LogPath == null)
			{
				LogPath = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LogFolder, String.Format("p4.log", Client));
			}
			else
			{
				LogPath = P4LogPath;
			}
		}

		/// <summary>
		/// Shortcut to Run but with P4.exe as the program name.
		/// </summary>
		/// <param name="CommandLine">Command line</param>
		/// <param name="Input">Stdin</param>
		/// <param name="AllowSpew">true for spew</param>
		/// <returns>Exit code</returns>
        public IProcessResult P4(string CommandLine, string Input = null, bool AllowSpew = true, bool WithClient = true, bool SpewIsVerbose = false)
		{
			CommandUtils.ERunOptions RunOptions = AllowSpew ? CommandUtils.ERunOptions.AllowSpew : CommandUtils.ERunOptions.NoLoggingOfRunCommand;
			if( SpewIsVerbose )
			{
				RunOptions |= CommandUtils.ERunOptions.SpewIsVerbose;
			}
            return CommandUtils.Run(HostPlatform.Current.P4Exe, (WithClient ? GlobalOptions : GlobalOptionsWithoutClient) + CommandLine, Input, Options:RunOptions);
		}

		/// <summary>
		/// Calls p4 and returns the output.
		/// </summary>
		/// <param name="Output">Output of the comman.</param>
		/// <param name="CommandLine">Commandline for p4.</param>
		/// <param name="Input">Stdin input.</param>
		/// <param name="AllowSpew">Whether the command should spew.</param>
		/// <returns>True if succeeded, otherwise false.</returns>
        public bool P4Output(out string Output, string CommandLine, string Input = null, bool AllowSpew = true, bool WithClient = true)
		{
			Output = "";

            var Result = P4(CommandLine, Input, AllowSpew, WithClient);

			Output = Result.Output;
			return Result.ExitCode == 0;
		}

		/// <summary>
		/// Calls p4 and returns the output.
		/// </summary>
		/// <param name="Output">Output of the comman.</param>
		/// <param name="CommandLine">Commandline for p4.</param>
		/// <param name="Input">Stdin input.</param>
		/// <param name="AllowSpew">Whether the command should spew.</param>
		/// <returns>True if succeeded, otherwise false.</returns>
        public bool P4Output(out string[] OutputLines, string CommandLine, string Input = null, bool AllowSpew = true, bool WithClient = true)
		{
			string Output;
			bool bResult = P4Output(out Output, CommandLine, Input, AllowSpew, WithClient);

			List<string> Lines = new List<string>();
			for(int Idx = 0; Idx < Output.Length; )
			{
				int EndIdx = Output.IndexOf('\n', Idx);
				if(EndIdx == -1)
				{
					Lines.Add(Output.Substring(Idx));
					break;
				}

				if(EndIdx > Idx && Output[EndIdx - 1] == '\r')
				{
					Lines.Add(Output.Substring(Idx, EndIdx - Idx - 1));
				}
				else
				{
					Lines.Add(Output.Substring(Idx, EndIdx - Idx));
				}

				Idx = EndIdx + 1;
			}
			OutputLines = Lines.ToArray();

			return bResult;
		}

		/// <summary>
		/// Calls p4 command and writes the output to a logfile.
		/// </summary>
		/// <param name="CommandLine">Commandline to pass to p4.</param>
		/// <param name="Input">Stdin input.</param>
		/// <param name="AllowSpew">Whether the command is allowed to spew.</param>
        public void LogP4(string CommandLine, string Input = null, bool AllowSpew = true, bool WithClient = true, bool SpewIsVerbose = false)
		{
			string Output;
            if (!LogP4Output(out Output, CommandLine, Input, AllowSpew, WithClient, SpewIsVerbose:SpewIsVerbose))
			{
				throw new P4Exception("p4.exe {0} failed.", CommandLine);
			}
		}

		/// <summary>
		/// Calls p4 and returns the output and writes it also to a logfile.
		/// </summary>
		/// <param name="Output">Output of the comman.</param>
		/// <param name="CommandLine">Commandline for p4.</param>
		/// <param name="Input">Stdin input.</param>
		/// <param name="AllowSpew">Whether the command should spew.</param>
		/// <returns>True if succeeded, otherwise false.</returns>
        public bool LogP4Output(out string Output, string CommandLine, string Input = null, bool AllowSpew = true, bool WithClient = true, bool SpewIsVerbose = false)
		{
			Output = "";

			if (String.IsNullOrEmpty(LogPath))
			{
				CommandUtils.LogError("P4Utils.SetupP4() must be called before issuing Peforce commands");
				return false;
			}

            var Result = P4(CommandLine, Input, AllowSpew, WithClient, SpewIsVerbose:SpewIsVerbose);

			CommandUtils.WriteToFile(LogPath, CommandLine + "\n");
			CommandUtils.WriteToFile(LogPath, Result.Output);
			Output = Result.Output;
			return Result.ExitCode == 0;
		}

		/// <summary>
		/// Execute a Perforce command and parse the output as marshalled Python objects. This is more robustly defined than the text-based tagged output
		/// format, because it avoids ambiguity when returned fields can have newlines.
		/// </summary>
		/// <param name="CommandLine">Command line to execute Perforce with</param>
		/// <param name="TaggedOutput">List that receives the output records</param>
		/// <param name="WithClient">Whether to include client information on the command line</param>
		public List<Dictionary<string, string>> P4TaggedOutput(string CommandLine, bool WithClient = true)
		{
			// Execute Perforce, consuming the binary output into a memory stream
			MemoryStream MemoryStream = new MemoryStream();
			using (Process Process = new Process())
			{
				Process.StartInfo.FileName = HostPlatform.Current.P4Exe;
				Process.StartInfo.Arguments = String.Format("-G {0} {1}", WithClient? GlobalOptions : GlobalOptionsWithoutClient, CommandLine);

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
						throw new P4Exception("Unexpected data while parsing marshalled output - expected '{'");
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
							throw new P4Exception("Unexpected key field type while parsing marshalled output ({0}) - expected 's'", (int)KeyFieldType);
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
							throw new P4Exception("Unexpected value field type while parsing marshalled output ({0}) - expected 's'", (int)ValueFieldType);
						}
					}
					Records.Add(Record);
				}
			}
			return Records;
		}

		/// <summary>
		/// Checks that the raw record data includes the given return code, or creates a ReturnCode value if it doesn't
		/// </summary>
		/// <param name="RawRecord">The raw record data</param>
		/// <param name="ExpectedCode">The expected code value</param>
		/// <param name="OtherReturnCode">Output variable for receiving the return code if it doesn't match</param>
		public static bool VerifyReturnCode(Dictionary<string, string> RawRecord, string ExpectedCode, out P4ReturnCode OtherReturnCode)
		{
			// Parse the code field
			string Code;
			if(!RawRecord.TryGetValue("code", out Code))
			{
				Code = "unknown";
			}

			// Check whether it matches what we expect
			if(Code == ExpectedCode)
			{
				OtherReturnCode = null;
				return true;
			}
			else
			{
				string Severity;
				if(!RawRecord.TryGetValue("severity", out Severity))
				{
					Severity = ((int)P4SeverityCode.Empty).ToString();
				}

				string Generic;
				if(!RawRecord.TryGetValue("generic", out Generic))
				{
					Generic = ((int)P4GenericCode.None).ToString();
				}

				string Message;
				if(!RawRecord.TryGetValue("data", out Message))
				{
					Message = "No description available.";
				}

				OtherReturnCode = new P4ReturnCode(Code, (P4SeverityCode)int.Parse(Severity), (P4GenericCode)int.Parse(Generic), Message.TrimEnd());
				return false;
			}
		}

		/// <summary>
		/// Invokes p4 login command.
		/// </summary>
		public string GetAuthenticationToken()
		{
			string AuthenticationToken = null;

			string Output;
            string P4Passwd = InternalUtils.GetEnvironmentVariable("uebp_PASS", "", true) + '\n';
            P4Output(out Output, "login -a -p", P4Passwd);

			// Validate output.
			const string PasswordPromptString = "Enter password: \r\n";
			if (Output.Substring(0, PasswordPromptString.Length) == PasswordPromptString)
			{
				int AuthenticationResultStartIndex = PasswordPromptString.Length;
				Regex TokenRegex = new Regex("[0-9A-F]{32}");
				Match TokenMatch = TokenRegex.Match(Output, AuthenticationResultStartIndex);
				if (TokenMatch.Success)
				{
					AuthenticationToken = Output.Substring(TokenMatch.Index, TokenMatch.Length);
				}
			}

			return AuthenticationToken;
		}

        /// <summary>
        /// Invokes p4 changes command.
        /// </summary>
        /// <param name="CommandLine">CommandLine to pass on to the command.</param>
        public class ChangeRecord
        {
            public int CL = 0;
            public string User = "";
            public string UserEmail = "";
            public string Summary = "";
            public static int Compare(ChangeRecord A, ChangeRecord B)
            {
                return (A.CL < B.CL) ? -1 : (A.CL > B.CL) ? 1 : 0;
            }

			public override string ToString()
			{
				return String.Format("CL {0}: {1}", CL, Summary);
			}
		}
        static Dictionary<string, string> UserToEmailCache = new Dictionary<string, string>();
        public string UserToEmail(string User)
        {
            if (UserToEmailCache.ContainsKey(User))
            {
                return UserToEmailCache[User];
            }
            string Result = "";
            try
            {
                var P4Result = P4(String.Format("user -o {0}", User), AllowSpew: false);
			    if (P4Result.ExitCode == 0)
			    {
				    var Tags = ParseTaggedP4Output(P4Result.Output);
                    Tags.TryGetValue("Email", out Result);
                }
            }
            catch(Exception)
            {
            }
            if (Result == "")
            {
				CommandUtils.LogWarning("Could not find email for P4 user {0}", User);
            }
            UserToEmailCache.Add(User, Result);
            return Result;
        }
        static Dictionary<string, List<ChangeRecord>> ChangesCache = new Dictionary<string, List<ChangeRecord>>();
        public bool Changes(out List<ChangeRecord> ChangeRecords, string CommandLine, bool AllowSpew = true, bool UseCaching = false, bool LongComment = false, bool WithClient = false)
        {
            // If the user specified '-l' or '-L', the summary will appear on subsequent lines (no quotes) instead of the same line (surrounded by single quotes)
            bool ContainsDashL = CommandLine.StartsWith("-L ", StringComparison.InvariantCultureIgnoreCase) ||
                CommandLine.IndexOf(" -L ", StringComparison.InvariantCultureIgnoreCase) > 0;
            bool bSummaryIsOnSameLine = !ContainsDashL;
            if (bSummaryIsOnSameLine && LongComment)
            {
                CommandLine = "-L " + CommandLine;
                bSummaryIsOnSameLine = false;
            } 
            if (UseCaching && ChangesCache.ContainsKey(CommandLine))
            {
                ChangeRecords = ChangesCache[CommandLine];
                return true;
            }
            ChangeRecords = new List<ChangeRecord>();
            try
            {
                // Change 1999345 on 2014/02/16 by buildmachine@BuildFarm_BUILD-23_buildmachine_++depot+UE4 'GUBP Node Shadow_LabelPromotabl'

                string Output;
                if (!LogP4Output(out Output, "changes " + CommandLine, null, AllowSpew, WithClient: WithClient))
                {
                    throw new AutomationException("P4 returned failure.");
                }

                var Lines = Output.Split(new string[] { Environment.NewLine }, StringSplitOptions.None);
                for(int LineIndex = 0; LineIndex < Lines.Length; ++LineIndex)
                {
					var Line = Lines[ LineIndex ];

					// If we've hit a blank line, then we're done
					if( String.IsNullOrEmpty( Line ) )
					{
						break;
					}

                    ChangeRecord Change = new ChangeRecord();
                    string MatchChange = "Change ";
                    string MatchOn = " on "; 
                    string MatchBy = " by ";

                    int ChangeAt = Line.IndexOf(MatchChange);
                    int OnAt = Line.IndexOf(MatchOn);
                    int ByAt = Line.IndexOf(MatchBy);
                    if (ChangeAt == 0 && OnAt > ChangeAt && ByAt > OnAt)
                    {
                        var ChangeString = Line.Substring(ChangeAt + MatchChange.Length, OnAt - ChangeAt - MatchChange.Length);
                        Change.CL = int.Parse(ChangeString);

						int AtAt = Line.IndexOf("@");
                        Change.User = Line.Substring(ByAt + MatchBy.Length, AtAt - ByAt - MatchBy.Length);

						if( bSummaryIsOnSameLine )
						{ 
							int TickAt = Line.IndexOf("'");
							int EndTick = Line.LastIndexOf("'");
							if( TickAt > ByAt && EndTick > TickAt )
							{ 
								Change.Summary = Line.Substring(TickAt + 1, EndTick - TickAt - 1);
							}
						}
						else
						{
							++LineIndex;
							if( LineIndex >= Lines.Length )
							{
								throw new AutomationException("Was expecting a change summary to appear after Change header output from P4, but there were no more lines to read");
							}

							Line = Lines[ LineIndex ];
							if( !String.IsNullOrEmpty( Line ) )
							{
                                throw new AutomationException("Was expecting blank line after Change header output from P4, got {0}", Line);
							}

							++LineIndex;
							for( ; LineIndex < Lines.Length; ++LineIndex )
							{
								Line = Lines[ LineIndex ];

								int SummaryChangeAt = Line.IndexOf(MatchChange);
								int SummaryOnAt = Line.IndexOf(MatchOn);
								int SummaryByAt = Line.IndexOf(MatchBy);
								if (SummaryChangeAt == 0 && SummaryOnAt > SummaryChangeAt && SummaryByAt > SummaryOnAt)
								{
									// OK, we found a new change. This isn't part of our summary.  We're done with the summary.  Back we go.
                                    //CommandUtils.Log("Next summary is {0}", Line);
									--LineIndex;
									break;
								}

								// Summary lines are supposed to begin with a single tab character (even empty lines)
								if( !String.IsNullOrEmpty( Line ) && Line[0] != '\t' )
								{
									throw new AutomationException("Was expecting every line of the P4 changes summary to start with a tab character or be totally empty");
								}

								// Remove the tab
								var SummaryLine = Line;
								if( Line.StartsWith( "\t" ) )
								{ 
									SummaryLine = Line.Substring( 1 );
								}

								// Add a CR if we already had some summary text
								if( !String.IsNullOrEmpty( Change.Summary ) )
								{
									Change.Summary += "\n";
								}

								// Append the summary line!
								Change.Summary += SummaryLine;
							}
						}
                        Change.UserEmail = UserToEmail(Change.User);
                        ChangeRecords.Add(Change);
                    }
					else
					{
						throw new AutomationException("Output of 'p4 changes' was not formatted how we expected.  Could not find 'Change', 'on' and 'by' in the output line: " + Line);
					}
                }
            }
			catch (Exception Ex)
            {
				CommandUtils.LogWarning("Unable to get P4 changes with {0}", CommandLine);
				CommandUtils.LogWarning(" Exception was {0}", LogUtils.FormatException(Ex));
                return false;
            }
            ChangeRecords.Sort((A, B) => ChangeRecord.Compare(A, B));
			if( ChangesCache.ContainsKey(CommandLine) )
			{
				ChangesCache[CommandLine] = ChangeRecords;
			}
			else
			{ 
				ChangesCache.Add(CommandLine, ChangeRecords);
			}
            return true;
        }

	
        public class DescribeRecord
        {
            public int CL = 0;
            public string User = "";
            public string UserEmail = "";
            public string Summary = "";
			public string Header = "";
			
			public class DescribeFile
			{
				public string File;
				public int Revision;
				public string ChangeType;

				public override string ToString()
				{
					return String.Format("{0}#{1} ({2})", File, Revision, ChangeType);
				}
			}
			public List<DescribeFile> Files = new List<DescribeFile>();
            
			public static int Compare(DescribeRecord A, DescribeRecord B)
            {
                return (A.CL < B.CL) ? -1 : (A.CL > B.CL) ? 1 : 0;
            }

			public override string ToString()
			{
				return String.Format("CL {0}: {1}", CL, Summary);
			}
		}

		/// <summary>
		/// Wraps P4 describe
		/// </summary>
		/// <param name="Changelist">Changelist numbers to query full descriptions for</param>
		/// <param name="DescribeRecord">Describe record for the given changelist.</param>
		/// <param name="AllowSpew"></param>
		/// <returns>True if everything went okay</returns>
        public bool DescribeChangelist(int Changelist, out DescribeRecord DescribeRecord, bool AllowSpew = true)
        {
			List<DescribeRecord> DescribeRecords;
			if(!DescribeChangelists(new List<int>{ Changelist }, out DescribeRecords, AllowSpew))
			{
				DescribeRecord = null;
				return false;
			}
			else if(DescribeRecords.Count != 1)
			{
				DescribeRecord = null;
				return false;
			}
			else 
			{
				DescribeRecord = DescribeRecords[0];
				return true;
			}
		}

		/// <summary>
		/// Wraps P4 describe
		/// </summary>
		/// <param name="Changelists">List of changelist numbers to query full descriptions for</param>
		/// <param name="DescribeRecords">List of records we found.  One for each changelist number.  These will be sorted from oldest to newest.</param>
		/// <param name="AllowSpew"></param>
		/// <returns>True if everything went okay</returns>
        public bool DescribeChangelists(List<int> Changelists, out List<DescribeRecord> DescribeRecords, bool AllowSpew = true)
        {
			DescribeRecords = new List<DescribeRecord>();
            try
            {
				// Change 234641 by This.User@WORKSPACE-C2Q-67_Dev on 2008/05/06 10:32:32
				// 
				//         Desc Line 1
				// 
				// Affected files ...
				// 
				// ... //depot/UnrealEngine3/Development/Src/Engine/Classes/ArrowComponent.uc#8 edit
				// ... //depot/UnrealEngine3/Development/Src/Engine/Classes/DecalActorBase.uc#4 edit


				string Output;
				string CommandLine = "-s";		// Don't automatically diff the files
				
				// Add changelists to the command-line
				foreach( var Changelist in Changelists )
				{
					CommandLine += " " + Changelist.ToString();
				}

                if (!LogP4Output(out Output, "describe " + CommandLine, null, AllowSpew))
                {
                    return false;
                }

				int ChangelistIndex = 0;
				var Lines = Output.Split(new string[] { Environment.NewLine }, StringSplitOptions.None);
                for (var LineIndex = 0; LineIndex < Lines.Length; ++LineIndex)
                {
					var Line = Lines[ LineIndex ];

					// If we've hit a blank line, then we're done
					if( String.IsNullOrEmpty( Line ) )
					{
						break;
					}

                    string MatchChange = "Change ";
                    string MatchOn = " on "; 
                    string MatchBy = " by ";

                    int ChangeAt = Line.IndexOf(MatchChange);
                    int OnAt = Line.IndexOf(MatchOn);
                    int ByAt = Line.IndexOf(MatchBy);
                    int AtAt = Line.IndexOf("@");
                    if (ChangeAt == 0 && OnAt > ChangeAt && ByAt < OnAt)
                    {
                        var ChangeString = Line.Substring(ChangeAt + MatchChange.Length, ByAt - ChangeAt - MatchChange.Length);

						var CurrentChangelist = Changelists[ ChangelistIndex++ ];

                        if (!ChangeString.Equals( CurrentChangelist.ToString()))
                        {
                            throw new AutomationException("Was expecting changelists to be reported back in the same order we asked for them (CL {0} != {1})", ChangeString, CurrentChangelist.ToString());
                        }

						var DescribeRecord = new DescribeRecord();
						DescribeRecords.Add( DescribeRecord );

						DescribeRecord.CL = CurrentChangelist;
                        DescribeRecord.User = Line.Substring(ByAt + MatchBy.Length, AtAt - ByAt - MatchBy.Length);
						DescribeRecord.Header = Line;

						++LineIndex;
						if( LineIndex >= Lines.Length )
						{
							throw new AutomationException("Was expecting a change summary to appear after Change header output from P4, but there were no more lines to read");
						}

						Line = Lines[ LineIndex ];
						if( !String.IsNullOrEmpty( Line ) )
						{
							throw new AutomationException("Was expecting blank line after Change header output from P4");
						}

						// Summary
						++LineIndex;
						for( ; LineIndex < Lines.Length; ++LineIndex )
						{
							Line = Lines[ LineIndex ];
							if(Line.Length > 0)
							{
								// Stop once we reach a line that doesn't begin with a tab. It's possible (through changelist descriptions that contain embedded newlines, like \r\r\n on Windows) to get 
								// empty lines that don't begin with a tab as we expect.
								if(Line[0] != '\t')
								{
									break;
								}

								// Remove the tab
								var SummaryLine = Line.Substring( 1 );

								// Add a CR if we already had some summary text
								if( !String.IsNullOrEmpty( DescribeRecord.Summary ) )
								{
									DescribeRecord.Summary += "\n";
								}

								// Append the summary line!
								DescribeRecord.Summary += SummaryLine;
							}
						}

						// Remove any trailing newlines from the end of the summary
						DescribeRecord.Summary = DescribeRecord.Summary.TrimEnd('\n');

						Line = Lines[ LineIndex ];

						string MatchAffectedFiles = "Affected files";
						int AffectedFilesAt = Line.IndexOf(MatchAffectedFiles);
						if( AffectedFilesAt == 0 )
						{
							++LineIndex;
							if( LineIndex >= Lines.Length )
							{
								throw new AutomationException("Was expecting a list of files to appear after Affected Files header output from P4, but there were no more lines to read");
							}

							Line = Lines[ LineIndex ];
							if( !String.IsNullOrEmpty( Line ) )
							{
								throw new AutomationException("Was expecting blank line after Affected Files header output from P4");
							}

							// Files
							++LineIndex;
							for( ; LineIndex < Lines.Length; ++LineIndex )
							{
								Line = Lines[ LineIndex ];

								if( String.IsNullOrEmpty( Line ) )
								{
									// Summaries end with a blank line (no tabs)
									break;
								}

								// File lines are supposed to begin with a "... " string
								if( !Line.StartsWith( "... " ) )
								{
									throw new AutomationException("Was expecting every line of the P4 describe files to start with a tab character");
								}

								// Remove the "... " prefix
								var FilesLine = Line.Substring( 4 );

								var DescribeFile = new DescribeRecord.DescribeFile();
								DescribeRecord.Files.Add( DescribeFile );
 							
								// Find the revision #
								var RevisionNumberAt = FilesLine.LastIndexOf( "#" ) + 1;
								var ChangeTypeAt = 1 + FilesLine.IndexOf( " ", RevisionNumberAt );
							
								DescribeFile.File = FilesLine.Substring( 0, RevisionNumberAt - 1 );
								string RevisionString = FilesLine.Substring( RevisionNumberAt, ChangeTypeAt - RevisionNumberAt );
								DescribeFile.Revision = int.Parse( RevisionString );
								DescribeFile.ChangeType = FilesLine.Substring( ChangeTypeAt );															  
							}
						}
						else
						{
							throw new AutomationException("Output of 'p4 describe' was not formatted how we expected.  Could not find 'Affected files' in the output line: " + Line);
						}

                        DescribeRecord.UserEmail = UserToEmail(DescribeRecord.User);
                    }
					else
					{
						throw new AutomationException("Output of 'p4 describe' was not formatted how we expected.  Could not find 'Change', 'on' and 'by' in the output line: " + Line);
					}
                }
            }
            catch (Exception)
            {
                return false;
            }
            DescribeRecords.Sort((A, B) => DescribeRecord.Compare(A, B));
            return true;
        }

		/// <summary>
		/// Invokes p4 sync command.
		/// </summary>
		/// <param name="CommandLine">CommandLine to pass on to the command.</param>
        public void Sync(string CommandLine, bool AllowSpew = true, bool SpewIsVerbose = false)
		{
			LogP4("sync " + CommandLine, null, AllowSpew, SpewIsVerbose:SpewIsVerbose);
		}

		/// <summary>
		/// Invokes p4 unshelve command.
		/// </summary>
		/// <param name="FromCL">Changelist to unshelve.</param>
		/// <param name="ToCL">Changelist where the checked out files should be added.</param>
		/// <param name="CommandLine">Commandline for the command.</param>
		public void Unshelve(int FromCL, int ToCL, string CommandLine = "", bool SpewIsVerbose = false)
		{
			LogP4("unshelve " + String.Format("-s {0} ", FromCL) + String.Format("-c {0} ", ToCL) + CommandLine, SpewIsVerbose: SpewIsVerbose);
		}

        /// <summary>
        /// Invokes p4 unshelve command.
        /// </summary>
        /// <param name="FromCL">Changelist to unshelve.</param>
        /// <param name="ToCL">Changelist where the checked out files should be added.</param>
        /// <param name="CommandLine">Commandline for the command.</param>
        public void Shelve(int FromCL, string CommandLine = "", bool AllowSpew = true)
        {
            LogP4("shelve " + String.Format("-r -c {0} ", FromCL) + CommandLine, AllowSpew: AllowSpew);
        }

		/// <summary>
        /// Deletes shelved files from a changelist
		/// </summary>
        /// <param name="FromCL">Changelist to unshelve.</param>
        /// <param name="CommandLine">Commandline for the command.</param>
        public void DeleteShelvedFiles(int FromCL, bool AllowSpew = true)
        {
			string Output;
            if (!LogP4Output(out Output, String.Format("shelve -d -c {0}", FromCL), AllowSpew: AllowSpew) && !Output.StartsWith("No shelved files in changelist to delete."))
			{
				throw new P4Exception("Couldn't unshelve files: {0}", Output);
			}
        }

		/// <summary>
		/// Invokes p4 edit command.
		/// </summary>
		/// <param name="CL">Changelist where the checked out files should be added.</param>
		/// <param name="CommandLine">Commandline for the command.</param>
		public void Edit(int CL, string CommandLine)
		{
			LogP4("edit " + String.Format("-c {0} ", CL) + CommandLine);
		}

		/// <summary>
		/// Invokes p4 edit command, no exceptions
		/// </summary>
		/// <param name="CL">Changelist where the checked out files should be added.</param>
		/// <param name="CommandLine">Commandline for the command.</param>
		public bool Edit_NoExceptions(int CL, string CommandLine)
		{
			try
			{
				string Output;
				if (!LogP4Output(out Output, "edit " + String.Format("-c {0} ", CL) + CommandLine, null, true))
				{
					return false;
				}
				if (Output.IndexOf("- opened for edit") < 0)
				{
					return false;
				}
				return true;
			}
			catch (Exception)
			{
				return false;
			}
		}

		/// <summary>
		/// Invokes p4 add command.
		/// </summary>
		/// <param name="CL">Changelist where the files should be added to.</param>
		/// <param name="CommandLine">Commandline for the command.</param>
		public void Add(int CL, string CommandLine)
		{
			LogP4("add " + String.Format("-c {0} ", CL) + CommandLine);
		}

		/// <summary>
		/// Invokes p4 reconcile command.
		/// </summary>
		/// <param name="CL">Changelist to check the files out.</param>
		/// <param name="CommandLine">Commandline for the command.</param>
		public void Reconcile(int CL, string CommandLine, bool AllowSpew = true)
		{
			LogP4("reconcile " + String.Format("-c {0} -ead -f ", CL) + CommandLine, AllowSpew: AllowSpew);
		}

        /// <summary>
        /// Invokes p4 reconcile command.
        /// </summary>
        /// <param name="CL">Changelist to check the files out.</param>
        /// <param name="CommandLine">Commandline for the command.</param>
        public void ReconcilePreview(string CommandLine)
        {
            LogP4("reconcile " + String.Format("-ead -n ") + CommandLine);
        }

		/// <summary>
		/// Invokes p4 reconcile command.
		/// Ignores files that were removed.
		/// </summary>
		/// <param name="CL">Changelist to check the files out.</param>
		/// <param name="CommandLine">Commandline for the command.</param>
		public void ReconcileNoDeletes(int CL, string CommandLine, bool AllowSpew = true)
		{
			LogP4("reconcile " + String.Format("-c {0} -ea ", CL) + CommandLine, AllowSpew: AllowSpew);
		}

		/// <summary>
		/// Invokes p4 resolve command.
		/// Resolves all files by accepting yours and ignoring theirs.
		/// </summary>
		/// <param name="CL">Changelist to resolve.</param>
		/// <param name="CommandLine">Commandline for the command.</param>
		public void Resolve(int CL, string CommandLine)
		{
			LogP4("resolve -ay " + String.Format("-c {0} ", CL) + CommandLine);
		}

		/// <summary>
		/// Invokes revert command.
		/// </summary>
		/// <param name="CommandLine">Commandline for the command.</param>
		public void Revert(string CommandLine, bool AllowSpew = true)
		{
			LogP4("revert " + CommandLine, AllowSpew: AllowSpew);
		}

		/// <summary>
		/// Invokes revert command.
		/// </summary>
		/// <param name="CL">Changelist to revert</param>
		/// <param name="CommandLine">Commandline for the command.</param>
		public void Revert(int CL, string CommandLine = "", bool AllowSpew = true)
		{
			LogP4("revert " + String.Format("-c {0} ", CL) + CommandLine, AllowSpew: AllowSpew);
		}

		/// <summary>
		/// Reverts all unchanged file from the specified changelist.
		/// </summary>
		/// <param name="CL">Changelist to revert the unmodified files from.</param>
		public void RevertUnchanged(int CL)
		{
			// caution this is a really bad idea if you hope to force submit!!!
			LogP4("revert -a " + String.Format("-c {0} ", CL));
		}

		/// <summary>
		/// Reverts all files from the specified changelist.
		/// </summary>
		/// <param name="CL">Changelist to revert.</param>
		public void RevertAll(int CL, bool SpewIsVerbose = false)
		{
			LogP4("revert " + String.Format("-c {0} //...", CL), SpewIsVerbose: SpewIsVerbose);
		}

		/// <summary>
		/// Submits the specified changelist.
		/// </summary>
		/// <param name="CL">Changelist to submit.</param>
		/// <param name="SubmittedCL">Will be set to the submitted changelist number.</param>
		/// <param name="Force">If true, the submit will be forced even if resolve is needed.</param>
		/// <param name="RevertIfFail">If true, if the submit fails, revert the CL.</param>
		public void Submit(int CL, out int SubmittedCL, bool Force = false, bool RevertIfFail = false)
		{
			if (!CommandUtils.AllowSubmit)
			{
				throw new P4Exception("Submit is not allowed currently. Please use the -Submit switch to override that.");
			}

			SubmittedCL = 0;
			int Retry = 0;
			string LastCmdOutput = "none?";
			while (Retry++ < 48)
			{
				bool Pending;
				if (!ChangeExists(CL, out Pending))
				{
					throw new P4Exception("Change {0} does not exist.", CL);
				}
				if (!Pending)
				{
					throw new P4Exception("Change {0} was not pending.", CL);
				}
                bool isClPending = false;
                if (ChangeFiles(CL, out isClPending, false).Count == 0)
                {
					CommandUtils.Log("No edits left to commit after brutal submit resolve. Assuming another build committed same changes already and exiting as success.");
                    DeleteChange(CL);
                    // No changes to submit, no need to retry.
                    return;
                }
				string CmdOutput;
				if (!LogP4Output(out CmdOutput, String.Format("submit -c {0}", CL)))
				{
					if (!Force)
					{
						throw new P4Exception("Change {0} failed to submit.\n{1}", CL, CmdOutput);
					}
					CommandUtils.Log("**** P4 Returned\n{0}\n*******", CmdOutput);

					LastCmdOutput = CmdOutput;
					bool DidSomething = false;

                    string[] KnownProblems =
                    {
                        " - must resolve",
                        " - already locked by",
                        " - add of added file",
                        " - edit of deleted file",
                    };

                    bool AnyIssue = false;
                    foreach (var ProblemString in KnownProblems)
                    {
                        int ThisIndex = CmdOutput.IndexOf(ProblemString);
                        if (ThisIndex > 0)
                        {
                            AnyIssue = true;
                            break;
                        }
                    }

                    if (AnyIssue)
                    {
                        string Work = CmdOutput;
                        HashSet<string> AlreadyDone = new HashSet<string>();
                        while (Work.Length > 0)
                        {
                            string SlashSlashStr = "//";
                            int SlashSlash = Work.IndexOf(SlashSlashStr);
                            if (SlashSlash < 0)
                            {
                                break;
                            }
                            Work = Work.Substring(SlashSlash);
                            int MinMatch = Work.Length + 1;
                            foreach (var ProblemString in KnownProblems)
                            {
                                int ThisIndex = Work.IndexOf(ProblemString);
                                if (ThisIndex >= 0 && ThisIndex < MinMatch)
                                {
                                    MinMatch = ThisIndex;
                                }
                            }
                            if (MinMatch > Work.Length)
                            {
                                break;
                            }                            
                            string File = Work.Substring(0, MinMatch).Trim();                            
                            if (File.IndexOf(SlashSlashStr) != File.LastIndexOf(SlashSlashStr))
                            {
                                // this is some other line about the same line, we ignore it, removing the first // so we advance
                                Work = Work.Substring(SlashSlashStr.Length);
                            }
                            else
                            {
                                Work = Work.Substring(MinMatch);
                                if (AlreadyDone.Contains(File))
                                {
                                    continue;
                                }
								CommandUtils.Log("Brutal 'resolve' on {0} to force submit.\n", File);
								Revert(CL, "-k " + CommandUtils.MakePathSafeToUseWithCommandLine(File));  // revert the file without overwriting the local one
								Sync("-f -k " + CommandUtils.MakePathSafeToUseWithCommandLine(File + "#head"), false); // sync the file without overwriting local one
								ReconcileNoDeletes(CL, CommandUtils.MakePathSafeToUseWithCommandLine(File));  // re-check out, if it changed, or add
                                DidSomething = true;
                                AlreadyDone.Add(File);															
                            }
                        }
                    }
					if (!DidSomething)
					{
						CommandUtils.Log("Change {0} failed to submit for reasons we do not recognize.\n{1}\nWaiting and retrying.", CL, CmdOutput);
					}
					System.Threading.Thread.Sleep(30000);
				}
				else
				{
					LastCmdOutput = CmdOutput;
					if (CmdOutput.Trim().EndsWith("submitted."))
					{
						if (CmdOutput.Trim().EndsWith(" and submitted."))
						{
							string EndStr = " and submitted.";
							string ChangeStr = "renamed change ";
							int Offset = CmdOutput.LastIndexOf(ChangeStr);
							int EndOffset = CmdOutput.LastIndexOf(EndStr);
							if (Offset >= 0 && Offset < EndOffset)
							{
								SubmittedCL = int.Parse(CmdOutput.Substring(Offset + ChangeStr.Length, EndOffset - Offset - ChangeStr.Length));
							}
						}
						else
						{
							string EndStr = " submitted.";
							string ChangeStr = "Change ";
							int Offset = CmdOutput.LastIndexOf(ChangeStr);
							int EndOffset = CmdOutput.LastIndexOf(EndStr);
							if (Offset >= 0 && Offset < EndOffset)
							{
								SubmittedCL = int.Parse(CmdOutput.Substring(Offset + ChangeStr.Length, EndOffset - Offset - ChangeStr.Length));
							}
						}

						CommandUtils.Log("Submitted CL {0} which became CL {1}\n", CL, SubmittedCL);
					}

					if (SubmittedCL < CL)
					{
						throw new P4Exception("Change {0} submission seemed to succeed, but did not look like it.\n{1}", CL, CmdOutput);
					}

					// Change submitted OK!  No need to retry.
					return;
				}
			}
			if (RevertIfFail)
			{
				CommandUtils.LogError("Submit CL {0} failed, reverting files\n", CL);
				RevertAll(CL);
				CommandUtils.LogError("Submit CL {0} failed, reverting files\n", CL);
			}
			throw new P4Exception("Change {0} failed to submit after 48 retries??.\n{1}", CL, LastCmdOutput);
		}

		/// <summary>
		/// Creates a new changelist with the specified owner and description.
		/// </summary>
		/// <param name="Owner">Owner of the changelist.</param>
		/// <param name="Description">Description of the changelist.</param>
		/// <returns>Id of the created changelist.</returns>
		public int CreateChange(string Owner = null, string Description = null, string User = null, string Type = null, bool AllowSpew = false)
		{
			var ChangeSpec = "Change: new" + "\n";
			ChangeSpec += "Client: " + ((Owner != null) ? Owner : "") + "\n";
			if(User != null)
			{
				ChangeSpec += "User: " + User + "\n";
			}
			if(Type != null)
			{
				ChangeSpec += "Type: " + Type + "\n";
			}
			ChangeSpec += "Description: " + ((Description != null) ? Description.Replace("\n", "\n\t") : "(none)") + "\n";
			string CmdOutput;
			int CL = 0;
			if(AllowSpew)
			{
				CommandUtils.Log("Creating Change\n {0}\n", ChangeSpec);
			}
			if (LogP4Output(out CmdOutput, "change -i", Input: ChangeSpec, AllowSpew: AllowSpew))
			{
				string EndStr = " created.";
				string ChangeStr = "Change ";
				int Offset = CmdOutput.LastIndexOf(ChangeStr);
				int EndOffset = CmdOutput.LastIndexOf(EndStr);
				if (Offset >= 0 && Offset < EndOffset)
				{
					CL = int.Parse(CmdOutput.Substring(Offset + ChangeStr.Length, EndOffset - Offset - ChangeStr.Length));
				}
			}
			if (CL <= 0)
			{
				throw new P4Exception("Failed to create Changelist. Owner: {0} Desc: {1}", Owner, Description);
			}
			else if(AllowSpew)
			{
				CommandUtils.Log("Returned CL {0}\n", CL);
			}
			return CL;
		}


		/// <summary>
		/// Updates a changelist with the given fields
		/// </summary>
		/// <param name="CL"></param>
		/// <param name="NewOwner"></param>
		/// <param name="NewDescription"></param>
		/// <param name="SpewIsVerbose"></param>
		public void UpdateChange(int CL, string NewOwner, string NewDescription, bool SpewIsVerbose = false)
		{
			string CmdOutput;
			if(!LogP4Output(out CmdOutput, String.Format("change -o {0}", CL), SpewIsVerbose: SpewIsVerbose))
			{
				throw new P4Exception("Couldn't describe changelist {0}", CL);
			}

			P4Spec Spec = P4Spec.FromString(CmdOutput);
			if(NewOwner != null)
			{
				Spec.SetField("Client", NewOwner);
			}
			if(NewDescription != null)
			{
				Spec.SetField("Description", NewDescription);
			}

			if(!LogP4Output(out CmdOutput, "change -i", Input: Spec.ToString(), SpewIsVerbose: SpewIsVerbose))
			{
				throw new P4Exception("Failed to update spec for changelist {0}", CL);
			}
			if(!CmdOutput.TrimEnd().EndsWith(String.Format("Change {0} updated.", CL)))
			{
				throw new P4Exception("Unexpected output from p4 change -i: {0}", CmdOutput);
			}
		}

		/// <summary>
		/// Deletes the specified changelist.
		/// </summary>
		/// <param name="CL">Changelist to delete.</param>
		/// <param name="RevertFiles">Indicates whether files in that changelist should be reverted.</param>
		public void DeleteChange(int CL, bool RevertFiles = true, bool SpewIsVerbose = false, bool AllowSpew = true)
		{
			if (RevertFiles)
			{
				RevertAll(CL, SpewIsVerbose: SpewIsVerbose);
			}

			string CmdOutput;
			if (LogP4Output(out CmdOutput, String.Format("change -d {0}", CL), SpewIsVerbose: SpewIsVerbose, AllowSpew: AllowSpew))
			{
				string EndStr = " deleted.";
				string ChangeStr = "Change ";
				int Offset = CmdOutput.LastIndexOf(ChangeStr);
				int EndOffset = CmdOutput.LastIndexOf(EndStr);
				if (Offset == 0 && Offset < EndOffset)
				{
					return;
				}
			}
			throw new P4Exception("Could not delete change {0} output follows\n{1}", CL, CmdOutput);
		}

		/// <summary>
		/// Tries to delete the specified empty changelist.
		/// </summary>
		/// <param name="CL">Changelist to delete.</param>
		/// <returns>True if the changelist was deleted, false otherwise.</returns>
		public bool TryDeleteEmptyChange(int CL)
		{
			string CmdOutput;
			if (LogP4Output(out CmdOutput, String.Format("change -d {0}", CL)))
			{
				string EndStr = " deleted.";
				string ChangeStr = "Change ";
				int Offset = CmdOutput.LastIndexOf(ChangeStr);
				int EndOffset = CmdOutput.LastIndexOf(EndStr);
				if (Offset == 0 && Offset < EndOffset && !CmdOutput.Contains("can't be deleted."))
				{
					return true;
				}
			}

			return false;
		}

		/// <summary>
		/// Returns the changelist specification.
		/// </summary>
		/// <param name="CL">Changelist to get the specification from.</param>
		/// <returns>Specification of the changelist.</returns>
		public string ChangeOutput(int CL, bool AllowSpew = true)
		{
			string CmdOutput;
			if (LogP4Output(out CmdOutput, String.Format("change -o {0}", CL), AllowSpew: AllowSpew))
			{
				return CmdOutput;
			}
			throw new P4Exception("ChangeOutput failed {0} output follows\n{1}", CL, CmdOutput);
		}

		/// <summary>
		/// Checks whether the specified changelist exists.
		/// </summary>
		/// <param name="CL">Changelist id.</param>
		/// <param name="Pending">Whether it is a pending changelist.</param>
		/// <returns>Returns whether the changelist exists.</returns>
		public bool ChangeExists(int CL, out bool Pending, bool AllowSpew = true)
		{
			string CmdOutput = ChangeOutput(CL, AllowSpew);
			Pending = false;
			if (CmdOutput.Length > 0)
			{
				string EndStr = " unknown.";
				string ChangeStr = "Change ";
				int Offset = CmdOutput.LastIndexOf(ChangeStr);
				int EndOffset = CmdOutput.LastIndexOf(EndStr);
				if (Offset == 0 && Offset < EndOffset)
				{
					CommandUtils.Log("Change {0} does not exist", CL);
					return false;
				}

				string StatusStr = "Status:";
				int StatusOffset = CmdOutput.LastIndexOf(StatusStr);

				if (StatusOffset < 1)
				{
					CommandUtils.LogError("Change {0} could not be parsed\n{1}", CL, CmdOutput);
					return false;
				}

				string Status = CmdOutput.Substring(StatusOffset + StatusStr.Length).TrimStart().Split('\n')[0].TrimEnd();
				CommandUtils.Log("Change {0} exists ({1})", CL, Status);
				Pending = (Status == "pending");
				return true;
			}
			CommandUtils.LogError("Change exists failed {0} no output?", CL, CmdOutput);
			return false;
		}

		/// <summary>
		/// Returns a list of files contained in the specified changelist.
		/// </summary>
		/// <param name="CL">Changelist to get the files from.</param>
		/// <param name="Pending">Whether the changelist is a pending one.</param>
		/// <returns>List of the files contained in the changelist.</returns>
		public List<string> ChangeFiles(int CL, out bool Pending, bool AllowSpew = true)
		{
			var Result = new List<string>();

			if (ChangeExists(CL, out Pending, AllowSpew))
			{
				string CmdOutput = ChangeOutput(CL, AllowSpew);
				if (CmdOutput.Length > 0)
				{

					string FilesStr = "Files:";
					int FilesOffset = CmdOutput.LastIndexOf(FilesStr);
					if (FilesOffset < 0)
					{
						throw new P4Exception("Change {0} returned bad output\n{1}", CL, CmdOutput);
					}
					else
					{
						CmdOutput = CmdOutput.Substring(FilesOffset + FilesStr.Length);
						while (CmdOutput.Length > 0)
						{
							string SlashSlashStr = "//";
							int SlashSlash = CmdOutput.IndexOf(SlashSlashStr);
							if (SlashSlash < 0)
							{
								break;
							}
							CmdOutput = CmdOutput.Substring(SlashSlash);
							string HashStr = "#";
							int Hash = CmdOutput.IndexOf(HashStr);
							if (Hash < 0)
							{
								break;
							}
							string File = CmdOutput.Substring(0, Hash).Trim();
							CmdOutput = CmdOutput.Substring(Hash);

							Result.Add(File);
						}
					}
				}
			}
			else
			{
				throw new P4Exception("Change {0} did not exist.", CL);
			}
			return Result;
		}

        /// <summary>
        /// Returns the output from p4 opened
        /// </summary>
        /// <param name="CL">Changelist to get the specification from.</param>
        /// <returns>Specification of the changelist.</returns>
        public string OpenedOutput()
        {
            string CmdOutput;
            if (LogP4Output(out CmdOutput, "opened"))
            {
                return CmdOutput;
            }
            throw new P4Exception("OpenedOutput failed, output follows\n{0}", CmdOutput);
        }
		/// <summary>
		/// Deletes the specified label.
		/// </summary>
		/// <param name="LabelName">Label to delete.</param>
        public void DeleteLabel(string LabelName, bool AllowSpew = true)
		{
			var CommandLine = "label -d " + LabelName;

			// NOTE: We don't throw exceptions when trying to delete a label
			string Output;
			if (!LogP4Output(out Output, CommandLine, null, AllowSpew))
			{
				CommandUtils.Log("Couldn't delete label '{0}'.  It may not have existed in the first place.", LabelName);
			}
		}

		/// <summary>
		/// Creates a new label.
		/// </summary>
		/// <param name="Name">Name of the label.</param>
		/// <param name="Options">Options for the label. Valid options are "locked", "unlocked", "autoreload" and "noautoreload".</param>
		/// <param name="View">View mapping for the label.</param>
		/// <param name="Owner">Owner of the label.</param>
		/// <param name="Description">Description of the label.</param>
		/// <param name="Date">Date of the label creation.</param>
		/// <param name="Time">Time of the label creation</param>
		public void CreateLabel(string Name, string Options, string View, string Owner = null, string Description = null, string Date = null, string Time = null)
		{
			var LabelSpec = "Label: " + Name + "\n";
			LabelSpec += "Owner: " + ((Owner != null) ? Owner : "") + "\n";
			LabelSpec += "Description: " + ((Description != null) ? Description : "") + "\n";
			if (Date != null)
			{
				LabelSpec += " Date: " + Date + "\n";
			}
			if (Time != null)
			{
				LabelSpec += " Time: " + Time + "\n";
			}
			LabelSpec += "Options: " + Options + "\n";
			LabelSpec += "View: \n";
			LabelSpec += " " + View;

			CommandUtils.Log("Creating Label\n {0}\n", LabelSpec);
			LogP4("label -i", Input: LabelSpec);
		}

		/// <summary>
		/// Invokes p4 tag command.
		/// Associates a named label with a file revision.
		/// </summary>
		/// <param name="LabelName">Name of the label.</param>
		/// <param name="FilePath">Path to the file.</param>
		/// <param name="AllowSpew">Whether the command is allowed to spew.</param>
		public void Tag(string LabelName, string FilePath, bool AllowSpew = true)
		{
			LogP4("tag -l " + LabelName + " " + FilePath, null, AllowSpew);
		}

		/// <summary>
		/// Syncs a label to the current content of the client.
		/// </summary>
		/// <param name="LabelName">Name of the label.</param>
		/// <param name="AllowSpew">Whether the command is allowed to spew.</param>
		public void LabelSync(string LabelName, bool AllowSpew = true, string FileToLabel = "")
		{
			string Quiet = "";
			if (!AllowSpew)
			{
				Quiet = "-q ";
			}
			if (FileToLabel == "")
			{
				LogP4("labelsync " + Quiet + "-l " + LabelName);
			}
			else
			{
				LogP4("labelsync " + Quiet + "-l" + LabelName + " " + FileToLabel);
			}
		}

		/// <summary>
		/// Syncs a label from another label.
		/// </summary>
		/// <param name="FromLabelName">Source label name.</param>
		/// <param name="ToLabelName">Target label name.</param>
		/// <param name="AllowSpew">Whether the command is allowed to spew.</param>
		public void LabelToLabelSync(string FromLabelName, string ToLabelName, bool AllowSpew = true)
		{
			string Quiet = "";
			if (!AllowSpew)
			{
				Quiet = "-q ";
			}
			LogP4("labelsync -a " + Quiet + "-l " + ToLabelName + " //...@" + FromLabelName);
		}

		/// <summary>
		/// Checks whether the specified label exists and has any files.
		/// </summary>
		/// <param name="Name">Name of the label.</param>
		/// <returns>Whether there is an label with files.</returns>
		public bool LabelExistsAndHasFiles(string Name)
		{
			string Output;
			return LogP4Output(out Output, "files -m 1 //...@" + Name);
		}

		/// <summary>
		/// Returns the label description.
		/// </summary>
		/// <param name="Name">Name of the label.</param>
		/// <param name="Description">Description of the label.</param>
		/// <param name="AllowSpew">Whether to allow log spew</param>
		/// <returns>Returns whether the label description could be retrieved.</returns>
		public bool LabelDescription(string Name, out string Description, bool AllowSpew = true)
		{
			string Output;
			Description = "";
			if (LogP4Output(out Output, "label -o " + Name, AllowSpew: AllowSpew))
			{
				string Desc = "Description:";
				int Start = Output.LastIndexOf(Desc);
				if (Start > 0)
				{
					Start += Desc.Length;
				}
				int End = Output.LastIndexOf("Options:");
				if (Start > 0 && End > 0 && End > Start)
				{
					Description = Output.Substring(Start, End - Start).Replace("\n\t", "\n");
					Description = Description.Trim();
					return true;
				}
			}
			return false;
		}

		/// <summary>
		/// Reads a label spec
		/// </summary>
		/// <param name="Name">Label name</param>
		/// <param name="AllowSpew">Whether to allow log spew</param>
		public P4Spec ReadLabelSpec(string Name, bool AllowSpew = true)
		{
			string LabelSpec;
			if(!LogP4Output(out LabelSpec, "label -o " + Name, AllowSpew: AllowSpew))
			{
				throw new P4Exception("Couldn't describe existing label '{0}', output was:\n", Name, LabelSpec);
			}
			return P4Spec.FromString(LabelSpec);
		}

		/// <summary>
		/// Updates a label with a new spec
		/// </summary>
		/// <param name="Spec">Label specification</param>
		/// <param name="AllowSpew">Whether to allow log spew</param>
		public void UpdateLabelSpec(P4Spec Spec, bool AllowSpew = true)
		{
			LogP4("label -i", Input: Spec.ToString(), AllowSpew: AllowSpew);
		}

		/// <summary>
		/// Updates a label description.
		/// </summary>
		/// <param name="Name">Name of the label</param>
		/// <param name="Description">Description of the label.</param>
		/// <param name="AllowSpew">Whether to allow log spew</param>
		public void UpdateLabelDescription(string Name, string NewDescription, bool AllowSpew = true)
		{
			string LabelSpec;
			if(!LogP4Output(out LabelSpec, "label -o " + Name, AllowSpew: AllowSpew))
			{
				throw new P4Exception("Couldn't describe existing label '{0}', output was:\n", Name, LabelSpec);
			}
			List<string> Lines = new List<string>(LabelSpec.Split('\n').Select(x => x.TrimEnd()));

			// Find the description text, and remove it
			int Idx = 0;
			for(; Idx < Lines.Count; Idx++)
			{
				if(Lines[Idx].StartsWith("Description:"))
				{
					int EndIdx = Idx + 1;
					while(EndIdx < Lines.Count && (Lines[EndIdx].Length == 0 || Char.IsWhiteSpace(Lines[EndIdx][0]) || Lines[EndIdx].IndexOf(':') == -1))
					{
						EndIdx++;
					}
					Lines.RemoveRange(Idx, EndIdx - Idx);
					break;
				}
			}

			// Insert the new description text
			Lines.Insert(Idx, "Description:  " + NewDescription.Replace("\n", "\n\t"));
			LabelSpec = String.Join("\n", Lines);

			// Update the label
			LogP4("label -i", Input: LabelSpec, AllowSpew: AllowSpew);
		}

		/* Pattern to parse P4 changes command output. */
		private static readonly Regex ChangesListOutputPattern = new Regex(@"^Change\s+(?<number>\d+)\s+.+$", RegexOptions.Compiled | RegexOptions.Multiline);

		/// <summary>
		/// Gets the latest CL number submitted to the depot. It equals to the @head.
		/// </summary>
		/// <returns>The head CL number.</returns>
		public int GetLatestCLNumber()
		{
			string Output;
			if (!LogP4Output(out Output, "changes -s submitted -m1") || string.IsNullOrWhiteSpace(Output))
			{
				throw new InvalidOperationException("The depot should have at least one submitted changelist. Brand new depot?");
			}

			var Match = ChangesListOutputPattern.Match(Output);

			if (!Match.Success)
			{
				throw new InvalidOperationException("The Perforce output is not in the expected format provided by 2014.1 documentation.");
			}

			return Int32.Parse(Match.Groups["number"].Value);
		}

		/* Pattern to parse P4 labels command output. */
		static readonly Regex LabelsListOutputPattern = new Regex(@"^Label\s+(?<name>[\w\/\.-]+)\s+(?<date>\d{4}/\d{2}/\d{2} \d{2}:\d{2}:\d{2})\s+'(?<description>.+)'\s*$", RegexOptions.Compiled | RegexOptions.Multiline);

		/// <summary>
		/// Gets all labels satisfying given filter.
		/// </summary>
		/// <param name="Filter">Filter for label names.</param>
		/// <param name="bCaseSensitive">Treat filter as case-sensitive.</param>
		/// <returns></returns>
		public P4Label[] GetLabels(string Filter, bool bCaseSensitive = true)
		{
			var LabelList = new List<P4Label>();

			string Output;
			if (P4Output(out Output, "labels -t " + (bCaseSensitive ? "-e" : "-E") + Filter, null, false))
			{
				foreach (Match LabelMatch in LabelsListOutputPattern.Matches(Output))
				{
					LabelList.Add(new P4Label(LabelMatch.Groups["name"].Value,
						DateTime.ParseExact(
							LabelMatch.Groups["date"].Value, "yyyy/MM/dd HH:mm:ss",
							System.Globalization.CultureInfo.InvariantCulture)
					));
				}
			}

			return LabelList.ToArray();
		}

		/// <summary>
		/// Validate label for some content.
		/// </summary>
		/// <returns>True if label exists and has at least one file tagged. False otherwise.</returns>
		public bool ValidateLabelContent(string LabelName)
		{
			string Output;
			if (P4Output(out Output, "files -m 1 @" + LabelName, null, false))
			{
				if (Output.StartsWith("//depot"))
				{
					// If it starts with depot path then label has at least one file tagged in it.
					return true;
				}
			}
			else
			{
				throw new InvalidOperationException("For some reason P4 files failed.");
			}

			return false;
		}

        /// <summary>
        /// Given a file path in the depot, returns the local disk mapping for the current view
        /// </summary>
		/// <param name="DepotFile">The full file path in depot naming form</param>
        /// <returns>The file's first reported path on disk or null if no mapping was found</returns>
        public string DepotToLocalPath(string DepotFile, bool AllowSpew = true)
        {
			//  P4 where outputs missing entries 
			string Command = String.Format("-z tag fstat \"{0}\"", DepotFile);

			// Run the command.
			string[] Lines;
			if (!P4Output(out Lines, Command, AllowSpew: AllowSpew))
			{
				throw new P4Exception("p4.exe {0} failed.", Command);
			}

			// Find the line containing the client file prefix
			const string ClientFilePrefix = "... clientFile ";
			foreach(string Line in Lines)
			{
				if(Line.StartsWith(ClientFilePrefix))
				{
					return Line.Substring(ClientFilePrefix.Length);
				}
			}
			return null;
        }

        /// <summary>
        /// Given a set of file paths in the depot, returns the local disk mapping for the current view
        /// </summary>
		/// <param name="DepotFiles">The full file paths in depot naming form</param>
        /// <returns>The file's first reported path on disk or null if no mapping was found</returns>
        public string[] DepotToLocalPaths(string[] DepotFiles, bool AllowSpew = true)
        {
			const int BatchSize = 20;

			// Parse the output from P4
			List<string> Lines = new List<string>();
			for(int Idx = 0; Idx < DepotFiles.Length; Idx += BatchSize)
			{
				// Build the argument list
				StringBuilder Command = new StringBuilder("-z tag fstat ");
				for(int ArgIdx = Idx; ArgIdx < Idx + BatchSize && ArgIdx < DepotFiles.Length; ArgIdx++)
				{
					Command.AppendFormat(" {0}", CommandUtils.MakePathSafeToUseWithCommandLine(DepotFiles[ArgIdx]));
				}

				// Run the command.
				string[] Output;
				if (!P4Output(out Output, Command.ToString(), AllowSpew: AllowSpew))
				{
					throw new P4Exception("p4.exe {0} failed.", Command);
				}

				// Append it to the combined output
				Lines.AddRange(Output);
			}

			// Parse all the error lines. These may occur out of sequence due to stdout/stderr buffering.
			for(int LineIdx = 0; LineIdx < Lines.Count; LineIdx++)
			{
				if(Lines[LineIdx].Length > 0 && !Lines[LineIdx].StartsWith("... "))
				{
					throw new AutomationException("Unexpected output from p4.exe fstat: {0}", Lines[LineIdx]);
				}
			}

			// Parse the output lines
			string[] LocalFiles = new string[DepotFiles.Length];
			for(int FileIdx = 0, LineIdx = 0; FileIdx < DepotFiles.Length; FileIdx++)
			{
				string DepotFile = DepotFiles[FileIdx];
				if(LineIdx == Lines.Count)
				{
					throw new AutomationException("Unexpected end of output looking for file record for {0}", DepotFile);
				}
				else
				{
					// We've got a file record; try to parse the matching fields
					for(; LineIdx < Lines.Count && Lines[LineIdx].Length > 0; LineIdx++)
					{
						const string DepotFilePrefix = "... depotFile ";
						if(Lines[LineIdx].StartsWith(DepotFilePrefix) && Lines[LineIdx].Substring(DepotFilePrefix.Length) != DepotFile)
						{
							throw new AutomationException("Expected file record for '{0}'; received output '{1}'", DepotFile, Lines[LineIdx]);
						}

						const string ClientFilePrefix = "... clientFile ";
						if(Lines[LineIdx].StartsWith(ClientFilePrefix))
						{
							LocalFiles[FileIdx] = Lines[LineIdx].Substring(ClientFilePrefix.Length);
						}
					}

					// Skip any blank lines
					while(LineIdx < Lines.Count && Lines[LineIdx].Length == 0)
					{
						LineIdx++;
					}
				}
			}
			return LocalFiles;
        }

		/// <summary>
		/// Determines the mappings for a depot file in the workspace, without that file having to exist. 
		/// NOTE: This function originally allowed multiple depot paths at once. The "file(s) not in client view" messages are written to stderr 
		/// rather than stdout, and buffering them separately garbles the output when they're merged together.
		/// </summary>
		/// <param name="DepotFile">Depot path</param>
		/// <param name="AllowSpew">Allows logging</param>
		/// <returns>List of records describing the file's mapping. Usually just one, but may be more.</returns>
		public P4WhereRecord[] Where(string DepotFile, bool AllowSpew = true)
		{
			//  P4 where outputs missing entries 
			string Command = String.Format("-z tag where \"{0}\"", DepotFile);

			// Run the command.
			string Output;
			if (!LogP4Output(out Output, Command, AllowSpew: AllowSpew))
			{
				throw new P4Exception("p4.exe {0} failed.", Command);
			}

			// Copy the results into the local paths lookup. Entries may occur more than once, and entries may be missing from the client view, or deleted in the client view.
			string[] Lines = Output.Split(new char[] { '\r', '\n' }, StringSplitOptions.RemoveEmptyEntries);

			// Check for the file not existing
			if(Lines.Length == 1 && Lines[0].EndsWith(" - file(s) not in client view."))
			{
				return null;
			}

			// Parse it into records
			List<P4WhereRecord> Records = new List<P4WhereRecord>();
			for (int LineIdx = 0; LineIdx < Lines.Length; )
			{
				P4WhereRecord Record = new P4WhereRecord();

				// Parse an optional "... unmap"
				if (Lines[LineIdx].Trim() == "... unmap")
				{
					Record.bUnmap = true;
					LineIdx++;
				}

				// Parse "... depotFile <depot path>"
				const string DepotFilePrefix = "... depotFile ";
				if (LineIdx >= Lines.Length || !Lines[LineIdx].StartsWith(DepotFilePrefix))
				{
					throw new AutomationException("Unexpected output from p4 where: {0}", String.Join("\n", Lines.Skip(LineIdx)));
				}
				Record.DepotFile = Lines[LineIdx++].Substring(DepotFilePrefix.Length).Trim();

				// Parse "... clientFile <client path>"
				const string ClientFilePrefix = "... clientFile ";
				if (LineIdx >= Lines.Length || !Lines[LineIdx].StartsWith(ClientFilePrefix))
				{
					throw new AutomationException("Unexpected output from p4 where: {0}", String.Join("\n", Lines.Skip(LineIdx)));
				}
				Record.ClientFile = Lines[LineIdx++].Substring(ClientFilePrefix.Length).Trim();

				// Parse "... path <path to file>"
				const string PathPrefix = "... path ";
				if (LineIdx >= Lines.Length || !Lines[LineIdx].StartsWith(PathPrefix))
				{
					throw new AutomationException("Unexpected output from p4 where: {0}", String.Join("\n", Lines.Skip(LineIdx)));
				}
				Record.Path = Lines[LineIdx++].Substring(PathPrefix.Length).Trim();

				// Add it to the output list
				Records.Add(Record);
			}
			return Records.ToArray();
		}

		/// <summary>
		/// Determines whether a file exists in the depot.
		/// </summary>
		/// <param name="DepotFile">Depot path</param>
		/// <returns>List of records describing the file's mapping. Usually just one, but may be more.</returns>
		public bool FileExistsInDepot(string DepotFile, bool AllowSpew = true)
		{
			string CommandLine = String.Format("-z tag fstat {0}", CommandUtils.MakePathSafeToUseWithCommandLine(DepotFile));

			string Output;
			if(!LogP4Output(out Output, CommandLine, AllowSpew: false) || Output.Contains("no such file(s)"))
			{
				return false;
			}

			return true;
		}

		/// <summary>
		/// Gets file stats.
		/// </summary>
		/// <param name="Filename">Filenam</param>
		/// <returns>File stats (invalid if the file does not exist in P4)</returns>
		public P4FileStat FStat(string Filename)
		{
			string Output;
			string Command = "fstat " + CommandUtils.MakePathSafeToUseWithCommandLine(Filename);
			if (!LogP4Output(out Output, Command))
			{
				throw new P4Exception("p4.exe {0} failed.", Command);
			}

			P4FileStat Stat = P4FileStat.Invalid;

			if (Output.Contains("no such file(s)") == false)
			{
				Output = Output.Replace("\r", "");
				var FormLines = Output.Split('\n');
				foreach (var Line in FormLines)
				{
					var StatAttribute = Line.StartsWith("... ") ? Line.Substring(4) : Line;
					var StatPair = StatAttribute.Split(' ');
					if (StatPair.Length == 2 && !String.IsNullOrEmpty(StatPair[1]))
					{
						switch (StatPair[0])
						{
							case "type":
								// Use type (current CL if open) if possible
								ParseFileType(StatPair[1], ref Stat);
								break;
							case "headType":
								if (Stat.Type == P4FileType.Unknown)
								{
									ParseFileType(StatPair[1], ref Stat);
								}
								break;
							case "action":
								Stat.Action = ParseAction(StatPair[1]);
								break;
							case "change":
								Stat.Change = StatPair[1];
								break;
						}
					}
				}
				if (Stat.IsValid == false)
				{
					throw new AutomationException("Unable to parse fstat result for {0} (unknown file type).", Filename);
				}
			}
			return Stat;
		}

		/// <summary>
		/// Set file attributes (additively)
		/// </summary>
		/// <param name="Filename">File to change the attributes of.</param>
		/// <param name="Attributes">Attributes to set.</param>
		public void ChangeFileType(string Filename, P4FileAttributes Attributes, string Changelist = null)
		{
			CommandUtils.LogLog("ChangeFileType({0}, {1}, {2})", Filename, Attributes, String.IsNullOrEmpty(Changelist) ? "null" : Changelist);

			var Stat = FStat(Filename);
			if (String.IsNullOrEmpty(Changelist))
			{
				Changelist = (Stat.Action != P4Action.None) ? Stat.Change : "default";
			}
			// Only update attributes if necessary
			if ((Stat.Attributes & Attributes) != Attributes)
			{
				var CmdLine = String.Format("{0} -c {1} -t {2} {3}",
					(Stat.Action != P4Action.None) ? "reopen" : "open",
					Changelist, FileAttributesToString(Attributes | Stat.Attributes), CommandUtils.MakePathSafeToUseWithCommandLine(Filename));
				LogP4(CmdLine);
			}
		}

		/// <summary>
		/// Parses P4 forms and stores them as a key/value pairs.
		/// </summary>
		/// <param name="Output">P4 command output (must be a form).</param>
		/// <returns>Parsed output.</returns>
		public Dictionary<string, string> ParseTaggedP4Output(string Output)
		{
			var Tags = new Dictionary<string, string>(StringComparer.InvariantCultureIgnoreCase);
			var Lines = Output.Split(new string[] { Environment.NewLine }, StringSplitOptions.RemoveEmptyEntries);
            string DelayKey = "";
            int DelayIndex = 0;
			foreach (var Line in Lines)
			{
				var TrimmedLine = Line.Trim();
                if (TrimmedLine.StartsWith("#") == false)
				{
                    if (DelayKey != "")
                    {
                        if (Line.StartsWith("\t"))
                        {
                            if (DelayIndex > 0)
                            {
                                Tags.Add(String.Format("{0}{1}", DelayKey, DelayIndex), TrimmedLine);
                            }
                            else
                            {
                                Tags.Add(DelayKey, TrimmedLine);
                            }
                            DelayIndex++;
                            continue;
                        }
                        DelayKey = "";
                        DelayIndex = 0;
                    }
					var KeyEndIndex = TrimmedLine.IndexOf(':');
					if (KeyEndIndex >= 0)
					{
						var BaseKey = TrimmedLine.Substring(0, KeyEndIndex);

						// Uniquify the key before adding anything to the dictionary. P4 info can sometimes return multiple fields with identical names (eg. 'Broker address', 'Broker version')
						DelayIndex = 0;
						var Key = BaseKey;
						while(Tags.ContainsKey(Key))
						{
							DelayIndex++;
							Key = String.Format("{0}{1}", BaseKey, DelayIndex);
						}

						var Value = TrimmedLine.Substring(KeyEndIndex + 1).Trim();
                        if (Value == "")
                        {
                            DelayKey = BaseKey;
                        }
                        else
                        {
							Tags.Add(Key, Value);
                        }
					}
				}
			}
			return Tags;
		}

		/// <summary>
		/// Checks if the client exists in P4.
		/// </summary>
		/// <param name="ClientName">Client name</param>
		/// <returns>True if the client exists.</returns>
		public bool DoesClientExist(string ClientName, bool Quiet = false)
		{
			if(!Quiet)
			{
				CommandUtils.LogLog("Checking if client {0} exists", ClientName);
			}

            var P4Result = P4(String.Format("-c {0} where //...", ClientName), AllowSpew: false, WithClient: false);
            return P4Result.Output.IndexOf("unknown - use 'client' command", StringComparison.InvariantCultureIgnoreCase) < 0 && P4Result.Output.IndexOf("doesn't exist", StringComparison.InvariantCultureIgnoreCase) < 0;
		}

		/// <summary>
		/// Gets client info.
		/// </summary>
		/// <param name="ClientName">Name of the client.</param>
		/// <returns></returns>
		public P4ClientInfo GetClientInfo(string ClientName, bool Quiet = false)
		{
			if(!Quiet)
			{
				CommandUtils.LogLog("Getting info for client {0}", ClientName);
			}
			if (!DoesClientExist(ClientName, Quiet))
			{
				return null;
			}

			return GetClientInfoInternal(ClientName);
		}
        /// <summary>
        /// Parses a string with enum values separated with spaces.
        /// </summary>
        /// <param name="ValueText"></param>
        /// <param name="EnumType"></param>
        /// <returns></returns>
        private static object ParseEnumValues(string ValueText, Type EnumType)
        {
			ValueText = new Regex("[+ ]").Replace(ValueText, ",");
			return Enum.Parse(EnumType, ValueText, true);
        }

		/// <summary>
		/// Gets client info (does not check if the client exists)
		/// </summary>
		/// <param name="ClientName">Name of the client.</param>
		/// <returns></returns>
		public P4ClientInfo GetClientInfoInternal(string ClientName)
		{
			P4ClientInfo Info = new P4ClientInfo();
            var P4Result = P4(String.Format("client -o {0}", ClientName), AllowSpew: false, WithClient: false);
			if (P4Result.ExitCode == 0)
			{
				var Tags = ParseTaggedP4Output(P4Result.Output);
                Info.Name = ClientName;
				Tags.TryGetValue("Host", out Info.Host);
				Tags.TryGetValue("Root", out Info.RootPath);
				if (!String.IsNullOrEmpty(Info.RootPath))
				{
					Info.RootPath = CommandUtils.ConvertSeparators(PathSeparator.Default, Info.RootPath);
				}
				Tags.TryGetValue("Owner", out Info.Owner);
				Tags.TryGetValue("Stream", out Info.Stream);
				string AccessTime;
				Tags.TryGetValue("Access", out AccessTime);
				if (!String.IsNullOrEmpty(AccessTime))
				{
					DateTime.TryParse(AccessTime, out Info.Access);
				}
				else
				{
					Info.Access = DateTime.MinValue;
				}
                string LineEnd;
                Tags.TryGetValue("LineEnd", out LineEnd);
                if (!String.IsNullOrEmpty(LineEnd))
                {
                    Info.LineEnd = (P4LineEnd)ParseEnumValues(LineEnd, typeof(P4LineEnd));
                }
                string ClientOptions;
                Tags.TryGetValue("Options", out ClientOptions);
                if (!String.IsNullOrEmpty(ClientOptions))
                {
                    Info.Options = (P4ClientOption)ParseEnumValues(ClientOptions, typeof(P4ClientOption));
                }
                string SubmitOptions;
                Tags.TryGetValue("SubmitOptions", out SubmitOptions);
                if (!String.IsNullOrEmpty(SubmitOptions))
                {
                    Info.SubmitOptions = (P4SubmitOption)ParseEnumValues(SubmitOptions, typeof(P4SubmitOption));
                }
                string ClientMappingRoot = "//" + ClientName;
                foreach (var Pair in Tags)
                {
                    if (Pair.Key.StartsWith("View", StringComparison.InvariantCultureIgnoreCase))
                    {
                        string Mapping = Pair.Value;
                        int ClientStartIndex = Mapping.IndexOf(ClientMappingRoot, StringComparison.InvariantCultureIgnoreCase);
                        if (ClientStartIndex > 0)
                        {
                            var ViewPair = new KeyValuePair<string, string>(
                                Mapping.Substring(0, ClientStartIndex - 1),
                                Mapping.Substring(ClientStartIndex + ClientMappingRoot.Length));
                            Info.View.Add(ViewPair);
                        }
                    }
                }
			}
			else
			{
				throw new AutomationException("p4 client -o {0} failed!", ClientName);
			}
			return Info;
		}

		/// <summary>
		/// Gets all clients owned by the user.
		/// </summary>
		/// <param name="UserName"></param>
		/// <returns>List of clients owned by the user.</returns>
		public P4ClientInfo[] GetClientsForUser(string UserName, string PathUnderClientRoot = null)
		{
			var ClientList = new List<P4ClientInfo>();

			// Get all clients for this user
            var P4Result = P4(String.Format("clients -u {0}", UserName), AllowSpew: false, WithClient: false);
			if (P4Result.ExitCode != 0)
			{
				throw new AutomationException("p4 clients -u {0} failed.", UserName);
			}

			// Parse output.
			var Lines = P4Result.Output.Split(new string[] { Environment.NewLine }, StringSplitOptions.RemoveEmptyEntries);
			foreach (string Line in Lines)
			{
				var Tokens = Line.Split(new char[] { ' ' }, StringSplitOptions.RemoveEmptyEntries);
				P4ClientInfo Info = null;

				// Retrieve the client name and info.
				for (int TokenIndex = 0; TokenIndex < Tokens.Length; ++TokenIndex)
				{
					if (Tokens[TokenIndex] == "Client")
					{
						var ClientName = Tokens[++TokenIndex];
						Info = GetClientInfoInternal(ClientName);
						break;
					}
				}

				if (Info == null || String.IsNullOrEmpty(Info.Name) || String.IsNullOrEmpty(Info.RootPath))
				{
					throw new AutomationException("Failed to retrieve p4 client info for user {0}. Unable to set up local environment", UserName);
				}
				
				bool bAddClient = true;
				// Filter the client out if the specified path is not under the client root
				if (!String.IsNullOrEmpty(PathUnderClientRoot) && !String.IsNullOrEmpty(Info.RootPath))
				{
					var ClientRootPathWithSlash = Info.RootPath;
					if (!ClientRootPathWithSlash.EndsWith("\\") && !ClientRootPathWithSlash.EndsWith("/"))
					{
						ClientRootPathWithSlash = CommandUtils.ConvertSeparators(PathSeparator.Default, ClientRootPathWithSlash + "/");
					}
					bAddClient = PathUnderClientRoot.StartsWith(ClientRootPathWithSlash, StringComparison.CurrentCultureIgnoreCase);
				}

				if (bAddClient)
				{
					ClientList.Add(Info);
				}
			}
			return ClientList.ToArray();
		}
        /// <summary>
        /// Deletes a client.
        /// </summary>
        /// <param name="Name">Client name.</param>
        /// <param name="Force">Forces the operation (-f)</param>
        public void DeleteClient(string Name, bool Force = false, bool AllowSpew = true)
        {
            LogP4(String.Format("client -d {0} {1}", (Force ? "-f" : ""), Name), WithClient: false, AllowSpew: AllowSpew);
        }

        /// <summary>
        /// Creates a new client.
        /// </summary>
        /// <param name="ClientSpec">Client specification.</param>
        /// <returns></returns>
        public P4ClientInfo CreateClient(P4ClientInfo ClientSpec, bool AllowSpew = true)
        {
            string SpecInput = "Client: " + ClientSpec.Name + Environment.NewLine;
            SpecInput += "Owner: " + ClientSpec.Owner + Environment.NewLine;
            SpecInput += "Host: " + ClientSpec.Host + Environment.NewLine;
            SpecInput += "Root: " + ClientSpec.RootPath + Environment.NewLine;
            SpecInput += "Options: " + ClientSpec.Options.ToString().ToLowerInvariant().Replace(",", "") + Environment.NewLine;
            SpecInput += "SubmitOptions: " + ClientSpec.SubmitOptions.ToString().ToLowerInvariant().Replace(", ", "+") + Environment.NewLine;
            SpecInput += "LineEnd: " + ClientSpec.LineEnd.ToString().ToLowerInvariant() + Environment.NewLine;
			if(ClientSpec.Stream != null)
			{
				SpecInput += "Stream: " + ClientSpec.Stream + Environment.NewLine;
			}
			else
			{
				SpecInput += "View:" + Environment.NewLine;
				foreach (var Mapping in ClientSpec.View)
				{
					SpecInput += "\t" + Mapping.Key + " //" + ClientSpec.Name + Mapping.Value + Environment.NewLine;
				}
			}
			if (AllowSpew) CommandUtils.LogLog(SpecInput);
            LogP4("client -i", SpecInput, AllowSpew: AllowSpew, WithClient: false);
            return ClientSpec;
        }


		/// <summary>
		/// Lists immediate sub-directories of the specified directory.
		/// </summary>
		/// <param name="CommandLine"></param>
		/// <returns>List of sub-directories of the specified directories.</returns>
		public List<string> Dirs(string CommandLine)
		{
			var DirsCmdLine = String.Format("dirs {0}", CommandLine);
			var P4Result = P4(DirsCmdLine, AllowSpew: false);
			if (P4Result.ExitCode != 0)
			{
				throw new AutomationException("{0} failed.", DirsCmdLine);
			}
			var Result = new List<string>();
			var Lines = P4Result.Output.Split(new string[] { Environment.NewLine }, StringSplitOptions.RemoveEmptyEntries);
			foreach (string Line in Lines)
			{
				if (!Line.Contains("no such file"))
				{
					Result.Add(Line);
				}
			}
			return Result;
		}

		/// <summary>
		/// Lists files of the specified directory non-recursively.
		/// </summary>
		/// <param name="CommandLine"></param>
		/// <returns>List of files in the specified directory.</returns>
		public List<string> Files(string CommandLine)
		{
			List<string> DeleteActions = new List<string> { "delete", "move/delete", "archive", "purge" };
			string FilesCmdLine = String.Format("files {0}", CommandLine);
			IProcessResult P4Result = P4(FilesCmdLine, AllowSpew: false);
			if (P4Result.ExitCode != 0)
			{
				throw new AutomationException("{0} failed.", FilesCmdLine);
			}
			List<string> Result = new List<string>();
			string[] Lines = P4Result.Output.Split(new string[] { Environment.NewLine }, StringSplitOptions.RemoveEmptyEntries);
			Regex OutputSplitter = new Regex(@"(?<filename>.+)#\d+ \- (?<action>[a-zA-Z/]+) .+");
			foreach (string Line in Lines)
			{
				if (!Line.Contains("no such file") && OutputSplitter.IsMatch(Line))
				{
					Match RegexMatch = OutputSplitter.Match(Line);
					string Filename = RegexMatch.Groups["filename"].Value;
					string Action = RegexMatch.Groups["action"].Value;
					if (!DeleteActions.Contains(Action))
					{
						Result.Add(Filename);
					}
				}
			}
			return Result;
		}

		/// <summary>
		/// Gets the contents of a particular file in the depot without syncing it
		/// </summary>
		/// <param name="DepotPath">Depot path to the file (with revision/range if necessary)</param>
		/// <returns>Contents of the file</returns>
		public string Print(string DepotPath, bool AllowSpew = true)
		{
			string Output;
			if(!P4Output(out Output, "print -q " + DepotPath, AllowSpew: AllowSpew, WithClient: false))
			{
				throw new AutomationException("p4 print {0} failed", DepotPath);
			}
			if(!Output.Trim().Contains("\n") && Output.Contains("no such file(s)"))
			{
				throw new AutomationException("p4 print {0} failed", DepotPath);
			}
			return Output;
		}

		/// <summary>
		/// Gets the contents of a particular file in the depot and writes it to a local file without syncing it
		/// </summary>
		/// <param name="DepotPath">Depot path to the file (with revision/range if necessary)</param>
		/// <param name="OutputFileName">Output file to write to</param>
		public void PrintToFile(string DepotPath, string FileName, bool AllowSpew = true)
		{
			string Output;
			if(!P4Output(out Output, "print -q -o \"" + FileName + "\" " + DepotPath, AllowSpew: AllowSpew, WithClient: false))
			{
				throw new AutomationException("p4 print {0} failed", DepotPath);
			}
			if(!Output.Trim().Contains("\n") && Output.Contains("no such file(s)"))
			{
				throw new AutomationException("p4 print {0} failed", DepotPath);
			}
		}

		/// <summary>
		/// Runs the 'interchanges' command on a stream, to determine a list of changelists that have not been integrated to its parent (or vice-versa, if bReverse is set).
		/// </summary>
		/// <param name="StreamName">The name of the stream, eg. //UE4/Dev-Animation</param>
		/// <param name="bReverse">If true, returns changes that have not been merged from the parent stream into this one.</param>
		/// <returns>List of changelist numbers that are pending integration</returns>
		public List<int> StreamInterchanges(string StreamName, bool bReverse)
		{
			string Output;
			if(!P4Output(out Output, String.Format("interchanges {0}-S {1}", bReverse? "-r " : "", StreamName), Input:null, AllowSpew:false))
			{
				throw new AutomationException("Couldn't get unintegrated stream changes from {0}", StreamName);
			}

			List<int> Changelists = new List<int>();
			if(!Output.StartsWith("All revision(s) already integrated"))
			{
				foreach(string Line in Output.Split('\n'))
				{
					string[] Tokens = Line.Split(new char[]{ ' ' }, StringSplitOptions.RemoveEmptyEntries);
					if(Tokens.Length > 0)
					{
						int Changelist;
						if(Tokens[0] != "Change" || !int.TryParse(Tokens[1], out Changelist))
						{
							throw new AutomationException("Unexpected output from p4 interchanges: {0}", Line);
						}
						Changelists.Add(Changelist);
					}
				}
			}
			return Changelists;
		}

		/// <summary>
		/// Execute the 'filelog' command
		/// </summary>
		/// <param name="Options">Options for the command</param>
		/// <param name="FileSpecs">List of file specifications to query</param>
		/// <returns>List of file records</returns>
		public P4FileRecord[] FileLog(P4FileLogOptions Options, params string[] FileSpecs)
		{
			return FileLog(-1, -1, Options, FileSpecs);
		}

		/// <summary>
		/// Execute the 'filelog' command
		/// </summary>
		/// <param name="MaxChanges">Number of changelists to show. Ignored if zero or negative.</param>
		/// <param name="Options">Options for the command</param>
		/// <param name="FileSpecs">List of file specifications to query</param>
		/// <returns>List of file records</returns>
		public P4FileRecord[] FileLog(int MaxChanges, P4FileLogOptions Options, params string[] FileSpecs)
		{
			return FileLog(-1, MaxChanges, Options, FileSpecs);
		}

		/// <summary>
		/// Execute the 'filelog' command
		/// </summary>
		/// <param name="ChangeNumber">Show only files modified by this changelist. Ignored if zero or negative.</param>
		/// <param name="MaxChanges">Number of changelists to show. Ignored if zero or negative.</param>
		/// <param name="Options">Options for the command</param>
		/// <param name="FileSpecs">List of file specifications to query</param>
		/// <returns>List of file records</returns>
		public P4FileRecord[] FileLog(int ChangeNumber, int MaxChanges, P4FileLogOptions Options, params string[] FileSpecs)
		{
			P4FileRecord[] Records;
			P4ReturnCode ReturnCode = TryFileLog(ChangeNumber, MaxChanges, Options, FileSpecs, out Records);
			if(ReturnCode != null)
			{
				if(ReturnCode.Generic == P4GenericCode.Empty)
				{
					return new P4FileRecord[0];
				}
				else
				{
					throw new P4Exception(ReturnCode.ToString());
				}
			}
			return Records;
		}

		/// <summary>
		/// Execute the 'filelog' command
		/// </summary>
		/// <param name="ChangeNumber">Show only files modified by this changelist. Ignored if zero or negative.</param>
		/// <param name="MaxChanges">Number of changelists to show. Ignored if zero or negative.</param>
		/// <param name="Options">Options for the command</param>
		/// <param name="FileSpecs">List of file specifications to query</param>
		/// <returns>List of file records</returns>
		public P4ReturnCode TryFileLog(int ChangeNumber, int MaxChanges, P4FileLogOptions Options, string[] FileSpecs, out P4FileRecord[] OutRecords)
		{
			// Build the argument list
			List<string> Arguments = new List<string>();
			if(ChangeNumber > 0)
			{
				Arguments.Add(String.Format("-c {0}", ChangeNumber));
			}
			if((Options & P4FileLogOptions.ContentHistory) != 0)
			{
				Arguments.Add("-h");
			}
			if((Options & P4FileLogOptions.FollowAcrossBranches) != 0)
			{
				Arguments.Add("-i");
			}
			if((Options & P4FileLogOptions.FullDescriptions) != 0)
			{
				Arguments.Add("-l");
			}
			if((Options & P4FileLogOptions.LongDescriptions) != 0)
			{
				Arguments.Add("-L");
			}
			if(MaxChanges > 0)
			{
				Arguments.Add(String.Format("-m {0}", MaxChanges));
			}
			if((Options & P4FileLogOptions.DoNotFollowPromotedTaskStreams) != 0)
			{
				Arguments.Add("-p");
			}
			if((Options & P4FileLogOptions.IgnoreNonContributoryIntegrations) != 0)
			{
				Arguments.Add("-s");
			}

			// Always include times to simplify parsing
			Arguments.Add("-t");

			// Add the file arguments
			foreach(string FileSpec in FileSpecs)
			{
				Arguments.Add(CommandUtils.MakePathSafeToUseWithCommandLine(FileSpec));
			}

			// Format the full command line
			string CommandLine = String.Format("filelog {0}", String.Join(" ", Arguments));

			// Get the output
			List<Dictionary<string, string>> RawRecords = P4TaggedOutput(CommandLine);

			// Parse all the output
			List<P4FileRecord> Records = new List<P4FileRecord>();
			foreach(Dictionary<string, string> RawRecord in RawRecords)
			{
				// Make sure the record has the correct return value
				P4ReturnCode OtherReturnCode;
				if(!VerifyReturnCode(RawRecord, "stat", out OtherReturnCode))
				{
					OutRecords = null;
					return OtherReturnCode;
				}

				// Get the depot path for this revision
				string DepotPath = RawRecord["depotFile"];

				// Parse the revisions
				List<P4RevisionRecord> Revisions = new List<P4RevisionRecord>();
				for(;;)
				{
					string RevisionSuffix = String.Format("{0}", Revisions.Count);

					string RevisionNumberText;
					if(!RawRecord.TryGetValue("rev" + RevisionSuffix, out RevisionNumberText))
					{
						break;
					}

					int RevisionNumber = int.Parse(RevisionNumberText);
					int RevisionChangeNumber = int.Parse(RawRecord["change" + RevisionSuffix]);
					P4Action Action = ParseActionText(RawRecord["action" + RevisionSuffix]);
					DateTime DateTime = UnixEpoch + TimeSpan.FromSeconds(long.Parse(RawRecord["time" + RevisionSuffix]));
					string Type = RawRecord["type" + RevisionSuffix];
					string UserName = RawRecord["user" + RevisionSuffix];
					string ClientName = RawRecord["client" + RevisionSuffix];
					int FileSize = RawRecord.ContainsKey("fileSize" + RevisionSuffix)? int.Parse(RawRecord["fileSize" + RevisionSuffix]) : -1;
					string Digest = RawRecord.ContainsKey("digest" + RevisionSuffix)? RawRecord["digest" + RevisionSuffix] : null;
					string Description = RawRecord["desc" + RevisionSuffix];

					// Parse all the following integration info
					List<P4IntegrationRecord> Integrations = new List<P4IntegrationRecord>();
					for(;;)
					{
						string IntegrationSuffix = String.Format("{0},{1}", Revisions.Count, Integrations.Count);

						string HowText;
						if(!RawRecord.TryGetValue("how" + IntegrationSuffix, out HowText))
						{
							break;
						}

						P4IntegrateAction IntegrateAction = ParseIntegrateActionText(HowText);
						string OtherFile = RawRecord["file" + IntegrationSuffix];
						string StartRevisionText = RawRecord["srev" + IntegrationSuffix];
						string EndRevisionText = RawRecord["erev" + IntegrationSuffix];
						int StartRevisionNumber = (StartRevisionText == "#none")? 0 : int.Parse(StartRevisionText.Substring(1));
						int EndRevisionNumber = int.Parse(EndRevisionText.Substring(1));

						Integrations.Add(new P4IntegrationRecord(IntegrateAction, OtherFile, StartRevisionNumber, EndRevisionNumber));
					}

					// Add the revision
					Revisions.Add(new P4RevisionRecord(RevisionNumber, RevisionChangeNumber, Action, Type, DateTime, UserName, ClientName, FileSize, Digest, Description, Integrations.ToArray()));
				}

				// Add the file record
				Records.Add(new P4FileRecord(DepotPath, Revisions.ToArray()));
			}
			OutRecords = Records.ToArray();
			return null;
		}

		static readonly DateTime UnixEpoch = new DateTime(1970, 1, 1, 0, 0, 0, DateTimeKind.Utc);

		/// <summary>
		/// Enumerates all streams in a depot
		/// </summary>
		/// <param name="StreamPath">The path for streams to enumerate (eg. "//UE4/...")</param>
		/// <returns>List of streams matching the given criteria</returns>
		public List<P4StreamRecord> Streams(string StreamPath)
		{
			return Streams(StreamPath, -1, null, false);
		}

		/// <summary>
		/// Enumerates all streams in a depot
		/// </summary>
		/// <param name="StreamPath">The path for streams to enumerate (eg. "//UE4/...")</param>
		/// <param name="MaxResults">Maximum number of results to return</param>
		/// <param name="Filter">Additional filter to be applied to the results</param>
		/// <param name="bUnloaded">Whether to enumerate unloaded workspaces</param>
		/// <returns>List of streams matching the given criteria</returns>
		public List<P4StreamRecord> Streams(string StreamPath, int MaxResults, string Filter, bool bUnloaded)
		{
			// Build the command line
			StringBuilder CommandLine = new StringBuilder("streams");
			if(bUnloaded)
			{
				CommandLine.Append(" -U");
			}
			if(Filter != null)
			{
				CommandLine.AppendFormat("-F \"{0}\"", Filter);
			}
			if(MaxResults > 0)
			{
				CommandLine.AppendFormat("-m {0}", MaxResults);
			}
			CommandLine.AppendFormat(" \"{0}\"", StreamPath);

			// Execute the command
			List<Dictionary<string, string>> RawRecords = P4TaggedOutput(CommandLine.ToString(), false);

			// Parse the output
			List<P4StreamRecord> Records = new List<P4StreamRecord>();
			foreach(Dictionary<string, string> RawRecord in RawRecords)
			{
				// Make sure the record has the correct return value
				P4ReturnCode OtherReturnCode;
				if(!VerifyReturnCode(RawRecord, "stat", out OtherReturnCode))
				{
					throw new P4Exception(OtherReturnCode.ToString());
				}

				// Parse the fields
				string Stream = RawRecord["Stream"];
				DateTime Update = UnixEpoch + TimeSpan.FromSeconds(long.Parse(RawRecord["Update"]));
				DateTime Access = UnixEpoch + TimeSpan.FromSeconds(long.Parse(RawRecord["Access"]));
				string Owner = RawRecord["Owner"];
				string Name = RawRecord["Name"];
				string Parent = RawRecord["Parent"];
				P4StreamType Type = (P4StreamType)Enum.Parse(typeof(P4StreamType), RawRecord["Type"], true);
				string Description = RawRecord["desc"];
				P4StreamOptions Options = ParseStreamOptions(RawRecord["Options"]);
				Nullable<bool> FirmerThanParent = ParseNullableBool(RawRecord["firmerThanParent"]);
				bool ChangeFlowsToParent = bool.Parse(RawRecord["changeFlowsToParent"]);
				bool ChangeFlowsFromParent = bool.Parse(RawRecord["changeFlowsFromParent"]);
				string BaseParent = RawRecord["baseParent"];

				// Add the new stream record
				Records.Add(new P4StreamRecord(Stream, Update, Access, Owner, Name, Parent, Type, Description, Options, FirmerThanParent, ChangeFlowsToParent, ChangeFlowsFromParent, BaseParent));
			}
			return Records;
		}

		/// <summary>
		/// Parse a nullable boolean
		/// </summary>
		/// <param name="Text">Text to parse. May be "true", "false", or "n/a".</param>
		/// <returns>The parsed boolean</returns>
		static Nullable<bool> ParseNullableBool(string Text)
		{
			switch(Text)
			{
				case "true":
					return true;
				case "false":
					return false;
				case "n/a":
					return null;
				default:
					throw new P4Exception("Invalid value for nullable bool: {0}", Text);
			}
		}

		/// <summary>
		/// Parse a list of stream option flags
		/// </summary>
		/// <param name="Text">Text to parse</param>
		/// <returns>Flags for the stream options</returns>
		static P4StreamOptions ParseStreamOptions(string Text)
		{
			P4StreamOptions Options = 0;
			foreach(string Option in Text.Split(' '))
			{
				switch(Option)
				{
					case "locked":
						Options |= P4StreamOptions.Locked;
						break;
					case "ownersubmit":
						Options |= P4StreamOptions.OwnerSubmit;
						break;
					case "toparent":
						Options |= P4StreamOptions.ToParent;
						break;
					case "fromparent":
						Options |= P4StreamOptions.FromParent;
						break;
					case "mergedown":
						Options |= P4StreamOptions.MergeDown;
						break;
					case "unlocked":
					case "allsubmit":
					case "notoparent":
					case "nofromparent":
					case "mergeany":
						break;
					default:
						throw new P4Exception("Unknown stream option '{0}'", Option);
				}
			}
			return Options;
		}

		static Dictionary<string, T> GetEnumLookup<T>()
		{
			Dictionary<string, T> Lookup = new Dictionary<string, T>();
			foreach(T Value in Enum.GetValues(typeof(T)))
			{
				foreach(MemberInfo Member in typeof(T).GetMember(Value.ToString()))
				{
					string Description = Member.GetCustomAttribute<DescriptionAttribute>().Description;
					Lookup.Add(Description, Value);
				}
			}
			return Lookup;
		}

		static Lazy<Dictionary<string, P4Action>> DescriptionToAction = new Lazy<Dictionary<string, P4Action>>(() => GetEnumLookup<P4Action>());

		static P4Action ParseActionText(string ActionText)
		{
			P4Action Action;
			if(!DescriptionToAction.Value.TryGetValue(ActionText, out Action))
			{
				throw new P4Exception("Invalid action '{0}'", Action);
			}
			return Action;
		}

		static Lazy<Dictionary<string, P4IntegrateAction>> DescriptionToIntegrationAction = new Lazy<Dictionary<string, P4IntegrateAction>>(() => GetEnumLookup<P4IntegrateAction>());

		static P4IntegrateAction ParseIntegrateActionText(string ActionText)
		{
			P4IntegrateAction Action;
			if(!DescriptionToIntegrationAction.Value.TryGetValue(ActionText, out Action))
			{
				throw new P4Exception("Invalid integration action '{0}'", Action);
			}
			return Action;
		}

		static DateTime ParseDateTime(string DateTimeText)
		{
			return DateTime.ParseExact(DateTimeText, "yyyy/MM/dd HH:mm:ss", System.Globalization.CultureInfo.InvariantCulture);
		}

		/// <summary>
		/// For a given file (and revision, potentially), returns where it was integrated from. Useful in conjunction with files in a P4DescribeRecord, with action = "integrate".
		/// </summary>
		/// <param name="DepotPath">The file to check. May have a revision specifier at the end (eg. //depot/UE4/foo.cpp#2) </param>
		/// <returns>The file that it was integrated from, without a revision specifier</returns>
		public string GetIntegrationSource(string DepotPath)
		{
			string Output;
			if(P4Output(out Output, "filelog -m 1 \"" + DepotPath + "\"", Input:null, AllowSpew:false))
			{
				foreach(string Line in Output.Split('\n').Select(x => x.Trim()))
				{
					const string MergePrefix = "... ... merge from ";
					if(Line.StartsWith(MergePrefix))
					{
						return Line.Substring(MergePrefix.Length, Line.LastIndexOf('#') - MergePrefix.Length);
					}

					const string CopyPrefix = "... ... copy from ";
					if(Line.StartsWith(CopyPrefix))
					{
						return Line.Substring(CopyPrefix.Length, Line.LastIndexOf('#') - CopyPrefix.Length);
					}

					const string EditPrefix = "... ... edit from ";
					if (Line.StartsWith(EditPrefix))
					{
						return Line.Substring(EditPrefix.Length, Line.LastIndexOf('#') - EditPrefix.Length);
					}
				}
			}
			return null;
		}

		#region Utilities

		private static object[] OldStyleBinaryFlags = new object[]
		{
			P4FileAttributes.Uncompressed,
			P4FileAttributes.Executable,
			P4FileAttributes.Compressed,
			P4FileAttributes.RCS
		};

		private static void ParseFileType(string Filetype, ref P4FileStat Stat)
		{
			var AllFileTypes = GetEnumValuesAndKeywords(typeof(P4FileType));
			var AllAttributes = GetEnumValuesAndKeywords(typeof(P4FileAttributes));

			Stat.Type = P4FileType.Unknown;
			Stat.Attributes = P4FileAttributes.None;

			// Parse file flags
			var OldFileFlags = GetEnumValuesAndKeywords(typeof(P4FileAttributes), OldStyleBinaryFlags);
			foreach (var FileTypeFlag in OldFileFlags)
			{
				if ((!String.IsNullOrEmpty(FileTypeFlag.Value) && Char.ToLowerInvariant(FileTypeFlag.Value[0]) == Char.ToLowerInvariant(Filetype[0]))
					// @todo: This is a nasty hack to get .ipa files to work - RobM plz fix?
					|| (FileTypeFlag.Value == "F" && Filetype == "ubinary"))
				{
					Stat.IsOldType = true;
					Stat.Attributes |= (P4FileAttributes)FileTypeFlag.Key;
					break;
				}
			}
			if (Stat.IsOldType)
			{
				Filetype = Filetype.Substring(1);
			}
			// Parse file type
			var TypeAndAttributes = Filetype.Split('+');
			foreach (var FileType in AllFileTypes)
			{
				if (FileType.Value == TypeAndAttributes[0])
				{
					Stat.Type = (P4FileType)FileType.Key;
					break;
				}
			}
			// Parse attributes
			if (TypeAndAttributes.Length > 1 && !String.IsNullOrEmpty(TypeAndAttributes[1]))
			{
				var FileAttributes = TypeAndAttributes[1];
				for (int AttributeIndex = 0; AttributeIndex < FileAttributes.Length; ++AttributeIndex)
				{
					char Attr = FileAttributes[AttributeIndex];
					foreach (var FileAttribute in AllAttributes)
					{
						if (!String.IsNullOrEmpty(FileAttribute.Value) && FileAttribute.Value[0] == Attr)
						{
							Stat.Attributes |= (P4FileAttributes)FileAttribute.Key;
							break;
						}
					}
				}
			}
		}

		static P4Action ParseAction(string Action)
		{
			P4Action Result = P4Action.Unknown;
			var AllActions = GetEnumValuesAndKeywords(typeof(P4Action));
			foreach (var ActionKeyword in AllActions)
			{
				if (ActionKeyword.Value == Action)
				{
					Result = (P4Action)ActionKeyword.Key;
					break;
				}
			}
			return Result;
		}

		private static KeyValuePair<object, string>[] GetEnumValuesAndKeywords(Type EnumType)
		{
			var Values = Enum.GetValues(EnumType);
			KeyValuePair<object, string>[] ValuesAndKeywords = new KeyValuePair<object, string>[Values.Length];
			int ValueIndex = 0;
			foreach (var Value in Values)
			{
				ValuesAndKeywords[ValueIndex++] = new KeyValuePair<object, string>(Value, GetEnumDescription(EnumType, Value));
			}
			return ValuesAndKeywords;
		}

		private static KeyValuePair<object, string>[] GetEnumValuesAndKeywords(Type EnumType, object[] Values)
		{
			KeyValuePair<object, string>[] ValuesAndKeywords = new KeyValuePair<object, string>[Values.Length];
			int ValueIndex = 0;
			foreach (var Value in Values)
			{
				ValuesAndKeywords[ValueIndex++] = new KeyValuePair<object, string>(Value, GetEnumDescription(EnumType, Value));
			}
			return ValuesAndKeywords;
		}

		private static string GetEnumDescription(Type EnumType, object Value)
		{
			var MemberInfo = EnumType.GetMember(Value.ToString());
			var Atributes = MemberInfo[0].GetCustomAttributes(typeof(DescriptionAttribute), false);
			return ((DescriptionAttribute)Atributes[0]).Description;
		}

		private static string FileAttributesToString(P4FileAttributes Attributes)
		{
			var AllAttributes = GetEnumValuesAndKeywords(typeof(P4FileAttributes));
			string Text = "";
			foreach (var Attr in AllAttributes)
			{
				var AttrValue = (P4FileAttributes)Attr.Key;
				if ((Attributes & AttrValue) == AttrValue)
				{
					Text += Attr.Value;
				}
			}
			if (String.IsNullOrEmpty(Text) == false)
			{
				Text = "+" + Text;
			}
			return Text;
		}

		#endregion
	}
}