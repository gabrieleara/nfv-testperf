#ifndef PAYLOAD_UTIL_H
#define PAYLOAD_UTIL_H

#include <endian.h>
#include <rte_memcpy.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t byte_t;

static inline void produce_data(void *payload_v, size_t size) {
    size_t checksum_index = size - 1;
    byte_t *payload = (byte_t *)payload_v;
    byte_t sum = 0;

    for (size_t i = 0; i < checksum_index; ++i) {
        // IDEA: produce random data?
        payload[i] = i;
        sum += payload[i];
    }

    payload[checksum_index] = ~sum + 1;
}

static inline void produce_data_offset(void *payload_v, size_t size,
                                       size_t offset) {
    produce_data((byte_t *)payload_v + offset, size);
}

static inline bool consume_data(void *payload_v, size_t size) {
    byte_t *payload = (byte_t *)payload_v;
    byte_t sum = 0;

    if (size == 0)
        return true;

    for (size_t i = 0; i < size; ++i) {
        sum += payload[i];
    }

    return sum == 0;
}

static inline bool consume_data_offset(void *payload_v, size_t size,
                                       size_t offset) {
    return consume_data((byte_t *)payload_v + offset, size);
}

static inline void put_i64(byte_t *data, uint64_t value) {
    uint64_t value_be = htobe64(value);
    rte_memcpy(data, &value_be, sizeof(uint64_t));
}

static inline uint64_t get_i64(byte_t *data) {
    uint64_t value_be;
    rte_memcpy(&value_be, data, sizeof(uint64_t));

    return be64toh(value_be);
}

static inline void put_i64_offset(void *data, size_t offset, uint64_t value) {
    put_i64((byte_t *)data + offset, value);
}

static inline uint64_t get_i64_offset(void *data, size_t offset) {
    return get_i64((byte_t *)data + offset);
}

#define PUT_OFFSET_V(type, array, offset, value)                               \
    (*((type *)((byte_t *)array + offset)) = (value))

#define PUT_V(type, array, value) PUT_OFFSET_V(type, array, 0, value)

#ifdef __cplusplus
} // extern "C"
#endif

#endif // PAYLOAD_UTIL_H
