Welcome to **VARCem**, the Virtual Archaeological Computer EMulator.

VARCem is a software application which emulates a selection of of (mostly)
x86-based PC systems and devices based on the ISA, VLB, MCA and PCI buses.

The program tries to be cycle-exact, meaning the guest software (the software
being run inside the emulated system) will run as fast (or, depending on how
you see that..) as slow as it did "back then".  If you selected, say, a 80286
running at 12MHz, that is what the software will see, and how fast it will be
run.


HISTORY
-------
VARCem started out as a new branch of the 86Box emulator, after discussions 
on which path to follow forward. 86Box in its own is a fork of the popular
PCem emulator. At each generation, code and ideas were taken from other
emulator projects, such as DOSBox, Vbox, MAME, Qemu and so on.


LICENSE
-------
VARCem as a whole is released under the BSD 3-Clause Open Software License,
which is compatible with the GPL (GNU Public License) version 2 or later that
is used by many of the modules imported from other emulator projects. Author
credits and specific copyrights can be found in each of the source files.

Obviously, where license terms of individual modules deviate from the general
project license terms, the terms of such an individual module prevail.


Community and Support
---------------------
Information, downloads, additional modules and such can be found on our
[Website](http://www.VARCem.com/). Live support and general help can
also be found on our [IRC channel](https://kiwiirc.com/client/irc.freenode.net/?nick=VARCem_Guest|?#VARCem)

Additionally, documentation can be found (once it is written..) on the [Wiki](http://www.VARCem.com/?page=wiki),
and general discussions and support can be found at the [Forum](http://www.VARCem.com/?page=forum) where you
can find announcements, leave bug reports, and so on.


LEGAL
-----
"This program is  distributed in the hope that it will be useful, but
WITHOUT   ANY  WARRANTY;  without  even   the  implied  warranty  of
MERCHANTABILITY  or FITNESS  FOR A PARTICULAR  PURPOSE."

It's free, it comes with all the sources, but it does not come with
any warranty whatsoever. You probably should not use this software to run
a business-critical piece of (old) software at work, for example.

If there is a problem with the software, please open up a GIT issue so we
can work on it, and/or talk to us on the IRC channel. We cannot promise a
fix, but will try the best we can !


BUILD STATUS
------------
The auto-builds handled by Travis-CI are [![Build Status](https://travis-ci.org/VARCem/VARCem.svg?branch=master)](https://travis-ci.org/VARCem/VARCem)

Last Updated: 2020/06/05
