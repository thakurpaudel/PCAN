#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define PCAN_USBFD_TYPE_EXT 2

struct pcan_ufd_fw_info {
  uint16_t size_of;
  uint16_t type;
  uint8_t hw_type;
  uint8_t bl_version[3];
  uint8_t hw_version;
  uint8_t fw_version[3];
  uint64_t dev_id;
  uint32_t ser_no;
  uint32_t flags;
  uint8_t cmd_out_ep;
  uint8_t cmd_in_ep;
  uint16_t data_out_ep;
  uint8_t data_in_ep;
  uint8_t dummy[3];
} __attribute__((packed));

void dump_struct() {
  struct pcan_ufd_fw_info info = {.size_of = 36,
                                  .type = PCAN_USBFD_TYPE_EXT,
                                  .hw_type = 0,
                                  .bl_version = {1, 0, 0},
                                  .hw_version = 1,
                                  .fw_version = {1, 3, 3},
                                  .dev_id = 0,
                                  .ser_no = 12345,
                                  .flags = 0,
                                  .cmd_out_ep = 0x03,
                                  .cmd_in_ep = 0x83,
                                  .data_out_ep = 0x01 | (0x02 << 8),
                                  .data_in_ep = 0x81,
                                  .dummy = {0, 0, 0}};

  printf("--- PCAN-USB Pro FD Firmware Info Structure Verification ---\n");
  printf("Total Size: %zu bytes (Expected: 36)\n\n", sizeof(info));

  printf("Field Offsets:\n");
  printf("  size_of:     %zu\n", offsetof(struct pcan_ufd_fw_info, size_of));
  printf("  type:        %zu\n", offsetof(struct pcan_ufd_fw_info, type));
  printf("  hw_version:  %zu (Driver looks for 'vN' here)\n",
         offsetof(struct pcan_ufd_fw_info, hw_version));
  printf("  fw_version:  %zu (Driver looks for 'fw vX.Y.Z' here)\n",
         offsetof(struct pcan_ufd_fw_info, fw_version));
  printf("  cmd_out_ep:  %zu (CRITICAL: Error -8 if wrong! Expected: 28)\n",
         offsetof(struct pcan_ufd_fw_info, cmd_out_ep));
  printf("  cmd_in_ep:   %zu (Expected: 29)\n",
         offsetof(struct pcan_ufd_fw_info, cmd_in_ep));
  printf("  data_out_ep: %zu\n",
         offsetof(struct pcan_ufd_fw_info, data_out_ep));

  printf("\nHex Dump:\n");
  uint8_t *ptr = (uint8_t *)&info;
  for (int i = 0; i < sizeof(info); i++) {
    printf("%02X ", ptr[i]);
    if ((i + 1) % 4 == 0)
      printf(" ");
    if ((i + 1) % 16 == 0)
      printf("\n");
  }
  printf("\n");
}

int main() {
  dump_struct();
  return 0;
}
