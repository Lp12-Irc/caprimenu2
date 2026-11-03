// language: C# / file: MainForm.cs / runtime: .NET 8 / target: Windows x64 WinForms
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Globalization;
using System.Linq;
using System.IO;
using System.Text.Json;
using System.Runtime.InteropServices;
using System.Windows.Forms;
using System.Threading.Tasks;
using System.Diagnostics;

public class MainForm : Form {
    private readonly Database _db;
    private readonly AdminLoginStore _loginStore;
    private readonly CheckBox _rememberLogin = new() { Text="Lembrar login neste PC", AutoSize=true, Checked=true, ForeColor=Palette.Muted, FlatStyle=FlatStyle.Flat, BackColor=Palette.Surface, Font=new Font("Segoe UI",9), Padding=new Padding(0,4,0,0) };
    private readonly Panel _body = new() { Dock = DockStyle.Fill };
    private readonly Panel _login = new AmbientPanel { Dock = DockStyle.Fill };
    private readonly Panel _dashboard = new AmbientPanel { Dock = DockStyle.Fill };
    private readonly DataGridView _grid = new();
    private readonly TextBox _user = Field("Usuário"), _password = Field("Senha");
    private readonly TextBox _search = Field("Buscar licença, cliente ou observação  ·  Ctrl+F");
    private readonly TextBox _note = Field("Cliente, pedido ou observação");
    private readonly TextBox _selectedKey = Field("Selecione uma licença na lista");
    private readonly NumericUpDown _days = new();
    private TableLayoutPanel _dashboardLayout = new();
    private Surface _generatorPanel = new();
    private readonly ActionButton _newLicense = new(){Text="+ Nova licença",Primary=true,Dock=DockStyle.Fill};
    private readonly CheckBox _showDetails = new(){Text="Mais detalhes",AutoSize=true,ForeColor=Palette.Muted};
    private readonly Label _selectionNote=Label("",9,Palette.Muted);
    private readonly Label _expiryPreview = Label("",9,Palette.Muted);
    private readonly Label _selectionTitle = Label("CHAVE SELECIONADA",8,Palette.Muted,true);
    private readonly Label _loginFeedback = Label("", 9, Palette.Red);
    private readonly Label _loginHeading=Label("Bem-vindo de volta",22,Palette.Text,true);
    private readonly Label _loginDescription=Label("Entre com suas credenciais para continuar.",10,Palette.Muted);
    private readonly Label _feedback = Label("Pronto para gerenciar suas licenças.", 9, Palette.Muted);
    private readonly Label _total = Label("0", 26, Palette.Accent, true), _active = Label("0", 26, Palette.Green, true), _inactive = Label("0", 26, Palette.Violet, true);
    private readonly Label _resultCount = Label("0 licenças", 9, Palette.Muted);
    private readonly ActionButton _revoke = new() { Text = "Revogar", Danger = true, Width = 118, Enabled = false };
    private readonly ActionButton _confirm = new() { Text = "Confirmar revogação", Danger = true, Width = 170, Visible = false };
    private readonly ActionButton _cancelRevoke = new() { Text = "Cancelar", Width = 100, Visible = false };
    private readonly ActionButton _copy = new() { Text = "Copiar chave", Width = 130, Enabled = false };
    private readonly ActionButton _delete = new(){Text="Apagar key",Danger=true,Width=104,Enabled=false};
    private readonly ActionButton _ips = new() { Text="IPs / limite",Width=112,Enabled=false };
    private readonly List<ActionButton> _filters = new();
    private List<KeyRecord> _records = new();
    private string _statusFilter = "all";
    private string? _pendingRevoke;
    private bool _lifetime, _databaseReady;
    private ActionButton _loginButton = new();
    private readonly System.Windows.Forms.Timer _expiryTimer = new() { Interval = 2000 };
    private readonly System.Windows.Forms.Timer _transition = new() { Interval=16 };
    private readonly Stopwatch _transitionClock=new();
    private string _signedInUser="";
    private int _loginFailures;
    private DateTime _retryAt=DateTime.MinValue;
    private bool _loginPending;
    private bool _refreshPending;
    private bool _deletePending;
    public bool CanAutoUpdate=>!_deletePending && !_loginPending && _pendingRevoke==null && _note.Text.Length==0 && _password.Text.Length==0;
    [DllImport("user32.dll")] private static extern bool SystemParametersInfo(uint action,uint parameter,out bool value,uint flags);
    [DllImport("user32.dll")] private static extern bool ReleaseCapture();
    [DllImport("user32.dll")] private static extern IntPtr SendMessage(IntPtr h, int m, IntPtr w, IntPtr l);

    public MainForm(string? databasePath = null,string? remoteEndpoint=null) {
        Text = "Capri · Administração"; StartPosition = FormStartPosition.CenterScreen;
        var area=Screen.PrimaryScreen?.WorkingArea??new Rectangle(0,0,1240,850);
        Size=new Size(Math.Min(1240,area.Width-24),Math.Min(820,area.Height-24));MinimumSize=new Size(Math.Min(1000,Size.Width),Math.Min(700,Size.Height));FormBorderStyle = FormBorderStyle.None;
        BackColor = Palette.Background; ForeColor = Palette.Text; Font = new Font("Segoe UI", 10);
        AutoScaleMode = AutoScaleMode.Dpi; DoubleBuffered = true; Padding = new Padding(1);
        string resolvedDatabase=Path.GetFullPath(databasePath ?? Path.Combine(AppContext.BaseDirectory,"hillblack.db"));
        _db = new Database(resolvedDatabase,remoteEndpoint);_loginStore=new AdminLoginStore(remoteEndpoint==null?resolvedDatabase:Path.Combine(Path.GetDirectoryName(resolvedDatabase)!,"remote-"+Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(System.Text.Encoding.UTF8.GetBytes(_db.RemoteEndpoint))).Substring(0,16)+".db"));
        Controls.Add(_body); Controls.Add(BuildChrome()); BuildLogin(); BuildDashboard();
        _body.Controls.Add(_dashboard); _body.Controls.Add(_login); ShowDashboard(false);
        Shown += (_, _) => {
            try { _db.Init(); _databaseReady = true; if(!_db.HasAdmins()){_loginHeading.Text="Crie sua conta";_loginDescription.Text="Defina suas credenciais. Não há senha padrão.";_loginFeedback.Text="Use pelo menos 12 caracteres na senha.";_loginButton.Text="Criar minha conta";} if(_db.HasAdmins())RestoreSavedLogin();if(_user.Text.Length>0)_loginButton.Focus();else _user.Focus(); }
            catch (Exception ex) { _loginFeedback.Text = "Não foi possível abrir hillblack.db. " + ex.Message; _loginButton.Enabled = false; }
        };
        _expiryTimer.Tick += (_, _) => { if (_dashboard.Visible && _pendingRevoke==null && OwnedForms.Length==0) {
            UpdateExpiryPreview();
            if(_db.IsRemote){RefreshRemoteRecords();return;}
            try{string? selected=SelectedRecord()?.KeyValue;_records=_db.GetAllKeys();ApplyFilter(selected);}catch(Exception ex){Feedback("Falha ao atualizar: "+ex.Message,Palette.Red);}
        }}; _expiryTimer.Start();
        _transition.Tick+=(_,_)=>{Opacity=Math.Min(1,.75+.25*_transitionClock.ElapsedMilliseconds/220.0);if(Opacity>=1)_transition.Stop();};
        Shown+=(_,_)=>AnimateEntrance();
    }
    private void AnimateEntrance() {
        if(SystemParametersInfo(0x1042,0,out bool enabled,0) && enabled && !SystemInformation.HighContrast){Opacity=.75;_transitionClock.Restart();_transition.Start();}
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
        var layout = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 2, BackColor = Color.Transparent, Padding = new Padding(38, 28, 38, 28) };
        layout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 48)); layout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 52));
        var intro = new TableLayoutPanel { Dock = DockStyle.Fill, RowCount = 7, ColumnCount = 1, BackColor=Color.Transparent,Padding = new Padding(12, 0, 36, 0) };
        foreach (var h in new[] { 70, 52, 105, 82, 72, 72 }) intro.RowStyles.Add(new RowStyle(SizeType.Absolute, h)); intro.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        intro.Controls.Add(new BrandMark(), 0, 0); intro.Controls.Add(Label("CAPRI", 23, Palette.Text, true), 0, 1);
        intro.Controls.Add(Label("Cada acesso,\nsob seu controle.", 27, Palette.Text, true), 0, 2);
        intro.Controls.Add(Label("Crie licenças, acompanhe os prazos\ne organize seus clientes.", 12, Palette.Muted), 0, 3);
        intro.Controls.Add(Label("SUA CONTA\nAcesso reservado aos administradores.", 10, Palette.Muted), 0, 4);
        intro.Controls.Add(Label("CONTROLE\nConfirmação antes de revogar um acesso.", 10, Palette.Muted), 0, 5);
        var local = Label("ARMAZENAMENTO LOCAL  ·  HILLBLACK.DB", 8, Palette.Muted, true); local.Dock = DockStyle.Bottom; local.Height = 32; intro.Controls.Add(local, 0, 6);
        var holder = new Panel { Dock = DockStyle.Fill,BackColor=Color.Transparent };
        var card = new Surface { Size = new Size(420, 460), Padding = new Padding(28) };
        holder.Resize += (_, _) => { card.Width = Math.Min(480, Math.Max(300, holder.ClientSize.Width)); card.Location = new Point((holder.Width - card.Width) / 2, Math.Max(0, (holder.Height - card.Height) / 2)); };
        var stack = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 11, BackColor = Palette.Surface };
        foreach (var h in new[] { 24, 38, 40, 24, 44, 24, 44, 30, 30, 46 }) stack.RowStyles.Add(new RowStyle(SizeType.Absolute, h)); stack.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        stack.Controls.Add(Label("ÁREA ADMINISTRATIVA", 9, Palette.Accent, true), 0, 0);
        stack.Controls.Add(_loginHeading, 0, 1);
        stack.Controls.Add(_loginDescription, 0, 2);
        stack.Controls.Add(Label("USUÁRIO", 9, Palette.Muted, true), 0, 3); stack.Controls.Add(InputShell(_user), 0, 4);
        stack.Controls.Add(Label("SENHA", 9, Palette.Muted, true), 0, 5); _password.UseSystemPasswordChar = true; stack.Controls.Add(InputShell(_password), 0, 6);
        var show = new CheckBox { Text = "Mostrar senha", AutoSize = true, ForeColor = Palette.Muted, FlatStyle = FlatStyle.Flat, BackColor = Palette.Surface, Font = new Font("Segoe UI", 9), Padding = new Padding(0, 4, 0, 0) };
        show.CheckedChanged += (_, _) => _password.UseSystemPasswordChar = !show.Checked; stack.Controls.Add(show, 0, 7);
        _rememberLogin.CheckedChanged+=(_,_)=>{if(!_rememberLogin.Checked){try{_loginStore.Clear();}catch{_loginFeedback.Text="Não foi possível apagar o login salvo. Tente novamente.";}}};
        stack.Controls.Add(_rememberLogin,0,8);
        _loginButton = new ActionButton { Text = "Entrar no painel", Primary = true, Dock = DockStyle.Fill, Margin = new Padding(0, 4, 0, 0) };
        _loginButton.Click += (_, _) => Login(); stack.Controls.Add(_loginButton, 0, 9);
        _loginFeedback.Padding = new Padding(0, 8, 0, 0); stack.Controls.Add(_loginFeedback, 0, 10);
        card.Controls.Add(stack); holder.Controls.Add(card); layout.Controls.Add(intro, 0, 0); layout.Controls.Add(holder, 1, 0); _login.Controls.Add(layout);
    }
    private void RestoreSavedLogin() {
        try {
            var saved=_loginStore.Load();if(saved is null)return;
            _user.Text=saved.User;_password.Text=saved.Password;_rememberLogin.Checked=true;
        } catch {
            _password.Clear();_loginFeedback.Text="Não foi possível recuperar o login salvo. Digite suas credenciais.";
        }
    }
    private bool SaveLoginPreference(string user,string password) {
        try {if(_rememberLogin.Checked)_loginStore.Save(user,password);else _loginStore.Clear();return true;}
        catch {try{_loginStore.Clear();}catch{}return false;}
    }
    private async void Login() {
        if (!_databaseReady || _loginPending) return;
        if(DateTime.UtcNow<_retryAt){_loginFeedback.Text="Aguarde um momento antes de tentar novamente.";return;}
        if (string.IsNullOrWhiteSpace(_user.Text) || string.IsNullOrEmpty(_password.Text)) { _loginFeedback.Text = "Preencha o usuário e a senha."; return; }
        _loginPending=true;_loginButton.Enabled=false;_user.Enabled=_password.Enabled=_rememberLogin.Enabled=false;
        string user=_user.Text.Trim(),password=_password.Text;
        _loginButton.Text="Verificando acesso…";_loginButton.IsBusy=true;
        try {
            bool first=!_db.HasAdmins();
            if(first && !ConfirmPassword(password))return;
            bool valid=await Task.Run(()=>{if(first){_db.CreateFirstAdmin(user,password);return true;}return _db.CheckLogin(user,password);});
            if(IsDisposed)return;
            if (valid) { bool remembered=SaveLoginPreference(user,password);_signedInUser=user;_loginFailures=0;_password.Clear();_loginHeading.Text="Bem-vindo de volta";_loginDescription.Text="Entre com suas credenciais para continuar."; _loginFeedback.Text = ""; ShowDashboard(true);if(!remembered)Feedback("Login efetuado, mas não foi possível salvar o acesso neste PC.",Palette.Red);AnimateEntrance(); }
            else { if(++_loginFailures>=5){_retryAt=DateTime.UtcNow.AddSeconds(30);_loginFailures=0;}_loginFeedback.Text = "Usuário ou senha inválidos."; _password.SelectAll(); }
        } catch (Exception ex) { _loginFeedback.Text = "Falha ao entrar: " + ex.Message; }
        finally { if(!IsDisposed){_loginPending=false;_loginButton.IsBusy=false;_loginButton.Enabled=true;_user.Enabled=_password.Enabled=_rememberLogin.Enabled=true;_loginButton.Text="Entrar no painel";if(_login.Visible)_password.Focus();} }
    }
    private bool ConfirmPassword(string password) {
        if(password.Length<12 || System.Text.Encoding.UTF8.GetByteCount(password)>72){_loginFeedback.Text="Use pelo menos 12 caracteres, até 72 bytes UTF-8.";return false;}
        using var dialog=new Form{Text="Confirmar senha",ClientSize=new Size(420,160),StartPosition=FormStartPosition.CenterParent,BackColor=Palette.Background,ForeColor=Palette.Text,FormBorderStyle=FormBorderStyle.FixedDialog,MaximizeBox=false,MinimizeBox=false};
        var label=Label("Repita a senha para criar sua conta.",10,Palette.Text);label.Dock=DockStyle.Top;label.Height=42;
        var input=Field("Confirmar senha");input.UseSystemPasswordChar=true;input.Dock=DockStyle.Top;
        var ok=new ActionButton{Text="Confirmar",Primary=true,Dock=DockStyle.Bottom,DialogResult=DialogResult.OK};
        dialog.Padding=new Padding(20);dialog.Controls.Add(input);dialog.Controls.Add(label);dialog.Controls.Add(ok);dialog.AcceptButton=ok;
        if(dialog.ShowDialog(this)!=DialogResult.OK)return false;
        bool matches=input.Text==password;input.Clear();if(!matches)_loginFeedback.Text="As senhas não coincidem.";return matches;
    }
    private async void ChangePassword() {
        if(string.IsNullOrEmpty(_signedInUser))return;
        using var dialog=new Form{Text="Alterar senha",ClientSize=new Size(440,332),StartPosition=FormStartPosition.CenterParent,BackColor=Palette.Background,ForeColor=Palette.Text,FormBorderStyle=FormBorderStyle.FixedDialog,MaximizeBox=false,MinimizeBox=false};
        var layout=new TableLayoutPanel{Dock=DockStyle.Fill,ColumnCount=1,RowCount=8,Padding=new Padding(20)};
        foreach(int h in new[]{20,42,20,42,20,42,32,42})layout.RowStyles.Add(new RowStyle(SizeType.Absolute,h));
        TextBox current=Field("Senha atual"),next=Field("Nova senha"),confirm=Field("Repita a nova senha");
        foreach(var field in new[]{current,next,confirm})field.UseSystemPasswordChar=true;
        layout.Controls.Add(Label("SENHA ATUAL",9,Palette.Muted));layout.Controls.Add(InputShell(current));layout.Controls.Add(Label("NOVA SENHA · MÍNIMO 12 CARACTERES",9,Palette.Muted));layout.Controls.Add(InputShell(next));layout.Controls.Add(Label("CONFIRME A NOVA SENHA",9,Palette.Muted));layout.Controls.Add(InputShell(confirm));
        var feedback=Label("",9,Palette.Red);layout.Controls.Add(feedback);
        var save=new ActionButton{Text="Salvar nova senha",Primary=true,Dock=DockStyle.Fill};layout.Controls.Add(save);
        save.Click+=async(_,_)=>{if(next.Text!=confirm.Text){feedback.Text="As senhas não coincidem.";return;}save.Enabled=false;string old=current.Text,newPassword=next.Text;try{bool changed=await Task.Run(()=>_db.ChangePassword(_signedInUser,old,newPassword));if(dialog.IsDisposed)return;if(changed){bool remembered=SaveLoginPreference(_signedInUser,newPassword);dialog.Close();Feedback(remembered?"Senha atualizada.":"Senha atualizada, mas o login não pôde ser salvo neste PC.",remembered?Palette.Green:Palette.Red);}else feedback.Text="A senha atual está incorreta.";}catch(Exception ex){if(!dialog.IsDisposed)feedback.Text=ex.Message;}finally{if(!dialog.IsDisposed)save.Enabled=true;}};
        dialog.Controls.Add(layout);dialog.AcceptButton=save;dialog.ShowDialog(this);await Task.CompletedTask;
    }
    private void BuildDashboard() {
        var sidebar = new Panel { Dock = DockStyle.Left, Width = 208, BackColor = Palette.Sidebar, Padding = new Padding(20, 26, 20, 20) };
        var brand = new BrandMark { Location = new Point(20, 28) };
        var brandTitle = Label("CAPRI", 15, Palette.Text, true); brandTitle.Dock = DockStyle.None; brandTitle.SetBounds(80, 25, 110, 35);
        var subtitle = Label("Administração", 10, Palette.Muted); subtitle.Dock = DockStyle.None; subtitle.SetBounds(80, 60, 110, 24);
        var section = Label("GERENCIAMENTO", 8, Palette.Muted, true); section.Dock = DockStyle.None; section.SetBounds(20, 124, 174, 26);
        var licenses = new ActionButton { Text = "Licenças", Selected = true }; licenses.SetBounds(20, 163, 168, 44); licenses.Click += (_, _) => _search.Focus();
        var refresh = new ActionButton { Text = "Atualizar lista" }; refresh.SetBounds(20, 218, 168, 42); refresh.Click += (_, _) => RefreshRecords();
        var updates=new ActionButton {Text="Publicar atualizacao"};updates.SetBounds(20,273,168,42);updates.Click+=(_,_)=>ShowReleaseEditor();
        updates.Enabled=!_db.IsRemote;
        var admins=new ActionButton{Text="Administradores",Enabled=!_db.IsRemote};admins.SetBounds(20,328,168,42);admins.Click+=(_,_)=>ShowAdminEditor();
        var footer = new Panel { Dock = DockStyle.Bottom, Height = 180, BackColor = Palette.Sidebar };
        var local = Label(_db.IsRemote?"Servidor compartilhado\n"+new Uri(_db.RemoteEndpoint).Host:"Banco neste PC\nhillblack.db", 9, Palette.Muted); local.Dock = DockStyle.Top; local.Height = 60;
        var logout = new ActionButton { Text = "Sair da conta", Dock = DockStyle.Bottom }; logout.Click += (_, _) => ShowDashboard(false);
        var passwordButton=new ActionButton{Text="Alterar senha",Dock=DockStyle.Bottom};passwordButton.Click+=(_,_)=>ChangePassword();
        var checkUpdate=new ActionButton{Text="Atualizar ADM",Dock=DockStyle.Bottom};checkUpdate.Click+=async(_,_)=>await AdminUpdate.CheckAsync(this,true);
        footer.Controls.Add(local);footer.Controls.Add(checkUpdate);footer.Controls.Add(passwordButton); footer.Controls.Add(logout); sidebar.Controls.AddRange(new Control[] { brand, brandTitle, subtitle, section, licenses, refresh, updates,admins, footer });
        var content = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 7, Padding = new Padding(26, 20, 26, 18), BackColor = Color.Transparent };
        _dashboardLayout=content;
        foreach (var h in new[] { 70, 78, 0, 88 }) content.RowStyles.Add(new RowStyle(SizeType.Absolute, h));
        content.RowStyles.Add(new RowStyle(SizeType.Percent, 100)); content.RowStyles.Add(new RowStyle(SizeType.Absolute, 84)); content.RowStyles.Add(new RowStyle(SizeType.Absolute, 32));
        var header = new Panel { Dock = DockStyle.Fill,BackColor=Color.Transparent,Padding=new Padding(0,0,182,0) };
        _newLicense.Size=new Size(164,42);_newLicense.Dock=DockStyle.Right;_newLicense.Click+=(_,_)=>ToggleGenerator();
        var headerActions=new Panel{Dock=DockStyle.Right,Width=164,Padding=new Padding(0,10,0,18)};headerActions.Controls.Add(_newLicense);
        var heading = Label("Licenças", 22, Palette.Text, true); heading.Dock = DockStyle.Top; heading.Height = 32;
        var desc = Label("Acompanhe os prazos e gerencie o acesso dos seus clientes.", 10, Palette.Muted); desc.Dock = DockStyle.Bottom; desc.Height = 24;
        header.Controls.Add(heading); header.Controls.Add(desc);header.Padding=new Padding(0);
        var headerRow=new Panel{Dock=DockStyle.Fill};header.Dock=DockStyle.Fill;headerRow.Controls.Add(header);headerRow.Controls.Add(headerActions);content.Controls.Add(headerRow,0,0);
        var stats = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 3, RowCount = 1, Margin = new Padding(0, 4, 0, 10) };
        stats.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        for (int i = 0; i < 3; i++) stats.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100f / 3));
        stats.Controls.Add(Stat("TOTAL DE LICENÇAS", _total), 0, 0); stats.Controls.Add(Stat("ATIVAS", _active), 1, 0); stats.Controls.Add(Stat("EXPIRADAS / REVOGADAS", _inactive), 2, 0);
        _generatorPanel=BuildGenerator();_generatorPanel.Visible=false;
        content.Controls.Add(stats, 0, 1); content.Controls.Add(_generatorPanel, 0, 2); content.Controls.Add(BuildFilters(), 0, 3);
        ConfigureGrid(); content.Controls.Add(_grid, 0, 4); content.Controls.Add(BuildSelection(), 0, 5);
        _feedback.Padding = new Padding(2, 8, 0, 0); content.Controls.Add(_feedback, 0, 6);
        _dashboard.Controls.Add(content); _dashboard.Controls.Add(sidebar);
    }
    private void ToggleGenerator() {
        bool show=!_generatorPanel.Visible;_generatorPanel.Visible=show;
        _dashboardLayout.RowStyles[2].Height=show?166:0;
        _newLicense.Text=show?"Fechar emissão":"+ Nova licença";
        if(show)_days.Focus();
    }
    private static Surface Stat(string title,Label number) {
        var card=new Surface{Dock=DockStyle.Fill,Padding=new Padding(16,8,16,8),Margin=new Padding(0,0,10,8)};
        var caption=Label(title,9,Palette.Muted,true);caption.Dock=DockStyle.Fill;
        number.Dock=DockStyle.Right;number.Width=72;number.TextAlign=ContentAlignment.MiddleRight;
        card.Controls.Add(caption);card.Controls.Add(number);return card;
    }
    private Surface BuildGenerator() {
        var card = new Surface { Dock = DockStyle.Fill, Padding = new Padding(18, 10, 18, 14), Margin = new Padding(0, 0, 0, 12) };
        var title = Label("Emitir nova licença", 12, Palette.Text, true); title.Dock = DockStyle.Top; title.Height = 30; title.BackColor = Palette.Surface;
        var fields = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 4, RowCount = 3, BackColor = Palette.Surface };
        fields.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 102)); fields.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 118)); fields.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100)); fields.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 154));
        fields.RowStyles.Add(new RowStyle(SizeType.Absolute, 23)); fields.RowStyles.Add(new RowStyle(SizeType.Percent, 100)); fields.RowStyles.Add(new RowStyle(SizeType.Absolute,28));
        fields.Controls.Add(Label("VALIDADE (DIAS)", 8, Palette.Muted, true), 0, 0); fields.Controls.Add(Label("DURAÇÃO", 8, Palette.Muted, true), 1, 0); fields.Controls.Add(Label("OBSERVAÇÃO", 8, Palette.Muted, true), 2, 0);
        _days.Minimum = 1; _days.Maximum = 36500; _days.Value = 30; _days.ValueChanged += (_,_)=>UpdateExpiryPreview(); _days.BorderStyle = BorderStyle.None; _days.BackColor = Palette.Input; _days.ForeColor = Palette.Text; _days.Font = new Font("Segoe UI", 11); _days.Dock = DockStyle.Fill;
        fields.Controls.Add(InputShell(_days), 0, 1);
        var lifetime = new ActionButton { Text = "Vitalícia: não", Dock = DockStyle.Fill, Margin = new Padding(0, 0, 10, 0) };
        lifetime.Click += (_, _) => { _lifetime = !_lifetime; lifetime.Text = _lifetime ? "Vitalícia: sim" : "Vitalícia: não"; lifetime.Selected = _lifetime; lifetime.Invalidate(); _days.Enabled = !_lifetime;UpdateExpiryPreview(); }; fields.Controls.Add(lifetime, 1, 1);
        _note.MaxLength = 300; fields.Controls.Add(InputShell(_note), 2, 1);
        var generate = new ActionButton { Text = "+  Gerar licença", Primary = true, Dock = DockStyle.Fill, Margin = new Padding(0) }; generate.Click += (_, _) => GenerateKey(); fields.Controls.Add(generate, 3, 1);
        fields.Controls.Add(_expiryPreview,0,2);fields.SetColumnSpan(_expiryPreview,4);UpdateExpiryPreview();
        card.Controls.Add(fields); card.Controls.Add(title); return card;
    }
    private void UpdateExpiryPreview() {
        _expiryPreview.Text=_lifetime?"Vitalícia · sem data de vencimento.":$"{_days.Value} dias a partir da emissão · previsão: {DateTime.UtcNow.AddDays((double)_days.Value).ToLocalTime():dd/MM/yyyy HH:mm:ss} (horário deste PC)";
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
        _showDetails.Margin=new Padding(12,9,0,0);_showDetails.CheckedChanged+=(_,_)=>SetGridDetails();filters.Controls.Add(_showDetails);
        _resultCount.AutoSize = true; _resultCount.Dock = DockStyle.None; _resultCount.Margin = new Padding(8, 9, 0, 0); filters.Controls.Add(_resultCount);
        panel.Controls.Add(search); panel.Controls.Add(filters); return panel;
    }
    private void ConfigureGrid() {
        _grid.Dock = DockStyle.Fill; _grid.Margin = new Padding(0, 4, 0, 4); _grid.BackgroundColor = Palette.Surface; _grid.BorderStyle = BorderStyle.None; _grid.GridColor = Palette.Border; _grid.EnableHeadersVisualStyles = false;
        _grid.ColumnHeadersDefaultCellStyle = new DataGridViewCellStyle { BackColor = Palette.Sidebar, ForeColor = Palette.Muted, Font = new Font("Segoe UI", 9, FontStyle.Bold), Padding = new Padding(10, 0, 0, 0), SelectionBackColor = Palette.Sidebar, SelectionForeColor = Palette.Muted };
        _grid.ColumnHeadersHeight = 40; _grid.ColumnHeadersHeightSizeMode = DataGridViewColumnHeadersHeightSizeMode.DisableResizing;
        _grid.DefaultCellStyle = new DataGridViewCellStyle { BackColor = Palette.Surface, ForeColor = Palette.Text, SelectionBackColor = Color.FromArgb(51, 36, 45), SelectionForeColor = Palette.Text, Padding = new Padding(10, 0, 0, 0), Font = new Font("Segoe UI", 9) };
        _grid.AlternatingRowsDefaultCellStyle.BackColor = Color.FromArgb(24, 28, 35); _grid.CellBorderStyle = DataGridViewCellBorderStyle.SingleHorizontal; _grid.ColumnHeadersBorderStyle = DataGridViewHeaderBorderStyle.None;
        _grid.RowTemplate.Height = 58; _grid.DefaultCellStyle.WrapMode=DataGridViewTriState.True; _grid.RowHeadersVisible = false; _grid.AllowUserToAddRows = _grid.AllowUserToDeleteRows = _grid.AllowUserToResizeRows = false;
        _grid.ReadOnly = true; _grid.MultiSelect = false; _grid.SelectionMode = DataGridViewSelectionMode.FullRowSelect; _grid.AutoGenerateColumns = false; _grid.AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode.Fill;
        foreach (var col in new[] {
            new DataGridViewTextBoxColumn { Name = "KeyValue", HeaderText = "LICENÇA", FillWeight = 35, MinimumWidth = 160 },
            new DataGridViewTextBoxColumn { Name = "Status", HeaderText = "STATUS", FillWeight = 15, MinimumWidth = 92 },
            new DataGridViewTextBoxColumn { Name = "Expires", HeaderText = "VENCE EM", FillWeight = 17, MinimumWidth = 105 },
            new DataGridViewTextBoxColumn { Name = "Created", HeaderText = "CRIADA EM", FillWeight = 16, MinimumWidth = 105 },
            new DataGridViewTextBoxColumn { Name = "IpCount", HeaderText = "IPs", FillWeight = 8, MinimumWidth = 64 },
            new DataGridViewTextBoxColumn { Name = "FirstIp", HeaderText = "PRIMEIRO IP", FillWeight = 20, MinimumWidth = 130 },
            new DataGridViewTextBoxColumn { Name = "Note", HeaderText = "OBSERVAÇÃO", FillWeight = 24, MinimumWidth = 100 },
            new DataGridViewTextBoxColumn {Name="Remaining",HeaderText="RESTANTE",FillWeight=20,MinimumWidth=126} }) _grid.Columns.Add(col);
        _grid.Columns["Remaining"].DisplayIndex=3;SetGridDetails();
        _grid.Columns["Expires"].DefaultCellStyle.WrapMode=DataGridViewTriState.True;
        _grid.Columns["Created"].DefaultCellStyle.WrapMode=DataGridViewTriState.True;
        _grid.SelectionChanged += (_, _) => UpdateSelection(); _grid.CellDoubleClick += (_, e) => { if (e.RowIndex >= 0) CopyKey(); };
        _grid.KeyDown += (_, e) => { if (e.Control && e.KeyCode == Keys.C) { CopyKey(); e.Handled = true; e.SuppressKeyPress = true; } };
        _grid.CellFormatting += (_, e) => { if (e.ColumnIndex == 1 && e.Value is string status && e.CellStyle != null) e.CellStyle.ForeColor = status == "Ativa" ? Palette.Green : status == "Revogada" ? Palette.Red : Palette.Accent; };
        typeof(DataGridView).GetProperty("DoubleBuffered", System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.NonPublic)?.SetValue(_grid, true);
    }
    private void SetGridDetails(){
        _grid.Columns["Created"].Visible=_showDetails.Checked;_grid.Columns["FirstIp"].Visible=_showDetails.Checked;
    }
    private Panel BuildSelection() {
        var panel = new Panel { Dock = DockStyle.Fill, BackColor = Palette.Background };
        var label = _selectionTitle; label.Dock = DockStyle.Top; label.Height = 25;
        var row = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 2, RowCount = 1 }; row.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100)); row.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        _selectedKey.ReadOnly = true; _selectedKey.Font = new Font("Consolas", 10); var input = InputShell(_selectedKey); input.Height = 42; input.Dock = DockStyle.Top;
        var actions = new FlowLayoutPanel { AutoSize = true, WrapContents = false, Dock = DockStyle.Top, Height = 44, Margin = new Padding(0) };
        _copy.Margin = new Padding(0, 0, 8, 0); _revoke.Margin = new Padding(0); _copy.Click += (_, _) => CopyKey();
        _revoke.Click += async (_, _) => { if (SelectedRecord() is not KeyRecord record) return;
            if(record.Status=="revoked"){try{if(_db.IsRemote)await Task.Run(()=>_db.ActivateKey(record.KeyValue));else _db.ActivateKey(record.KeyValue);RefreshRecords(record.KeyValue);Feedback("Chave reativada. Os IPs registrados foram preservados.",Palette.Green);}catch(Exception ex){Feedback(ex.Message,Palette.Red);}return;}
            _pendingRevoke = record.KeyValue; _confirm.Visible = _cancelRevoke.Visible = true; _revoke.Visible = _copy.Visible = false; Feedback("Revogar a licença selecionada? Ela deixará de autorizar novos acessos.", Palette.Accent); };
        _delete.Margin=new Padding(8,0,0,0);_delete.Click+=async(_,_)=>await DeleteSelected();
        _confirm.Click += (_, _) => RevokeSelected(); _cancelRevoke.Click += (_, _) => { CancelRevoke(); Feedback("Revogação cancelada."); };
        _ips.Margin=new Padding(0,0,8,0);_ips.Click+=(_,_)=>ShowIpEditor();
        actions.Controls.AddRange(new Control[] { _copy,_ips, _revoke, _delete, _confirm, _cancelRevoke }); row.Controls.Add(input, 0, 0); row.Controls.Add(actions, 1, 0); panel.Controls.Add(row); panel.Controls.Add(label); return panel;
    }
    private async void GenerateKey() {
        try {
            DateTime? expires=_lifetime?null:DateTime.UtcNow.AddDays((double)_days.Value);string note=_note.Text.Trim();
            string key = _db.IsRemote?await Task.Run(()=>_db.GenerateKey(expires,note)):_db.GenerateKey(expires,note);
            _search.Clear(); _statusFilter = "all"; for (int i = 0; i < _filters.Count; i++) { _filters[i].Selected = i == 0; _filters[i].Invalidate(); }
            RefreshRecords(key); _selectedKey.Text = key;
            if (TryCopy(key)) Feedback("Licença criada e copiada para a área de transferência.", Palette.Green);
            else Feedback("Licença criada. A área de transferência está ocupada; copie o campo selecionado.", Palette.Accent);
            _note.Clear();
        } catch (Exception ex) { Feedback("Não foi possível gerar a licença: " + ex.Message, Palette.Red); }
    }
    private void RefreshRecords(string? selected = null) {
        if(_db.IsRemote){RefreshRemoteRecords(selected);return;}
        try { _records = _db.GetAllKeys(); ApplyFilter(selected); Feedback("Lista atualizada às " + DateTime.Now.ToString("HH:mm") + "."); }
        catch (Exception ex) { Feedback("Falha ao consultar o banco: " + ex.Message, Palette.Red); }
    }
    private async void RefreshRemoteRecords(string? selected=null){
        if(_refreshPending)return;_refreshPending=true;selected??=SelectedRecord()?.KeyValue;
        try{var records=await Task.Run(()=>_db.GetAllKeys());if(IsDisposed || !_dashboard.Visible)return;_records=records;ApplyFilter(selected);Feedback("Servidor sincronizado as "+DateTime.Now.ToString("HH:mm:ss")+".");}
        catch(UnauthorizedAccessException ex){if(!IsDisposed){ShowDashboard(false);_loginFeedback.Text=ex.Message;}}
        catch(Exception){if(!IsDisposed)Feedback("Sem conexao com o servidor. Aguardando reconexao.",Palette.Red);}
        finally{_refreshPending=false;}
    }
    private void ApplyFilter(string? selected = null) {
        selected ??= SelectedRecord()?.KeyValue; string query = _search.Text.Trim();
        var view = _records.Where(k => (_statusFilter == "all" || EffectiveStatus(k) == _statusFilter) && (query.Length == 0 || k.KeyValue.Contains(query, StringComparison.OrdinalIgnoreCase) || k.Note.Contains(query, StringComparison.OrdinalIgnoreCase) || k.HwidBound.Contains(query, StringComparison.OrdinalIgnoreCase))).ToList();
        _grid.Rows.Clear();
        foreach (var k in view) { int row = _grid.Rows.Add(k.KeyValue, StatusLabel(EffectiveStatus(k)), DisplayDate(k.ExpiresAt), DisplayDate(k.CreatedAt),$"{k.IpCount}/{k.IpLimit}",k.FirstIp==""?"Aguardando acesso":k.FirstIp, k.Note, EffectiveStatus(k)=="revoked"?"Revogada":KeyExpiry.Remaining(k.ExpiresAt,DateTimeOffset.UtcNow)); _grid.Rows[row].Tag = k; _grid.Rows[row].Cells[0].ToolTipText = k.KeyValue + (string.IsNullOrWhiteSpace(k.HwidBound) ? "\nDispositivo ainda não vinculado" : "\nDispositivo: " + k.HwidBound);_grid.Rows[row].Cells[5].ToolTipText=k.FirstIp; }
        _grid.ClearSelection(); _grid.CurrentCell = null;
        var found = _grid.Rows.Cast<DataGridViewRow>().FirstOrDefault(r => (r.Tag as KeyRecord)?.KeyValue == selected); if (found != null) { _grid.CurrentCell = found.Cells[0]; found.Selected = true; }
        _total.Text = _records.Count.ToString(); int active = _records.Count(k => EffectiveStatus(k) == "active"); _active.Text = active.ToString(); _inactive.Text = (_records.Count - active).ToString(); _resultCount.Text = $"{view.Count} de {_records.Count} licenças"; UpdateSelection();
    }
    private static string EffectiveStatus(KeyRecord key) {
        if (key.Status != "active") return key.Status; if (key.ExpiresAt == "never") return "active";
        return KeyExpiry.IsExpired(key.ExpiresAt,DateTimeOffset.UtcNow)?"expired":"active";
    }
    private static string StatusLabel(string status) => status switch { "active" => "Ativa", "revoked" => "Revogada", "expired" => "Expirada", _ => status };
    private static string DisplayDate(string value) => value == "never" ? "Vitalícia" : DateTimeOffset.TryParse(value, CultureInfo.InvariantCulture, DateTimeStyles.AssumeUniversal, out var d) ? d.ToLocalTime().ToString("dd/MM/yy")+"\r\n"+d.ToLocalTime().ToString("HH:mm:ss") : "Data inválida";
    private KeyRecord? SelectedRecord() => _grid.SelectedRows.Count > 0 ? _grid.SelectedRows[0].Tag as KeyRecord : null;
    private void UpdateSelection() { var record = SelectedRecord();_delete.Enabled=record!=null; _selectionTitle.Text=record==null?"Selecione uma licença para copiar, gerenciar IPs ou revogar":$"{StatusLabel(EffectiveStatus(record))} · {KeyExpiry.Remaining(record.ExpiresAt,DateTimeOffset.UtcNow)} · horários deste PC"; _selectedKey.Text = record?.KeyValue ?? "";_ips.Enabled=record!=null; _copy.Enabled = record != null; _revoke.Enabled = record != null && EffectiveStatus(record)!="expired";_revoke.Text=record?.Status=="revoked"?"Ativar":"Revogar"; if (_pendingRevoke != null && record?.KeyValue != _pendingRevoke) CancelRevoke(); }
    private async Task DeleteSelected() {
        var record=SelectedRecord();if(record==null || _deletePending)return;
        _deletePending=true;
        try {
            if(MessageBox.Show(this,"Apagar definitivamente a key selecionada?\n\n"+record.KeyValue+"\n\nA licença e os IPs vinculados serão removidos. Essa ação não pode ser desfeita.","Apagar key",MessageBoxButtons.YesNo,MessageBoxIcon.Warning,MessageBoxDefaultButton.Button2)!=DialogResult.Yes)return;
            _delete.Enabled=false;
            if(_db.IsRemote)await Task.Run(()=>_db.DeleteKey(record.KeyValue));else _db.DeleteKey(record.KeyValue);
            CancelRevoke();RefreshRecords();Feedback("Key apagada. Ela não autoriza mais acessos.",Palette.Green);
        }catch(Exception ex){Feedback("Não foi possível apagar a key: "+ex.Message,Palette.Red);}
        finally{_deletePending=false;if(!IsDisposed)UpdateSelection();}
    }
    private void CopyKey() {
        var record = SelectedRecord(); if (record == null) { Feedback("Selecione uma licença na lista."); return; } _selectedKey.Text = record.KeyValue;
        if (TryCopy(record.KeyValue)) Feedback("Chave copiada. Cole no Capri com Ctrl+V.", Palette.Green); else Feedback("Área de transferência ocupada. Copie o campo selecionado com Ctrl+C.", Palette.Accent);
    }
    private bool TryCopy(string value) { try { Clipboard.SetText(value); return true; } catch (ExternalException) { _selectedKey.Focus(); _selectedKey.SelectAll(); return false; } }
    private async void RevokeSelected() {
        string? key = _pendingRevoke; if (key == null) return;
        try { if(_db.IsRemote)await Task.Run(()=>_db.RevokeKey(key));else _db.RevokeKey(key); CancelRevoke(); RefreshRecords(key); Feedback("Licença revogada.", Palette.Green); }
        catch (Exception ex) { Feedback("Falha ao revogar: " + ex.Message, Palette.Red); }
    }
    private void CancelRevoke() { _pendingRevoke = null; _confirm.Visible = _cancelRevoke.Visible = false; _revoke.Visible = _copy.Visible = true; }
    private void Feedback(string message, Color? color = null) { _feedback.Text = message; _feedback.ForeColor = color ?? Palette.Muted; }
    private void ShowDashboard(bool show) {
        CancelRevoke(); _login.Visible = !show; _dashboard.Visible = show; AcceptButton = show ? null : _loginButton;
        if (show) { _dashboard.BringToFront(); RefreshRecords(); }
        else { _db.Logout();_signedInUser="";_login.BringToFront(); _records.Clear(); _grid.Rows.Clear(); _selectedKey.Clear(); _password.Clear(); _user.Focus(); }
    }
    protected override bool ProcessCmdKey(ref Message msg, Keys keyData) {
        if (keyData == Keys.Escape && _pendingRevoke != null) { CancelRevoke(); return true; }
        if (_dashboard.Visible && keyData == Keys.F5) { RefreshRecords(); return true; }
        if (_dashboard.Visible && keyData == (Keys.Control | Keys.F)) { _search.Focus(); return true; }
        return base.ProcessCmdKey(ref msg, keyData);
    }
    protected override void Dispose(bool disposing) { if (disposing) {_expiryTimer.Dispose();_transition.Dispose();} base.Dispose(disposing); }
    private static Label Label(string text, float size, Color color, bool bold = false) => new() { Text = text, Font = new Font("Segoe UI", size, bold ? FontStyle.Bold : FontStyle.Regular), ForeColor = color, BackColor = Color.Transparent, Dock = DockStyle.Fill, TextAlign = ContentAlignment.MiddleLeft, Margin = new Padding(0), AutoEllipsis = true };
    private static TextBox Field(string placeholder) => new() { PlaceholderText = placeholder, BorderStyle = BorderStyle.None, BackColor = Palette.Input, ForeColor = Palette.Text, Font = new Font("Segoe UI", 11), Dock = DockStyle.Fill };
    private static Surface InputShell(Control field) { var shell = new Surface { Dock = DockStyle.Fill, Fill = Palette.Input, Radius = 8, Padding = new Padding(12, 9, 10, 7), Margin = new Padding(0, 0, 10, 5) }; shell.Controls.Add(field); return shell; }
    private void ShowReleaseEditor() {
        using var dialog=new Form {Text="Publicar atualizacao obrigatoria",ClientSize=new Size(640,460),StartPosition=FormStartPosition.CenterParent,FormBorderStyle=FormBorderStyle.FixedDialog,MaximizeBox=false,MinimizeBox=false,BackColor=Palette.Background,ForeColor=Palette.Text};
        string? selected=null;
        var layout=new TableLayoutPanel {Dock=DockStyle.Fill,Padding=new Padding(24),ColumnCount=1,RowCount=8,AutoScroll=true};
        void Add(Control control,int height){control.Dock=DockStyle.Fill;layout.RowStyles.Add(new RowStyle(SizeType.Absolute,height));layout.Controls.Add(control);}
        Add(Label("Escolha o ZIP pronto. Seu PC envia o pacote e os clientes aplicam a atualizacao pelo botao Atualizar.",10,Palette.Muted),44);
        var choose=new ActionButton {Text="Selecionar ZIP do cliente ou do ADM"};Add(choose,40);
        var details=Label("Nenhum pacote selecionado.",10,Palette.Text);Add(details,52);
        Add(Label("O que mudou",9,Palette.Text),24);var notes=Field("Descreva as modificacoes");notes.Multiline=true;notes.ScrollBars=ScrollBars.Vertical;notes.MaxLength=2000;Add(notes,90);
        var feedback=Label("A chave e as configuracoes do cliente serao preservadas.",9,Palette.Muted);Add(feedback,40);
        var publish=new ActionButton {Text="Publicar e exigir atualizacao",Primary=true,Enabled=false};Add(publish,40);layout.RowStyles.Add(new RowStyle(SizeType.Percent,100));
        choose.Click+=(_,_)=>{
            using var picker=new OpenFileDialog {Filter="Pacote Capri (*.zip)|*.zip",Title="Selecionar pacote pronto",InitialDirectory=Path.Combine(AppContext.BaseDirectory,"entregas"),FileName="Capri-Cliente.zip",CheckFileExists=true};
            if(picker.ShowDialog(dialog)!=DialogResult.OK)return;
            try{var release=_db.InspectPackage(picker.FileName);selected=picker.FileName;details.Text=$"{Path.GetFileName(selected)}  |  {release.Version} / build {release.Build}  |  {release.PackageSize/1048576.0:F2} MB";publish.Enabled=true;feedback.Text="Pacote verificado. Pronto para publicar.";feedback.ForeColor=Palette.Green;}
            catch(Exception ex){selected=null;publish.Enabled=false;feedback.Text=ex.Message;feedback.ForeColor=Palette.Red;}
        };
        publish.Click+=(_,_)=>{
            if(selected==null)return;
            if(MessageBox.Show(dialog,"Publicar este pacote e bloquear versoes antigas?","Confirmar publicacao",MessageBoxButtons.YesNo,MessageBoxIcon.Question)!=DialogResult.Yes)return;
            try{_db.PublishPackage(selected,notes.Text);Feedback("Pacote publicado. Cada painel recebe a atualizacao correspondente.",Palette.Green);dialog.Close();}
            catch(Exception ex){feedback.Text=ex.Message;feedback.ForeColor=Palette.Red;}
        };
        dialog.Controls.Add(layout);dialog.ShowDialog(this);
    }
    private void ShowAdminEditor(){
        if(_db.IsRemote || _signedInUser=="")return;
        using var dialog=new Form{Text="Criar administrador",ClientSize=new Size(470,460),StartPosition=FormStartPosition.CenterParent,BackColor=Palette.Background,ForeColor=Palette.Text,FormBorderStyle=FormBorderStyle.FixedDialog,MaximizeBox=false,MinimizeBox=false};
        var layout=new TableLayoutPanel{Dock=DockStyle.Fill,Padding=new Padding(22),ColumnCount=1,AutoScroll=true};
        void Add(Control control,int height){control.Dock=DockStyle.Fill;layout.RowStyles.Add(new RowStyle(SizeType.Absolute,height));layout.Controls.Add(control);}
        Add(Label("Crie um acesso separado para o outro dono. Ele administrara as mesmas chaves pelo servidor.",10,Palette.Muted),52);
        Add(Label("Sua senha atual",9,Palette.Text),23);var current=Field("Senha do administrador conectado");current.UseSystemPasswordChar=true;Add(current,36);
        Add(Label("Usuario do novo administrador",9,Palette.Text),23);var user=Field("Usuario");Add(user,36);
        Add(Label("Nova senha (12 caracteres ou mais)",9,Palette.Text),23);var password=Field("Senha");password.UseSystemPasswordChar=true;Add(password,36);
        Add(Label("Confirme a nova senha",9,Palette.Text),23);var confirmation=Field("Repita a senha");confirmation.UseSystemPasswordChar=true;Add(confirmation,36);
        var feedback=Label("Compartilhe as credenciais separadamente do ZIP.",9,Palette.Muted);Add(feedback,44);
        var save=new ActionButton{Text="Criar acesso",Primary=true};Add(save,42);
        save.Click+=async(_,_)=>{if(password.Text!=confirmation.Text){feedback.Text="As senhas nao coincidem.";return;}
            string owner=_signedInUser,old=current.Text,name=user.Text,secret=password.Text;save.Enabled=false;save.IsBusy=true;
            try{await Task.Run(()=>_db.AddAdmin(owner,old,name,secret));if(dialog.IsDisposed)return;password.Clear();confirmation.Clear();current.Clear();feedback.Text="Administrador criado. Envie o ZIP Painel ADM e as credenciais separadamente.";}
            catch(Exception ex){if(!dialog.IsDisposed)feedback.Text=ex.Message;}finally{if(!dialog.IsDisposed){save.Enabled=true;save.IsBusy=false;}}};
        dialog.Controls.Add(layout);dialog.ShowDialog(this);
    }
    private void ShowIpEditor(){
        var selected=SelectedRecord();if(selected==null)return;string key=selected.KeyValue;
        using var dialog=new Form{Text="IPs por chave",ClientSize=new Size(740,450),StartPosition=FormStartPosition.CenterParent,BackColor=Palette.Background,ForeColor=Palette.Text,FormBorderStyle=FormBorderStyle.FixedDialog,MaximizeBox=false,MinimizeBox=false};
        var layout=new TableLayoutPanel{Dock=DockStyle.Fill,Padding=new Padding(22),ColumnCount=1,RowCount=6};
        foreach(int height in new[]{42,42,48})layout.RowStyles.Add(new RowStyle(SizeType.Absolute,height));
        layout.RowStyles.Add(new RowStyle(SizeType.Percent,100));layout.RowStyles.Add(new RowStyle(SizeType.Absolute,48));layout.RowStyles.Add(new RowStyle(SizeType.Absolute,30));
        var heading=Label("IPs publicos autorizados nesta chave",13,Palette.Text,true);heading.Dock=DockStyle.Fill;layout.Controls.Add(heading);
        var row=new FlowLayoutPanel{Dock=DockStyle.Fill,WrapContents=false};var caption=Label("Limite de IPs",10,Palette.Text);caption.AutoSize=true;caption.Margin=new Padding(0,8,12,0);
        var limit=new NumericUpDown{Minimum=1,Maximum=32,Value=selected.IpLimit,Width=80,BackColor=Palette.Input,ForeColor=Palette.Text,Font=new Font("Segoe UI",11)};
        row.Controls.Add(caption);row.Controls.Add(limit);layout.Controls.Add(row);
        var help=Label("O primeiro acesso valido registra o IP. Mudar de rede, VPN ou operadora pode exigir liberacao aqui.",9,Palette.Muted);help.Dock=DockStyle.Fill;layout.Controls.Add(help);
        var grid=new DataGridView{Dock=DockStyle.Fill,ReadOnly=true,AllowUserToAddRows=false,AllowUserToDeleteRows=false,RowHeadersVisible=false,AutoSizeColumnsMode=DataGridViewAutoSizeColumnsMode.Fill,BackgroundColor=Palette.Surface,BorderStyle=BorderStyle.None,ForeColor=Palette.Text,SelectionMode=DataGridViewSelectionMode.FullRowSelect};
        grid.DefaultCellStyle.BackColor=Palette.Surface;grid.DefaultCellStyle.ForeColor=Palette.Text;grid.DefaultCellStyle.SelectionBackColor=Palette.Sidebar;grid.DefaultCellStyle.SelectionForeColor=Palette.Text;grid.RowTemplate.Height=32;
        grid.EnableHeadersVisualStyles=false;grid.ColumnHeadersDefaultCellStyle.BackColor=Palette.Sidebar;grid.ColumnHeadersDefaultCellStyle.ForeColor=Palette.Muted;grid.ColumnHeadersHeight=32;
        grid.Columns.Add("Ip","IP");grid.Columns.Add("First","PRIMEIRO ACESSO");grid.Columns.Add("Last","ULTIMO ACESSO");grid.Columns[0].FillWeight=45;grid.Columns[1].FillWeight=grid.Columns[2].FillWeight=27.5f;layout.Controls.Add(grid);
        var actions=new FlowLayoutPanel{Dock=DockStyle.Fill,WrapContents=false,Padding=new Padding(0,7,0,0)};
        var save=new ActionButton{Text="Salvar limite",Primary=true,Width=160};var clear=new ActionButton{Text="Liberar troca de IP",Width=180,Danger=true};actions.Controls.Add(save);actions.Controls.Add(clear);layout.Controls.Add(actions);
        var feedback=Label("",9,Palette.Muted);feedback.Dock=DockStyle.Fill;layout.Controls.Add(feedback);
        void Reload(){grid.Rows.Clear();var ips=_db.GetKeyIps(key);foreach(var ip in ips)grid.Rows.Add(ip.Ip,DateTimeOffset.Parse(ip.FirstSeen,CultureInfo.InvariantCulture).ToLocalTime().ToString("dd/MM/yyyy HH:mm:ss"),DateTimeOffset.Parse(ip.LastSeen,CultureInfo.InvariantCulture).ToLocalTime().ToString("dd/MM/yyyy HH:mm:ss"));feedback.Text=$"{ips.Count} de {(int)limit.Value} IPs registrados. O primeiro IP e preservado ao reduzir o limite.";}
        save.Click+=(_,_)=>{try{
            if((int)limit.Value<_db.GetKeyIps(key).Count && MessageBox.Show(dialog,"Reduzir o limite remove os IPs excedentes, mantendo os primeiros registrados. Continuar?","Reduzir limite",MessageBoxButtons.YesNo,MessageBoxIcon.Question)!=DialogResult.Yes)return;
            _db.SetIpLimit(key,(int)limit.Value);Reload();RefreshRecords(key);feedback.Text="Limite salvo. A regra vale na proxima verificacao do cliente.";
        }catch(Exception ex){feedback.Text=ex.Message;}};
        clear.Click+=(_,_)=>{if(MessageBox.Show(dialog,"Feche as sessoes antigas antes de liberar. O proximo acesso valido registrara o novo primeiro IP. Limpar os IPs desta chave?","Liberar troca de IP",MessageBoxButtons.YesNo,MessageBoxIcon.Question)!=DialogResult.Yes)return;
            try{_db.ResetKeyIps(key);Reload();RefreshRecords(key);}catch(Exception ex){feedback.Text=ex.Message;}};
        dialog.Controls.Add(layout);try{Reload();}catch(Exception ex){feedback.Text=ex.Message;}dialog.ShowDialog(this);
    }
}
