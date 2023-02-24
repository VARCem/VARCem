#!/bin/bash
#
# VARCem	Virtual ARchaeological Computer EMulator.
#		An emulator of (mostly) x86-based PC systems and devices,
#		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
#		spanning the era between 1981 and 1995.
#
#		This file is part of the VARCem Project.
#
#		Build script for the CircleCI remote builder service.
#
# Version:	@(#)build.sh	1.0.1	2023/02/23
#
# Author:	Fred N. van Kempen, <waltje@varcem.com>
#
#		Copyright 2018-2023 Fred N. van Kempen.
#
#		Redistribution and  use  in source  and binary forms, with
#		or  without modification, are permitted  provided that the
#		following conditions are met:
#
#		1. Redistributions of  source  code must retain the entire
#		   above notice, this list of conditions and the following
#		   disclaimer.
#
#		2. Redistributions in binary form must reproduce the above
#		   copyright  notice,  this list  of  conditions  and  the
#		   following disclaimer in  the documentation and/or other
#		   materials provided with the distribution.
#
#		3. Neither the  name of the copyright holder nor the names
#		   of  its  contributors may be used to endorse or promote
#		   products  derived from  this  software without specific
#		   prior written permission.
#
# THIS SOFTWARE  IS  PROVIDED BY THE  COPYRIGHT  HOLDERS AND CONTRIBUTORS
# "AS IS" AND  ANY EXPRESS  OR  IMPLIED  WARRANTIES,  INCLUDING, BUT  NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
# PARTICULAR PURPOSE  ARE  DISCLAIMED. IN  NO  EVENT  SHALL THE COPYRIGHT
# HOLDER OR  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL,  EXEMPLARY,  OR  CONSEQUENTIAL  DAMAGES  (INCLUDING,  BUT  NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE  GOODS OR SERVICES;  LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED  AND ON  ANY
# THEORY OF  LIABILITY, WHETHER IN  CONTRACT, STRICT  LIABILITY, OR  TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING  IN ANY  WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    # Set up variables for the build.
    export COMMIT=${CIRCLE_SHA1::7}
    export BUILD=${CIRCLE_PIPELINE_NUM}
    export EXT_PATH=../external

    # Define the default build options here.
    OPTS="VNC=d VNS=n RDP=n CHD=n"
    TARGET="win-${BUILD}-x86"
    if [ "x$1" = "xstd" ]; then
	PROG=VARCem
	FLAGS="CROSS=y DEBUG=n"
    elif [ "x$1" = "xlog" ]; then
	PROG=VARCem
	FLAGS="CROSS=y DEBUG=n LOGGING=y"
    elif [ "x$1" = "xdbg" ]; then
	PROG=VARCem
	FLAGS="CROSS=y DEBUG=n"
    elif [ "x$1" = "xdev" ]; then
	PROG=VARCem_dev
	FLAGS="CROSS=y DEBUG=y DEV_BUILD=y"
	TARGET="win-${BUILD}_dev-x86"
    fi
    export PROG FLAGS

    echo ; echo "Downloading external build dependencies.."
    curl -# "${EXTDEP_URL}" | tar xzf -
    if [ $? != 0 ]; then
	exit 1
    fi

    # Build the project.
    echo ; echo "Building #${BUILD} target ${TARGET}"
    echo "Compile flags: ${FLAGS}"
    echo "Options selected: ${OPTS}"
#   /usr/bin/i686-w64-mingw32-gcc --version
#   /usr/bin/x86_64-w64-mingw32-gcc --version
    ls -l /usr/bin/i686-w64-mingw32*

    cd src
    make -f win/Makefile.MinGW ${OPTS}
    if [ $? != 0 ]; then
	echo "Build failed, not uploading." 

	exit 1
    fi

    # Package the results so we can upload them.
    echo ; echo "Build #${BUILD} OK, packing up."

    zip -q -9 ../${TARGET}.zip *.exe
    if [ $? != 0 ]; then
	echo "ZIP failed, not uploading." 

	exit 1
    fi

    exit 0

# End of file.
