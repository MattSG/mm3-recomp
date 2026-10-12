<#
.SYNOPSIS
Run the recompiled Midtown Madness 3 built by scripts\setup.ps1.

.DESCRIPTION
  pwsh scripts\play.ps1 [-Keyboard] [-Fullscreen] [-Profile Balanced|Quality|LowPower] [-FpsLimit 60] [-Fov 100] [-TraceBuildings | -NoTraceBuildings] [-Exe <path>]

Any RECOMP_*/MM3_* variable already set in your shell overrides these
defaults (see README "Options"). F11 or Alt+Enter toggles fullscreen.
Every launch saves combined, timestamped output and launch metadata under logs\.
Normal launches leave focused building/texture/stream-wait diagnostics off.
Use -TraceBuildings to enable them for a pop-in investigation.
#>
param(
    [switch]$Keyboard,
    [switch]$Fullscreen,
    [ValidateSet('Balanced', 'Quality', 'LowPower')][string]$Profile = 'Balanced',
    [ValidateRange(1, 60)][int]$FpsLimit,
    [ValidateRange(60, 110)][double]$Fov,
    [switch]$TraceBuildings,
    [switch]$NoTraceBuildings,
    [string]$Exe
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (-not $Exe) {
    $Exe = @('build\RelWithDebInfo\mm3_recomp.exe', 'build\Release\mm3_recomp.exe') |
        ForEach-Object { Join-Path $repo $_ } | Where-Object { Test-Path $_ } | Select-Object -First 1
}
if (-not $Exe -or -not (Test-Path $Exe)) { throw 'No build found: run scripts\setup.ps1 first.' }
$Exe = (Resolve-Path -LiteralPath $Exe).Path
if (-not (Test-Path (Join-Path $repo 'game_files\default.xbe'))) { throw 'game_files\ missing: run scripts\setup.ps1 -Iso <disc>.' }
if ($NoTraceBuildings) { $TraceBuildings = $false }

$defaults = @{
    RECOMP_PB_EXEC = '1'; RECOMP_FB_WINDOW = '1'; RECOMP_USB = '1'
    RECOMP_APU_DSP_ACK = '0x80458810'; MM3_PERFORMANCE_PROFILE = $Profile
    RECOMP_KEYBOARD_LAYOUT = 'mm3'
    RECOMP_ASPECT = '16:10'
}
if ($PSBoundParameters.ContainsKey('FpsLimit')) { $defaults.RECOMP_FPS_LIMIT = "$FpsLimit" }
if ($PSBoundParameters.ContainsKey('Fov')) {
    $defaults.MM3_FOV_DEGREES = $Fov.ToString([Globalization.CultureInfo]::InvariantCulture)
}
if ($Keyboard) { $defaults.RECOMP_KEYBOARD = '1' }
if ($Fullscreen) { $defaults.RECOMP_FULLSCREEN = '1' }
if ($TraceBuildings) {
    $defaults.MM3_BUILDING_TRACE = '1'
    $defaults.RECOMP_TEXTURE_STREAM_TRACE = '1'
    $defaults.RECOMP_ASYNC_WAIT_TRACE = '1'
}

# Drain both native pipes concurrently. PowerShell's native stderr redirection
# differs between Windows PowerShell and pwsh; treating it as an error pipeline
# can abort a healthy game or block when one pipe fills. Each complete line is
# flushed immediately and retains its source. This never changes game buffering.
if (-not ('MM3LoggedProcess' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Diagnostics;
using System.IO;
using System.Text;

public sealed class MM3LoggedProcess : IDisposable {
    readonly object gate = new object();
    readonly Process process;
    readonly StreamWriter writer;
    string captureError;
    public int Id { get { return process.Id; } }
    public string CaptureError { get { lock (gate) { return captureError; } } }

    static string Quote(string value) {
        var result = new StringBuilder("\"");
        int slashes = 0;
        foreach (char c in value) {
            if (c == '\\') { ++slashes; continue; }
            result.Append('\\', c == '"' ? slashes * 2 + 1 : slashes);
            result.Append(c);
            slashes = 0;
        }
        result.Append('\\', slashes * 2);
        return result.Append('"').ToString();
    }

    MM3LoggedProcess(string executable, string[] arguments, string directory, string log) {
        writer = new StreamWriter(new FileStream(log, FileMode.CreateNew,
            FileAccess.Write, FileShare.ReadWrite), new UTF8Encoding(false));
        writer.AutoFlush = true;
        var command = new StringBuilder();
        foreach (string argument in arguments) {
            if (command.Length != 0) command.Append(' ');
            command.Append(Quote(argument));
        }
        process = new Process();
        process.StartInfo = new ProcessStartInfo(executable, command.ToString()) {
            WorkingDirectory = directory, UseShellExecute = false,
            RedirectStandardOutput = true, RedirectStandardError = true,
            CreateNoWindow = true
        };
        process.OutputDataReceived += (sender, e) => Write("stdout", e.Data);
        process.ErrorDataReceived += (sender, e) => Write("stderr", e.Data);
    }

    void Write(string source, string line) {
        if (line == null) return;
        lock (gate) {
            try {
                writer.WriteLine("{0:o} [{1}] {2}", DateTimeOffset.UtcNow, source, line);
                Console.WriteLine(line);
            } catch (Exception error) {
                if (captureError == null) captureError = error.ToString();
            }
        }
    }

    public static MM3LoggedProcess Start(string executable, string[] arguments, string directory, string log) {
        var session = new MM3LoggedProcess(executable, arguments, directory, log);
        try {
            session.process.Start();
            session.process.BeginOutputReadLine();
            session.process.BeginErrorReadLine();
            return session;
        } catch { session.Dispose(); throw; }
    }

    public int Wait() {
        // The parameterless wait also waits for the final asynchronous pipe events.
        process.WaitForExit();
        return process.ExitCode;
    }

    public void Dispose() {
        lock (gate) { writer.Dispose(); }
        process.Dispose();
    }
}
'@
}

function Get-LaunchFile([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $null }
    $item = Get-Item -LiteralPath $Path
    return [ordered]@{
        path = $item.FullName; size = $item.Length
        last_write_utc = $item.LastWriteTimeUtc.ToString('o')
        sha256 = (Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    }
}

function Save-LaunchMetadata {
    [IO.File]::WriteAllText($metadataPath, ($metadata | ConvertTo-Json -Depth 20),
                          [Text.UTF8Encoding]::new($false))
}

# The game reads game_files\ relative to the working directory.
$previous = @{}
$loggedProcess = $null
$launchGuard = $null
$ownsLaunchGuard = $false
$metadata = $null
$launchExitCode = 1
Push-Location $repo
try {
    if ([IO.Path]::GetFileName($Exe) -ieq 'mm3_recomp.exe') {
        # The title opens the shared Data_hd.zip cache exclusively. A second
        # game reports its sharing violation as a dirty/damaged disc. Hold a
        # launcher mutex as well, so simultaneous clicks cannot both pass.
        $launchGuard = [Threading.Mutex]::new($false, 'Local\MM3RecompSharedCacheLauncher')
        try { $ownsLaunchGuard = $launchGuard.WaitOne(0) }
        catch [Threading.AbandonedMutexException] { $ownsLaunchGuard = $true }
        $runningGames = @(Get-Process -Name mm3_recomp -ErrorAction SilentlyContinue)
        if ($runningGames.Count -gt 0) {
            $runningIds = ($runningGames | ForEach-Object { $_.Id } | Sort-Object) -join ', '
            throw "Midtown Madness 3 is already running (PID $runningIds). Close all existing game windows before launching again; they share an exclusive game cache."
        }
        if (-not $ownsLaunchGuard) {
            throw 'Another Midtown Madness 3 launcher is active. Close the existing game/launcher before starting another copy; they share an exclusive game cache.'
        }
    }
    foreach ($k in $defaults.Keys) {
        $value = [Environment]::GetEnvironmentVariable($k, 'Process')
        if (-not $value) {
            $previous[$k] = $value
            [Environment]::SetEnvironmentVariable($k, $defaults[$k], 'Process')
        }
    }
    $logDirectory = Join-Path $repo 'logs'
    New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null
    $runName = 'play-{0}-{1}' -f [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff'), [Guid]::NewGuid().ToString('N').Substring(0, 8)
    $logPath = Join-Path $logDirectory ($runName + '.log')
    $metadataPath = Join-Path $logDirectory ($runName + '.json')
    $settings = [ordered]@{}
    Get-ChildItem Env: | Where-Object { $_.Name -match '^(MM3_|RECOMP_)' } |
        Sort-Object Name | ForEach-Object { $settings[$_.Name] = $_.Value }
    # These are the runtime profile's defaults for unset settings; the complete
    # environment above records overrides, including invalid values verbatim.
    $profileDefaults = switch ($settings.MM3_PERFORMANCE_PROFILE) {
        'Quality' { @{ render_scale = '3.0'; msaa = '8'; draw_distance = '2'; aniso = '16'; fps_limit = '60' } }
        'LowPower' { @{ render_scale = '1.5'; msaa = '1'; draw_distance = '1'; aniso = '16'; fps_limit = '30' } }
        default { @{ render_scale = '2.0'; msaa = '4'; draw_distance = '2'; aniso = '16'; fps_limit = '60' } }
    }
    $configFiles = @('game_files\default.xbe', 'game_files\Data\mm3_factory_physics.ini',
        'game_files\Data\mm3_speedometers.ini', 'game_files\.mm3-car-mods\state.json',
        'tools\car-mods\cars.local.json', 'tools\car-mods\cars.json', 'build\CMakeCache.txt')
    $files = @($configFiles | ForEach-Object { Get-LaunchFile (Join-Path $repo $_) } | Where-Object { $null -ne $_ })
    $cmakeOptions = [ordered]@{}
    $cachePath = Join-Path (Split-Path $Exe -Parent) 'CMakeCache.txt'
    if (-not (Test-Path -LiteralPath $cachePath -PathType Leaf)) {
        $cachePath = Join-Path (Split-Path (Split-Path $Exe -Parent) -Parent) 'CMakeCache.txt'
    }
    if (-not (Test-Path -LiteralPath $cachePath -PathType Leaf)) { $cachePath = Join-Path $repo 'build\CMakeCache.txt' }
    if (Test-Path -LiteralPath $cachePath -PathType Leaf) {
        foreach ($line in Get-Content -LiteralPath $cachePath) {
            if ($line -match '^((?:MM3_|RECOMP_|CMAKE_(?:C_COMPILER|C_FLAGS|BUILD_TYPE|CONFIGURATION_TYPES|GENERATOR|PROJECT_VERSION|EXE_LINKER_FLAGS))[^:]*):[^=]+=(.*)$') {
                $cmakeOptions[$Matches[1]] = $Matches[2]
            }
        }
    }
    $installedState = Join-Path $repo 'game_files\.mm3-car-mods\state.json'
    $installedConfig = $null
    if (Test-Path -LiteralPath $installedState -PathType Leaf) {
        try { $installedConfig = Get-Content -LiteralPath $installedState -Raw | ConvertFrom-Json }
        catch { $installedConfig = @{ parse_error = $_.Exception.Message } }
    }
    $command = $Exe
    $commandArguments = @()
    # Keep -Exe useful for harmless launcher fixtures and local .ps1 helpers.
    if ([IO.Path]::GetExtension($Exe) -eq '.ps1') {
        $command = [Diagnostics.Process]::GetCurrentProcess().MainModule.FileName
        $commandArguments = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $Exe)
    }
    $metadata = [ordered]@{
        schema_version = 1; started_utc = [DateTime]::UtcNow.ToString('o')
        state = 'starting'; log_path = $logPath; working_directory = $repo
        executable = Get-LaunchFile $Exe
        build_configuration = (Split-Path (Split-Path $Exe -Parent) -Leaf)
        build_context_at_launch = @{ cmake_cache = Get-LaunchFile $cachePath; cmake_options = $cmakeOptions
            source_files = @(@('CMakeLists.txt', 'src\mm3_render_distance.c', 'src\mm3_widescreen.c',
                'src\mm3_performance_profile.h', 'src\mm3_building_diagnostics.c', 'src\mm3_facade_capacity.c', 'src\main.c',
                'src\recomp_manual.c', 'tools\xboxrecomp\src\kernel\kernel_bridge.c',
                'tools\xboxrecomp\src\video\nv2a_d3d11.c') | ForEach-Object { Get-LaunchFile (Join-Path $repo $_) } | Where-Object { $null -ne $_ }) }
        command = $command; arguments = $commandArguments
        launch_options = @{ profile = $Profile; keyboard = [bool]$Keyboard; fullscreen = [bool]$Fullscreen
            trace_buildings = [bool]$TraceBuildings; fps_limit = if ($PSBoundParameters.ContainsKey('FpsLimit')) { $FpsLimit } else { $null }
            fov = if ($PSBoundParameters.ContainsKey('Fov')) { $Fov } else { $null } }
        environment = $settings; profile_defaults_for_unset_settings = $profileDefaults
        configuration_files = $files; installed_car_configuration = $installedConfig
    }
    Save-LaunchMetadata
    Write-Host "Game log: $logPath"
    Write-Host "Launch details: $metadataPath"
    if ($TraceBuildings) {
        $traceValues = @('MM3_BUILDING_TRACE', 'RECOMP_TEXTURE_STREAM_TRACE', 'RECOMP_ASYNC_WAIT_TRACE') |
            ForEach-Object { '{0}={1}' -f $_, $settings[$_] }
        Write-Host "Building diagnostics: $($traceValues -join ' ')"
    }
    $loggedProcess = [MM3LoggedProcess]::Start($command, [string[]]$commandArguments, $repo, $logPath)
    $metadata.state = 'running'
    $metadata.process_id = $loggedProcess.Id
    try { Save-LaunchMetadata } catch { Write-Warning "Cannot update launch metadata: $_" }
    $launchExitCode = $loggedProcess.Wait()
    $metadata.state = 'exited'
    $metadata.exit_code = $launchExitCode
    $metadata.finished_utc = [DateTime]::UtcNow.ToString('o')
    if ($loggedProcess.CaptureError) {
        $metadata.capture_error = $loggedProcess.CaptureError
        Write-Warning "Game output could not be fully saved: $($loggedProcess.CaptureError)"
    }
    try { Save-LaunchMetadata } catch { Write-Warning "Cannot update launch metadata: $_" }
} catch {
    if ($null -ne $metadata) {
        $metadata.state = 'launch_failed'
        $metadata.launch_error = $_.Exception.Message
        $metadata.finished_utc = [DateTime]::UtcNow.ToString('o')
        try { Save-LaunchMetadata } catch { Write-Warning "Cannot update launch metadata: $_" }
    }
    throw
} finally {
    if ($null -ne $loggedProcess) { $loggedProcess.Dispose() }
    if ($null -ne $launchGuard) {
        if ($ownsLaunchGuard) { $launchGuard.ReleaseMutex() }
        $launchGuard.Dispose()
    }
    foreach ($k in $previous.Keys) {
        if ($null -eq $previous[$k]) {
            Remove-Item -LiteralPath "Env:\$k" -ErrorAction SilentlyContinue
        } else {
            [Environment]::SetEnvironmentVariable($k, $previous[$k], 'Process')
        }
    }
    Pop-Location
}
exit $launchExitCode
