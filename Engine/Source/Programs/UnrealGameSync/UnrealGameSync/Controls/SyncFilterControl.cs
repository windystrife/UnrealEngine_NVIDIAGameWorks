using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Data;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Runtime.InteropServices;

namespace UnrealGameSync.Controls
{
	public partial class SyncFilterControl : UserControl
	{
		HashSet<string> StripLines = new HashSet<string>()
		{
			"; Rules are specified one per line, and may use any standard Perforce wildcards:",
			";    ?    Matches one character.",
			";    *    Matches any sequence of characters, except for a directory separator.",
			";    ...  Matches any sequence of characters, including directory separators.",
			"; Patterns may match any file fragment (eg. *.pdb), or may be rooted to the branch (eg. /Engine/Binaries/.../*.pdb).",
			"; To exclude files which match a pattern, prefix the rule with the '-' character (eg. -/Engine/Documentation/...)",
			"; Global rules are applied to the files being synced first, followed by any workspace-specific patterns.",
		};

		public SyncFilterControl()
		{
			InitializeComponent();
		}

		private void SyntaxButton_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
		{
			SyncFilterSyntax Dialog = new SyncFilterSyntax();
			Dialog.ShowDialog(ParentForm);
		}

		public void SetView(string[] View)
		{
			ViewTextBox.Lines = View.Where(x => !StripLines.Contains(x.Trim())).SkipWhile(x => x.Trim().Length == 0 || x.Trim() == ";").ToArray();
			ViewTextBox.Select(ViewTextBox.Text.Length, 0);
		}

		public string[] GetView()
		{
			return ViewTextBox.Lines;
		}
	}
}
