/**
 * @file esp_wifi_config_uri.h
 * @brief Percent-decoding for URI path segments.
 *
 * Header-only and free of any ESP-IDF dependency so that a host build can
 * include it directly (see test/uri_decode). It is also kept out of the SRCS
 * list on purpose: CMakeLists.txt explains why adding a translation unit
 * there shifts link order and with it the image.
 */
#ifndef ESP_WIFI_CONFIG_URI_H
#define ESP_WIFI_CONFIG_URI_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Result of wcfg_uri_decode(). */
typedef enum {
    WCFG_URI_DECODE_OK = 0,     ///< Decoded; @p out is NUL-terminated
    WCFG_URI_DECODE_EMPTY,      ///< Input (or its decoding) was empty
    WCFG_URI_DECODE_MALFORMED,  ///< "%" not followed by two hex digits
    WCFG_URI_DECODE_TOO_LONG,   ///< Decoded value does not fit in @p cap - 1 bytes
} wcfg_uri_decode_result_t;

static inline int wcfg_uri_hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/**
 * @brief Percent-decode one URI path segment into a fixed buffer.
 *
 * Decodes every "%XX" (case-insensitive hex) to its byte. Anything else is
 * copied through unchanged -- including '+'. The "+ means space" rule belongs
 * to application/x-www-form-urlencoded (query strings and form bodies), not to
 * path segments: RFC 3986 leaves '+' as an ordinary sub-delim in a path, and
 * the Web UI's encodeURIComponent() never emits it for a space (it emits %20,
 * and encodes a literal '+' as %2B). Mapping '+' here would make an SSID or
 * variable key containing a real '+' unreachable.
 *
 * On any result other than WCFG_URI_DECODE_OK the contents of @p out are
 * unspecified beyond being NUL-terminated; callers must not use a partially
 * decoded value. Overflow is an error, never a truncation: a truncated SSID
 * or key would silently address a different record.
 *
 * @param src  Raw segment, NUL-terminated. Not required to stop at '/'.
 * @param out  Destination buffer.
 * @param cap  Size of @p out in bytes; at most cap - 1 decoded bytes are kept.
 */
static inline wcfg_uri_decode_result_t wcfg_uri_decode(const char *src, char *out, size_t cap)
{
    size_t n = 0;

    if (cap == 0) {
        return WCFG_URI_DECODE_TOO_LONG;
    }
    out[0] = '\0';

    for (const char *p = src; *p != '\0'; p++) {
        char c = *p;
        if (c == '%') {
            int hi = wcfg_uri_hexval(p[1]);
            int lo = (hi >= 0) ? wcfg_uri_hexval(p[2]) : -1;
            /* "%00" is rejected with the malformed escapes: a NUL would end
             * the C string early and silently address a shorter name. */
            if (hi < 0 || lo < 0 || (hi == 0 && lo == 0)) {
                out[0] = '\0';
                return WCFG_URI_DECODE_MALFORMED;
            }
            c = (char)((hi << 4) | lo);
            p += 2;
        }
        if (n + 1 >= cap) {
            out[0] = '\0';
            return WCFG_URI_DECODE_TOO_LONG;
        }
        out[n++] = c;
    }
    out[n] = '\0';

    return (n == 0) ? WCFG_URI_DECODE_EMPTY : WCFG_URI_DECODE_OK;
}

#ifdef __cplusplus
}
#endif

#endif /* ESP_WIFI_CONFIG_URI_H */
