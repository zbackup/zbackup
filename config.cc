// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org> and ZBackup contributors, see CONTRIBUTORS
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#include <sys/wait.h>
#include <cerrno>

#include "zbackup_base.hh"
#include "zbackup.pb.h"
#include "sptr.hh"

#include "config.hh"
#include "file.hh"
#include "debug.hh"

#if defined(_PATH_VI)
# define EDITOR _PATH_VI
#else
# define EDITOR "/bin/vi"
#endif

#ifndef _PATH_BSHELL
# define _PATH_BSHELL "/bin/sh"
#endif

#define	MAX_TEMPSTR 131072

ZConfig::ZConfig( string const & storageDir, string const & password ):
  ZBackupBase( storageDir, password, true )
{
}

string ZConfig::toString( google::protobuf::Message const & message )
{
  std::string str;
  google::protobuf::TextFormat::PrintToString( message, &str );

  return str;
}

void ZConfig::show()
{
  printf( "%s", toString( extendedStorageInfo.config() ).c_str() );
}

bool ZConfig::edit()
{
  string configData( toString( extendedStorageInfo.config() ) );

  /* Turn off signals. */
  (void) signal( SIGHUP, SIG_IGN );
  (void) signal( SIGINT, SIG_IGN );
  (void) signal( SIGQUIT, SIG_IGN );

  sptr< TemporaryFile > tmpFile = tmpMgr.makeTemporaryFile();
  const char * tmpFileName = tmpFile->getFileName().c_str();

  sptr< File> tmpConfig = new File( tmpFileName, File::WriteOnly );
  tmpConfig->writeRecords( configData.c_str(), configData.size(), 1 );

again:
  tmpConfig->rewind();
  if ( tmpConfig->error() )
  {
    verbosePrintf( "Error while writing config to %s\n", tmpFileName );
fatal:
    tmpFile.reset();
    exit( EXIT_FAILURE );
  }

  char * editorEnv;
  string editor;
  if ( ( ( editorEnv = getenv( "VISUAL" ) ) == NULL || *editorEnv == '\0' ) &&
      ( ( editorEnv = getenv( "EDITOR" ) ) == NULL || *editorEnv == '\0' ) )
    editor.assign( EDITOR );
  else
    editor.assign( editorEnv );

  /* we still have the file open.  editors will generally rewrite the
   * original file rather than renaming/unlinking it and starting a
   * new one; even backup files are supposed to be made by copying
   * rather than by renaming.  if some editor does not support this,
   * then don't use it.  the security problems are more severe if we
   * close and reopen the file around the edit.
   */

  string shellArgs;
  shellArgs += editor;
  shellArgs += " ";
  shellArgs += tmpFileName;

  pid_t pid, xpid;

  switch ( pid = fork() )
  {
    case -1:
      perror( "fork" );
      goto fatal;
    case 0:
      /* child */
      execlp( _PATH_BSHELL, _PATH_BSHELL, "-c", shellArgs.c_str(), (char *) 0 );
      perror( editor.c_str() );
      exit( EXIT_FAILURE );
    /*NOTREACHED*/
    default:
      /* parent */
      break;
  }

  /* parent */
  int waiter;
  for ( ; ; )
  {
    xpid = waitpid( pid, &waiter, 0 );
    if ( xpid == -1 )
    {
      if ( errno != EINTR )
        verbosePrintf( "waitpid() failed waiting for PID %ld from \"%s\": %s\n",
            (long) pid, editor.c_str(), strerror( errno ) );
    }
    else
    if (xpid != pid)
    {
      verbosePrintf( "wrong PID (%ld != %ld) from \"%s\"\n",
          (long) xpid, (long) pid, editor.c_str() );
      goto fatal;
    }
    else
    if ( WIFEXITED( waiter ) && WEXITSTATUS( waiter ) )
    {
      verbosePrintf( "\"%s\" exited with status %d\n",
          editor.c_str(), WEXITSTATUS( waiter ) );
      goto fatal;
    }
    else
    if ( WIFSIGNALED( waiter ) )
    {
      verbosePrintf( "\"%s\" killed; signal %d (%score dumped)\n",
          editor.c_str(), WTERMSIG( waiter ),
          WCOREDUMP( waiter ) ? "" : "no ");
      goto fatal;
    }
    else
      break;
  }
  (void) signal( SIGHUP, SIG_DFL );
  (void) signal( SIGINT, SIG_DFL );
  (void) signal( SIGQUIT, SIG_DFL );

  tmpConfig->close();
  tmpConfig = new File( tmpFileName, File::ReadOnly );

  string newConfigData;
  newConfigData.resize( tmpConfig->size() );
  tmpConfig->read( &newConfigData[ 0 ], newConfigData.size() );
  ConfigInfo newConfig;
  bool isChanged = false;

  int ret = 0;
  if ( !parse( newConfigData, &newConfig ) )
    ret = -1;
  else
  {
    if ( toString( extendedStorageInfo.config() ) == toString( newConfig ) )
    {
      verbosePrintf( "No changes made to config\n" );
      goto end;
    }
    else
      verbosePrintf( "Updating configuration...\n" );
  }

  switch ( ret )
  {
    case 0:
      goto success;
    case -1:
      for ( ; ; )
      {
        printf( "Do you want to retry the same edit? " );
        fflush( stdout );

        string input;
        input.resize( MAX_TEMPSTR );
        if ( fgets( &input[ 0 ], input.size(), stdin ) == 0L )
          continue;

        switch ( input[ 0 ] )
        {
          case 'y':
          case 'Y':
            goto again;
          case 'n':
          case 'N':
            goto abandon;
          default:
            fprintf( stderr, "Enter Y or N\n" );
        }
      }
    /*NOTREACHED*/
    case -2:

abandon:
      verbosePrintf( "Configuration is kept intact\n" );
      goto end;

success:
      extendedStorageInfo.mutable_config()->CopyFrom( newConfig );
      verbosePrintf(
"Configuration successfully updated!\n"
"Updated configuration:\n\n%s", toString( extendedStorageInfo.config() ).c_str() );
      isChanged = true;
      goto end;

    default:
      verbosePrintf( "panic: bad switch()\n" );
      goto fatal;
  }

end:
  tmpConfig.reset();
  tmpFile.reset();

  return isChanged;
}

bool ZConfig::parse( const string & str, google::protobuf::Message * mutable_message )
{
  return google::protobuf::TextFormat::ParseFromString( str, mutable_message );
}
