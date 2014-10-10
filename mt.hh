// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#ifndef MT_HH_INCLUDED__
#define MT_HH_INCLUDED__

#include <pthread.h>
#include <stddef.h>

#include "nocopy.hh"

/// Multithreading

class Condition;

class Mutex
{
  friend class Condition;

  pthread_mutex_t mutex;

public:

  Mutex();

  /// Please consider using the Lock class instead
  void lock();

  void unlock();

  ~Mutex();
};

class Lock: NoCopy
{
  Mutex * m;

public:

  Lock( Mutex & mutex ): m( &mutex ) { m->lock(); }

  ~Lock()
  { m->unlock(); }
};

/// Condition variable. Atomically unlocks the given mutex before it suspends
/// waiting for event, and upon the awakening reacquires it
class Condition
{
  pthread_cond_t cond;

public:

  Condition();

  void signal();

  void broadcast();

  /// Mutex must be locked on entrance
  void wait( Mutex & m );

  ~Condition();
};

class Thread
{
public:
  void start();
  void detach();
  void * join();

  virtual ~Thread() {}

protected:
  /// This is the function that is meant to work in a separate thread
  virtual void * threadFunction() throw()=0;

private:
  pthread_t thread;
  static void * __thread_routine( void * );
};

/// Returns the number of CPUs this system has
size_t getNumberOfCpus();

#endif
