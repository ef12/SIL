/**
 * @file io.h
 * @brief IO driver abstraction for SIL modules.
 */

#ifndef IO_H
#define IO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Placeholder init for IO driver.
 *
 * The application manages buffers through io_transport_udp module.
 * This function is kept for API compatibility if needed.
 *
 * @return true on success.
 */
bool io_init(void);

/**
 * @brief Writes a digital value to a pin.
 *
 * @param pin Digital pin index.
 * @param value Value to write.
 * @return true on success, false on invalid pin or if IO is not initialized.
 */
bool io_digital_write(uint16_t pin, bool value);

/**
 * @brief Reads a digital value from a pin.
 *
 * @param pin Digital pin index.
 * @param value Output pointer for read value.
 * @return true on success, false on invalid input or if IO is not initialized.
 */
bool io_digital_read(uint16_t pin, bool *value);

/**
 * @brief Writes an analog value to a pin.
 *
 * @param pin Analog pin index.
 * @param value Value to write.
 * @return true on success, false on invalid pin or if IO is not initialized.
 */
bool io_analog_write(uint16_t pin, uint16_t value);

/**
 * @brief Reads an analog value from a pin.
 *
 * @param pin Analog pin index.
 * @param value Output pointer for read value.
 * @return true on success, false on invalid input or if IO is not initialized.
 */
bool io_analog_read(uint16_t pin, uint16_t *value);

#ifdef __cplusplus
}
#endif

#endif /* IO_H */
