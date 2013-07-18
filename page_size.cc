// Copyright (c) 2012-2013 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later

#include "page_size.hh"

#include <unistd.h>

unsigned getPageSize()
{
  static unsigned value = 0;

  if ( !value )
    value = sysconf( _SC_PAGESIZE );

  return value;
}
