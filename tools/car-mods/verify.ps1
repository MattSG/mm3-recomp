<#
.SYNOPSIS
Read the running PAL MM3 vehicle catalog and compare it with a car manifest.
.DESCRIPTION
Uses only OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION) and
ReadProcessMemory. Log must be this process's stderr log so its guest-memory
mapping is correct. A JSON result is emitted; ready is false before the catalog
loads or when an expected car/ID/rating is missing. A live player's telemetry is
included with parsed tuning values when a race has started. No save, game file,
or game memory is changed.
.EXAMPLE
./tools/car-mods/verify.ps1 -ProcessId 1234 -Log ./out/car-check/stderr.log -ExpectedManifest ./tools/car-mods/cars.json
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateRange(1, 2147483647)][int]$ProcessId,
    [Parameter(Mandatory)][string]$Log,
    [Alias('Manifest')][string]$ExpectedManifest,
    [ValidateRange(1, 100000)][int]$Samples = 1,
    [ValidateRange(10, 10000)][int]$IntervalMs = 150
)
$ErrorActionPreference = 'Stop'
$gameProcess = Get-Process -Id $ProcessId
if ($gameProcess.ProcessName -ne 'mm3_recomp') { throw 'Expected an mm3_recomp process.' }

# Read with sharing: the running game still owns and appends to this log.
$stream = [IO.File]::Open((Resolve-Path -LiteralPath $Log).Path,
    [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
$reader = [IO.StreamReader]::new($stream)
$memoryOffset = $null
try {
    for ($lineNumber = 0; $lineNumber -lt 512 -and -not $reader.EndOfStream; $lineNumber++) {
        $line = $reader.ReadLine()
        if ($line -match 'offset \+(\d+) from Xbox base') {
            $memoryOffset = [uint64]$Matches[1]
            break
        }
    }
} finally { $reader.Dispose() }
if ($null -eq $memoryOffset) { throw 'Guest memory mapping is unavailable in this log.' }

if (-not ('MM3CarCatalogReadOnly' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class MM3CarCatalogReadOnly {
    [DllImport("kernel32.dll", SetLastError=true)]
    public static extern IntPtr OpenProcess(uint access, bool inherit, int pid);
    [DllImport("kernel32.dll", SetLastError=true)]
    public static extern bool ReadProcessMemory(IntPtr process, IntPtr address,
        byte[] bytes, UIntPtr count, out UIntPtr read);
    [DllImport("kernel32.dll")]
    public static extern bool CloseHandle(IntPtr handle);
}
'@
}
$gameHandle = [MM3CarCatalogReadOnly]::OpenProcess(0x410, $false, $ProcessId)
if ($gameHandle -eq [IntPtr]::Zero) {
    throw "Cannot inspect MM3 (Win32 error $([Runtime.InteropServices.Marshal]::GetLastWin32Error()))."
}
function Read-Guest([uint32]$Address, [int]$Size = 4) {
    if ($Address -lt 0x10000 -or $Size -lt 1 -or
        ([uint64]$Address + [uint64]$Size) -gt 0x08000000) {
        throw ('Invalid guest memory range {0:X8}+{1}.' -f $Address, $Size)
    }
    $buffer = [byte[]]::new($Size)
    $readCount = [UIntPtr]::Zero
    $ok = [MM3CarCatalogReadOnly]::ReadProcessMemory($gameHandle,
        [IntPtr]([long]($memoryOffset + $Address)), $buffer, [UIntPtr]::new([uint64]$Size), [ref]$readCount)
    if (-not $ok -or $readCount.ToUInt64() -ne $Size) {
        throw ('Cannot read guest memory at {0:X8}.' -f $Address)
    }
    return ,$buffer
}
function Read-Word([uint32]$Address) { [BitConverter]::ToUInt32((Read-Guest $Address), 0) }
function Read-Float([uint32]$Address) { [BitConverter]::ToSingle((Read-Guest $Address), 0) }
function Address-Text([uint32]$Address) { '0x{0:X8}' -f $Address }

# The PAL game's MSVC string has a 16-byte buffer union at +4 and
# length/capacity at +0x14/+0x18. Capacity <16 means inline storage.
function Read-NarrowString([uint32]$Address) {
    $bytes = Read-Guest $Address 0x1C
    $length = [BitConverter]::ToUInt32($bytes, 0x14)
    $capacity = [BitConverter]::ToUInt32($bytes, 0x18)
    if ($length -gt $capacity -or $length -gt 4096) { throw 'Invalid guest narrow string.' }
    if ($length -eq 0) { return '' }
    if ($capacity -lt 16) {
        return [Text.Encoding]::ASCII.GetString($bytes, 4, [int]$length)
    }
    $chars = [BitConverter]::ToUInt32($bytes, 4)
    return [Text.Encoding]::ASCII.GetString((Read-Guest $chars ([int]$length)))
}

try {
    $entries = [Collections.Generic.List[object]]::new()
    $issues = [Collections.Generic.List[string]]::new()
    $catalog = Read-Word 0x003C5C94
    $start = 0
    $end = 0
    if ($catalog) {
        $start = Read-Word ($catalog + 8)
        $end = Read-Word ($catalog + 0xC)
        if ($start -and $end -ge $start -and (($end - $start) % 4) -eq 0 -and
            ($end - $start) -le 8192) {
            if ($end -gt $start) {
                $pointers = Read-Guest $start ($end - $start)
                for ($index = 0; $index -lt ($pointers.Length / 4); $index++) {
                    $entry = [BitConverter]::ToUInt32($pointers, $index * 4)
                    try {
                        $bytes = Read-Guest $entry 0x2A4
                        $entries.Add([pscustomobject]@{
                            index = $index
                            address = Address-Text $entry
                            id = [BitConverter]::ToUInt32($bytes, 0)
                            physics_id = [BitConverter]::ToUInt32($bytes, 4)
                            alias = Read-NarrowString ($entry + 0xC)
                            hidden = [bool]$bytes[0x28]
                            locked = [bool]$bytes[0x29]
                            class = [BitConverter]::ToUInt32($bytes, 0x30)
                            ratings = [pscustomobject]@{
                                TOPSPEED = [BitConverter]::ToInt32($bytes, 0x270)
                                ACCELERATION = [BitConverter]::ToInt32($bytes, 0x274)
                                MASS = [BitConverter]::ToInt32($bytes, 0x278)
                                HANDLING = [BitConverter]::ToInt32($bytes, 0x27C)
                                DURABILITY = [BitConverter]::ToInt32($bytes, 0x280)
                            }
                        })
                    } catch { $issues.Add("Catalog entry ${index}: $($_.Exception.Message)") }
                }
            }
        } else { $issues.Add('Catalog pointer range is not ready or is invalid.') }
    }
    $catalogReady = $entries.Count -gt 0 -and $issues.Count -eq 0
    $checks = [Collections.Generic.List[object]]::new()
    $expectedVerified = $null
    if ($ExpectedManifest) {
        $manifest = Get-Content -LiteralPath $ExpectedManifest -Raw | ConvertFrom-Json
        if (-not $manifest.cars -or @($manifest.cars).Count -eq 0) {
            throw 'Expected manifest must contain a nonempty cars array.'
        }
        foreach ($expected in $manifest.cars) {
            $matches = @($entries | Where-Object { $_.id -eq [uint32]$expected.id })
            $problems = [Collections.Generic.List[string]]::new()
            $actual = $null
            if ($matches.Count -ne 1) {
                $problems.Add("Expected one loaded car with ID $($expected.id); found $($matches.Count).")
            } else {
                $actual = $matches[0]
                if ($actual.alias -ine $expected.alias) { $problems.Add("Basename is '$($actual.alias)', expected '$($expected.alias)'.") }
                if ($actual.locked) { $problems.Add('Car is locked in this profile.') }
                foreach ($rating in @('TOPSPEED', 'ACCELERATION', 'MASS', 'HANDLING', 'DURABILITY')) {
                    $property = $expected.ratings.PSObject.Properties[$rating]
                    if ($null -eq $property) { $problems.Add("Expected manifest lacks rating $rating.") }
                    elseif ($actual.ratings.$rating -ne [int]$property.Value) {
                        $problems.Add("$rating is $($actual.ratings.$rating), expected $($property.Value).")
                    }
                }
            }
            $checks.Add([pscustomobject]@{
                id = $expected.id
                alias = $expected.alias
                expected_display_name = $expected.display_name
                verified = $problems.Count -eq 0
                problems = @($problems)
                loaded = $actual
            })
        }
        $expectedVerified = @($checks | Where-Object { -not $_.verified }).Count -eq 0
    }

    # Optional observation of a race. Null links simply mean no live player yet.
    function Read-CurrentPlayer {
      $current = [pscustomobject]@{ available = $false }
      try {
        $manager = Read-Word 0x003C5CDC
        $race = if ($manager) { Read-Word ($manager + 0x54) } else { 0 }
        $player = if ($race) { Read-Word ($race + 0xD8) } else { 0 }
        $vehicle = if ($player) { Read-Word ($player + 0x19C) } else { 0 }
        $car = if ($vehicle) { Read-Word ($vehicle + 8) } else { 0 }
        $simulation = if ($car) { Read-Word ($car + 0x3C) } else { 0 }
        $engine = if ($simulation) { Read-Word $simulation } else { 0 }
        $body = if ($simulation) { Read-Word ($simulation + 4) } else { 0 }
        if ($car -and $simulation -and $engine -and $body) {
            # Player+0x1A4 retains the CarBody parser's parameter allocation,
            # which sub_0021ED64 passes to the real physics constructor.
            $parameters = Read-Word ($player + 0x1A4)
            $alias = Read-NarrowString ($player + 0x1DC)
            $selectedEntries = @($entries | Where-Object { $_.alias -ieq $alias })
            $tuning = $null
            $physicsMatches = [Collections.Generic.List[object]]::new()
            if ($parameters) {
                $ratioStart = Read-Word ($parameters + 0x14)
                $ratioEnd = Read-Word ($parameters + 0x18)
                $gearRatios = @()
                if ($ratioStart -and $ratioEnd -ge $ratioStart -and
                    ($ratioEnd - $ratioStart) -le 64 -and (($ratioEnd - $ratioStart) % 4) -eq 0) {
                    if ($ratioEnd -gt $ratioStart) {
                        $ratioBytes = Read-Guest $ratioStart ($ratioEnd - $ratioStart)
                        $gearRatios = @(for ($ratioIndex = 0; $ratioIndex -lt ($ratioBytes.Length / 4); $ratioIndex++) {
                            [BitConverter]::ToSingle($ratioBytes, $ratioIndex * 4)
                        })
                    }
                }
                $tuning = [pscustomobject]@{
                    NGears = Read-Word ($parameters + 0xC)
                    GearRatioVector = $gearRatios
                    Mass = Read-Float ($parameters + 0x58)
                    TorqueScale = Read-Float ($parameters + 0x5C)
                    WheelDrive = Read-Word ($parameters + 0x28)
                    WheelRatio = Read-Float ($parameters + 0x60)
                    WheelRadius = Read-Float ($parameters + 0x64)
                    BrakeTorque = Read-Float ($parameters + 0xA8)
                    LatFrictionFactor = Read-Float ($parameters + 0xF4)
                    AirKZZ = Read-Float ($parameters + 0x98)
                    MaxTorqueRPM = Read-Float ($parameters + 0x9C)
                    MaxTurboTorqueRPM = Read-Float ($parameters + 0xA0)
                    MinRPMGearChange = Read-Float ($parameters + 0xA4)
                    MinRPM = Read-Float ($parameters + 0xAC)
                    MaxRPM = Read-Float ($parameters + 0xB0)
                    CutRPM = Read-Float ($parameters + 0xB4)
                    TimeChangeGear = Read-Float ($parameters + 0xC4)
                    TimeChangeGearFR = Read-Float ($parameters + 0xC8)
                    GearStatusDownGearFactor = Read-Float ($parameters + 0xD4)
                }
                if ($ExpectedManifest) {
                    foreach ($expected in $manifest.cars) {
                        $differences = [Collections.Generic.List[string]]::new()
                        $unchecked = [Collections.Generic.List[string]]::new()
                        foreach ($property in $expected.physics.PSObject.Properties) {
                            if ($null -eq $tuning.PSObject.Properties[$property.Name]) {
                                $unchecked.Add($property.Name)
                                continue
                            }
                            $observedValues = @($tuning.($property.Name))
                            $wantedValues = @($property.Value)
                            if ($observedValues.Count -ne $wantedValues.Count) {
                                $differences.Add("$($property.Name) has $($observedValues.Count) values, expected $($wantedValues.Count).")
                                continue
                            }
                            for ($valueIndex = 0; $valueIndex -lt $wantedValues.Count; $valueIndex++) {
                                $observed = [double]$observedValues[$valueIndex]
                                $wanted = [double]$wantedValues[$valueIndex]
                                $tolerance = 0.000001 * [math]::Max(1, [math]::Abs($wanted))
                                if ([double]::IsNaN($observed) -or [double]::IsInfinity($observed) -or
                                    [math]::Abs($observed - $wanted) -gt $tolerance) {
                                    $differences.Add("$($property.Name)[$valueIndex] is $observed, expected $wanted.")
                                }
                            }
                        }
                        # These are value matches, not an inferred car identity.
                        $physicsMatches.Add([pscustomobject]@{
                            id = $expected.id
                            alias = $expected.alias
                            selected = $expected.alias -ieq $alias
                            all_fields_match = $differences.Count -eq 0 -and $unchecked.Count -eq 0
                            differences = @($differences)
                            unchecked_fields = @($unchecked)
                        })
                    }
                }
            }
            # sub_001D57A6 copies these parameters into the live simulation.
            # Read the fixed fields in one block to keep sampling overhead low.
            $simulationBytes = Read-Guest $simulation 0x108
            $simulationGearCount = [BitConverter]::ToUInt32($simulationBytes, 0x50)
            $simulationRatioStart = [BitConverter]::ToUInt32($simulationBytes, 0x3C)
            $simulationShiftStart = [BitConverter]::ToUInt32($simulationBytes, 0x40)
            $simulationRatios = @()
            $simulationShiftThresholds = @()
            if ($simulationRatioStart -and $simulationGearCount -le 15) {
                $simulationRatioBytes = Read-Guest $simulationRatioStart (($simulationGearCount + 1) * 4)
                $simulationRatios = @(for ($ratioIndex = 0; $ratioIndex -le $simulationGearCount; $ratioIndex++) {
                    [BitConverter]::ToSingle($simulationRatioBytes, $ratioIndex * 4)
                })
            }
            if ($simulationShiftStart -and $simulationGearCount -gt 0 -and $simulationGearCount -le 15) {
                $simulationShiftBytes = Read-Guest $simulationShiftStart ($simulationGearCount * 4)
                $simulationShiftThresholds = @(for ($gearIndex = 0; $gearIndex -lt $simulationGearCount; $gearIndex++) {
                    [BitConverter]::ToSingle($simulationShiftBytes, $gearIndex * 4)
                })
            }
            $simulationTuning = [pscustomobject]@{
                NGears = $simulationGearCount
                GearRatioVector = $simulationRatios
                ShiftSpeedThresholdsKmh = $simulationShiftThresholds
                WheelDrive = [BitConverter]::ToUInt32($simulationBytes, 0x58)
                WheelRatio = [BitConverter]::ToSingle($simulationBytes, 0x90)
                WheelRadius = [BitConverter]::ToSingle($simulationBytes, 0x94)
                MaxTorqueRPM = [BitConverter]::ToSingle($simulationBytes, 0xCC)
                MaxTurboTorqueRPM = [BitConverter]::ToSingle($simulationBytes, 0xD0)
                MinRPMGearChange = [BitConverter]::ToSingle($simulationBytes, 0xD4)
                MinRPM = [BitConverter]::ToSingle($simulationBytes, 0xDC)
                MaxRPM = [BitConverter]::ToSingle($simulationBytes, 0xE0)
                CutRPM = [BitConverter]::ToSingle($simulationBytes, 0xE4)
                TimeChangeGear = [BitConverter]::ToSingle($simulationBytes, 0xF4)
                TimeChangeGearFR = [BitConverter]::ToSingle($simulationBytes, 0xF8)
                GearStatusDownGearFactor = [BitConverter]::ToSingle($simulationBytes, 0x104)
            }
            # The automatic shift branch uses this body velocity magnitude,
            # converted from m/s to km/h (sub_001CC5D1).
            $bodyBytes = Read-Guest $body 0xD4
            $velocityX = [BitConverter]::ToSingle($bodyBytes, 0x20)
            $velocityY = [BitConverter]::ToSingle($bodyBytes, 0x24)
            $velocityZ = [BitConverter]::ToSingle($bodyBytes, 0x28)
            $engineFlags = Read-Word ($engine + 0x10)
            $current = [pscustomobject]@{
                available = $true
                alias = $alias
                catalog_id = if ($selectedEntries.Count -eq 1) { $selectedEntries[0].id } else { $null }
                player = Address-Text $player
                car = Address-Text $car
                simulation = Address-Text $simulation
                engine = Address-Text $engine
                body = Address-Text $body
                parameters = Address-Text $parameters
                tuning = $tuning
                simulation_tuning = $simulationTuning
                simulation_wheel_drive = $simulationTuning.WheelDrive
                physics_manifest_matches = @($physicsMatches)
                speed = Read-Float ($simulation + 8)
                road_speed_kmh = [math]::Sqrt($velocityX * $velocityX +
                    $velocityY * $velocityY + $velocityZ * $velocityZ) * 3.6
                rpm = Read-Float ($simulation + 0xC)
                gear = Read-Word ($simulation + 0x2C)
                max_gear = Read-Word ($simulation + 0x50)
                transmission = Read-Word ($simulation + 0x5C)
                shift_state = $engineFlags
                shift_active = ($engineFlags -band 0x08) -ne 0
                shift_timer = Read-Float ($engine + 0x14)
                throttle = Read-Float ($simulation + 0x24)
                brake = Read-Float ($simulation + 0x28)
                requested_throttle = Read-Float ($car + 0x4C)
                requested_brake = Read-Float ($car + 0x50)
                handbrake = Read-Float ($car + 0x54)
                x = [BitConverter]::ToSingle($bodyBytes, 0xC8)
                z = [BitConverter]::ToSingle($bodyBytes, 0xD0)
            }
        }
      } catch { $current = [pscustomobject]@{ available = $false; reason = $_.Exception.Message } }
      return $current
    }
    $telemetry = [Collections.Generic.List[object]]::new()
    $watch = [Diagnostics.Stopwatch]::StartNew()
    for ($sampleIndex = 0; $sampleIndex -lt $Samples; $sampleIndex++) {
        $current = Read-CurrentPlayer
        $sample = [ordered]@{
            sample_index = $sampleIndex
            qpc_ms = [Diagnostics.Stopwatch]::GetTimestamp() * 1000.0 / [Diagnostics.Stopwatch]::Frequency
            elapsed_ms = $watch.Elapsed.TotalMilliseconds
        }
        foreach ($property in $current.PSObject.Properties) { $sample[$property.Name] = $property.Value }
        $telemetry.Add([pscustomobject]$sample)
        if ($sampleIndex + 1 -lt $Samples) { Start-Sleep -Milliseconds $IntervalMs }
    }

    [pscustomobject]@{
        process_id = $ProcessId
        guest_memory_offset = $memoryOffset
        ready = $catalogReady -and ($null -eq $expectedVerified -or $expectedVerified)
        catalog_ready = $catalogReady
        catalog_manager = Address-Text $catalog
        catalog_count = $entries.Count
        catalog_issues = @($issues)
        expected_verified = $expectedVerified
        checks = @($checks)
        catalog = @($entries)
        current_player = $current
        samples = @($telemetry)
    } | ConvertTo-Json -Depth 10
} finally { [void][MM3CarCatalogReadOnly]::CloseHandle($gameHandle) }
