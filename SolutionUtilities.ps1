<#

Tools for manipulating solution files at the command line

#>

param(
    [parameter(ValueFromRemainingArguments = $true)]
    [string[]]$args # Leave all argument validation to the script, not to powershell
)

function Show-Help {
    Write-Host "SolutionUtilities.ps1 [command] [arguments]"
    Write-Host
    Write-Host "Commands:"
    Write-Host
    Write-Host "  Add-Project [solution] [project]"
    Write-Host
    Write-Host "    Adds specified project file to specified solution"
    Write-Host

    Exit 1
}

function Add-Project($params) {
    if ($params.Length -lt 2) {
        Show-Help
    }

    #$dte = [System.Runtime.InteropServices.Marshal]::GetActiveObject("VisualStudio.DTE.14.0")
    $dte = New-Object -ComObject VisualStudio.DTE.14.0
    Write-Host "Open Solution: " $params[0]
    $dte.Solution.Open($params[0])
    Write-Host "Add File: " $params[1]
    $dte.Solution.AddFromFile($params[1], $false)
    $dte.Solution.Close($true)
}


if (($args.Length -lt 1) -or $h) {
    Show-Help
}

$command = $args[0]
$params = $args[1..($args.Length - 1)]

switch ($command) {
    "Add-Project" { Add-Project $params; break }
    default { Show-Help; break }
}


