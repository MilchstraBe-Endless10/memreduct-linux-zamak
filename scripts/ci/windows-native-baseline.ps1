# SPDX-License-Identifier: GPL-3.0-or-later
$ErrorActionPreference = 'Stop'

$tag = 'v.3.5.2'
$tagCommit = '2f5c55d15f69a2c88859e036b16c6f4c971e1968'
$releaseBase = 'https://github.com/henrypp/memreduct/releases/download/v.3.5.2'
$artifactDir = Join-Path $env:RUNNER_TEMP 'memreduct-windows-native'
$workDir = Join-Path $env:RUNNER_TEMP 'memreduct-windows-native-work'
New-Item -ItemType Directory -Force -Path $artifactDir, $workDir | Out-Null
$statusFile = Join-Path $artifactDir 'result.txt'
$appProcess = $null

function Write-Status([string]$Line) {
    Add-Content -LiteralPath $statusFile -Value $Line -Encoding UTF8
    Write-Host $Line
}

function Invoke-Checked([string]$Exe, [string[]]$Arguments) {
    $result = & $Exe @Arguments 2>&1
    $exitCode = $LASTEXITCODE
    if ($exitCode -ne 0) { throw "$Exe failed with exit ${exitCode}: $($result -join ' ')" }
    return $result
}

try {
    $release = Invoke-RestMethod -Headers @{ 'User-Agent' = 'memreduct-linux-zamak-p2c-ci' } `
        -Uri 'https://api.github.com/repos/henrypp/memreduct/releases/tags/v.3.5.2'
    if ($release.tag_name -ne $tag) { throw "Unexpected release tag: $($release.tag_name)" }
    Write-Status "Official release tag=$($release.tag_name) published=$($release.published_at) url=$($release.html_url)"
    $tagObject = Invoke-RestMethod -Headers @{ 'User-Agent' = 'memreduct-linux-zamak-p2c-ci' } `
        -Uri 'https://api.github.com/repos/henrypp/memreduct/git/ref/tags/v.3.5.2'
    while ($tagObject.object.type -eq 'tag') {
        $tagObject = Invoke-RestMethod -Headers @{ 'User-Agent' = 'memreduct-linux-zamak-p2c-ci' } `
            -Uri "https://api.github.com/repos/henrypp/memreduct/git/tags/$($tagObject.object.sha)"
    }
    if ($tagObject.object.sha -ne $tagCommit) { throw "The official v.3.5.2 tag resolved to unexpected commit $($tagObject.object.sha)" }
    Write-Status "PASS official source tag commit=$tagCommit"

    $checksumPath = Join-Path $workDir 'memreduct-3.5.2.sha256'
    $archivePath = Join-Path $workDir 'memreduct-3.5.2-bin.7z'
    $setupPath = Join-Path $workDir 'memreduct-3.5.2-setup.exe'
    $signaturePath = Join-Path $workDir 'memreduct-3.5.2-setup.exe.sig'
    Invoke-WebRequest -Uri "$releaseBase/memreduct-3.5.2.sha256" -OutFile $checksumPath
    Invoke-WebRequest -Uri "$releaseBase/memreduct-3.5.2-bin.7z" -OutFile $archivePath
    Invoke-WebRequest -Uri "$releaseBase/memreduct-3.5.2-setup.exe" -OutFile $setupPath
    Invoke-WebRequest -Uri "$releaseBase/memreduct-3.5.2-setup.exe.sig" -OutFile $signaturePath

    $checksumText = Get-Content -LiteralPath $checksumPath -Raw
    $expectedArchive = 'ec41ca15ff623f53850d9ee01986d3f8ff457ec93457a6ceeb993b7164023d15'
    $expectedSetup = '025e5c06f7beccf5c85cbe931bc058f53a4642ca83a00855a475026a5e6334c4'
    $expectedExe = 'dd55a81d56e8c32918e5190b2ec2a1f2a7edbbca884627439b4b619fdfb5602d'
    if ($checksumText -notmatch "$expectedArchive\s+\*memreduct-3\.5\.2-bin\.7z") { throw 'Published checksum manifest does not contain the pinned x64 archive hash.' }
    if ($checksumText -notmatch "$expectedSetup\s+\*memreduct-3\.5\.2-setup\.exe") { throw 'Published checksum manifest does not contain the pinned setup hash.' }
    $archiveHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $archivePath).Hash.ToLowerInvariant()
    $setupHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $setupPath).Hash.ToLowerInvariant()
    if ($archiveHash -ne $expectedArchive) { throw "Release archive SHA-256 mismatch: $archiveHash" }
    if ($setupHash -ne $expectedSetup) { throw "Release setup SHA-256 mismatch: $setupHash" }
    Write-Status "PASS release archive SHA-256=$archiveHash"
    Write-Status "PASS setup SHA-256=$setupHash"

    $gpg = Get-Command gpg.exe -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $gpg) { throw 'The Windows runner image must provide gpg.exe; package installation is disabled in this workflow.' }
    $gpgExe = $gpg.Source
    if (-not $gpgExe) { $gpgExe = $gpg.FullName }
    Write-Status "Using runner-provided GnuPG executable=$gpgExe"
    $cygpathExe = Join-Path (Split-Path $gpgExe -Parent) 'cygpath.exe'
    if (-not (Test-Path -LiteralPath $cygpathExe)) { throw 'Git for Windows cygpath.exe is required to pass runner paths to its GnuPG runtime.' }
    function Convert-GitPathToPosix([string]$Path) {
        $converted = & $cygpathExe -u $Path
        if ($LASTEXITCODE -ne 0) { throw "cygpath.exe could not convert path: $Path" }
        return ($converted -join '').Trim()
    }
    $gnupgHome = Join-Path $workDir 'gnupg-home'
    New-Item -ItemType Directory -Force -Path $gnupgHome | Out-Null
    $env:GNUPGHOME = Convert-GitPathToPosix $gnupgHome
    $keyPath = Join-Path $workDir 'henrypp.gpg'
    Invoke-WebRequest -Uri 'https://github.com/henrypp.gpg' -OutFile $keyPath
    $keyPathPosix = Convert-GitPathToPosix $keyPath
    $setupPathPosix = Convert-GitPathToPosix $setupPath
    $signaturePathPosix = Convert-GitPathToPosix $signaturePath
    Invoke-Checked $gpgExe @('--batch', '--import', $keyPathPosix) | Out-Null
    $verifyOutput = Invoke-Checked $gpgExe @('--batch', '--status-fd', '1', '--verify', $signaturePathPosix, $setupPathPosix)
    $validLine = $verifyOutput | Where-Object { $_ -match '^\[GNUPG:\] VALIDSIG ' } | Select-Object -First 1
    $fingerprint = if ($validLine) { ($validLine -split '\s+')[2] } else { '' }
    if ($fingerprint -ne 'D98523611524AB29BE7330AC288120A75635B5FD') { throw "Unexpected detached-signature fingerprint: $fingerprint" }
    Write-Status "PASS detached PGP signature for official setup asset; signer=$fingerprint"

    $sevenZip = Get-Command 7z.exe -ErrorAction SilentlyContinue
    if (-not $sevenZip) {
        $candidate = Join-Path $env:ProgramFiles '7-Zip\7z.exe'
        if (Test-Path -LiteralPath $candidate) { $sevenZip = Get-Item -LiteralPath $candidate }
    }
    if (-not $sevenZip) {
        Invoke-Checked 'choco.exe' @('install', '7zip.install', '-y', '--no-progress') | Out-Null
        $sevenZip = Get-Command 7z.exe -ErrorAction Stop
    }
    $sevenZipExe = $sevenZip.Source
    if (-not $sevenZipExe) { $sevenZipExe = $sevenZip.FullName }
    $extractDir = Join-Path $workDir 'portable'
    New-Item -ItemType Directory -Force -Path $extractDir | Out-Null
    Invoke-Checked $sevenZipExe @('x', '-y', "-o$extractDir", $archivePath) | Out-Null
    $candidateExe = Get-ChildItem -LiteralPath $extractDir -Filter 'memreduct.exe' -File -Recurse | Where-Object {
        (Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash.ToLowerInvariant() -eq $expectedExe
    } | Select-Object -First 1
    if (-not $candidateExe) { throw 'The x64 release executable with the published SHA-256 was not found in the official archive.' }
    $version = [System.Diagnostics.FileVersionInfo]::GetVersionInfo($candidateExe.FullName)
    if ($version.FileVersion -notmatch '^3\.5\.2(?:\.0)?$' -and $version.ProductVersion -notmatch '^3\.5\.2(?:\.0)?$') {
        throw "Unexpected executable version: file=$($version.FileVersion), product=$($version.ProductVersion)"
    }
    $authenticode = Get-AuthenticodeSignature -LiteralPath $candidateExe.FullName
    Write-Status "PASS portable x64 executable version=$($version.FileVersion) product=$($version.ProductVersion) SHA-256=$expectedExe"
    Write-Status "Executable Authenticode status=$($authenticode.Status) signer=$($authenticode.SignerCertificate.Subject)"
    Write-Status 'The release binary archive has no separate detached signature; authenticity evidence is the official release asset, pinned published SHA-256, signed setup asset, and pinned upstream source tag.'

    Add-Type -AssemblyName System.Windows.Forms
    Add-Type -AssemblyName System.Drawing
    Add-Type -AssemblyName UIAutomationClient
    Add-Type -AssemblyName UIAutomationTypes
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class P2CWin32 {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public sealed class WindowInfo {
    public IntPtr Handle; public uint ProcessId; public string Title; public string ClassName;
    public bool Visible; public int Width; public int Height; public int ControlId;
  }
  private delegate bool EnumWindowsCallback(IntPtr hWnd, IntPtr lParam);
  [DllImport("user32.dll", SetLastError=true)] public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
  [DllImport("user32.dll", SetLastError=true)] public static extern bool SetForegroundWindow(IntPtr hWnd);
  [DllImport("user32.dll", SetLastError=true)] public static extern bool PostMessage(IntPtr hWnd, uint message, IntPtr wParam, IntPtr lParam);
  [DllImport("user32.dll", SetLastError=true)] public static extern IntPtr SetFocus(IntPtr hWnd);
  [DllImport("user32.dll", SetLastError=true)] public static extern IntPtr SendMessage(IntPtr hWnd, uint message, IntPtr wParam, IntPtr lParam);
  [DllImport("user32.dll", CharSet=CharSet.Unicode, EntryPoint="SendMessageW", SetLastError=true)] private static extern IntPtr SendMessageText(IntPtr hWnd, uint message, IntPtr wParam, System.Text.StringBuilder lParam);
  [DllImport("user32.dll", SetLastError=true)] public static extern bool IsWindowVisible(IntPtr hWnd);
  [DllImport("user32.dll", SetLastError=true)] public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);
  [DllImport("user32.dll", CharSet=CharSet.Unicode, SetLastError=true)] private static extern int GetWindowText(IntPtr hWnd, System.Text.StringBuilder text, int maxCount);
  [DllImport("user32.dll", CharSet=CharSet.Unicode, SetLastError=true)] private static extern int GetClassName(IntPtr hWnd, System.Text.StringBuilder className, int maxCount);
  [DllImport("user32.dll", SetLastError=true)] private static extern bool EnumWindows(EnumWindowsCallback callback, IntPtr lParam);
  [DllImport("user32.dll", SetLastError=true)] private static extern bool EnumChildWindows(IntPtr parent, EnumWindowsCallback callback, IntPtr lParam);
  [DllImport("user32.dll", SetLastError=true)] private static extern int GetDlgCtrlID(IntPtr hWnd);
  [DllImport("user32.dll", SetLastError=true)] private static extern IntPtr GetParent(IntPtr hWnd);
  [DllImport("user32.dll", SetLastError=true)] public static extern IntPtr OpenInputDesktop(uint flags, bool inherit, uint access);
  [DllImport("user32.dll", SetLastError=true)] public static extern bool CloseDesktop(IntPtr desktop);
  [DllImport("user32.dll", SetLastError=true)] public static extern uint GetDpiForWindow(IntPtr hWnd);
  public static WindowInfo[] GetProcessWindows(uint processId) {
    var windows = new System.Collections.Generic.List<WindowInfo>();
    EnumWindows(delegate(IntPtr hWnd, IntPtr state) {
      uint owner; GetWindowThreadProcessId(hWnd, out owner);
      if (owner == processId) {
        var rect = new RECT(); GetWindowRect(hWnd, out rect);
        var title = new System.Text.StringBuilder(512); GetWindowText(hWnd, title, title.Capacity);
        var className = new System.Text.StringBuilder(256); GetClassName(hWnd, className, className.Capacity);
        windows.Add(new WindowInfo { Handle=hWnd, ProcessId=owner, Title=title.ToString(), ClassName=className.ToString(),
          Visible=IsWindowVisible(hWnd), Width=rect.Right-rect.Left, Height=rect.Bottom-rect.Top, ControlId=GetDlgCtrlID(hWnd) });
      }
      return true;
    }, IntPtr.Zero);
    return windows.ToArray();
  }
  public static WindowInfo[] GetChildWindows(IntPtr parent) {
    var windows = new System.Collections.Generic.List<WindowInfo>();
    EnumChildWindows(parent, delegate(IntPtr hWnd, IntPtr state) {
      var rect = new RECT(); GetWindowRect(hWnd, out rect);
      var title = new System.Text.StringBuilder(512); GetWindowText(hWnd, title, title.Capacity);
      var className = new System.Text.StringBuilder(256); GetClassName(hWnd, className, className.Capacity);
      windows.Add(new WindowInfo { Handle=hWnd, Title=title.ToString(), ClassName=className.ToString(), Visible=IsWindowVisible(hWnd),
        Width=rect.Right-rect.Left, Height=rect.Bottom-rect.Top, ControlId=GetDlgCtrlID(hWnd) });
      return true;
    }, IntPtr.Zero);
    return windows.ToArray();
  }
  public static string[] GetComboItems(IntPtr combo) {
    const uint CB_GETCOUNT=0x0146, CB_GETLBTEXTLEN=0x0149, CB_GETLBTEXT=0x0148;
    int count=SendMessage(combo, CB_GETCOUNT, IntPtr.Zero, IntPtr.Zero).ToInt32();
    var items=new System.Collections.Generic.List<string>();
    for (int index=0; index<count; index++) {
      int length=SendMessage(combo, CB_GETLBTEXTLEN, new IntPtr(index), IntPtr.Zero).ToInt32();
      var text=new System.Text.StringBuilder(Math.Max(length+1, 2));
      SendMessageText(combo, CB_GETLBTEXT, new IntPtr(index), text);
      items.Add(text.ToString());
    }
    return items.ToArray();
  }
  public static int GetComboSelection(IntPtr combo) {
    return SendMessage(combo, 0x0147, IntPtr.Zero, IntPtr.Zero).ToInt32();
  }
  private static void AppendTreeSiblings(IntPtr tree, IntPtr item, System.Collections.Generic.List<IntPtr> items) {
    while (item != IntPtr.Zero && items.Count < 64) {
      items.Add(item);
      IntPtr child=SendMessage(tree, 0x110A, new IntPtr(4), item);
      if (child != IntPtr.Zero) AppendTreeSiblings(tree, child, items);
      item=SendMessage(tree, 0x110A, new IntPtr(1), item);
    }
  }
  public static IntPtr[] GetTreeItems(IntPtr tree) {
    var items=new System.Collections.Generic.List<IntPtr>();
    IntPtr root=SendMessage(tree, 0x110A, IntPtr.Zero, IntPtr.Zero);
    AppendTreeSiblings(tree, root, items);
    return items.ToArray();
  }
  public static bool SelectTreeItem(IntPtr tree, IntPtr item) {
    return SendMessage(tree, 0x110B, new IntPtr(9), item) != IntPtr.Zero;
  }
  public static bool SelectComboItem(IntPtr combo, int index) {
    if (SendMessage(combo, 0x014E, new IntPtr(index), IntPtr.Zero).ToInt32() < 0) return false;
    int command=GetDlgCtrlID(combo) | (1 << 16);
    SendMessage(GetParent(combo), 0x0111, new IntPtr(command), combo);
    return GetComboSelection(combo) == index;
  }
}
'@
    $os = Get-CimInstance Win32_OperatingSystem
    $sessionId = (Get-Process -Id $PID).SessionId
    $inputDesktop = [P2CWin32]::OpenInputDesktop(0, $false, 0x0001)
    $desktopAccessible = $inputDesktop -ne [IntPtr]::Zero
    if ($desktopAccessible) { [void][P2CWin32]::CloseDesktop($inputDesktop) }
    $runnerScreen = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
    $runnerTheme = (Get-ItemProperty 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize' -ErrorAction SilentlyContinue).AppsUseLightTheme
    $scale = (Get-ItemProperty 'HKCU:\Control Panel\Desktop' -ErrorAction SilentlyContinue).LogPixels
    if (-not $scale) { $scale = 96 }
    @(
        "Windows=$($os.Caption) build=$($os.BuildNumber) version=$($os.Version)",
        "UserInteractive=$([Environment]::UserInteractive) session=$sessionId explorerCurrentSession=$([bool](Get-Process explorer -ErrorAction SilentlyContinue | Where-Object SessionId -eq $sessionId))",
        "InputDesktopAccessible=$desktopAccessible", "Resolution=$($runnerScreen.Width)x$($runnerScreen.Height)",
        "DPI=$scale scaling=$([math]::Round(100 * [double]$scale / 96))% ThemeAppsUseLightTheme=$runnerTheme",
        'Resource font=8pt Ms Shell Dlg; capture date UTC=' + [DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ')
    ) | Set-Content -LiteralPath (Join-Path $artifactDir 'windows-environment.txt') -Encoding UTF8
    Write-Status "Windows runner desktop probe: interactive=$([Environment]::UserInteractive) session=$sessionId inputDesktop=$desktopAccessible screen=$($runnerScreen.Width)x$($runnerScreen.Height) dpi=$scale"

    if (-not [Environment]::UserInteractive -or -not $desktopAccessible -or $runnerScreen.Width -lt 800) {
        Write-Status 'BLOCKED Windows Native screenshots and UI interaction: GitHub-hosted runner does not expose an accessible interactive desktop.'
    } else {
        $appProcess = Start-Process -FilePath $candidateExe.FullName -PassThru
        Start-Sleep -Seconds 4
        $appProcess.Refresh()
        $mainHwnd = $appProcess.MainWindowHandle
        if ($mainHwnd -eq [IntPtr]::Zero) {
            Write-Status 'BLOCKED Windows Native screenshots: released application did not expose a visible main-window handle in the runner session.'
        } else {
            [void][P2CWin32]::SetForegroundWindow($mainHwnd)
            Start-Sleep -Milliseconds 300
            $mainRect = [P2CWin32+RECT]::new()
            if (-not [P2CWin32]::GetWindowRect($mainHwnd, [ref]$mainRect)) { throw 'Unable to read native main-window bounds.' }
            $mainDpi = [P2CWin32]::GetDpiForWindow($mainHwnd)
            $bitmap = [System.Drawing.Bitmap]::new(($mainRect.Right - $mainRect.Left), ($mainRect.Bottom - $mainRect.Top))
            $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
            $graphics.CopyFromScreen($mainRect.Left, $mainRect.Top, 0, 0, $bitmap.Size)
            $bitmap.Save((Join-Path $artifactDir 'windows-native-main-en.png'), [System.Drawing.Imaging.ImageFormat]::Png)
            $graphics.Dispose(); $bitmap.Dispose()
            Write-Status "CAPTURED Windows Native main window geometry=$($mainRect.Right-$mainRect.Left)x$($mainRect.Bottom-$mainRect.Top) dpi=$mainDpi screenshot=windows-native-main-en.png"

            $mainHandleValue = $mainHwnd.ToInt64()
            [System.Windows.Forms.SendKeys]::SendWait('{F2}')
            Start-Sleep -Milliseconds 750
            $ownedWindows = @([P2CWin32]::GetProcessWindows([uint32]$appProcess.Id))
            $settingsWindow = $ownedWindows | Where-Object { $_.Visible -and $_.Handle.ToInt64() -ne $mainHandleValue -and $_.Width -gt 200 -and $_.Height -gt 100 } | Select-Object -First 1
            if (-not $settingsWindow) {
                Write-Status 'F2 did not create a visible settings top-level; sending the source-verified IDM_SETTINGS=155 command to the released main window.'
                [void][P2CWin32]::PostMessage($mainHwnd, 0x0111, [IntPtr]155, [IntPtr]::Zero)
                for ($attempt = 0; $attempt -lt 20 -and -not $settingsWindow; $attempt++) {
                    Start-Sleep -Milliseconds 250
                    $ownedWindows = @([P2CWin32]::GetProcessWindows([uint32]$appProcess.Id))
                    $settingsWindow = $ownedWindows | Where-Object { $_.Visible -and $_.Handle.ToInt64() -ne $mainHandleValue -and $_.Width -gt 200 -and $_.Height -gt 100 } | Select-Object -First 1
                }
            }
            foreach ($window in $ownedWindows) {
                Write-Status "Native app window hwnd=$($window.Handle) visible=$($window.Visible) size=$($window.Width)x$($window.Height) class=$($window.ClassName) title=$($window.Title)"
            }
            $desktopRoot = [System.Windows.Automation.AutomationElement]::RootElement
            if (-not $settingsWindow) {
                Write-Status 'BLOCKED Windows Native settings pages: no visible non-main application window appeared after F2 and IDM_SETTINGS=155.'
            } else {
                $settingsHwnd = $settingsWindow.Handle
                $settingsElement = [System.Windows.Automation.AutomationElement]::FromHandle($settingsHwnd)
                if (-not $settingsElement) { throw 'The settings window was created, but UI Automation could not attach to its native handle.' }
                $settingsRect = [P2CWin32+RECT]::new()
                if (-not [P2CWin32]::GetWindowRect([IntPtr]$settingsHwnd, [ref]$settingsRect)) { throw 'Unable to read native settings-window bounds.' }
                $settingsDpi = [P2CWin32]::GetDpiForWindow([IntPtr]$settingsHwnd)
                $settingsBitmap = [System.Drawing.Bitmap]::new(($settingsRect.Right - $settingsRect.Left), ($settingsRect.Bottom - $settingsRect.Top))
                $settingsGraphics = [System.Drawing.Graphics]::FromImage($settingsBitmap)
                $settingsGraphics.CopyFromScreen($settingsRect.Left, $settingsRect.Top, 0, 0, $settingsBitmap.Size)
                $settingsBitmap.Save((Join-Path $artifactDir 'windows-native-settings-initial-en.png'), [System.Drawing.Imaging.ImageFormat]::Png)
                $settingsGraphics.Dispose(); $settingsBitmap.Dispose()
                Write-Status "CAPTURED Windows Native settings initial page geometry=$($settingsRect.Right-$settingsRect.Left)x$($settingsRect.Bottom-$settingsRect.Top) dpi=$settingsDpi screenshot=windows-native-settings-initial-en.png"

                foreach ($child in @([P2CWin32]::GetChildWindows($settingsHwnd))) {
                    Write-Status "Native settings control id=$($child.ControlId) hwnd=$($child.Handle) visible=$($child.Visible) size=$($child.Width)x$($child.Height) class=$($child.ClassName) text=$($child.Title)"
                }
                try {
                    $uiaControls = $settingsElement.FindAll([System.Windows.Automation.TreeScope]::Descendants,
                        [System.Windows.Automation.Condition]::TrueCondition)
                    Write-Status "UI Automation settings descendants=$($uiaControls.Count)"
                    for ($controlIndex = 0; $controlIndex -lt [Math]::Min(60, $uiaControls.Count); $controlIndex++) {
                        $control = $uiaControls.Item($controlIndex)
                        Write-Status "UIA control type=$($control.Current.ControlType.ProgrammaticName) id=$($control.Current.AutomationId) name=$($control.Current.Name)"
                    }
                } catch { Write-Status "UIA settings tree inspection failed: $($_.Exception.Message)" }

                $controlTypes = @(
                    [System.Windows.Automation.ControlType]::ListItem,
                    [System.Windows.Automation.ControlType]::TabItem,
                    [System.Windows.Automation.ControlType]::RadioButton
                )
                $pageLabelsEnglish = @('General', 'Memory cleaning', 'Appearance', 'Tray icon', 'Advanced')
                $pageLabelsChinese = @('常规', '内存清理', '外观', '托盘图标', '高级')
                $selectPage = {
                    param($Root, [string]$Label)
                    $descendants = $Root.FindAll([System.Windows.Automation.TreeScope]::Descendants,
                        [System.Windows.Automation.Condition]::TrueCondition)
                    foreach ($control in $descendants) {
                        if ($controlTypes -contains $control.Current.ControlType -and
                            ($control.Current.Name -eq $Label -or $control.Current.Name -match [regex]::Escape($Label))) {
                            try {
                                $pattern = $control.GetCurrentPattern([System.Windows.Automation.SelectionItemPattern]::Pattern)
                                $pattern.Select()
                                return $true
                            } catch {
                                try {
                                    $pattern = $control.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern)
                                    $pattern.Invoke()
                                    return $true
                                } catch { }
                            }
                        }
                    }
                    return $false
                }.GetNewClosure()
                $captureWindow = {
                    param([IntPtr]$Handle, [string]$Path)
                    $rect = [P2CWin32+RECT]::new()
                    if (-not [P2CWin32]::GetWindowRect($Handle, [ref]$rect)) { throw 'Unable to read screenshot window bounds.' }
                    $shot = [System.Drawing.Bitmap]::new(($rect.Right - $rect.Left), ($rect.Bottom - $rect.Top))
                    $g = [System.Drawing.Graphics]::FromImage($shot)
                    $g.CopyFromScreen($rect.Left, $rect.Top, 0, 0, $shot.Size)
                    $shot.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
                    $g.Dispose(); $shot.Dispose()
                }.GetNewClosure()

                $pageSentinels = @('116,123', '124,125,131', '133,139,140', '141,148,152', '153,154')
                $settingsChildren = @([P2CWin32]::GetChildWindows($settingsHwnd))
                $navControl = $settingsChildren | Where-Object { $_.ControlId -eq 104 } | Select-Object -First 1
                $treeItems = @([P2CWin32]::GetTreeItems($navControl.Handle))
                Write-Status "Native settings navigation IDC_NAV tree items=$($treeItems.Count)"
                $selectPageByTree = {
                    param([int]$PageIndex, [string]$Language, [string]$Label)
                    $expectedIds = @($pageSentinels[$PageIndex].Split(',') | ForEach-Object { [int]$_ })
                    foreach ($treeItem in $treeItems) {
                        [void][P2CWin32]::SelectTreeItem($navControl.Handle, $treeItem)
                        Start-Sleep -Milliseconds 200
                        $currentChildren = @([P2CWin32]::GetChildWindows($settingsHwnd))
                        $visibleIds = @($currentChildren | Where-Object { $_.Visible } | ForEach-Object { $_.ControlId })
                        $missing = @($expectedIds | Where-Object { $_ -notin $visibleIds })
                        if ($missing.Count -eq 0) {
                            $shotName = "windows-native-settings-$PageIndex-$Language.png"
                            & $captureWindow $settingsHwnd (Join-Path $artifactDir $shotName)
                            $statusLine = "CAPTURED Windows Native page=$Label language=$Language dpi=$settingsDpi screenshot=$shotName visible_resource_ids=$($pageSentinels[$PageIndex])"
                            Add-Content -LiteralPath $statusFile -Value $statusLine -Encoding UTF8
                            Write-Host $statusLine
                            return $true
                        }
                    }
                    $statusLine = "BLOCKED Windows Native $Language page '$Label': IDC_NAV tree selection did not show resource IDs $($expectedIds -join ',')."
                    Add-Content -LiteralPath $statusFile -Value $statusLine -Encoding UTF8
                    Write-Host $statusLine
                    return $false
                }.GetNewClosure()

                $pageComplete = $true
                foreach ($pageIndex in 0..4) {
                    if (-not (& $selectPage $settingsElement $pageLabelsEnglish[$pageIndex])) {
                        if (-not (& $selectPageByTree $pageIndex 'English' $pageLabelsEnglish[$pageIndex])) {
                            $pageComplete = $false; break
                        }
                    } else {
                        Start-Sleep -Milliseconds 250
                        $shotName = "windows-native-settings-$pageIndex-en.png"
                        & $captureWindow ([IntPtr]$settingsHwnd) (Join-Path $artifactDir $shotName)
                        Write-Status "CAPTURED Windows Native page=$($pageLabelsEnglish[$pageIndex]) language=English dpi=$settingsDpi screenshot=$shotName"
                    }
                }

                if (-not $pageComplete) {
                    Write-Status 'BLOCKED complete Windows Native settings capture: settings navigation controls were not exposed through UI Automation.'
                } else {
                    $allControls = $settingsElement.FindAll([System.Windows.Automation.TreeScope]::Descendants,
                        [System.Windows.Automation.PropertyCondition]::new(
                            [System.Windows.Automation.AutomationElement]::ControlTypeProperty,
                            [System.Windows.Automation.ControlType]::ComboBox))
                    $languageCombo = $null
                    foreach ($control in $allControls) {
                        if ($control.Current.AutomationId -eq '123' -or $control.Current.Name -match 'language|语言') { $languageCombo = $control; break }
                    }
                    $languageSelected = $false
                    if ($languageCombo -and (& $selectPage $settingsElement 'General')) {
                        try {
                            $languageCombo.SetFocus()
                            $expand = $languageCombo.GetCurrentPattern([System.Windows.Automation.ExpandCollapsePattern]::Pattern)
                            $expand.Expand()
                            Start-Sleep -Milliseconds 250
                            $desktopElements = $desktopRoot.FindAll([System.Windows.Automation.TreeScope]::Descendants,
                                [System.Windows.Automation.Condition]::TrueCondition)
                            foreach ($item in $desktopElements) {
                                if ($item.Current.ControlType -eq [System.Windows.Automation.ControlType]::ListItem -and
                                    $item.Current.Name -match 'Chinese.*Simplified|Simplified Chinese|简体中文') {
                                    $selection = $item.GetCurrentPattern([System.Windows.Automation.SelectionItemPattern]::Pattern)
                                    $selection.Select()
                                    $languageSelected = $true
                                    break
                                }
                            }
                        } catch { Write-Status "Chinese language selection through native UI Automation failed: $($_.Exception.Message)" }
                    }
                    $nativeLanguageControl = $settingsChildren | Where-Object { $_.ControlId -eq 123 -and $_.ClassName -eq 'ComboBox' } | Select-Object -First 1
                    if (-not $languageSelected -and $nativeLanguageControl) {
                        $languageItems = @([P2CWin32]::GetComboItems($nativeLanguageControl.Handle))
                        for ($itemIndex = 0; $itemIndex -lt $languageItems.Count; $itemIndex++) {
                            Write-Status "Windows Native language item index=$itemIndex text=$($languageItems[$itemIndex])"
                        }
                        $chineseIndex = -1
                        for ($itemIndex = 0; $itemIndex -lt $languageItems.Count; $itemIndex++) {
                            if ($languageItems[$itemIndex] -match 'Chinese.*Simplified|Simplified Chinese|简体中文') { $chineseIndex = $itemIndex; break }
                        }
                        if ($chineseIndex -ge 0) {
                            [void](& $selectPageByTree 0 'English' $pageLabelsEnglish[0])
                            $languageSelected = [P2CWin32]::SelectComboItem($nativeLanguageControl.Handle, $chineseIndex)
                            Start-Sleep -Milliseconds 500
                            $selectedIndex = [P2CWin32]::GetComboSelection($nativeLanguageControl.Handle)
                            $languageSelected = $languageSelected -and $selectedIndex -eq $chineseIndex
                            Write-Status "Windows Native language selection requested_index=$chineseIndex selected_index=$selectedIndex"
                        } else {
                            Write-Status 'BLOCKED Windows Native Simplified Chinese screenshots: IDC_LANGUAGE has no Simplified Chinese choice.'
                        }
                    }
                    if (-not $languageCombo -and -not $nativeLanguageControl) {
                        Write-Status 'BLOCKED Windows Native Simplified Chinese screenshots: IDC_LANGUAGE was not exposed to UI Automation.'
                    } elseif (-not $languageSelected) {
                        Write-Status 'BLOCKED Windows Native Simplified Chinese screenshots: no Simplified Chinese option was selectable through UI Automation.'
                    } else {
                        Start-Sleep -Milliseconds 500
                        foreach ($pageIndex in 0..4) {
                            if (-not (& $selectPage $settingsElement $pageLabelsChinese[$pageIndex])) {
                                if (-not (& $selectPageByTree $pageIndex 'zh-CN' $pageLabelsChinese[$pageIndex])) { $pageComplete = $false; break }
                            } else {
                                Start-Sleep -Milliseconds 250
                                $shotName = "windows-native-settings-$pageIndex-zh-CN.png"
                                & $captureWindow ([IntPtr]$settingsHwnd) (Join-Path $artifactDir $shotName)
                                Write-Status "CAPTURED Windows Native page=$($pageLabelsChinese[$pageIndex]) language=zh-CN dpi=$settingsDpi screenshot=$shotName"
                            }
                        }
                        if ($pageComplete) {
                            [System.Windows.Forms.SendKeys]::SendWait('{ESC}')
                            Start-Sleep -Milliseconds 500
                            [void][P2CWin32]::SetForegroundWindow($mainHwnd)
                            Start-Sleep -Milliseconds 300
                            & $captureWindow $mainHwnd (Join-Path $artifactDir 'windows-native-main-zh-CN.png')
                            Write-Status 'CAPTURED Windows Native main window language=zh-CN screenshot=windows-native-main-zh-CN.png'
                        } else {
                            Write-Status 'BLOCKED complete Chinese settings screenshots: localized settings navigation labels were not exposed to UI Automation.'
                        }
                    }
                }
            }
        }
    }
} catch {
    Write-Status "FAIL Windows release verification/capture: $($_.Exception.Message)"
    throw
} finally {
    if ($appProcess -and -not $appProcess.HasExited) {
        try { $appProcess.CloseMainWindow() | Out-Null; [void]$appProcess.WaitForExit(3000) } catch { }
        if (-not $appProcess.HasExited) { Stop-Process -Id $appProcess.Id -Force -ErrorAction SilentlyContinue }
    }
    $manifest = Get-ChildItem -LiteralPath $artifactDir -File -Filter '*.png' -ErrorAction SilentlyContinue | Get-FileHash -Algorithm SHA256
    foreach ($item in $manifest) { Write-Status "Screenshot SHA-256=$($item.Hash.ToLowerInvariant()) file=$([System.IO.Path]::GetFileName($item.Path))" }
    if (-not (Test-Path -LiteralPath $statusFile)) { 'FAIL no result was recorded' | Set-Content -LiteralPath $statusFile -Encoding UTF8 }
}
