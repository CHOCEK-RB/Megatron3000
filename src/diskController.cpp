#include "const.cpp"
#include <cstdint>
#include <cstdio>
#include <cstring>
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

  head = new Head(numberDisks);

  block = new Sector[sectorsBlock];
  sectorsLoaded = 0;

  fdBlock = open(PATH_BLOCK, O_RDWR | O_CREAT | O_TRUNC, 0644);
};

DiskController::~DiskController() {
  delete head;
  freeBlock();
  delete[] block;
}

uint32_t DiskController::getSectorID(uint16_t headId, int track, int sector) {
  if (track != -1 && sector != -1) {
    return headId * numberTracks * numberSectors + track * numberSectors +
           sector;
  }

  return headId * numberTracks * numberSectors +
         head->currentTrack * numberSectors + head->currentSector;
}

void DiskController::nextSector() {
  head->currentSector = (head->currentSector + 1) % numberSectors;
}

uint32_t DiskController::nextSectorFree(uint16_t currentSector,
                                        bool consecutive) {
  freeBlock();
  loadBlocks(blockBitmap, block);
  loadBitMap = true;

  if (consecutive) {
    // Verficar desde el cilindro actual hacia adelante
    int surface =
        (currentSector / numberSectors) % (2 * numberTracks) / numberTracks;
    int track =
        (currentSector / numberSectors) % (2 * numberTracks) % numberTracks;
    int sector = currentSector % numberSectors;

    for (uint32_t i = track; i < numberTracks; ++i) {
      for (uint32_t j = sector; j < numberSectors; ++j) {
        for (uint32_t k = surface; k < numberDisks * 2; ++k) {
          uint32_t nextSector = getSectorID(k, i, j);

          uint32_t byteIndex = nextSector / 8;
          uint8_t bitPos = nextSector % 8;
          uint32_t sectorOffset = byteIndex / (numberBytes - 4);
          uint32_t byteOffset = byteIndex % (numberBytes - 4);

          uint32_t posBlock = sectorOffset % sectorsBlock;

          uint8_t bitmapByte = block[posBlock].data[byteOffset];

          if ((bitmapByte & (1 << bitPos)) == 0)
            return nextSector;
        }
        surface = 0;
      }
      sector = 0;
    }

    return 0xFFFFFFFF;
  }

  // Verificar por superficies en todos los cilindros
  uint32_t surface = 0;
  uint32_t track = 0;
  uint32_t sector = 0;

  for (uint32_t i = surface; i < numberDisks * 2; ++i) {
    for (uint32_t j = track; j < numberTracks; ++j) {
      for (uint32_t k = sector; k < numberSectors; ++k) {
        uint32_t nextSector = getSectorID(i, j, k);

        uint32_t byteIndex = nextSector / 8;
        uint8_t bitPos = nextSector % 8;
        uint32_t sectorOffset = byteIndex / (numberBytes - 4);
        uint32_t byteOffset = byteIndex % (numberBytes - 4);

        uint32_t posBlock = sectorOffset % sectorsBlock;

        uint8_t bitmapByte = block[posBlock].data[byteOffset];

        if ((bitmapByte & (1 << bitPos)) == 0)
          return nextSector;
      }
      sector = 0;
    }
    track = 0;
  }

  return 0xFFFFFFFF;
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

uint16_t DiskController::moveToSector(uint32_t sectorID) {
  uint16_t platter = sectorID / (2 * numberTracks * numberSectors);
  uint16_t surface =
      (sectorID / numberSectors) % (2 * numberTracks) / numberTracks;
  head->currentTrack =
      (sectorID / numberSectors) % (2 * numberTracks) % numberTracks;
  head->currentSector = sectorID % numberSectors;

  return platter * 2 + surface;
}

uint16_t DiskController::getHeadId(uint32_t sectorID) {
  uint16_t platter = sectorID / (2 * numberTracks * numberSectors);
  uint16_t surface =
      (sectorID / numberSectors) % (2 * numberTracks) / numberTracks;

  return platter * 2 + surface;
}

bool DiskController::isInCurrentCilinder(uint32_t sectorID) {
  uint32_t track = (sectorID / numberSectors) % numberTracks;
  uint32_t sector = sectorID % numberSectors;
  return head->currentTrack == track && head->currentSector == sector;
}

bool DiskController::loadBlocks(uint32_t startSector, Sector *block) {
  if (loadBitMap) {
    loadBitMap = false;
  }

  uint32_t nextSector = startSector;

  int headId = moveToSector(nextSector);
  head->openCurrentSectorFD();

  for (uint32_t i = 0; i < sectorsBlock; ++i) {
    if (!isInCurrentCilinder(nextSector)) {
      if (i == 0)
        return false;
      break;
    }

    headId = moveToSector(nextSector);

    Sector &s = block[i];
    s.data = new uint8_t[numberBytes];
    s.size = read(head->heads[headId], s.data, numberBytes);
    s.headId = headId;
    s.modified = false;
    s.isValid = true;

    sectorsLoaded++;

    lseek(head->heads[headId], -4, SEEK_END);

    if (readBinary(nextSector, headId) != sizeof(nextSector)) {
      perror("Error al leer el siguiente sector");
      return false;
    }

    if (nextSector == 0xFFFFFFFF) {
      break;
    }
  }

  for (uint16_t i = 0; i < sectorsBlock; ++i) {
    Sector &s = block[i];
    if (s.isValid && s.headId == headId) {
      write(fdBlock, s.data, s.size);
    }
  }

  lseek(fdBlock, 0, SEEK_END);

  return true;
}

void DiskController::flushModifiedSectors(Sector *loadedSectors) {
  for (uint16_t i = 0; i < sectorsBlock; ++i) {
    Sector &s = loadedSectors[i];
    if (!s.isValid)
      continue;
    if (s.modified) {
      lseek(head->heads[s.headId], 0, SEEK_SET);
      write(head->heads[s.headId], s.data, s.size);
    }
  }
}

void DiskController::freeBlock() {
  if (loadBitMap) {
    loadBitMap = false;
  }

  for (uint16_t i = 0; i < sectorsBlock; ++i) {
    Sector &s = block[i];

    if (s.data != nullptr) {
      delete[] s.data;
      s.data = nullptr;
    }

    s.size = 0;
    s.headId = -1;
    s.modified = false;
    s.isValid = false;
  }

  sectorsLoaded = 0;
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
  // Inicializar el sector de metadatos
  initializeMetadata();
  // Describir la estructura del disco
  describeStructure();

  createFile("esquema.txt");
  createFile("titanic.txt");
  createFile("nepe.txt");
}

void DiskController::initializeBootSector() {
  head->resetPosition();
  head->openCurrentSectorFD();

  uint32_t h = 1, t = 0, sec = 0;
  // Es continuo al sector boot en su mismo cilindro
  loadBitMap = false;
  blockBitmap = getSectorID(1);
  uint32_t totalBytes =
      (numberDisks * 2 * numberTracks * numberSectors + 7) / 8;

  sectorsForBitmap = (totalBytes + numberBytes - 3) / (numberBytes - 4);

  for (int i = 0; i < sectorsForBitmap; ++i) {
    if (h + 1 < numberDisks * 2) {
      ++h;
    } else if (sec + 1 < numberSectors) {
      ++sec;
      h = 0;
    } else if (t + 1 < numberTracks) {
      ++t;
      h = 0;
    }
  }

  blockMetadata = getSectorID(h, t, sec);

  sectorsForMetadata = 1;

  // 4 bytes: número de discos
  writeBinary(numberDisks, 0);
  // 4 bytes: pistas por superficie
  writeBinary(numberTracks, 0);
  // 4 bytes: sectores por pista
  writeBinary(numberSectors, 0);
  // 4 bytes: bytes por sector
  writeBinary(numberBytes, 0);
  // 4 bytes: sectores por bloque
  writeBinary(sectorsBlock, 0);
  // 1 byte: sector de inicio para bit map
  uint8_t empty = 0;
  writeBinary(blockBitmap, 0);
  // 1 byte: número de sectores para bit map
  writeBinary(sectorsForBitmap, 0);
  // 1 byte: sector de inicio para metadatos
  writeBinary(blockMetadata, 0);
  // 1 byte: número de sectores para metadatos
  writeBinary(sectorsForMetadata, 0);
  // 1 byte: tipo de atributos (0 = estáticos, 1 = variados)
  writeBinary(empty, 0);
}

void DiskController::initializeBitMap() {
  // total sectors = numberDisks * 2 * numberTracks * numberSectors
  uint32_t totalBytes =
      ((numberDisks * 2 * numberTracks * numberSectors) + 7) / 8;

  uint32_t sectorsBitMap[sectorsForBitmap];
  uint32_t sector = 1;

  uint32_t writtenBytes = 0;
  sectorsBitMap[0] = blockBitmap;

  uint32_t headId = moveToSector(blockBitmap);
  head->openCurrentSectorFD();

  while (writtenBytes < totalBytes) {
    for (uint32_t i = 0; i < numberBytes - 4 && writtenBytes < totalBytes;
         ++i) {
      uint8_t bitmapByte = 0x00;
      writeBinary(bitmapByte, headId);
      ++writtenBytes;
    }

    if (writtenBytes < totalBytes) {
      if (headId < numberDisks * 2 * numberTracks - 1) {
        ++headId;
        uint32_t nextSector = getSectorID(headId);

        writeBinary(nextSector, headId - 1);

        sectorsBitMap[sector] = nextSector;
      } else {
        uint32_t nextSector = (head->currentSector + 1) % numberSectors;
        writeBinary(nextSector, headId);

        head->moveTo(0, nextSector);
        head->openCurrentSectorFD();

        sectorsBitMap[sector] = nextSector;
        headId = 0;
      }

      ++sector;
    } else {

      uint32_t nullSector = 0xFFFFFFFF;
      writeBinary(nullSector, headId);
    }
  }

  for (uint32_t i = 0; i < sectorsForBitmap; ++i) {
    markSector(sectorsBitMap[i], true);
  }
}

void DiskController::initializeMetadata() {
  uint32_t headId = moveToSector(blockMetadata);
  head->openCurrentSectorFD();
  // Caso donde metadata ocupa un unico sector
  for (uint32_t i = 0; i < numberBytes - 4; ++i) {
    uint8_t empty = 0;
    writeBinary(empty, headId);
  }

  uint32_t nullSector = 0xFFFFFFFF;
  writeBinary(nullSector, headId);

  markSector(blockMetadata, true);
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

  if (!loadBitMap) {
    // Suponinendo que todo el bitmap está en un bloque
    freeBlock();
    loadBlocks(blockBitmap, block);
  }

  uint32_t posBlock = sectorOffset % sectorsBlock;

  if (!block[posBlock].isValid) {
    perror("Error: el sector objetivo no está cargado en el bloque\n");
    return false;
  }

  uint8_t &bitmapByte = block[posBlock].data[byteOffset];

  if (used) {
    bitmapByte |= (1 << bitPos);
  } else {
    bitmapByte &= ~(1 << bitPos);
  }

  block[posBlock].modified = true;

  flushModifiedSectors(block);

  return true;
}

bool DiskController::createFile(const char *fileName) {
  char fileRegister[36] = {0};

  for (int i = 0; i < FILE_NAME_LENGTH && fileName[i] != '\0'; ++i) {
    fileRegister[i] = fileName[i];
  }

  uint32_t sectorID = nextSectorFree(0, false);

  if (sectorID == 0xFFFFFFFF) {
    perror("No hay espacio suficiente para crear el archivo");
    return false;
  }

  uint32_t metadata = 0;
  memcpy(fileRegister + 20, &sectorID, sizeof(uint32_t));
  memcpy(fileRegister + 24, &metadata, sizeof(uint32_t));
  memcpy(fileRegister + 28, &metadata, sizeof(uint32_t));
  memcpy(fileRegister + 32, &metadata, sizeof(uint32_t));

  if (!markSector(sectorID, true)) {
    perror("Error al marcar el sector como ocupado");
    return false;
  }

  if (!loadBlocks(blockMetadata, block)) {
    perror("No se pudo cargar el bloque de metadatos");
    return false;
  }

  for (uint16_t i = 0; i < sectorsBlock; ++i) {
    Sector &s = block[i];

    for (uint32_t offset = 0; offset <= numberBytes - 40; offset += 36) {
      if (s.data[offset] == 0) {
        memcpy(s.data + offset, fileRegister, 36);
        s.modified = true;
        flushModifiedSectors(block);
        return true;
      }
    }
  }

  perror("No hay espacio en el bloque de metadatos para registrar el archivo");
  return false;
}
