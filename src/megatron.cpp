#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <diskController.hpp>
#include <megatron.hpp>
#include <string>
#include <utils.hpp>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "const.cpp"
#include <iostream>

Megatron::~Megatron() { delete diskController; }

void Megatron::init() {
  std::cout << "% Megatron 3000\n";
  std::cout << "\tWelcome to Megatron 3000!\n\n";
  std::cout << "% Opciones:\n";
  std::cout << "1) Construir disco\n";
  std::cout << "2) Salir\n\n";

  int choice;
  std::cout << "Opcion : ";
  std::cin >> choice;

  switch (choice) {
  case 1:
    buildStructure();

    diskController->init();

    createSchema();

    std::cout << getSchema("titanic") << '\n';
    break;
  case 2:
    std::cout << "Hasta luego.\n";
    break;
  default:
    std::cout << "Opcion invalida.\n";
    break;
  }
}

void Megatron::buildStructure() {
  unsigned int numberDisks, numberTracks, numberSectors, numberBytes,
      sectorsBlock;

  std::cout << "& Cantidad de discos : ";
  std::cin >> numberDisks;

  std::cout << "& Cantidad de pistas por superficie : ";
  std::cin >> numberTracks;

  std::cout << "& Cantidad de sectores por pista : ";
  std::cin >> numberSectors;

  std::cout << "& Cantidad de bytes por sector : ";
  std::cin >> numberBytes;

  std::cout << "& Cantidad de sectores por bloque : ";
  std::cin >> sectorsBlock;

  if (mkdir(PATH, 0777) == -1 && errno != EEXIST) {
    std::cerr << "Error creating base directory: " << PATH << "\n";
    return;
  }

  char path[SIZE_FULL_PATH];

  for (size_t d = 0; d < numberDisks; ++d) {
    int pos = 0;
    for (int i = 0; PATH[i]; ++i)
      path[pos++] = PATH[i];
    path[pos++] = '/';

    utils::writeInt(d, path, pos);
    path[pos] = '\0';

    if (mkdir(path, 0777) == -1 && errno != EEXIST) {
      std::cerr << "Error creating disk directory: " << path << "\n";
      return;
    }

    for (size_t s = 0; s < 2; ++s) {
      int posS = pos;
      path[posS++] = '/';

      utils::writeInt(s, path, posS);
      path[posS] = '\0';

      if (mkdir(path, 0777) == -1 && errno != EEXIST) {
        std::cerr << "Error creating surface directory: " << path << "\n";
        return;
      }

      for (size_t t = 0; t < numberTracks; ++t) {
        int posT = posS;
        path[posT++] = '/';

        utils::writeInt(t, path, posT);
        path[posT] = '\0';

        if (mkdir(path, 0777) == -1 && errno != EEXIST) {
          std::cerr << "Error creating track directory: " << path << "\n";
          return;
        }

        for (size_t sec = 0; sec < numberSectors; ++sec) {
          int posF = posT;
          path[posF++] = '/';

          utils::writeInt(sec, path, posF);
          const char *ext = ".dat";

          for (int i = 0; ext[i]; ++i)
            path[posF++] = ext[i];
          path[posF] = '\0';

          int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
          if (fd == -1) {
            std::cerr << "Error al crear archivo: " << path << "\n";
            return;
          }

          uint8_t empty = 0;
          write(fd, &empty, sizeof(uint8_t));
          close(fd);
        }
      }
    }
  }

  diskController = new DiskController(
      numberDisks, numberTracks, numberSectors, numberBytes, sectorsBlock);
}

void Megatron::createSchema() {
  std::cin.ignore();
  std::string schemaName;
  std::cout << "\n% Ingrese el nombre del esquema : ";
  getline(std::cin, schemaName, '\n');

  if (schemaName.empty()) {
    std::cout << "Nombre de esquema invalido.\n";
    return;
  }

  std::string schemaDefinition = utils::inputSchema();
  std::string fullSchema = schemaName + schemaDefinition;

  if (fullSchema.empty()) {
    std::cout << "Esquema invalido.\n";
    return;
  }

  printf("& Creando esquema: %s\n", fullSchema.c_str());

  if (!diskController->createFile(schemaName.c_str())) {
    std::cout << "No se pudo crear el archivo para el esquema '" << schemaName
              << "'.\n";
    return;
  }

  char *info = diskController->searchFile("esquema.txt");
  if (!info) {
    std::cout << "No se encontró 'esquema.txt'.\n";
    return;
  }

  uint32_t sectorID;
  memcpy(&sectorID, info + FILE_NAME_LENGTH, sizeof(uint32_t));
  delete[] info;

  const char *schemaData = fullSchema.c_str();
  uint32_t schemaSize = fullSchema.size();
  uint32_t totalSize = 4 + schemaSize;

  while (true) {
    diskController->freeBlock();
    diskController->loadBlocks(sectorID, diskController->block);

    Sector &header = diskController->block[0];
    uint32_t nextSector, usedBytes, freeBytes;
    memcpy(&nextSector, header.data, sizeof(uint32_t));
    memcpy(&usedBytes, header.data + 4, sizeof(uint32_t));
    memcpy(&freeBytes, header.data + 8, sizeof(uint32_t));

    uint32_t totalSize = sizeof(uint32_t) + schemaSize; // nextOffset + esquema

    if (freeBytes >= totalSize) {
      uint32_t offset = 12;
      bool first = (usedBytes == 12);

      if (!first) {
        while (true) {
          uint32_t nextOffset;

          int si = offset / diskController->numberBytes;
          int io = offset % diskController->numberBytes;

          memcpy(&nextOffset,
                 diskController->block[si].data + io,
                 sizeof(uint32_t));

          if (nextOffset >= usedBytes)
            break;
          offset = nextOffset;
        }

        // Actualizar el último esquema para que apunte al nuevo
        uint32_t newOffset = usedBytes;
        int si = offset / diskController->numberBytes;
        int io = offset % diskController->numberBytes;
        memcpy(
            diskController->block[si].data + io, &newOffset, sizeof(uint32_t));
        diskController->block[si].modified = true;
      }

      // Escribir offset al siguiente (al final del esquema)
      uint32_t nextOffset = usedBytes + totalSize;
      int si = usedBytes / diskController->numberBytes;
      int io = usedBytes % diskController->numberBytes;

      memcpy(
          diskController->block[si].data + io, &nextOffset, sizeof(uint32_t));

      // Escribir los datos del esquema justo después
      uint32_t copied = 0;
      uint32_t remaining = schemaSize;
      uint32_t currentOffset = usedBytes + sizeof(uint32_t);

      while (remaining > 0) {
        int sectorIndex = currentOffset / diskController->numberBytes;
        int sectorOffset = currentOffset % diskController->numberBytes;

        Sector &s = diskController->block[sectorIndex];
        uint32_t available = diskController->numberBytes - sectorOffset;

        uint32_t toCopy = std::min(available, remaining);
        memcpy(s.data + sectorOffset, schemaData + copied, toCopy);

        s.modified = true;
        s.size = sectorOffset + toCopy;

        copied += toCopy;
        remaining -= toCopy;
        currentOffset += toCopy;
      }

      usedBytes += totalSize;
      freeBytes -= totalSize;

      memcpy(header.data + 4, &usedBytes, sizeof(uint32_t));
      memcpy(header.data + 8, &freeBytes, sizeof(uint32_t));
      header.modified = true;

      diskController->flushModifiedSectors(diskController->block);
      std::cout << "+ Esquema guardado correctamente.\n";
      return;
    }
  }
}

std::string Megatron::getSchema(const std::string &targetName) {
  char *esquemaInfo = diskController->searchFile("esquema.txt");
  if (!esquemaInfo) {
    std::cout << "No se encontró 'esquema.txt'.\n";
    return "";
  }

  uint32_t sectorID;
  memcpy(&sectorID, esquemaInfo + FILE_NAME_LENGTH, sizeof(uint32_t));
  delete[] esquemaInfo;

  while (true) {
    diskController->freeBlock();
    diskController->loadBlocks(sectorID, diskController->block);

    Sector &header = diskController->block[0];
    uint32_t nextSector, usedBytes;
    memcpy(&nextSector, header.data, sizeof(uint32_t));
    memcpy(&usedBytes, header.data + 4, sizeof(uint32_t));

    uint32_t offset = 12;
    while (offset < usedBytes) {
      // Leer offset al siguiente esquema
      uint32_t nextOffset;
      int si = offset / diskController->numberBytes;
      int io = offset % diskController->numberBytes;
      memcpy(
          &nextOffset, diskController->block[si].data + io, sizeof(uint32_t));

      // Calcular tamaño de este esquema
      uint32_t schemaStart = offset + sizeof(uint32_t);
      uint32_t schemaSize = nextOffset - schemaStart;

      // Leer los datos del esquema
      std::string schema = "";
      uint32_t copied = 0;
      while (copied < schemaSize) {
        uint32_t absoluteOffset = schemaStart + copied;
        int sIndex = absoluteOffset / diskController->numberBytes;
        int sOffset = absoluteOffset % diskController->numberBytes;

        Sector &s = diskController->block[sIndex];
        uint32_t available = diskController->numberBytes - sOffset;
        uint32_t toRead = std::min(schemaSize - copied, available);

        for (uint32_t i = 0; i < toRead; ++i) {
          schema += s.data[sOffset + i];
        }

        copied += toRead;
      }

      // Comparar nombre
      size_t pos = schema.find('#');
      if (pos != std::string::npos) {
        std::string name = schema.substr(0, pos);
        if (name == targetName) {
          return schema;
        }
      }

      offset = nextOffset;
    }

    if (nextSector == 0xFFFFFFFF)
      break;

    sectorID = nextSector;
  }

  return ""; // No se encontró
}
