ESP-IDF sdk installed from single Windows installer, to common directory in c:\bin
There are various ways to install it and run the tools. dfu-util doesn't seem to be added to the PATH in all cases.
Some commands work better in CMD shell, others in Visual Studio Code tool scripts
VS Code tool scripts: Press F1 then type ESP. Each has a simpler shortcut.

Install Zadig app, for USB driver installation.
List all devices.
Expect to see ESP32-S2 Interface 2
Install a version of libusb for the device


    *  Executing task: dfu-util -d 303a:2 -D c:\develop\esp32\tusb_hid\build\dfu.bin 

    dfu-util 0.11

    Copyright 2005-2009 Weston Schmidt, Harald Welte and OpenMoko Inc.
    Copyright 2010-2021 Tormod Volden and Stefan Schmidt
    This program is Free Software and has ABSOLUTELY NO WARRANTY      
    Please report bugs to http://sourceforge.net/p/dfu-util/tickets/  

    Failed to retrieve language identifiers
    Opening DFU capable USB device...
    Device ID 303a:0002
    Run-Time device DFU version 0110
    Claiming USB DFU (Run-Time) Interface...
    Setting Alternate Interface zero...
    Determining device status...
    error get_status: LIBUSB_ERROR_IO

    *  The terminal process "C:\WINDOWS\System32\WindowsPowerShell\v1.0\powershell.exe -Command dfu-util -d 303a:2 -D c:\develop\esp32\tusb_hid\build\dfu.bin" terminated with exit code: 1. 

After uninstalling USB devie drivers, it can help to reboot the computer.
When resetting the s2mini device, it sometimes behaves differently. Try again, slower.
Hold GPIO 0 button down. Briefly press RESET button. Later, release pin 0.
Two devices appear in Windows:
- ESP32S2 device 
- Serial port COM7. Not sure what this is good for.

Installed WinUSB (libusb) using Zadig. Then serial port COM5 appeared.

Trying with device target = ESP32S2 by ESP-USB bridge (not DFU)
https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/api-guides/usb-otg-console.html

Using idf.py at the command line, I was able to flash the ESP32-S2. After several resets,
it connected correctly as a USB device and did the trick. The mouse moved in a square and
the letter A was typed.

This worked about 3 times after various resets, but since then,
- The HID device has not been recognised again.
- I cannot see the serial port device when in bootloader mode.
- It does recognise that there is a USB device with VID and PID, but no further detail.
- Same behaviour when I press G0 button or not. Button is not damaged. Checked on pin 5 of actual chip
- Somebody on the web said it is impossible to trash the ROM bootloader. I am not sure about this.

2023-12-30:
My ESP32s2 Mini board #1 accepted DFU flashing, although I thought it was in secure mode and would never accept DFU again.
https://www.wemos.cc/en/latest/s2/s2_mini.html

Build in Visual Studio Code using Ctrl-E, B
Flash in CMD shell with dfu-util:
(which has run "C:\bin\Espressif-tools\idf_cmd_init.bat" esp-idf-89a89b1f44f341c977a9f01f3e405218" )
dfu-util -d 303a:2 -D c:\develop\esp32\tusb_hid\build\dfu.bin

Console output: ----------------------
Use sdkconfig to set console output to UART1 on GPIO's 17, 18
Connect FTDI USB serial port to GPIO pins 17, 18 and GND
Monitor COM3 serial port using PuTTy (or whichever COM port the FTDI interface appears on)
Console output from bootloader may not appear, but actually does seem to.

Physical pins:
WEMOS ESP32 S2 Mini board 
39 - Keyboard column L
37 - Keyboard column K
35 - Diode to Keyboard Row Y
33 - Diode to Keyboard Row H
18 - UART1 RX (trivial)
17 - UART1 TX (console log)
16 - Keyboard Column J
