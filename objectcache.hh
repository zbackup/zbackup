// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#ifndef OBJECTCACHE_HH_INCLUDED__
#define OBJECTCACHE_HH_INCLUDED__

#include <string>
#include <list>
#include <set>
#include <utility>
#include "sptr.hh"
#include "nocopy.hh"

/// ObjectCache allows caching dynamically-allocated objects of any type. The
/// size of the cache is upper-bound and is specified at construction-time.
/// Newly added or recently found objects are placed to the top of the internal
/// stack. When there's no space in the cache, object become removed from the
/// bottom of it
class ObjectCache: NoCopy
{
public:
  ObjectCache( unsigned maxObjects );

  /// Id of the object being stored in the cache
  typedef std::string ObjectId;

  /// Returns a reference to the stored object with the given id, or creates
  /// one if none existed. The caller must know the expected type of the object
  /// and specify it explicitly
  template< class T >
  sptr< T > & entry( ObjectId const & );

  /// Removes a stored object with the given id. Returns true if the object
  /// was removed, false if it didn't exist in the cache
  bool remove( ObjectId const & );

  /// Deletes all the objects from cache
  void clear();

  ~ObjectCache()
  { clear(); }

private:

  /// Base class for a reference to an object being stored
  struct Reference: NoCopy
  {
    virtual ~Reference()
    {}
  };

  /// Having this class allows to delete T via virtual destructor accessible
  /// from the base Reference class
  template< class T >
  struct ReferenceTo: public Reference
  {
    sptr< T > ref;
  };

  struct Object
  {
    ObjectId id;
    Reference * reference;
  };
  typedef std::list< Object > Objects;

  struct ObjectsIteratorComp
  {
    bool operator () ( Objects::iterator const & x, Objects::iterator const & y )
    { return x->id < y->id; }
  };

  typedef std::set< Objects::iterator, ObjectsIteratorComp > ObjectMap;

  unsigned maxObjects;
  Objects objects;
  unsigned totalObjects;
  ObjectMap objectMap;

};

template< class T >
sptr< T > & ObjectCache::entry( ObjectId const & id )
{
  Objects tmp;
  tmp.push_back( Object() );
  tmp.back().id = id;

  std::pair< ObjectMap::iterator, bool > r = objectMap.insert( tmp.begin() );

  if ( r.second )
  {
    // The object was created

    // Init the reference
    ReferenceTo< T > * refTo = new ReferenceTo< T >();
    tmp.back().reference = refTo;

    // Add the object to top of our objects
    objects.splice( objects.begin(), tmp );
    ++totalObjects;

    // evict an entry at the bottom, if needed
    if ( totalObjects > maxObjects )
    {
      Objects::iterator i = --objects.end();
      objectMap.erase( i );
      Reference * ref = i->reference;
      objects.pop_back();
      --totalObjects;

      delete ref;  // We expect that it may throw
    }

    return refTo->ref;
  }
  else
  {
    // The object was existent
    // Move it to the top
    objects.splice( objects.begin(), objects, *r.first );

    return dynamic_cast< ReferenceTo< T > & >( *objects.front().reference ).ref;
  }
}

#endif // OBJECTCACHE_HH
