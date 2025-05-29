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

bool DiskController::nextTrack() {
  if (head->currentTrack + 1 >= numberTracks) {
    return false;
  }

  head->currentTrack++;
  return true;
}

bool DiskController::afterTrack() {
  if (head->currentSector - 1 < 0) {
    return false;
  }

  head->currentSector--;
  return true;
}

bool DiskController::nextSurface() {
  if (head->currentSurface + 1 >= 2) {
    head->currentSurface = 0;
    return nextDisk();
  }

  head->currentSurface++;
  return true;
}

bool DiskController::afterSurface() {
  if (head->currentSurface - 1 < 0) {
    head->currentSurface = 1;
    return afterDisk();
  }

  head->currentSurface--;
  return true;
}

bool DiskController::nextDisk() {
  if (head->currentDisk + 1 >= numberDisks)
    return false;

  head->currentDisk++;
  return true;
}

bool DiskController::afterDisk() {
  if (head->currentDisk - 1 < 0)
    return false;

  head->currentDisk--;
  return true;
}

void DiskController::moveToSector(uint32_t sectorID) {
  head->currentDisk = sectorID / (2 * numberTracks * numberSectors);
  head->currentSurface =
      (sectorID / numberSectors) % (2 * numberTracks) / numberTracks;
  head->currentTrack =
      (sectorID / numberSectors) % (2 * numberTracks) % numberTracks;
  head->currentSector = sectorID % numberSectors;
}

bool DiskController::loadBlocks(uint32_t startSector, uint16_t *block) {
  uint32_t nextSector = startSector;

  for (uint32_t i = 0; i < sectorsBlock; ++i) {
    moveToSector(nextSector);
    block[i] = head->openCurrentSectorFD();

    lseek(head->currentFd, -4, SEEK_END);

    if (readBinary(nextSector) != sizeof(nextSector)) {
      perror("Error al leer el sector");
      return false;
    }

    if (nextSector == 0xFFFFFFFF) {
      // Rellenar el resto del bloque con 0xFFFF
      for (uint32_t j = i + 1; j < sectorsBlock; ++j) {
        block[j] = 0xFFFF;
      }
      return true;
    }
  }
  return true;
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

void DiskController::init() {
  head->resetPosition();
  head->openCurrentSectorFD();

  // Inicializar el sector de arranque
  initializeBootSector();

  // Inicializar el bit map
  initializeBitMap();
  markSector(0, true);
  // Describir la estructura del disco
  describeStructure();
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
  // total sectors = numberDisks * 2 * numberTracks * numberSectors
  uint32_t totalBytes =
      ((numberDisks * 2 * numberTracks * numberSectors) + 7) / 8;

  uint32_t sectorsUsed = (totalBytes + numberBytes - 3) / (numberBytes - 4);
  uint32_t sectorsBitMap[sectorsUsed];
  uint32_t sector = 1;

  uint32_t writtenBytes = 0;
  nextSurface();
  blockBitmap = sectorsBitMap[0] = getSectorID();
  head->openCurrentSectorFD();

  while (writtenBytes < totalBytes) {
    for (uint32_t i = 0; i < numberBytes - 4 && writtenBytes < totalBytes;
         ++i) {
      uint8_t bitmapByte = 0x00;
      writeBinary(bitmapByte);
      ++writtenBytes;
    }

    if (writtenBytes < totalBytes) {
      if (head->currentSurface + 1 < 2 || head->currentDisk + 1 < numberDisks) {
        nextSurface();

        uint32_t nextSector = getSectorID();

        writeBinary(nextSector);
        head->openCurrentSectorFD();

        sectorsBitMap[sector] = nextSector;
      } else {

        uint32_t nextSector = (head->currentSector + 1) % numberSectors;
        writeBinary(nextSector);

        head->moveTo(0, 0, 0, nextSector);
        head->openCurrentSectorFD();

        sectorsBitMap[sector] = nextSector;
      }

      ++sector;
    } else {

      uint32_t nullSector = 0xFFFFFFFF;
      writeBinary(nullSector);
    }
  }

  for (uint32_t i = 0; i < sectorsUsed; ++i) {
    markSector(sectorsBitMap[i], true);
  }
}

bool DiskController::markSector(uint32_t sectorID, bool used) {
  // byteIndex: índice del byte en el bitmap
  uint32_t byteIndex = sectorID / 8;
  // bitPos: posición del bit dentro del byte
  uint8_t bitPos = sectorID % 8;
  // sectorsOffset: índice del sector en el bitmap
  uint32_t sectorOffset = byteIndex / (numberBytes - 4);
  // byteOffset: indice del byte dentro del sector
  uint32_t byteOffset = byteIndex % (numberBytes - 4);

  uint16_t block[sectorsBlock];

  for (uint32_t nextSector = blockBitmap, i = 0;
       i <= sectorOffset / sectorsBlock;
       ++i) {

    if (i == 0) {
      if (!loadBlocks(nextSector, block))
        return false;
    } else {

      head->currentFd = block[sectorsBlock - 1];
      lseek(head->currentFd, -4, SEEK_END);
      readBinary(nextSector);

      if (nextSector == 0xFFFFFFFF) {
        perror("No hay suficientes bloques para marcar el sector como usado.");
        return false;
      }

      if (!loadBlocks(nextSector, block))
        return false;
    }
  }

  head->currentFd = block[sectorOffset % sectorsBlock];

  lseek(head->currentBlock, byteOffset, SEEK_SET);

  uint8_t bitmapByte;
  readBinary(bitmapByte);

  if (used) {
    bitmapByte |= (1 << bitPos);
  } else {
    bitmapByte &= ~(1 << bitPos);
  }

  lseek(head->currentFd, byteOffset, SEEK_SET);
  writeBinary(bitmapByte);

  return true;
}
