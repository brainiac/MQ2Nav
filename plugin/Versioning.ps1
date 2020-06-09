$CPPFile = Get-ChildItem -Recurse PluginMain.cpp
$ResFile = Get-ChildItem -Recurse Version.rc
$VersionMatch = Select-String -Path $CPPFile -Pattern 'MQ2NAV_PLUGIN_VERSION\s\"(.*)\"'
$Version = $VersionMatch.Matches.Groups[1].Value
If ($Version) {
    $VersionCommas = $Version -replace '\.', ','
    (Get-Content $ResFile) | ForEach-Object {
        $_ -replace 'FILEVERSION (.*)', "FILEVERSION $VersionCommas" `
           -replace 'PRODUCTVERSION (.*)', "PRODUCTVERSION $VersionCommas" `
           -replace 'VALUE "FileVersion", (.*)', "VALUE `"FileVersion`", `"$Version`"" `
           -replace 'VALUE "ProductVersion", (.*)', "VALUE `"ProductVersion`", `"$Version`""
    } | Set-Content $ResFile
}
