#ifndef DISKCONTROLLER_HPP
#define DISKCONTROLLER_HPP

#include <cstdint>
#include <head.hpp>
#include <unistd.h>

class DiskController {
public:
  int numberDisks;
  int numberTracks;
  int numberSectors;
  int numberBytes;
  int sectorsBlock;

  Head *head;

  DiskController(int numberDisks, int numberTracks, int numberSectors, int numberBytes, int sectorsBlock);
  ~DiskController();

  void moveHeadTo(int disk, int surface, int track, int sector);
  void advanceHead();

  void nextSector();
  void nextTrack();
  void nextSurface();
  void nextDisk();

  void moveToBlock(uint32_t blockID);

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
