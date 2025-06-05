#include "const.cpp"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <diskController.hpp>
#include <fcntl.h>
#include <head.hpp>

#include <iostream>
#include <unistd.h>

DiskController::DiskController(int numberDisks,
                               int numberTracks,
                               int numberSectors,
                               int numberBytes,
                               int sectorsBlock)
    : numberDisks(numberDisks), numberTracks(numberTracks),
      numberSectors(numberSectors), numberBytes(numberBytes),
      sectorsBlock(sectorsBlock), loadBitMap(false), startSectorBitmap(0),
      blocksPerBitMap(0), loadMetada(false), startSectorMetadata(0),
      blocksPerMetadata(0), sectorsLoaded(0) {

  head = new Head(numberDisks);

  block = new Sector[sectorsBlock];
  for (unsigned int i = 0; i < sectorsBlock; ++i) {
    block[i].isValid = false;
    block[i].data = nullptr;
  }

  fdBlock = open(PATH_BLOCK, O_RDWR | O_CREAT | O_TRUNC, 0644);
}

DiskController::~DiskController() {
  delete head;
  freeBlock();
  delete[] block;
}
// headId representa una superficie en todo el disco
uint32_t DiskController::getSectorID(uint16_t headId) {
  return headId * numberTracks * numberSectors +
         head->currentTrack * numberSectors + head->currentSector;
}

uint32_t DiskController::getSectorID(SectorInfo &sector) {
  return sector.headId * numberTracks * numberSectors +
         sector.currentTrack * numberSectors + sector.currentSector;
}

uint32_t DiskController::nextSectorFree(Sector *block,
                                        uint16_t currentSector,
                                        bool consecutive) {
  if (consecutive) {
    // Verficar desde el cilindro actual hacia adelante
    int surface =
        (currentSector / numberSectors) % (2 * numberTracks) / numberTracks;
    int track =
        (currentSector / numberSectors) % (2 * numberTracks) % numberTracks;
    int sector = currentSector % numberSectors;

    for (uint32_t i = track; i < numberTracks; ++i) {
      for (uint32_t j = sector; j < numberSectors; j += sectorsBlock) {
        for (uint32_t k = surface; k < numberDisks * 2; ++k) {
          SectorInfo sector = {k, i, j};
          uint32_t nextSector = getSectorID(sector);

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
      for (uint32_t k = sector; k < numberSectors; k += 4) {
        SectorInfo sector = {i, j, k};
        uint32_t nextSector = getSectorID(sector);

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
  uint32_t track =
      (sectorID / numberSectors) % (2 * numberTracks) % numberTracks;
  uint32_t sector = sectorID % numberSectors;
  return head->currentTrack == track && head->currentSector == sector;
}

// Carga a memoria un bloque, para esto se pone el sector donde inicia

bool DiskController::loadBlocks(uint32_t startSectorID, Sector *block) {
  if (loadBitMap || loadMetada) {
    loadBitMap = false;
    loadMetada = false;
  }

  sectorsLoaded = 0;

  uint32_t startSectorBlock = startSectorID % numberSectors;

  for (uint8_t i = startSectorBlock, j = 0;
       i < numberSectors && j < sectorsBlock;
       ++i, ++j) {
    int headId = moveToSector(startSectorID + j);
    head->openCurrentSectorFD();

    Sector &s = block[j];
    s.data = new uint8_t[numberBytes];
    s.size = read(head->heads[headId], s.data, numberBytes);
    s.sectorID = startSectorID + j;
    s.modified = false;
    s.isValid = true;

    sectorsLoaded++;
  }

  for (uint16_t i = 0; i < sectorsLoaded; ++i) {
    Sector &s = block[i];
    write(fdBlock, s.data, s.size);
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
      uint32_t headId = moveToSector(s.sectorID);
      head->openCurrentSectorFD();
      lseek(head->heads[headId], 0, SEEK_SET);
      write(head->heads[headId], s.data, s.size);
    }
  }
}

void DiskController::freeBlock() {
  if (loadBitMap || loadMetada) {
    loadBitMap = false;
    loadMetada = false;
  }

  for (uint16_t i = 0; i < sectorsBlock; ++i) {
    Sector &s = block[i];

    if (s.data != nullptr) {
      delete[] s.data;
      s.data = nullptr;
    }

    s.size = 0;
    s.sectorID = -1;
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
  // Inicializar el sector de metadatos
  initializeMetadata();

  freeBlock();
  loadBlocks(startSectorBitmap, block);

  markBlock(block, 0);
  markBlock(block, startSectorBitmap);
  markBlock(block, startSectorMetadata);

  flushModifiedSectors(block);

  createSchemaFile();
}

void DiskController::initializeBootSector() {
  head->resetPosition();
  head->openCurrentSectorFD();

  loadBitMap = false;
  startSectorBitmap = 4;
  blocksPerBitMap = 1;

  loadMetada = false;
  startSectorMetadata = 8;
  blocksPerMetadata = 0;

  // 4 bytes: número de platos
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
  writeBinary(startSectorBitmap, 0);
  // 1 byte: número de bloque para bit map
  writeBinary(blocksPerBitMap, 0);
  // 1 byte: sector de inicio para metadatos
  writeBinary(startSectorMetadata, 0);
  // 1 byte: número de sectores para metadatos
  writeBinary(blocksPerMetadata, 0);
}

void DiskController::initializeBitMap() {
  uint32_t totalSectors = numberDisks * 2 * numberTracks * numberSectors;
  uint32_t totalBytes = (totalSectors + 7) / 8;
  uint32_t writtenBytes = 0;
  uint32_t nextSector = 0xFFFFFFFF;

  uint32_t currentSector = startSectorBitmap;

  freeBlock();
  loadBlocks(currentSector, block);

  for (uint8_t i = 0; i < sectorsLoaded && writtenBytes < totalBytes; ++i) {
    uint8_t *data = block[i].data;
    uint16_t j = (i == 0) ? 4 : 0;

    for (; j < numberBytes && writtenBytes < totalBytes; ++j, ++writtenBytes) {
      data[j] = 0x00;
    }

    block[i].size = j;
    block[i].modified = true;
  }

  memcpy(block[0].data, &nextSector, sizeof(nextSector));

  flushModifiedSectors(block);
}

// se reserva un bloque en un cilindro para metadat
void DiskController::initializeMetadata() {

  loadBlocks(startSectorMetadata, block);

  uint32_t nextSector = 0xFFFFFFFF;
  uint32_t writtenRegisters = 0;

  uint32_t maxRegisters =
      numberBytes * sectorsBlock -
      SIZE_FIRST_HEADER_METADATA / SIZE_PER_REGISTER_METADATA;
  uint16_t bytesForBitmap = (maxRegisters + 7) / 8;

  maxRegisters = numberBytes * sectorsBlock -
                 (SIZE_FIRST_HEADER_METADATA + bytesForBitmap) /
                     SIZE_PER_REGISTER_METADATA;
  bytesForBitmap = (maxRegisters + 7) / 8;

  uint32_t offset = SIZE_FIRST_HEADER_METADATA + bytesForBitmap;

  uint8_t *header = new uint8_t[SIZE_FIRST_HEADER_METADATA + bytesForBitmap]{0};

  uint32_t index = 0;
  memcpy(header + index, &nextSector, sizeof(nextSector));
  index += sizeof(nextSector);
  memcpy(header + index, &offset, sizeof(offset));
  index += sizeof(offset);
  memcpy(header + index, &maxRegisters, sizeof(maxRegisters));
  index += sizeof(maxRegisters);
  memcpy(header + index, &writtenRegisters, sizeof(writtenRegisters));
  index += sizeof(writtenRegisters);

  memcpy(block[0].data, header, SIZE_FIRST_HEADER_METADATA + bytesForBitmap);
  block[0].size = SIZE_FIRST_HEADER_METADATA + bytesForBitmap;
  block[0].modified = true;

  flushModifiedSectors(block);
  delete[] header;
}
bool DiskController::markBlock(Sector *block,
                               uint32_t startSectorBlock,
                               bool used) {
  // byteIndex: índice del byte en el bitmap
  uint32_t byteIndex = startSectorBlock / 8 + 4;
  // bitPos: posición del bit dentro del byte
  uint8_t bitPos = startSectorBlock % 8;
  // sectorsOffset: índice del sector en el bitmap
  uint32_t sectorOffset = byteIndex / numberBytes;
  // byteOffset: indice del byte dentro del sector
  uint32_t byteOffset = byteIndex % numberBytes;

  uint8_t &bitmapByte = block[sectorOffset].data[byteOffset + 4];

  if (used) {
    bitmapByte |= (1 << bitPos);
  } else {
    bitmapByte &= ~(1 << bitPos);
  }

  block[sectorOffset].modified = true;

  return true;
}

bool DiskController::createFile(const char *fileName) {
  char fileRegister[SIZE_PER_REGISTER_METADATA] = {0};

  // Copiar nombre del archivo al registro
  for (int i = 0; i < FILE_NAME_LENGTH && fileName[i] != '\0'; ++i) {
    fileRegister[i] = fileName[i];
  }

  // Obtener sector inicial libre
  freeBlock();
  loadBlocks(startSectorBitmap, block);

  uint32_t startSectorFile = nextSectorFree(block, 0, false);
  uint32_t metadata = 0;

  markBlock(block, startSectorFile);
  flushModifiedSectors(block);

  // Copiar datos de sector inicial y metadata al registro
  memcpy(fileRegister + FILE_NAME_LENGTH, &startSectorFile, sizeof(uint32_t));
  memcpy(fileRegister + FILE_NAME_LENGTH + 4, &metadata, sizeof(uint32_t));

  // Cargar bloque de metadatos
  freeBlock();
  loadBlocks(startSectorMetadata, block);

  // Leer header
  Sector &sector = block[0];
  uint32_t nextSector, offset, maxRegisters, writtenRegisters;
  memcpy(&nextSector, sector.data, sizeof(uint32_t));
  memcpy(&offset, sector.data + 4, sizeof(uint32_t));
  memcpy(&maxRegisters, sector.data + 8, sizeof(uint32_t));
  memcpy(&writtenRegisters, sector.data + 12, sizeof(uint32_t));

  if (writtenRegisters >= maxRegisters) {
    return false;
  }

  uint32_t bytesUsed = offset + writtenRegisters * SIZE_PER_REGISTER_METADATA;
  uint32_t sectorIndex = bytesUsed / numberBytes;
  uint32_t localOffset = bytesUsed % numberBytes;

  Sector &targetSector = block[sectorIndex];
  uint32_t spaceAvailable = numberBytes - localOffset;

  if (spaceAvailable >= SIZE_PER_REGISTER_METADATA) {
    // Todo el registro cabe en el sector actual
    memcpy(targetSector.data + localOffset,
           fileRegister,
           SIZE_PER_REGISTER_METADATA);
    targetSector.modified = true;
    if (targetSector.size < localOffset + SIZE_PER_REGISTER_METADATA)
      targetSector.size = localOffset + SIZE_PER_REGISTER_METADATA;
  } else {
    uint32_t firstPart = spaceAvailable;
    uint32_t secondPart = SIZE_PER_REGISTER_METADATA - firstPart;

    memcpy(targetSector.data + localOffset, fileRegister, firstPart);
    targetSector.modified = true;

    if (targetSector.size < numberBytes)
      targetSector.size = numberBytes;

    if (sectorIndex + 1 >= sectorsLoaded)
      return false;

    Sector &next = block[sectorIndex + 1];
    memcpy(next.data, fileRegister + firstPart, secondPart);
    next.modified = true;
    if (next.size < secondPart)
      next.size = secondPart;
  }

  // Actualizar contador de registros escritos
  ++writtenRegisters;
  memcpy(sector.data + 12, &writtenRegisters, sizeof(uint32_t));
  sector.modified = true;

  flushModifiedSectors(block);
  return true;
}

char *DiskController::searchFile(const char *fileName) {
  freeBlock();
  loadBlocks(startSectorMetadata, block);

  Sector &header = block[0];
  uint32_t offset, maxRegisters, writtenRegisters;
  memcpy(&offset, header.data + 4, sizeof(uint32_t));
  memcpy(&maxRegisters, header.data + 8, sizeof(uint32_t));
  memcpy(&writtenRegisters, header.data + 12, sizeof(uint32_t));

  uint32_t totalBytes = writtenRegisters * SIZE_PER_REGISTER_METADATA;

  for (uint32_t i = 0; i < totalBytes;) {
    uint32_t sectorIndex = (offset + i) / numberBytes;
    uint32_t localOffset = (offset + i) % numberBytes;

    if (sectorIndex >= sectorsLoaded)
      break;

    Sector &sector = block[sectorIndex];

    uint32_t spaceAvailable = numberBytes - localOffset;
    uint32_t bytesToRead =
        std::min(spaceAvailable, (uint32_t)SIZE_PER_REGISTER_METADATA);

    char registerData[SIZE_PER_REGISTER_METADATA] = {0};

    if (bytesToRead == SIZE_PER_REGISTER_METADATA) {
      memcpy(
          registerData, sector.data + localOffset, SIZE_PER_REGISTER_METADATA);
    } else {
      // Fragmentado entre dos sectores
      memcpy(registerData, sector.data + localOffset, bytesToRead);
      if (sectorIndex + 1 >= sectorsLoaded)
        break;

      Sector &next = block[sectorIndex + 1];
      memcpy(registerData + bytesToRead,
             next.data,
             SIZE_PER_REGISTER_METADATA - bytesToRead);
    }

    if (strncmp(registerData, fileName, FILE_NAME_LENGTH) == 0) {
      char *result = new char[SIZE_PER_REGISTER_METADATA];
      memcpy(result, registerData, SIZE_PER_REGISTER_METADATA);
      return result;
    }

    i += SIZE_PER_REGISTER_METADATA;
  }

  return nullptr;
}

bool DiskController::modifyMetadata(const char *fileName,
                                    uint32_t newMetadata) {
  freeBlock();
  loadBlocks(startSectorMetadata, block);

  Sector &header = block[0];
  uint32_t offset, maxRegisters, writtenRegisters;
  memcpy(&offset, header.data + 4, sizeof(uint32_t));
  memcpy(&maxRegisters, header.data + 8, sizeof(uint32_t));
  memcpy(&writtenRegisters, header.data + 12, sizeof(uint32_t));

  uint32_t totalBytes = writtenRegisters * SIZE_PER_REGISTER_METADATA;

  for (uint32_t i = 0; i < totalBytes;) {
    uint32_t sectorIndex = (offset + i) / numberBytes;
    uint32_t localOffset = (offset + i) % numberBytes;

    if (sectorIndex >= sectorsLoaded)
      break;

    Sector &sector = block[sectorIndex];

    uint32_t spaceAvailable = numberBytes - localOffset;
    uint32_t bytesToRead =
        std::min(spaceAvailable, (uint32_t)SIZE_PER_REGISTER_METADATA);

    char registerData[SIZE_PER_REGISTER_METADATA] = {0};

    if (bytesToRead == SIZE_PER_REGISTER_METADATA) {
      memcpy(
          registerData, sector.data + localOffset, SIZE_PER_REGISTER_METADATA);
    } else {
      if (sectorIndex + 1 >= sectorsLoaded)
        break;
      Sector &next = block[sectorIndex + 1];
      memcpy(registerData, sector.data + localOffset, bytesToRead);
      memcpy(registerData + bytesToRead,
             next.data,
             SIZE_PER_REGISTER_METADATA - bytesToRead);
    }

    if (strncmp(registerData, fileName, FILE_NAME_LENGTH) == 0) {
      // Encontrado, escribir los últimos 4 bytes (offset 20)
      uint32_t metadataOffset = FILE_NAME_LENGTH + 4;

      if (spaceAvailable >= metadataOffset + 4) {
        memcpy(sector.data + localOffset + metadataOffset,
               &newMetadata,
               sizeof(uint32_t));
        sector.modified = true;
      } else {
        // Puede estar dividido en dos sectores
        uint32_t part1 = numberBytes - (localOffset + metadataOffset);
        uint32_t part2 = 4 - part1;

        memcpy(sector.data + localOffset + metadataOffset, &newMetadata, part1);
        sector.modified = true;

        Sector &next = block[sectorIndex + 1];
        memcpy(next.data, ((uint8_t *)&newMetadata) + part1, part2);
        next.modified = true;
      }

      flushModifiedSectors(block);
      return true;
    }

    i += SIZE_PER_REGISTER_METADATA;
  }

  return false;
}

void DiskController::createSchemaFile(){
  char *informationEschema;

  if (createFile("esquema.txt"))
    informationEschema = searchFile("esquema.txt");
  else
    return;

  uint32_t startSectorFile;
  memcpy(&startSectorFile, informationEschema + FILE_NAME_LENGTH, sizeof(startSectorFile));

  uint32_t nextSector = 0xFFFFFFFF;
  uint32_t usedBytes = 12;
  uint32_t freeStorage = numberBytes - 12;
  
  freeBlock();
  loadBlocks(startSectorFile, block);

  memcpy(block[0].data, &nextSector, sizeof(uint32_t));
  memcpy(block[0].data + 4, &usedBytes, sizeof(uint32_t));
  memcpy(block[0].data + 8, &freeStorage, sizeof(uint32_t));
  
  block[0].size = 12;
  block[0].modified = true;

  flushModifiedSectors(block);
}
