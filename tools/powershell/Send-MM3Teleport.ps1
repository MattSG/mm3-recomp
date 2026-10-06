[CmdletBinding(DefaultParameterSetName = 'Position')]
param(
    [Parameter(Mandatory)][int]$ProcessId,
    [Parameter(Mandatory, ParameterSetName = 'Teleport')][float]$X,
    [Parameter(Mandatory, ParameterSetName = 'Teleport')][float]$Z
)

$ErrorActionPreference = 'Stop'
$command = 'position'
if ($PSCmdlet.ParameterSetName -eq 'Teleport') {
    foreach ($coordinate in @($X, $Z)) {
        if ([float]::IsNaN($coordinate) -or [float]::IsInfinity($coordinate)) {
            throw 'Coordinates must be finite.'
        }
    }
    $command = [string]::Format([cultureinfo]::InvariantCulture, 'teleport {0:R} {1:R}', $X, $Z)
}
$pipe = [IO.Pipes.NamedPipeClientStream]::new('.', "MM3Teleport-$ProcessId", [IO.Pipes.PipeDirection]::InOut)
try {
    $pipe.Connect(3000)
    $bytes = [Text.Encoding]::ASCII.GetBytes($command)
    $pipe.Write($bytes, 0, $bytes.Length)
    $buffer = [byte[]]::new(512)
    $read = $pipe.ReadAsync($buffer, 0, $buffer.Length)
    if (-not $read.Wait(5000)) { throw 'MM3 debug response timed out.' }
    $response = [Text.Encoding]::ASCII.GetString($buffer, 0, $read.Result) | ConvertFrom-Json
    if ($response.error) { throw $response.error }
    if ($PSCmdlet.ParameterSetName -eq 'Teleport' -and $response.result -ne 'applied') {
        throw "Teleport failed: $($response.result)"
    }
    $response
} finally {
    $pipe.Dispose()
}
