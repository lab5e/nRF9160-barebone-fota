#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define FIRMWARE_VER_ID 1
#define MODEL_NUMBER_ID 2
#define SERIAL_NUMBER_ID 3
#define MANUFACTURER_ID 4

#define HOST_ID 1
#define PORT_ID 2
#define PATH_ID 3
#define AVAILABLE_ID 4

size_t encode_tlv_string(uint8_t *buf, uint8_t id, const uint8_t *str);

/**
 * tlv_id returns the next ID in the buffer
 */
uint8_t tlv_id(const uint8_t *buf, size_t idx);

/**
 * decode_tlv_string decode_tlv_string decodes a tlv encoded string in buffer and
 * returns the result in str.
 */
int decode_tlv_string(const uint8_t *buf, size_t *idx, char *str);

/**
 * decode_tlv_uint32 decodes a tlv encoded uint32 in buffer and returns the value in val
 */
int decode_tlv_uint32(const uint8_t *buf, size_t *idx, uint32_t *val);

/**
 * decode_tlv_bool decodes a tlv encoded bool in buffer and returns the value in val
 */
int decode_tlv_bool(const uint8_t *buf, size_t *idx, bool *val);