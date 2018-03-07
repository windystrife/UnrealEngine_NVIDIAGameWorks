// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using AutomationTool;
using UnrealBuildTool;
using System.Xml;

[Help("Submits a generated Utilization report to EC")]
class SubmitUtilizationReportToEC : BuildCommand
{
	public override void ExecuteBuild()
	{
		string DayParam = ParseParamValue("Day");
		DateTime Day;
		string FileName = ParseParamValue("FileName");

		// delete anything older than two weeks
		DateTime TwoWeeksAgo = DateTime.UtcNow.Subtract(new TimeSpan(14, 0, 0, 0));

		// The day we're passing in is running at midnight+ for the daily, so we actually want to label this as yesterday's report as far as the graph
		// since the utilization capture is actually getting from 12am the previous day to 12am today.
		// All builds run in that timeframe will be technically yesterday's builds.
		if (string.IsNullOrEmpty(DayParam))
		{
			Day = DateTime.Today.Add(new TimeSpan(1,0,0,0));
			Log("Day parameter not specified, defaulting to today's report: " + Day.Date.ToString("MM-dd-yyyy"));
		}
		else
		{
			if (!DateTime.TryParse(DayParam, out Day))
			{
				throw new AutomationException("Day was passed in an incorrect format. Format <mm/dd/yy>");
			}
			if(Day < TwoWeeksAgo)
			{
				throw new AutomationException("The day passed is more than two weeks in the past. The report would be deleted tomorrow. Not running.");
			}
		}
		if(string.IsNullOrEmpty(FileName))
		{
			throw new AutomationException("FileName not Specified!");
		}

		if (!File.Exists(FileName))
		{
			throw new AutomationException("Could not find file at path: " + FileName);
		}



		CommandUtils.ERunOptions Opts = CommandUtils.ERunOptions.Default | CommandUtils.ERunOptions.SpewIsVerbose;
		XmlDocument Reader = new XmlDocument();
		// get all of the existing properties
		Reader.LoadXml(CommandUtils.RunAndLog("ectool", "--timeout 900 getProperties --path \"/projects/GUBP_V5/Generated/Utilization\"", Options: Opts));
		// grab just the prop names
		XmlNodeList ExistingProps = Reader.GetElementsByTagName("propertyName");
		foreach(XmlNode prop in ExistingProps)
		{
			string date = prop.InnerText.Replace("Report_", string.Empty);
			DateTime ExistingDate;
			if (!DateTime.TryParse(date, out ExistingDate))
			{
				LogWarning("Found property in Utilization properties that had a malformed name: " + prop.InnerText + ", remove this manually ASAP.");
				continue;
			}
			// delete anything older than two weeks
			if(ExistingDate < TwoWeeksAgo)
			{
				Log("Deleting out-of-date property: " + prop.InnerText);
				CommandUtils.RunAndLog("ectool", string.Format("--timeout 900 deleteProperty --propertyName \"/projects/GUBP_V5/Generated/Utilization/{0}\"", prop.InnerText), Options: Opts);
			}

		}
		// add today's
		Log("Adding report: " + Day.Subtract(new TimeSpan(1,0,0,0)).ToString("yyyy-MM-dd") + "->" + Day.ToString("yyyy-MM-dd"));
		CommandUtils.RunAndLog("ectool", string.Format("--timeout 900 setProperty \"/projects/GUBP_V5/Generated/Utilization/Report_{0}\" --valueFile {1}", Day.Subtract(new TimeSpan(1,0,0,0)).ToString("yyyy-MM-dd"), FileName), Options: Opts);
	}
}
