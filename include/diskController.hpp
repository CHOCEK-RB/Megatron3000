#ifndef DISKCONTROLLER_HPP
#define DISKCONTROLLER_HPP

#include <cstdint>
#include <cstdio>
#include <head.hpp>
#include <unistd.h>

struct Sector {
  uint8_t *data = nullptr;
  size_t size = 0;
  uint32_t sectorID = 0;
  bool modified = false;
  bool isValid = false;
};

struct SectorInfo {
  uint32_t headId = -1;
  uint32_t currentTrack = -1;
  uint32_t currentSector = -1;
};

class DiskController {
public:
  unsigned int numberDisks;
  unsigned int numberTracks;
  unsigned int numberSectors;
  unsigned int numberBytes;
  unsigned int sectorsBlock;

  bool loadBitMap;
  uint32_t startSectorBitmap;
  uint8_t blocksPerBitMap;

  bool loadMetada;
  uint32_t startSectorMetadata;
  uint8_t blocksPerMetadata;

  Sector *block;
  int sectorsLoaded;

  int fdBlock;

  Head *head;

  DiskController(int numberDisks,
                 int numberTracks,
                 int numberSectors,
                 int numberBytes,
                 int sectorsBlock);
  ~DiskController();

  uint32_t getSectorID(uint16_t headId);
  uint32_t getSectorID(SectorInfo &sector);
  SectorInfo getSectorInfo(uint32_t sectorID);
  uint32_t nextSectorFree(Sector *block,
                          uint16_t currentSector,
                          bool consecutive = true);

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

  bool markBlock(Sector *block, uint32_t startSectorBlock, bool used = true);
  bool markSector(uint32_t sectorID, bool used = true);
  bool createFile(const char *fileName);
  bool modifyMetadata(const char *fileName, uint32_t newMetadata);
  char *searchFile(const char *fileName);

  void createSchemaFile();

  template <typename T>

  int writeBinary(const T &content, uint16_t headId, int fd = -1) {
    if (fd != -1) {
      return write(fd, &content, sizeof(T));
    }

    return write(head->heads[headId], &content, sizeof(T));
  }

  template <typename T>

  int readBinary(T &store, uint16_t headId, int fd = -1) {
    if (fd != -1) {
      return read(fd, &store, sizeof(T));
    }

    return read(head->heads[headId], &store, sizeof(T));
  }
};

#endif // !DISKCONTROLLER_HPP
