param(
    [Parameter(Mandatory=$true)]
    [string]$CodePath,

    [Parameter(Mandatory=$true)]
    [ValidateSet('EUR','USA','JPN','KOR')]
    [string]$Region
)

$ErrorActionPreference = 'Stop'

# Offsets documented in the GBAtemp logo.bin thread. The checksum check below
# prevents saving a key when the code.bin revision does not match these offsets.
$offsets = @{
    EUR = 0x206465
    USA = 0x205465
    JPN = 0x206465
    KOR = 0x205465
}
$expectedSha256 = '001ECEFBF56F3614A21532391A7E303CC62A054F5C7BBBF92A97F9C62443AD15'

if (-not (Test-Path -LiteralPath $CodePath -PathType Leaf)) {
    throw "Fichier code.bin introuvable : $CodePath"
}

$codeBytes = [System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $CodePath).Path)
$offset = [int]$offsets[$Region]
$keyBytes = $null
$sha = $null

try {
    if ($codeBytes.Length -lt ($offset + 32)) {
        throw 'Le fichier code.bin est trop court pour cet emplacement.'
    }

    $keyBytes = New-Object byte[] 32
    [Array]::Copy($codeBytes, $offset, $keyBytes, 0, 32)
    $shaProvider = [System.Security.Cryptography.SHA256]::Create()
    try { $sha = $shaProvider.ComputeHash($keyBytes) }
    finally { $shaProvider.Dispose() }
    $actualSha256 = [BitConverter]::ToString($sha).Replace('-', '')

    if ($actualSha256 -ne $expectedSha256) {
        throw 'La vérification ne correspond pas. Rien n’a été enregistré. Le code.bin ou la région/version ne correspond peut-être pas au tutoriel.'
    }

    $keyFolder = Join-Path $env:USERPROFILE '.3ds'
    [System.IO.Directory]::CreateDirectory($keyFolder) | Out-Null
    $keyPath = Join-Path $keyFolder 'logo_hmackey_text'
    $keyText = [BitConverter]::ToString($keyBytes).Replace('-', '')
    [System.IO.File]::WriteAllText($keyPath, $keyText)
    Write-Output "Clé extraite et vérifiée. Fichier créé : $keyPath"
    Write-Output 'La clé elle-même n’est pas affichée.'
}
finally {
    if ($keyBytes) { [Array]::Clear($keyBytes, 0, $keyBytes.Length) }
    if ($sha) { [Array]::Clear($sha, 0, $sha.Length) }
    if ($codeBytes) { [Array]::Clear($codeBytes, 0, $codeBytes.Length) }
}
