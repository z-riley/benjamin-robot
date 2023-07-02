#include <stdint.h>
#include "remote.h"

#define LOG_MODULE_NAME remote
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

static K_SEM_DEFINE(bt_init_ok, 0, 1);

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME)-1)

static struct bt_remote_service_cb remote_service_callbacks;

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN)
};

static const struct bt_data sd[] = {
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_REMOTE_SERV_VAL),
};

/* Declarations */
static ssize_t on_write(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len, uint16_t offset, uint8_t flags);

// Robot control service
BT_GATT_SERVICE_DEFINE(remote_srv,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_REMOTE_SERVICE),
    BT_GATT_CHARACTERISTIC(BT_UUID_REMOTE_MESSAGE_CHRC,
    BT_GATT_CHRC_WRITE_WITHOUT_RESP,
    BT_GATT_PERM_WRITE,
    NULL, on_write, NULL),
);

// Radar data service
// BT_GATT_SERVICE_DEFINE(remote_srv,
//     BT_GATT_PRIMARY_SERVICE(BT_UUID_DATA_SERVICE),
//     BT_GATT_CHARACTERISTIC(BT_UUID_REMOTE_RADAR_CHRC,
//     BT_GATT_CHRC_READ,
//     BT_GATT_PERM_READ,
//     NULL, on_read, NULL),
// );

/* Callbacks */
void bt_ready(int err)
{
    if (err) {
        LOG_ERR("bt_ready returned %d", err);
    }
    k_sem_give(&bt_init_ok);
} /* bt_ready */

static ssize_t on_write(struct bt_conn *conn,
                        const struct bt_gatt_attr *attr,
                        const void *buf,
                        uint16_t len,
                        uint16_t offset,
                        uint8_t flags)
{
    LOG_DBG("Received data, handle %d, conn %p",
        attr->handle, (void *)conn);

    if (remote_service_callbacks.data_received) {
        remote_service_callbacks.data_received(conn, buf, len);
    }
    return len;
} /* on_write */


int bluetooth_init(struct bt_conn_cb *bt_cb, struct bt_remote_service_cb *remote_cb)
{
    int err;
    LOG_INF("Initializing Bluetooth");

    if (bt_cb == NULL){
        return NRFX_ERROR_NULL;
    }
    bt_conn_cb_register(bt_cb);
    remote_service_callbacks.data_received = remote_cb->data_received;

    err = bt_enable(bt_ready);
    if (err) {
        LOG_ERR("bt_enable returned %d", err);
        return err;
    }

    k_sem_take(&bt_init_ok, K_FOREVER);

    err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
    if (err){
        LOG_ERR("Couldn't start advertising (err = %d", err);
        return err;
    }

    return err;
} /* bluetooth_init */
