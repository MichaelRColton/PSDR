PSDR
====

PortableSDR - A Stand Alone HF Software Defined Transciever.



Firmware Build Notes
====================

* Install gcc-arm-none-eabi: apt-get install gcc-arm-none-eabi, or download from https://launchpad.net/gcc-arm-embedded/4.7
* Download Eclipse Kepler, v4.3 for C/C++ dev from https://eclipse.org/downloads/packages/release/Kepler/SR2

* Help -> Install New software
* add Name: GNU ARM, Location: http://gnuarmeclipse.sourceforge.net/updates
* Click the radio button, and hit next, next, agree to the license, finsh, Hit OK to allow unsigned contents
* Restart eclipse
* Help -> Install New Software
* same as before,add: Name: Zylin Embedded CDT, Location: http://opensource.zylin.com/zylincdt 
* sudo apt-get install openocd
* File -> import -> Existing Projects into Workspace (May be a better way here!)
* Navigate to the PSDR/Source directory and add it.
* hit Build All

You should end up with PSDR/Source/Debug/PSDR.elf  which is the binary firmware file

TODO: Flashing firmware to the PSDR instructions


