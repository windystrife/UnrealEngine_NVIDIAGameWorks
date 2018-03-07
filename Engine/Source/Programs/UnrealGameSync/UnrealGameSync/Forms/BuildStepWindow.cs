// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace UnrealGameSync
{
	partial class BuildStepWindow : Form
	{
		BuildStep Step;
		List<string> TargetNames;
		string BaseDirectoryPrefix;
		IReadOnlyDictionary<string, string> Variables;
		VariablesWindow VariablesWindow;

		public BuildStepWindow(BuildStep InTask, List<string> InTargetNames, string InBaseDirectory, IReadOnlyDictionary<string, string> InVariables)
		{
			Step = InTask;
			TargetNames = InTargetNames;
			BaseDirectoryPrefix = Path.GetFullPath(InBaseDirectory) + Path.DirectorySeparatorChar;
			Variables = InVariables;

			InitializeComponent();
		}

		private void BuildTaskWindow_Load(object sender, EventArgs e)
		{
			string DefaultTargetName = TargetNames.FirstOrDefault(x => !x.EndsWith("Editor") && !x.EndsWith("Client") && !x.EndsWith("Server")) ?? ((TargetNames.Count > 0)? TargetNames[0] : "");
			CompileTargetComboBox.Items.AddRange(TargetNames.ToArray());
			CompileTargetComboBox.Text = String.IsNullOrEmpty(Step.Target)? DefaultTargetName : Step.Target;

			DescriptionTextBox.Text = Step.Description;
			StatusTextTextBox.Text = Step.StatusText;
			DurationTextBox.Text = Step.EstimatedDuration.ToString();

			switch(Step.Type)
			{
				case BuildStepType.Compile:
					CompileRadioButton.Checked = true;
					CompilePlatformComboBox.Text = Step.Platform;
					CompileConfigComboBox.Text = Step.Configuration;
					CompileArgumentsTextBox.Text = Step.Arguments;
					break;
				case BuildStepType.Cook:
					CookRadioButton.Checked = true;
					CookFileNameTextBox.Text = Step.FileName;
					break;
				case BuildStepType.Other:
					OtherRadioButton.Checked = true;
					OtherFileNameTextBox.Text = Step.FileName;
					OtherArgumentsTextBox.Text = Step.Arguments;
					OtherUseLogWindowCheckBox.Checked = Step.bUseLogWindow;
					break;
			}

			if(String.IsNullOrEmpty(CompilePlatformComboBox.Text))
			{
				CompilePlatformComboBox.Text = "Win64";
			}

			if(String.IsNullOrEmpty(CompileConfigComboBox.Text))
			{
				CompileConfigComboBox.Text = "Development";
			}

			UpdateType();
		}

		public void UpdateType()
		{
			bool bIsCompile = CompileRadioButton.Checked;
			CompileTargetComboBox.Enabled = bIsCompile;
			CompilePlatformComboBox.Enabled = bIsCompile;
			CompileConfigComboBox.Enabled = bIsCompile;
			CompileArgumentsTextBox.Enabled = bIsCompile;

			bool bIsCook = CookRadioButton.Checked;
			CookFileNameTextBox.Enabled = bIsCook;
			CookFileNameButton.Enabled = bIsCook;

			bool bIsOther = OtherRadioButton.Checked;
			OtherFileNameTextBox.Enabled = bIsOther;
			OtherFileNameButton.Enabled = bIsOther;
			OtherArgumentsTextBox.Enabled = bIsOther;
			OtherUseLogWindowCheckBox.Enabled = bIsOther;
		}

		private void CompileRadioButton_CheckedChanged(object sender, EventArgs e)
		{
			UpdateType();
		}

		private void CookRadioButton_CheckedChanged(object sender, EventArgs e)
		{
			UpdateType();
		}

		private void OtherRadioButton_CheckedChanged(object sender, EventArgs e)
		{
			UpdateType();
		}

		private void OkButton_Click(object sender, EventArgs e)
		{
			Step.Description = DescriptionTextBox.Text;
			Step.StatusText = StatusTextTextBox.Text;

			if(!int.TryParse(DurationTextBox.Text, out Step.EstimatedDuration))
			{
				Step.EstimatedDuration = 1;
			}

			if(CompileRadioButton.Checked)
			{
				Step.Type = BuildStepType.Compile;
				Step.Target = CompileTargetComboBox.Text;
				Step.Platform = CompilePlatformComboBox.Text;
				Step.Configuration = CompileConfigComboBox.Text;
				Step.FileName = null;
				Step.Arguments = CompileArgumentsTextBox.Text;
				Step.bUseLogWindow = true;
			}
			else if(CookRadioButton.Checked)
			{
				Step.Type = BuildStepType.Cook;
				Step.Target = null;
				Step.Platform = null;
				Step.Configuration = null;
				Step.FileName = CookFileNameTextBox.Text;
				Step.Arguments = null;
				Step.bUseLogWindow = true;
			}
			else
			{
				Step.Type = BuildStepType.Other;
				Step.Target = null;
				Step.Platform = null;
				Step.Configuration = null;
				Step.FileName = OtherFileNameTextBox.Text;
				Step.Arguments = OtherArgumentsTextBox.Text;
				Step.bUseLogWindow = OtherUseLogWindowCheckBox.Checked;
			}

			DialogResult = DialogResult.OK;
			Close();
		}

		private void NewCancelButton_Click(object sender, EventArgs e)
		{
			DialogResult = DialogResult.Cancel;
			Close();
		}

		private void CookFileNameButton_Click(object sender, EventArgs e)
		{
			OpenFileDialog Dialog = new OpenFileDialog();
			Dialog.Filter = "Cook/Launch Profiles (*.ulp2)|*.ulp2";
			Dialog.FileName = AddBaseDirectory(CookFileNameTextBox.Text);
			if(Dialog.ShowDialog() == DialogResult.OK)
			{
				CookFileNameTextBox.Text = RemoveBaseDirectory(Dialog.FileName);
			}
		}

		private void OtherFileNameButton_Click(object sender, EventArgs e)
		{
			OpenFileDialog Dialog = new OpenFileDialog();
			Dialog.Filter = "Executable Files (*.exe,*.bat)|*.exe;*.bat|All files (*.*)|*.*";
			Dialog.FileName = AddBaseDirectory(OtherFileNameTextBox.Text);
			if(Dialog.ShowDialog() == DialogResult.OK)
			{
				OtherFileNameTextBox.Text = RemoveBaseDirectory(Dialog.FileName);
			}
		}

		private string AddBaseDirectory(string FileName)
		{
			if(FileName.Contains("$("))
			{
				return "";
			}
			else if(Path.IsPathRooted(FileName))
			{
				return FileName;
			}
			else
			{
				return Path.Combine(BaseDirectoryPrefix, FileName);
			}
		}

		private string RemoveBaseDirectory(string FileName)
		{
			string FullFileName = Path.GetFullPath(FileName);
			if(FullFileName.StartsWith(BaseDirectoryPrefix, StringComparison.InvariantCultureIgnoreCase))
			{
				return FullFileName.Substring(BaseDirectoryPrefix.Length);
			}
			else
			{
				return FullFileName;
			}
		}

		private void OnClosedVariablesWindow(object sender, EventArgs e)
		{
			VariablesWindow = null;
		}

		private void InsertVariable(string Name)
		{
			IContainerControl Container = this;
			if(Container != null)
			{
				for(;;)
				{
					IContainerControl NextContainer = Container.ActiveControl as IContainerControl;
					if(NextContainer == null)
					{
						break;
					}
					Container = NextContainer;
				}

				TextBox FocusTextBox = Container.ActiveControl as TextBox;
				if(FocusTextBox != null)
				{
					FocusTextBox.SelectedText = Name;
				}
			}
		}

		private void VariablesButton_Click(object sender, EventArgs e)
		{
			if(VariablesWindow == null)
			{
				VariablesWindow = new VariablesWindow(Variables);
				VariablesWindow.OnInsertVariable += InsertVariable;
				VariablesWindow.Location = new Point(Bounds.Right + 20, Bounds.Top);
				VariablesWindow.Size = new Size(VariablesWindow.Size.Width, Size.Height);
				VariablesWindow.FormClosed += OnClosedVariablesWindow;
				VariablesWindow.Show(this);
			}
			else
			{
				VariablesWindow.Close();
				VariablesWindow = null;
			}
		}
	}
}
