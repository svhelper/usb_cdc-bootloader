//  From https://github.com/mmoskal/uf2-stm32f/blob/master/hf2.c
#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/flash.h>
#include <libopencm3/cm3/cortex.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/usb/usbd.h>
#include <libopencm3/usb/hid.h>
#include <logger.h>
#include "usb_conf.h"
#include "ghostfat.h"
#include "uf2hid.h"
#include "uf2cfg.h"
#include "hf2.h"
#ifdef INTF_HF2

#define CONTROL_CALLBACK_TYPE_STANDARD (USB_REQ_TYPE_STANDARD | USB_REQ_TYPE_INTERFACE)
#define CONTROL_CALLBACK_MASK_STANDARD (USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT)

#define CONTROL_CALLBACK_TYPE_CLASS (USB_REQ_TYPE_CLASS | USB_REQ_TYPE_INTERFACE)
#define CONTROL_CALLBACK_MASK_CLASS (USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT)

#define VALID_FLASH_ADDR(addr, sz) (USER_FLASH_START <= (addr) && (addr) + (sz) <= USER_FLASH_END)
////#define HF2_BUF_SIZE 1024 + 16
#define HF2_BUF_SIZE FLASH_PAGE_SIZE + 64 //// TODO: devices will typically limit it to the native flash page size + 64 bytes
#define HF2_PAGE_SIZE 256  //  MakeCode fails to flash if page size is not the same as file page size: U.assert(b.payloadSize == this.pageSize)
#define usb_assert assert
#define LOG(s) debug_println(s)

typedef struct {
    uint16_t size;
    union {
        uint8_t buf[HF2_BUF_SIZE];
        uint32_t buf32[HF2_BUF_SIZE / 4];
        uint16_t buf16[HF2_BUF_SIZE / 2];
        HF2_Command cmd;
        HF2_Response resp;
    };
} HF2_Buffer;

HF2_Buffer pkt;

const uint8_t *dataToSend;
volatile uint32_t dataToSendLength;
uint8_t dataToSendFlag;
static usbd_device *_usbd_dev;

static void pokeSend() {
    // debug_println("pokeSend"); debug_flush(); ////
    static uint8_t buf[64];
    bool sendIt = false;

    memset(buf, 0, sizeof(buf));

    cm_disable_interrupts();
    if (dataToSendLength) {
        int flag = dataToSendFlag;
        int s = 63;
        if (dataToSendLength <= 63) {
            s = dataToSendLength;
        } else {
            if (flag == HF2_FLAG_CMDPKT_LAST)
                flag = HF2_FLAG_CMDPKT_BODY;
        }

        buf[0] = flag | s;
        memcpy(buf + 1, dataToSend, s);
        dataToSend += s;
        dataToSendLength -= s;
        sendIt = true;
    }
    cm_enable_interrupts();

    if (sendIt) {
        uint16_t len = sizeof(buf);
        // dump_buffer("hf2pkt >>", buf, len); // debug_flush(); ////
        usbd_ep_write_packet(_usbd_dev, HF2_IN, buf, len);
    }
}

static void send_hf2_response(int size) {
    dataToSend = pkt.buf;
    dataToSendFlag = HF2_FLAG_CMDPKT_LAST;
    dataToSendLength = 4 + size;
    dump_buffer("hf2 >>", dataToSend, size); // debug_flush(); ////
    pokeSend();
}

extern const char infoUf2File[];

#define MURMUR3 0

#define checkDataSize(str, add) assert(sz == 8 + sizeof(cmd->str) + (add), "*** ERROR: checkDataSize failed")

static void assert(bool assertion, const char *msg) {
    if (assertion) { return; }
    debug_print("*** ERROR: "); debug_println(msg); debug_flush();
}

#if MURMUR3
static void murmur3_core_2(const void *data, uint32_t len, uint32_t *dst) {
    // compute two hashes with different seeds in parallel, hopefully reducing
    // collisions
    uint32_t h0 = 0x2F9BE6CC;
    uint32_t h1 = 0x1EC3A6C8;
    const uint32_t *data32 = (const uint32_t *)data;
    while (len--) {
        uint32_t k = *data32++;
        k *= 0xcc9e2d51;
        k = (k << 15) | (k >> 17);
        k *= 0x1b873593;

        h0 ^= k;
        h1 ^= k;
        h0 = (h0 << 13) | (h0 >> 19);
        h1 = (h1 << 13) | (h1 >> 19);
        h0 = (h0 * 5) + 0xe6546b64;
        h1 = (h1 * 5) + 0xe6546b64;
    }

    dst[0] = h0;
    dst[1] = h1;
}
#endif

static void handle_command() {
    int tmp;

    // one has to be careful dealing with these, as they share memory
    HF2_Command *cmd = &pkt.cmd;
    HF2_Response *resp = &pkt.resp;

    uint32_t cmdId = cmd->command_id;
    int sz = pkt.size;
    resp->tag = cmd->tag;
    resp->status16 = HF2_STATUS_OK;

#ifdef TODO
    if (timer[TIMER_BL_WAIT] < 10000)
        timer[TIMER_BL_WAIT] = 10000;
#endif  //  TODO

    switch (cmdId) {
    case HF2_CMD_INFO:
        debug_println("hf2 info"); // debug_flush(); ////
        tmp = strlen(infoUf2File);
        memcpy(pkt.resp.data8, infoUf2File, tmp);
        send_hf2_response(tmp);
        return;

    case HF2_CMD_BININFO:
        debug_println("hf2 bininfo"); // debug_flush(); ////
        resp->bininfo.mode = HF2_MODE_BOOTLOADER;
        resp->bininfo.flash_page_size = HF2_PAGE_SIZE;  //  Previously 128 * 1024
        resp->bininfo.flash_num_pages = FLASH_SIZE_OVERRIDE / HF2_PAGE_SIZE;
        resp->bininfo.max_message_size = sizeof(pkt.buf);
        resp->bininfo.uf2_family = UF2_FAMILY;
        send_hf2_response(sizeof(resp->bininfo));
        return;

    case HF2_CMD_RESET_INTO_APP:
        //  TODO: Flush flash
        debug_println("hf2 rst app"); // debug_flush(); ////
#ifdef TODO
        resetIntoApp();
#endif  //  TODO
        break;
    case HF2_CMD_RESET_INTO_BOOTLOADER:
        //  TODO: Flush flash
        debug_println("hf2 rst boot"); // debug_flush(); ////
#ifdef TODO
        resetIntoBootloader();
#endif  //  TODO
        break;
    case HF2_CMD_START_FLASH:
        // userspace app should reboot into bootloader on this command; we just ignore it
        // userspace can also call hf2_handover() here
        debug_println("hf2 start"); // debug_flush(); ////
        break;
    case HF2_CMD_WRITE_FLASH_PAGE:
        // first send ACK and then start writing, while getting the next packet
        debug_println("hf2 write"); // debug_flush(); ////
        checkDataSize(write_flash_page, 256);
        send_hf2_response(0);
        if (VALID_FLASH_ADDR(cmd->write_flash_page.target_addr, 256)) {
            flash_write(cmd->write_flash_page.target_addr,
                        (const uint8_t *)cmd->write_flash_page.data, 256);
        }
        return;
    case HF2_CMD_READ_WORDS:
        debug_println("hf2 read"); // debug_flush(); ////
        checkDataSize(read_words, 0);
        tmp = cmd->read_words.num_words;
        memcpy(resp->data32, (void *)cmd->read_words.target_addr, tmp << 2);
        send_hf2_response(tmp << 2);
        return;
#if MURMUR3
    case HF2_CMD_MURMUR3:
        debug_println("hf2 murmur"); // debug_flush(); ////
        checkDataSize(murmur3, 0);
        murmur3_core_2((void *)cmd->murmur3.target_addr, cmd->murmur3.num_words, resp->data32);
        send_hf2_response(8);
        return;
#endif
    default:
        // command not understood
        debug_print("hf2 unknown cmd "); debug_print_unsigned(cmdId); debug_println(""); // debug_flush(); ////
        resp->status16 = HF2_STATUS_INVALID_CMD;
        break;
    }

    send_hf2_response(0);
}

static const char bad_packet_message[] = "bad packet";
static uint8_t buf[64];

static void hf2_data_rx_cb(usbd_device *usbd_dev, uint8_t ep) {
    //  debug_print("hf2 << ep "); debug_printhex(ep); debug_println(""); // debug_flush(); ////
    int len;
    len = usbd_ep_read_packet(usbd_dev, ep, buf, sizeof(buf));    
    debug_print("hf2 << tag "); debug_printhex(buf[0]); debug_println("");  // DMESG("HF2 read: %d", len);
    // dump_buffer(",", buf, len); // debug_flush(); ////    
    if (len <= 0) return;

    uint8_t tag = buf[0];
    // serial packets not allowed when in middle of command packet
    usb_assert(pkt.size == 0 || !(tag & HF2_FLAG_SERIAL_OUT), bad_packet_message);
    int size = tag & HF2_SIZE_MASK;
    usb_assert(pkt.size + size <= (int)sizeof(pkt.buf), bad_packet_message);
    memcpy(pkt.buf + pkt.size, buf + 1, size);
    pkt.size += size;
    tag &= HF2_FLAG_MASK;
    if (tag != HF2_FLAG_CMDPKT_BODY) {
        if (tag == HF2_FLAG_CMDPKT_LAST) {
            handle_command();
        } else {
            // do something about serial?
        }
        pkt.size = 0;
    }
}

static void hf2_data_tx_cb(usbd_device *usbd_dev, uint8_t ep) {
    (void)usbd_dev;
    (void)ep;
    pokeSend();
}

/** @brief Setup the endpoints to be bulk & register the callbacks. */
static void hf2_set_config(usbd_device *usbd_dev, uint16_t wValue) {
    LOG("HF2 config");
    (void)wValue;
    usbd_ep_setup(usbd_dev, HF2_IN, USB_ENDPOINT_ATTR_BULK, MAX_USB_PACKET_SIZE, hf2_data_tx_cb);
    usbd_ep_setup(usbd_dev, HF2_OUT, USB_ENDPOINT_ATTR_BULK, MAX_USB_PACKET_SIZE, hf2_data_rx_cb);
}

void hf2_setup(usbd_device *usbd_dev) {
    _usbd_dev = usbd_dev;
    int status = aggregate_register_config_callback(usbd_dev, hf2_set_config);
    if (status < 0) { debug_println("*** hf2_setup failed"); debug_flush(); }
}
#endif  //  INTF_HF2

#ifdef NOTUSED

hf2 << len 64, tag 48, 64 / 48 01 00 00 00 5c a4 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00

48 - len 8
01 00 00 00 - cmd
5c a4 - tag
00 00 

hf2 bininfo
hf2 >> 64 / 58 5c a4 00 00 01 00 00 00 00 04 00 00 00 01 00 00 10 04 00 00 72 10 e2 5e 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00

58 - len 24
5c a4 - tag
00 - status
00 - statusinfo
01 00 00 00 - mode: bootloader
00 04 00 00 - flash_page_size: 1024 bytes
00 01 00 00 - flash_num_pages: 256
40 04 00 00 - max_message_size: 1024 + 64 = 1088
72 10 e2 5e - UF2_FAMILY 0x5ee21072

struct HF2_BININFO_Result {
    uint32_t mode;
    uint32_t flash_page_size;
    uint32_t flash_num_pages;
    uint32_t max_message_size;
    uint32_t uf2_family;
};

#endif  //  NOTUSED
