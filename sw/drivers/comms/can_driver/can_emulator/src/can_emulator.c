#include "can_emulator.h"

#include <string.h>

static bool is_valid_frame(const CanFrame *frame)
{
  return can_frame_is_valid(frame);
}

static CanEmulatorNode *find_node(CanEmulator *emulator, CanNodeId node_id)
{
  size_t i;

  if (emulator == NULL)
  {
    return NULL;
  }

  for (i = 0; i < CAN_EMULATOR_MAX_NODES; ++i)
  {
    if (emulator->nodes[i].active && emulator->nodes[i].node_id == node_id)
    {
      return &emulator->nodes[i];
    }
  }

  return NULL;
}

static const CanEmulatorNode *find_node_const(const CanEmulator *emulator, CanNodeId node_id)
{
  size_t i;

  if (emulator == NULL)
  {
    return NULL;
  }

  for (i = 0; i < CAN_EMULATOR_MAX_NODES; ++i)
  {
    if (emulator->nodes[i].active && emulator->nodes[i].node_id == node_id)
    {
      return &emulator->nodes[i];
    }
  }

  return NULL;
}

static bool enqueue_rx(CanEmulatorNode *node, const CanFrame *frame, CanNodeId sender)
{
  size_t write_index;

  if (node == NULL || frame == NULL)
  {
    return false;
  }

  if (node->rx_count >= CAN_EMULATOR_MAX_RX_QUEUE)
  {
    return false;
  }

  write_index = (node->rx_head + node->rx_count) % CAN_EMULATOR_MAX_RX_QUEUE;
  node->rx_queue[write_index].frame = *frame;
  node->rx_queue[write_index].sender = sender;
  node->rx_count++;
  return true;
}

static size_t find_best_tx_index(const CanEmulator *emulator)
{
  size_t i;
  size_t best_index;

  best_index = 0;
  for (i = 1; i < emulator->pending_tx_count; ++i)
  {
    const CanEmulatorTxEntry *candidate = &emulator->pending_tx[i];
    const CanEmulatorTxEntry *best = &emulator->pending_tx[best_index];

    if (candidate->frame.id < best->frame.id)
    {
      best_index = i;
      continue;
    }

    if (candidate->frame.id == best->frame.id && candidate->sequence < best->sequence)
    {
      best_index = i;
    }
  }

  return best_index;
}

void can_emulator_init(CanEmulator *emulator)
{
  if (emulator == NULL)
  {
    return;
  }

  (void)memset(emulator, 0, sizeof(*emulator));
}

bool can_emulator_register_node(CanEmulator *emulator, CanNodeId node_id)
{
  size_t i;

  if (emulator == NULL)
  {
    return false;
  }

  if (find_node(emulator, node_id) != NULL)
  {
    return true;
  }

  for (i = 0; i < CAN_EMULATOR_MAX_NODES; ++i)
  {
    if (!emulator->nodes[i].active)
    {
      emulator->nodes[i].active = true;
      emulator->nodes[i].node_id = node_id;
      emulator->nodes[i].rx_head = 0U;
      emulator->nodes[i].rx_count = 0U;
      return true;
    }
  }

  return false;
}

bool can_emulator_submit(CanEmulator *emulator, CanNodeId sender, const CanFrame *frame)
{
  CanEmulatorTxEntry *entry;

  if (emulator == NULL || !is_valid_frame(frame))
  {
    return false;
  }

  if (find_node_const(emulator, sender) == NULL)
  {
    return false;
  }

  if (emulator->pending_tx_count >= CAN_EMULATOR_MAX_PENDING_TX)
  {
    return false;
  }

  entry = &emulator->pending_tx[emulator->pending_tx_count];
  entry->frame = *frame;
  entry->sender = sender;
  entry->sequence = emulator->next_sequence;

  emulator->pending_tx_count++;
  emulator->next_sequence++;
  return true;
}

bool can_emulator_step(CanEmulator *emulator)
{
  size_t i;
  size_t best_index;
  CanEmulatorTxEntry selected;

  if (emulator == NULL || emulator->pending_tx_count == 0U)
  {
    return false;
  }

  best_index = find_best_tx_index(emulator);
  selected = emulator->pending_tx[best_index];

  for (i = best_index; (i + 1U) < emulator->pending_tx_count; ++i)
  {
    emulator->pending_tx[i] = emulator->pending_tx[i + 1U];
  }
  emulator->pending_tx_count--;

  for (i = 0; i < CAN_EMULATOR_MAX_NODES; ++i)
  {
    CanEmulatorNode *node = &emulator->nodes[i];

    if (!node->active || node->node_id == selected.sender)
    {
      continue;
    }

    if (!enqueue_rx(node, &selected.frame, selected.sender))
    {
      return false;
    }
  }

  return true;
}

bool can_emulator_receive(CanEmulator *emulator, CanNodeId receiver, CanFrame *out_frame,
                          CanNodeId *out_sender)
{
  CanEmulatorNode *node;
  CanEmulatorRxEntry *entry;

  if (emulator == NULL || out_frame == NULL)
  {
    return false;
  }

  node = find_node(emulator, receiver);
  if (node == NULL || node->rx_count == 0U)
  {
    return false;
  }

  entry = &node->rx_queue[node->rx_head];
  *out_frame = entry->frame;
  if (out_sender != NULL)
  {
    *out_sender = entry->sender;
  }

  node->rx_head = (node->rx_head + 1U) % CAN_EMULATOR_MAX_RX_QUEUE;
  node->rx_count--;
  return true;
}

size_t can_emulator_pending_tx_count(const CanEmulator *emulator)
{
  if (emulator == NULL)
  {
    return 0U;
  }

  return emulator->pending_tx_count;
}
