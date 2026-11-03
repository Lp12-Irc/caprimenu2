// language: C# / file: ThemeControls.cs / runtime: .NET 8 / target: Windows x64 WinForms
using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Windows.Forms;
using System.Diagnostics;

internal static class Palette {
    [System.Runtime.InteropServices.DllImport("gdi32.dll",CharSet=System.Runtime.InteropServices.CharSet.Unicode)]private static extern int AddFontResourceEx(string file,uint flags,IntPtr reserved);
    static Palette(){foreach(var name in new[]{"segoeui.ttf","segoeuib.ttf","seguisb.ttf"})AddFontResourceEx(System.IO.Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Fonts),name),0x10,IntPtr.Zero);}
    public static readonly Color Background = Color.FromArgb(14, 16, 21);
    public static readonly Color Sidebar = Color.FromArgb(18, 21, 27);
    public static readonly Color Surface = Color.FromArgb(24, 28, 36);
    public static readonly Color Input = Color.FromArgb(14, 18, 25);
    public static readonly Color Border = Color.FromArgb(48, 55, 68);
    public static readonly Color Text = Color.FromArgb(240, 248, 250);
    public static readonly Color Muted = Color.FromArgb(169, 177, 190);
    public static readonly Color Accent = Color.FromArgb(248, 123, 157);
    public static readonly Color Green = Color.FromArgb(104, 212, 161);
    public static readonly Color Violet = Color.FromArgb(226, 180, 108);
    public static readonly Color Red = Color.FromArgb(255, 125, 134);
    [System.Runtime.InteropServices.DllImport("user32.dll")]private static extern bool SystemParametersInfo(uint action,uint parameter,out bool value,uint flags);
    public static bool MotionEnabled => SystemParametersInfo(0x1042,0,out bool value,0) && value && !SystemInformation.HighContrast;
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

internal sealed class AmbientPanel : Panel {
    private readonly Timer _motion=new(){Interval=40};
    private readonly Stopwatch _clock=new();
    private bool _foreground;
    private Form? _owner;
    public long Frames {get;private set;}
    public AmbientPanel(){DoubleBuffered=true;BackColor=Palette.Background;SetStyle(ControlStyles.ResizeRedraw,true);_motion.Tick+=(_,_)=>{if(!Visible || !_foreground || _owner?.WindowState==FormWindowState.Minimized){_motion.Stop();_clock.Stop();return;}Frames++;Invalidate();};}
    private void UpdateMotion(){if(IsHandleCreated && Visible && _foreground && Palette.MotionEnabled && _owner?.WindowState!=FormWindowState.Minimized){_clock.Start();_motion.Start();}else{_motion.Stop();_clock.Stop();}}
    private void OwnerResize(object? sender,EventArgs e)=>UpdateMotion();
    private void OwnerActivated(object? sender,EventArgs e){_foreground=true;UpdateMotion();}
    private void OwnerDeactivated(object? sender,EventArgs e){_foreground=false;UpdateMotion();}
    protected override void OnHandleCreated(EventArgs e){base.OnHandleCreated(e);_owner=FindForm();_foreground=_owner==Form.ActiveForm;if(_owner!=null){_owner.Resize+=OwnerResize;_owner.Activated+=OwnerActivated;_owner.Deactivate+=OwnerDeactivated;}UpdateMotion();}
    protected override void OnVisibleChanged(EventArgs e){base.OnVisibleChanged(e);UpdateMotion();}
    protected override void OnPaint(PaintEventArgs e){
        base.OnPaint(e);var g=e.Graphics;g.SmoothingMode=SmoothingMode.AntiAlias;
        float w=Width,h=Height;double t=Palette.MotionEnabled?_clock.Elapsed.TotalSeconds:0;
        for(int i=0;i<3;i++){float drift=(float)Math.Sin(t*.35f+i)*h*.07f;using var pen=new Pen(Color.FromArgb(6+i*3,Palette.Accent),1);g.DrawBezier(pen,-w*.18f,h*(.7f+i*.12f)+drift,w*.15f,h*.05f+drift,w*.78f,h*1.02f,w*1.16f,h*(.2f+i*.12f)-drift);}
        using var edge=new Pen(Color.FromArgb(35,Palette.Accent),2);float y=h*(.5f+.36f*(float)Math.Sin(t*.45f));g.DrawLine(edge,w-2,y-28,w-2,y+28);
    }
    protected override void Dispose(bool disposing){if(disposing){_motion.Dispose();_clock.Stop();if(_owner!=null){_owner.Resize-=OwnerResize;_owner.Activated-=OwnerActivated;_owner.Deactivate-=OwnerDeactivated;}}base.Dispose(disposing);}
}

internal sealed class Surface : Panel {
    private readonly Timer _hoverTimer=new(){Interval=16};private float _hoverAmount;private bool _inside;
    public int Radius { get; set; } = 8;
    public Color Fill { get; set; } = Palette.Surface;
    public Surface() { DoubleBuffered = true; SetStyle(ControlStyles.ResizeRedraw, true); BackColor = Palette.Background; Padding = new Padding(20);
        _hoverTimer.Tick+=(_,_)=>{if(!Visible || FindForm()?.WindowState==FormWindowState.Minimized){_hoverTimer.Stop();return;}float target=_inside?1:0;_hoverAmount+=(target-_hoverAmount)*.25f;if(Math.Abs(target-_hoverAmount)<.01f){_hoverAmount=target;_hoverTimer.Stop();}Invalidate();};
        MouseEnter+=(_,_)=>Hover(true);MouseLeave+=(_,_)=>Hover(false);
    }
    private void Hover(bool inside){_inside=inside;if(Palette.MotionEnabled)_hoverTimer.Start();else{_hoverAmount=inside?1:0;Invalidate();}}
    protected override void OnControlAdded(ControlEventArgs e){base.OnControlAdded(e);if(e.Control!=null){e.Control.MouseEnter+=(_,_)=>Hover(true);e.Control.MouseLeave+=(_,_)=>Hover(false);}}
    protected override void OnPaint(PaintEventArgs e) {
        base.OnPaint(e); e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
        using var p = Palette.Round(new Rectangle(0, 0, Width - 1, Height - 1), Radius);
        var border=Color.FromArgb((int)(Palette.Border.R+(Palette.Accent.R-Palette.Border.R)*_hoverAmount*.22f),(int)(Palette.Border.G+(Palette.Accent.G-Palette.Border.G)*_hoverAmount*.22f),(int)(Palette.Border.B+(Palette.Accent.B-Palette.Border.B)*_hoverAmount*.22f));
        using var b = new SolidBrush(Fill); using var pen = new Pen(border);
        e.Graphics.FillPath(b, p); e.Graphics.DrawPath(pen, p);
    }
    protected override void Dispose(bool disposing){if(disposing)_hoverTimer.Dispose();base.Dispose(disposing);}
}

internal sealed class ActionButton : Button {
    private bool _hover, _pressed;
    private readonly Timer _motion=new(){Interval=16};
    private float _amount;
    private bool _busy;
    public bool IsBusy {get=>_busy;set{_busy=value;if(Palette.MotionEnabled)_motion.Start();else _motion.Stop();Invalidate();}}
    public bool Primary { get; set; }
    public bool Danger { get; set; }
    public bool Selected { get; set; }
    public ActionButton() {
        SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw, true);
        FlatStyle = FlatStyle.Flat; FlatAppearance.BorderSize = 0;
        BackColor = Palette.Surface; ForeColor = Palette.Text;
        Font = new Font("Segoe UI", 10, FontStyle.Bold); Cursor = Cursors.Hand; Height = 40; UseVisualStyleBackColor = false;
        _motion.Tick+=(_,_)=>{float target=_hover?1:0;_amount+=(target-_amount)*.3f;if(Math.Abs(target-_amount)<.005f){_amount=target;if(!_busy)_motion.Stop();}if(Visible && FindForm()?.WindowState!=FormWindowState.Minimized)Invalidate();};
    }
    protected override void OnMouseEnter(EventArgs e) { _hover = true;if(Palette.MotionEnabled)_motion.Start();else{_amount=1;Invalidate();} base.OnMouseEnter(e); }
    protected override void OnMouseLeave(EventArgs e) { _hover = _pressed = false;if(Palette.MotionEnabled)_motion.Start();else{_amount=0;Invalidate();} base.OnMouseLeave(e); }
    protected override void OnMouseDown(MouseEventArgs e) { _pressed = true; Invalidate(); base.OnMouseDown(e); }
    protected override void OnMouseUp(MouseEventArgs e) { _pressed = false; Invalidate(); base.OnMouseUp(e); }
    protected override void OnGotFocus(EventArgs e) { Invalidate(); base.OnGotFocus(e); }
    protected override void OnLostFocus(EventArgs e) { Invalidate(); base.OnLostFocus(e); }
    protected override void OnPaint(PaintEventArgs e) {
        e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
        var fill = Primary ? Color.FromArgb(178,42,86) : Selected ? Color.FromArgb(51,33,44) : Palette.Surface;
        var text = Primary ? Color.White : Danger ? Palette.Red : Selected ? Palette.Accent : Palette.Text;
        if(Enabled){var target=Primary?Color.FromArgb(199,51,98):ControlPaint.Light(fill,.12f);fill=Color.FromArgb((int)(fill.R+(target.R-fill.R)*_amount),(int)(fill.G+(target.G-fill.G)*_amount),(int)(fill.B+(target.B-fill.B)*_amount));}
        if (_pressed && Enabled) fill = ControlPaint.Dark(fill, 0.1f);
        if (!Enabled) { fill = Palette.Input; text = Palette.Muted; }
        using var p = Palette.Round(new Rectangle(1, 1, Width - 3, Height - 3), 8);
        using var brush = new SolidBrush(fill); using var pen = new Pen(Focused || Selected ? Palette.Accent : Palette.Border);
        e.Graphics.FillPath(brush, p); e.Graphics.DrawPath(pen, p);
        TextRenderer.DrawText(e.Graphics, Text, Font, ClientRectangle, text, TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter | TextFormatFlags.EndEllipsis);
        if(_busy){int span=Math.Max(12,(Width-24)/4);float phase=Palette.MotionEnabled?(Environment.TickCount64%800)/800f:0;using var indicator=new SolidBrush(Palette.Accent);e.Graphics.FillRectangle(indicator,12+(Width-24-span)*phase,Height-5,span,2);}
    }
    protected override void Dispose(bool disposing){if(disposing)_motion.Dispose();base.Dispose(disposing);}
}

internal sealed class BrandMark : Control {
    private readonly Image? _avatar;
    public BrandMark() {
        SetStyle(ControlStyles.SupportsTransparentBackColor,true);BackColor=Color.Transparent;
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
        using var ring = new LinearGradientBrush(ClientRectangle, Palette.Accent, Color.FromArgb(255,155,189), 45);
        using var pen = new Pen(ring, 2); e.Graphics.DrawEllipse(pen, bounds);
    }
    protected override void Dispose(bool disposing) { if (disposing) _avatar?.Dispose(); base.Dispose(disposing); }
}
