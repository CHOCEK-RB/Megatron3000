#define POS_DISKS 0
#define POS_TRACKS 4
#define POS_SECTORS 8
#define POS_BYTES 12
#define POS_BLOCK 16
#define POS_FAT 20 
#define POS_METADATA 21
#define POS_FILES 23 
#define POS_TYPE 24

constexpr char PATH[] = "megatron";
constexpr char PATH_BLOCK[] = "block.txt";

constexpr int SIZE_FULL_PATH = 100; 
constexpr int FILE_NAME_LENGTH = 20;

// name(20 bytes) startSector (4 bytes) size (4 bytes)
constexpr int SIZE_PER_REGISTER_METADATA = 28;

// next(4 bytes) offset_inicio(4 bytes) maximo_registros(4 bytes) total_registros(4 bytes)
constexpr int SIZE_FIRST_HEADER_METADATA = 16;
