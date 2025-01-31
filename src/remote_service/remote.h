
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>

/** @brief UUID of the Remote Service. **/
#define BT_UUID_REMOTE_SERV_VAL \
	BT_UUID_128_ENCODE(0xe9ea0001, 0xe19b, 0x482d, 0x9293, 0xc7907585fc48)

/** @brief UUID of the Message Characteristic. **/
#define BT_UUID_REMOTE_MESSAGE_CHRC_VAL \
	BT_UUID_128_ENCODE(0xe9ea0003, 0xe19b, 0x482d, 0x9293, 0xc7907585fc48)

/** @brief UUID of the Radar Characteristic. **/
#define BT_UUID_REMOTE_RADAR_SERV_VAL \
	BT_UUID_128_ENCODE(0xe9ea0011, 0xe19b, 0x482d, 0x9293, 0xc7907585fc48)

/** @brief UUID of the Radar Characteristic. **/
#define BT_UUID_REMOTE_RADAR_CHRC_VAL \
	BT_UUID_128_ENCODE(0xe9ea00012, 0xe19b, 0x482d, 0x9293, 0xc7907585fc48)

#define BT_UUID_REMOTE_SERVICE          BT_UUID_DECLARE_128(BT_UUID_REMOTE_SERV_VAL)
#define BT_UUID_REMOTE_MESSAGE_CHRC 	BT_UUID_DECLARE_128(BT_UUID_REMOTE_MESSAGE_CHRC_VAL)

#define BT_UUID_DATA_SERVICE			BT_UUID_DECLARE_128(BT_UUID_REMOTE_RADAR_SERV_VAL)
#define BT_UUID_REMOTE_RADAR_CHRC		BT_UUID_DECLARE_128(BT_UUID_REMOTE_RADAR_CHRC_VAL)

struct bt_remote_service_cb {
    void (*data_received)(struct bt_conn *conn, const uint8_t *const data, uint16_t len);
};

int bluetooth_init(struct bt_conn_cb *bt_cb, struct bt_remote_service_cb *remote_cb);
