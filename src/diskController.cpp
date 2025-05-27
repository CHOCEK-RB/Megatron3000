#include <cmath>
#include <cstdint>
#include <diskController.hpp>
#include <head.hpp>
#include "const.cpp"
#include <unistd.h>

DiskController::DiskController(int numberDisks,
                               int numberTracks,
                               int numberSectors,
                               int numberBytes,
                               int sectorsBlock)
    : numberDisks(numberDisks), numberTracks(numberTracks),
      numberSectors(numberSectors), numberBytes(numberBytes),
      sectorsBlock(sectorsBlock) {
  head = new Head;
};

DiskController::~DiskController() { delete head; }

void DiskController::nextSector() {
  head->currentSector++;

  if (head->currentSector >= numberSectors) {
    head->currentSector = 0;
    nextTrack();
  }
}

void DiskController::nextTrack() {
  head->currentTrack++;

  if (head->currentTrack >= numberTracks) {
    head->currentTrack = 0;
    nextSurface();
  }
}

void DiskController::nextSurface() {
  head->currentSurface++;

  if (head->currentSurface >= 2) {
    head->currentSurface = 0;
    nextDisk();
  }
}

void DiskController::nextDisk() {
  head->currentDisk++;

  if (head->currentDisk >= numberDisks) {
    head->currentDisk = 0;
  }
}

void DiskController::moveToBlock(uint32_t blockID) {
  int blocksPerTrack = numberSectors / sectorsBlock;
  int blocksPerSurface = numberTracks * blocksPerTrack;
  int blocksPerDisk = 2 * blocksPerSurface;

  int disk = blockID / blocksPerDisk;
  blockID %= blocksPerDisk;

  int surface = blockID / blocksPerSurface;
  blockID %= blocksPerSurface;

  int track = blockID / blocksPerTrack;
  int blockInTrack = blockID % blocksPerTrack;

  int sector = blockInTrack * sectorsBlock;

  head->moveTo(disk, surface, track, sector);
  head->openCurrentSectorFD();
}

void DiskController::initializeBootSector() {

  head->resetPosition();
  head->openCurrentSectorFD();

  // 4 bytes: número de discos
  writeBinary(numberDisks);
  // 4 bytes: pistas por superficie
  writeBinary(numberTracks);
  // 4 bytes: sectores por pista
  writeBinary(numberSectors);
  // 4 bytes: bytes por sector
  writeBinary(numberBytes);
  // 4 bytes: sectores por bloque
  writeBinary(sectorsBlock);

  // 1 byte: bloque de inicio para FAT
  uint8_t empty = 0;
  writeBinary(empty + 1);

  // 1 byte: bloque de inicio para metadatos
  writeBinary(empty);
  // 1 byte: bloques que ocupan los metadatos
  writeBinary(empty);

  // 1 byte: bloque de inicio para datos reales
  writeBinary(empty);

  // 1 byte: tipo de atributos (0 = estáticos, 1 = variados)
  writeBinary(empty);
}

void DiskController::initializeFAT() {
  moveToBlock(1);

  uint16_t entriesPerBlock = (numberBytes * sectorsBlock) / sizeof(uint32_t);

  uint32_t totalBlocks =
      numberDisks * 2 * numberTracks * (numberSectors / sectorsBlock);

  uint8_t fatBlocks = ceil(totalBlocks + 0.0 / entriesPerBlock);

  // 4 bytes en 0 para bloques libres
  uint32_t empty = 0;
  uint32_t written = 0;

  for (uint8_t b = 0; b < fatBlocks; ++b) {
    moveToBlock(1 + b);
    for (uint32_t i = 0; i < entriesPerBlock && written < totalBlocks; ++i) {
      writeBinary(empty);
      ++written;
    }
  }

  head->resetPosition();
  head->openCurrentSectorFD();

  lseek(head->currentFd, POS_METADATA, SEEK_SET);
  writeBinary(fatBlocks - 1);
}

void DiskController::writeFATEntry(uint32_t blockID, uint32_t nextBlockID) {
  uint32_t entriesPerBlock = (numberBytes * sectorsBlock) / sizeof(uint32_t);

  uint32_t blockOffset = blockID / entriesPerBlock;
  uint32_t entryOffset = blockID % entriesPerBlock;

  moveToBlock(1 + blockOffset);
  lseek(head->currentFd, entryOffset * sizeof(uint32_t), SEEK_CUR);
  writeBinary(nextBlockID);
}

uint32_t DiskController::readFATEntry(uint32_t blockID) {
  uint32_t entriesPerBlock = (numberBytes * sectorsBlock) / sizeof(uint32_t);

  uint32_t blockOffset = blockID / entriesPerBlock;
  uint32_t entryOffset = blockID % entriesPerBlock;

  moveToBlock(1 + blockOffset);
  lseek(head->currentFd, entryOffset * sizeof(uint32_t), SEEK_CUR);

  uint32_t nextBlock;
  readBinary(nextBlock);
  return nextBlock;
}
