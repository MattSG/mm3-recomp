param(
    [string]$Distro = 'Ubuntu-22.04',
    [string]$GeneratorDir = 'conformance_tmp/mm3_targeted_gen'
)

$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path.Replace('\', '/')
$wslRepo = "/mnt/" + $repo.Substring(0, 1).ToLower() + $repo.Substring(2)

wsl.exe -d $Distro -- bash -lc "cd '$wslRepo/tools/xboxrecomp' && python3 -m tools.recomp ../../game_files/default.xbe --all --split 1000 --gen-dir ../../$GeneratorDir --trace-functions ../../conformance_tmp/mm3_focus_trace_functions.json --exclude-manual ../../src/recomp_manual.c --seh-prolog 0x00097AA4 --skip-binary-check"
if ($LASTEXITCODE -ne 0) {
    throw "XboxRecomp generation failed with exit code $LASTEXITCODE"
}

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
