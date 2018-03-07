// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using AutomationTool;
using UnrealBuildTool;

namespace EpicGames.OneSkyLocalization.Config
{
	/// <summary>
	/// Class for retrieving OneSky localization configuration data
	/// </summary>
	public class OneSkyConfigHelper
	{
		// List of configs is cached off for fetching from multiple times
		private static Dictionary<string, OneSkyConfigData> Configs;

		public static OneSkyConfigData Find(string ConfigName)
		{
			if (Configs == null)
			{
				// Load all secret configs by trying to instantiate all classes derived from OneSkyConfigData from all loaded DLLs.
				// Note that we're using the default constructor on the secret config types.
				Configs = new Dictionary<string, OneSkyConfigData>();
				Assembly[] LoadedAssemblies = AppDomain.CurrentDomain.GetAssemblies();
				foreach (var Dll in LoadedAssemblies)
				{
					Type[] AllTypes = Dll.GetTypes();
					foreach (var PotentialConfigType in AllTypes)
					{
						if (PotentialConfigType != typeof(OneSkyConfigData) && typeof(OneSkyConfigData).IsAssignableFrom(PotentialConfigType))
						{
							try
							{
								OneSkyConfigData Config = Activator.CreateInstance(PotentialConfigType) as OneSkyConfigData;
								if (Config != null)
								{
									Configs.Add(Config.Name, Config);
								}
							}
							catch
							{
								BuildCommand.LogWarning("Unable to create OneSky config data: {0}", PotentialConfigType.Name);
							}
						}
					}
				}
			}
			OneSkyConfigData LoadedConfig;
			Configs.TryGetValue(ConfigName, out LoadedConfig);
			if (LoadedConfig == null)
			{
				throw new AutomationException("Unable to find requested OneSky config data: {0}", ConfigName);
			}
			return LoadedConfig;
		}
	}

	/// <summary>
	/// Class for storing OneSky Localization configuration data
	/// </summary>
	public class OneSkyConfigData
	{
		public OneSkyConfigData(string InName, string InApiKey, string InApiSecret)
		{
			Name = InName;
			ApiKey = InApiKey;
			ApiSecret = InApiSecret;
		}

		public string Name;
		public string ApiKey;
		public string ApiSecret;

		public void SpewValues()
		{
			CommandUtils.Log("Name : {0}", Name);
			CommandUtils.Log("ApiKey : {0}", ApiKey);
			//CommandUtils.LogConsole("ApiSecret : {0}", ApiSecret);  // This should probably never be revealed.
		}
	}
}
