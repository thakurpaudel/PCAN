#include "pcanpro_can.h"
#include "fdcan.h"
#include "main.h"
#include "pcanpro_timestamp.h"
#include "pcanpro_variant.h"
#include <assert.h>
#include <stdio.h>

// Use external FDCAN handles from CubeMX-generated fdcan.c
extern FDCAN_HandleTypeDef hfdcan1;
extern FDCAN_HandleTypeDef hfdcan2;

// static FDCAN_HandleTypeDef *hcan[CAN_BUS_TOTAL] = {[CAN_BUS_1] = &hfdcan1,
//                                                    [CAN_BUS_2] = &hfdcan2};

#define CAN_TX_FIFO_SIZE (256)
static struct t_can_dev {
  void *dev;
  uint32_t tx_msgs;
  uint32_t tx_errs;
  uint32_t tx_ovfs;

  uint32_t rx_msgs; // count
  uint32_t rx_errs; // error count
  uint32_t rx_ovfs; // overflow

  struct t_can_msg tx_fifo[CAN_TX_FIFO_SIZE];
  uint32_t tx_head;
  uint32_t tx_tail;
  uint32_t esr_reg;

  uint8_t fd_mode; // 0 = Classic CAN, 1 = CAN FD
  int (*rx_isr)(uint8_t, struct t_can_msg *);
  int (*tx_isr)(uint8_t, struct t_can_msg *);
  void (*err_handler)(int bus, uint32_t esr);
} can_dev_array[CAN_BUS_TOTAL] = {
    [CAN_BUS_1] = {.dev = &hfdcan1, .fd_mode = 0},
    [CAN_BUS_2] = {.dev = &hfdcan2, .fd_mode = 0},
};

// FDCAN interrupt flags
#define FDCAN_IT_FLAGS                                                         \
  (FDCAN_IT_RX_FIFO0_NEW_MESSAGE | FDCAN_IT_RX_FIFO1_NEW_MESSAGE |             \
   FDCAN_IT_TX_COMPLETE | FDCAN_IT_TX_FIFO_EMPTY | FDCAN_IT_BUS_OFF |          \
   FDCAN_IT_ERROR_PASSIVE | FDCAN_IT_ERROR_WARNING)

/* --------------- HAL PART ------------- */
static int _bus_from_int_dev(FDCAN_GlobalTypeDef *can) {
  if (can == FDCAN1)
    return CAN_BUS_1;
  else if (can == FDCAN2)
    return CAN_BUS_2;
  return CAN_BUS_1;
}

#define CAN_WITHOUT_ISR 1

// Convert DLC to actual data length
static const uint8_t DLC_to_bytes[] = {0, 1,  2,  3,  4,  5,  6,  7,
                                       8, 12, 16, 20, 24, 32, 48, 64};

static uint32_t bytes_to_dlc(uint8_t bytes) {
  if (bytes <= 8)
    return bytes; // H7 HAL uses raw 0-8 for classic
  else if (bytes <= 12)
    return FDCAN_DLC_BYTES_12;
  else if (bytes <= 16)
    return FDCAN_DLC_BYTES_16;
  else if (bytes <= 20)
    return FDCAN_DLC_BYTES_20;
  else if (bytes <= 24)
    return FDCAN_DLC_BYTES_24;
  else if (bytes <= 32)
    return FDCAN_DLC_BYTES_32;
  else if (bytes <= 48)
    return FDCAN_DLC_BYTES_48;
  else
    return FDCAN_DLC_BYTES_64;
}

static uint8_t dlc_to_bytes(uint32_t dlc) {
  uint32_t code = dlc & 0x0F; // No shift on H7 HAL
  if (code < 16)
    return DLC_to_bytes[code];
  return 8;
}

uint32_t pcan_can_msg_time(const struct t_can_msg *pmsg, uint32_t nt,
                           uint32_t dt) {
  const uint32_t data_bits = pmsg->size << 3;
  const uint32_t control_bits = (pmsg->flags & MSG_FLAG_EXT) ? 67 : 47;

  if (pmsg->flags & MSG_FLAG_BRS)
    return (control_bits * nt) + (data_bits * dt);
  else
    return (control_bits + data_bits) * nt;
}

int pcan_can_set_filter_mask(int bus, int num, int format, uint32_t id,
                             uint32_t mask) {
  FDCAN_FilterTypeDef filter = {0};
  FDCAN_HandleTypeDef *p_can = can_dev_array[bus].dev;

  if (!p_can)
    return 0;

  if (num >= CAN_INT_FILTER_MAX)
    return -1;

  filter.IdType =
      (format == MSG_FLAG_EXT) ? FDCAN_EXTENDED_ID : FDCAN_STANDARD_ID;
  filter.FilterIndex = num;
  filter.FilterType = FDCAN_FILTER_MASK;
  filter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;

  if (format == MSG_FLAG_EXT) {
    filter.FilterID1 = id & 0x1FFFFFFF;
    filter.FilterID2 = mask & 0x1FFFFFFF;
  } else {
    filter.FilterID1 = id & 0x7FF;
    filter.FilterID2 = mask & 0x7FF;
  }

  if (HAL_FDCAN_ConfigFilter(p_can, &filter) != HAL_OK)
    return -1;

  return 0;
}

int pcan_can_filter_init_stdid_list(int bus, const uint16_t *id_list,
                                    int id_len) {
  FDCAN_FilterTypeDef filter = {0};
  FDCAN_HandleTypeDef *p_can = can_dev_array[bus].dev;

  if (!p_can)
    return 0;

  int i;

  filter.IdType = FDCAN_STANDARD_ID;
  filter.FilterType = FDCAN_FILTER_DUAL;
  filter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;

  for (i = 0; i < id_len; i += 2) {
    filter.FilterIndex = i >> 1;
    filter.FilterID1 = id_list[i] & 0x7FF;

    if ((i + 1) < id_len)
      filter.FilterID2 = id_list[i + 1] & 0x7FF;
    else
      filter.FilterID2 = 0x7FF; // Don't care for last odd entry

    if (HAL_FDCAN_ConfigFilter(p_can, &filter) != HAL_OK)
      return -1;
  }

  return id_len;
}

static int _can_send(FDCAN_HandleTypeDef *p_can, struct t_can_msg *p_msg,
                     uint8_t fd_mode) {
  FDCAN_TxHeaderTypeDef msg = {0};

  // Check if TX FIFO is full
  if (HAL_FDCAN_GetTxFifoFreeLevel(p_can) == 0) {
    if (HAL_GetTick() % 1000 < 50) { // Limit frequency
      printf("CAN%d HW TX FIFO FULL (Bus stuck?)\r\n",
             (int)(_bus_from_int_dev(p_can->Instance) + 1));
    }
    return -1;
  }

  if (p_msg->flags & MSG_FLAG_EXT) {
    msg.Identifier = p_msg->id & 0x1FFFFFFF;
    msg.IdType = FDCAN_EXTENDED_ID;
  } else {
    msg.Identifier = p_msg->id & 0x7FF;
    msg.IdType = FDCAN_STANDARD_ID;
  }

  msg.TxFrameType =
      (p_msg->flags & MSG_FLAG_RTR) ? FDCAN_REMOTE_FRAME : FDCAN_DATA_FRAME;
  msg.ErrorStateIndicator =
      (p_msg->flags & MSG_FLAG_ESI) ? FDCAN_ESI_PASSIVE : FDCAN_ESI_ACTIVE;

  // CAN FD specific
  if (fd_mode && (p_msg->flags & MSG_FLAG_FD)) {
    msg.FDFormat = FDCAN_FD_CAN;
    msg.BitRateSwitch =
        (p_msg->flags & MSG_FLAG_BRS) ? FDCAN_BRS_ON : FDCAN_BRS_OFF;
    msg.DataLength = bytes_to_dlc(p_msg->size);
  } else {
    msg.FDFormat = FDCAN_CLASSIC_CAN;
    msg.BitRateSwitch = FDCAN_BRS_OFF;
    msg.DataLength = bytes_to_dlc(p_msg->size > 8 ? 8 : p_msg->size);
  }

  msg.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
  msg.MessageMarker = 0;

  if (HAL_FDCAN_AddMessageToTxFifoQ(p_can, &msg, p_msg->data) != HAL_OK) {
    printf("CAN HW TX ERR\r\n");
    return -1;
  }

  printf("CAN%d HW TX: ID=0x%lx DLC=%lu\r\n",
         (int)(_bus_from_int_dev(p_can->Instance) + 1),
         (unsigned long)msg.Identifier, (unsigned long)msg.DataLength);

  return 0;
}

static void pcan_can_flush_tx(int bus) {
  struct t_can_dev *p_dev = &can_dev_array[bus];
  struct t_can_msg *p_msg;

  // Empty fifo
  if (p_dev->tx_head == p_dev->tx_tail)
    return;

  if (!p_dev->dev)
    return;

  p_msg = &p_dev->tx_fifo[p_dev->tx_tail];
  if (_can_send(p_dev->dev, p_msg, p_dev->fd_mode) < 0)
    return;

  if (p_dev->tx_isr) {
    (void)p_dev->tx_isr(bus, p_msg);
  }

  // Update fifo index
  p_dev->tx_tail = (p_dev->tx_tail + 1) & (CAN_TX_FIFO_SIZE - 1);
  // printf("CAN%d TX OK\r\n", bus + 1);
}

int pcan_can_write(int bus, struct t_can_msg *p_msg) {
  struct t_can_dev *p_dev = &can_dev_array[bus];

  if (!p_dev)
    return 0;

  if (!p_msg)
    return 0;

  uint32_t tx_head_next = (p_dev->tx_head + 1) & (CAN_TX_FIFO_SIZE - 1);
  // Overflow? Just skip it
  if (tx_head_next == p_dev->tx_tail) {
    ++p_dev->tx_ovfs;
    return -1;
  }

  p_dev->tx_fifo[p_dev->tx_head] = *p_msg;
  p_dev->tx_head = tx_head_next;

  return 0;
}

void pcan_can_install_rx_callback(int bus,
                                  int (*cb)(uint8_t, struct t_can_msg *)) {
  struct t_can_dev *p_dev = &can_dev_array[bus];
  p_dev->rx_isr = cb;
}

void pcan_can_install_tx_callback(int bus,
                                  int (*cb)(uint8_t, struct t_can_msg *)) {
  struct t_can_dev *p_dev = &can_dev_array[bus];
  p_dev->tx_isr = cb;
}

void pcan_can_install_err_callback(int bus, void (*cb)(int, uint32_t)) {
  struct t_can_dev *p_dev = &can_dev_array[bus];
  p_dev->err_handler = cb;
}

// Bitrate calculation for FDCAN
// Assuming FDCAN kernel clock = 80 MHz (typical for H7)
// Adjust based on your actual clock configuration
static int _get_precalculated_bitrate(uint32_t bitrate, uint32_t *prescaler,
                                      uint32_t *tseg1, uint32_t *tseg2,
                                      uint32_t *sjw) {
  *sjw = 4; // Use a wider SJW for better sync

  // These values assume 120 MHz FDCAN clock (PLL1_Q)
  // Target sample point ~80-87%
  // Formula: 120MHz / (Prescaler * (1 + Tseg1 + Tseg2))
  switch (bitrate) {
  case 1000000u:
    *prescaler = 6;
    *tseg1 = 15;
    *tseg2 = 4;
    break;
  case 800000u:
    *prescaler = 15;
    *tseg1 = 7;
    *tseg2 = 2;
    break;
  case 500000u:
    *prescaler = 12;
    *tseg1 = 15;
    *tseg2 = 4;
    break;
  case 250000u:
    *prescaler = 24;
    *tseg1 = 15;
    *tseg2 = 4;
    break;
  case 125000u:
    *prescaler = 48;
    *tseg1 = 15;
    *tseg2 = 4;
    break;
  case 100000u:
    *prescaler = 60;
    *tseg1 = 15;
    *tseg2 = 4;
    break;
  case 50000u:
    *prescaler = 120;
    *tseg1 = 15;
    *tseg2 = 4;
    break;
  default:
    // Fallback for 500k
    *prescaler = 12;
    *tseg1 = 15;
    *tseg2 = 4;
    break;
  }

  return 0;
}

int pcan_can_init_ex(int bus, uint32_t bitrate) {
  FDCAN_HandleTypeDef *p_can = can_dev_array[bus].dev;
  uint32_t prescaler, tseg1, tseg2, sjw;

  if (!p_can)
    return 0;

  // Frame format: Classic CAN by default
  p_can->Init.FrameFormat = FDCAN_FRAME_CLASSIC;
  can_dev_array[bus].fd_mode = 0;

  p_can->Init.Mode = FDCAN_MODE_NORMAL;
  p_can->Init.AutoRetransmission = DISABLE;
  p_can->Init.TransmitPause = DISABLE;
  p_can->Init.ProtocolException = DISABLE;

  // Nominal bit timing (for arbitration phase)
  _get_precalculated_bitrate(bitrate, &prescaler, &tseg1, &tseg2, &sjw);

  p_can->Init.NominalPrescaler = prescaler;
  p_can->Init.NominalSyncJumpWidth = sjw;
  p_can->Init.NominalTimeSeg1 = tseg1;
  p_can->Init.NominalTimeSeg2 = tseg2;

  // Data bit timing (same as nominal for classic CAN)
  p_can->Init.DataPrescaler = prescaler;
  p_can->Init.DataSyncJumpWidth = sjw;
  p_can->Init.DataTimeSeg1 = tseg1;
  p_can->Init.DataTimeSeg2 = tseg2;

  // Message RAM configuration
  p_can->Init.MessageRAMOffset = (bus == CAN_BUS_1) ? 0 : 1280;
  p_can->Init.StdFiltersNbr = 28;
  p_can->Init.ExtFiltersNbr = 8;
  p_can->Init.RxFifo0ElmtsNbr = 16;
  p_can->Init.RxFifo0ElmtSize = FDCAN_DATA_BYTES_8;
  p_can->Init.RxFifo1ElmtsNbr = 16;
  p_can->Init.RxFifo1ElmtSize = FDCAN_DATA_BYTES_8;
  p_can->Init.RxBuffersNbr = 0;
  p_can->Init.RxBufferSize = FDCAN_DATA_BYTES_8;
  p_can->Init.TxEventsNbr = 0;
  p_can->Init.TxBuffersNbr = 0;
  p_can->Init.TxFifoQueueElmtsNbr = 16;
  p_can->Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;
  p_can->Init.TxElmtSize = FDCAN_DATA_BYTES_8;

  if (HAL_FDCAN_Init(p_can) != HAL_OK)
    return -1;

  // Permissive filter for debugging: Accept all
  if (HAL_FDCAN_ConfigGlobalFilter(
          p_can, FDCAN_ACCEPT_IN_RX_FIFO0, FDCAN_ACCEPT_IN_RX_FIFO0,
          FDCAN_FILTER_REMOTE, FDCAN_FILTER_REMOTE) != HAL_OK)
    return -1;

  // Activate notifications
  if (HAL_FDCAN_ActivateNotification(p_can, FDCAN_IT_FLAGS, 0) != HAL_OK)
    return -1;

  if (HAL_FDCAN_Start(p_can) != HAL_OK)
    return -1;

  printf("CAN%d: Init Classic @ %u bps (NORMAL MODE)\r\n", bus + 1,
         (unsigned int)bitrate);
  return 0;
}

// Initialize with CAN FD support
int pcan_can_init_fd(int bus, uint32_t nom_bitrate, uint32_t data_bitrate) {
  FDCAN_HandleTypeDef *p_can = can_dev_array[bus].dev;
  uint32_t prescaler, tseg1, tseg2, sjw;
  uint32_t data_prescaler, data_tseg1, data_tseg2, data_sjw;

  if (!p_can)
    return 0;

  // Frame format: FD CAN with BRS
  p_can->Init.FrameFormat = FDCAN_FRAME_FD_BRS;
  can_dev_array[bus].fd_mode = 1;

  p_can->Init.Mode = FDCAN_MODE_NORMAL;
  p_can->Init.AutoRetransmission = DISABLE;
  p_can->Init.TransmitPause = DISABLE;
  p_can->Init.ProtocolException = DISABLE;

  // Nominal bit timing
  _get_precalculated_bitrate(nom_bitrate, &prescaler, &tseg1, &tseg2, &sjw);

  p_can->Init.NominalPrescaler = prescaler;
  p_can->Init.NominalSyncJumpWidth = sjw;
  p_can->Init.NominalTimeSeg1 = tseg1;
  p_can->Init.NominalTimeSeg2 = tseg2;

  // Data bit timing (faster for FD)
  _get_precalculated_bitrate(data_bitrate, &data_prescaler, &data_tseg1,
                             &data_tseg2, &data_sjw);

  p_can->Init.DataPrescaler = data_prescaler;
  p_can->Init.DataSyncJumpWidth = data_sjw;
  p_can->Init.DataTimeSeg1 = data_tseg1;
  p_can->Init.DataTimeSeg2 = data_tseg2;

  // Message RAM configuration for FD (larger buffers, fewer elements to fit
  // 10KB RAM)
  p_can->Init.MessageRAMOffset = (bus == CAN_BUS_1) ? 0 : 1280;
  p_can->Init.StdFiltersNbr = 28;
  p_can->Init.ExtFiltersNbr = 8;
  p_can->Init.RxFifo0ElmtsNbr = 16;
  p_can->Init.RxFifo0ElmtSize = FDCAN_DATA_BYTES_64;
  p_can->Init.RxFifo1ElmtsNbr = 16;
  p_can->Init.RxFifo1ElmtSize = FDCAN_DATA_BYTES_64;
  p_can->Init.RxBuffersNbr = 0;
  p_can->Init.RxBufferSize = FDCAN_DATA_BYTES_64;
  p_can->Init.TxEventsNbr = 0;
  p_can->Init.TxBuffersNbr = 0;
  p_can->Init.TxFifoQueueElmtsNbr = 16;
  p_can->Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;
  p_can->Init.TxElmtSize = FDCAN_DATA_BYTES_64;

  if (HAL_FDCAN_Init(p_can) != HAL_OK)
    return -1;

  // Permissive filter for debugging: Accept all
  if (HAL_FDCAN_ConfigGlobalFilter(
          p_can, FDCAN_ACCEPT_IN_RX_FIFO0, FDCAN_ACCEPT_IN_RX_FIFO0,
          FDCAN_FILTER_REMOTE, FDCAN_FILTER_REMOTE) != HAL_OK)
    return -1;

  // Activate notifications
  if (HAL_FDCAN_ActivateNotification(p_can, FDCAN_IT_FLAGS, 0) != HAL_OK)
    return -1;

  if (HAL_FDCAN_Start(p_can) != HAL_OK)
    return -1;

  printf("CAN%d: Init FD @ %u/%u bps (NORMAL MODE)\r\n", bus + 1,
         (unsigned int)nom_bitrate, (unsigned int)data_bitrate);
  return 0;
}

void pcan_can_set_silent(int bus, uint8_t silent_mode) {
  FDCAN_HandleTypeDef *p_can = can_dev_array[bus].dev;

  if (!p_can)
    return;

  HAL_FDCAN_Stop(p_can);

  p_can->Init.Mode =
      silent_mode ? FDCAN_MODE_BUS_MONITORING : FDCAN_MODE_NORMAL;

  if (HAL_FDCAN_Init(p_can) != HAL_OK) {
    assert(0);
  }

  HAL_FDCAN_Start(p_can);
}

// FDCAN supports ISO/non-ISO mode
void pcan_can_set_iso_mode(int bus, uint8_t iso_mode) {
  FDCAN_HandleTypeDef *p_can = can_dev_array[bus].dev;

  if (!p_can)
    return;

  HAL_FDCAN_Stop(p_can);

  // ISO mode is the default, non-ISO uses different CRC
  p_can->Init.FrameFormat =
      iso_mode ? FDCAN_FRAME_FD_BRS : FDCAN_FRAME_FD_NO_BRS;

  if (HAL_FDCAN_Init(p_can) != HAL_OK) {
    assert(0);
  }

  HAL_FDCAN_Start(p_can);
}

void pcan_can_set_loopback(int bus, uint8_t loopback) {
  FDCAN_HandleTypeDef *p_can = can_dev_array[bus].dev;

  if (!p_can)
    return;

  HAL_FDCAN_Stop(p_can);

  p_can->Init.Mode =
      loopback ? FDCAN_MODE_INTERNAL_LOOPBACK : FDCAN_MODE_NORMAL;

  if (HAL_FDCAN_Init(p_can) != HAL_OK) {
    assert(0);
  }

  HAL_FDCAN_Start(p_can);
}

void pcan_can_set_bus_active(int bus, uint16_t mode) {
  FDCAN_HandleTypeDef *p_can = can_dev_array[bus].dev;

  if (!p_can)
    return;

  if (mode) {
    HAL_FDCAN_Start(p_can);
  } else {
    HAL_FDCAN_Stop(p_can);
  }
}

void pcan_can_set_bitrate(int bus, uint32_t bitrate, int is_data_bitrate) {
  FDCAN_HandleTypeDef *p_can = can_dev_array[bus].dev;
  uint32_t prescaler, tseg1, tseg2, sjw;

  if (!p_can)
    return;

  _get_precalculated_bitrate(bitrate, &prescaler, &tseg1, &tseg2, &sjw);

  HAL_FDCAN_Stop(p_can);

  if (is_data_bitrate) {
    p_can->Init.DataPrescaler = prescaler;
    p_can->Init.DataSyncJumpWidth = sjw;
    p_can->Init.DataTimeSeg1 = tseg1;
    p_can->Init.DataTimeSeg2 = tseg2;
  } else {
    p_can->Init.NominalPrescaler = prescaler;
    p_can->Init.NominalSyncJumpWidth = sjw;
    p_can->Init.NominalTimeSeg1 = tseg1;
    p_can->Init.NominalTimeSeg2 = tseg2;
  }

  if (HAL_FDCAN_Init(p_can) != HAL_OK) {
    assert(0);
  }

  HAL_FDCAN_Start(p_can);
}

void pcan_can_set_bitrate_ex(int bus, uint16_t brp, uint8_t tseg1,
                             uint8_t tseg2, uint8_t sjw) {
  FDCAN_HandleTypeDef *p_can = can_dev_array[bus].dev;

  if (!p_can)
    return;

  HAL_FDCAN_Stop(p_can);

  p_can->Init.NominalPrescaler = brp;
  p_can->Init.NominalSyncJumpWidth = sjw;
  p_can->Init.NominalTimeSeg1 = tseg1;
  p_can->Init.NominalTimeSeg2 = tseg2;

  if (HAL_FDCAN_Init(p_can) != HAL_OK) {
    assert(0);
  }

  HAL_FDCAN_Start(p_can);
}

static void pcan_can_tx_complete(int bus) { ++can_dev_array[bus].tx_msgs; }

static void pcan_can_tx_err(int bus) { ++can_dev_array[bus].tx_errs; }

int pcan_can_stats(int bus, struct t_can_stats *p_stats) {
  struct t_can_dev *p_dev = &can_dev_array[bus];

  p_stats->tx_msgs = p_dev->tx_msgs;
  p_stats->tx_errs = p_dev->tx_errs;
  p_stats->rx_msgs = p_dev->rx_msgs;
  p_stats->rx_errs = p_dev->rx_errs;
  p_stats->rx_ovfs = p_dev->rx_ovfs;

  return sizeof(struct t_can_stats);
}

void pcan_can_poll(void) {
  static uint32_t err_last_check = 0;
  uint32_t ts_ms;

  ts_ms = pcan_timestamp_millis();

#if (CAN_WITHOUT_ISR == 1)
  HAL_FDCAN_IRQHandler(&hfdcan1);
  HAL_FDCAN_IRQHandler(&hfdcan2);
#endif

  pcan_can_flush_tx(CAN_BUS_1);
  pcan_can_flush_tx(CAN_BUS_2);

  if ((uint32_t)(ts_ms - err_last_check) > 250) {
    err_last_check = ts_ms;
    for (int i = 0; i < CAN_BUS_TOTAL; i++) {
      if (!can_dev_array[i].err_handler)
        continue;
      FDCAN_HandleTypeDef *pcan = can_dev_array[i].dev;
      if (!pcan)
        continue;

      uint32_t psr = pcan->Instance->PSR;
      if (can_dev_array[i].esr_reg != psr) {
        can_dev_array[i].esr_reg = psr;
        can_dev_array[i].err_handler(i, can_dev_array[i].esr_reg);
      }
    }
  }
}

static void pcan_can_isr_frame(FDCAN_HandleTypeDef *hcan, uint32_t fifo) {
  FDCAN_RxHeaderTypeDef hdr;
  const int bus = _bus_from_int_dev(hcan->Instance);
  struct t_can_dev *const p_dev = &can_dev_array[bus];
  struct t_can_msg msg = {0};
  uint32_t rx_location;

  if (fifo == FDCAN_RX_FIFO0)
    rx_location = FDCAN_RX_FIFO0;
  else
    rx_location = FDCAN_RX_FIFO1;

  if (HAL_FDCAN_GetRxMessage(hcan, rx_location, &hdr, msg.data) != HAL_OK)
    return;

  // Convert ID
  if (hdr.IdType == FDCAN_STANDARD_ID) {
    msg.id = hdr.Identifier;
  } else {
    msg.id = hdr.Identifier;
    msg.flags |= MSG_FLAG_EXT;
  }

  // Check RTR
  if (hdr.RxFrameType == FDCAN_REMOTE_FRAME) {
    msg.flags |= MSG_FLAG_RTR;
  }

  // CAN FD specific flags
  if (hdr.FDFormat == FDCAN_FD_CAN) {
    msg.flags |= MSG_FLAG_FD;

    if (hdr.BitRateSwitch == FDCAN_BRS_ON)
      msg.flags |= MSG_FLAG_BRS;

    if (hdr.ErrorStateIndicator == FDCAN_ESI_PASSIVE)
      msg.flags |= MSG_FLAG_ESI;
  }

  msg.size = dlc_to_bytes(hdr.DataLength);
  msg.timestamp = pcan_timestamp_us();

  if (p_dev->rx_isr) {
    if (p_dev->rx_isr(bus, &msg) < 0) {
      ++p_dev->rx_ovfs;
      printf("CAN%d RX OVF\r\n", bus + 1);
      return;
    }
  }
  printf("CAN%d RX: ID=0x%X Len=%d\r\n", bus + 1, (unsigned int)msg.id,
         msg.size);
  ++p_dev->rx_msgs;
}

/* HAL MSP Init/DeInit are handled by CubeMX-generated code in fdcan.c */
/* Removed duplicate functions to avoid conflicts */

/* FDCAN HAL Callbacks */
void HAL_FDCAN_TxFifoEmptyCallback(FDCAN_HandleTypeDef *hcan) {
  int bus = _bus_from_int_dev(hcan->Instance);
  pcan_can_tx_complete(bus);
}

void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hcan, uint32_t RxFifo0ITs) {
  if (RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) {
    pcan_can_isr_frame(hcan, FDCAN_RX_FIFO0);
  }
}

void HAL_FDCAN_RxFifo1Callback(FDCAN_HandleTypeDef *hcan, uint32_t RxFifo1ITs) {
  if (RxFifo1ITs & FDCAN_IT_RX_FIFO1_NEW_MESSAGE) {
    pcan_can_isr_frame(hcan, FDCAN_RX_FIFO1);
  }
}

void HAL_FDCAN_ErrorCallback(FDCAN_HandleTypeDef *hcan) {
  int bus = _bus_from_int_dev(hcan->Instance);
  pcan_can_tx_err(bus);
}

void HAL_FDCAN_ErrorStatusCallback(FDCAN_HandleTypeDef *hcan,
                                   uint32_t ErrorStatusITs) {
  int bus = _bus_from_int_dev(hcan->Instance);

  if (ErrorStatusITs & FDCAN_IT_BUS_OFF) {
    ++can_dev_array[bus].tx_errs;
  }

  if (can_dev_array[bus].err_handler) {
    can_dev_array[bus].err_handler(bus, hcan->Instance->PSR);
  }
  printf("CAN%d ERR: Status=0x%X\r\n", bus + 1, (unsigned int)ErrorStatusITs);
}

/* ISR Handlers */
#if (CAN_WITHOUT_ISR == 0)
void FDCAN1_IT0_IRQHandler(void) { HAL_FDCAN_IRQHandler(&hcan[CAN_BUS_1]); }
void FDCAN1_IT1_IRQHandler(void) { HAL_FDCAN_IRQHandler(&hcan[CAN_BUS_1]); }
void FDCAN2_IT0_IRQHandler(void) { HAL_FDCAN_IRQHandler(&hcan[CAN_BUS_2]); }
void FDCAN2_IT1_IRQHandler(void) { HAL_FDCAN_IRQHandler(&hcan[CAN_BUS_2]); }
#endif