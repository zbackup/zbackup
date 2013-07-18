# Copyright (c) 2012-2013 Konstantin Isakov <ikm@zbackup.org>
# Part of ZBackup. Licensed under GNU GPLv2 or later

cmake_minimum_required( VERSION 2.6.0 )
project( tartool )

set( CMAKE_BUILD_TYPE Release )

add_executable( tartool tartool.cc ../file.cc ../dir.cc )

install( TARGETS tartool DESTINATION bin )
