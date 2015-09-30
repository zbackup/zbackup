#include <iostream>
#include <fstream>
#include <ostream>
#include <string>

#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>

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
    item->set_ctime((uint64_t)statresult.st_ctim.tv_sec);
    item->set_ctimenano((uint64_t)statresult.st_ctim.tv_nsec);
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
  
}

uint64_t outputFileData(const std::string *path, std::ostream *stream)
{
  std::fstream file(path->c_str(), std::fstream::in | std::fstream::binary);
  
  uint64_t total = 0;
  
  char buffer[4096];
  while(true) {
    file.read(buffer, 4096);
  
    uint64_t readed = file.gcount();
    if(readed == 0)
      break;
      
    stream->write(buffer, readed);
    total += readed;
  }
  file.close();
  return total;
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

void outputData(Backup *backup, std::ostream *stream, uint32_t alignment)
{
  std::string backupString = backup->SerializeAsString();
  uint64_t size = (uint64_t)backupString.length();
  
  stream->write(reinterpret_cast<const char *>(&size), sizeof(size));
  *stream << backupString;
  
  outputAlign(backupString.length() + 8, stream, alignment);
  
  uint64_t posAlign = ALIGN_SIZE(backupString.length() + 8, alignment);
  
  for(uint64_t i = 0; i < backup->item_size(); i++)
  {
    Backup_Item item = backup->item(i);
    if(!item.has_file())
      continue;
    
    uint64_t written = outputFileData(&item.path(), stream);
    
    if(written != item.file().size())
    {
      std::cerr << "File " << item.path() << " has changed size. Written " << written << " should be "<< item.file().size() << std::endl;
    }
    
    posAlign += ALIGN_SIZE(written, alignment);
    outputAlign(written, stream, alignment);
  }
}

int backup(uint32_t alignment)
{
  Backup backup;
  backup.set_alignsize(alignment);
  readFileTree(&backup, alignment);
  
  outputData(&backup, &std::cout, alignment);
  return 0;
}



void printHelp()
{
  std::cout << "Use: backupcreator [backup | restore] [align size in bytes]" << std::endl;
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
  
  if(argv[1] == "backup")
  {
    if(argc == 3)
    {
      align = atol(argv[2]);
    }
    return backup(align);
  }
  else if(argv[1] == "restore")
  {
  }
  else
  {
    printHelp();
    return 1;
  }
  
  google::protobuf::ShutdownProtobufLibrary();
}