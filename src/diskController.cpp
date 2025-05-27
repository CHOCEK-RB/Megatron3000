#include "const.cpp"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <diskController.hpp>
#include <head.hpp>
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

void DiskController::contructDisk() {
  head->resetPosition();
  head->openCurrentSectorFD();

  initializeBootSector();
  initializeFAT();
}

void DiskController::moveToBlock(uint32_t blockID) {

  uint32_t sectorIndex = blockID * sectorsBlock;

  int sectorsPerSurface = numberTracks * numberSectors;
  int sectorsPerDisk = 2 * sectorsPerSurface;

  int disk = sectorIndex / sectorsPerDisk;
  sectorIndex %= sectorsPerDisk;

  int surface = sectorIndex / sectorsPerSurface;
  sectorIndex %= sectorsPerSurface;

  int track = sectorIndex / numberSectors;
  int sector = sectorIndex % numberSectors;

  head->moveTo(disk, surface, track, sector);
  head->currentBlock = blockID;
  head->openCurrentSectorFD();
}

void DiskController::nextBlockFree(bool consecutive) {
  moveToBlock(1);

  if (consecutive) {
    uint32_t nextBlock = readFATEntry(head->currentBlock);
    while (nextBlock != 0 && nextBlock != 0xFFFFFFFF) {
      head->currentBlock = nextBlock;
      nextBlock = readFATEntry(head->currentBlock);
    }

    moveToBlock(head->currentBlock);
  } else {
    uint64_t totalSectors = numberDisks * 2 * numberTracks * numberSectors;

    while (true) {
      nextTrack();

      uint64_t newSector =
          head->currentDisk * 2 * numberTracks * numberSectors +
          head->currentSurface * numberTracks * numberSectors +
          head->currentTrack * numberSectors;

      uint64_t newBlockID = newSector / sectorsBlock;

      uint32_t nextBlock = readFATEntry(newBlockID);
      if (nextBlock == 0 || nextBlock == 0xFFFFFFFF) {
        head->currentBlock = newBlockID;
        moveToBlock(newBlockID);
        break;
      }
    }
  }
}

void DiskController::describeStructure() {
  printf("Capacidad del disco: %d\n",
         numberDisks * numberTracks * numberSectors * numberBytes);
  printf("Capacidad por bloque: %d\n", sectorsBlock * numberBytes);
  printf("Nro. de Bloques por pista: %d\n", numberSectors / sectorsBlock);
  printf("Nro. de Bloques por disco: %d\n",
         numberDisks * 2 * numberTracks * (numberSectors / sectorsBlock));

  // Tree structure
  printf("Estructura del disco:\n");
  printf("Megatron 3000:\n");
  for (uint32_t d = 0; d < numberDisks; ++d) {
    if (d == numberDisks - 1)
      printf("└──%d:\n", d);
    else
      printf("├──%d:\n", d);
    for (uint32_t s = 0; s < 2; ++s) {
      if (s == 1)
        printf("│  └──%d:\n", s);
      else
        printf("│  ├──%d:\n", s);
      for (uint32_t t = 0; t < numberTracks; ++t) {
        if (t == numberTracks - 1)
          printf("│  │  └──%d:\n", t);
        else
          printf("│  │  ├──%d:\n", t);
        for (uint32_t sec = 0; sec < numberSectors; ++sec) {
          if (sec == numberSectors - 1)
            printf("│  │  │  └──%d.dat\n", sec);
          else
            printf("│  │  │  ├───%d.dat\n", sec);
        }
      }
    }
  }
}

void DiskController::initializeBootSector() {
  moveToBlock(0);
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

  uint32_t totalBlocks =
      (numberDisks * 2 * numberTracks * numberSectors) / sectorsBlock;
  uint32_t entriesPerBlock = (numberBytes * sectorsBlock) / sizeof(uint32_t);
  uint8_t fatBlocks = ceil((totalBlocks + 0.0) / entriesPerBlock);

  printf("FAT: %d bloques\n", fatBlocks);

  // 4 bytes en 0 para bloques libres
  uint32_t empty = 0;
  uint32_t written = 0;

  for (uint8_t b = 0; b < fatBlocks; ++b) {
    moveToBlock(1 + b);
    for (uint32_t i = 0; i < entriesPerBlock && written < totalBlocks; ++i) {
      if (written < fatBlocks - 1 && written != 0) {
        writeBinary(written + 1);
      } else {
        writeBinary(empty);
      }
      ++written;
    }
  }

  moveToBlock(1);

  lseek(head->currentFd, 0, SEEK_SET);
  writeBinary(0xFFFFFFFF); // Bloque Boot
  lseek(head->currentFd, fatBlocks * sizeof(uint32_t), SEEK_SET);
  writeBinary(0xFFFFFFFF); // Último bloque de la FAT

  head->resetPosition();
  head->openCurrentSectorFD();

  lseek(head->currentFd, POS_METADATA, SEEK_SET);
  // Inicio del bloque Metadata
  writeBinary(fatBlocks);
  // Número de bloques que ocupan los metadatos
  writeBinary(4);
  // Inicio del bloque de datos reales
  writeBinary(fatBlocks + 4);
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
