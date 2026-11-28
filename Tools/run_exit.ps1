# Runs the game and writes its full Windows exit code (bash only sees the low byte) to Saved/Logs/Views-Exit.txt.
# usage: powershell -NoProfile -File Tools/run_exit.ps1 <seconds> <exe> <args...>
$Secs = [int]$args[0]; $Exe = $args[1]; $Rest = @($args | Select-Object -Skip 2)
$Out = Join-Path $PSScriptRoot '..\Saved\Logs\Views-Exit.txt'
$p = Start-Process -FilePath $Exe -ArgumentList $Rest -PassThru -NoNewWindow
$h = $p.Handle   # (kept, so the exit code can be read after it ends)
if (-not $p.WaitForExit($Secs * 1000)) { $p.Kill(); "timeout after $Secs s" | Out-File -Encoding utf8 $Out; exit 124 }
$c = $p.ExitCode
$n = switch ('{0:X8}' -f $c) { 'C0000005' {'access violation'} 'C00000FD' {'stack overflow'} 'C0000409' {'fast fail / stack buffer'} 'C0000374' {'heap damaged'} 'C000007F' {'disk full'} 'C0000017' {'no memory'} 'C000001D' {'illegal instruction'} 'E06D7363' {'C++ exception'} '887A0005' {'GPU device removed'} '887A0006' {'GPU hung'} '80000003' {'breakpoint'} default {'?'} }
("{0} (0x{1:X8}, {2})" -f $c, $c, $n) | Out-File -Encoding utf8 $Out
exit ($c -band 255)
