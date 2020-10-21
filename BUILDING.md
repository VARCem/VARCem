## Building VARCem from Source


#### Fred N. van Kempen, <decwiz@yahoo.com>

#### Last Updated: 10/20/2020


These instructions guide you in building the **VARCem** emulator from
its published sources. Currently, the emulator can be built for **Windows**,
**Linux** (Debian-tested) and **BSD UNIX** (FreeBSD-tested). More supported 
platforms will be added as time permits.


#### Microsoft Windows

#### Get the sources.
Get a copy of the source tree (by cloning the GIT repository, or
by downloading a ZIP archive from GIT, or from the website) and
unpack them (if needed) into a folder:

  `D:`
  `cd \VARCem`
  `mkdir src`
  `cd src`
  `unzip varcem-1.7.4.zip`

which puts the source tree into the *D:\VARCem\src* folder.

You will also need the "externals" package, which is a single archive containing all the files from other libraries and packages the emulator depends on. You can find this package on the Downloads page of the website, near the bottom of the page. The ZIP file is intended for Windows systems, and the tar.gz file is for UNIX and Linux systems (although these, too, can use the ZIP file.) The contents are the same.

  `D:`
  `cd \VARCem`
  `unzip externals.zip`

The default placement of this package is in the folder "external" at the same level as where the "src" folder is located. This may seem odd, but it was done to allow for more than one "src" folder, for example, one for a version that you are working on.

With the default placement, all is ready.  If you used a different path for the package, make sure to update the EXT_PATH variable in the Makefile.

For the Windows environment, two methods of compiling these sources are currently possible:

##### Using Microsoft's Visual Studio development system.
Tested with versions **2015** ("VC14") and **2017*** ("VC15"), this is now a
fairly easy process.

1.  Install the Visual Studio system (the *Community* release is sufficient) and make sure it works.

2.  Go to the *win\* folder, then to the *vc14* folder for Visual Studio 2015, or to the *vc15* folder for the 2017 release of Visual Studio.

3.  Double-click on the *VARCem.sln* solution file, and Visual Studio will load the project for you.

4.  Select your preferred type of build (x86 or x64, Debug or Release) and build the project.

**All done!**  You can repeat this for other build types if so desired.

For those who prefer to use **command-line tools** to process a *Makefile* (the author is such an oddity), you can also open up a **Visual Studio Command Prompt** from the Start Menu, and type the command:

  `d:`
  `cd \VARCem\src`
  `make -f win\Makefile.VC`
  
and voila, there it goes. If things are set up properly, this is about the fastest way to compile the sources on Windows.

##### Using the MSYS2 "MinGW" toolset.
Building the emulator is also possible using the free **MinGW** compiler toolset, which is based on the well-known *gcc* toolset. Although also available as a standalone project, we will use the version that is part of the **MSYS2** project, which aims to create a *usable Linux-like environment on the Windows platform*. The MinGW toolset is part of that environment.

1.  Download the most recent **MSYS2** distribution for your system.

    Please note that MSYS2 is a *64-bit application*, even though it
    (also) contains a 32-bit MinGW compiler.

2.  Install MSYS2 onto your system, using either *C:\msysS2* or the
    suggested *C:\msysS64* folders as a root folder.

3.  The standard installation of MSYS2 does not include a compiler
    or other 'build tools'. This is surprising, given the main
    objecttive of the project, but so be it.

    The easiest way to install packages like these is to use its
    built-in package manager, **pacman**.  You can install a full
    "base setup" for GCC and friends with the command:

      `pacman -S --needed base-devel mingw-w64-i686-toolchain \`
      `mingw-w64-x86_64-toolchain git subversion mercurial \`
      `mingw-w64-i686-cmake mingw-w64-x86_64-cmake \`
      `mingw-w64-i686-libpng mingw-w64-x86_64-libpng`

    (the backslashes are only for clarity; you can type it as one
    single line if you wish.)

    Pacman will show you the various packages and sub-packages to
    install, just type **ENTER** to agree, followed by a **y** to begin
    downloading and installing these packages.

4.  Make sure this step worked, by typing commands like:

     `cc`
     `gcc`
     `make`

    just so we know this is done.

5.  You may want to install a text editor of choice; in our case, we
    will install the **vi** (or, really, **Vim**) editor, using the package
    manager:

      `pacman -S vim`

    Again, make sure this step worked.

6.  Finally, we are good to go at compiling the VARCem sources.

    You are now ready to build, using a command like:

      `make -f win/Makefile.MinGW`

    which will hopefully build the application's executables.

With these steps, you have set up a build environment, suitable
to build (and develop !) the VARCem project application(s) for
Windows.

There is also a **Makefile.local** file, which you can use as a base for
yourself.  Copy it to just **Makefile**, and use your favorite text editor
to update the settings the way you want them. You can now just type

      `make`

and it should build.
