# launch_sweep.ps1 -- start the Ocean scaling sweep detached inside WSL Ubuntu.
# Progress: ocean_omp\_build\sweep_stdout.txt   Result: ocean_omp\_build\summary.csv
param([int]$Reps = 5, [string]$Threads = "1 2 4 8 12 16 24")
$wslDir = "/mnt/c/Users/chait/Chaitu/Prometius/Claude_works/parallel_arch/ocean_omp"
$cmd = "cd $wslDir; bash scripts/run_scaling.sh $Reps '$Threads' > _build/sweep_stdout.txt 2>&1"
Start-Process -FilePath "wsl.exe" -ArgumentList @("-d", "Ubuntu", "--", "bash", "-lc", $cmd) -WindowStyle Hidden
Write-Output "launched: reps=$Reps threads='$Threads'"
