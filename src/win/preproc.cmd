@ECHO OFF
REM
REM VARCem	Virtual ARchaeological Computer EMulator.
REM		An emulator of (mostly) x86-based PC systems and devices,
REM		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
REM		spanning the era between 1981 and 1995.
REM
REM		This file is part of the VARCem Project.
REM
REM		Preprocessor script for Windows systems.
REM
REM		Needed by the IDE building environment.
REM
REM Version:	@(#)preproc.cmd	1.0.3	2020/12/06
REM
REM Author:	Fred N. van Kempen, <decwiz@yahoo.com>
REM
REM		Copyright 2018-2020 Fred N. van Kempen.
REM
REM		Redistribution and  use  in source  and binary forms, with
REM		or  without modification, are permitted  provided that the
REM		following conditions are met:
REM
REM		1. Redistributions of  source  code must retain the entire
REM		   above notice, this list of conditions and the following
REM		   disclaimer.
REM
REM		2. Redistributions in binary form must reproduce the above
REM		   copyright  notice,  this list  of  conditions  and  the
REM		   following disclaimer in  the documentation and/or other
REM		   materials provided with the distribution.
REM
REM		3. Neither the  name of the copyright holder nor the names
REM		   of  its  contributors may be used to endorse or promote
REM		   products  derived from  this  software without specific
REM		   prior written permission.
REM
REM THIS SOFTWARE  IS  PROVIDED BY THE  COPYRIGHT  HOLDERS AND CONTRIBUTORS
REM "AS IS" AND  ANY EXPRESS  OR  IMPLIED  WARRANTIES,  INCLUDING, BUT  NOT
REM LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
REM PARTICULAR PURPOSE  ARE  DISCLAIMED. IN  NO  EVENT  SHALL THE COPYRIGHT
REM HOLDER OR  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
REM SPECIAL,  EXEMPLARY,  OR  CONSEQUENTIAL  DAMAGES  (INCLUDING,  BUT  NOT
REM LIMITED TO, PROCUREMENT OF SUBSTITUTE  GOODS OR SERVICES;  LOSS OF USE,
REM DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED  AND ON  ANY
REM THEORY OF  LIABILITY, WHETHER IN  CONTRACT, STRICT  LIABILITY, OR  TORT
REM (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING  IN ANY  WAY OUT OF THE USE
REM OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  SETLOCAL
  SET infile=%1
  SET outfile=%2
  SET PREPROC=%3
  SET args=%4 %5 %6 %7 %8 %9

  ECHO Preprocessing %infile% ..
  DEL %outfile% 2>NUL
  %PREPROC% %args% %infile% >%outfile% 2>NUL

  ENDLOCAL
