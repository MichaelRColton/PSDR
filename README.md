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





New Architecture Build Instructions (WIP):

Windows:
1. Install the GNU Tools ARM Embedded. Add bin (not arm-none-eabi/bin) to path?
2. Navigate to firmware directory in shell and execute: make
3. Use whatever editor you want for the files

For eclipse:
1. File->New->Project...
2. C/C++->Makefule Project with Existing Code, "Next"
3. Browse to the root directory of the PSDR firmware
4. I have been selecting "Cross ARM GCC" not sure if that's needed. "Next"
5. Right click on the project in the Project Explorer and select settings. In C/C++ Build, uncheck "Use default build command" and type "make" (or "make VERBOSE=1") without the quotes. Clock "OK"
6. Try to build the project: CTRL-B or Project Menu->Build-All or right click on the project->Build Project
7. TODO: Set up debugging. Fix all the error messages that come up.
For some reason this doesn't work for me unless I build it at least once from the command line. Also, if I do a make clean, I have to build from the command line. More to learn!