@echo off
"%ProgramFiles(x86)%\Windows Kits\10\bin\10.0.22621.0\x64\midl.exe" ^
/env x64 ^
/tlb "x64\Debug\ATLProjectcomserver.tlb" ^
/h "ATLProjectcomserver_i.h" ^
/iid "ATLProjectcomserver_i.c" ^
/proxy "ATLProjectcomserver_p.c" ^
/W1 ^
/char unsigned ^
/target NT60 ^
"ATLProjectcomserver.idl" 