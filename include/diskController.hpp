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

  unsigned int blockFAT;
  unsigned int blockMetadata;

  Head *head;

  DiskController(int numberDisks, int numberTracks, int numberSectors, int numberBytes, int sectorsBlock);
  ~DiskController();

  void nextSector();

  void nextTrack();
  void afterTrack();

  void nextSurface();
  void afterSurface();

  void nextDisk();
  void afterDisk();

  void nextCylinder();

  void moveToSector(uint32_t sectorID);

  void loadBlocks(uint32_t startSector, uint16_t *block);
  void describeStructure();
  
  void init();
  void initializeBootSector();
  void initializeBitMap();
  void initializeMetadata();

  void markSectorUsed(uint32_t sectorID);
  
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
