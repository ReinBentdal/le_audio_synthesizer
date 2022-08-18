/**
 * @file ble_discovered.h
 * @author Nordic Semiconductor ASA (Rein Gundersen Bentdal)
 * @brief Stores all discovered devices in a list
 * @version 0.1
 * @date 2022-08-01
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef _BLE_DISCOVERED_H
#define _BLE_DISCOVERED_H

#include <zephyr/bluetooth/gatt.h>
#include <zephyr/kernel.h>

#define BT_MAX_DISCOVERED_DEVICES 20
#define BT_NAME_MAX_SIZE 15

BUILD_ASSERT(BT_MAX_DISCOVERED_DEVICES > 2);

void ble_discovered_init(void);

void ble_discovered_add(const bt_addr_le_t* addr, const uint8_t *data, uint8_t data_len, const int8_t rssi);

/**
 * @brief prints discovered devices, sorted by strength
 */
void ble_discovered_log(void);

#endif