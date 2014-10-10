// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#include "mt.hh"

#include <unistd.h>
#include "check.hh"

Mutex::Mutex()
{
  pthread_mutex_init( &mutex, 0 );
}

void Mutex::lock()
{
  pthread_mutex_lock( &mutex );
}

void Mutex::unlock()
{
  pthread_mutex_unlock( &mutex );
}

Mutex::~Mutex()
{
  pthread_mutex_destroy( &mutex );
}

Condition::Condition()
{
  pthread_cond_init( &cond, 0 );
}

void Condition::signal()
{
  pthread_cond_signal( &cond );
}

void Condition::broadcast()
{
  pthread_cond_broadcast( &cond );
}

void Condition::wait( Mutex & m )
{
  pthread_cond_wait( &cond, &m.mutex );
}

Condition::~Condition()
{
  pthread_cond_destroy( &cond );
}

void * Thread::__thread_routine( void * param )
{
  return ( (Thread *)param ) -> threadFunction();
}

void Thread::start()
{
  CHECK( pthread_create( &thread, 0, &__thread_routine, this ) == 0,
         "pthread_create() failed" );
}

void Thread::detach()
{
  CHECK( pthread_detach( thread ) == 0, "pthread_detach() failed" );
}

void * Thread::join()
{
  void * ret;
  pthread_join( thread, &ret );
  return ret;
}

size_t getNumberOfCpus()
{
  long result = sysconf( _SC_NPROCESSORS_ONLN );

  // Handle -1 and also sanitize the 0 value which wouldn't make sense
  return result < 1 ? 1 : result;
}
