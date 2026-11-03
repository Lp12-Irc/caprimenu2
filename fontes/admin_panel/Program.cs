// language: C# / file: Program.cs / runtime: .NET 8 / target: Windows x64 WinForms
using System;
using System.Windows.Forms;

static class Program {
    [STAThread]
    static void Main() {
        Application.SetHighDpiMode(HighDpiMode.PerMonitorV2);
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);
        try{
            string? endpoint=AdminUpdate.RemoteEndpoint();
            if(endpoint==null && !System.IO.File.Exists(System.IO.Path.Combine(AppContext.BaseDirectory,"hillblack.db")) && !System.IO.File.Exists(System.IO.Path.Combine(AppContext.BaseDirectory,"servidor/license_server.py"))){MessageBox.Show("Extraia o ZIP completo Painel ADM. A configuracao do servidor esta ausente.");return;}
            using var form=new MainForm(null,endpoint);
            using var timer=new Timer{Interval=60000};timer.Tick+=async(_,_)=>await AdminUpdate.CheckAsync(form);timer.Start();
            form.Shown+=async(_,_)=>await AdminUpdate.CheckAsync(form);
            Application.Run(form);
        }catch(Exception){MessageBox.Show("Configuracao do ADM invalida. Extraia o ZIP completo ou confira admin-connection.json.");}
    }
}
