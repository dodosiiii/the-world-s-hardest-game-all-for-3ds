param(
    [Parameter(Mandatory=$true)]
    [string]$KeyPath
)

$ErrorActionPreference = 'Stop'
$hubRoot = Split-Path -Parent $PSScriptRoot
$meta = Join-Path $hubRoot 'meta'
$darcPath = Join-Path $meta 'launch-logo-unhashed.bin'
$signedPath = Join-Path $meta 'launch-logo-signed.bin'
$compressedPath = Join-Path $meta 'custom-logo.lz11'
$outputPath = Join-Path $meta 'custom-logo-padded.lz11'
$bannertool = Join-Path $PSScriptRoot 'bin\bannertool.exe'
$keyBytes = $null
$hmac = $null

try {
    if (-not (Test-Path -LiteralPath $KeyPath -PathType Leaf)) {
        throw "Key file not found: $KeyPath"
    }
    $keyText = [System.IO.File]::ReadAllText((Resolve-Path -LiteralPath $KeyPath).Path) -replace '\s',''
    if ($keyText -notmatch '^[0-9A-Fa-f]{64}$') {
        throw 'Expected a local file containing exactly 64 hexadecimal characters.'
    }
    if (-not (Test-Path -LiteralPath $darcPath -PathType Leaf)) {
        throw "Prepared logo archive not found: $darcPath"
    }
    if (-not (Test-Path -LiteralPath $bannertool -PathType Leaf)) {
        throw "Logo compression tool not found: $bannertool. Install bannertool or restore it in tools/bin."
    }

    $keyBytes = New-Object byte[] 32
    for ($i = 0; $i -lt 32; $i++) {
        $keyBytes[$i] = [Convert]::ToByte($keyText.Substring($i * 2, 2), 16)
    }
    $darcBytes = [System.IO.File]::ReadAllBytes($darcPath)
    $hmac = [System.Security.Cryptography.HMACSHA256]::new($keyBytes)
    $signature = $hmac.ComputeHash($darcBytes)
    $signedBytes = New-Object byte[] ($darcBytes.Length + $signature.Length)
    [Array]::Copy($darcBytes, 0, $signedBytes, 0, $darcBytes.Length)
    [Array]::Copy($signature, 0, $signedBytes, $darcBytes.Length, $signature.Length)
    [System.IO.File]::WriteAllBytes($signedPath, $signedBytes)

    & $bannertool lz11 -i $signedPath -o $compressedPath
    if ($LASTEXITCODE -ne 0) { throw 'The logo compression step failed.' }
    $compressedBytes = [System.IO.File]::ReadAllBytes($compressedPath)
    if ($compressedBytes.Length -gt 8192) {
        throw "Compressed logo is $($compressedBytes.Length) bytes; the maximum is 8192. Reduce the artwork before packaging."
    }

    $paddedBytes = New-Object byte[] 8192
    [Array]::Copy($compressedBytes, 0, $paddedBytes, 0, $compressedBytes.Length)
    [System.IO.File]::WriteAllBytes($outputPath, $paddedBytes)
    Write-Output "Created $outputPath ($($compressedBytes.Length) compressed bytes, padded to 8192)."
    Write-Output 'The key was read only from the local file and was not written into the project.'
}
finally {
    if ($hmac) { $hmac.Dispose() }
    if ($keyBytes) { [Array]::Clear($keyBytes, 0, $keyBytes.Length) }
}
