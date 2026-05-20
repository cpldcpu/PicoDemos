/* Windows endian shim. Windows is little-endian on the platforms we care
 * about (x86, x86_64, aarch64), so host->LE is identity and host->BE is bswap.
 */
#ifndef WINDOWS_ENDIAN_H
#define WINDOWS_ENDIAN_H

#include <stdint.h>
#include <stdlib.h>

#define htobe16(x) _byteswap_ushort((uint16_t)(x))
#define htole16(x) ((uint16_t)(x))
#define be16toh(x) _byteswap_ushort((uint16_t)(x))
#define le16toh(x) ((uint16_t)(x))

#define htobe32(x) _byteswap_ulong((uint32_t)(x))
#define htole32(x) ((uint32_t)(x))
#define be32toh(x) _byteswap_ulong((uint32_t)(x))
#define le32toh(x) ((uint32_t)(x))

#define htobe64(x) _byteswap_uint64((uint64_t)(x))
#define htole64(x) ((uint64_t)(x))
#define be64toh(x) _byteswap_uint64((uint64_t)(x))
#define le64toh(x) ((uint64_t)(x))

#endif
