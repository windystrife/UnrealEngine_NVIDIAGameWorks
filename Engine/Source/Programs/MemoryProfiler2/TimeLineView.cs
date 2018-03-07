// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Collections.Generic;
using System.Windows.Forms;
using System.Windows.Forms.DataVisualization;
using System.Windows.Forms.DataVisualization.Charting;

namespace MemoryProfiler2
{
	public static class FTimeLineChartView
	{
		/// <summary> Memory allocated calculated by the profiler. </summary>
		static public string AllocatedMemory = "Allocated Memory";

		/// <summary> Reference to the main memory profiler window. </summary>
		private static MainWindow OwnerWindow;

		public static void SetProfilerWindow( MainWindow InMainWindow )
		{
			OwnerWindow = InMainWindow;
		}

		public static void ClearView()
		{
			for( int SerieIndex = 0; SerieIndex < OwnerWindow.TimeLineChart.Series.Count; SerieIndex++ )
			{
				Series ChartSeries = OwnerWindow.TimeLineChart.Series[ SerieIndex ];
				if( ChartSeries != null )
				{
					ChartSeries.Points.Clear();
				}
			}
		}

		private static void AddSeriesV4( Chart TimeLineChart, string SeriesName, FStreamSnapshot Snapshot, ESliceTypesV4 SliceType, bool bVisible )
		{
			Series ChartSeries = TimeLineChart.Series[SeriesName];
			ChartSeries.Enabled = bVisible;

			if( ChartSeries != null )
			{
				ChartSeries.Points.Clear();

				foreach( FMemorySlice Slice in Snapshot.OverallMemorySlice )
				{
					DataPoint Point = new DataPoint();
					Point.YValues[0] = Slice.GetSliceInfoV4( SliceType ) / ( 1024.0 * 1024.0 );

					ChartSeries.Points.Add( Point );
				}
			}
		}

		private static void AddEmptySeries( Chart TimeLineChart, string SeriesName, int NumPoints )
		{
			Series ChartSeries = TimeLineChart.Series[SeriesName];
			ChartSeries.Enabled = false;

			if( ChartSeries != null )
			{
				ChartSeries.Points.Clear();

				for( int Index = 0; Index < NumPoints; Index++ )
				{
					ChartSeries.Points.Add( 0 );
				}
			}
		}

		private static void AddFakeSeries( Chart TimeLineChart, string SeriesName, int NumPoints, int Value )
		{
			Series ChartSeries = TimeLineChart.Series[SeriesName];
			ChartSeries.Enabled = true;

			if( ChartSeries != null )
			{
				ChartSeries.Points.Clear();

				for( int Index = 0; Index < NumPoints; Index++ )
				{
					ChartSeries.Points.Add( Value );
				}
			}
		}

		public static void ParseSnapshot( Chart TimeLineChart, FStreamSnapshot Snapshot )
		{
			// Progress bar.
			OwnerWindow.UpdateStatus( "Updating time line view for " + OwnerWindow.CurrentFilename );

			TimeLineChart.Annotations.Clear();

			AddSeriesV4( TimeLineChart, FTimeLineChartView.AllocatedMemory, Snapshot, ESliceTypesV4.OverallAllocatedMemory, true );
			AddSeriesV4( TimeLineChart, FMemoryAllocationStatsV4.PlatformUsedPhysical, Snapshot, ESliceTypesV4.PlatformUsedPhysical, true );

			AddSeriesV4( TimeLineChart, FMemoryAllocationStatsV4.MemoryProfilingOverhead, Snapshot, ESliceTypesV4.MemoryProfilingOverhead, true );
			AddSeriesV4( TimeLineChart, FMemoryAllocationStatsV4.BinnedWasteCurrent, Snapshot, ESliceTypesV4.BinnedWasteCurrent, true );
			AddSeriesV4( TimeLineChart, FMemoryAllocationStatsV4.BinnedSlackCurrent, Snapshot, ESliceTypesV4.BinnedSlackCurrent, true );
			AddSeriesV4( TimeLineChart, FMemoryAllocationStatsV4.BinnedUsedCurrent, Snapshot, ESliceTypesV4.BinnedUsedCurrent, true );
				
			AddEmptySeries( TimeLineChart, "Image Size", Snapshot.OverallMemorySlice.Count );
			AddEmptySeries( TimeLineChart, "OS Overhead", Snapshot.OverallMemorySlice.Count );

			AddEmptySeries( TimeLineChart, "Virtual Used", Snapshot.OverallMemorySlice.Count );
			AddEmptySeries( TimeLineChart, "Virtual Slack", Snapshot.OverallMemorySlice.Count );
			AddEmptySeries( TimeLineChart, "Virtual Waste", Snapshot.OverallMemorySlice.Count );
			AddEmptySeries( TimeLineChart, "Physical Used", Snapshot.OverallMemorySlice.Count );
			AddEmptySeries( TimeLineChart, "Physical Slack", Snapshot.OverallMemorySlice.Count );
			AddEmptySeries( TimeLineChart, "Physical Waste", Snapshot.OverallMemorySlice.Count );

			AddEmptySeries( TimeLineChart, "Host Used", Snapshot.OverallMemorySlice.Count );
			AddEmptySeries( TimeLineChart, "Host Slack", Snapshot.OverallMemorySlice.Count );
			AddEmptySeries( TimeLineChart, "Host Waste", Snapshot.OverallMemorySlice.Count );
		}

		public static bool AddCustomSnapshot( Chart TimeLineChart, CursorEventArgs Event )
		{
			if( TimeLineChart.Series[FTimeLineChartView.AllocatedMemory].Points.Count == 0 )
			{
				return false;
			}
			else 
			{
				VerticalLineAnnotation A = new VerticalLineAnnotation();
				A.AxisX = Event.Axis;
				A.AnchorX = ( int )Event.NewSelectionStart + 1;
				A.ToolTip = "Snapshot";
				A.IsInfinitive = true;
				TimeLineChart.Annotations.Add( A );
				return true;
			}
		}
	}
}

