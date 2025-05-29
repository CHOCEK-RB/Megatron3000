#ifndef DISKCONTROLLER_HPP
#define DISKCONTROLLER_HPP

#include <cstdint>
#include <cstdio>
#include <head.hpp>
#include <unistd.h>

class DiskController {
public:
  unsigned int numberDisks;
  unsigned int numberTracks;
  unsigned int numberSectors;
  unsigned int numberBytes;
  unsigned int sectorsBlock;

  unsigned int blockBitmap;
  unsigned int blockMetadata;

  Head *head;

  DiskController(int numberDisks, int numberTracks, int numberSectors, int numberBytes, int sectorsBlock);
  ~DiskController();

  uint32_t getSectorID() const {
    return head->currentDisk * 2 * numberTracks * numberSectors +
           head->currentSurface * numberTracks * numberSectors +
           head->currentTrack * numberSectors + head->currentSector;
  }

  void nextSector();

  bool nextTrack();
  bool afterTrack();

  bool nextSurface();
  bool afterSurface();

  bool nextDisk();
  bool afterDisk();

  void nextCylinder();

  void moveToSector(uint32_t sectorID);

  bool loadBlocks(uint32_t startSector, uint16_t *block);
  void describeStructure();

  void init();
  void initializeBootSector();
  void initializeBitMap();
  void initializeMetadata();

  bool markSector(uint32_t sectorID, bool used = true);

  template <typename T>

  int writeBinary(const T &content, int fd = -1) {
    if (fd != -1) {
      head->currentFd = fd;
    }
    return write(head->currentFd, &content, sizeof(T));
  }

  template <typename T>

  int readBinary(T &store, int fd = -1) {
    if (fd != -1) {
      head->currentFd = fd;
    }
    return read(head->currentFd, &store, sizeof(T));
  }
};

#endif // !DISKCONTROLLER_HPP
