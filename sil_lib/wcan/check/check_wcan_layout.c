/*
 * Layout probe. The shared segment is the real ABI between processes, so its
 * size, field offsets and alignment must be identical for every consumer —
 * 32-bit MinGW for the simulation, 64-bit MSVC for Python and the VT.
 *
 * Compile with each toolchain and diff the output.
 */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "wcan_layout.h"

#define SHOW_SIZE(type) \
    printf("  sizeof(%-24s) = %6u\n", #type, (unsigned)sizeof(type))

#define SHOW_OFFSET(type, field)                                   \
    printf("  offsetof(%-18s, %-16s) = %6u\n", #type, #field, \
           (unsigned)offsetof(type, field))

int main(void)
{
    printf("pointer width: %u bits\n", (unsigned)(sizeof(void *) * 8u));
#if defined(_MSC_VER)
    printf("compiler: MSVC %d\n", (int)_MSC_VER);
#elif defined(__GNUC__)
    printf("compiler: GCC %d.%d\n", __GNUC__, __GNUC_MINOR__);
#endif
    printf("\nsizes\n");
    SHOW_SIZE(wcan_shm_slot_t);
    SHOW_SIZE(wcan_shm_pending_t);
    SHOW_SIZE(wcan_shm_node_t);
    SHOW_SIZE(wcan_shm_bus_t);

    printf("\nslot offsets\n");
    SHOW_OFFSET(wcan_shm_slot_t, can_id);
    SHOW_OFFSET(wcan_shm_slot_t, dlc);
    SHOW_OFFSET(wcan_shm_slot_t, data);
    SHOW_OFFSET(wcan_shm_slot_t, bus_time);
    SHOW_OFFSET(wcan_shm_slot_t, sequence);

    printf("\npending offsets\n");
    SHOW_OFFSET(wcan_shm_pending_t, slot);
    SHOW_OFFSET(wcan_shm_pending_t, arrival);
    SHOW_OFFSET(wcan_shm_pending_t, sender);
    SHOW_OFFSET(wcan_shm_pending_t, bits);

    printf("\nnode offsets\n");
    SHOW_OFFSET(wcan_shm_node_t, active);
    SHOW_OFFSET(wcan_shm_node_t, overrun);
    SHOW_OFFSET(wcan_shm_node_t, ring);

    printf("\nbus offsets\n");
    SHOW_OFFSET(wcan_shm_bus_t, magic);
    SHOW_OFFSET(wcan_shm_bus_t, qpc_frequency);
    SHOW_OFFSET(wcan_shm_bus_t, bus_free_at);
    SHOW_OFFSET(wcan_shm_bus_t, pending_count);
    SHOW_OFFSET(wcan_shm_bus_t, pending);
    SHOW_OFFSET(wcan_shm_bus_t, bus_name);
    SHOW_OFFSET(wcan_shm_bus_t, nodes);
    return 0;
}
