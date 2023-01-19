#include "ble_discovered.h"

#include <zephyr/kernel.h>
#include <string.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(ble_discovered, CONFIG_LOG_BLE_DEVICES_LEVEL);

struct bt_device {
	bt_addr_le_t addr;
	char name[BT_NAME_MAX_SIZE];
	uint8_t name_size;
    int8_t rssi;

	struct bt_device* next;
};

/* linked lists for the currently used and unused bt_device instances defined in _bt_devices */
static struct bt_device* _bt_device_discovered = NULL;
static struct bt_device* _bt_device_unused = NULL;

static struct bt_device _bt_devices[BT_MAX_DISCOVERED_DEVICES];

static void _device_find_and_remove(const bt_addr_le_t* device_addr);
static void _device_remove(struct bt_device* device, struct bt_device* prev);
static void _device_insert(struct bt_device* device, struct bt_device* prev);

K_MUTEX_DEFINE(_mutex);


void ble_discovered_init(void) {
    __ASSERT(_bt_device_discovered == NULL && _bt_device_unused == NULL, "initializer must be called before the module is used");

    /* append all elements in buffer to unused linked list */
    k_mutex_lock(&_mutex, K_FOREVER);
    for (int i = 0; i < BT_MAX_DISCOVERED_DEVICES; i++) {
        struct bt_device* tmp = &_bt_devices[i];
        tmp->next = _bt_device_unused;
        _bt_device_unused = tmp;
    }
    k_mutex_unlock(&_mutex);
}

void ble_discovered_add(const bt_addr_le_t* addr, const uint8_t *data, uint8_t data_len, const int8_t rssi) {
    __ASSERT_NO_MSG(addr != NULL);
    __ASSERT_NO_MSG(data != NULL);

    k_mutex_lock(&_mutex, K_FOREVER);

    /* check if exact device is already discovered and remove */
    _device_find_and_remove(addr);

    /* find available destination for new device */
    struct bt_device* destination;
    if (_bt_device_unused != NULL) {
        destination = _bt_device_unused;
        _bt_device_unused = _bt_device_unused->next;
    } 
    
    /* remove last device from discovered */
    else {
        __ASSERT_NO_MSG(_bt_device_discovered != NULL);
        destination = _bt_device_discovered;
        struct bt_device* prev = NULL;
        while (destination->next != NULL) { /* loop to last element */
            prev = destination;
            destination = destination->next;
        }
        /* there should be BT_MAX_DISCOVERED_DEVICES > 2 number of elements, thus prev device should always exist */
        __ASSERT_NO_MSG(prev != NULL);
        prev->next = NULL;
    }

    *destination = (struct bt_device){
        .addr = *addr,
        .name_size = data_len,
        .next = NULL,
        .rssi = rssi,
    };
    strncpy(destination->name, (char*)data, data_len > BT_NAME_MAX_SIZE ? BT_NAME_MAX_SIZE : data_len);

    /* discovered devices sorted by strength */
    struct bt_device* tmp = _bt_device_discovered;
    struct bt_device* prev = NULL;
    while (tmp != NULL && tmp->rssi > destination->rssi) {
        prev = tmp;
        tmp = tmp->next;
    }
    
    _device_insert(destination, prev);

    k_mutex_unlock(&_mutex);
}

void ble_discovered_log(void) {
    k_mutex_lock(&_mutex, K_FOREVER);
    LOG_INF("");
    LOG_INF("--Discovered devices--");
    struct bt_device* tmp = _bt_device_discovered;
    int count = 0;
    while (tmp != NULL) {
        /* short sleep to prevent dropped messages in debug terminal */
        // not ideal because it locks the mutex, forcing the thread which inserts new devices to wait for a substantial amount of time
        k_sleep(K_MSEC(15));

        char addr_str[BT_ADDR_LE_STR_LEN];
	    bt_addr_le_to_str(&tmp->addr, addr_str, sizeof(addr_str));

        LOG_INF("%s %d dBm: %.*s", addr_str, tmp->rssi, tmp->name_size, tmp->name);

        count++;
        tmp = tmp->next;
    }
    LOG_INF("total devices: %d", count);
    LOG_INF("---\n");
    k_mutex_unlock(&_mutex);
}

static void _device_insert(struct bt_device* device, struct bt_device* prev) {
    __ASSERT_NO_MSG(device != NULL);
    __ASSERT_NO_MSG(device->next == NULL);

    /* insert first */
    if (prev == NULL) {
        device->next = _bt_device_discovered;
        _bt_device_discovered = device;
    }

    /* insert last */
    else if (prev->next == NULL) {
        prev->next = device;
    }

    /* insert middle */
    else {
        device->next = prev->next;
        prev->next = device;
    }

}

static void _device_find_and_remove(const bt_addr_le_t* device_addr) {
    __ASSERT_NO_MSG(device_addr != NULL);

    struct bt_device* tmp = _bt_device_discovered;
    struct bt_device* prev = NULL;

    while(tmp != NULL) {
        if (bt_addr_le_cmp(device_addr, &tmp->addr) == 0) {
            _device_remove(tmp, prev);
            break;
        }
        prev = tmp;
        tmp = tmp->next;
    }
}

static void _device_remove(struct bt_device* device, struct bt_device* prev) {
    __ASSERT_NO_MSG(device != NULL);
    __ASSERT_NO_MSG((prev != NULL && prev->next == device) || prev == NULL);

    /* last element */
    if (prev == NULL) {
        __ASSERT(_bt_device_discovered == device, "if there is no previous device, the current must be the first");
        _bt_device_discovered = device->next;
    } else {
        prev->next = device->next;
    }

    device->next = _bt_device_unused;
    _bt_device_unused = device;
}
