// language: C# / file: Program.cs / runtime: .NET 8 / target: isolated Windows x64 admin UI smoke tests
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Windows.Forms;

internal static class Smoke {
    private static readonly BindingFlags Private = BindingFlags.NonPublic | BindingFlags.Instance;
    private static T Field<T>(MainForm form, string name) => (T)(typeof(MainForm).GetField(name, Private)?.GetValue(form) ?? throw new Exception("Field missing: " + name));
    private static void Call(MainForm form, string name, params object?[] args) => typeof(MainForm).GetMethod(name, Private)!.Invoke(form, args);
    private static void Check(bool result, string message) { if (!result) throw new Exception(message); Console.WriteLine("PASS " + message); }

    [STAThread]
    private static int Main(string[] args) {
        string folder = Path.Combine(Path.GetTempPath(), "HillBlack.Admin.Smoke", Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(folder);
        string path = Path.Combine(folder, "hillblack.db");
        Console.WriteLine("Isolated database: " + path);
        Application.SetHighDpiMode(HighDpiMode.PerMonitorV2);
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);
        using var form = new MainForm(path);
        int result = 0;
        form.Shown += (_, _) => form.BeginInvoke(new Action(() => {
            try {
                var db = Field<Database>(form, "_db");
                Check(!db.CheckLogin("nonexistent", "invalid"), "unknown login rejected");
                Check(!db.CheckLogin("HillBlack", "wrong"), "wrong password rejected");
                Check(db.CheckLogin("HillBlack", "HillBlack2025!"), "existing authentication remains valid");
                var active = db.GenerateKey(DateTime.UtcNow.AddDays(30), "Equipe Atlas · mensal");
                db.GenerateKey(null, "Studio Norte · vitalícia");
                db.GenerateKey(DateTime.UtcNow.AddDays(-1), "Conta de teste · expirada");
                var revoked = db.GenerateKey(null, "Acesso encerrado · revogada");
                db.RevokeKey(revoked);
                Call(form, "ShowDashboard", true);
                var grid = Field<DataGridView>(form, "_grid");
                var search = Field<TextBox>(form, "_search");
                Check(grid.Rows.Count == 4, "all database records rendered");
                Check(Field<Label>(form, "_active").Text == "2", "expired records excluded from active metric");
                search.Text = "Atlas";
                Check(grid.Rows.Count == 1, "search filters observations");
                search.Text = active.Substring(0, 12);
                Check(grid.Rows.Count == 1, "search filters key values");
                search.Clear();
                Call(form, "RefreshRecords", active);
                Check(Field<TextBox>(form, "_selectedKey").Text == active, "selection restored by key");
                Field<TextBox>(form, "_note").Text = "Emitida pelo teste da interface";
                Call(form, "GenerateKey");
                Check(db.GetAllKeys().Count == 5, "generate action persists license");
                Check(grid.Rows.Count == 5, "generated license reflected in grid");
                string generated = Field<TextBox>(form, "_selectedKey").Text;
                Check(generated.Length == 32, "generated license selected");
                Field<Button>(form, "_revoke").PerformClick();
                Check(Field<Button>(form, "_confirm").Visible, "revocation requires explicit confirmation");
                Field<Button>(form, "_cancelRevoke").PerformClick();
                Check(db.GetAllKeys().Single(k => k.KeyValue == generated).Status == "active", "cancel keeps license active");
                Field<Button>(form, "_revoke").PerformClick();
                Field<Button>(form, "_confirm").PerformClick();
                Check(db.GetAllKeys().Single(k => k.KeyValue == generated).Status == "revoked", "confirmed revocation persists");
                Check(!Field<Button>(form, "_revoke").Enabled, "already revoked license cannot be revoked again");
                Call(form, "RefreshRecords", active);
                int renderIndex = Array.IndexOf(args, "--render");
                if (renderIndex >= 0 && renderIndex + 1 < args.Length) {
                    string output = Path.GetFullPath(args[renderIndex + 1]); Directory.CreateDirectory(output);
                    form.Refresh();
                    using (var bitmap = new Bitmap(form.Width, form.Height)) { form.DrawToBitmap(bitmap, form.ClientRectangle); bitmap.Save(Path.Combine(output, "admin-dashboard.png"), ImageFormat.Png); }
                    Call(form, "ShowDashboard", false); form.Refresh();
                    using (var bitmap = new Bitmap(form.Width, form.Height)) { form.DrawToBitmap(bitmap, form.ClientRectangle); bitmap.Save(Path.Combine(output, "admin-login.png"), ImageFormat.Png); }
                    Call(form, "ShowDashboard", true);
                    Console.WriteLine("RENDERED " + output);
                }
                if (args.Contains("--preview")) { Console.WriteLine("PREVIEW READY: isolated dashboard. Close window to end."); }
                else { form.Close(); }
            } catch (Exception ex) { result = 1; Console.Error.WriteLine(ex.InnerException ?? ex); form.Close(); }
        }));
        Application.Run(form);
        Console.WriteLine(result == 0 ? "Admin smoke tests passed." : "Admin smoke tests failed.");
        return result;
    }
}
