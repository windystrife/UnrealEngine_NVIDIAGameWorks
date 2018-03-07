// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.IO;
using AutomationTool;
using UnrealBuildTool;

namespace EpicGames.Localization
{

public struct ProjectImportExportInfo
{
	/** The destination path for the files containing the gathered data for this project (extracted from its config file - relative to the root working directory for the commandlet) */
	public string DestinationPath;

	/** The name to use for this projects manifest file (extracted from its config file) */
	public string ManifestName;

	/** The name to use for this projects archive file (extracted from its config file) */
	public string ArchiveName;

	/** The name to use for this projects portable object file (extracted from its config file) */
	public string PortableObjectName;

	/** The native culture for this project (extracted from its config file) */
	public string NativeCulture;

	/** The cultures to generate for this project (extracted from its config file) */
	public List<string> CulturesToGenerate;

	/** True if we should use a per-culture directly when importing/exporting */
	public bool bUseCultureDirectory;
};

public struct ProjectStepInfo
{
	public ProjectStepInfo(string InName, string InLocalizationConfigFile)
	{
		Name = InName;
		LocalizationConfigFile = InLocalizationConfigFile;
	}
	
	/** The name of this localization step */
	public string Name;

	/** Absolute path to this steps localization config file */
	public string LocalizationConfigFile;
};

public struct ProjectInfo
{
	/** The name of this project */
	public string ProjectName;

	/** Path to this projects localization step data - ordered so that iterating them runs in the correct order */
	public List<ProjectStepInfo> LocalizationSteps;

	/** Config data used by the PO file import process */
	public ProjectImportExportInfo ImportInfo;

	/** Config data used by the PO file export process */
	public ProjectImportExportInfo ExportInfo;
};

public abstract class LocalizationProvider
{
	public struct LocalizationProviderArgs
	{
		public string RootWorkingDirectory;
		public string RootLocalizationTargetDirectory;
		public string RemoteFilenamePrefix;
		public BuildCommand Command;
		public int PendingChangeList;
	};

	public LocalizationProvider(LocalizationProviderArgs InArgs)
	{
		RootWorkingDirectory = InArgs.RootWorkingDirectory;
		RemoteFilenamePrefix = InArgs.RemoteFilenamePrefix;
		Command = InArgs.Command;
		PendingChangeList = InArgs.PendingChangeList;

		LocalizationBranchName = Command.ParseParamValue("LocalizationBranch");
		bUploadAllCultures = Command.ParseParam("UploadAllCultures");
	}

	public virtual string GetLocalizationProviderId()
	{
		throw new AutomationException("Unimplemented GetLocalizationProviderId.");
	}

	public virtual void DownloadProjectFromLocalizationProvider(string ProjectName, ProjectImportExportInfo ProjectImportInfo)
	{
		throw new AutomationException("Unimplemented DownloadProjectFromLocalizationProvider.");
	}

	public virtual void UploadProjectToLocalizationProvider(string ProjectName, ProjectImportExportInfo ProjectExportInfo)
	{
		throw new AutomationException("Unimplemented UploadProjectToLocalizationProvider.");
	}

	public static LocalizationProvider GetLocalizationProvider(string InLocalizationProviderId, LocalizationProvider.LocalizationProviderArgs InLocalizationProviderArgs)
	{
		if (String.IsNullOrEmpty(InLocalizationProviderId))
		{
			return null;
		}

		if (CachedLocalizationProviderTypes == null)
		{
			// Find all types that derive from LocalizationProvider in any of our DLLs
			CachedLocalizationProviderTypes = new Dictionary<string, Type>();
			var LoadedAssemblies = AppDomain.CurrentDomain.GetAssemblies();
			foreach (var Dll in LoadedAssemblies)
			{
				var AllTypes = Dll.GetTypes();
				foreach (var PotentialLocalizationNodeType in AllTypes)
				{
					if (PotentialLocalizationNodeType != typeof(LocalizationProvider) && !PotentialLocalizationNodeType.IsAbstract && !PotentialLocalizationNodeType.IsInterface && typeof(LocalizationProvider).IsAssignableFrom(PotentialLocalizationNodeType))
					{
						// Types should implement a static StaticGetLocalizationProviderId method
						var Method = PotentialLocalizationNodeType.GetMethod("StaticGetLocalizationProviderId");
						if (Method != null)
						{
							try
							{
								var LocalizationProviderId = Method.Invoke(null, null) as string;
								CachedLocalizationProviderTypes.Add(LocalizationProviderId, PotentialLocalizationNodeType);
							}
							catch
							{
								BuildCommand.LogWarning("Type '{0}' threw when calling its StaticGetLocalizationProviderId method.", PotentialLocalizationNodeType.FullName);
							}
						}
						else
						{
							BuildCommand.LogWarning("Type '{0}' derives from LocalizationProvider but is missing its StaticGetLocalizationProviderId method.", PotentialLocalizationNodeType.FullName);
						}
					}
				}
			}
		}

		Type LocalizationNodeType;
		CachedLocalizationProviderTypes.TryGetValue(InLocalizationProviderId, out LocalizationNodeType);
		if (LocalizationNodeType != null)
		{
			try
			{
				return Activator.CreateInstance(LocalizationNodeType, new object[] { InLocalizationProviderArgs }) as LocalizationProvider;
			}
			catch (Exception e)
			{
				BuildCommand.LogWarning("Unable to create an instance of the type '{0}'. {1}", LocalizationNodeType.FullName, e.ToString());
			}
		}
		else
		{
			BuildCommand.LogWarning("Could not find a localization provider for '{0}'", InLocalizationProviderId);
		}

		return null;
	}

	protected string RootWorkingDirectory;
	protected string LocalizationBranchName;
	protected string RemoteFilenamePrefix;
	protected bool bUploadAllCultures;
	protected BuildCommand Command;
	protected int PendingChangeList;

	private static Dictionary<string, Type> CachedLocalizationProviderTypes;
};

}
