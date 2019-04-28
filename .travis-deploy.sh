#!/bin/bash
#
# VARCem	Virtual ARchaeological Computer EMulator.
#		An emulator of (mostly) x86-based PC systems and devices,
#		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
#		spanning the era between 1981 and 1995.
#
#		This file is part of the VARCem Project.
#
#		Deployment script for the Travis CI remote builder service.
#
# Version:	@(#).travis-deploy.sh	1.0.4	2019/04/27
#
# Author:	Fred N. van Kempen, <decwiz@yahoo.com>
#
#		Copyright 2018,2019 Fred N. van Kempen.
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

    # Ignore the cron builds.
    [ "x${TRAVIS_EVENT_TYPE}" = "xcron" ] && exit 0

    if [ "x${DEV_BUILD}" = "xy" ]; then
	TARGET="win-${TRAVIS_BUILD_NUMBER}_dev-x86"
	BTYPE=dev
    elif [ "x${DEBUG}" = "xy" ]; then
	TARGET="win-${TRAVIS_BUILD_NUMBER}_dbg-x86"
	BTYPE=debug
    elif [ "x${LOGGING}" = "xy" ]; then
	TARGET="win-${TRAVIS_BUILD_NUMBER}_log-x86"
	BTYPE=logging
    else
	TARGET="win-${TRAVIS_BUILD_NUMBER}-x86"
	BTYPE=regular
    fi

    if [ ! -f ${TARGET}.zip ]; then
	echo "Target file ${TARGET}.zip not found, giving up."

	exit 1
    fi

    # We only need the first few characters of the commit ID.
    export COMMIT=${TRAVIS_COMMIT::7}

    echo "Uploading VARCem build #${TRAVIS_BUILD_NUMBER} target ${TARGET}"

    curl -# -X POST \
       -F "type=${BTYPE}" \
       -F "build=${TRAVIS_BUILD_NUMBER}" \
       -F "id=${COMMIT}" \
       -F "notes=GIT #${COMMIT}" \
       -F "file_name=@${TARGET}.zip" \
       ${SITE_URL}

    if [ $? != 0 ]; then
	echo Upload failed.
	exit 1
    fi

    exit 0

# End of file.
