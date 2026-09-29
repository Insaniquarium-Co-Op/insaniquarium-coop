# Builds Insaniquarium Co-op on Windows from this source folder (run Build.bat).
#
# Everything it needs is downloaded the first time, as portable tools from their
# official GitHub releases, each checked against a pinned SHA-256:
#   llvm-mingw (compiler), CMake, Ninja, MinGit (for the game's source).
# They live in %LOCALAPPDATA%\InsaniquariumCoop\BuildTools: nothing is installed on the
# system and no admin rights are needed. Then it fetches the game's source and framework
# (see scripts\upstream.lock), builds the libraries and the game, and puts the result in the
# "Built Game" folder next to Build.bat. Later runs only redo what changed.
#
#   powershell -ExecutionPolicy Bypass -File scripts\windows\build.ps1 [-Clean]
param([switch]$Clean)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$Root = (Resolve-Path "$PSScriptRoot\..\..").Path
$Tools = Join-Path $env:LOCALAPPDATA 'InsaniquariumCoop\BuildTools'
$Deps = Join-Path $Root 'build-deps-win'
$Build = Join-Path $Root 'build-win'
$Out = Join-Path $Root 'Built Game'
$Version = (Get-Content (Join-Path $Root 'COOP_VERSION') -Raw).Trim()

function Say($m) { Write-Host "== $m" -ForegroundColor Cyan }
function Fail($m) { Write-Host "`nBuild failed: $m" -ForegroundColor Red; exit 1 }
function Run($exe, [string[]]$argv, $log) {
	# Runs a program; its output goes to $log (if given), and a failure stops the build.
	# (Windows PowerShell 5.1 treats a program's stderr as an error under 'Stop'.)
	$old = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
	if ($log) { & $exe @argv *> $log } else { & $exe @argv }
	$ErrorActionPreference = $old
	if ($LASTEXITCODE -ne 0) {
		if ($log) { Get-Content $log -Tail 25 | Write-Host }
		Fail "$exe $($argv -join ' ') (exit $LASTEXITCODE)$(if ($log) { ", see $log" })"
	}
}
function Download($url, $file) {
	if (Test-Path $file) { return }
	Write-Host "   downloading $(Split-Path $url -Leaf)"
	& curl.exe -fsSL --retry 5 --retry-delay 3 -o "$file.part" $url
	if ($LASTEXITCODE -ne 0) { Fail "couldn't download $url" }
	Move-Item "$file.part" $file
}

# --- Tools ---------------------------------------------------------------------------
$ToolList = @(
	@{ Name = 'llvm-mingw'; Url = 'https://github.com/mstorsjo/llvm-mingw/releases/download/20260922/llvm-mingw-20260922-ucrt-x86_64.zip'
	   Sha = 'e3ad77d117a4bea19a7a3b333341824d79a5a371004a10e25b8504e7b3047666'; Bin = 'llvm-mingw-20260922-ucrt-x86_64\bin' },
	@{ Name = 'cmake'; Url = 'https://github.com/Kitware/CMake/releases/download/v4.4.3/cmake-4.4.3-windows-x86_64.zip'
	   Sha = '4d52ebab7193a698651639ed80d8d04fd903358843572cf44c7fd234cb7c26ab'; Bin = 'cmake-4.4.3-windows-x86_64\bin' },
	@{ Name = 'ninja'; Url = 'https://github.com/ninja-build/ninja/releases/download/v1.13.2/ninja-win.zip'
	   Sha = '07fc8261b42b20e71d1720b39068c2e14ffcee6396b76fb7a795fb460b78dc65'; Bin = '.' },
	@{ Name = 'mingit'; Url = 'https://github.com/git-for-windows/git/releases/download/v2.56.0.windows.1/MinGit-2.56.0-64-bit.zip'
	   Sha = '064b440ff870ed5198527e8f3a92cdf5bd2fd0fedf5e718af95e3fdaddeff718'; Bin = 'cmd' }
)
Say "Build tools ($Tools)"
New-Item -ItemType Directory -Force -Path $Tools | Out-Null
foreach ($t in $ToolList) {
	$dir = Join-Path $Tools "$($t.Name)-$($t.Sha.Substring(0, 12))"
	if (-not (Test-Path (Join-Path $dir '.ok'))) {
		$zip = "$dir.zip"
		Download $t.Url $zip
		$hash = (Get-FileHash $zip -Algorithm SHA256).Hash.ToLower()
		if ($hash -ne $t.Sha) { Remove-Item $zip; Fail "$($t.Name) download doesn't match its pinned SHA-256 (got $hash)" }
		if (Test-Path $dir) { Remove-Item -Recurse -Force $dir }
		Write-Host "   unpacking $($t.Name)"
		Expand-Archive $zip -DestinationPath $dir
		Remove-Item $zip
		Set-Content (Join-Path $dir '.ok') $t.Sha
	}
	$env:PATH = (Join-Path $dir $t.Bin) + ';' + $env:PATH
}
if ($Clean) { Remove-Item -Recurse -Force $Deps, $Build -ErrorAction SilentlyContinue }

# --- The game's source (WinFish) and framework (PvZ-Portable), fetched and patched ------
Say 'Game source'
$lock = @{}
Get-Content (Join-Path $Root 'scripts\upstream.lock') | Where-Object { $_ -match '^\s*(\w+)=(.*)$' } | ForEach-Object { $lock[$Matches[1]] = $Matches[2].Trim() }
$up = Join-Path $Root 'build-upstream\insaniquarium-mac'
function Get-Upstream {
	$have = ''
	if (Test-Path (Join-Path $up '.git')) { $ErrorActionPreference = 'Continue'; $have = (& git -C $up rev-parse HEAD 2>$null); $ErrorActionPreference = 'Stop' }
	if ($have -eq $lock.UPSTREAM_COMMIT) { return }
	Write-Host "   downloading $($lock.UPSTREAM_REPO) at $($lock.UPSTREAM_COMMIT.Substring(0, 7))"
	if (Test-Path $up) { Remove-Item -Recurse -Force $up }
	New-Item -ItemType Directory -Force -Path $up | Out-Null
	Run git @('-C', $up, 'init', '-q')
	Run git @('-C', $up, 'config', 'core.autocrlf', 'false')	# the patches expect the files as they are upstream
	Run git @('-C', $up, 'fetch', '-q', '--depth', '1', $lock.UPSTREAM_REPO, $lock.UPSTREAM_COMMIT)
	Run git @('-C', $up, '-c', 'advice.detachedHead=false', 'checkout', '-q', 'FETCH_HEAD')
	# git checks the commit's content hash, so a changed upstream can't slip in.
	if ((& git -C $up rev-parse HEAD) -ne $lock.UPSTREAM_COMMIT) { Fail 'unexpected upstream commit' }
}
function Get-Patched($name, $patchName) {
	$dir = Join-Path $Root $name
	$patch = Join-Path $Root "patches\$patchName"
	$marker = Join-Path $dir '.fetched'
	$stamp = "$($lock.UPSTREAM_COMMIT) $((Get-FileHash $patch -Algorithm SHA256).Hash.ToLower())"
	if ((Test-Path $dir) -and -not (Test-Path $marker)) { Write-Host "   $name\: developer copy, left as it is"; return }
	if ((Test-Path $marker) -and ((Get-Content $marker -Raw).Trim() -eq $stamp)) { Write-Host "   $name\: up to date"; return }
	Get-Upstream
	$new = "$dir.new"
	if (Test-Path $new) { Remove-Item -Recurse -Force $new }
	Copy-Item -Recurse (Join-Path $up $name) $new
	Push-Location $Root
	try { Run git @('apply', '--whitespace=nowarn', "--directory=$name.new", '-p2', $patch) } finally { Pop-Location }
	Remove-Item -Force (Join-Path $new 'source\WinFish\Insaniquarium.ico') -ErrorAction SilentlyContinue	# PopCap's icon; the build uses ours
	if (Test-Path $dir) { Remove-Item -Recurse -Force $dir }
	Move-Item $new $dir
	Set-Content $marker $stamp
	Write-Host "   $name\: ready (patched with patches\$patchName)"
}
Get-Patched 'WinFish' 'winfish.patch'
Get-Patched 'PvZ-Portable' 'pvz-portable.patch'

# --- Libraries -----------------------------------------------------------------------
Say "Libraries ($Deps)"
$Src = Join-Path $Deps 'src'
$Prefix = Join-Path $Deps 'win64'
New-Item -ItemType Directory -Force -Path $Src, $Prefix | Out-Null
$P = $Prefix -replace '\\', '/'
$Common = @('-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release', "-DCMAKE_INSTALL_PREFIX=$P", "-DCMAKE_PREFIX_PATH=$P",
	'-DCMAKE_C_COMPILER=clang', '-DCMAKE_CXX_COMPILER=clang++', '-DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres', '-DCMAKE_POSITION_INDEPENDENT_CODE=ON', '-DCMAKE_POLICY_VERSION_MINIMUM=3.5')
function Tarball($name, $url) {
	$dir = Join-Path $Src $name
	if (Test-Path $dir) { return $dir }
	$file = Join-Path $Src "$name.tar.gz"
	Download $url $file
	$tmp = Join-Path $Src "$name.tmp"
	New-Item -ItemType Directory -Force -Path $tmp | Out-Null
	Run tar @('-xzf', $file, '-C', $tmp)
	Move-Item (Get-ChildItem $tmp | Select-Object -First 1).FullName $dir
	Remove-Item -Recurse -Force $tmp, $file
	return $dir
}
function GitTag($name, $url, $tag) {
	$dir = Join-Path $Src $name
	if (-not (Test-Path $dir)) { Run git @('-c', 'core.autocrlf=false', 'clone', '-q', '--depth', '1', '--branch', $tag, $url, $dir) }
	return $dir
}
function Lib($name, $src, [string[]]$opts) {
	$done = Join-Path $Prefix ".built-$name"
	$key = "$src $($opts -join ' ')"
	if ((Test-Path $done) -and ((Get-Content $done -Raw).Trim() -eq $key)) { Write-Host "   $name`: already built"; return }
	$b = Join-Path $Deps "build\$name"
	if (Test-Path $b) { Remove-Item -Recurse -Force $b }
	$log = Join-Path $Deps "build\$name.log"
	New-Item -ItemType Directory -Force -Path (Split-Path $log) | Out-Null
	Write-Host "   building $name"
	Run cmake (@('-S', $src, '-B', $b) + $Common + $opts) $log
	Run ninja @('-C', $b, 'install') "$log.install"
	Set-Content $done $key
}
Lib zlib (Tarball 'zlib' 'https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.gz') @('-DZLIB_BUILD_EXAMPLES=OFF')
Lib png (GitTag 'libpng' 'https://github.com/pnggroup/libpng.git' 'v1.6.43') @('-DPNG_SHARED=OFF', '-DPNG_TESTS=OFF', '-DPNG_TOOLS=OFF', '-DPNG_EXECUTABLES=OFF', "-DZLIB_ROOT=$P")
Lib jpeg (Tarball 'jpeg' 'https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/3.0.4/libjpeg-turbo-3.0.4.tar.gz') @('-DENABLE_SHARED=OFF', '-DWITH_TURBOJPEG=OFF', '-DREQUIRE_SIMD=OFF', '-DWITH_SIMD=OFF')
Lib ogg (Tarball 'ogg' 'https://github.com/xiph/ogg/releases/download/v1.3.5/libogg-1.3.5.tar.gz') @('-DBUILD_SHARED_LIBS=OFF', '-DINSTALL_DOCS=OFF')
Lib vorbis (Tarball 'vorbis' 'https://github.com/xiph/vorbis/releases/download/v1.3.7/libvorbis-1.3.7.tar.gz') @('-DBUILD_SHARED_LIBS=OFF')
Lib miniupnpc (Join-Path (GitTag 'miniupnp' 'https://github.com/miniupnp/miniupnp.git' 'miniupnpc_2_2_8') 'miniupnpc') @('-DUPNPC_BUILD_SHARED=OFF', '-DUPNPC_BUILD_TESTS=OFF', '-DUPNPC_BUILD_SAMPLE=OFF')
Lib sdl2 (Tarball 'SDL2' 'https://github.com/libsdl-org/SDL/releases/download/release-2.30.9/SDL2-2.30.9.tar.gz') @('-DSDL_SHARED=ON', '-DSDL_STATIC=OFF', '-DSDL_TEST=OFF')
$ompt = (GitTag 'openmpt' 'https://github.com/OpenMPT/openmpt.git' 'libopenmpt-0.7.10') -replace '\\', '/'
Lib openmpt (Join-Path $Root 'scripts\deps\libopenmpt') @("-DOPENMPT_SRC=$ompt")

# --- The game ------------------------------------------------------------------------
Say 'Insaniquarium Co-op'
Run cmake (@('-S', $Root, '-B', $Build) + $Common + @(
	"-DZLIB_LIBRARY=$P/lib/libzlibstatic.a", "-DZLIB_INCLUDE_DIR=$P/include",
	"-DPNG_LIBRARY=$P/lib/libpng16.a", "-DPNG_PNG_INCLUDE_DIR=$P/include",
	"-DOPENMPT_LIBRARY=$P/lib/libopenmpt.a")) (Join-Path $Root 'build-win-configure.log')
Run ninja @('-C', $Build)

# --- The playable folder -------------------------------------------------------------
if (Test-Path $Out) { Remove-Item -Recurse -Force $Out }
New-Item -ItemType Directory -Force -Path $Out | Out-Null
Run llvm-strip @((Join-Path $Build 'InsaniquariumCoop.exe'), '-o', (Join-Path $Out 'InsaniquariumCoop.exe'))
Copy-Item (Join-Path $Prefix 'bin\SDL2.dll') $Out
Copy-Item (Join-Path $Root 'platform\windows\dist\*') $Out
Copy-Item (Join-Path $Root 'THIRD_PARTY_NOTICES.txt') $Out
Copy-Item (Join-Path $Root 'LICENSE') (Join-Path $Out 'LICENSE.txt')
Write-Host ''
Write-Host "Done! Insaniquarium Co-op $Version is in:" -ForegroundColor Green
Write-Host "   $Out"
Write-Host 'Double-click InsaniquariumCoop.exe there to play. Each player needs their own copy of Insaniquarium Deluxe.'
