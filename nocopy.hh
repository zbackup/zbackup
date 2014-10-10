// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#ifndef NOCOPY_HH_INCLUDED__
#define NOCOPY_HH_INCLUDED__

/// A simple class to disallow copying of the class objects. Inherit from it to
/// use it
class NoCopy
{
public:
  NoCopy() {}

private:
  NoCopy( NoCopy const & );
  NoCopy & operator = ( NoCopy const & );
};

#endif // NOCOPY_HH
