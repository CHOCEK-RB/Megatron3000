#include "const.cpp"
#include <cstdint>
#include <cstdio>
#include <diskController.hpp>
#include <fcntl.h>
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
  head->currentSector = (head->currentSector + 1) % numberSectors;
}

void DiskController::nextTrack() {
  head->currentTrack++;

  if (head->currentTrack >= numberTracks) {
    head->currentTrack = -1;
  }
}

void DiskController::afterTrack() { head->currentSector--; }

void DiskController::nextSurface() {
  head->currentSurface++;

  if (head->currentSurface >= 2) {
    head->currentSurface = 0;
    nextDisk();
  }
}

void DiskController::afterSurface() {
  head->currentSurface--;
  if (head->currentSurface < 0) {
    head->currentSurface = 1;
    afterDisk();
  }
}

void DiskController::nextDisk() {
  head->currentDisk++;

  if (head->currentDisk >= numberDisks) {
    head->currentDisk = -1;
  }
}

void DiskController::afterDisk() { head->currentDisk--; }

void DiskController::moveToSector(uint32_t sectorID) {
  head->currentDisk = sectorID / (2 * numberTracks * numberSectors);
  head->currentSurface =
      (sectorID / numberSectors) % (2 * numberTracks) / numberTracks;
  head->currentTrack =
      (sectorID / numberSectors) % (2 * numberTracks) % numberTracks;
  head->currentSector = sectorID % numberSectors;
}

void DiskController::loadBlocks(uint32_t startSector, uint16_t *block) {
  uint32_t nextSector = startSector;

  for (uint32_t i = 0; i < sectorsBlock; ++i) {
    moveToSector(nextSector);
    block[i] = head->openCurrentSectorFD();

    lseek(head->currentFd, -4, SEEK_END);

    readBinary(nextSector);

    if (nextSector == 0xFFFFFFFF) {
      for (uint32_t j = i; j < sectorsBlock; ++j) {
        block[j] = 0xFFFF;      }
      return;
    }
  }
}

void DiskController::describeStructure() {
  printf("Capacidad del disco (mb): %d\n",
         numberDisks * numberTracks * numberSectors * numberBytes /
             (1024 * 1024));
  printf("Capacidad por bloque (mb): %d\n",
         sectorsBlock * numberBytes / (1024 * 1024));
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

  // 1 byte: sector de inicio para bit map
  uint8_t empty = 0;
  writeBinary(empty + 1);

  // 1 byte: sector de inicio para metadatos
  writeBinary(empty);

  // 1 byte: bloque de inicio para datos reales
  writeBinary(empty);

  // 1 byte: tipo de atributos (0 = estáticos, 1 = variados)
  writeBinary(empty);
}

void DiskController::initializeBitMap() {
  uint32_t totalSectors = numberDisks * 2 * numberTracks * numberSectors;
  uint32_t totalBytes = (totalSectors + 7) / 8;

  uint32_t writtenBytes = 0;

  nextSurface();
  head->openCurrentSectorFD();

  while (writtenBytes < totalBytes) {
    for (uint32_t i = 0; i < numberBytes - 4 && writtenBytes < totalBytes; ++i) {
      uint8_t bitmapByte = 0x00;
      writeBinary(bitmapByte);
      ++writtenBytes;
    }

    if (writtenBytes < totalBytes) {
      if (head->currentSurface + 1 < 2 || head->currentDisk + 1 < numberDisks) {
        nextSurface();

        uint32_t nextSector =
            head->currentDisk * 2 * numberTracks * numberSectors +
            head->currentSurface * numberTracks * numberSectors +
            head->currentTrack * numberSectors + head->currentSector + 1;

        writeBinary(nextSector);
        head->openCurrentSectorFD();
      } else {

        uint32_t nextSector = (head->currentSector + 1) % numberSectors;
        writeBinary(nextSector);

        head->moveTo(0, 0, 0, nextSector);
        head->openCurrentSectorFD();
      }
    } else {

      uint32_t nullSector = 0xFFFFFFFF;
      writeBinary(nullSector);
    }
  }
}


void DiskController::markSectorUsed(uint32_t sectorID) {

  uint32_t byteIndex = sectorID / 8;
  uint8_t bitPos = sectorID % 8;

  uint32_t sectorOffset = byteIndex / (numberBytes - 4);
  uint32_t byteOffset = byteIndex % (numberBytes - 4);

  uint16_t block[sectorsBlock];

  loadBlocks(1, block);

  for (uint32_t i = 0; i < sectorOffset; ++i) {
    if (head->currentSurface + 1 < 2 || head->currentDisk + 1 < numberDisks) {
      nextSurface();
    } else {
      head->moveTo(0, 0, 0, (head->currentSector + 1) % numberSectors);
    }
  }

  head->openCurrentSectorFD();

  // Ir al byte correspondiente dentro del archivo
  lseek(head->currentFd, byteOffset, SEEK_SET);

  // Leer, modificar y escribir el byte
  uint8_t bitmapByte;
  readBinary(bitmapByte);
  bitmapByte |= (1 << bitPos);
  lseek(head->currentFd, byteOffset, SEEK_SET);
  writeBinary(bitmapByte);
}

