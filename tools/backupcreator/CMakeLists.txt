# Copyright (c) 2015 Markus Knetschke <markus.knetschke@gmail.com>
# Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

cmake_minimum_required( VERSION 2.6.0 )
project( backupcreator )

if( NOT CMAKE_BUILD_TYPE )
  set( CMAKE_BUILD_TYPE Release )
endif( NOT CMAKE_BUILD_TYPE )

find_package( Protobuf REQUIRED )
include_directories( ${PROTOBUF_INCLUDE_DIRS} )
include_directories( ${CMAKE_CURRENT_BINARY_DIR} )

find_program( PROTOBUF_PROTOC_CHECK NAMES protoc DOC "Protobuf compiler binary" )

IF( ${PROTOBUF_PROTOC_CHECK} STREQUAL "PROTOBUF_PROTOC_CHECK-NOTFOUND" )
  MESSAGE( FATAL_ERROR "Could not find protobuf compiler. Make sure protobuf-compiler package is installed." )
ENDIF( ${PROTOBUF_PROTOC_CHECK} STREQUAL "PROTOBUF_PROTOC_CHECK-NOTFOUND" )

PROTOBUF_GENERATE_CPP( protoSrcs protoHdrs backupcreator.proto )

file( GLOB sourceFiles "*.cc" )
add_executable( backupcreator ${sourceFiles} ${protoSrcs} ${protoHdrs} )

target_link_libraries( backupcreator
  ${PROTOBUF_LIBRARIES}
)

install( TARGETS backupcreator DESTINATION bin )
