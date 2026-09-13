/*
 * Host test for wcfg_uri_decode() -- the percent-decoder behind the
 * :ssid and :key path parameters in src/esp_wifi_config_http.c.
 *
 *   ./test/uri_decode/run.sh
 */
#include <stdio.h>
#include <string.h>
#include "esp_wifi_config_uri.h"

static int failures = 0;

static void expect(const char *in, size_t cap, wcfg_uri_decode_result_t want, const char *out_want)
{
    char out[64];
    memset(out, 'X', sizeof(out));
    wcfg_uri_decode_result_t got = wcfg_uri_decode(in, out, cap);
    int ok = (got == want) && (want != WCFG_URI_DECODE_OK || strcmp(out, out_want) == 0);
    if (!ok) {
        failures++;
        printf("FAIL: \"%s\" cap=%zu -> %d \"%s\" (want %d \"%s\")\n",
               in, cap, (int)got, out, (int)want, out_want ? out_want : "");
    }
}

int main(void)
{
    expect("MyWiFi",             33, WCFG_URI_DECODE_OK, "MyWiFi");
    expect("My%20Home%20WiFi",   33, WCFG_URI_DECODE_OK, "My Home WiFi");
    expect("caf%C3%A9",          33, WCFG_URI_DECODE_OK, "caf\xC3\xA9");
    expect("%2f%2F%25",          33, WCFG_URI_DECODE_OK, "//%");        /* hex case */
    expect("a+b",                33, WCFG_URI_DECODE_OK, "a+b");        /* '+' is not a space */
    expect("a%2Bb",              33, WCFG_URI_DECODE_OK, "a+b");
    expect("",                   33, WCFG_URI_DECODE_EMPTY, NULL);
    expect("%2",                 33, WCFG_URI_DECODE_MALFORMED, NULL);
    expect("%",                  33, WCFG_URI_DECODE_MALFORMED, NULL);
    expect("%zz",                33, WCFG_URI_DECODE_MALFORMED, NULL);
    expect("a%00b",              33, WCFG_URI_DECODE_MALFORMED, NULL);
    expect("abcdefghijklmnopqrstuvwxyz012345",   33, WCFG_URI_DECODE_OK, "abcdefghijklmnopqrstuvwxyz012345"); /* 32 fits */
    expect("abcdefghijklmnopqrstuvwxyz0123456",  33, WCFG_URI_DECODE_TOO_LONG, NULL);                         /* 33 does not */
    expect("abcdefghijklmnopqrstuvwxyz01234%35", 33, WCFG_URI_DECODE_OK, "abcdefghijklmnopqrstuvwxyz012345");  /* raw > cap, decoded fits */
    expect("abcdefghijklmnopqrstuvwxyz0123%2045", 33, WCFG_URI_DECODE_TOO_LONG, NULL);                        /* overflow, not truncation */
    expect("x",                   1, WCFG_URI_DECODE_TOO_LONG, NULL);

    printf("%s: uri_decode\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
