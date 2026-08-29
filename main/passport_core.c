// main/passport_core.c —— 创智学员 Passport 纯逻辑层实现。
//
// 自带一个最小 SHA-256/HMAC-SHA256 实现（按 FIPS 180-4 编写），
// 使本模块在 host 与固件上行为逐位一致、且无任何外部依赖。
#include "passport_core.h"

#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// SHA-256（FIPS 180-4）
// ---------------------------------------------------------------------------

typedef struct {
    uint32_t state[8];
    uint64_t total_len;   // 已处理消息总字节数
    uint8_t  block[64];   // 未满一块的缓存
    size_t   block_len;
} sha256_ctx_t;

static const uint32_t K[64] = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
    0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
    0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
    0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
    0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
    0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
    0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
    0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
    0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
    0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
    0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
    0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U,
};

static uint32_t rotr32(uint32_t x, unsigned n)
{
    return (x >> n) | (x << (32U - n));
}

static void sha256_compress(sha256_ctx_t *ctx, const uint8_t block[64])
{
    uint32_t w[64];
    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)block[i * 4] << 24) |
               ((uint32_t)block[i * 4 + 1] << 16) |
               ((uint32_t)block[i * 4 + 2] << 8) |
               ((uint32_t)block[i * 4 + 3]);
    }
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = rotr32(w[i - 15], 7) ^ rotr32(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = rotr32(w[i - 2], 17) ^ rotr32(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    uint32_t a = ctx->state[0], b = ctx->state[1], c = ctx->state[2],
             d = ctx->state[3], e = ctx->state[4], f = ctx->state[5],
             g = ctx->state[6], h = ctx->state[7];

    for (int i = 0; i < 64; i++) {
        uint32_t s1 = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t t1 = h + s1 + ch + K[i] + w[i];
        uint32_t s0 = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = s0 + maj;
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }

    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c;
    ctx->state[3] += d; ctx->state[4] += e; ctx->state[5] += f;
    ctx->state[6] += g; ctx->state[7] += h;
}

static void sha256_init(sha256_ctx_t *ctx)
{
    ctx->state[0] = 0x6a09e667U; ctx->state[1] = 0xbb67ae85U;
    ctx->state[2] = 0x3c6ef372U; ctx->state[3] = 0xa54ff53aU;
    ctx->state[4] = 0x510e527fU; ctx->state[5] = 0x9b05688cU;
    ctx->state[6] = 0x1f83d9abU; ctx->state[7] = 0x5be0cd19U;
    ctx->total_len = 0;
    ctx->block_len = 0;
}

static void sha256_update(sha256_ctx_t *ctx, const uint8_t *data, size_t len)
{
    ctx->total_len += len;
    while (len > 0) {
        size_t space = sizeof(ctx->block) - ctx->block_len;
        size_t take = len < space ? len : space;
        memcpy(ctx->block + ctx->block_len, data, take);
        ctx->block_len += take;
        data += take;
        len -= take;
        if (ctx->block_len == sizeof(ctx->block)) {
            sha256_compress(ctx, ctx->block);
            ctx->block_len = 0;
        }
    }
}

static void sha256_final(sha256_ctx_t *ctx, uint8_t out[32])
{
    uint64_t bit_len = ctx->total_len * 8U;

    uint8_t pad = 0x80;
    sha256_update(ctx, &pad, 1);
    uint8_t zero = 0x00;
    while (ctx->block_len != 56) {
        sha256_update(ctx, &zero, 1);
    }
    uint8_t len_bytes[8];
    for (int i = 0; i < 8; i++) {
        len_bytes[i] = (uint8_t)(bit_len >> (56 - i * 8));
    }
    // 手动追加长度，不再计入 total_len（已不需要）。
    memcpy(ctx->block + 56, len_bytes, 8);
    sha256_compress(ctx, ctx->block);

    for (int i = 0; i < 8; i++) {
        out[i * 4]     = (uint8_t)(ctx->state[i] >> 24);
        out[i * 4 + 1] = (uint8_t)(ctx->state[i] >> 16);
        out[i * 4 + 2] = (uint8_t)(ctx->state[i] >> 8);
        out[i * 4 + 3] = (uint8_t)(ctx->state[i]);
    }
}

static void sha256(const uint8_t *data, size_t len, uint8_t out[32])
{
    sha256_ctx_t ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, data, len);
    sha256_final(&ctx, out);
}

// ---------------------------------------------------------------------------
// HMAC-SHA256（RFC 2104），密钥固定 32 字节，不长于块长，无需先哈希。
// ---------------------------------------------------------------------------

static void hmac_sha256(const uint8_t key[PASSPORT_SECRET_LEN],
                        const uint8_t *msg, size_t msg_len,
                        uint8_t out[32])
{
    uint8_t k_ipad[64];
    uint8_t k_opad[64];
    for (size_t i = 0; i < 64; i++) {
        uint8_t k = i < PASSPORT_SECRET_LEN ? key[i] : 0x00;
        k_ipad[i] = k ^ 0x36;
        k_opad[i] = k ^ 0x5c;
    }

    sha256_ctx_t ctx;
    uint8_t inner[32];

    sha256_init(&ctx);
    sha256_update(&ctx, k_ipad, sizeof(k_ipad));
    sha256_update(&ctx, msg, msg_len);
    sha256_final(&ctx, inner);

    sha256_init(&ctx);
    sha256_update(&ctx, k_opad, sizeof(k_opad));
    sha256_update(&ctx, inner, sizeof(inner));
    sha256_final(&ctx, out);

    // 清痕：中间密钥材料不留栈上残影（尽力而为）。
    memset(k_ipad, 0, sizeof(k_ipad));
    memset(k_opad, 0, sizeof(k_opad));
    memset(inner, 0, sizeof(inner));
}

// ---------------------------------------------------------------------------
// 公开 API
// ---------------------------------------------------------------------------

uint32_t passport_window(uint32_t ts)
{
    return ts / PASSPORT_WINDOW_SECONDS;
}

int passport_time_is_valid(uint32_t ts)
{
    return ts >= PASSPORT_MIN_VALID_TS;
}

void passport_qr_code(const uint8_t secret[PASSPORT_SECRET_LEN],
                      const char *pid, uint32_t ts,
                      char out_code[PASSPORT_CODE_LEN + 1])
{
    char msg[96];
    int msg_len = snprintf(msg, sizeof(msg), "qr:%s:%lu",
                           pid, (unsigned long)passport_window(ts));

    uint8_t mac[32];
    hmac_sha256(secret, (const uint8_t *)msg, (size_t)msg_len, mac);

    uint32_t v = ((uint32_t)mac[0] << 24) | ((uint32_t)mac[1] << 16) |
                 ((uint32_t)mac[2] << 8) | (uint32_t)mac[3];
    snprintf(out_code, PASSPORT_CODE_LEN + 1, "%06lu",
             (unsigned long)(v % 1000000U));

    memset(mac, 0, sizeof(mac));
}

int passport_qr_url(const char *pid, uint32_t ts,
                    const char code[PASSPORT_CODE_LEN + 1],
                    char *buf, size_t buflen)
{
    int len = snprintf(buf, buflen, "%s?pid=%s&ts=%lu&c=%s",
                       PASSPORT_QR_BASE_URL, pid,
                       (unsigned long)ts, code);
    if (len < 0 || (size_t)len >= buflen) return -1;
    return len;
}

void passport_auth_sig(const uint8_t secret[PASSPORT_SECRET_LEN],
                       const char *pid, uint32_t ts,
                       char out_sig_hex[PASSPORT_SIG_HEX_LEN + 1])
{
    char msg[96];
    int msg_len = snprintf(msg, sizeof(msg), "auth:%s:%lu",
                           pid, (unsigned long)ts);

    uint8_t mac[32];
    hmac_sha256(secret, (const uint8_t *)msg, (size_t)msg_len, mac);

    static const char HEX[] = "0123456789abcdef";
    for (int i = 0; i < 32; i++) {
        out_sig_hex[i * 2]     = HEX[mac[i] >> 4];
        out_sig_hex[i * 2 + 1] = HEX[mac[i] & 0x0f];
    }
    out_sig_hex[PASSPORT_SIG_HEX_LEN] = '\0';

    memset(mac, 0, sizeof(mac));
}

void passport_avatar_pattern(const char *student_id,
                             uint8_t out[PASSPORT_AVATAR_SIZE][PASSPORT_AVATAR_SIZE])
{
    uint8_t digest[32];
    sha256((const uint8_t *)student_id, strlen(student_id), digest);

    // 左半 4 列取自哈希前 4 字节（每字节一列，位即像素），右半镜像。
    for (int r = 0; r < PASSPORT_AVATAR_SIZE; r++) {
        for (int c = 0; c < PASSPORT_AVATAR_SIZE / 2; c++) {
            uint8_t on = (uint8_t)((digest[c] >> r) & 1U);
            out[r][c] = on;
            out[r][PASSPORT_AVATAR_SIZE - 1 - c] = on;
        }
    }

    memset(digest, 0, sizeof(digest));
}
