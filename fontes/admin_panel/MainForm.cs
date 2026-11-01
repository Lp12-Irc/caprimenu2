// language: C# / file: MainForm.cs / runtime: .NET 8 / target: Windows x64 WinForms
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Globalization;
using System.Linq;
using System.Runtime.InteropServices;
using System.Windows.Forms;

public class MainForm : Form {
    private readonly Database _db;
    private readonly Panel _body = new() { Dock = DockStyle.Fill };
    private readonly Panel _login = new() { Dock = DockStyle.Fill };
    private readonly Panel _dashboard = new() { Dock = DockStyle.Fill };
    private readonly DataGridView _grid = new();
    private readonly TextBox _user = Field("Usuário"), _password = Field("Senha");
    private readonly TextBox _search = Field("Buscar por chave, observação ou dispositivo...");
    private readonly TextBox _note = Field("Cliente, pedido ou observação");
    private readonly TextBox _selectedKey = Field("Selecione uma licença na lista");
    private readonly NumericUpDown _days = new();
    private readonly Label _loginFeedback = Label("", 9, Palette.Red);
    private readonly Label _feedback = Label("Pronto para gerenciar suas licenças.", 9, Palette.Muted);
    private readonly Label _total = Label("0", 26, Palette.Accent, true), _active = Label("0", 26, Palette.Green, true), _inactive = Label("0", 26, Palette.Violet, true);
    private readonly Label _resultCount = Label("0 licenças", 9, Palette.Muted);
    private readonly ActionButton _revoke = new() { Text = "Revogar", Danger = true, Width = 118, Enabled = false };
    private readonly ActionButton _confirm = new() { Text = "Confirmar revogação", Danger = true, Width = 170, Visible = false };
    private readonly ActionButton _cancelRevoke = new() { Text = "Cancelar", Width = 100, Visible = false };
    private readonly ActionButton _copy = new() { Text = "Copiar chave", Width = 130, Enabled = false };
    private readonly List<ActionButton> _filters = new();
    private List<KeyRecord> _records = new();
    private string _statusFilter = "all";
    private string? _pendingRevoke;
    private bool _lifetime, _databaseReady;
    private ActionButton _loginButton = new();
    private readonly System.Windows.Forms.Timer _expiryTimer = new() { Interval = 60000 };
    [DllImport("user32.dll")] private static extern bool ReleaseCapture();
    [DllImport("user32.dll")] private static extern IntPtr SendMessage(IntPtr h, int m, IntPtr w, IntPtr l);

    public MainForm(string? databasePath = null) {
        Text = "Capri · Administração"; StartPosition = FormStartPosition.CenterScreen;
        Size = new Size(1240, 850); MinimumSize = new Size(1060, 760); FormBorderStyle = FormBorderStyle.None;
        BackColor = Palette.Background; ForeColor = Palette.Text; Font = new Font("Segoe UI", 10);
        AutoScaleMode = AutoScaleMode.Dpi; DoubleBuffered = true; Padding = new Padding(1);
        _db = new Database(databasePath ?? System.IO.Path.Combine(AppContext.BaseDirectory, "hillblack.db"));
        Controls.Add(_body); Controls.Add(BuildChrome()); BuildLogin(); BuildDashboard();
        _body.Controls.Add(_dashboard); _body.Controls.Add(_login); ShowDashboard(false);
        Shown += (_, _) => {
            try { _db.Init(); _databaseReady = true; _user.Focus(); }
            catch (Exception ex) { _loginFeedback.Text = "Não foi possível abrir hillblack.db. " + ex.Message; _loginButton.Enabled = false; }
        };
        _expiryTimer.Tick += (_, _) => { if (_dashboard.Visible) ApplyFilter(); }; _expiryTimer.Start();
    }
    private Panel BuildChrome() {
        var p = new Panel { Dock = DockStyle.Top, Height = 42, BackColor = Palette.Sidebar };
        var title = Label("CAPRI   /   ADMIN CONSOLE", 9, Palette.Muted, true); title.Padding = new Padding(18, 0, 0, 0);
        MouseEventHandler drag = (_, e) => { if (e.Button == MouseButtons.Left) { ReleaseCapture(); SendMessage(Handle, 0xA1, new IntPtr(2), IntPtr.Zero); } };
        title.MouseDown += drag; p.MouseDown += drag; title.DoubleClick += (_, _) => ToggleMaximize();
        var minimize = new ActionButton { Text = "−", Dock = DockStyle.Right, Width = 46, TabStop = false };
        var maximize = new ActionButton { Text = "□", Dock = DockStyle.Right, Width = 46, TabStop = false };
        var close = new ActionButton { Text = "×", Dock = DockStyle.Right, Width = 46, Danger = true, TabStop = false };
        minimize.Click += (_, _) => WindowState = FormWindowState.Minimized; maximize.Click += (_, _) => ToggleMaximize(); close.Click += (_, _) => Close();
        p.Controls.Add(title); p.Controls.Add(minimize); p.Controls.Add(maximize); p.Controls.Add(close); return p;
    }
    private void ToggleMaximize() { MaximizedBounds = Screen.FromControl(this).WorkingArea; WindowState = WindowState == FormWindowState.Maximized ? FormWindowState.Normal : FormWindowState.Maximized; }
    protected override void WndProc(ref Message m) {
        base.WndProc(ref m); if (m.Msg != 0x84 || WindowState != FormWindowState.Normal || (int)m.Result != 1) return;
        var p = PointToClient(new Point(unchecked((short)((long)m.LParam & 0xffff)), unchecked((short)(((long)m.LParam >> 16) & 0xffff))));
        int edge = Math.Max(6, DeviceDpi / 16); bool left = p.X < edge, right = p.X >= Width - edge, top = p.Y < edge, bottom = p.Y >= Height - edge;
        if (top && left) m.Result = new IntPtr(13); else if (top && right) m.Result = new IntPtr(14);
        else if (bottom && left) m.Result = new IntPtr(16); else if (bottom && right) m.Result = new IntPtr(17);
        else if (left) m.Result = new IntPtr(10); else if (right) m.Result = new IntPtr(11);
        else if (top) m.Result = new IntPtr(12); else if (bottom) m.Result = new IntPtr(15);
    }
    private void BuildLogin() {
        var layout = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 2, BackColor = Palette.Background, Padding = new Padding(56, 42, 56, 42) };
        layout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 48)); layout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 52));
        var intro = new TableLayoutPanel { Dock = DockStyle.Fill, RowCount = 7, ColumnCount = 1, Padding = new Padding(12, 0, 36, 0) };
        foreach (var h in new[] { 70, 52, 105, 82, 72, 72 }) intro.RowStyles.Add(new RowStyle(SizeType.Absolute, h)); intro.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        intro.Controls.Add(new BrandMark(), 0, 0); intro.Controls.Add(Label("CAPRI", 23, Palette.Text, true), 0, 1);
        intro.Controls.Add(Label("Controle em cada\ndetalhe.", 29, Palette.Text, true), 0, 2);
        intro.Controls.Add(Label("Um espaço para criar, acompanhar e\ngerenciar suas licenças.", 12, Palette.Muted), 0, 3);
        intro.Controls.Add(Label("01    Visão clara da sua operação\n        Licenças e vencimentos em um só lugar.", 10, Palette.Muted), 0, 4);
        intro.Controls.Add(Label("02    Ações no seu ritmo\n        Pesquisa, emissão e revogação direta.", 10, Palette.Muted), 0, 5);
        var local = Label("ARMAZENAMENTO LOCAL  ·  HILLBLACK.DB", 8, Palette.Muted, true); local.Dock = DockStyle.Bottom; local.Height = 32; intro.Controls.Add(local, 0, 6);
        var holder = new Panel { Dock = DockStyle.Fill };
        var card = new Surface { Size = new Size(420, 492), Padding = new Padding(28) };
        holder.Resize += (_, _) => { card.Width = Math.Min(480, Math.Max(300, holder.ClientSize.Width)); card.Location = new Point((holder.Width - card.Width) / 2, Math.Max(0, (holder.Height - card.Height) / 2)); };
        var stack = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 10, BackColor = Palette.Surface };
        foreach (var h in new[] { 30, 42, 44, 26, 48, 26, 48, 30, 48 }) stack.RowStyles.Add(new RowStyle(SizeType.Absolute, h)); stack.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        stack.Controls.Add(Label("ÁREA ADMINISTRATIVA", 9, Palette.Accent, true), 0, 0);
        stack.Controls.Add(Label("Bem-vindo de volta", 22, Palette.Text, true), 0, 1);
        stack.Controls.Add(Label("Entre com suas credenciais para continuar.", 10, Palette.Muted), 0, 2);
        stack.Controls.Add(Label("USUÁRIO", 9, Palette.Muted, true), 0, 3); stack.Controls.Add(InputShell(_user), 0, 4);
        stack.Controls.Add(Label("SENHA", 9, Palette.Muted, true), 0, 5); _password.UseSystemPasswordChar = true; stack.Controls.Add(InputShell(_password), 0, 6);
        var show = new CheckBox { Text = "Mostrar senha", AutoSize = true, ForeColor = Palette.Muted, FlatStyle = FlatStyle.Flat, BackColor = Palette.Surface, Font = new Font("Segoe UI", 9), Padding = new Padding(0, 4, 0, 0) };
        show.CheckedChanged += (_, _) => _password.UseSystemPasswordChar = !show.Checked; stack.Controls.Add(show, 0, 7);
        _loginButton = new ActionButton { Text = "Entrar no painel   →", Primary = true, Dock = DockStyle.Fill, Margin = new Padding(0, 4, 0, 0) };
        _loginButton.Click += (_, _) => Login(); stack.Controls.Add(_loginButton, 0, 8);
        _loginFeedback.Padding = new Padding(0, 8, 0, 0); stack.Controls.Add(_loginFeedback, 0, 9);
        card.Controls.Add(stack); holder.Controls.Add(card); layout.Controls.Add(intro, 0, 0); layout.Controls.Add(holder, 1, 0); _login.Controls.Add(layout);
    }
    private void Login() {
        if (!_databaseReady) return;
        if (string.IsNullOrWhiteSpace(_user.Text) || string.IsNullOrEmpty(_password.Text)) { _loginFeedback.Text = "Preencha o usuário e a senha."; return; }
        _loginButton.Enabled = false;
        try {
            if (_db.CheckLogin(_user.Text.Trim(), _password.Text)) { _password.Clear(); _loginFeedback.Text = ""; ShowDashboard(true); }
            else { _loginFeedback.Text = "Usuário ou senha inválidos."; _password.SelectAll(); _password.Focus(); }
        } catch (Exception ex) { _loginFeedback.Text = "Falha ao entrar: " + ex.Message; }
        finally { _loginButton.Enabled = true; }
    }
    private void BuildDashboard() {
        var sidebar = new Panel { Dock = DockStyle.Left, Width = 208, BackColor = Palette.Sidebar, Padding = new Padding(20, 26, 20, 20) };
        var brand = new BrandMark { Location = new Point(20, 28) };
        var brandTitle = Label("CAPRI", 15, Palette.Text, true); brandTitle.Dock = DockStyle.None; brandTitle.SetBounds(20, 86, 174, 35);
        var subtitle = Label("Administração", 10, Palette.Muted); subtitle.Dock = DockStyle.None; subtitle.SetBounds(20, 120, 174, 24);
        var section = Label("WORKSPACE", 8, Palette.Muted, true); section.Dock = DockStyle.None; section.SetBounds(20, 188, 174, 26);
        var licenses = new ActionButton { Text = "Licenças", Selected = true }; licenses.SetBounds(20, 222, 168, 44); licenses.Click += (_, _) => _search.Focus();
        var refresh = new ActionButton { Text = "Atualizar lista" }; refresh.SetBounds(20, 280, 168, 42); refresh.Click += (_, _) => RefreshRecords();
        var footer = new Panel { Dock = DockStyle.Bottom, Height = 138, BackColor = Palette.Sidebar };
        var local = Label("●  Banco local\nhillblack.db", 9, Palette.Muted); local.Dock = DockStyle.Top; local.Height = 60;
        var logout = new ActionButton { Text = "Sair da conta", Dock = DockStyle.Bottom }; logout.Click += (_, _) => ShowDashboard(false);
        footer.Controls.Add(local); footer.Controls.Add(logout); sidebar.Controls.AddRange(new Control[] { brand, brandTitle, subtitle, section, licenses, refresh, footer });
        var content = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 7, Padding = new Padding(26, 20, 26, 18), BackColor = Palette.Background };
        foreach (var h in new[] { 76, 102, 150, 84 }) content.RowStyles.Add(new RowStyle(SizeType.Absolute, h));
        content.RowStyles.Add(new RowStyle(SizeType.Percent, 100)); content.RowStyles.Add(new RowStyle(SizeType.Absolute, 90)); content.RowStyles.Add(new RowStyle(SizeType.Absolute, 36));
        var header = new Panel { Dock = DockStyle.Fill };
        var heading = Label("Licenças", 25, Palette.Text, true); heading.Dock = DockStyle.Top; heading.Height = 42;
        var desc = Label("Visibilidade, organização e controle de acesso.", 10, Palette.Muted); desc.Dock = DockStyle.Bottom; desc.Height = 28;
        header.Controls.Add(heading); header.Controls.Add(desc); content.Controls.Add(header, 0, 0);
        var stats = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 3, RowCount = 1, Margin = new Padding(0, 4, 0, 10) };
        stats.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        for (int i = 0; i < 3; i++) stats.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100f / 3));
        stats.Controls.Add(Stat("TOTAL DE LICENÇAS", _total), 0, 0); stats.Controls.Add(Stat("ATIVAS", _active), 1, 0); stats.Controls.Add(Stat("EXPIRADAS / REVOGADAS", _inactive), 2, 0);
        content.Controls.Add(stats, 0, 1); content.Controls.Add(BuildGenerator(), 0, 2); content.Controls.Add(BuildFilters(), 0, 3);
        ConfigureGrid(); content.Controls.Add(_grid, 0, 4); content.Controls.Add(BuildSelection(), 0, 5);
        _feedback.Padding = new Padding(2, 8, 0, 0); content.Controls.Add(_feedback, 0, 6);
        _dashboard.Controls.Add(content); _dashboard.Controls.Add(sidebar);
    }
    private static Surface Stat(string title, Label number) {
        var card = new Surface { Dock = DockStyle.Fill, Padding = new Padding(16, 9, 16, 8), Margin = new Padding(0, 0, 10, 0) };
        var label = Label(title, 8, Palette.Muted, true); label.Dock = DockStyle.Top; label.Height = 22; label.BackColor = Palette.Surface;
        number.Dock = DockStyle.Fill; number.BackColor = Palette.Surface; card.Controls.Add(number); card.Controls.Add(label); return card;
    }
    private Surface BuildGenerator() {
        var card = new Surface { Dock = DockStyle.Fill, Padding = new Padding(18, 10, 18, 14), Margin = new Padding(0, 0, 0, 12) };
        var title = Label("Emitir nova licença", 12, Palette.Text, true); title.Dock = DockStyle.Top; title.Height = 30; title.BackColor = Palette.Surface;
        var fields = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 4, RowCount = 2, BackColor = Palette.Surface };
        fields.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 102)); fields.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 118)); fields.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100)); fields.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 154));
        fields.RowStyles.Add(new RowStyle(SizeType.Absolute, 23)); fields.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        fields.Controls.Add(Label("VALIDADE (DIAS)", 8, Palette.Muted, true), 0, 0); fields.Controls.Add(Label("DURAÇÃO", 8, Palette.Muted, true), 1, 0); fields.Controls.Add(Label("OBSERVAÇÃO", 8, Palette.Muted, true), 2, 0);
        _days.Minimum = 1; _days.Maximum = 36500; _days.Value = 30; _days.BorderStyle = BorderStyle.None; _days.BackColor = Palette.Input; _days.ForeColor = Palette.Text; _days.Font = new Font("Segoe UI", 11); _days.Dock = DockStyle.Fill;
        fields.Controls.Add(InputShell(_days), 0, 1);
        var lifetime = new ActionButton { Text = "Vitalícia: não", Dock = DockStyle.Fill, Margin = new Padding(0, 0, 10, 0) };
        lifetime.Click += (_, _) => { _lifetime = !_lifetime; lifetime.Text = _lifetime ? "Vitalícia: sim" : "Vitalícia: não"; lifetime.Selected = _lifetime; lifetime.Invalidate(); _days.Enabled = !_lifetime; }; fields.Controls.Add(lifetime, 1, 1);
        _note.MaxLength = 300; fields.Controls.Add(InputShell(_note), 2, 1);
        var generate = new ActionButton { Text = "+  Gerar licença", Primary = true, Dock = DockStyle.Fill, Margin = new Padding(0) }; generate.Click += (_, _) => GenerateKey(); fields.Controls.Add(generate, 3, 1);
        card.Controls.Add(fields); card.Controls.Add(title); return card;
    }
    private Panel BuildFilters() {
        var panel = new Panel { Dock = DockStyle.Fill, Margin = new Padding(0), BackColor = Palette.Background };
        var search = InputShell(_search); search.Dock = DockStyle.Top; search.Height = 40; search.Margin = new Padding(0); _search.TextChanged += (_, _) => ApplyFilter();
        var filters = new FlowLayoutPanel { Dock = DockStyle.Bottom, Height = 40, WrapContents = false, BackColor = Palette.Background };
        var labels = new[] { "Todas", "Ativas", "Expiradas", "Revogadas" }; var values = new[] { "all", "active", "expired", "revoked" };
        for (int i = 0; i < labels.Length; i++) {
            string value = values[i]; var button = new ActionButton { Text = labels[i], Selected = i == 0, Width = 95, Height = 32, Margin = new Padding(0, 3, 6, 2), Font = new Font("Segoe UI", 9) };
            button.Click += (_, _) => { _statusFilter = value; foreach (var b in _filters) { b.Selected = b == button; b.Invalidate(); } ApplyFilter(); }; _filters.Add(button); filters.Controls.Add(button);
        }
        _resultCount.AutoSize = true; _resultCount.Dock = DockStyle.None; _resultCount.Margin = new Padding(8, 9, 0, 0); filters.Controls.Add(_resultCount);
        panel.Controls.Add(search); panel.Controls.Add(filters); return panel;
    }
    private void ConfigureGrid() {
        _grid.Dock = DockStyle.Fill; _grid.Margin = new Padding(0, 4, 0, 4); _grid.BackgroundColor = Palette.Surface; _grid.BorderStyle = BorderStyle.None; _grid.GridColor = Palette.Border; _grid.EnableHeadersVisualStyles = false;
        _grid.ColumnHeadersDefaultCellStyle = new DataGridViewCellStyle { BackColor = Palette.Sidebar, ForeColor = Palette.Muted, Font = new Font("Segoe UI", 9, FontStyle.Bold), Padding = new Padding(10, 0, 0, 0), SelectionBackColor = Palette.Sidebar, SelectionForeColor = Palette.Muted };
        _grid.ColumnHeadersHeight = 40; _grid.ColumnHeadersHeightSizeMode = DataGridViewColumnHeadersHeightSizeMode.DisableResizing;
        _grid.DefaultCellStyle = new DataGridViewCellStyle { BackColor = Palette.Surface, ForeColor = Palette.Text, SelectionBackColor = Color.FromArgb(23, 73, 52), SelectionForeColor = Palette.Text, Padding = new Padding(10, 0, 0, 0), Font = new Font("Segoe UI", 9) };
        _grid.AlternatingRowsDefaultCellStyle.BackColor = Color.FromArgb(19, 13, 30); _grid.CellBorderStyle = DataGridViewCellBorderStyle.SingleHorizontal; _grid.ColumnHeadersBorderStyle = DataGridViewHeaderBorderStyle.None;
        _grid.RowTemplate.Height = 43; _grid.RowHeadersVisible = false; _grid.AllowUserToAddRows = _grid.AllowUserToDeleteRows = _grid.AllowUserToResizeRows = false;
        _grid.ReadOnly = true; _grid.MultiSelect = false; _grid.SelectionMode = DataGridViewSelectionMode.FullRowSelect; _grid.AutoGenerateColumns = false; _grid.AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode.Fill;
        foreach (var col in new[] {
            new DataGridViewTextBoxColumn { Name = "KeyValue", HeaderText = "CHAVE", FillWeight = 35, MinimumWidth = 180 },
            new DataGridViewTextBoxColumn { Name = "Status", HeaderText = "STATUS", FillWeight = 15, MinimumWidth = 92 },
            new DataGridViewTextBoxColumn { Name = "Expires", HeaderText = "VALIDADE", FillWeight = 17, MinimumWidth = 105 },
            new DataGridViewTextBoxColumn { Name = "Created", HeaderText = "CRIADA EM", FillWeight = 16, MinimumWidth = 105 },
            new DataGridViewTextBoxColumn { Name = "Note", HeaderText = "OBSERVAÇÃO", FillWeight = 24, MinimumWidth = 100 } }) _grid.Columns.Add(col);
        _grid.SelectionChanged += (_, _) => UpdateSelection(); _grid.CellDoubleClick += (_, e) => { if (e.RowIndex >= 0) CopyKey(); };
        _grid.KeyDown += (_, e) => { if (e.Control && e.KeyCode == Keys.C) { CopyKey(); e.Handled = true; e.SuppressKeyPress = true; } };
        _grid.CellFormatting += (_, e) => { if (e.ColumnIndex == 1 && e.Value is string status && e.CellStyle != null) e.CellStyle.ForeColor = status == "Ativa" ? Palette.Green : status == "Revogada" ? Palette.Red : Palette.Accent; };
        typeof(DataGridView).GetProperty("DoubleBuffered", System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.NonPublic)?.SetValue(_grid, true);
    }
    private Panel BuildSelection() {
        var panel = new Panel { Dock = DockStyle.Fill, BackColor = Palette.Background };
        var label = Label("CHAVE SELECIONADA", 8, Palette.Muted, true); label.Dock = DockStyle.Top; label.Height = 26;
        var row = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 2, RowCount = 1 }; row.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100)); row.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        _selectedKey.ReadOnly = true; _selectedKey.Font = new Font("Consolas", 10); var input = InputShell(_selectedKey); input.Height = 42; input.Dock = DockStyle.Top;
        var actions = new FlowLayoutPanel { AutoSize = true, WrapContents = false, Dock = DockStyle.Top, Height = 44, Margin = new Padding(0) };
        _copy.Margin = new Padding(0, 0, 8, 0); _revoke.Margin = new Padding(0); _copy.Click += (_, _) => CopyKey();
        _revoke.Click += (_, _) => { if (SelectedRecord() is not KeyRecord record) return; _pendingRevoke = record.KeyValue; _confirm.Visible = _cancelRevoke.Visible = true; _revoke.Visible = _copy.Visible = false; Feedback("Revogar a licença selecionada? Ela deixará de autorizar novos acessos.", Palette.Accent); };
        _confirm.Click += (_, _) => RevokeSelected(); _cancelRevoke.Click += (_, _) => { CancelRevoke(); Feedback("Revogação cancelada."); };
        actions.Controls.AddRange(new Control[] { _copy, _revoke, _confirm, _cancelRevoke }); row.Controls.Add(input, 0, 0); row.Controls.Add(actions, 1, 0); panel.Controls.Add(row); panel.Controls.Add(label); return panel;
    }
    private void GenerateKey() {
        try {
            string key = _db.GenerateKey(_lifetime ? null : DateTime.UtcNow.AddDays((double)_days.Value), _note.Text.Trim());
            _search.Clear(); _statusFilter = "all"; for (int i = 0; i < _filters.Count; i++) { _filters[i].Selected = i == 0; _filters[i].Invalidate(); }
            RefreshRecords(key); _selectedKey.Text = key;
            if (TryCopy(key)) Feedback("Licença criada e copiada para a área de transferência.", Palette.Green);
            else Feedback("Licença criada. A área de transferência está ocupada; copie o campo selecionado.", Palette.Accent);
            _note.Clear();
        } catch (Exception ex) { Feedback("Não foi possível gerar a licença: " + ex.Message, Palette.Red); }
    }
    private void RefreshRecords(string? selected = null) {
        try { _records = _db.GetAllKeys(); ApplyFilter(selected); Feedback("Lista atualizada às " + DateTime.Now.ToString("HH:mm") + "."); }
        catch (Exception ex) { Feedback("Falha ao consultar o banco: " + ex.Message, Palette.Red); }
    }
    private void ApplyFilter(string? selected = null) {
        selected ??= SelectedRecord()?.KeyValue; string query = _search.Text.Trim();
        var view = _records.Where(k => (_statusFilter == "all" || EffectiveStatus(k) == _statusFilter) && (query.Length == 0 || k.KeyValue.Contains(query, StringComparison.OrdinalIgnoreCase) || k.Note.Contains(query, StringComparison.OrdinalIgnoreCase) || k.HwidBound.Contains(query, StringComparison.OrdinalIgnoreCase))).ToList();
        _grid.Rows.Clear();
        foreach (var k in view) { int row = _grid.Rows.Add(k.KeyValue, StatusLabel(EffectiveStatus(k)), DisplayDate(k.ExpiresAt), DisplayDate(k.CreatedAt), k.Note); _grid.Rows[row].Tag = k; _grid.Rows[row].Cells[0].ToolTipText = k.KeyValue + (string.IsNullOrWhiteSpace(k.HwidBound) ? "\nDispositivo ainda não vinculado" : "\nDispositivo: " + k.HwidBound); }
        _grid.ClearSelection(); _grid.CurrentCell = null;
        var found = _grid.Rows.Cast<DataGridViewRow>().FirstOrDefault(r => (r.Tag as KeyRecord)?.KeyValue == selected); if (found != null) { _grid.CurrentCell = found.Cells[0]; found.Selected = true; }
        _total.Text = _records.Count.ToString(); int active = _records.Count(k => EffectiveStatus(k) == "active"); _active.Text = active.ToString(); _inactive.Text = (_records.Count - active).ToString(); _resultCount.Text = $"{view.Count} de {_records.Count} licenças"; UpdateSelection();
    }
    private static string EffectiveStatus(KeyRecord key) {
        if (key.Status != "active") return key.Status; if (key.ExpiresAt == "never") return "active";
        return DateTimeOffset.TryParse(key.ExpiresAt, CultureInfo.InvariantCulture, DateTimeStyles.AssumeUniversal, out var expiry) && expiry > DateTimeOffset.UtcNow ? "active" : "expired";
    }
    private static string StatusLabel(string status) => status switch { "active" => "Ativa", "revoked" => "Revogada", "expired" => "Expirada", _ => status };
    private static string DisplayDate(string value) => value == "never" ? "Vitalícia" : DateTimeOffset.TryParse(value, CultureInfo.InvariantCulture, DateTimeStyles.AssumeUniversal, out var d) ? d.ToLocalTime().ToString("dd/MM/yyyy") : "Data inválida";
    private KeyRecord? SelectedRecord() => _grid.SelectedRows.Count > 0 ? _grid.SelectedRows[0].Tag as KeyRecord : null;
    private void UpdateSelection() { var record = SelectedRecord(); _selectedKey.Text = record?.KeyValue ?? ""; _copy.Enabled = record != null; _revoke.Enabled = record != null && record.Status != "revoked"; if (_pendingRevoke != null && record?.KeyValue != _pendingRevoke) CancelRevoke(); }
    private void CopyKey() {
        var record = SelectedRecord(); if (record == null) { Feedback("Selecione uma licença na lista."); return; } _selectedKey.Text = record.KeyValue;
        if (TryCopy(record.KeyValue)) Feedback("Chave copiada. Cole no Capri com Ctrl+V.", Palette.Green); else Feedback("Área de transferência ocupada. Copie o campo selecionado com Ctrl+C.", Palette.Accent);
    }
    private bool TryCopy(string value) { try { Clipboard.SetText(value); return true; } catch (ExternalException) { _selectedKey.Focus(); _selectedKey.SelectAll(); return false; } }
    private void RevokeSelected() {
        string? key = _pendingRevoke; if (key == null) return;
        try { _db.RevokeKey(key); CancelRevoke(); RefreshRecords(key); Feedback("Licença revogada.", Palette.Green); }
        catch (Exception ex) { Feedback("Falha ao revogar: " + ex.Message, Palette.Red); }
    }
    private void CancelRevoke() { _pendingRevoke = null; _confirm.Visible = _cancelRevoke.Visible = false; _revoke.Visible = _copy.Visible = true; }
    private void Feedback(string message, Color? color = null) { _feedback.Text = message; _feedback.ForeColor = color ?? Palette.Muted; }
    private void ShowDashboard(bool show) {
        CancelRevoke(); _login.Visible = !show; _dashboard.Visible = show; AcceptButton = show ? null : _loginButton;
        if (show) { _dashboard.BringToFront(); RefreshRecords(); }
        else { _login.BringToFront(); _records.Clear(); _grid.Rows.Clear(); _selectedKey.Clear(); _password.Clear(); _user.Focus(); }
    }
    protected override bool ProcessCmdKey(ref Message msg, Keys keyData) {
        if (keyData == Keys.Escape && _pendingRevoke != null) { CancelRevoke(); return true; }
        if (_dashboard.Visible && keyData == Keys.F5) { RefreshRecords(); return true; }
        if (_dashboard.Visible && keyData == (Keys.Control | Keys.F)) { _search.Focus(); return true; }
        return base.ProcessCmdKey(ref msg, keyData);
    }
    protected override void Dispose(bool disposing) { if (disposing) _expiryTimer.Dispose(); base.Dispose(disposing); }
    private static Label Label(string text, float size, Color color, bool bold = false) => new() { Text = text, Font = new Font("Segoe UI", size, bold ? FontStyle.Bold : FontStyle.Regular), ForeColor = color, BackColor = Color.Transparent, Dock = DockStyle.Fill, TextAlign = ContentAlignment.MiddleLeft, Margin = new Padding(0), AutoEllipsis = true };
    private static TextBox Field(string placeholder) => new() { PlaceholderText = placeholder, BorderStyle = BorderStyle.None, BackColor = Palette.Input, ForeColor = Palette.Text, Font = new Font("Segoe UI", 11), Dock = DockStyle.Fill };
    private static Surface InputShell(Control field) { var shell = new Surface { Dock = DockStyle.Fill, Fill = Palette.Input, Radius = 8, Padding = new Padding(12, 9, 10, 7), Margin = new Padding(0, 0, 10, 5) }; shell.Controls.Add(field); return shell; }
}

