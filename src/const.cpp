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
constexpr int SIZE_FULL_PATH = sizeof(PATH) + 3 * (1 + 10) + 7;
