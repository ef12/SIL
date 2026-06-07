/**
 * @file can_emulator.c
 * @brief Virtual CAN bus emulator — arbitration, routing, and node management.
 */

#include "can_emulator.h"

#include <stdlib.h>
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

  for (i = 0; i < emulator->max_nodes; ++i)
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

  for (i = 0; i < emulator->max_nodes; ++i)
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

  if (node->rx_count >= node->rx_capacity)
  {
    return false;
  }

  write_index = (node->rx_head + node->rx_count) % node->rx_capacity;
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

static void free_node_queues(CanEmulator *emulator)
{
  size_t i;

  if (emulator->nodes == NULL)
  {
    return;
  }

  for (i = 0; i < emulator->max_nodes; ++i)
  {
    free(emulator->nodes[i].rx_queue);
    emulator->nodes[i].rx_queue = NULL;
  }
}

bool can_emulator_init(CanEmulator *emulator, const CanEmulatorConfig *config)
{
  size_t i;

  if (emulator == NULL || config == NULL)
  {
    return false;
  }

  if (config->max_nodes == 0U || config->max_pending_tx == 0U || config->max_rx_queue == 0U)
  {
    return false;
  }

  (void)memset(emulator, 0, sizeof(*emulator));

  emulator->nodes = (CanEmulatorNode *)calloc(config->max_nodes, sizeof(CanEmulatorNode));
  if (emulator->nodes == NULL)
  {
    return false;
  }

  for (i = 0; i < config->max_nodes; ++i)
  {
    emulator->nodes[i].rx_queue =
        (CanEmulatorRxEntry *)calloc(config->max_rx_queue, sizeof(CanEmulatorRxEntry));
    if (emulator->nodes[i].rx_queue == NULL)
    {
      free_node_queues(emulator);
      free(emulator->nodes);
      emulator->nodes = NULL;
      return false;
    }
    emulator->nodes[i].rx_capacity = config->max_rx_queue;
  }

  emulator->pending_tx =
      (CanEmulatorTxEntry *)calloc(config->max_pending_tx, sizeof(CanEmulatorTxEntry));
  if (emulator->pending_tx == NULL)
  {
    free_node_queues(emulator);
    free(emulator->nodes);
    emulator->nodes = NULL;
    return false;
  }

  emulator->max_nodes = config->max_nodes;
  emulator->max_pending_tx = config->max_pending_tx;
  emulator->max_rx_queue = config->max_rx_queue;
  emulator->initialized = true;
  return true;
}

void can_emulator_deinit(CanEmulator *emulator)
{
  if (emulator == NULL || !emulator->initialized)
  {
    return;
  }

  free_node_queues(emulator);
  free(emulator->nodes);
  free(emulator->pending_tx);
  (void)memset(emulator, 0, sizeof(*emulator));
}

bool can_emulator_register_node(CanEmulator *emulator, CanNodeId node_id)
{
  size_t i;

  if (emulator == NULL || !emulator->initialized)
  {
    return false;
  }

  if (find_node(emulator, node_id) != NULL)
  {
    return true;
  }

  for (i = 0; i < emulator->max_nodes; ++i)
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

  if (emulator == NULL || !emulator->initialized || !is_valid_frame(frame))
  {
    return false;
  }

  if (find_node_const(emulator, sender) == NULL)
  {
    return false;
  }

  if (emulator->pending_tx_count >= emulator->max_pending_tx)
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

  if (emulator == NULL || !emulator->initialized || emulator->pending_tx_count == 0U)
  {
    return false;
  }

  best_index = find_best_tx_index(emulator);
  selected = emulator->pending_tx[best_index];

  /* Route to all other nodes' RX queues BEFORE removing from TX. */
  for (i = 0; i < emulator->max_nodes; ++i)
  {
    CanEmulatorNode *node = &emulator->nodes[i];

    if (!node->active || node->node_id == selected.sender)
    {
      continue;
    }

    if (!enqueue_rx(node, &selected.frame, selected.sender))
    {
      /* RX queue full — frame stays in TX queue for retry. */
      return false;
    }
  }

  /* All recipients accepted — now remove from TX queue. */
  for (i = best_index; (i + 1U) < emulator->pending_tx_count; ++i)
  {
    emulator->pending_tx[i] = emulator->pending_tx[i + 1U];
  }
  emulator->pending_tx_count--;

  return true;
}

bool can_emulator_receive(CanEmulator *emulator, CanNodeId receiver, CanFrame *out_frame,
                          CanNodeId *out_sender)
{
  CanEmulatorNode *node;
  CanEmulatorRxEntry *entry;

  if (emulator == NULL || !emulator->initialized || out_frame == NULL)
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

  node->rx_head = (node->rx_head + 1U) % node->rx_capacity;
  node->rx_count--;
  return true;
}

size_t can_emulator_pending_tx_count(const CanEmulator *emulator)
{
  if (emulator == NULL || !emulator->initialized)
  {
    return 0U;
  }

  return emulator->pending_tx_count;
}
