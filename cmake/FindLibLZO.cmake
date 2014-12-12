# - Find LibLZO
# Find LibLZO headers and library
#
#  LIBLZO_FOUND             - True if liblzo is found.
#  LIBLZO_INCLUDE_DIRS      - Directory where liblzo headers are located.
#  LIBLZO_LIBRARIES         - Lzma libraries to link against.
#  LIBLZO_HAS_AUTO_DECODER  - True if lzo_auto_decoder() is found (required).
#  LIBLZO_HAS_EASY_ENCODER  - True if lzo_easy_encoder() is found (required).
#  LIBLZO_HAS_LZO_PRESET    - True if lzo_lzo_preset() is found (required).
#  LIBLZO_VERSION_MAJOR     - The major version of lzo
#  LIBLZO_VERSION_MINOR     - The minor version of lzo
#  LIBLZO_VERSION_PATCH     - The patch version of lzo
#  LIBLZO_VERSION_STRING    - version number as a string (ex: "5.0.3")

#=============================================================================
# Copyright 2008 Per Øyvind Karlsen <peroyvind@mandriva.org>
# Copyright 2009 Alexander Neundorf <neundorf@kde.org>
# Copyright 2009 Helio Chissini de Castro <helio@kde.org>
# Copyright 2012 Mario Bensi <mbensi@ipsquad.net>
# Copyright 2012-2014 Konstantin Isakov <ikm@zbackup.org> and ZBackup
# contributors, see CONTRIBUTORS
#
# Distributed under the OSI-approved BSD License (the "License"):
#
# CMake - Cross Platform Makefile Generator
# Copyright 2000-2011 Kitware, Inc., Insight Software Consortium
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# * Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
#
# * Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
#
# * Neither the names of Kitware, Inc., the Insight Software Consortium,
#   nor the names of their contributors may be used to endorse or promote
#   products derived from this software without specific prior written
#   permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# ------------------------------------------------------------------------------
#
# The above copyright and license notice applies to distributions of
# CMake in source and binary form.  Some source files contain additional
# notices of original copyright by their contributors; see each source
# for details.  Third-party software packages supplied with CMake under
# compatible licenses provide their own copyright notices documented in
# corresponding subdirectories.
#
# ------------------------------------------------------------------------------
#
# CMake was initially developed by Kitware with the following sponsorship:
#
#  * National Library of Medicine at the National Institutes of Health
#    as part of the Insight Segmentation and Registration Toolkit (ITK).
#
#  * US National Labs (Los Alamos, Livermore, Sandia) ASC Parallel
#    Visualization Initiative.
#
#  * National Alliance for Medical Image Computing (NAMIC) is funded by the
#    National Institutes of Health through the NIH Roadmap for Medical Research,
#    Grant U54 EB005149.
#
#  * Kitware, Inc.
#=============================================================================


find_path(LIBLZO_INCLUDE_DIR lzo/lzo1x.h )
find_library(LIBLZO_LIBRARY lzo2)

if(LIBLZO_INCLUDE_DIR AND EXISTS "${LIBLZO_INCLUDE_DIR}/lzo/lzoconf.h")
    file(STRINGS "${LIBLZO_INCLUDE_DIR}/lzo/lzoconf.h" LIBLZO_HEADER_CONTENTS REGEX "#define LZO_VERSION_STRING.+\"[^\"]+\"")
    string(REGEX REPLACE ".*#define LZO_VERSION_STRING.+\"([^\"]+)\".*" "\\1" LIBLZO_VERSION_STRING "${LIBLZO_HEADER_CONTENTS}")
    unset(LIBLZO_HEADER_CONTENTS)
endif()

# We're just using two functions.
if (LIBLZO_LIBRARY)
   include(CheckLibraryExists)
   CHECK_LIBRARY_EXISTS(${LIBLZO_LIBRARY} lzo1x_decompress_safe "" LIBLZO_HAS_LZO1X_DECOMPRESS_SAFE)
   CHECK_LIBRARY_EXISTS(${LIBLZO_LIBRARY} lzo1x_1_compress "" LIBLZO_HAS_LZO1X_1_COMPRESS)
endif ()

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LibLZO DEFAULT_MSG LIBLZO_INCLUDE_DIR
                                                      LIBLZO_LIBRARY
                                                      LIBLZO_HAS_LZO1X_DECOMPRESS_SAFE
                                                      LIBLZO_HAS_LZO1X_1_COMPRESS
                                 )

if (LIBLZO_FOUND)
    set(LIBLZO_LIBRARIES ${LIBLZO_LIBRARY})
    set(LIBLZO_INCLUDE_DIRS ${LIBLZO_INCLUDE_DIR})
endif ()

mark_as_advanced( LIBLZO_INCLUDE_DIR LIBLZO_LIBRARY )
