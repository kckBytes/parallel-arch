# launch_sweep.ps1 -- start the Ocean scaling sweep detached inside WSL Ubuntu.
# Progress: ocean_omp\_build\sweep_stdout.txt   Result: ocean_omp\_build\summary.csv
param([int]$Reps = 5, [string]$Threads = "1 2 4 8 12 16 24")
# Derive the WSL path of this script's ocean_omp folder from its own location.
$winDir = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$wslDir = (wsl.exe -d Ubuntu -- wslpath -a ($winDir -replace "\\", "/")).Trim()
$cmd = "cd $wslDir; bash scripts/run_scaling.sh $Reps '$Threads' > _build/sweep_stdout.txt 2>&1"
Start-Process -FilePath "wsl.exe" -ArgumentList @("-d", "Ubuntu", "--", "bash", "-lc", $cmd) -WindowStyle Hidden
Write-Output "launched: reps=$Reps threads='$Threads'"
