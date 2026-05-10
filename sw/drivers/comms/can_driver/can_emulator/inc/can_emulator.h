/**
 * @file can_emulator.h
 * @brief In-memory virtual CAN bus emulator for SIL testing.
 */

#ifndef CAN_EMULATOR_H
#define CAN_EMULATOR_H

#include "can_frame.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum registered nodes in the emulator. */
#define CAN_EMULATOR_MAX_NODES      16U
/** Maximum queued transmit frames awaiting arbitration. */
#define CAN_EMULATOR_MAX_PENDING_TX 64U
/** Maximum receive queue depth per node. */
#define CAN_EMULATOR_MAX_RX_QUEUE   64U
/** Emulator payload size aligned to shared CAN frame payload size. */
#define CAN_EMULATOR_MAX_DATA_LEN   CAN_FRAME_MAX_DATA_LEN

/** Emulator node identifier type. */
typedef uint8_t CanNodeId;

/**
 * @brief One pending transmit entry.
 */
typedef struct
{
  /** CAN frame to arbitrate and route. */
  CanFrame frame;
  /** Node that submitted this frame. */
  CanNodeId sender;
  /** Submission sequence number for deterministic tie-break. */
  uint32_t sequence;
} CanEmulatorTxEntry;

/**
 * @brief One received frame entry for a node queue.
 */
typedef struct
{
  /** Received frame payload. */
  CanFrame frame;
  /** Sender node ID of the received frame. */
  CanNodeId sender;
} CanEmulatorRxEntry;

/**
 * @brief Per-node emulator state.
 */
typedef struct
{
  /** Node ID for this slot. */
  CanNodeId node_id;
  /** Slot usage flag. */
  bool active;
  /** Circular receive queue. */
  CanEmulatorRxEntry rx_queue[CAN_EMULATOR_MAX_RX_QUEUE];
  /** Receive queue head index. */
  size_t rx_head;
  /** Number of queued receive entries. */
  size_t rx_count;
} CanEmulatorNode;

/**
 * @brief Virtual CAN bus emulator state.
 */
typedef struct
{
  /** Registered nodes table. */
  CanEmulatorNode nodes[CAN_EMULATOR_MAX_NODES];
  /** Pending transmit frames awaiting arbitration. */
  CanEmulatorTxEntry pending_tx[CAN_EMULATOR_MAX_PENDING_TX];
  /** Number of pending transmit entries. */
  size_t pending_tx_count;
  /** Next submission sequence counter. */
  uint32_t next_sequence;
} CanEmulator;

/**
 * @brief Initializes the emulator state.
 *
 * @param emulator Emulator instance.
 */
void can_emulator_init(CanEmulator *emulator);

/**
 * @brief Registers a node on the virtual bus.
 *
 * @param emulator Emulator instance.
 * @param node_id Node ID to register.
 * @return true on success, false if no slot is available or input is invalid.
 */
bool can_emulator_register_node(CanEmulator *emulator, CanNodeId node_id);

/**
 * @brief Submits a frame for arbitration.
 *
 * @param emulator Emulator instance.
 * @param sender Sender node ID.
 * @param frame Frame to enqueue.
 * @return true on success, false otherwise.
 */
bool can_emulator_submit(CanEmulator *emulator, CanNodeId sender, const CanFrame *frame);

/**
 * @brief Processes one arbitration/routing step.
 *
 * @param emulator Emulator instance.
 * @return true if one frame was processed, false if queue is empty or input invalid.
 */
bool can_emulator_step(CanEmulator *emulator);

/**
 * @brief Receives one queued frame for a node.
 *
 * @param emulator Emulator instance.
 * @param receiver Receiver node ID.
 * @param out_frame Output frame buffer.
 * @param out_sender Optional output sender node ID.
 * @return true if one frame was dequeued.
 */
bool can_emulator_receive(CanEmulator *emulator, CanNodeId receiver, CanFrame *out_frame,
                          CanNodeId *out_sender);

/**
 * @brief Returns the number of pending transmit frames.
 *
 * @param emulator Emulator instance.
 * @return Pending transmit queue size, or 0 for invalid input.
 */
size_t can_emulator_pending_tx_count(const CanEmulator *emulator);

#ifdef __cplusplus
}
#endif

#endif /* CAN_EMULATOR_H */
