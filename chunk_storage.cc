// Copyright (c) 2012-2014 Konstantin Isakov <ikm@zbackup.org> and ZBackup contributors, see CONTRIBUTORS
// Part of ZBackup. Licensed under GNU GPLv2 or later + OpenSSL, see LICENSE

#include "check.hh"
#include "chunk_storage.hh"
#include "debug.hh"
#include "dir.hh"
#include "utils.hh"
#include "random.hh"

namespace ChunkStorage {

Writer::Writer( Config const & configIn,
                EncryptionKey const & encryptionKey,
                TmpMgr & tmpMgr, ChunkIndex & index, string const & bundlesDir,
                string const & indexDir, size_t maxCompressorsToRun ):
  config( configIn ), encryptionKey( encryptionKey ),
  tmpMgr( tmpMgr ), index( index ), bundlesDir( bundlesDir ),
  indexDir( indexDir ), hasCurrentBundleId( false ),
  maxCompressorsToRun( maxCompressorsToRun ), runningCompressors( 0 )
{
  verbosePrintf( "Using up to %zu thread(s) for compression\n",
                 maxCompressorsToRun );
}

Writer::~Writer()
{
  waitForAllCompressorsToFinish();
}

bool Writer::add( ChunkId const & id, void const * data, size_t size )
{
  if ( index.addChunk( id, size, getCurrentBundleId() ) )
  {
    // Added to the index? Emit to the bundle then
    if ( getCurrentBundle().getPayloadSize() + size >
         config.GET_STORABLE( bundle, max_payload_size ) )
      finishCurrentBundle();

    getCurrentBundle().addChunk( id.toBlob(), data, size );

    return true;
  }
  else
    return false;
}

void Writer::addBundle( BundleInfo const & bundleInfo, Bundle::Id const & bundleId )
{
  if ( !indexFile.get() )
  {
    // Create a new index file
    indexTempFile = tmpMgr.makeTemporaryFile();
    indexFile = new IndexFile::Writer( encryptionKey,
                                       indexTempFile->getFileName() );
  }

  indexFile->add( bundleInfo, bundleId );
}

void Writer::commit()
{
  finishCurrentBundle();

  waitForAllCompressorsToFinish();

  // Move all bundles
  for ( size_t x = pendingBundleRenames.size(); x--; )
  {
    PendingBundleRename & r = pendingBundleRenames[ x ];
    r.first->moveOverTo( Bundle::generateFileName( r.second, bundlesDir,
                                                   true ) );
  }

  pendingBundleRenames.clear();

  // Move the index file
  if ( indexFile.get() )
  {
    indexFile.reset();
    // Generate a random filename
    unsigned char buf[ 24 ]; // Same comments as for Bundle::IdSize

    Random::generatePseudo( buf, sizeof( buf ) );

    indexTempFile->moveOverTo( Dir::addPath( indexDir,
                                             Utils::toHex( buf, sizeof( buf ) ) ) );
    indexTempFile.reset();
  }
}

void Writer::reset()
{
  finishCurrentBundle();

  waitForAllCompressorsToFinish();

  pendingBundleRenames.clear();

  if ( indexFile.get() )
  {
    indexFile.reset();
  }
}

Bundle::Creator & Writer::getCurrentBundle()
{
  if ( !currentBundle.get() )
    currentBundle = new Bundle::Creator;
  return *currentBundle;
}

void Writer::finishCurrentBundle()
{
  if ( !currentBundle.get() )
    return;

  Bundle::Id const & bundleId = getCurrentBundleId();

  addBundle( currentBundle->getCurrentBundleInfo(), bundleId );

  sptr< TemporaryFile > file = tmpMgr.makeTemporaryFile();

  pendingBundleRenames.push_back( PendingBundleRename( file, bundleId ) );

  // Create a new compressor

  // Wait for some compressors to finish if there are too many of them
  Lock _( runningCompressorsMutex );
  while ( runningCompressors >= maxCompressorsToRun )
    runningCompressorsCondition.wait( runningCompressorsMutex );

  Compressor * compressor = new Compressor( config,
                                            *this, currentBundle,
                                            file->getFileName() );

  currentBundle.reset();
  hasCurrentBundleId = false;

  compressor->start();
  ++runningCompressors;
}

void Writer::waitForAllCompressorsToFinish()
{
  Lock _( runningCompressorsMutex );
  while ( runningCompressors )
    runningCompressorsCondition.wait( runningCompressorsMutex );
}

Bundle::Id const & Writer::getCurrentBundleId()
{
  if ( !hasCurrentBundleId )
  {
    // Generate a new one
    Random::generatePseudo( &currentBundleId, sizeof( currentBundleId ) );
    hasCurrentBundleId = true;
  }

  return currentBundleId;
}

Writer::Compressor::Compressor( Config const & configIn, Writer & writer,
                                sptr< Bundle::Creator > const & bundleCreator,
                                string const & fileName ):
  writer( writer ), bundleCreator( bundleCreator ), fileName( fileName ),
  config( configIn )
{
}

void * Writer::Compressor::Compressor::threadFunction() throw()
{
  try
  {
    bundleCreator->write( config, fileName, writer.encryptionKey );
  }
  catch( std::exception & e )
  {
    FAIL( "Bundle writing failed: %s", e.what() );
  }

  {
    Lock _( writer.runningCompressorsMutex );
    ZBACKUP_CHECK( writer.runningCompressors, "no running compressors" );
    --writer.runningCompressors;
    writer.runningCompressorsCondition.signal();
  }

  detach();

  // We're in detached thread, so no further cleanup is necessary
  delete this;

  return NULL;
}

Reader::Reader( Config const & configIn,
                EncryptionKey const & encryptionKey,
                ChunkIndex & index, string const & bundlesDir,
                size_t maxCacheSizeBytes ):
  config( configIn ), encryptionKey( encryptionKey ),
  index( index ), bundlesDir( bundlesDir ),
  // We need to have at least one cached reader, otherwise we would have to
  // unpack a bundle each time a chunk is read, even for consecutive chunks
  // in the same bundle
  cachedReaders(
      maxCacheSizeBytes < config.GET_STORABLE( bundle, max_payload_size ) ?
      1 : maxCacheSizeBytes / config.GET_STORABLE( bundle, max_payload_size ) )
{
  verbosePrintf( "Using up to %zu MB of RAM as cache\n",
                 maxCacheSizeBytes / 1048576 );
}

Bundle::Id const * Reader::getBundleId( ChunkId const & chunkId, size_t & size )
{
  uint32_t s;
  if ( Bundle::Id const * bundleId = index.findChunk( chunkId, &s ) )
  {
    size = s;
    return bundleId;
  }
  else
  {
    string blob = chunkId.toBlob();
    throw exNoSuchChunk( Utils::toHex( ( unsigned char const * ) blob.data(),
                                blob.size() ) );
  }
}

void Reader::get( ChunkId const & chunkId, string & data, size_t & size )
{
  if ( Bundle::Id const * bundleId = index.findChunk( chunkId ) )
  {
    Bundle::Reader & reader = getReaderFor( *bundleId );
    reader.get( chunkId.toBlob(), data, size );
  }
  else
  {
    string blob = chunkId.toBlob();
    throw exNoSuchChunk( Utils::toHex( ( unsigned char const * ) blob.data(),
                                blob.size() ) );
  }
}

Bundle::Reader & Reader::getReaderFor( Bundle::Id const & id )
{
  sptr< Bundle::Reader > & reader = cachedReaders.entry< Bundle::Reader >(
    string( ( char const * ) &id, sizeof( id ) ) );

  if ( !reader.get() )
  {
    // Load the bundle
    reader =
      new Bundle::Reader( Bundle::generateFileName( id, bundlesDir, false ),
                          encryptionKey );
  }

  return *reader;
}

}
