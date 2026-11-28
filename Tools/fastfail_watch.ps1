# Waits for the next UnrealEditor.exe started after this script (never one already running, such as
# the owner's), attaches as a small debugger and, when that process dies with 0xC0000409 (fast fail,
# which skips Unreal's crash reporter and leaves no Windows record), writes the dying thread's call
# stack with function names to Saved/Logs/FastFail-Stack.txt. Uses only dbghelp.dll from Windows.
# Usage: powershell -File Tools/fastfail_watch.ps1 [wait-seconds]
param([int]$WaitSecs = 900)
$ErrorActionPreference = 'Stop'
$Out = Join-Path $PSScriptRoot '..\Saved\Logs\FastFail-Stack.txt'
$Bins = 'C:\Users\Simon\Documents\Airport-Dynasty-Pro-Unreal\Binaries\Win64;C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64'
Add-Type -TypeDefinition @'
using System; using System.Text; using System.Runtime.InteropServices; using System.Collections.Generic;
public static class FFW {
  [DllImport("kernel32.dll", SetLastError=true)] static extern bool DebugActiveProcess(int pid);
  [DllImport("kernel32.dll")] static extern bool DebugSetProcessKillOnExit(bool k);
  [DllImport("kernel32.dll")] static extern bool WaitForDebugEvent(IntPtr ev, int ms);
  [DllImport("kernel32.dll")] static extern bool ContinueDebugEvent(int pid, int tid, uint st);
  [DllImport("kernel32.dll")] static extern IntPtr OpenProcess(uint a, bool i, int pid);
  [DllImport("kernel32.dll")] static extern IntPtr OpenThread(uint a, bool i, int tid);
  [DllImport("kernel32.dll")] static extern bool GetThreadContext(IntPtr t, IntPtr c);
  [DllImport("kernel32.dll")] static extern bool SetThreadContext(IntPtr t, IntPtr c);
  [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr h);
  [DllImport("dbghelp.dll", CharSet=CharSet.Unicode)] static extern bool SymInitializeW(IntPtr p, string path, bool invade);
  [DllImport("dbghelp.dll")] static extern uint SymSetOptions(uint o);
  [DllImport("dbghelp.dll")] public static extern IntPtr SymFunctionTableAccess64(IntPtr p, ulong a);
  [DllImport("dbghelp.dll")] public static extern ulong SymGetModuleBase64(IntPtr p, ulong a);
  [DllImport("dbghelp.dll", CharSet=CharSet.Unicode)] static extern bool SymFromAddrW(IntPtr p, ulong a, out ulong d, IntPtr si);
  [DllImport("dbghelp.dll", CharSet=CharSet.Unicode)] static extern bool SymGetLineFromAddrW64(IntPtr p, ulong a, out uint d, IntPtr line);
  [DllImport("dbghelp.dll")] static extern bool SymRefreshModuleList(IntPtr p);
  public delegate IntPtr FTA(IntPtr p, ulong a); public delegate ulong GMB(IntPtr p, ulong a);
  [DllImport("dbghelp.dll")] static extern bool StackWalk64(uint mt, IntPtr p, IntPtr t, IntPtr frame, IntPtr ctx, IntPtr rd, FTA fta, GMB gmb, IntPtr tr);
  [DllImport("psapi.dll", CharSet=CharSet.Unicode)] static extern uint GetModuleFileNameExW(IntPtr p, IntPtr m, StringBuilder s, int n);

  static string Name(IntPtr hp, ulong a) {
    IntPtr si = Marshal.AllocHGlobal(88 + 2*512); for (int i=0;i<88+1024;i++) Marshal.WriteByte(si,i,0);
    Marshal.WriteInt32(si,0,88); Marshal.WriteInt32(si,80,512);
    string s; ulong d;
    if (SymFromAddrW(hp,a,out d,si)) s = Marshal.PtrToStringUni(si+84) + "+0x" + d.ToString("x"); else s = "?";
    Marshal.FreeHGlobal(si);
    ulong mb = SymGetModuleBase64(hp,a); var sb = new StringBuilder(520);
    string mod = mb != 0 && GetModuleFileNameExW(hp,(IntPtr)(long)mb,sb,520) > 0 ? System.IO.Path.GetFileName(sb.ToString()) : "?";
    IntPtr ln = Marshal.AllocHGlobal(40); for (int i=0;i<40;i++) Marshal.WriteByte(ln,i,0); Marshal.WriteInt32(ln,0,40); uint ld;
    string where = "";
    if (SymGetLineFromAddrW64(hp,a,out ld,ln)) where = "  " + System.IO.Path.GetFileName(Marshal.PtrToStringUni(Marshal.ReadIntPtr(ln,24))) + ":" + Marshal.ReadInt32(ln,16);
    Marshal.FreeHGlobal(ln);
    return "0x" + a.ToString("x") + "  " + mod + "!" + s + where;
  }
  static string Stack(IntPtr hp, int tid, string syms) {
    var sb = new StringBuilder();
    IntPtr ht = OpenThread(0x1FFFFF,false,tid);
    IntPtr raw = Marshal.AllocHGlobal(1232+16); IntPtr ctx = (IntPtr)(((long)raw + 15) & ~15L);
    for (int i=0;i<1232;i++) Marshal.WriteByte(ctx,i,0); Marshal.WriteInt32(ctx,0x30,0x10000B);
    if (!GetThreadContext(ht,ctx)) { sb.AppendLine("GetThreadContext failed " + Marshal.GetLastWin32Error()); return sb.ToString(); }
    SymSetOptions(0x2 | 0x4 | 0x10 | 0x200);
    SymInitializeW(hp, syms, true); SymRefreshModuleList(hp);
    IntPtr fr = Marshal.AllocHGlobal(1024); for (int i=0;i<1024;i++) Marshal.WriteByte(fr,i,0);
    Marshal.WriteInt64(fr,0,Marshal.ReadInt64(ctx,0xF8)); Marshal.WriteInt32(fr,12,3);
    Marshal.WriteInt64(fr,32,Marshal.ReadInt64(ctx,0xA0)); Marshal.WriteInt32(fr,44,3);
    Marshal.WriteInt64(fr,48,Marshal.ReadInt64(ctx,0x98)); Marshal.WriteInt32(fr,60,3);
    FTA fta = SymFunctionTableAccess64; GMB gmb = SymGetModuleBase64;
    for (int n=0; n<64; n++) {
      if (!StackWalk64(0x8664,hp,ht,fr,ctx,IntPtr.Zero,fta,gmb,IntPtr.Zero)) break;
      ulong pc = (ulong)Marshal.ReadInt64(fr,0); if (pc == 0) break;
      sb.AppendLine("  " + n.ToString("00") + " " + Name(hp,pc));
    }
    return sb.ToString();
  }
  public static string Watch(int pid, string syms, Func<string,bool> log) {
    if (!DebugActiveProcess(pid)) return "attach failed " + Marshal.GetLastWin32Error();
    DebugSetProcessKillOnExit(false);
    IntPtr hp = OpenProcess(0x1FFFFF,false,pid);
    IntPtr ev = Marshal.AllocHGlobal(256); bool first = true; int seen = 0;
    while (true) {
      if (!WaitForDebugEvent(ev, 1000)) continue;
      int code = Marshal.ReadInt32(ev,0), epid = Marshal.ReadInt32(ev,4), tid = Marshal.ReadInt32(ev,8);
      uint cont = 0x00010002; // DBG_CONTINUE
      if (code == 5) { ContinueDebugEvent(epid,tid,cont); return "process ended (code " + Marshal.ReadInt32(ev,16) + ")"; }
      if (code == 1) {
        uint ex = (uint)Marshal.ReadInt32(ev,16); ulong addr = (ulong)Marshal.ReadInt64(ev,32); int firstChance = Marshal.ReadInt32(ev,16+152);
        if (ex == 0x80000003 || ex == 0x4000001F) {
          if (first) first = false;
          else { // a hard-coded break (ensure with a debugger attached): step past it
            IntPtr ht = OpenThread(0x1FFFFF,false,tid); IntPtr raw = Marshal.AllocHGlobal(1248); IntPtr c = (IntPtr)(((long)raw+15)&~15L);
            for (int i=0;i<1232;i++) Marshal.WriteByte(c,i,0); Marshal.WriteInt32(c,0x30,0x10000B);
            if (GetThreadContext(ht,c) && (ulong)Marshal.ReadInt64(c,0xF8) == addr) { Marshal.WriteInt64(c,0xF8,(long)addr+1); SetThreadContext(ht,c); }
            Marshal.FreeHGlobal(raw); CloseHandle(ht);
            if (++seen < 20) log("break (ensure?) at " + Name(hp,addr) + " - stepped past");
          }
        } else if (ex == 0xC0000409 || ex == 0xC00000FD || (firstChance == 0 && ex != 0xE06D7363)) {
          string r = "DEATH 0x" + ex.ToString("X8") + " (fast-fail code " + Marshal.ReadInt64(ev,16+24+8) + ") on thread " + tid + " at " + Name(hp,addr) + "\r\n" + Stack(hp,tid,syms);
          log(r);
          ContinueDebugEvent(epid,tid,0x80010001); return r;
        } else cont = 0x80010001; // DBG_EXCEPTION_NOT_HANDLED: the game handles its own
      }
      ContinueDebugEvent(epid,tid,cont);
    }
  }
}
'@
$t0 = Get-Date
function Write-Out($s) { Add-Content -Path $Out -Value $s -Encoding utf8; $true }
Set-Content -Path $Out -Value "FastFail watch from $t0" -Encoding utf8
$p = $null
while (-not $p -and ((Get-Date) - $t0).TotalSeconds -lt $WaitSecs) {
  $p = Get-Process UnrealEditor -ErrorAction SilentlyContinue | Where-Object { $_.StartTime -gt $t0 } | Select-Object -First 1
  if (-not $p) { Start-Sleep -Milliseconds 500 }
}
if (-not $p) { Write-Out "no new UnrealEditor started"; exit 1 }
Start-Sleep -Seconds 5
Write-Out "attached to pid $($p.Id)"
$r = [FFW]::Watch($p.Id, $Bins, [Func[string,bool]]{ param($s) Write-Out $s })
Write-Out "end: $r"
