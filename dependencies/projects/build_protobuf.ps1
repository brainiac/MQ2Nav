param(
	[Parameter(Mandatory = $true)]
	[ValidateSet('Debug', 'Release')]
	[string]$Configuration,
	[Parameter(Mandatory = $true)]
	[ValidateSet('Win32', 'x64')]
	[string]$Platform,
	[switch]$Rebuild,
	[switch]$Clean
)

$ErrorActionPreference = 'Stop'

function Resolve-CMake {
	$command = Get-Command cmake.exe -ErrorAction SilentlyContinue
	if ($command) {
		return $command.Source
	}

	$installer = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
	if (Test-Path $installer) {
		$installation = & $installer -latest -products '*' -property installationPath
		if ($LASTEXITCODE -eq 0 -and $installation) {
			$candidate = Join-Path $installation 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
			if (Test-Path $candidate) {
				return $candidate
			}
		}
	}

	throw 'CMake was not found. Install the CMake component for Visual Studio 2022.'
}

function Invoke-CMake {
	param([string[]]$Arguments)

	& $script:cmake @Arguments
	if ($LASTEXITCODE -ne 0) {
		throw "CMake failed with exit code $LASTEXITCODE."
	}
}

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$sourceRoot = Join-Path $repositoryRoot 'dependencies\protobuf'
$buildRoot = Join-Path $repositoryRoot "build\protobuf-cmake\$Platform"
$outputRoot = Join-Path $repositoryRoot "build\protobuf\$Platform"
$configurationOutput = Join-Path $outputRoot $Configuration
$protoDirectory = Join-Path $repositoryRoot 'common\proto'
$protoFile = Join-Path $protoDirectory 'NavMeshFile.proto'
$cmake = Resolve-CMake

if ($Clean) {
	if (Test-Path (Join-Path $buildRoot 'CMakeCache.txt')) {
		Invoke-CMake @('--build', $buildRoot, '--config', $Configuration, '--target', 'clean')
	}
	return
}

$architecture = if ($Platform -eq 'Win32') { 'Win32' } else { 'x64' }
$cmakeArguments = @(
	'-S', $sourceRoot,
	'-B', $buildRoot,
	'-G', 'Visual Studio 17 2022',
	'-A', $architecture,
	'-T', 'v143',
	'-Dprotobuf_BUILD_TESTS=OFF',
	'-Dprotobuf_BUILD_EXAMPLES=OFF',
	'-Dprotobuf_BUILD_CONFORMANCE=OFF',
	'-Dprotobuf_BUILD_PROTOC_BINARIES=ON',
	'-Dprotobuf_WITH_ZLIB=OFF',
	'-Dprotobuf_INSTALL=OFF',
	'-Dprotobuf_BUILD_SHARED_LIBS=OFF',
	'-Dprotobuf_MSVC_STATIC_RUNTIME=ON',
	'-Dprotobuf_DEBUG_POSTFIX=',
	"-DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG=$(Join-Path $outputRoot 'Debug')",
	"-DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE=$(Join-Path $outputRoot 'Release')",
	"-DCMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG=$(Join-Path $outputRoot 'Debug')",
	"-DCMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE=$(Join-Path $outputRoot 'Release')"
)

Invoke-CMake $cmakeArguments

if ($Rebuild) {
	Invoke-CMake @('--build', $buildRoot, '--config', $Configuration, '--target', 'clean')
}

Invoke-CMake @('--build', $buildRoot, '--config', $Configuration, '--target', 'protoc', '--parallel')

$library = Join-Path $configurationOutput 'libprotobuf.lib'
$protoc = Join-Path $configurationOutput 'protoc.exe'
if (-not (Test-Path $library)) {
	throw "CMake did not produce the expected library: $library"
}
if (-not (Test-Path $protoc)) {
	throw "CMake did not produce the expected compiler: $protoc"
}

$installedProtoc = Join-Path $sourceRoot 'bin\protoc.exe'
New-Item -ItemType Directory -Force -Path (Split-Path $installedProtoc) | Out-Null
Copy-Item -LiteralPath $protoc -Destination $installedProtoc -Force

Push-Location $protoDirectory
try {
	& $installedProtoc --cpp_out=. --proto_path=. 'NavMeshFile.proto'
	if ($LASTEXITCODE -ne 0) {
		throw "protoc failed with exit code $LASTEXITCODE."
	}
}
finally {
	Pop-Location
}
