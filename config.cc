// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org> and ZBackup contributors, see CONTRIBUTORS
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <utime.h>

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

void ZConfig::print()
{
  printf( "%s", toString( extendedStorageInfo.config() ).c_str() );
}

void ZConfig::edit()
{
  // TODO: Rewrite that copy-paste from cronie on C++
  char q[MAX_TEMPSTR], *editor;
  struct stat statbuf;
  struct utimbuf utimebuf;
  int waiter, ret;
  pid_t pid, xpid;
  FILE *legacyTmpNewConfig;

  sptr< TemporaryFile > tmpFile = tmpMgr.makeTemporaryFile();
  const char * tmpFileName = tmpFile->getFileName().c_str();

  if ( !( legacyTmpNewConfig = fopen( tmpFileName, "w" ) ) )
  {
    if ( errno != ENOENT )
    {
      perror( tmpFileName );
      exit( EXIT_FAILURE );
    }
  }

  fputs( toString( extendedStorageInfo.config() ).c_str(), legacyTmpNewConfig );

  if ( fflush( legacyTmpNewConfig ) < EXIT_SUCCESS )
  {
    perror( tmpFileName );
    exit( EXIT_FAILURE );
  }

  /* Set it to 1970 */
  utimebuf.actime = 0;
  utimebuf.modtime = 0;
  utime( tmpFileName, &utimebuf );

again:
  rewind( legacyTmpNewConfig );
  if ( ferror( legacyTmpNewConfig ) )
  {
    fprintf( stderr, "Error while writing new config to %s\n", tmpFileName );

fatal:
    unlink( tmpFileName );
    exit( EXIT_FAILURE );
  }

  if ( ( ( editor = getenv( "VISUAL" )) == NULL || *editor == '\0' ) &&
      ( ( editor = getenv("EDITOR")) == NULL || *editor == '\0') )
  {
    editor = EDITOR;
  }

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

  switch ( pid = fork() )
  {
    case -1:
      perror( "fork" );
      goto fatal;
    case 0:
      /* child */
      execlp( _PATH_BSHELL, _PATH_BSHELL, "-c", shellArgs.c_str(), (char *) 0 );
      perror( editor );
      exit( EXIT_FAILURE );
    /*NOTREACHED*/
    default:
      /* parent */
      break;
  }

  /* parent */
  for ( ; ; )
  {
    xpid = waitpid( pid, &waiter, 0 );
    if ( xpid == -1 )
    {
      if ( errno != EINTR )
        fprintf(stderr,
            "waitpid() failed waiting for PID %ld from \"%s\": %s\n",
            (long) pid, editor, strerror( errno ) );
    }
    else
    if (xpid != pid)
    {
      fprintf( stderr, "wrong PID (%ld != %ld) from \"%s\"\n",
          (long) xpid, (long) pid, editor);
      goto fatal;
    }
    else
    if ( WIFEXITED( waiter ) && WEXITSTATUS( waiter ) )
    {
      fprintf(stderr, "\"%s\" exited with status %d\n",
          editor, WEXITSTATUS( waiter ) );
      goto fatal;
    }
    else
    if ( WIFSIGNALED( waiter ) )
    {
      fprintf(stderr,
          "\"%s\" killed; signal %d (%score dumped)\n",
          editor, WTERMSIG( waiter ),
          WCOREDUMP( waiter ) ? "" : "no ");
      goto fatal;
    }
    else
      break;
  }
  (void) signal( SIGHUP, SIG_DFL );
  (void) signal( SIGINT, SIG_DFL );
  (void) signal( SIGQUIT, SIG_DFL );

  /* lstat doesn't make any harm, because 
   * the file is stat'ed only when config is touched
   */
  if ( lstat( tmpFileName, &statbuf ) < 0 )
  {
    perror( "lstat" );
    goto fatal;
  }

  if ( !S_ISREG( statbuf.st_mode ) )
  {
    fprintf( stderr, "Illegal config\n" );
    goto remove;
  }

  if ( statbuf.st_mtime == 0 )
  {
    fprintf( stderr, "No changes made to config\n" );
    goto remove;
  }

  fprintf( stderr, "Installing new config\n" );
  fclose( legacyTmpNewConfig );

  if ( !( legacyTmpNewConfig = fopen( tmpFileName, "r+" ) ) )
  {
    perror("cannot read new config");
    goto remove;
  }
  if ( legacyTmpNewConfig == 0L )
  {
    perror("fopen");
    goto fatal;
  }

  try
  {
    File tmpNewConfig( tmpFileName, File::Update );
    ConfigInfo newConfig;
    string newConfigData;
    newConfigData.resize( tmpNewConfig.size() );
    tmpNewConfig.read( &newConfigData[ 0 ], newConfigData.size() );
    parse( newConfigData, &newConfig );
    tmpNewConfig.close();
    ret = 0;
  }
  catch ( std::exception & e )
  {
    ret = -1;
  }

  switch ( ret )
  {
    case 0:
      break;
    case -1:
      for ( ; ; )
      {
        printf( "Do you want to retry the same edit? " );
        fflush( stdout );
        q[ 0 ] = '\0';
        if ( fgets( q, sizeof q, stdin ) == 0L )
          continue;

        switch ( q[ 0 ] )
        {
          case 'y':
          case 'Y':
            goto again;
          case 'n':
          case 'N':
            goto abandon;
          default:
            fprintf(stderr, "Enter Y or N\n");
        }
      }
    /*NOTREACHED*/
    case -2:
abandon:
      fprintf( stderr, "edits left in %s\n", tmpFileName );
      goto done;
    default:
      fprintf( stderr, "panic: bad switch() in replace_cmd()\n" );
      goto fatal;
  }

  remove:
    tmpFile.reset();
  done:
    verbosePrintf(
"Configuration successfully updated!\n"
"Current repo configuration:\n" );

//  printf( "%s", toString( extendedStorageInfo.config() ).c_str() );
}

bool ZConfig::parse( const string & str, google::protobuf::Message * mutable_message )
{
  return google::protobuf::TextFormat::ParseFromString( str, mutable_message );
}
