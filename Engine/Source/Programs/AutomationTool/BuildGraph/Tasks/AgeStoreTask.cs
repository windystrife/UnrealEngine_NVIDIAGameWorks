// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using AutomationTool;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Xml;
using Tools.DotNETCommon;
using UnrealBuildTool;

namespace Win.Automation
{
    /// <summary>
    /// Parameters for a task that purges data from a symbol store after a given age
    /// </summary>
    public class AgeStoreTaskParameters
    {
        /// <summary>
        /// The target platform to age symbols for.
        /// </summary>
        [TaskParameter]
        public UnrealTargetPlatform Platform;

        /// <summary>
        /// The symbol server directory.
        /// </summary>
        [TaskParameter]
        public string StoreDir;

        /// <summary>
        /// Number of days worth of symbols to keep.
        /// </summary>
        [TaskParameter]
        public int Days;

        /// <summary>
        /// A substring to match in directory file names before deleting symbols. This allows the "age store" task
        /// to avoid deleting symbols from other builds in the case where multiple builds share the same symbol server.
        /// Specific use of the filter value is determined by the symbol server structure defined by the platform tool chain.
        /// </summary>
        [TaskParameter(Optional = true)]
        public string Filter;
    }

    /// <summary>
    /// Task which strips symbols from a set of files. This task is named after the AGESTORE utility that comes with the Microsoft debugger tools SDK, but is actually a separate implementation. The main
    /// difference is that it uses the last modified time rather than last access time to determine which files to delete.
    /// </summary>
    [TaskElement("AgeStore", typeof(AgeStoreTaskParameters))]
    public class AgeStoreTask : CustomTask
    {
        /// <summary>
        /// Parameters for this task
        /// </summary>
        AgeStoreTaskParameters Parameters;

        /// <summary>
        /// Construct a spawn task
        /// </summary>
        /// <param name="InParameters">Parameters for the task</param>
        public AgeStoreTask(AgeStoreTaskParameters InParameters)
        {
            Parameters = InParameters;
        }

        private static void TryDelete(DirectoryInfo Directory)
        {
            try
            {
                Directory.Delete(true);
                CommandUtils.Log("Removed '{0}'", Directory.FullName);
            }
            catch
            {
                CommandUtils.LogWarning("Couldn't delete '{0}' - skipping", Directory.FullName);
            }
        }

        private void RecurseDirectory(DateTime ExpireTimeUtc, DirectoryInfo CurrentDirectory, string[] DirectoryStructure, int Level, string Filter)
        {
            // Do a file search at the last level.
            if (Level == DirectoryStructure.Length)
            {
                // If all files are out of date, delete the directory...
                if (CurrentDirectory.EnumerateFiles().All(x => x.LastWriteTimeUtc < ExpireTimeUtc))
                    TryDelete(CurrentDirectory);
            }
            else
            {
                string[] Patterns = DirectoryStructure[Level].Split(';');
                foreach (var Pattern in Patterns)
                {
                    string ReplacedPattern = string.Format(Pattern, Filter);

                    foreach (var ChildDirectory in CurrentDirectory.GetDirectories(ReplacedPattern, SearchOption.TopDirectoryOnly))
                    {
                        RecurseDirectory(ExpireTimeUtc, ChildDirectory, DirectoryStructure, Level + 1, Filter);
                    }
                }

                // Delete this directory if it is empty, and it is not the root directory.
                if (Level > 0 && !CurrentDirectory.EnumerateFileSystemInfos().Any())
                    TryDelete(CurrentDirectory);
            }
        }

        /// <summary>
        /// Execute the task.
        /// </summary>
        /// <param name="Job">Information about the current job</param>
        /// <param name="BuildProducts">Set of build products produced by this node.</param>
        /// <param name="TagNameToFileSet">Mapping from tag names to the set of files they include</param>
        public override void Execute(JobContext Job, HashSet<FileReference> BuildProducts, Dictionary<string, HashSet<FileReference>> TagNameToFileSet)
        {
            // Get the list of symbol file name patterns from the platform.
            Platform TargetPlatform = Platform.GetPlatform(Parameters.Platform);
            string[] DirectoryStructure = TargetPlatform.SymbolServerDirectoryStructure;
            if (DirectoryStructure == null)
            {
                throw new AutomationException("Platform does not specify the symbol server structure. Cannot age the symbol server.");
            }

            string Filter = string.IsNullOrWhiteSpace(Parameters.Filter)
                ? string.Empty
                : Parameters.Filter.Trim();

            // Get the time at which to expire files
            DateTime ExpireTimeUtc = DateTime.UtcNow - TimeSpan.FromDays(Parameters.Days);
            CommandUtils.Log("Expiring all files before {0}...", ExpireTimeUtc);
            
            // Scan the store directory and delete old symbol files
            DirectoryReference SymbolServerDirectory = ResolveDirectory(Parameters.StoreDir);
            LockFile.TakeLock(SymbolServerDirectory, TimeSpan.FromMinutes(15), () =>
            {
                RecurseDirectory(ExpireTimeUtc, new DirectoryInfo(SymbolServerDirectory.FullName), DirectoryStructure, 0, Filter);
            });
        }

        /// <summary>
        /// Output this task out to an XML writer.
        /// </summary>
        public override void Write(XmlWriter Writer)
        {
            Write(Writer, Parameters);
        }

        /// <summary>
        /// Find all the tags which are used as inputs to this task
        /// </summary>
        /// <returns>The tag names which are read by this task</returns>
        public override IEnumerable<string> FindConsumedTagNames()
        {
            yield break;
        }

        /// <summary>
        /// Find all the tags which are modified by this task
        /// </summary>
        /// <returns>The tag names which are modified by this task</returns>
        public override IEnumerable<string> FindProducedTagNames()
        {
            yield break;
        }
    }
}
