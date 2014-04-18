@echo off
start "" "TimerRunService.exe" "-i"
@ping -n 1 127.1 >nul 2>nul
start "" "TimerRunService.exe" "-r"
