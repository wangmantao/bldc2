SET C51INC=C:\Keil\C51\INC\Nuvoton\;C:\Keil\C51\INC\
SET C51LIB=C:\Keil\C51\LIB
SET CPU_TYPE=N76E003
SET CPU_VENDOR=Nuvoton
SET UV2_TARGET=BLDC
SET CPU_XTAL=0x00F42400
"C:\Keil\C51\BIN\C51.EXE" @.\Output\main.__i
"C:\Keil\C51\BIN\BL51.EXE" @.\Output\bldc.lnp
"C:\Keil\C51\BIN\OH51.EXE" ".\Output\bldc" 

hex2bin.exe Output\bldc.hex
