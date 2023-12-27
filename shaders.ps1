
# Get the directory of the executing script
$scriptDirectory = $PSScriptRoot

$shadersDirectory = Join-Path -Path $scriptDirectory -ChildPath "meshgen/shaders"
Set-Location -Path $shadersDirectory

# Get a list of subdirectories in the current directory
$subdirectories = Get-ChildItem -Path $shadersDirectory -Directory

$VcPkgDir = "d:/source/mqnext/contrib/vcpkg/installed/x64-windows"
$BgfxShaderC = "$VcPkgDir/tools/bgfx/shaderc.exe"
$BgfxIncludeDir = "$VcPkgDir/include/bgfx"
$ExtraShaderArgs = "-i $BgfxIncludeDir --platform windows -p s_5_0 -O 3"

# Iterate through each subdirectory
foreach ($subdirectory in $subdirectories) {
    # Change the working directory
    Set-Location -Path $subdirectory.FullName

    Write-Host "Executing command in $($subdirectory.Name)"

    # Iterate through files with .sc extension
    Get-ChildItem -Path $subdirectory.FullName -Filter *.sc | ForEach-Object {
        $fileName = $_.Name
        $baseName = $_.BaseName

        # Check if the file starts with "fs_"
        if ($fileName -match '^fs_') {
            # Command to run on files starting with "fs_"
            Write-Host "Running command on $fileName (Fragment Shader)"
            # Replace the following line with your actual command
            # Example: Invoke-Expression -Command ".\YourFragmentShaderCompiler.exe $_.FullName"
            Invoke-Expression -Command "$BgfxShaderC -f $fileName --bin2c ${baseName}_dx11 --type fragment -o $baseName.bin.h $ExtraShaderArgs"
        }
        # Check if the file starts with "vs_"
        elseif ($fileName -match '^vs_') {
            # Command to run on files starting with "vs_"
            Write-Host "Running command on $fileName (Vertex Shader)"
            # Replace the following line with your actual command
            # Example: Invoke-Expression -Command ".\YourVertexShaderCompiler.exe $_.FullName"
            Invoke-Expression -Command "$BgfxShaderC -f $fileName --bin2c ${baseName}_dx11 --type vertex -o $baseName.bin.h $ExtraShaderArgs"
        }
        else {
            # Do nothing or handle other cases if needed
        }
    }
}

Set-Location -Path $scriptDirectory





# SET SHADERC_PATH=%VCPKG_DIR%/tools/bgfx/shaderc.exe
# SET BGFX_INCLUDE_PATH=%VCPKG_DIR%/include/bgfx

# SET SHADER_OPTS=-i %BGFX_INCLUDE_PATH% --platform windows -p s_5_0 -O 3
