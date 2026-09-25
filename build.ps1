# Build ban C++ (MinGW-w64 g++):  .\build.ps1
#   -O2, bo RTTI, strip symbol, ma hoa chuoi luc bien dich
param([switch]$NoEmbed)

$ErrorActionPreference = 'Stop'
$PROJ = $PSScriptRoot
$BIN  = "C:\Users\tanda\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin"
$GXX  = "$BIN\g++.exe"
$RC   = "$BIN\windres.exe"
$OUT  = "$PROJ\build"
$EXE  = "ToolFixThongBaoXiaomi.exe"

if (-not (Test-Path $GXX)) { throw "Khong thay g++.exe tai $GXX" }
New-Item -ItemType Directory -Force -Path $OUT | Out-Null

Write-Host "== 1/3 Dich resource (.rc -> .o) ==" -ForegroundColor Cyan
& $RC -I "$PROJ" -i "$PROJ\resources.rc" -o "$OUT\res.o"
if ($LASTEXITCODE -ne 0) { throw "windres loi" }

Write-Host "== 2/3 Bien dich C++ ==" -ForegroundColor Cyan
$src = (Get-ChildItem "$PROJ\src\*.cpp").FullName
$def = @()
if ($NoEmbed) { $def += "-DXNF_NO_EMBED" }
& $GXX -std=c++17 -O2 -mwindows -s -static -static-libgcc -static-libstdc++ -fno-rtti -fno-exceptions `
    -fno-asynchronous-unwind-tables -Wall -Wno-unused-variable `
    -o "$OUT\$EXE" $src "$OUT\res.o" `
    -lcomctl32 -lgdi32 -luser32 -lshell32 -lole32 -lgdiplus -lstdc++ -lwinmm
if ($LASTEXITCODE -ne 0) { throw "bien dich loi" }

Write-Host "== 3/3 Ket qua ==" -ForegroundColor Cyan
Get-ChildItem "$OUT\$EXE" | Select-Object Name, @{n='MB';e={[math]::Round($_.Length/1MB,2)}}