param(
    [string]$Distro = 'Ubuntu-22.04',
    [string]$GeneratorDir = 'conformance_tmp/mm3_targeted_gen',
    [switch]$TraceFunctions
)

$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path.Replace('\', '/')
$wslRepo = "/mnt/" + $repo.Substring(0, 1).ToLower() + $repo.Substring(2)
$traceArgument = if ($TraceFunctions) {
    ' --trace-functions ../../conformance_tmp/mm3_focus_trace_functions.json'
} else {
    ''
}

$analysisCommand = "cd '$wslRepo/tools/xboxrecomp' && python3 -m tools.disasm ../../game_files/default.xbe --force -v && python3 -m tools.func_id ../../game_files/default.xbe && python3 -m tools.abi_analysis ../../game_files/default.xbe"
wsl.exe -d $Distro -- bash -lc $analysisCommand
if ($LASTEXITCODE -ne 0) {
    throw "XboxRecomp analysis pipeline failed with exit code $LASTEXITCODE"
}

wsl.exe -d $Distro -- bash -lc "cd '$wslRepo/tools/xboxrecomp' && python3 -m tools.recomp ../../game_files/default.xbe --all --split 1000 --gen-dir ../../$($GeneratorDir)$traceArgument --exclude-manual ../../src/recomp_manual.c --seh-prolog 0x00097AA4 --skip-binary-check"
if ($LASTEXITCODE -ne 0) {
    throw "XboxRecomp generation failed with exit code $LASTEXITCODE"
}

# 0x93B04 is a false vtable start inside memmove's computed-copy tail. Extend
# its recovered bounds through the shared epilogue and generate it as an FPO
# tail body so RECOMP_ITAIL can resume the active memmove frame.
$tailFunctionsPath = Join-Path $GeneratorDir 'memmove_tail_functions.json'
$tailFunctions = @(Get-Content (Join-Path $repo 'tools\xboxrecomp\tools\disasm\output\functions.json') -Raw | ConvertFrom-Json)
$tailFunction = $tailFunctions | Where-Object start -eq '0x00093B04' | Select-Object -First 1
if ($tailFunction) {
    $tailFunction.end = '0x00093B9D'
    $tailFunction.size = 153
    $tailFunction.has_prologue = $false
} else {
    $tailFunctions += [pscustomobject]@{
        start = '0x00093B04'; end = '0x00093B9D'; size = 153
        name = 'sub_00093B04'; section = '.text'; confidence = 0.75
        detection_method = 'vtable_thunk'; num_instructions = 60
        has_prologue = $false; calls_to = @(); called_by = @()
    }
}
$tailFunctions | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $tailFunctionsPath -Encoding ascii
$tailOverlayDir = Join-Path $GeneratorDir 'recovered_tail'
New-Item -ItemType Directory -Force -Path $tailOverlayDir | Out-Null
$tailErrorPath = Join-Path $tailOverlayDir '00093B04.err.log'
$tailCommand = "cd '$wslRepo/tools/xboxrecomp' && python3 -m tools.recomp ../../game_files/default.xbe --function 0x00093B04 --functions ../../$GeneratorDir/memmove_tail_functions.json --seh-prolog 0x00097AA4 --skip-binary-check"
$tailText = @(& wsl.exe -d $Distro -- bash -lc $tailCommand 2> $tailErrorPath)
if ($LASTEXITCODE -ne 0) {
    throw 'Memmove tail generation failed for 0x00093B04'
}
$tailOverlay = @(
    '#define RECOMP_GENERATED_CODE'
    '#include "recomp_funcs.h"'
    '#include <math.h>'
    ''
    ($tailText -replace 'void sub_00093B04\(void\)', 'void sub_00093B04_gen(void)')
)
Set-Content -LiteralPath (Join-Path $GeneratorDir 'recomp_00093b04_tail.c') -Value $tailOverlay -Encoding utf8

# Keep runtime semantics project-owned while leaving tools/xboxrecomp untouched.
$runtimeHeader = Join-Path $repo 'src\recomp\gen_trace\recomp_types.h'
Copy-Item -LiteralPath $runtimeHeader -Destination (Join-Path $GeneratorDir 'recomp_types.h') -Force

# Generate alternate-SEH bodies through the official generator as overlays.
# They stay separate so the normal title-wide prolog remains 97AA4.
$overlayDir = Join-Path $GeneratorDir 'alternate_seh'
New-Item -ItemType Directory -Force -Path $overlayDir | Out-Null
$alternateSeh = @('00083FBB', '00084020', '0008427E', '000842EA',
    '000854CF', '000858F3', '000860AA', '0008629E', '00096738', '00097AFC',
    '0009E85A')
foreach ($address in $alternateSeh) {
    $overlayPath = Join-Path $GeneratorDir ("recomp_{0}_seh.c" -f $address.ToLower())
    $errPath = Join-Path $overlayDir ("{0}.err.log" -f $address)
    $overlayCommand = "cd '$wslRepo/tools/xboxrecomp' && python3 -m tools.recomp ../../game_files/default.xbe --function 0x$address --seh-prolog 0x00094FC0 --skip-binary-check"
    $overlayText = @(& wsl.exe -d $Distro -- bash -lc $overlayCommand 2> $errPath)
    if ($LASTEXITCODE -ne 0) {
        throw "Alternate-SEH generation failed for 0x$address"
    }
    $overlay = @(
        '#define RECOMP_GENERATED_CODE'
        '#include "recomp_funcs.h"'
        '#include <math.h>'
        ''
        ($overlayText -replace "void sub_$address\(void\)", "void sub_${address}_gen(void)")
    )
    Set-Content -LiteralPath $overlayPath -Value $overlay -Encoding utf8
}

# Keep the official body for this CRT callback, but expose it under an alias so
# the game-specific wrapper can enforce the x86 callee-save contract.
$address = '00093C45'
$overlayPath = Join-Path $GeneratorDir ("recomp_{0}_abi.c" -f $address.ToLower())
$overlayDir = Join-Path $GeneratorDir 'abi_overlays'
New-Item -ItemType Directory -Force -Path $overlayDir | Out-Null
$errPath = Join-Path $overlayDir ("{0}.err.log" -f $address)
$overlayCommand = "cd '$wslRepo/tools/xboxrecomp' && python3 -m tools.recomp ../../game_files/default.xbe --function 0x$address --seh-prolog 0x00094FC0 --skip-binary-check"
$overlayText = @(& wsl.exe -d $Distro -- bash -lc $overlayCommand 2> $errPath)
if ($LASTEXITCODE -ne 0) {
    throw "ABI-preserving generation failed for 0x$address"
}
$overlay = @(
    '#define RECOMP_GENERATED_CODE'
    '#include "recomp_funcs.h"'
    '#include <math.h>'
    ''
    ($overlayText -replace "void sub_$address\(void\)", "void sub_${address}_gen(void)")
)
Set-Content -LiteralPath $overlayPath -Value $overlay -Encoding utf8

# The generated worker callback reads its context after a nested ret-4 helper.
# Preserve the real return sentinel while placing the context at that read slot.
$address = '001E7B8F'
$overlayPath = Join-Path $GeneratorDir ("recomp_{0}_abi.c" -f $address.ToLower())
$errPath = Join-Path $overlayDir ("{0}.err.log" -f $address)
$overlayCommand = "cd '$wslRepo/tools/xboxrecomp' && python3 -m tools.recomp ../../game_files/default.xbe --function 0x$address --seh-prolog 0x00097AA4 --skip-binary-check"
$overlayText = @(& wsl.exe -d $Distro -- bash -lc $overlayCommand 2> $errPath)
if ($LASTEXITCODE -ne 0) {
    throw "ABI-preserving generation failed for 0x$address"
}
$overlay = @(
    '#define RECOMP_GENERATED_CODE'
    '#include "recomp_funcs.h"'
    '#include <math.h>'
    ''
    ($overlayText -replace "void sub_$address\(void\)", "void sub_${address}_gen(void)")
)
Set-Content -LiteralPath $overlayPath -Value $overlay -Encoding utf8

# Keep the official body for this FPO cdecl helper under an alias so the
# project wrapper can restore the x86 callee-save contract.
$address = '0002539A'
$overlayPath = Join-Path $GeneratorDir ("recomp_{0}_abi.c" -f $address.ToLower())
$errPath = Join-Path $overlayDir ("{0}.err.log" -f $address)
$overlayCommand = "cd '$wslRepo/tools/xboxrecomp' && python3 -m tools.recomp ../../game_files/default.xbe --function 0x$address --seh-prolog 0x00097AA4 --skip-binary-check"
$overlayText = @(& wsl.exe -d $Distro -- bash -lc $overlayCommand 2> $errPath)
if ($LASTEXITCODE -ne 0) {
    throw "ABI-preserving generation failed for 0x$address"
}
$overlay = @(
    '#define RECOMP_GENERATED_CODE'
    '#include "recomp_funcs.h"'
    '#include <math.h>'
    ''
    ($overlayText -replace "void sub_$address\(void\)", "void sub_${address}_gen(void)")
)
Set-Content -LiteralPath $overlayPath -Value $overlay -Encoding utf8

# Keep the official body for this sibling FPO cdecl helper under an alias.
$address = '000943ED'
$overlayPath = Join-Path $GeneratorDir ("recomp_{0}_abi.c" -f $address.ToLower())
$errPath = Join-Path $overlayDir ("{0}.err.log" -f $address)
$overlayCommand = "cd '$wslRepo/tools/xboxrecomp' && python3 -m tools.recomp ../../game_files/default.xbe --function 0x$address --seh-prolog 0x00097AA4 --skip-binary-check"
$overlayText = @(& wsl.exe -d $Distro -- bash -lc $overlayCommand 2> $errPath)
if ($LASTEXITCODE -ne 0) {
    throw "ABI-preserving generation failed for 0x$address"
}
$overlay = @(
    '#define RECOMP_GENERATED_CODE'
    '#include "recomp_funcs.h"'
    '#include <math.h>'
    ''
    ($overlayText -replace "void sub_$address\(void\)", "void sub_${address}_gen(void)")
)
Set-Content -LiteralPath $overlayPath -Value $overlay -Encoding utf8

# Keep the official body for this FPO cdecl helper, but expose it under an
# alias so the project wrapper can enforce its callee-save/return contract.
$address = '0009418C'
$overlayPath = Join-Path $GeneratorDir ("recomp_{0}_abi.c" -f $address.ToLower())
$errPath = Join-Path $overlayDir ("{0}.err.log" -f $address)
$overlayCommand = "cd '$wslRepo/tools/xboxrecomp' && python3 -m tools.recomp ../../game_files/default.xbe --function 0x$address --seh-prolog 0x00097AA4 --skip-binary-check"
$overlayText = @(& wsl.exe -d $Distro -- bash -lc $overlayCommand 2> $errPath)
if ($LASTEXITCODE -ne 0) {
    throw "ABI-preserving generation failed for 0x$address"
}
$overlay = @(
    '#define RECOMP_GENERATED_CODE'
    '#include "recomp_funcs.h"'
    '#include <math.h>'
    ''
    ($overlayText -replace "void sub_$address\(void\)", "void sub_${address}_gen(void)")
)
Set-Content -LiteralPath $overlayPath -Value $overlay -Encoding utf8
