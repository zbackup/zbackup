#include <iostream>
#include <fstream>
#include <ostream>
#include <string>
#include <cstring>

#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#include <zlib.h>

#include "backupcreator.pb.h"


#define ALIGN (16*1024)
#define ALIGN_SIZE(size, align) ((size % align == 0) ? (size / align) : (size / align + 1))

char *readlink_malloc (const char *filename)
{
  int size = 100;
  char *buffer = NULL;

  while(1)
  {
    buffer = (char *)realloc(buffer, size);
    if(buffer == NULL)
      return NULL;
    
    int nchars = readlink(filename, buffer, size);
    if(nchars < 0)
    {
      free(buffer);
      return NULL;
    }
    if(nchars < size)
      return buffer;
    size *= 2;
    }
}
void outputAlign(uint64_t written, std::ostream *stream, uint32_t alignment)
{
  uint32_t alignToWrite = alignment - (written % alignment);
  if(alignment == alignToWrite)
    return;
  
  char *alignmentBuffer = (char *)malloc(alignToWrite);
  memset(alignmentBuffer, '\0', alignToWrite);
  if(alignmentBuffer == NULL)
  {
    std::cerr << "Unable to allocate alignmentBuffer" << std::endl;
    return;
  }
  
  stream->write(alignmentBuffer, alignToWrite);
  
  free(alignmentBuffer);
}
void skipBytes(std::istream *stream, uint64_t skip)
{
  char *buffer = (char *)malloc(skip);
  stream->read(buffer, skip);
  free(buffer);
}

void readFileTree(Backup *backup, uint32_t alignment)
{
  uint64_t currentPos = 0;
  while(true)
  {
    std::string path;
    
    std::getline(std::cin, path, '\0');
    
    if(std::cin.eof())
      break;
    
    if(std::cin.fail())
      break;
    
    struct stat64 statresult;
    
    if(lstat64(path.c_str(), &statresult))
    {
      std::cerr << "Error: unable to stat file " << path;
      continue;
    }
    // skip  fifos and sockets we can't restore them anyway
    if((statresult.st_mode & __S_IFMT == __S_IFSOCK) || (statresult.st_mode & __S_IFMT == __S_IFIFO))
    {
      std::cerr << "Skipped fifo or socket " << path << " " << statresult.st_mode << std::endl;
      continue;
    }
        
    Backup_Item *item = backup->add_item();
    item->set_path(path);
    item->set_owner(statresult.st_uid);
    item->set_group(statresult.st_gid);
    item->set_atime((uint64_t)statresult.st_atim.tv_sec);
    item->set_atimenano((uint64_t)statresult.st_atim.tv_nsec);
    item->set_mtime((uint64_t)statresult.st_mtim.tv_sec);
    item->set_mtimenano((uint64_t)statresult.st_mtim.tv_nsec);
    item->set_accessrights(statresult.st_mode & ~__S_IFMT);
    
    switch(statresult.st_mode & __S_IFMT) {
      case __S_IFDIR:
        item->mutable_directory();
        break;
      case __S_IFCHR:
      {
        Backup_Item_Device *dev = item->mutable_device();
        dev->set_deviceid(statresult.st_rdev);
        dev->set_block(false);
      }
        break;
      case __S_IFBLK:
      {
        Backup_Item_Device *dev = item->mutable_device();
        dev->set_deviceid(statresult.st_rdev);
        dev->set_block(true);
      }
        break;
      case __S_IFREG:
      {
        Backup_Item_File *file = item->mutable_file();
        file->set_position(currentPos);
        file->set_size(statresult.st_size);
        currentPos += ALIGN_SIZE(statresult.st_size, alignment);
      }
        break;
      case __S_IFLNK:
      {
        Backup_Item_Symlink *symlink = item->mutable_symlink();
        char *link = readlink_malloc(path.c_str());
        
        if(link == NULL)
        {
          std::cerr << "Unable to read link" << path << std::endl;
          return;
        }
        symlink->set_target(link);
        
        free(link);
      }
        break;
    }
    
  }
  
  // now we are on the end of the filelist here starts the checksums
  backup->set_checksumblockpos(currentPos);
}

uint64_t outputFileData(const std::string *path, std::ostream *stream, uint32_t &checksum)
{
  std::fstream file(path->c_str(), std::fstream::in | std::fstream::binary);
  
  uint64_t total = 0;
  
  checksum = 0;
  
  char buffer[4096];
  while(true) {
    file.read(buffer, 4096);
  
    uint64_t readed = file.gcount();
    if(readed == 0)
      break;
      
    stream->write(buffer, readed);
    total += readed;
    
    checksum = crc32(checksum, (Bytef const *)buffer, readed);
    
    if(stream->bad())
      std::cerr << "Write File Data, Badbit on stream" << std::endl;
  }
  file.close();
  return total;
}

void outputData(Backup *backup, std::ostream *stream, uint32_t alignment)
{
  uint64_t headerSize = (uint64_t)backup->ByteSize();
  void *backupHeader = malloc(headerSize);
  backup->SerializeToArray(backupHeader, headerSize);
  
  stream->write(reinterpret_cast<const char *>(&headerSize), sizeof(headerSize));
  stream->write(reinterpret_cast<const char *>(backupHeader), headerSize);

  uint32_t headerChecksum = crc32(0, (Bytef const *) backupHeader, headerSize);
  stream->write(reinterpret_cast<const char *>(&headerChecksum), sizeof(headerChecksum));
  
  free(backupHeader);
  
  // header length + 8 byte header length field & 4 byte header checksum size 
  outputAlign(headerSize + 8 + 4, stream, alignment);
  
  uint64_t posAlign = ALIGN_SIZE(headerSize + 8 + 4, alignment);
  
  ChecksumBlock checksumBlock;
  
  for(uint64_t i = 0; i < backup->item_size(); i++)
  {
    Backup_Item item = backup->item(i);
    if(!item.has_file())
      continue;
    
    uint32_t sum = 0;
    
    uint64_t written = outputFileData(&item.path(), stream, sum);
    
    std::cerr << "File: " << item.path() << " checksum: " << sum << std::endl;
    
    ChecksumBlock_Checksum *checksum = checksumBlock.add_checksum();
    checksum->set_id(i);
    checksum->set_sum(sum);
    
    if(written != item.file().size())
    {
      std::cerr << "File " << item.path() << " has changed size. Written " << written << " should be "<< item.file().size() << std::endl;
    }
    
    posAlign += ALIGN_SIZE(written, alignment);
    outputAlign(written, stream, alignment);
  }
  
  uint64_t checksumSize = (uint64_t)checksumBlock.ByteSize();
  void *checksumBuffer = malloc(checksumSize);
  backup->SerializeToArray(checksumBuffer, checksumSize);
  
  // write checksum Block size
  stream->write(reinterpret_cast<const char *>(&checksumSize), sizeof(checksumSize));
  
  // write checksum Block
  stream->write(reinterpret_cast<const char *>(checksumBuffer), checksumSize);
  
  free(checksumBuffer);
}

int backup(uint32_t alignment)
{
  Backup backup;
  backup.set_alignsize(alignment);
  readFileTree(&backup, alignment);
  
  outputData(&backup, &std::cout, alignment);
  return 0;
}

Backup *readHeader(std::istream *stream)
{
  uint64_t size = 0;
  
  stream->read(reinterpret_cast<char *>(&size), sizeof(size));
  
  void *buffer = malloc(size);
  if(buffer == NULL)
    return NULL;
  
  stream->read(reinterpret_cast<char *>(buffer), size);
  
  uint32_t checksum = 0;
  stream->read(reinterpret_cast<char *>(&checksum), sizeof(checksum));
  
  if(checksum != crc32(0, reinterpret_cast<const Bytef*>(buffer), size))
  {
    std::cerr << "Header checksum invalid";
    return NULL;
  }
  
  Backup *backup = new Backup();
  backup->ParseFromArray(buffer, size);
  
  uint32_t alignToSkip = backup->alignsize() - ((size + 8) % backup->alignsize());
  if(backup->alignsize() == alignToSkip)
    return backup;
  
  std::cerr << "Align to skip: " << alignToSkip << std::endl;
  skipBytes(stream, alignToSkip);
  
  return backup;
}

void setAttributes(Backup_Item *item)
{
  int err = chown(item->path().c_str(), item->owner(), item->group());
  if(err)
  {
    std::cerr << "Can't set owner/group path " << item->path() << " error: " << err << std::endl;
  }

  timespec times[2];
  times[0].tv_nsec = item->atimenano();
  times[0].tv_sec = item->atime();
  times[1].tv_nsec = item->mtimenano();
  times[1].tv_sec = item->mtime();
  err = utimensat(AT_FDCWD, item->path().c_str(), times, AT_SYMLINK_NOFOLLOW);
  if(err)
  {
    std::cerr << "Can't set access/modify times path " << item->path() << " error: " << err << std::endl;
  }
}

void createFileTree(Backup *backup)
{
  for(uint64_t i = 0; i < backup->item_size(); i++)
  {
    Backup_Item item = backup->item(i);
    if(!item.has_directory())
      continue;
    
    std::string path = item.path();
    struct stat64 statresult;
    
    if(!lstat64(path.c_str(), &statresult))
    {
      std::cerr << path << " already exists, skipping." << std::endl;
    }
    else
    {
      int err = mkdir(path.c_str(), item.accessrights());
      if(err)
      {
        std::cerr << "Can't create path " << path << " error: " << err << std::endl;
      }
      else
      {
        setAttributes(&item);
      }
    }
  }
}

void writeFileData(Backup_Item *item, std::istream *stream, uint32_t alignsize, uint64_t startOffset, uint32_t &checksum)
{
  std::fstream file(item->path().c_str(), std::fstream::out | std::fstream::binary);
  
  int64_t posDiff = (item->file().position() * alignsize) - (stream->tellg() - (int64_t)startOffset);
  
  if(posDiff < 0)
  {
    std::cerr << "Error posDiff: " << posDiff << std::endl;
  }
  else if(posDiff > 0)
  {
    std::cerr << "Skip posDiff: " << posDiff << std::endl;
    skipBytes(stream, posDiff);
  }
  
  uint64_t missing = item->file().size();
  
  checksum = 0;
  
  char buffer[4096];
  while(true) {
    stream->read(buffer, ((missing > 4096) ? 4096 : missing));
  
    uint64_t readed = stream->gcount();
    if(readed == 0)
      break;
      
    file.write(buffer, readed);
    missing -= readed;
    
    checksum = crc32(checksum, reinterpret_cast<const Bytef*>(buffer), readed);
  }
  if(missing != 0)
    std::cerr << "Unable to write " << missing << " bytes in file: " << item->path() << std::endl;
  file.close();
  
  setAttributes(item);
  
  uint32_t alignToSkip = alignsize - (item->file().size() % alignsize);
  
  std::cerr << "File: " << item->path() << " pos: " << item->file().position() << " written: "<< item->file().size() << " Align to skip: " << alignToSkip << std::endl;
  
  if(alignsize == alignToSkip)
    return;
  
  skipBytes(stream, alignToSkip);
}

void createFiles(Backup *backup, std::istream *stream)
{
  uint64_t startoffset = stream->tellg();
  
  for(uint64_t i = 0; i < backup->item_size(); i++)
  {
    Backup_Item item = backup->item(i);
    if(item.has_directory())
      continue;
    
    std::string path = item.path();
    struct stat64 statresult;
    
    ChecksumBlock cb;
    
    if(!lstat64(path.c_str(), &statresult))
    {
      std::cerr << path << " already exists, skipping." << std::endl;
    }
    else
    {
      if(item.has_file())
      {
        uint32_t checksum = 0;
        writeFileData(&item, stream, backup->alignsize(), startoffset, checksum);
        
        ChecksumBlock_Checksum *c = cb.add_checksum();
        c->set_id(i);
        c->set_sum(checksum);
      }
      else if(item.has_device())
      {
        std::cerr << "Create device unimplemented" << std::endl;
      }
      else if(item.has_symlink())
      {
        std::cerr << "Create symlink unimplemented" << std::endl;
      }
    }
  }
}

int restore()
{
  Backup *backup = readHeader(&std::cin);
  
  std::cerr << "Size: " << backup->item_size() << std::endl;
  
  // stage 1 create path tree
  createFileTree(backup);
  
  createFiles(backup, &std::cin);
  
}

void printHelp()
{
  std::cerr << "Use: backupcreator [backup | restore] [align size in bytes]" << std::endl;
}


int main(int argc, const char* argv[])
{
  GOOGLE_PROTOBUF_VERIFY_VERSION;
  uint32_t align = ALIGN;
  
  if(argc < 2)
  {
    printHelp();
    return 1;
  }
  
  if(strncmp("backup", argv[1], sizeof("backup")) == 0)
  {
    if(argc == 3)
    {
      align = atol(argv[2]);
    }
    return backup(align);
  }
  else if(strncmp("restore", argv[1], sizeof("restore")) == 0)
  {
    return restore();
  }
  else
  {
    printHelp();
    return 1;
  }
  
  google::protobuf::ShutdownProtobufLibrary();
}