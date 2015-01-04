@echo off
FOR %%c in (*.png) DO convert.exe %%c -strip %%c