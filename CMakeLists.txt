# Copyright (c) 2012-2013 Konstantin Isakov <ikm@zbackup.org>
# Part of ZBackup. Licensed under GNU GPLv2 or later

cmake_minimum_required( VERSION 2.8.2 )
project( zbackup )

if( ${CMAKE_VERSION} VERSION_LESS "2.8.9" )
  # Use the included FindLibLZMA then
  set( CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" )
endif()

set( CMAKE_BUILD_TYPE Release )

find_package( ZLIB REQUIRED )
include_directories( ${ZLIB_INCLUDE_DIRS} )

find_package( OpenSSL REQUIRED )
include_directories( ${OPENSSL_INCLUDE_DIR} )

find_package( Protobuf REQUIRED )
include_directories( ${PROTOBUF_INCLUDE_DIRS} )
include_directories( ${CMAKE_CURRENT_BINARY_DIR} )

find_program(PROTOBUF_PROTOC_CHECK NAMES protoc DOC "Protobuf compiler binary")

IF(${PROTOBUF_PROTOC_CHECK} STREQUAL "PROTOBUF_PROTOC_CHECK-NOTFOUND")
    MESSAGE(FATAL_ERROR "Could not find protobuf compiler. Make sure protobuf-compiler package is installed.")
ENDIF(${PROTOBUF_PROTOC_CHECK} STREQUAL "PROTOBUF_PROTOC_CHECK-NOTFOUND")

PROTOBUF_GENERATE_CPP( protoSrcs protoHdrs zbackup.proto )

find_package( Threads REQUIRED )

find_package( LibLZMA REQUIRED )
include_directories( ${LIBLZMA_INCLUDE_DIRS} )

file( GLOB sourceFiles "*.cc" )
add_executable( zbackup ${sourceFiles} ${protoSrcs} ${protoHdrs} )

target_link_libraries( zbackup
  ${PROTOBUF_LIBRARIES}
  ${OPENSSL_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT}
  ${ZLIB_LIBRARIES}
  ${LIBLZMA_LIBRARIES}
)

install( TARGETS zbackup DESTINATION bin )
