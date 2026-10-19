#include "usb_telemetry.h"
#include "bsp_usb.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "runtime_tuning.h"
extern uint8_t UserRxBufferFS[];
static void receive(const char *text)
{
    uint32_t length = (uint32_t)strlen(text);
    memcpy(UserRxBufferFS, text, length);
    assert(USBD_Interface_fops_FS.Receive(UserRxBufferFS, &length) == USBD_OK);
}

USBD_HandleTypeDef hUsbDeviceFS;
static USBD_CDC_HandleTypeDef cdc;
static uint8_t next_tx_status;
static unsigned submitted;

uint8_t USBD_CDC_SetTxBuffer(USBD_HandleTypeDef *dev, uint8_t *buf, uint32_t len)
{
    USBD_CDC_HandleTypeDef *state = dev->pClassData;
    state->TxBuffer = buf; state->TxLength = len; return USBD_OK;
}
uint8_t USBD_CDC_SetRxBuffer(USBD_HandleTypeDef *dev, uint8_t *buf)
{ (void)dev; (void)buf; return USBD_OK; }
uint8_t USBD_CDC_ReceivePacket(USBD_HandleTypeDef *dev)
{ (void)dev; return USBD_OK; }
uint8_t USBD_CDC_TransmitPacket(USBD_HandleTypeDef *dev)
{
    if (next_tx_status != USBD_OK) return next_tx_status;
    ((USBD_CDC_HandleTypeDef *)dev->pClassData)->TxState = 1;
    ++submitted; return USBD_OK;
}
static void dtr(uint16_t value)
{
    USBD_SetupReqTypedef req = { .wValue = value };
    assert(USBD_Interface_fops_FS.Control(CDC_SET_CONTROL_LINE_STATE,
                                         (uint8_t *)&req, 0) == USBD_OK);
}
static void complete(void)
{
    uint32_t length = cdc.TxLength;
    cdc.TxState = 0;
    assert(USBD_Interface_fops_FS.TransmitCplt(cdc.TxBuffer, &length, 1) == USBD_OK);
}

int main(void)
{
    uint8_t bytes[4] = {1,2,3,4};
    hUsbDeviceFS.pClassData = &cdc;
    assert(USBTransmit(bytes, sizeof bytes) == USBD_FAIL);
    hUsbDeviceFS.pClassData = NULL;
    /* Must return FAIL, never dereference NULL before USB enumeration. */
    assert(USBTransmit(bytes, sizeof bytes) == USBD_FAIL);
    hUsbDeviceFS.pClassData = &cdc;
    assert(USBTransmit(bytes, sizeof bytes) == USBD_FAIL);
    hUsbDeviceFS.dev_state = USBD_STATE_CONFIGURED;
    assert(USBD_Interface_fops_FS.Init() == USBD_OK);
    uint8_t coding[7] = {0};
    assert(USBD_Interface_fops_FS.Control(CDC_GET_LINE_CODING, coding, 7) == USBD_OK);
    assert(memcmp(coding, (uint8_t[]){0,194,1,0,0,0,8}, 7) == 0);
    uint8_t changed[7] = {0,225,0,0,2,2,7};
    assert(USBD_Interface_fops_FS.Control(CDC_SET_LINE_CODING, changed, 7) == USBD_OK);
    assert(USBD_Interface_fops_FS.Control(CDC_GET_LINE_CODING, coding, 7) == USBD_OK);
    assert(memcmp(coding, changed, 7) == 0);
    assert(USBD_Interface_fops_FS.Control(CDC_SET_LINE_CODING, changed, 6) == USBD_FAIL);
    changed[6] = 0;
    assert(USBD_Interface_fops_FS.Control(CDC_SET_LINE_CODING, changed, 7) == USBD_FAIL);
    USBTelemetryStreamInit();
    USBTelemetrySample_s sample = {0};
    USBTelemetryStreamSend(&sample, 0);
    assert(submitted == 0); /* CDC is configured but host has not opened it. */
    dtr(1);
    USBTelemetryStreamSend(&sample, 20);
    assert(submitted == 1 && memcmp(cdc.TxBuffer, "version,seq,tick,", 17) == 0);
    complete();
    sample.rc_lx = -123;
    USBTelemetryStreamSend(&sample, 40);
    assert(submitted == 2 && sample.seq == 2 && sample.version == 1);
    uint8_t saved[2048];
    uint8_t *in_flight = cdc.TxBuffer;
    uint32_t length = cdc.TxLength;
    memcpy(saved, in_flight, length);
    sample.rc_lx = 456;
    USBTelemetryStreamSend(&sample, 60);
    assert(submitted == 2 && memcmp(in_flight, saved, length) == 0);
    assert(USBTransmit(bytes, sizeof bytes) == USBD_BUSY);
    dtr(0); dtr(1);
    USBTelemetryStreamSend(&sample, 80);
    assert(memcmp(in_flight, saved, length) == 0); /* Reopen cannot overwrite A. */
    complete();
    USBTelemetryStreamSend(&sample, 100);
    assert(memcmp(cdc.TxBuffer, "version,seq,tick,", 17) == 0);
    complete();
    USBTelemetryStreamSend(&sample, 120);
    assert(sample.tx_drop_count == 2);
    complete();
    USBTelemetryStreamSend(&sample, 1100);
    assert(memcmp(cdc.TxBuffer, "version,seq,tick,", 17) == 0); /* Repeated header. */
    complete();
    next_tx_status = USBD_FAIL;
    assert(USBTransmit(bytes, sizeof bytes) == USBD_FAIL);
    next_tx_status = USBD_BUSY;
    assert(USBTransmit(bytes, sizeof bytes) == USBD_BUSY);
    next_tx_status = USBD_OK;
    assert(USBD_Interface_fops_FS.DeInit() == USBD_OK);
    hUsbDeviceFS.pClassData = NULL;
    USBTelemetryStreamSend(&sample, 1120);
    memset(&cdc, 0, sizeof cdc); /* The USB class has closed its endpoints. */
    hUsbDeviceFS.pClassData = &cdc;
    assert(USBD_Interface_fops_FS.Init() == USBD_OK);
    USBTelemetryStreamSend(&sample, 1140);
    const unsigned before_open = submitted;
    dtr(1);
    USBTelemetryStreamSend(&sample, 1160);
    assert(submitted == before_open + 1);
    assert(memcmp(cdc.TxBuffer, "version,seq,tick,", 17) == 0);
    complete();
    float tuning_pid[24] = {.75f};
    RuntimeTuningRegister(TUNING_PID, tuning_pid, 0);
    receive("@T1,17,GE");
    USBTelemetryStreamSend(&sample, 1180);
    assert(memcmp(cdc.TxBuffer, "@T1,", 4) != 0);
    complete();
    receive("T,pid\r");
    receive("\n");
    USBTelemetryStreamSend(&sample, 1200);
    assert(memcmp(cdc.TxBuffer, "@T1,17,OK,pid,1,0,3f400000", 24) == 0);
    uint8_t tuning_in_flight[512];
    memcpy(tuning_in_flight, cdc.TxBuffer, cdc.TxLength);
    uint32_t tuning_length = cdc.TxLength;
    receive("@T1,18,GET,pid\r\n");
    USBTelemetryStreamSend(&sample, 1220);
    assert(memcmp(tuning_in_flight, cdc.TxBuffer, tuning_length) == 0);
    complete();
    USBTelemetryStreamSend(&sample, 1240);
    assert(memcmp(cdc.TxBuffer, "@T1,18,OK,pid", 12) == 0);
    complete();
    /* Oversized and NUL-containing lines cannot turn into valid suffix commands. */
    char large[600]; memset(large, 'x', sizeof large - 1); large[sizeof large - 1] = 0;
    receive(large); receive("@T1,19,GET,pid\n");
    USBTelemetryStreamSend(&sample, 1260);
    assert(memcmp(cdc.TxBuffer, "@T1,", 4) != 0);
    complete();
    receive("@T1,20,GE");
    dtr(0); dtr(1);
    receive("@T1,21,GET,pid\n"); /* New-session bytes before task sees DTR. */
    USBTelemetryStreamSend(&sample, 1280);
    complete(); /* Session header first. */
    USBTelemetryStreamSend(&sample, 1300);
    assert(memcmp(cdc.TxBuffer, "@T1,21,OK,pid", 12) == 0);
    complete();
    puts("USB telemetry transport tests passed");
    return 0;
}
