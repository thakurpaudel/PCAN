#include "pcanpro_can.h"
#include "fdcan.h"
#include "main.h"
#include "pcanpro_timestamp.h"
#include "pcanpro_variant.h"
#include <assert.h>
#include <stdio.h>
#include <stdbool.h>

// Use external FDCAN handles from CubeMX-generated fdcan.c
extern FDCAN_HandleTypeDef hfdcan1;
extern FDCAN_HandleTypeDef hfdcan2;

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

#define CAN_WITHOUT_ISR 1

// Convert DLC to actual data length
static const uint8_t DLC_to_bytes[] = {0, 1,  2,  3,  4,  5,  6,  7,
                                       8, 12, 16, 20, 24, 32, 48, 64};

static uint32_t bytes_to_dlc(uint8_t bytes) {
  if (bytes <= 8)
    return bytes; 
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

static uint32_t abs_diff_u32(uint32_t a, uint32_t b)
{
    return (a > b) ? (a - b) : (b - a);
}
/* helper functions*/
 /*
 * sample point:
 *   sample_point = (1 + tseg1) / (1 + tseg1 + tseg2)
 *
 * Inputs:
 *   can_clock_hz                -> STM32 FDCAN kernel clock, e.g. 120000000
 *   target_bitrate              -> e.g. 250000, 500000, 1000000
 *   target_sample_point_permille-> e.g. 875 = 87.5%
 *   out                         -> result timing
 *
 * Returns:
 *   true if a valid timing was found
 */
static bool stm32h7_fdcan_calc_bittiming(uint32_t can_clock_hz,
                                         uint32_t target_bitrate,
                                         uint32_t target_sample_point_permille,
                                         struct t_can_bitrate *out)
{
    bool found = false;
    uint32_t best_bitrate_error = 0xFFFFFFFFU;
    uint32_t best_sp_error = 0xFFFFFFFFU;

    if (out == NULL || can_clock_hz == 0U || target_bitrate == 0U)
    {
        return false;
    }

    /*
     * Practical search range for STM32H7 nominal timing.
     * Kept simple and readable.
     */
    for (uint32_t brp = 1U; brp <= 512U; brp++)
    {
        for (uint32_t tseg1 = 1U; tseg1 <= 255U; tseg1++)
        {
            for (uint32_t tseg2 = 1U; tseg2 <= 127U; tseg2++)
            {
                uint32_t total_tq = 1U + tseg1 + tseg2;
                uint64_t denom = (uint64_t)brp * (uint64_t)total_tq;

                if (denom == 0U)
                {
                    continue;
                }

                uint32_t actual_bitrate = (uint32_t)((uint64_t)can_clock_hz / denom);
                if (actual_bitrate == 0U)
                {
                    continue;
                }

                uint32_t bitrate_error = abs_diff_u32(actual_bitrate, target_bitrate);

                uint32_t sample_point_permille =
                    (uint32_t)(((uint64_t)(1U + tseg1) * 1000U) / total_tq);

                uint32_t sp_error =
                    abs_diff_u32(sample_point_permille, target_sample_point_permille);

                /*
                 * Prefer lower bitrate error first.
                 * If same, prefer closer sample point.
                 */
                if (!found ||
                    (bitrate_error < best_bitrate_error) ||
                    ((bitrate_error == best_bitrate_error) && (sp_error < best_sp_error)))
                {
                    found = true;
                    best_bitrate_error = bitrate_error;
                    best_sp_error = sp_error;

                    out->brp = brp;
                    out->tseg1 = tseg1;
                    out->tseg2 = tseg2;

                    /*
                     * SJW rule:
                     *   - must be <= tseg2
                     *   - 4 is usually a safe practical choice
                     */
                    out->sjw = (tseg2 >= 4U) ? 4U : tseg2;
                }

                /*
                 * Perfect match, stop early.
                 */
                if ((bitrate_error == 0U) && (sp_error == 0U))
                {
                    return true;
                }
            }
        }
    }

    return found;
}



static void pcan_can_dump_status(FDCAN_HandleTypeDef *hcan, int bus) {
  uint32_t psr = hcan->Instance->PSR;
  uint32_t ecr = hcan->Instance->ECR;
  uint32_t txfqs = hcan->Instance->TXFQS;

  printf("CAN%d PSR=0x%08lX ECR=0x%08lX TXFQS=0x%08lX\r\n", bus + 1,
         (unsigned long)psr, (unsigned long)ecr, (unsigned long)txfqs);

  printf("CAN%d LEC=%lu ACT=%lu EP=%lu EW=%lu BO=%lu TEC=%lu REC=%lu\r\n",
         bus + 1, (unsigned long)((psr >> 0) & 0x7),
         (unsigned long)((psr >> 3) & 0x3), (unsigned long)((psr >> 5) & 0x1),
         (unsigned long)((psr >> 6) & 0x1), (unsigned long)((psr >> 7) & 0x1),
         (unsigned long)((ecr >> 0) & 0xFF),
         (unsigned long)((ecr >> 8) & 0x7F));
}

static uint8_t dlc_to_bytes(uint32_t dlc) {
  uint32_t code = dlc & 0x0F; 
  if (code < 16)
    return DLC_to_bytes[code];
  return 8;
}

/* --------------- HAL PART ------------- */
static int _bus_from_int_dev(FDCAN_GlobalTypeDef *can) {
  if (can == FDCAN1)
    return CAN_BUS_1;
  else if (can == FDCAN2)
    return CAN_BUS_2;
  return CAN_BUS_1;
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
      pcan_can_dump_status(p_can, _bus_from_int_dev(p_can->Instance));
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

  // Throttled TX log: printing every frame over UART (~1ms/line at 115200
  // baud) would block the poll loop and cause SW ring buffer overflow,
  // especially with 8-byte payloads. Log every 100 frames instead.
  static uint32_t tx_log_counter = 0;
  if (++tx_log_counter % 100 == 0) {
    uint8_t payload_len = (fd_mode && (p_msg->flags & MSG_FLAG_FD))
                              ? p_msg->size
                              : (p_msg->size > 8 ? 8 : p_msg->size);
    printf("CAN%d HW TX [%lu]: ID=0x%lx LEN=%u Data: ",
           (int)(_bus_from_int_dev(p_can->Instance) + 1), tx_log_counter,
           (unsigned long)msg.Identifier, payload_len);
    for (uint8_t i = 0; i < payload_len; i++) {
      printf("%02X ", p_msg->data[i]);
    }
    printf("\r\n");
  }

  return 0;
}

static void pcan_can_flush_tx(int bus) {
  struct t_can_dev *p_dev = &can_dev_array[bus];
  FDCAN_HandleTypeDef *hcan = (FDCAN_HandleTypeDef *)p_dev->dev;

  if (!hcan)
    return;

  // Drain as many frames as the HW TX FIFO has space for in one poll cycle.
  // Previously this only sent ONE frame per call, causing the SW queue to
  // stall when bursts arrived faster than the poll rate (FIFO full error).
  while (p_dev->tx_head != p_dev->tx_tail) {
    if (HAL_FDCAN_GetTxFifoFreeLevel(hcan) == 0) {
      // HW FIFO full - come back next poll cycle
      break;
    }

    struct t_can_msg *p_msg = &p_dev->tx_fifo[p_dev->tx_tail];

    if (_can_send(hcan, p_msg, p_dev->fd_mode) != 0) {
      // TX error - stop and retry next cycle
      break;
    }

    if (p_dev->tx_isr) {
      (void)p_dev->tx_isr(bus, p_msg);
    }

    ++p_dev->tx_msgs;
    p_dev->tx_tail = (p_dev->tx_tail + 1) & (CAN_TX_FIFO_SIZE - 1);
  }
}

int pcan_can_write(int bus, struct t_can_msg *p_msg) {
  struct t_can_dev *p_dev = &can_dev_array[bus];

  if (!p_dev)
    return 0;

  if (!p_msg)
    return 0;

  uint32_t tx_head_next = (p_dev->tx_head + 1) & (CAN_TX_FIFO_SIZE - 1);
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
// Assuming FDCAN kernel clock = 120 MHz
// Adjust based on your actual clock configuration

// static int _get_precalculated_bitrate(uint32_t bitrate, uint32_t *prescaler,
//                                       uint32_t *tseg1, uint32_t *tseg2,
//                                       uint32_t *sjw) {
//   *sjw = 4; // Use a wider SJW for better sync

//   // These values assume 120 MHz FDCAN clock (PLL1_Q)
//   // Target sample point ~80-87%
//   // Formula: 120MHz / (Prescaler * (1 + Tseg1 + Tseg2))
//   switch (bitrate) {
//   case 1000000u:
//     *prescaler = 6;
//     *tseg1 = 15;
//     *tseg2 = 4;
//     break;
//   case 800000u:
//     *prescaler = 15;
//     *tseg1 = 7;
//     *tseg2 = 2;
//     break;
//   case 500000u:
//     *prescaler = 12;
//     *tseg1 = 15;
//     *tseg2 = 4;
//     break;
//   case 250000u:
//     *prescaler = 24;
//     *tseg1 = 15;
//     *tseg2 = 4;
//     break;
//   case 125000u:
//     *prescaler = 48;
//     *tseg1 = 15;
//     *tseg2 = 4;
//     break;
//   case 100000u:
//     *prescaler = 60;
//     *tseg1 = 15;
//     *tseg2 = 4;
//     break;
//   case 50000u:
//     *prescaler = 120;
//     *tseg1 = 15;
//     *tseg2 = 4;
//     break;
//   default:
//     // Fallback for 500k
//     *prescaler = 12;
//     *tseg1 = 15;
//     *tseg2 = 4;
//     break;
//   }

//   return 0;
// }

static int _get_precalculated_bitrate(uint32_t bitrate, uint32_t *prescaler,
                                      uint32_t *tseg1, uint32_t *tseg2,
                                      uint32_t *sjw) {
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
    *prescaler = 12;
    *tseg1 = 15;
    *tseg2 = 4;
    break;
  }

  *sjw = (*tseg2 >= 4) ? 4 : *tseg2;
  return 0;
}

int pcan_can_init_ex(int bus, uint32_t bitrate) {
  FDCAN_HandleTypeDef *p_can = can_dev_array[bus].dev;
  uint32_t prescaler, tseg1, tseg2, sjw;

  if (!p_can)
    return 0;

  printf("=== CAN%d Init Start (POLLING MODE) ===\r\n", bus + 1);

  HAL_FDCAN_DeInit(p_can);

  // Frame format: Classic CAN by default
  p_can->Init.FrameFormat = FDCAN_FRAME_CLASSIC;
  can_dev_array[bus].fd_mode = 0;

  p_can->Init.Mode = FDCAN_MODE_NORMAL;
  // ENABLE: Retransmit failed frames so the error counter (TEC) does not
  // rise. With TxFifoQueueElmtsNbr=3, at most 3 frames can block at once
  // instead of 32, so the deadlock window is tiny.
  p_can->Init.AutoRetransmission = ENABLE;
  // DISABLE: TransmitPause adds a 2-bit gap between frames. Not needed here;
  // keeping DISABLE restores known-good init behaviour.
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
  // 3 slots: STM32H7 FDCAN has 3 physical TX buffers. Keeping HW FIFO small
  // means the 256-entry SW ring buffer (pcan_can_flush_tx loop) controls
  // pacing. A large HW FIFO of 32 hides saturation and stalls 32 frames at
  // once when the bus goes silent.
  p_can->Init.TxFifoQueueElmtsNbr = 3;
  p_can->Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;
  p_can->Init.TxElmtSize = FDCAN_DATA_BYTES_8;

  if (HAL_FDCAN_Init(p_can) != HAL_OK) {
    printf("CAN%d: HAL_FDCAN_Init FAILED!\r\n", bus + 1);
    return -1;
  }
  printf("CAN%d: HAL_FDCAN_Init OK\r\n", bus + 1);

  // Permissive filter for debugging: Accept all messages
  if (HAL_FDCAN_ConfigGlobalFilter(
          p_can, FDCAN_ACCEPT_IN_RX_FIFO0, FDCAN_ACCEPT_IN_RX_FIFO0,
          FDCAN_FILTER_REMOTE, FDCAN_FILTER_REMOTE) != HAL_OK) {
    printf("CAN%d: Global Filter Config FAILED!\r\n", bus + 1);
    return -1;
  }
  printf("CAN%d: Global Filter Config OK (Accept All)\r\n", bus + 1);

  // IMPORTANT: DO NOT activate notifications in polling mode!
  // Notifications are for interrupt-driven mode only
  // Removed: HAL_FDCAN_ActivateNotification(p_can, FDCAN_IT_FLAGS, 0);

  if (HAL_FDCAN_Start(p_can) != HAL_OK) {
    printf("CAN%d: HAL_FDCAN_Start FAILED!\r\n", bus + 1);
    return -1;
  }

  // Check initial status
  uint32_t psr = p_can->Instance->PSR;
  printf("CAN%d: Started OK, PSR=0x%08lX\r\n", bus + 1, psr);
  printf("CAN%d: Init Classic @ %u bps (POLLING MODE - No ISR)\r\n", bus + 1,
         (unsigned int)bitrate);
  printf("CAN%d: Prescaler=%lu, TSEG1=%lu, TSEG2=%lu, SJW=%lu\r\n", bus + 1,
         prescaler, tseg1, tseg2, sjw);

  return 0;
}

// static int _get_precalculated_bitrate(uint32_t bitrate, uint32_t *prescaler,
//                                       uint32_t *tseg1, uint32_t *tseg2,
//                                       uint32_t *sjw) {
//     switch (bitrate) {
//     case 1000000u:
//         *prescaler = 6;
//         *tseg1 = 15;
//         *tseg2 = 4;
//         *sjw = 4;
//         break;
//     case 800000u:
//         *prescaler = 10;   // Changed from 15
//         *tseg1 = 11;       // Adjusted
//         *tseg2 = 3;        // Adjusted
//         *sjw = 3;          // Fixed
//         // 120MHz / (10×15) = 800kHz, SP=80%
//         break;
//     case 500000u:
//         *prescaler = 12;
//         *tseg1 = 15;
//         *tseg2 = 4;
//         *sjw = 4;
//         break;
//     case 250000u:
//         *prescaler = 20;   // Changed from 24 - EVEN NUMBER
//         *tseg1 = 18;       // Adjusted
//         *tseg2 = 5;        // Adjusted
//         *sjw = 5;          // Adjusted
//         // 120MHz / (20×24) = 250kHz, SP=79.2%
//         break;
//     case 125000u:
//         *prescaler = 48;
//         *tseg1 = 15;
//         *tseg2 = 4;
//         *sjw = 4;
//         break;
//     case 100000u:
//         *prescaler = 60;
//         *tseg1 = 15;
//         *tseg2 = 4;
//         *sjw = 4;
//         break;
//     case 50000u:
//         *prescaler = 120;
//         *tseg1 = 15;
//         *tseg2 = 4;
//         *sjw = 4;
//         break;
//     default:
//         *prescaler = 12;
//         *tseg1 = 15;
//         *tseg2 = 4;
//         *sjw = 4;
//         break;
//     }
//     return 0;
// }
// int pcan_can_init_ex(int bus, uint32_t bitrate) {
//   FDCAN_HandleTypeDef *p_can = can_dev_array[bus].dev;
//   uint32_t prescaler, tseg1, tseg2, sjw;

//   if (!p_can)
//     return 0;

//   printf("=== CAN%d Init Start (POLLING MODE) ===\r\n", bus + 1);

//   // ✓ Verify clock source
//   uint32_t fdcan_clk = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_FDCAN);
//   printf("CAN%d: Kernel Clock = %lu Hz (expected 120000000)\r\n",
//          bus + 1, fdcan_clk);

//   if (fdcan_clk != 120000000) {
//     printf("CAN%d: WARNING - Clock mismatch! Bit timing will be wrong!\r\n",
//            bus + 1);
//   }

//   HAL_FDCAN_DeInit(p_can);

//   // Frame format: Classic CAN
//   p_can->Init.FrameFormat = FDCAN_FRAME_CLASSIC;
//   can_dev_array[bus].fd_mode = 0;
//   p_can->Init.Mode = FDCAN_MODE_NORMAL;

//   // ✓ FIXED: Enable auto-retransmission for reliability
//   p_can->Init.AutoRetransmission = ENABLE;

//   p_can->Init.TransmitPause = DISABLE;
//   p_can->Init.ProtocolException = DISABLE;

//   // Nominal bit timing
//   _get_precalculated_bitrate(bitrate, &prescaler, &tseg1, &tseg2, &sjw);

//   p_can->Init.NominalPrescaler = prescaler;
//   p_can->Init.NominalSyncJumpWidth = sjw;
//   p_can->Init.NominalTimeSeg1 = tseg1;
//   p_can->Init.NominalTimeSeg2 = tseg2;

//   // ✓ FIXED: Data bit timing for Classic CAN
//   // Option 1: Set to minimum valid values (recommended for Classic CAN)
//   p_can->Init.DataPrescaler = 1;
//   p_can->Init.DataSyncJumpWidth = 1;
//   p_can->Init.DataTimeSeg1 = 1;
//   p_can->Init.DataTimeSeg2 = 1;

//   // Option 2: Match nominal (your original approach - also valid)
//   // p_can->Init.DataPrescaler = prescaler;
//   // p_can->Init.DataSyncJumpWidth = sjw;
//   // p_can->Init.DataTimeSeg1 = tseg1;
//   // p_can->Init.DataTimeSeg2 = tseg2;

//   // Message RAM configuration
//   p_can->Init.MessageRAMOffset = (bus == CAN_BUS_1) ? 0 : 1280;
//   p_can->Init.StdFiltersNbr = 28;
//   p_can->Init.ExtFiltersNbr = 8;
//   p_can->Init.RxFifo0ElmtsNbr = 16;
//   p_can->Init.RxFifo0ElmtSize = FDCAN_DATA_BYTES_8;
//   p_can->Init.RxFifo1ElmtsNbr = 16;
//   p_can->Init.RxFifo1ElmtSize = FDCAN_DATA_BYTES_8;
//   p_can->Init.RxBuffersNbr = 0;
//   p_can->Init.RxBufferSize = FDCAN_DATA_BYTES_8;
//   p_can->Init.TxEventsNbr = 0;
//   p_can->Init.TxBuffersNbr = 0;
//   p_can->Init.TxFifoQueueElmtsNbr = 16;
//   p_can->Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;
//   p_can->Init.TxElmtSize = FDCAN_DATA_BYTES_8;

//   if (HAL_FDCAN_Init(p_can) != HAL_OK) {
//     printf("CAN%d: HAL_FDCAN_Init FAILED! Error=0x%08lX\r\n",
//            bus + 1, p_can->ErrorCode);
//     return -1;
//   }
//   printf("CAN%d: HAL_FDCAN_Init OK\r\n", bus + 1);

//   // Permissive filter
//   if (HAL_FDCAN_ConfigGlobalFilter(
//           p_can, FDCAN_ACCEPT_IN_RX_FIFO0, FDCAN_ACCEPT_IN_RX_FIFO0,
//           FDCAN_FILTER_REMOTE, FDCAN_FILTER_REMOTE) != HAL_OK) {
//     printf("CAN%d: Global Filter Config FAILED!\r\n", bus + 1);
//     return -1;
//   }
//   printf("CAN%d: Global Filter Config OK (Accept All)\r\n", bus + 1);

//   if (HAL_FDCAN_Start(p_can) != HAL_OK) {
//     printf("CAN%d: HAL_FDCAN_Start FAILED! Error=0x%08lX\r\n",
//            bus + 1, p_can->ErrorCode);
//     return -1;
//   }

//   // ✓ Enhanced status reporting
//   uint32_t psr = p_can->Instance->PSR;
//   uint32_t ecr = p_can->Instance->ECR;

//   printf("CAN%d: Started OK\r\n", bus + 1);
//   printf("  PSR=0x%08lX (LEC=%lu, ACT=%lu, EP=%lu, EW=%lu, BO=%lu)\r\n",
//          psr,
//          (psr >> 0) & 0x7,   // LEC - Last Error Code
//          (psr >> 3) & 0x3,   // ACT - Activity
//          (psr >> 5) & 0x1,   // EP - Error Passive
//          (psr >> 6) & 0x1,   // EW - Error Warning
//          (psr >> 7) & 0x1);  // BO - Bus Off
//   printf("  ECR=0x%08lX (TEC=%lu, REC=%lu)\r\n",
//          ecr,
//          (ecr >> 0) & 0xFF,  // TX Error Counter
//          (ecr >> 8) & 0x7F); // RX Error Counter

//   printf("CAN%d: Classic CAN @ %u bps (POLLING MODE)\r\n",
//          bus + 1, (unsigned int)bitrate);
//   printf("  Nominal: Prescaler=%lu, TSEG1=%lu, TSEG2=%lu, SJW=%lu\r\n",
//          prescaler, tseg1, tseg2, sjw);
//   printf("  Sample Point = %.1f%%\r\n",
//          100.0 * (1.0 + tseg1) / (1.0 + tseg1 + tseg2));

//   return 0;
// }

// Simple initialization for Classic CAN (alias for pcan_can_init_ex)
int pcan_can_init(int bus, uint32_t bitrate) {
  return pcan_can_init_ex(bus, bitrate);
}

// Initialize with CAN FD support
int pcan_can_init_fd(int bus, uint32_t nom_bitrate, uint32_t data_bitrate) {
  FDCAN_HandleTypeDef *p_can = can_dev_array[bus].dev;
  uint32_t prescaler, tseg1, tseg2, sjw;
  uint32_t data_prescaler, data_tseg1, data_tseg2, data_sjw;

  if (!p_can)
    return 0;

  printf("=== CAN%d FD Init Start (POLLING MODE) ===\r\n", bus + 1);

  HAL_FDCAN_DeInit(p_can);

  // Frame format: FD CAN with BRS
  p_can->Init.FrameFormat = FDCAN_FRAME_FD_BRS;
  can_dev_array[bus].fd_mode = 1;

  p_can->Init.Mode = FDCAN_MODE_NORMAL;
  p_can->Init.AutoRetransmission = ENABLE;
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
  p_can->Init.TxFifoQueueElmtsNbr = 32;
  p_can->Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;
  p_can->Init.TxElmtSize = FDCAN_DATA_BYTES_64;

  if (HAL_FDCAN_Init(p_can) != HAL_OK) {
    printf("CAN%d FD: HAL_FDCAN_Init FAILED!\r\n", bus + 1);
    return -1;
  }
  printf("CAN%d FD: HAL_FDCAN_Init OK\r\n", bus + 1);

  // Permissive filter for debugging: Accept all
  if (HAL_FDCAN_ConfigGlobalFilter(
          p_can, FDCAN_ACCEPT_IN_RX_FIFO0, FDCAN_ACCEPT_IN_RX_FIFO0,
          FDCAN_FILTER_REMOTE, FDCAN_FILTER_REMOTE) != HAL_OK) {
    printf("CAN%d FD: Global Filter Config FAILED!\r\n", bus + 1);
    return -1;
  }

  // DO NOT activate notifications in polling mode!
  // Removed: HAL_FDCAN_ActivateNotification(p_can, FDCAN_IT_FLAGS, 0);

  if (HAL_FDCAN_Start(p_can) != HAL_OK) {
    printf("CAN%d FD: HAL_FDCAN_Start FAILED!\r\n", bus + 1);
    return -1;
  }

  uint32_t psr = p_can->Instance->PSR;
  printf("CAN%d FD: Started OK, PSR=0x%08lX\r\n", bus + 1, psr);
  printf("CAN%d: Init FD @ %u/%u bps (POLLING MODE)\r\n", bus + 1,
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
  printf("CAN%d: Silent mode %s\r\n", bus + 1, silent_mode ? "ON" : "OFF");
}

// FDCAN supports ISO/non-ISO mode
void pcan_can_set_iso_mode(int bus, uint8_t iso_mode) {
  FDCAN_HandleTypeDef *p_can = can_dev_array[bus].dev;

  if (!p_can)
    return;

  HAL_FDCAN_Stop(p_can);

  // ISO mode is the default, non-ISO uses different CRC
  // p_can->Init.FrameFormat =
  //     iso_mode ? FDCAN_FRAME_FD_BRS : FDCAN_FRAME_FD_NO_BRS;

  p_can->Init.FrameFormat = FDCAN_FRAME_FD_NO_BRS;
  if (HAL_FDCAN_Init(p_can) != HAL_OK) {
    assert(0);
  }

  HAL_FDCAN_Start(p_can);
  printf("CAN%d: ISO mode %s\r\n", bus + 1, iso_mode ? "ON" : "OFF");
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
  printf("CAN%d: Loopback mode %s\r\n", bus + 1, loopback ? "ON" : "OFF");
}

void pcan_can_set_bus_active(int bus, uint16_t mode) {
  FDCAN_HandleTypeDef *p_can = can_dev_array[bus].dev;

  if (!p_can)
    return;

  if (mode) {
    HAL_FDCAN_Start(p_can);
    printf("CAN%d: Bus ACTIVE\r\n", bus + 1);
  } else {
    HAL_FDCAN_Stop(p_can);
    printf("CAN%d: Bus STOPPED\r\n", bus + 1);
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
    printf("CAN%d: Data bitrate set to %lu bps\r\n", bus + 1, bitrate);
  } else {
    p_can->Init.NominalPrescaler = prescaler;
    p_can->Init.NominalSyncJumpWidth = sjw;
    p_can->Init.NominalTimeSeg1 = tseg1;
    p_can->Init.NominalTimeSeg2 = tseg2;
    printf("CAN%d: Nominal bitrate set to %lu bps\r\n", bus + 1, bitrate);
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
  printf("CAN%d: Bitrate set (BRP=%u, TSEG1=%u, TSEG2=%u, SJW=%u)\r\n", bus + 1,
         brp, tseg1, tseg2, sjw);
}

void pcan_can_set_bitrate_raw(int bus, uint16_t brp, uint8_t tseg1,
                              uint8_t tseg2, uint8_t sjw, int is_data) {
  FDCAN_HandleTypeDef *p_can = can_dev_array[bus].dev;

  if (!p_can)
    return;

  HAL_FDCAN_Stop(p_can);

  if (is_data) {
    p_can->Init.DataPrescaler = brp;
    p_can->Init.DataSyncJumpWidth = sjw;
    p_can->Init.DataTimeSeg1 = tseg1;
    p_can->Init.DataTimeSeg2 = tseg2;
  } else {
    p_can->Init.NominalPrescaler = brp;
    p_can->Init.NominalSyncJumpWidth = sjw;
    p_can->Init.NominalTimeSeg1 = tseg1;
    p_can->Init.NominalTimeSeg2 = tseg2;
  }

  if (HAL_FDCAN_Init(p_can) != HAL_OK) {
    assert(0);
  }

  HAL_FDCAN_Start(p_can);
  printf("CAN%d: Raw Bitrate set (%s, BRP=%u, TSEG1=%u, TSEG2=%u, SJW=%u)\r\n",
         bus + 1, is_data ? "DATA" : "NOMINAL", brp, tseg1, tseg2, sjw);
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

  if (HAL_FDCAN_GetRxMessage(hcan, rx_location, &hdr, msg.data) != HAL_OK) {
    printf("CAN%d: GetRxMessage FAILED (FIFO%ld)\r\n", bus + 1, fifo);
    return;
  }

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
  // printf("CAN%d RX: ID=0x%lX Len=%d Data=[", bus + 1, msg.id, msg.size);
  // for (int i = 0; i < msg.size; i++) {
  //   printf("%02X", msg.data[i]);
  //   if (i < msg.size - 1)
  //     printf(" ");
  // }
  // printf("]\r\n");

  if (p_dev->rx_isr) {
    if (p_dev->rx_isr(bus, &msg) < 0) {
      ++p_dev->rx_ovfs;
      printf("CAN%d RX OVF\r\n", bus + 1);
      return;
    }
  }

  // printf("CAN%d RX: ID=0x%lX Len=%d Data=[", bus + 1, msg.id, msg.size);
  // for (int i = 0; i < msg.size; i++) {
  //   printf("%02X", msg.data[i]);
  //   if (i < msg.size - 1)
  //     printf(" ");
  // }
  // printf("]\r\n");

  ++p_dev->rx_msgs;
}

void pcan_can_poll(void) {
  static uint32_t err_last_check = 0;
  uint32_t ts_ms;

  ts_ms = pcan_timestamp_millis();

  // POLLING MODE: Manually check RX FIFOs for all buses
  for (int bus = 0; bus < CAN_BUS_TOTAL; bus++) {
    FDCAN_HandleTypeDef *hcan = can_dev_array[bus].dev;
    if (!hcan)
      continue;

    // Check and process all messages in FIFO0
    uint32_t fifo0_level = HAL_FDCAN_GetRxFifoFillLevel(hcan, FDCAN_RX_FIFO0);
    while (fifo0_level > 0) {
      pcan_can_isr_frame(hcan, FDCAN_RX_FIFO0);
      fifo0_level = HAL_FDCAN_GetRxFifoFillLevel(hcan, FDCAN_RX_FIFO0);
    }

    // Check and process all messages in FIFO1
    uint32_t fifo1_level = HAL_FDCAN_GetRxFifoFillLevel(hcan, FDCAN_RX_FIFO1);
    while (fifo1_level > 0) {
      pcan_can_isr_frame(hcan, FDCAN_RX_FIFO1);
      fifo1_level = HAL_FDCAN_GetRxFifoFillLevel(hcan, FDCAN_RX_FIFO1);
    }
  }

  // Flush TX queues
  pcan_can_flush_tx(CAN_BUS_1);
  pcan_can_flush_tx(CAN_BUS_2);

  // Periodic CAN bus status debug (every 5 seconds)
  // static uint32_t last_status_print = 0;
  // if ((ts_ms - last_status_print) > 5000) {
  //   last_status_print = ts_ms;
  //   for (int i = 0; i < CAN_BUS_TOTAL; i++) {
  //     FDCAN_HandleTypeDef *pcan = can_dev_array[i].dev;
  //     if (!pcan)
  //       continue;

  //     // uint32_t psr = pcan->Instance->PSR;
  //     // uint32_t fifo0 = HAL_FDCAN_GetRxFifoFillLevel(pcan, FDCAN_RX_FIFO0);
  //     // uint32_t fifo1 = HAL_FDCAN_GetRxFifoFillLevel(pcan, FDCAN_RX_FIFO1);
  //     // uint32_t tx_free = HAL_FDCAN_GetTxFifoFreeLevel(pcan);

  //     // printf("==== CAN%d Status ====\r\n", i + 1);
  //     // printf("  PSR: 0x%08lX\r\n", psr);
  //     // printf("  FIFO0: %lu msg, FIFO1: %lu msg\r\n", fifo0, fifo1);
  //     // printf("  TX Free: %lu\r\n", tx_free);
  //     // printf("  RX: %lu, TX: %lu, ERR: %lu, OVF: %lu\r\n",
  //     //        can_dev_array[i].rx_msgs, can_dev_array[i].tx_msgs,
  //     //        can_dev_array[i].tx_errs, can_dev_array[i].rx_ovfs);
  //   }
  // }

  // Error status reporting (every 250ms)
  // NOTE: Bus-off recovery via Stop/Start was removed because it was
  // unconditionally resetting the FDCAN peripheral every 250ms, breaking all
  // transmission. Bus-off should not occur with AutoRetransmission=ENABLE on
  // a healthy bus. If bus-off is ever seen in the UART log, perform a full
  // re-init by calling pcan_can_init_ex() from the application layer.
  if ((uint32_t)(ts_ms - err_last_check) > 250) {
    err_last_check = ts_ms;
    for (int i = 0; i < CAN_BUS_TOTAL; i++) {
      // Only report if an error handler is registered
      if (!can_dev_array[i].err_handler)
        continue;
      FDCAN_HandleTypeDef *pcan = can_dev_array[i].dev;
      if (!pcan)
        continue;

      uint32_t psr = pcan->Instance->PSR;
      if (can_dev_array[i].esr_reg != psr) {
        can_dev_array[i].esr_reg = psr;
        can_dev_array[i].err_handler(i, psr);
      }
    }
  }
}

/* HAL MSP Init/DeInit are handled by CubeMX-generated code in fdcan.c */
/* These callbacks are NOT needed in polling mode, but kept for compatibility */

/* FDCAN HAL Callbacks - These will NOT be called in polling mode! */
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
  printf("CAN%d: Error Callback\r\n", bus + 1);
}

void HAL_FDCAN_ErrorStatusCallback(FDCAN_HandleTypeDef *hcan,
                                   uint32_t ErrorStatusITs) {
  int bus = _bus_from_int_dev(hcan->Instance);

  if (ErrorStatusITs & FDCAN_IT_BUS_OFF) {
    ++can_dev_array[bus].tx_errs;
    printf("CAN%d: BUS OFF!\r\n", bus + 1);
  }

  if (can_dev_array[bus].err_handler) {
    can_dev_array[bus].err_handler(bus, hcan->Instance->PSR);
  }
  printf("CAN%d ERR: Status=0x%lX\r\n", bus + 1, ErrorStatusITs);
}