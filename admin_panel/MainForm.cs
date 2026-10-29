// language: C#, file: MainForm.cs — WinForms admin panel
using System;
using System.Drawing;
using System.Linq;
using System.Windows.Forms;

public class MainForm : Form {
    private Database _db;
    private Panel    _loginPanel = new();
    private Panel    _mainPanel  = new();
    private DataGridView _grid   = new();
    private TextBox  _tbKey      = new();
    private TextBox  _tbNote     = new();
    private NumericUpDown _nudDays = new();
    private CheckBox _cbLifetime = new();
    private Label    _lblStatus  = new();

    public MainForm() {
        Text = "HillBlack Admin Panel";
        Size = new Size(900, 600);
        BackColor = Color.FromArgb(18, 18, 18);
        ForeColor = Color.WhiteSmoke;
        _db = new Database("hillblack.db");
        _db.Init();
        BuildLoginPanel();
        BuildMainPanel();
        Controls.Add(_loginPanel);
        Controls.Add(_mainPanel);
        ShowPanel(false);
    }

    private void BuildLoginPanel() {
        _loginPanel.Dock = DockStyle.Fill;
        _loginPanel.BackColor = Color.FromArgb(25, 25, 25);

        var lblTitle = new Label { Text = "HillBlack Admin", Font = new Font("Segoe UI", 18, FontStyle.Bold),
            AutoSize = true, Location = new Point(300, 140), ForeColor = Color.Orange };
        var lblUser  = new Label { Text = "Username:", AutoSize = true, Location = new Point(260, 210), ForeColor = Color.Silver };
        var lblPass  = new Label { Text = "Password:", AutoSize = true, Location = new Point(260, 245), ForeColor = Color.Silver };
        var tbUser   = new TextBox { Location = new Point(360, 207), Width = 200, BackColor = Color.FromArgb(40,40,40), ForeColor = Color.White };
        var tbPass   = new TextBox { Location = new Point(360, 242), Width = 200, PasswordChar = '●',
            BackColor = Color.FromArgb(40,40,40), ForeColor = Color.White };
        var btnLogin = new Button { Text = "Login", Location = new Point(360, 280), Width = 200,
            BackColor = Color.Orange, ForeColor = Color.Black, FlatStyle = FlatStyle.Flat };

        btnLogin.Click += (s, e) => {
            if (_db.CheckLogin(tbUser.Text, tbPass.Text)) ShowPanel(true);
            else MessageBox.Show("Invalid credentials.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
        };

        _loginPanel.Controls.AddRange(new Control[] { lblTitle, lblUser, lblPass, tbUser, tbPass, btnLogin });
    }

    private void BuildMainPanel() {
        _mainPanel.Dock = DockStyle.Fill;
        _mainPanel.BackColor = Color.FromArgb(18, 18, 18);

        // Key generation area
        var pnlTop = new Panel { Dock = DockStyle.Top, Height = 80, BackColor = Color.FromArgb(25,25,25) };
        var lblGen = new Label { Text = "Generate Key:", AutoSize = true, Location = new Point(10,10), ForeColor = Color.Orange };
        var lblDays = new Label { Text = "Days:", AutoSize = true, Location = new Point(130,12), ForeColor = Color.Silver };
        _nudDays = new NumericUpDown { Location = new Point(175,10), Width = 70, Minimum = 1, Maximum = 36500, Value = 30,
            BackColor = Color.FromArgb(40,40,40), ForeColor = Color.White };
        _cbLifetime = new CheckBox { Text = "Lifetime", Location = new Point(255,12), ForeColor = Color.Silver, AutoSize = true };
        var lblNote = new Label { Text = "Note:", AutoSize = true, Location = new Point(350,12), ForeColor = Color.Silver };
        _tbNote = new TextBox { Location = new Point(395,10), Width = 160, BackColor = Color.FromArgb(40,40,40), ForeColor = Color.White };
        var btnGen = new Button { Text = "Generate", Location = new Point(570,8), Width = 90,
            BackColor = Color.Orange, ForeColor = Color.Black, FlatStyle = FlatStyle.Flat };
        var btnRevoke = new Button { Text = "Revoke Selected", Location = new Point(670,8), Width = 130,
            BackColor = Color.DarkRed, ForeColor = Color.White, FlatStyle = FlatStyle.Flat };
        var btnCopy = new Button { Text = "Copy Key", Location = new Point(810,8), Width = 80,
            BackColor = Color.FromArgb(40,80,40), ForeColor = Color.White, FlatStyle = FlatStyle.Flat };
        _lblStatus = new Label { AutoSize = true, Location = new Point(10, 50), ForeColor = Color.LightGreen };

        btnGen.Click += (s, e) => {
            DateTime? exp = _cbLifetime.Checked ? (DateTime?)null
                : DateTime.UtcNow.AddDays((double)_nudDays.Value);
            string key = _db.GenerateKey(exp, _tbNote.Text);
            _lblStatus.Text = $"Generated: {key}";
            Clipboard.SetText(key);
            RefreshGrid();
        };

        btnRevoke.Click += (s, e) => {
            if (_grid.SelectedRows.Count == 0) return;
            string? k = _grid.SelectedRows[0].Cells["KeyValue"]?.Value?.ToString();
            if (k != null) { _db.RevokeKey(k); RefreshGrid(); }
        };

        btnCopy.Click += (s, e) => {
            if (_grid.SelectedRows.Count == 0) return;
            string? k = _grid.SelectedRows[0].Cells["KeyValue"]?.Value?.ToString();
            if (k != null) Clipboard.SetText(k);
        };

        pnlTop.Controls.AddRange(new Control[]{
            lblGen, lblDays, _nudDays, _cbLifetime, lblNote, _tbNote,
            btnGen, btnRevoke, btnCopy, _lblStatus });

        // Grid
        _grid.Dock = DockStyle.Fill;
        _grid.BackgroundColor = Color.FromArgb(18,18,18);
        _grid.ForeColor = Color.WhiteSmoke;
        _grid.GridColor = Color.FromArgb(40,40,40);
        _grid.DefaultCellStyle.BackColor = Color.FromArgb(25,25,25);
        _grid.DefaultCellStyle.ForeColor = Color.WhiteSmoke;
        _grid.DefaultCellStyle.SelectionBackColor = Color.Orange;
        _grid.DefaultCellStyle.SelectionForeColor = Color.Black;
        _grid.SelectionMode = DataGridViewSelectionMode.FullRowSelect;
        _grid.ReadOnly = true;
        _grid.AllowUserToAddRows = false;
        _grid.RowHeadersVisible = false;
        _grid.AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode.Fill;
        _grid.Name = "KeyGrid";

        _mainPanel.Controls.Add(_grid);
        _mainPanel.Controls.Add(pnlTop);
    }

    private void RefreshGrid() {
        _grid.DataSource = null;
        var keys = _db.GetAllKeys();
        var dt = new System.Data.DataTable();
        dt.Columns.Add("ID");
        dt.Columns.Add("KeyValue");
        dt.Columns.Add("Status");
        dt.Columns.Add("Created");
        dt.Columns.Add("Expires");
        dt.Columns.Add("HWID");
        dt.Columns.Add("Note");
        foreach (var k in keys)
            dt.Rows.Add(k.Id, k.KeyValue, k.Status,
                        k.CreatedAt[..10], k.ExpiresAt == "never" ? "Lifetime" : k.ExpiresAt[..10],
                        k.HwidBound, k.Note);
        _grid.DataSource = dt;
    }

    private void ShowPanel(bool main) {
        _loginPanel.Visible = !main;
        _mainPanel.Visible  = main;
        if (main) RefreshGrid();
    }
}
