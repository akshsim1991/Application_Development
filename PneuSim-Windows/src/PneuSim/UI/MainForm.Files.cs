using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Imaging;
using System.Drawing.Text;
using System.IO;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text;
using System.Windows.Forms;
using Microsoft.VisualBasic;
using Microsoft.VisualBasic.CompilerServices;

namespace PneuSim;

public partial class MainForm
{

	private void ActivateProject(Project p, string path)
	{
		_project = p;
		_pageIndex = 0;
		_filePath = path;
		HideStartScreen();
		if (path != null)
		{
			AppSettings.AddRecent(path);
		}
		_dirty = false;
		_history.Clear();
		_history.Add((p.ToXml(), 0));
		_historyIndex = 0;
		RefreshPageTabs();
		ShowPage(0);
		UpdateTitle();
		UpdateUndoButtons();
		ScheduleCheck();
		CheckForHiddenFault();
	}

	private bool ConfirmDiscard()
	{
		if (!_dirty)
		{
			return true;
		}
		return MessageBox.Show("Save changes to the current project?", "PneuSim", MessageBoxButtons.YesNoCancel, MessageBoxIcon.Question) switch
		{
			DialogResult.Yes => SaveProject(_filePath), 
			DialogResult.No => true, 
			_ => false, 
		};
	}

	private void OnNew(object sender, EventArgs e)
	{
		NewProject(new Project(new Circuit()));
	}

	private void OnOpen(object sender, EventArgs e)
	{
		using OpenFileDialog openFileDialog = new OpenFileDialog
		{
			Filter = "PneuSim projects (*.pneu)|*.pneu|All files (*.*)|*.*"
		};
		if (openFileDialog.ShowDialog(this) != DialogResult.OK)
		{
			return;
		}
		try
		{
			NewProject(Project.Load(openFileDialog.FileName), openFileDialog.FileName);
		}
		catch (Exception ex)
		{
			ProjectData.SetProjectError(ex);
			Exception ex2 = ex;
			MessageBox.Show(string.Format("Could not open the file:{0}{1}", "\r\n", ex2.Message), "PneuSim", MessageBoxButtons.OK, MessageBoxIcon.Hand);
			ProjectData.ClearProjectError();
		}
	}

	public void OpenFile(string path)
	{
		try
		{
			NewProject(Project.Load(path), path);
		}
		catch (Exception ex)
		{
			ProjectData.SetProjectError(ex);
			Exception ex2 = ex;
			MessageBox.Show(string.Format("Could not open the file:{0}{1}", "\r\n", ex2.Message), "PneuSim", MessageBoxButtons.OK, MessageBoxIcon.Hand);
			ProjectData.ClearProjectError();
		}
	}

	private void OnSave(object sender, EventArgs e)
	{
		SaveProject(_filePath);
	}

	private void OnSaveAs(object sender, EventArgs e)
	{
		SaveProject(null);
	}

	private bool SaveProject(string path)
	{
		bool result;
		if (path == null)
		{
			using SaveFileDialog saveFileDialog = new SaveFileDialog
			{
				Filter = "PneuSim projects (*.pneu)|*.pneu|All files (*.*)|*.*",
				DefaultExt = "pneu",
				FileName = SafeName(_project.Info.Title) + ".pneu"
			};
			if (saveFileDialog.ShowDialog(this) != DialogResult.OK)
			{
				result = false;
				goto IL_00d7;
			}
			path = saveFileDialog.FileName;
		}
		try
		{
			_project.Save(path);
			_filePath = path;
			AppSettings.AddRecent(path);
			_dirty = false;
			UpdateTitle();
			_statusMessage.Text = $"Saved {path}";
			result = true;
		}
		catch (Exception ex)
		{
			ProjectData.SetProjectError(ex);
			Exception ex2 = ex;
			MessageBox.Show(string.Format("Could not save the file:{0}{1}", "\r\n", ex2.Message), "PneuSim", MessageBoxButtons.OK, MessageBoxIcon.Hand);
			result = false;
			ProjectData.ClearProjectError();
		}
		goto IL_00d7;
		IL_00d7:
		return result;
	}

	private void OnProjectInfo(object sender, EventArgs e)
	{
		string right = _project.ToXml();
		Form form = new Form
		{
			Text = "Project information",
			Size = new Size(520, 480),
			StartPosition = FormStartPosition.CenterParent,
			ShowInTaskbar = false,
			Font = Font,
			KeyPreview = true
		};
		try
		{
			PropertyGrid value = new PropertyGrid
			{
				Dock = DockStyle.Fill,
				SelectedObject = _project.Info,
				ToolbarVisible = false
			};
			form.Controls.Add(value);
			form.KeyDown += [SpecialName] (object s2, KeyEventArgs k) =>
			{
				if (k.KeyCode == Keys.Escape)
				{
					form.Close();
				}
			};
			Theme.Apply(form);
			form.ShowDialog(this);
		}
		finally
		{
			if (form != null)
			{
				((IDisposable)form).Dispose();
			}
		}
		if (Operators.CompareString(_project.ToXml(), right, TextCompare: false) != 0)
		{
			OnCircuitModified();
		}
	}

	private void OnExportImage(object sender, EventArgs e)
	{
		if (CurrentCircuit.Elements.Count == 0)
		{
			return;
		}
		string text = AskPath("PNG image (*.png)|*.png", "png", _project.Pages[_pageIndex].Name);
		if (text != null)
		{
			using (Bitmap bitmap = RenderCircuitImage(CurrentCircuit, _running))
			{
				bitmap.Save(text, ImageFormat.Png);
			}
			_statusMessage.Text = $"Exported {text}";
		}
	}

	private void OnExportSvg(object sender, EventArgs e)
	{
		if (CurrentCircuit.Elements.Count != 0)
		{
			string text = AskPath("SVG drawing (*.svg)|*.svg", "svg", _project.Pages[_pageIndex].Name);
			if (text != null)
			{
				VectorExport.SaveSvg(CurrentCircuit, text);
				_statusMessage.Text = $"Exported {text}";
			}
		}
	}

	private void OnExportDxf(object sender, EventArgs e)
	{
		if (CurrentCircuit.Elements.Count != 0)
		{
			string text = AskPath("AutoCAD DXF (*.dxf)|*.dxf", "dxf", _project.Pages[_pageIndex].Name);
			if (text != null)
			{
				VectorExport.SaveDxf(CurrentCircuit, text);
				_statusMessage.Text = $"Exported {text} (units: millimetres)";
			}
		}
	}

	private void OnExportPdf(object sender, EventArgs e)
	{
		string text = "";
		try
		{
			text = CircuitAnalysis.Explain(_project.SimulationCircuit(), null, _realPhysics);
		}
		catch (Exception ex)
		{
			ProjectData.SetProjectError(ex);
			text = "";
			ProjectData.ClearProjectError();
		}
		PdfDocument doc;
		try
		{
			doc = Reports.BuildPdf(_project, includeParts: true, text);
		}
		catch (Exception ex2)
		{
			ProjectData.SetProjectError(ex2);
			MessageBox.Show("Could not prepare the report: " + ex2.Message, "PneuSim", MessageBoxButtons.OK, MessageBoxIcon.Hand);
			ProjectData.ClearProjectError();
			return;
		}
		using PrintPreviewForm preview = new PrintPreviewForm(doc, _project.Info.Title);
		preview.SaveRequested += delegate
		{
			string text2 = AskPath("PDF document (*.pdf)|*.pdf", "pdf", _project.Info.Title);
			if (text2 == null)
			{
				return;
			}
			try
			{
				doc.Save(text2);
				_statusMessage.Text = $"Exported {text2}";
				try
				{
					Process.Start(new ProcessStartInfo(text2)
					{
						UseShellExecute = true
					});
				}
				catch (Exception)
				{
				}
			}
			catch (Exception ex3)
			{
				MessageBox.Show("Could not write the PDF: " + ex3.Message, "PneuSim", MessageBoxButtons.OK, MessageBoxIcon.Hand);
			}
		};
		Theme.Apply(preview);
		preview.ShowDialog(this);
	}

	private void OpenExample(Func<Circuit> build)
	{
		NewProject(new Project(build()));
		_statusMessage.Text = "Example loaded. Press Start (F9), then click the push buttons and switches.";
	}

	private void LoadLessonCircuit(Circuit c)
	{
		NewProject(new Project(c, "Lesson"));
		FitView();
		_statusMessage.Text = "Tutorial parts loaded. Connect them, then press 'Check my solution'.";
	}

	private void StartAutosave()
	{
		_autosaveTimer.Tick += [SpecialName] (object a0, EventArgs a1) =>
		{
			Autosave();
		};
		_autosaveTimer.Start();
	}

	private void Autosave()
	{
		if (_dirty)
		{
			try
			{
				Directory.CreateDirectory(Path.GetDirectoryName(AutosavePath));
				_project.Save(AutosavePath);
				File.WriteAllText(AutosavePath + ".source", _filePath ?? "");
			}
			catch (Exception ex)
			{
				ProjectData.SetProjectError(ex);
				Exception ex2 = ex;
				ProjectData.ClearProjectError();
			}
		}
	}

	private void DeleteAutosave()
	{
		try
		{
			if (File.Exists(AutosavePath))
			{
				File.Delete(AutosavePath);
			}
			if (File.Exists(AutosavePath + ".source"))
			{
				File.Delete(AutosavePath + ".source");
			}
		}
		catch (Exception ex)
		{
			ProjectData.SetProjectError(ex);
			Exception ex2 = ex;
			ProjectData.ClearProjectError();
		}
	}

	private void SaveExerciseReport()
	{
		if (_exercise == null)
		{
			return;
		}
		using SaveFileDialog saveFileDialog = new SaveFileDialog
		{
			Filter = "Text file (*.txt)|*.txt",
			FileName = SafeName(_project.Info.Title) + " troubleshooting.txt"
		};
		if (saveFileDialog.ShowDialog(this) == DialogResult.OK)
		{
			File.WriteAllText(saveFileDialog.FileName, _exercise.Report(_project.Info.Title), Encoding.UTF8);
			_statusMessage.Text = "Saved " + saveFileDialog.FileName;
		}
	}
}
