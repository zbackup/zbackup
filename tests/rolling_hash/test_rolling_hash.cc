// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org>
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <map>
#include <set>
#include <utility>
#include "../../rolling_hash.hh"
#include "../../random.hh"

using std::vector;
using std::map;
using std::set;
using std::pair;
using std::make_pair;

int main()
{
  // Generate a buffer with random data, then pick slices there and try
  // different strategies of rolling to them
  vector< char > data( 65536 );

  Random::genaratePseudo( data.data(), data.size() );

  for ( unsigned iteration = 0; iteration < 5000; ++iteration )
  {
    unsigned sliceBegin = rand() % data.size();
    unsigned sliceSize = 1 + ( rand() % ( data.size() - sliceBegin ) );

    // Calculate the hash by roll-ins only
    uint64_t rollIns;
    {
      RollingHash hash;

      for ( unsigned x = 0; x < sliceSize; ++x )
        hash.rollIn( data[ sliceBegin + x ] );

      rollIns = hash.digest();
    }

    // Calculate the hash by rolling-in from the beginning of data to sliceSize,
    // then rotating to sliceBegin

    uint64_t rotates;
    {
      RollingHash hash;

      for ( unsigned x = 0; x < sliceSize; ++x )
        hash.rollIn( data[ x ] );

      for ( unsigned x = 0; x < sliceBegin; ++x )
        hash.rotate( data[ sliceSize + x ], data[ x ] );

      rotates = hash.digest();
    }

    if ( rollIns != rotates )
    {
      fprintf( stderr, "Error in iteration %u: %016lx vs %016lx\n",
               iteration, rollIns, rotates );

      return EXIT_FAILURE;
    }

    printf( "Iteration %u: %016lx\n", iteration, rollIns );
  }
  fprintf( stderr, "Rolling hash test produced equal results\n" );

  // Test collisions

  // Maps the hash to the ranges. Ideally each hash should be mapped to a
  // single range
  map< uint64_t, set< pair< unsigned, unsigned > > > collisions;
  size_t collisionValuesCount = 0;

  for ( unsigned iteration = 0; iteration < 500000; ++iteration )
  {
    unsigned sliceBegin = rand() % ( data.size() - 7 );
    // A minimum of 16 should be enough to ensure every unique slice corresponds
    // to a unique random sequence with a very high probability
    unsigned sliceSize = 16 + ( rand() % ( data.size() - sliceBegin ) );

    // Calculate the hash by roll-ins (fastest)
    uint64_t rollIns;
    {
      RollingHash hash;

      for ( unsigned x = 0; x < sliceSize; ++x )
        hash.rollIn( data[ sliceBegin + x ] );

      rollIns = hash.digest();
    }

    if ( collisions[ rollIns ].insert( make_pair( sliceBegin, sliceSize ) ).second )
      ++collisionValuesCount;

    if ( ! ( ( iteration + 1 ) % 1000 ) )
    printf( "Iteration %u: %016lx\n", iteration, rollIns );
  }

  size_t collisionsFound = collisionValuesCount - collisions.size();
  double collisionsPercentage = double( collisionsFound ) * 100 /
                                collisionValuesCount;

  fprintf( stderr, "Collisions: %.04f%% (%zu in %zu)\n", collisionsPercentage,
           collisionsFound, collisionValuesCount );

  if ( collisionsFound )
  {
    // The probability of a collision in 500000 hashes is one to ~6 billions
    fprintf( stderr, "Found a collision, which should be highly unlikely\n" );
    return EXIT_FAILURE;
  }

  fprintf( stderr, "Rolling hash test succeeded\n" );

  return EXIT_SUCCESS;
}
