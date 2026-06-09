@echo off
cd /d C:\opt\guardian
set PORT=8155
start /B "" venv\Scripts\python.exe run_waitress.py > logs\full_app.log 2>&1
echo Started on port 8155
