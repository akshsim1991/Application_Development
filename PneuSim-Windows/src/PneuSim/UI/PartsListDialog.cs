using System;
using System.Collections.Generic;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text;
using System.Windows.Forms;
using Microsoft.VisualBasic.CompilerServices;

namespace PneuSim;

public class PartsListDialog : Form
{
	private readonly Project _project;

	private readonly DataGridView _grid;

	private readonly Label _total;

	private List<PartLine> _lines;

	public bool PricesChanged { get; set; }

	public PartsListDialog(Project project)
	{
		_grid = new DataGridView
		{
			Dock = DockStyle.Fill,
			AllowUserToAddRows = false,
			AllowUserToDeleteRows = false,
			RowHeadersVisible = false,
			AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode.Fill,
			BackgroundColor = Color.White
		};
		_total = new Label
		{
			Dock = DockStyle.Bottom,
			Height = 28,
			TextAlign = ContentAlignment.MiddleRight,
			Font = new Font("Segoe UI", 10f, FontStyle.Bold),
			Padding = new Padding(0, 0, 12, 0)
		};
		_project = project;
		Text = "Parts list";
		Font = new Font("Segoe UI", 9f);
		base.Size = new Size(820, 520);
		base.StartPosition = FormStartPosition.CenterParent;
		base.ShowInTaskbar = false;
		base.KeyPreview = true;
		base.KeyDown += [SpecialName] (object s, KeyEventArgs e) =>
		{
			if (e.KeyCode == Keys.Escape)
			{
				Close();
			}
		};
		_grid.Columns.Add("qty", "Qty");
		_grid.Columns.Add("desc", "Description");
		_grid.Columns.Add("labels", "Labels");
		_grid.Columns.Add("price", "Unit price (₹)");
		_grid.Columns.Add("total", "Total (₹)");
		_grid.Columns[0].FillWeight = 12f;
		_grid.Columns[1].FillWeight = 120f;
		_grid.Columns[2].FillWeight = 60f;
		_grid.Columns[3].FillWeight = 30f;
		_grid.Columns[4].FillWeight = 30f;
		foreach (DataGridViewColumn column in _grid.Columns)
		{
			column.ReadOnly = Operators.CompareString(column.Name, "price", TextCompare: false) != 0;
		}
		_grid.Columns["price"].DefaultCellStyle.BackColor = Color.FromArgb(255, 252, 225);
		_grid.CellEndEdit += OnPriceEdited;
		FlowLayoutPanel flowLayoutPanel = new FlowLayoutPanel
		{
			Dock = DockStyle.Bottom,
			Height = 40,
			FlowDirection = FlowDirection.RightToLeft,
			Padding = new Padding(6)
		};
		Button button = new Button
		{
			Text = "Close",
			Width = 90,
			DialogResult = DialogResult.OK
		};
		Button button2 = new Button
		{
			Text = "Export CSV...",
			Width = 110
		};
		button2.Click += OnExportCsv;
		flowLayoutPanel.Controls.AddRange(new Control[2] { button, button2 });
		Label value = new Label
		{
			Dock = DockStyle.Top,
			Height = 24,
			Padding = new Padding(6, 5, 0, 0),
			Text = "Prices are typical estimates; click a price to change it. Your prices are saved with the project."
		};
		base.Controls.Add(_grid);
		base.Controls.Add(value);
		base.Controls.Add(_total);
		base.Controls.Add(flowLayoutPanel);
		base.AcceptButton = button;
		Fill();
	}

	private void Fill()
	{
		_lines = PartsList.Build(_project);
		_grid.Rows.Clear();
		foreach (PartLine line in _lines)
		{
			_grid.Rows.Add(line.Quantity, line.Description, line.Labels, line.UnitPrice.ToString("0.00"), line.Total.ToString("#,##0.00"));
		}
		_total.Text = $"Total: ₹{_lines.Sum([SpecialName] (PartLine l) => l.Total):#,##0.00}";
	}

	private void OnPriceEdited(object sender, DataGridViewCellEventArgs e)
	{
		if (e.RowIndex >= 0 && e.RowIndex < _lines.Count)
		{
			string s = Convert.ToString(RuntimeHelpers.GetObjectValue(_grid.Rows[e.RowIndex].Cells["price"].Value));
			if (double.TryParse(s, NumberStyles.Float, CultureInfo.CurrentCulture, out var result) || double.TryParse(s, NumberStyles.Float, CultureInfo.InvariantCulture, out result))
			{
				_project.PartCosts[_lines[e.RowIndex].Description] = Math.Max(0.0, result);
				PricesChanged = true;
			}
			BeginInvoke(new Action(Fill));
		}
	}

	private void OnExportCsv(object sender, EventArgs e)
	{
		using SaveFileDialog saveFileDialog = new SaveFileDialog
		{
			Filter = "CSV file (*.csv)|*.csv",
			FileName = "parts-list.csv"
		};
		if (saveFileDialog.ShowDialog(this) == DialogResult.OK)
		{
			File.WriteAllText(saveFileDialog.FileName, PartsList.ToCsv(_lines), new UTF8Encoding(encoderShouldEmitUTF8Identifier: true));
		}
	}
}
