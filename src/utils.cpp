#include "const.cpp"
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>
#include <utils.hpp>
#include <string>

bool utils::directoryExists(const char *path) {
  struct stat info;
  if (stat(path, &info) != 0) {
    return false;
  }
  return (info.st_mode & S_IFDIR) != 0;
}

int utils::countDigits(int num) {
  if (num == 0)
    return 1;

  int count = 0;
  if (num < 0)
    num = -num;

  while (num > 0) {
    num /= 10;
    count++;
  }
  return count;
}

void utils::writeInt(int num, char *line, int &pos) {
  if (num == 0) {
    line[pos++] = '0';
    return;
  }

  int div = 1;
  int temp = num;

  while (temp >= 10) {
    temp /= 10;
    div *= 10;
  }

  while (div > 0) {
    char digit = '0' + (num / div);
    line[pos++] = digit;

    num %= div;
    div /= 10;
  }
}

std::string utils::createFullPath(int disk, int surface, int track, int sector) {
  return std::string(PATH) + "/" +
         std::to_string(disk) + "/" +
         std::to_string(surface) + "/" +
         std::to_string(track) + "/" +
         std::to_string(sector) + ".dat";
}

std::string utils::inputSchema() {
  std::string schema = "";

  while (true) {
    std::string name, type, size;

    std::cout << "% Ingrese el nombre del atributo (o ';' para terminar): ";
    std::getline(std::cin, name);
    if (name == ";") break;
    if (name.empty()) {
      std::cout << "Nombre inválido.\n";
      continue;
    }

    std::cout << "% Ingrese el tipo del atributo [int | float | varchar]: ";
    std::getline(std::cin, type);
    if (type != "int" && type != "float" && type != "varchar") {
      std::cout << "Tipo inválido.\n";
      continue;
    }

    std::cout << "% Ingrese el tamaño del atributo (entero > 0): ";
    std::getline(std::cin, size);
    try {
      int sz = std::stoi(size);
      if (sz <= 0) throw std::invalid_argument("tamaño inválido");
    } catch (...) {
      std::cout << "Tamaño inválido.\n";
      continue;
    }

    std::cout << "+ Atributo agregado: " << name << " " << type << "(" << size << ")\n";
    schema += "#" + name + "#" + type + "(" + size + ")";
  }

  return schema;
}

