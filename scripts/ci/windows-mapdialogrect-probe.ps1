# SPDX-License-Identifier: GPL-3.0-or-later
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Controlled', 'External')]
    [string]$Mode,
    [string]$Handle,
    [string]$FontFace = 'MS Shell Dlg',
    [int]$PointSize = 8,
    [int]$WidthDlu = 260,
    [int]$HeightDlu = 168
)

$ErrorActionPreference = 'Stop'

Add-Type -TypeDefinition @'
using System;
using System.Globalization;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;

public static class P2CMapDialogRectProbe {
  [StructLayout(LayoutKind.Sequential)]
  public struct RECT { public int Left, Top, Right, Bottom; }

  private delegate IntPtr DialogProc(IntPtr hWnd, uint message, IntPtr wParam, IntPtr lParam);

  [DllImport("user32.dll", EntryPoint="MapDialogRect", SetLastError=true)]
  private static extern bool MapDialogRect(IntPtr hDlg, ref RECT rect);
  [DllImport("user32.dll", EntryPoint="CreateDialogIndirectParamW", CharSet=CharSet.Unicode, SetLastError=true)]
  private static extern IntPtr CreateDialogIndirectParam(IntPtr instance, IntPtr template, IntPtr parent, DialogProc proc, IntPtr initParam);
  [DllImport("user32.dll", SetLastError=true)]
  private static extern bool DestroyWindow(IntPtr hWnd);
  [DllImport("user32.dll", SetLastError=true)]
  private static extern bool IsWindow(IntPtr hWnd);
  [DllImport("user32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
  private static extern int GetClassName(IntPtr hWnd, StringBuilder className, int capacity);
  [DllImport("user32.dll", SetLastError=true)]
  private static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);
  [DllImport("user32.dll", SetLastError=true)]
  private static extern IntPtr GetParent(IntPtr hWnd);
  [DllImport("user32.dll", SetLastError=true)]
  private static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
  [DllImport("user32.dll", SetLastError=true)]
  private static extern uint GetDpiForWindow(IntPtr hWnd);
  [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
  private static extern IntPtr GetModuleHandle(string moduleName);

  private static string ClassName(IntPtr hWnd) {
    var value = new StringBuilder(256);
    return GetClassName(hWnd, value, value.Capacity) > 0 ? value.ToString() : "<unavailable>";
  }

  private static byte[] CreateTemplate(string face, int pointSize, int widthDlu, int heightDlu) {
    if (String.IsNullOrWhiteSpace(face) || face.IndexOf('\0') >= 0 || pointSize < 1 || pointSize > 72)
      throw new ArgumentException("Invalid controlled dialog font metadata.");
    using (var stream = new MemoryStream())
    using (var writer = new BinaryWriter(stream, Encoding.Unicode)) {
      const uint WS_POPUP = 0x80000000;
      const uint DS_SETFONT = 0x00000040;
      writer.Write(WS_POPUP | DS_SETFONT);
      writer.Write((uint)0); // dwExtendedStyle
      writer.Write((ushort)0); // cdit
      writer.Write((short)0); writer.Write((short)0);
      writer.Write((short)widthDlu); writer.Write((short)heightDlu);
      writer.Write((ushort)0); // menu
      writer.Write((ushort)0); // default dialog class
      writer.Write((ushort)0); // title
      writer.Write((ushort)pointSize);
      writer.Write(Encoding.Unicode.GetBytes(face + "\0"));
      writer.Flush();
      return stream.ToArray();
    }
  }

  public static int RunControlled(string face, int pointSize, int widthDlu, int heightDlu) {
    byte[] bytes = CreateTemplate(face, pointSize, widthDlu, heightDlu);
    IntPtr template = Marshal.AllocHGlobal(bytes.Length);
    Marshal.Copy(bytes, 0, template, bytes.Length);
    DialogProc proc = delegate(IntPtr hWnd, uint message, IntPtr wParam, IntPtr lParam) { return IntPtr.Zero; };
    IntPtr hDlg = IntPtr.Zero;
    try {
      hDlg = CreateDialogIndirectParam(GetModuleHandle(null), template, IntPtr.Zero, proc, IntPtr.Zero);
      if (hDlg == IntPtr.Zero)
        throw new InvalidOperationException("CreateDialogIndirectParamW failed; last-error=" + Marshal.GetLastWin32Error());
      uint owner;
      GetWindowThreadProcessId(hDlg, out owner);
      if (!IsWindow(hDlg) || owner != (uint)System.Diagnostics.Process.GetCurrentProcess().Id || ClassName(hDlg) != "#32770")
        throw new InvalidOperationException("Controlled dialog did not produce a valid same-process #32770 HWND.");
      var baseUnits = new RECT { Right=4, Bottom=8 };
      if (!MapDialogRect(hDlg, ref baseUnits))
        throw new InvalidOperationException("Controlled MapDialogRect base-unit measurement failed; last-error=" + Marshal.GetLastWin32Error());
      var mapped = new RECT { Right=widthDlu, Bottom=heightDlu };
      if (!MapDialogRect(hDlg, ref mapped))
        throw new InvalidOperationException("Controlled MapDialogRect failed; last-error=" + Marshal.GetLastWin32Error());
      RECT actual;
      if (!GetWindowRect(hDlg, out actual))
        throw new InvalidOperationException("GetWindowRect for the controlled dialog failed; last-error=" + Marshal.GetLastWin32Error());
      Console.WriteLine("CONTROLLED hwnd=0x{0:X} pid={1} dpi={2} class={3} valid=true created_by=CreateDialogIndirectParamW rect_size={4} rect_right_offset={5} rect_bottom_offset={6} font={7} point_size={8} dlu={9}x{10} base_4x8_px={11}x{12} mapped_px={13}x{14} created_px={15}x{16}",
        hDlg.ToInt64(), owner, GetDpiForWindow(hDlg), ClassName(hDlg), Marshal.SizeOf(typeof(RECT)), Marshal.OffsetOf(typeof(RECT), "Right").ToInt32(),
        Marshal.OffsetOf(typeof(RECT), "Bottom").ToInt32(), face, pointSize, widthDlu, heightDlu,
        baseUnits.Right, baseUnits.Bottom, mapped.Right, mapped.Bottom, actual.Right-actual.Left, actual.Bottom-actual.Top);
      Console.Out.Flush();
      if (mapped.Right <= 0 || mapped.Bottom <= 0 || actual.Right-actual.Left != mapped.Right || actual.Bottom-actual.Top != mapped.Bottom)
        throw new InvalidOperationException("Controlled dialog pixel size did not match MapDialogRect conversion.");
      return 0;
    } finally {
      if (hDlg != IntPtr.Zero) DestroyWindow(hDlg);
      GC.KeepAlive(proc);
      Marshal.FreeHGlobal(template);
    }
  }

  public static int RunExternal(string handleText) {
    long raw = Int64.Parse(handleText, NumberStyles.Integer, CultureInfo.InvariantCulture);
    IntPtr hWnd = new IntPtr(raw);
    uint owner;
    uint thread = GetWindowThreadProcessId(hWnd, out owner);
    var rect = new RECT { Right=4, Bottom=8 };
    string className = ClassName(hWnd);
    Console.WriteLine("EXTERNAL_PRECALL caller_pid={0} hwnd=0x{1:X} hwnd_valid={2} hwnd_pid={3} hwnd_thread={4} hwnd_dpi={5} hwnd_class={6} parent=0x{7:X} rect_size={8} rect_right_offset={9} rect_bottom_offset={10} p_invoke=Winapi-BOOL-RECT-by-ref SetLastError=true",
      System.Diagnostics.Process.GetCurrentProcess().Id, hWnd.ToInt64(), IsWindow(hWnd), owner, thread, GetDpiForWindow(hWnd),
      className, GetParent(hWnd).ToInt64(), Marshal.SizeOf(typeof(RECT)), Marshal.OffsetOf(typeof(RECT), "Right").ToInt32(), Marshal.OffsetOf(typeof(RECT), "Bottom").ToInt32());
    Console.Out.Flush();
    if (!IsWindow(hWnd) || thread == 0 || owner == (uint)System.Diagnostics.Process.GetCurrentProcess().Id) {
      Console.WriteLine("EXTERNAL_RESULT invoked=false reason=invalid-or-not-foreign-window");
      return 20;
    }
    bool success = MapDialogRect(hWnd, ref rect);
    int error = Marshal.GetLastWin32Error();
    Console.WriteLine("EXTERNAL_RESULT invoked=true success={0} last_error={1} mapped_px={2}x{3}", success, error, rect.Right, rect.Bottom);
    Console.Out.Flush();
    return success ? 0 : 21;
  }
}
'@

if ($Mode -eq 'Controlled') {
    exit ([P2CMapDialogRectProbe]::RunControlled($FontFace, $PointSize, $WidthDlu, $HeightDlu))
}

if ([string]::IsNullOrWhiteSpace($Handle)) { throw 'External mode requires a numeric foreign HWND.' }
exit ([P2CMapDialogRectProbe]::RunExternal($Handle))
