// language: C# / file: Program.cs / runtime: .NET 8 / target: Windows x64 WinForms
using System;
using System.Windows.Forms;

static class Program {
    [STAThread]
    static void Main() {
        Application.SetHighDpiMode(HighDpiMode.PerMonitorV2);
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);
        Application.Run(new MainForm());
    }
}
