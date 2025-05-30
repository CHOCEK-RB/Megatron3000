#ifndef DISKCONTROLLER_HPP
#define DISKCONTROLLER_HPP

#include <cstdint>
#include <cstdio>
#include <head.hpp>
#include <unistd.h>

struct Sector {
  uint8_t *data = nullptr;
  size_t size = 0;
  int headId = -1;
  bool modified = false;
  bool isValid = false;
};

class DiskController {
public:
  unsigned int numberDisks;
  unsigned int numberTracks;
  unsigned int numberSectors;
  unsigned int numberBytes;
  unsigned int sectorsBlock;
  
  bool loadBitMap;
  unsigned int blockBitmap;
  uint16_t sectorsForBitmap;

  unsigned int blockMetadata;
  uint16_t sectorsForMetadata;
  
  Sector *block;
  int sectorsLoaded;

  int fdBlock;

  Head *head;

  DiskController(int numberDisks, int numberTracks, int numberSectors, int numberBytes, int sectorsBlock);
  ~DiskController();

  uint32_t getSectorID(uint16_t headId, int track = -1, int sector = -1);
  void nextSector();
  uint32_t nextSectorFree(uint16_t currentSector, bool consecutive = true);

  bool nextTrack();
  bool afterTrack();

  uint16_t moveToSector(uint32_t sectorID);
  uint16_t getHeadId(uint32_t sectorID);
  bool isInCurrentCilinder(uint32_t sectorID);

  bool loadBlocks(uint32_t startSector, Sector *block);
  void flushModifiedSectors(Sector *loadedSectors);
  void freeBlock();

  void describeStructure();

  void init();
  void initializeBootSector();
  void initializeBitMap();
  void initializeMetadata();

  bool markSector(uint32_t sectorID, bool used = true);
  bool createFile(const char *fileName);

  template <typename T>

  int writeBinary(const T &content, uint16_t headId, int fd = -1) {
    if (fd != -1) {
      return write(fd, &content, sizeof(T));
    }

    return write(head->heads[headId], &content, sizeof(T));
  }

  template <typename T>

  int readBinary(T &store, uint16_t headId,int fd = -1) {
    if (fd != -1) {
      return read(fd, &store, sizeof(T));
    }

    return read(head->heads[headId], &store, sizeof(T));
  }
};

#endif // !DISKCONTROLLER_HPP
