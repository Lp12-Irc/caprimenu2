// language: C# / file: ThemeControls.cs / runtime: .NET 8 / target: Windows x64 WinForms
using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Windows.Forms;

internal static class Palette {
    public static readonly Color Background = Color.FromArgb(8, 6, 18);
    public static readonly Color Sidebar = Color.FromArgb(14, 9, 26);
    public static readonly Color Surface = Color.FromArgb(22, 16, 34);
    public static readonly Color Input = Color.FromArgb(12, 9, 22);
    public static readonly Color Border = Color.FromArgb(65, 35, 83);
    public static readonly Color Text = Color.FromArgb(240, 248, 250);
    public static readonly Color Muted = Color.FromArgb(167, 168, 190);
    public static readonly Color Accent = Color.FromArgb(24, 242, 89);
    public static readonly Color Green = Color.FromArgb(24, 242, 89);
    public static readonly Color Violet = Color.FromArgb(151, 73, 226);
    public static readonly Color Red = Color.FromArgb(255, 28, 99);
    public static GraphicsPath Round(Rectangle rect, int radius) {
        var p = new GraphicsPath();
        int d = Math.Min(radius * 2, Math.Min(rect.Width, rect.Height));
        if (d <= 0) { p.AddRectangle(rect); return p; }
        p.AddArc(rect.X, rect.Y, d, d, 180, 90);
        p.AddArc(rect.Right - d, rect.Y, d, d, 270, 90);
        p.AddArc(rect.Right - d, rect.Bottom - d, d, d, 0, 90);
        p.AddArc(rect.X, rect.Bottom - d, d, d, 90, 90);
        p.CloseFigure(); return p;
    }
}

internal sealed class Surface : Panel {
    public int Radius { get; set; } = 14;
    public Color Fill { get; set; } = Palette.Surface;
    public Surface() { DoubleBuffered = true; SetStyle(ControlStyles.ResizeRedraw, true); BackColor = Palette.Background; Padding = new Padding(20); }
    protected override void OnPaint(PaintEventArgs e) {
        base.OnPaint(e); e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
        using var p = Palette.Round(new Rectangle(0, 0, Width - 1, Height - 1), Radius);
        using var b = new SolidBrush(Fill); using var pen = new Pen(Palette.Border);
        e.Graphics.FillPath(b, p); e.Graphics.DrawPath(pen, p);
    }
}

internal sealed class ActionButton : Button {
    private bool _hover, _pressed;
    public bool Primary { get; set; }
    public bool Danger { get; set; }
    public bool Selected { get; set; }
    public ActionButton() {
        SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw, true);
        FlatStyle = FlatStyle.Flat; FlatAppearance.BorderSize = 0;
        BackColor = Palette.Surface; ForeColor = Palette.Text;
        Font = new Font("Segoe UI", 10, FontStyle.Bold); Cursor = Cursors.Hand; Height = 40; UseVisualStyleBackColor = false;
    }
    protected override void OnMouseEnter(EventArgs e) { _hover = true; Invalidate(); base.OnMouseEnter(e); }
    protected override void OnMouseLeave(EventArgs e) { _hover = _pressed = false; Invalidate(); base.OnMouseLeave(e); }
    protected override void OnMouseDown(MouseEventArgs e) { _pressed = true; Invalidate(); base.OnMouseDown(e); }
    protected override void OnMouseUp(MouseEventArgs e) { _pressed = false; Invalidate(); base.OnMouseUp(e); }
    protected override void OnGotFocus(EventArgs e) { Invalidate(); base.OnGotFocus(e); }
    protected override void OnLostFocus(EventArgs e) { Invalidate(); base.OnLostFocus(e); }
    protected override void OnPaint(PaintEventArgs e) {
        e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
        var fill = Primary ? Palette.Accent : Selected ? Color.FromArgb(36, 40, 69) : Palette.Surface;
        var text = Primary ? Palette.Background : Danger ? Palette.Red : Selected ? Palette.Accent : Palette.Text;
        if (_hover && Enabled) fill = ControlPaint.Light(fill, 0.15f);
        if (_pressed && Enabled) fill = ControlPaint.Dark(fill, 0.1f);
        if (!Enabled) { fill = Palette.Input; text = Palette.Muted; }
        using var p = Palette.Round(new Rectangle(1, 1, Width - 3, Height - 3), 8);
        using var brush = new SolidBrush(fill); using var pen = new Pen(Focused || Selected ? Palette.Accent : Palette.Border);
        e.Graphics.FillPath(brush, p); e.Graphics.DrawPath(pen, p);
        TextRenderer.DrawText(e.Graphics, Text, Font, ClientRectangle, text, TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter | TextFormatFlags.EndEllipsis);
    }
}

internal sealed class BrandMark : Control {
    private readonly Image? _avatar;
    public BrandMark() {
        DoubleBuffered = true; Size = new Size(48, 48);
        using var stream = typeof(BrandMark).Assembly.GetManifestResourceStream("Capri.Avatar.jpg");
        if (stream != null) { using var source = Image.FromStream(stream); _avatar = new Bitmap(source); }
    }
    protected override void OnPaint(PaintEventArgs e) {
        e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
        e.Graphics.InterpolationMode = InterpolationMode.HighQualityBicubic;
        var bounds = new Rectangle(3, 3, Width - 7, Height - 7);
        using var circle = new GraphicsPath(); circle.AddEllipse(bounds);
        var state = e.Graphics.Save(); e.Graphics.SetClip(circle);
        if (_avatar != null) {
            float scale = Math.Max((float)bounds.Width / _avatar.Width, (float)bounds.Height / _avatar.Height);
            float width = _avatar.Width * scale, height = _avatar.Height * scale;
            e.Graphics.DrawImage(_avatar, bounds.X + (bounds.Width - width) / 2, bounds.Y + (bounds.Height - height) / 2, width, height);
        } else { using var fill = new SolidBrush(Palette.Violet); e.Graphics.FillEllipse(fill, bounds); }
        e.Graphics.Restore(state);
        using var ring = new LinearGradientBrush(ClientRectangle, Palette.Green, Palette.Violet, 45);
        using var pen = new Pen(ring, 2); e.Graphics.DrawEllipse(pen, bounds);
    }
    protected override void Dispose(bool disposing) { if (disposing) _avatar?.Dispose(); base.Dispose(disposing); }
}
