// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;

namespace MemoryProfiler2
{
	public partial class ProfilingOptions : Form
	{
		private CheckBox[] CheckBoxes;

		public ProfilingOptions()
		{
			InitializeComponent();

			CheckBoxes = new CheckBox[ 9 ];
			CheckBoxes[ 0 ] = GCStartSnapshotsCheckBox;
			CheckBoxes[ 1 ] = GCEndSnapshotsCheckBox;
			CheckBoxes[ 2 ] = LoadMapStartSnapshotsCheckBox;
			CheckBoxes[ 3 ] = LoadMapMidSnapshotsCheckBox;
			CheckBoxes[ 4 ] = LoadMapEndSnapshotsCheckBox;
			CheckBoxes[ 5 ] = LevelStreamStartSnapshotsCheckBox;
			CheckBoxes[ 6 ] = LevelStreamEndSnapshotsCheckBox;
			CheckBoxes[ 7 ] = GenerateSizeGraphsCheckBox;
			CheckBoxes[ 8 ] = KeepLifecyclesCheckBox;
		}

		public ProfilingOptions( ProfilingOptions Src, bool bShouldLockOptions )
			: this()
		{
			for( int i = 0; i < CheckBoxes.Length; i++ )
			{
				CheckBoxes[ i ].Checked = Src.CheckBoxes[ i ].Checked;
			}

			if( bShouldLockOptions )
			{
				LockOptions();
			}
		}

		public void ClearAllOptions()
		{
			foreach( CheckBox CheckBoxIt in CheckBoxes )
			{
				CheckBoxIt.Checked = false;
			}
		}

		public void LockOptions()
		{
			// disable any options that weren't selected at the start of the analysis
			for( int i = 0; i < CheckBoxes.Length; i++ )
			{
				if( !CheckBoxes[ i ].Checked )
				{
					CheckBoxes[ i ].Enabled = false;
				}
			}
		}

		public bool IsSnapshotTypeEnabled( EProfilingPayloadSubType SubType )
		{
			switch( SubType )
			{
				case EProfilingPayloadSubType.SUBTYPE_SnapshotMarker_GC_Start:
				return GCStartSnapshotsCheckBox.Checked;

				case EProfilingPayloadSubType.SUBTYPE_SnapshotMarker_GC_End:
				return GCEndSnapshotsCheckBox.Checked;

				case EProfilingPayloadSubType.SUBTYPE_SnapshotMarker_LoadMap_Start:
				return LoadMapStartSnapshotsCheckBox.Checked;

				case EProfilingPayloadSubType.SUBTYPE_SnapshotMarker_LoadMap_Mid:
				return LoadMapMidSnapshotsCheckBox.Checked;

				case EProfilingPayloadSubType.SUBTYPE_SnapshotMarker_LoadMap_End:
				return LoadMapEndSnapshotsCheckBox.Checked;

				case EProfilingPayloadSubType.SUBTYPE_SnapshotMarker_LevelStream_Start:
				return LevelStreamStartSnapshotsCheckBox.Checked;

				case EProfilingPayloadSubType.SUBTYPE_SnapshotMarker_LevelStream_End:
				return LevelStreamEndSnapshotsCheckBox.Checked;

				default:
				throw new ArgumentException();
			}
		}

		private void btnOK_Click( object sender, EventArgs e )
		{
			DialogResult = DialogResult.OK;
			Close();
		}

		private void btnCancel_Click( object sender, EventArgs e )
		{
			DialogResult = DialogResult.Cancel;
			Close();
		}

		private void btnSetDefaults_Click( object sender, EventArgs e )
		{

		}
	}
}