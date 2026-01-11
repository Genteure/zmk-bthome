/*
 * BTHome v2 AES-CCM encryption helper
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>
#include <zephyr/crypto/crypto.h>
#include <zephyr/logging/log.h>
#include <string.h>

#include <zmk_bthome/zmk_bthome.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

BUILD_ASSERT(sizeof(CONFIG_ZMK_BTHOME_ENCRYPTION_KEY) - 1 == 32,
             "CONFIG_ZMK_BTHOME_ENCRYPTION_KEY must be 32 hex characters (16 bytes)");

// TODO use only mbedtls shim?
#ifdef CONFIG_CRYPTO_MBEDTLS_SHIM
#define CRYPTO_DRV_NAME CONFIG_CRYPTO_MBEDTLS_SHIM_DRV_NAME
#elif CONFIG_CRYPTO_NRF_ECB
#define CRYPTO_DEV_COMPAT nordic_nrf_ecb
#elif CONFIG_CRYPTO_ESP32_AES
#define CRYPTO_DEV_COMPAT espressif_esp32_aes
#else
#error "You need to enable one crypto device"
#endif

static inline const struct device *get_crypto_dev(void)
{
#ifdef CRYPTO_DRV_NAME
    const struct device *dev = device_get_binding(CRYPTO_DRV_NAME);
#else
    const struct device *dev = DEVICE_DT_GET_ONE(CRYPTO_DEV_COMPAT);
#endif
    return dev;
}

static const struct device *crypto_dev = NULL;

static uint8_t bthome_key[16];
static uint8_t nonce[13] = {
    0,
    0,
    0,
    0,
    0,
    0, // BLE address (6 bytes, reversed order)
    ZMK_BTHOME_SERVICE_UUID_1,
    ZMK_BTHOME_SERVICE_UUID_2,
    ZMK_BTHOME_DEVICE_INFO, // UUID (2 bytes) + device info (1 byte)
    0,
    0,
    0,
    0 // Replay counter (4 bytes, little-endian)
};

void zmk_bthome_encrypt_init(const uint8_t ble_addr[6])
{
    /* Resolve crypto device at runtime to avoid non-constant static init */
    if (crypto_dev == NULL)
    {
        crypto_dev = get_crypto_dev();
    }
    if (!crypto_dev)
    {
        LOG_WRN("No crypto device available for BTHome encryption");
    }
    else if (!device_is_ready(crypto_dev))
    {
        LOG_WRN("Crypto device not ready: %s", crypto_dev->name);
    }

    /* Nonce prefix: BLE address in reversed order per spec/example */
    nonce[0] = ble_addr[5];
    nonce[1] = ble_addr[4];
    nonce[2] = ble_addr[3];
    nonce[3] = ble_addr[2];
    nonce[4] = ble_addr[1];
    nonce[5] = ble_addr[0];

    {
        size_t hex_len = sizeof(CONFIG_ZMK_BTHOME_ENCRYPTION_KEY) - 1;
        size_t key_len = hex2bin(CONFIG_ZMK_BTHOME_ENCRYPTION_KEY, hex_len, bthome_key, sizeof(bthome_key));
        if (key_len != sizeof(bthome_key))
        {
            LOG_ERR("Invalid CONFIG_ZMK_BTHOME_ENCRYPTION_KEY");
            memset(bthome_key, 0, sizeof(bthome_key));
        }
        else
        {
            LOG_DBG("BTHome encryption key loaded");
        }
    }
}

int zmk_bthome_encrypt_payload(const uint8_t *plaintext, const size_t plaintext_len,
                               const uint32_t replay_counter, uint8_t *enc_out,
                               uint8_t mic_out[BTHOME_ENCRYPT_TAG_LEN])
{
    if (!crypto_dev)
    {
        LOG_ERR("No crypto device available for BTHome encryption");
        return -ENODEV;
    }

    if (!device_is_ready(crypto_dev))
    {
        LOG_ERR("Crypto device not ready: %s", crypto_dev->name);
        return -ENODEV;
    }

    /* copy counter into nonce (little endian) */
    memcpy(&nonce[9], &replay_counter, 4);

    struct cipher_ctx context = {
        .keylen = sizeof(bthome_key),
        .key.bit_stream = bthome_key,
        .mode_params.ccm_info = {
            .nonce_len = sizeof(nonce),
            .tag_len = BTHOME_ENCRYPT_TAG_LEN,
        },
        .flags = CAP_RAW_KEY | CAP_SYNC_OPS | CAP_SEPARATE_IO_BUFS,
    };

    struct cipher_pkt encrypt_pkt = {
        .in_buf = (uint8_t *)plaintext,
        .in_len = plaintext_len,
        .out_buf_max = plaintext_len,
        .out_buf = enc_out,
    };

    struct cipher_aead_pkt ccm_op = {
        .ad = NULL,
        .ad_len = 0,
        .pkt = &encrypt_pkt,
        .tag = mic_out,
    };

    int err = cipher_begin_session(crypto_dev, &context, CRYPTO_CIPHER_ALGO_AES,
                                   CRYPTO_CIPHER_MODE_CCM, CRYPTO_CIPHER_OP_ENCRYPT);
    if (err)
    {
        LOG_ERR("cipher_begin_session returned %d", err);
        return err;
    }

    err = cipher_ccm_op(&context, &ccm_op, nonce);
    if (err)
    {
        LOG_ERR("Encrypt failed: %d", err);
        cipher_free_session(crypto_dev, &context);
        return err;
    }

    /* Cipher wrote ciphertext into `enc_out` and tag into `mic_out`. */

    cipher_free_session(crypto_dev, &context);
    return 0;
}
