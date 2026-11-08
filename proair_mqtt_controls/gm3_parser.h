/* Portable receive-only GM3 parser. LEN includes DST..DATA; total=LEN+6.
 * CRC-16/XMODEM covers LEN..DATA. Callbacks must copy data before returning.
 * An invalid candidate is emitted intact, then only its first byte is dropped.
 * All remaining buffered suffixes are scanned immediately (multiple callbacks).
 */
#ifndef GM3_PARSER_H
#define GM3_PARSER_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#define GM3_MAX_FRAME 512
#define GM3_MIN_FRAME 11
#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
  GM3_OK, GM3_REJECT_BAD_CRC, GM3_REJECT_BAD_END, GM3_REJECT_BAD_LEN,
  GM3_REJECT_PARTIAL_TIMEOUT, GM3_REJECT_NOISE
} gm3_status_t;
typedef struct {
  gm3_status_t status;
  const uint8_t *data;
  uint16_t len;
  uint16_t declared_payload;
  uint32_t expected_total; /* can be 65541 for invalid length, must not wrap */
} gm3_result_t;
typedef void (*gm3_emit_fn)(void *, const gm3_result_t *, uint32_t);
typedef struct {
  uint8_t data[GM3_MAX_FRAME];
  uint16_t len;
  uint32_t last_byte_ms;
} gm3_parser_t;
static inline uint16_t gm3_crc16_xmodem(const uint8_t *p, size_t n) {
  uint16_t c = 0;
  for (size_t i = 0; i < n; ++i) {
    c ^= (uint16_t)p[i] << 8;
    for (int b = 0; b < 8; ++b)
      c = (c & 0x8000) ? (uint16_t)((c << 1) ^ 0x1021) : (uint16_t)(c << 1);
  }
  return c;
}
static inline gm3_status_t gm3_validate_frame(const uint8_t *d, uint16_t n) {
  if (n < GM3_MIN_FRAME || n > GM3_MAX_FRAME || d[0] != 0x68)
    return GM3_REJECT_BAD_LEN;
  uint32_t total = (uint32_t)(d[1] | ((uint16_t)d[2] << 8)) + 6u;
  if (total != n) return GM3_REJECT_BAD_LEN;
  if (d[n-1] != 0x16) return GM3_REJECT_BAD_END;
  uint16_t crc = (uint16_t)(((uint16_t)d[n-3] << 8) | d[n-2]);
  return gm3_crc16_xmodem(d+1, n-4) == crc ? GM3_OK : GM3_REJECT_BAD_CRC;
}
static inline void gm3_parser_init(gm3_parser_t *p) { memset(p, 0, sizeof(*p)); }
static inline void gm3_drop(gm3_parser_t *p, uint16_t n) {
  p->len = (uint16_t)(p->len - n);
  memmove(p->data, p->data+n, p->len);
}
static inline void gm3_emit(gm3_parser_t *p, gm3_status_t status, uint16_t n,
                            uint32_t now, gm3_emit_fn fn, void *ctx) {
  gm3_result_t r;
  r.status = status; r.data = p->data; r.len = n;
  r.declared_payload = n >= 3 && p->data[0] == 0x68 ?
    (uint16_t)(p->data[1] | ((uint16_t)p->data[2] << 8)) : 0;
  r.expected_total = n >= 3 && p->data[0] == 0x68 ? (uint32_t)r.declared_payload + 6u : 0;
  fn(ctx, &r, now);
}
/* expired=true: also reject incomplete candidates; recover complete nested frames
 * without waiting for another incoming byte or timeout. */
static inline void gm3_scan(gm3_parser_t *p, uint32_t now, int expired,
                            gm3_emit_fn fn, void *ctx) {
  while (p->len) {
    if (p->data[0] != 0x68) {
      uint16_t n = 1;
      while (n < p->len && p->data[n] != 0x68) ++n;
      gm3_emit(p, GM3_REJECT_NOISE, n, now, fn, ctx);
      gm3_drop(p, n); continue;
    }
    if (p->len >= 3) {
      uint32_t total = (uint32_t)(p->data[1] | ((uint16_t)p->data[2] << 8)) + 6u;
      if (total < GM3_MIN_FRAME || total > GM3_MAX_FRAME) {
        gm3_emit(p, GM3_REJECT_BAD_LEN, 3, now, fn, ctx);
        gm3_drop(p, 1); continue;
      }
      if (p->len >= total) {
        gm3_status_t st = gm3_validate_frame(p->data, (uint16_t)total);
        gm3_emit(p, st, (uint16_t)total, now, fn, ctx);
        gm3_drop(p, st == GM3_OK ? (uint16_t)total : 1); continue;
      }
    }
    if (!expired) return;
    gm3_emit(p, GM3_REJECT_PARTIAL_TIMEOUT, p->len, now, fn, ctx);
    gm3_drop(p, 1);
  }
}
static inline void gm3_parser_tick(gm3_parser_t *p, uint32_t now, uint32_t timeout,
                                   gm3_emit_fn fn, void *ctx) {
  if (p->len && (uint32_t)(now - p->last_byte_ms) >= timeout)
    gm3_scan(p, now, 1, fn, ctx);
}
static inline void gm3_parser_byte(gm3_parser_t *p, uint8_t b, uint32_t now,
                                   uint32_t timeout, gm3_emit_fn fn, void *ctx) {
  gm3_parser_tick(p, now, timeout, fn, ctx);
  /* Invariant: scan never leaves len==512: any valid length is <=512. */
  p->data[p->len++] = b;
  p->last_byte_ms = now;
  gm3_scan(p, now, 0, fn, ctx);
}
static inline const char *gm3_status_str(gm3_status_t s) {
  switch(s) {
    case GM3_OK: return "ok";
    case GM3_REJECT_BAD_CRC: return "bad_crc";
    case GM3_REJECT_BAD_END: return "bad_end";
    case GM3_REJECT_BAD_LEN: return "bad_len";
    case GM3_REJECT_PARTIAL_TIMEOUT: return "partial_timeout";
    case GM3_REJECT_NOISE: return "noise";
    default: return "unknown";
  }
}
#ifdef __cplusplus
}
#endif
#endif
