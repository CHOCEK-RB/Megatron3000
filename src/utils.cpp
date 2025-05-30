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

  while(true){
    std::string attribute;

    std::cout << "% Ingrese el atributo (nombre#tipo#tamaño) o ';' para terminar : \n";
    getline(std::cin, attribute, '#');

    if (attribute == ";") {
      break;
    }

    size_t pos1 = attribute.find('#');
    if (pos1 == std::string::npos) {
      std::cout << "Atributo invalido.\n";
      continue;
    }

    size_t pos2 = attribute.find('#', pos1 + 1);
    if (pos2 == std::string::npos) {
      std::cout << "Atributo invalido.\n";
      continue;
    }

    std::string name = attribute.substr(0, pos1);
    std::string type = attribute.substr(pos1 + 1, pos2);
    std::string size = attribute.substr(pos2 + 1);

    if (name.empty()) {
      std::cout << "Nombre de atributo invalido.\n";
      continue;
    }

    if (type != "int" && type != "float" && type != "char") {
      std::cout << "Tipo de atributo invalido. Debe ser 'int', 'float' o 'char'.\n";
      continue;
    }

    if (size.empty() || std::stoi(size) <= 0) {
      std::cout << "Tamaño de atributo invalido. Debe ser un número.\n";
      continue;
    }

    schema += "#" + name + "#" + type + "(" + size + ")";
  }

  return schema;
}
