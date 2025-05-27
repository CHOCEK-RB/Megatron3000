#ifndef DISKCONTROLLER_HPP
#define DISKCONTROLLER_HPP

#include <cstdint>
#include <head.hpp>
#include <unistd.h>

class DiskController {
public:
  unsigned int numberDisks;
  unsigned int numberTracks;
  unsigned int numberSectors;
  unsigned int numberBytes;
  unsigned int sectorsBlock;

  Head *head;

  DiskController(int numberDisks, int numberTracks, int numberSectors, int numberBytes, int sectorsBlock);
  ~DiskController();

  void nextSector();
  void nextTrack();
  void nextSurface();
  void nextDisk();

  void moveToBlock(uint32_t blockID);
  void nextBlockFree(bool consecutive = false);

  void contructDisk();
  void describeStructure();

  void initializeBootSector();
  void initializeFAT();
  void writeFATEntry(uint32_t sectorID, uint32_t nextSectorID);
  uint32_t readFATEntry(uint32_t sectorID);

  void createRelation(const char *name, int sizeBytes);
  void listRelations();

  template <typename T>

  int writeBinary(const T &content) {
    return write(head->currentFd, &content, sizeof(T));
  }

  template <typename T>

  int readBinary(T &store) {
    return read(head->currentFd, &store, sizeof(T));
  }
};

#endif // !DISKCONTROLLER_HPP
