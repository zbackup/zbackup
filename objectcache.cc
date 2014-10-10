// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#include "objectcache.hh"

ObjectCache::ObjectCache( unsigned maxSize_ ): maxObjects( maxSize_ ),
  totalObjects( 0 )
{
}

bool ObjectCache::remove( ObjectId const & id )
{
  Objects tmp;
  tmp.push_back( Object() );
  tmp.back().id = id;
  ObjectMap::iterator i = objectMap.find( tmp.begin() );

  if ( i == objectMap.end() )
    return false;

  // Make sure that in case a destructor raises an exception, the cache
  // is left in a consistent state.
  Reference * ref = (*i)->reference;

  objects.erase( *i );
  objectMap.erase( i );
  --totalObjects;

  delete ref;

  return true;
}

void ObjectCache::clear()
{
  for ( Objects::iterator i = objects.begin(); i != objects.end(); )
  {
    // Make sure that in case a destructor raises an exception, the cache
    // is left in a consistent state.
    Reference * ref = i->reference;
    objectMap.erase( i );
    objects.erase( i++ );
    --totalObjects;

    delete ref;
  }
}
