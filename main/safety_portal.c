#include "safety_portal.h"

#include "jpeg_probe.h"
#include "jpeg_store.h"
#include "jpeg_view.h"
#include "safety_store.h"

#include "cJSON.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "mbedtls/sha256.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define QR_UPLOAD_MAX (120u * 1024u)

static const char *TAG = "safety_portal";
static httpd_handle_t s_http;
static TaskHandle_t s_dns_task;
static esp_netif_t *s_ap_netif;
static bool s_netif_ready;
static bool s_event_loop_ready;
static bool s_running;
static volatile bool s_dns_running;
static volatile bool s_saved;
static safety_profile_t *s_profile;
static char s_ssid[33];
static char s_password[17];

typedef struct __attribute__((packed)) {
    uint16_t id, flags, questions, answers, authority, additional;
} dns_header_t;

typedef struct __attribute__((packed)) {
    uint16_t pointer, type, class_value;
    uint32_t ttl;
    uint16_t length;
    uint32_t address;
} dns_answer_t;

static const char PAGE_HTML[] =
"<!doctype html><html lang=zh-CN><head><meta charset=utf-8>"
"<meta name=viewport content='width=device-width,initial-scale=1,viewport-fit=cover'>"
"<title>设置老人安心牌</title><style>"
":root{color-scheme:light}*{box-sizing:border-box}body{margin:0;background:#e9f5ff;color:#17202a;"
"font-family:-apple-system,BlinkMacSystemFont,'Segoe UI','PingFang SC',sans-serif}"
"main{max-width:620px;margin:auto;padding:24px 16px 56px}.hero{background:#1689e8;color:#fff;"
"padding:26px 22px;border:4px solid #17202a;box-shadow:6px 7px 0 #17202a}"
".tag{color:#ffe25b;font:800 13px ui-monospace,monospace;letter-spacing:.12em}"
"h1{font-size:30px;margin:10px 0 6px}.hero p{margin:0;line-height:1.6}.card{background:#fff;"
"margin-top:18px;padding:20px;border:3px solid #17202a;box-shadow:5px 6px 0 #17202a}"
"h2{font-size:19px;margin:0 0 14px}label{display:block;font-weight:700;margin:16px 0 7px}"
"input,textarea{width:100%;padding:13px 12px;border:2px solid #aebbc3;border-radius:0;font:inherit;background:#fff}"
"textarea{min-height:88px;resize:vertical}.row{display:grid;grid-template-columns:1fr 1fr;gap:12px}"
".check{display:flex;gap:9px;align-items:flex-start;font-weight:500}.check input{width:auto;margin-top:4px}"
".tip{font-size:13px;color:#5f6d75;line-height:1.55}button{width:100%;border:3px solid #17202a;"
"padding:15px;background:#ffd928;color:#17202a;font-size:17px;font-weight:900;margin-top:20px;"
"box-shadow:4px 5px 0 #17202a}.status{display:none;margin-top:16px;padding:13px;background:#dbffe7;"
"border:2px solid #17202a}@media(max-width:460px){.row{grid-template-columns:1fr}h1{font-size:26px}}</style>"
"</head><body><main><section class=hero><div class=tag>FOLOTOY / SAFE CARD</div>"
"<h1>设置随身安心信息</h1><p>资料从手机直接写入当前设备，不经过互联网。保存后设备会关闭热点。</p>"
"</section><form id=form><section class=card><h2>基本信息</h2>"
"<label>称呼或姓名 *</label><input id=name maxlength=12 required placeholder='例如：王爷爷'>"
"<label>求助说明 *</label><textarea id=help maxlength=36 required>您好，我可能迷路了，请帮我联系家人</textarea>"
"<label>居住区域</label><input id=area maxlength=22 placeholder='例如：上海市徐汇区田林街道'>"
"<label>完整家庭住址</label><textarea id=address maxlength=48 placeholder='请确认展示完整住址的隐私风险'></textarea>"
"<label class=check><input id=showAddress type=checkbox><span>允许设备直接展示完整家庭住址</span></label>"
"</section><section class=card><h2>联系家人</h2><div class=row><div><label>联系人</label>"
"<input id=contact maxlength=12 placeholder='王女士'></div><div><label>与老人关系</label>"
"<input id=relation maxlength=8 placeholder='女儿'></div></div><label>联系电话</label>"
"<input id=phone inputmode=tel maxlength=24 placeholder='138 0000 0000'><label>备用联系电话</label>"
"<input id=backup inputmode=tel maxlength=24><p class=tip>联系电话会完整显示，方便帮助者直接联系家人。</p>"
"<label>家属微信二维码</label><input id=qr type=file accept='image/jpeg,image/png,image/webp'>"
"<p class=tip>手机会把图片居中裁成清晰的 220×220 二维码。建议上传边缘完整、正对屏幕的二维码截图。</p>"
"<label>微信联系说明</label>"
"<input id=wechat maxlength=25 value='请添加我的家人，备注安心牌'></section>"
"<section class=card><h2>健康提醒</h2><label>过敏、疾病、常用药或照护提醒</label>"
"<textarea id=medical maxlength=70 placeholder='例如：对青霉素过敏；患有高血压'></textarea></section>"
"<section class=card><h2>管理与隐私</h2><label>管理密码（4～6 位数字）</label>"
"<input id=pin type=password inputmode=numeric pattern='[0-9]{4,6}' maxlength=6 placeholder='留空则保留原密码'>"
"<p class=tip>以后重新设置时需要这个密码。设备只保存加盐摘要，不保存明文。</p>"
"<label class=check><input id=confirm type=checkbox required><span>我确认以上内容会显示给拿到设备的人，并同意写入设备本地。</span></label>"
"<button id=save type=submit>保存到安心牌</button><div id=status class=status></div></section></form></main>"
"<script>const $=id=>document.getElementById(id);let adminPin='';"
"async function api(path,opt={}){opt.headers=opt.headers||{};if(adminPin)opt.headers['X-Admin-Pin']=adminPin;"
"const r=await fetch(path,opt);if(!r.ok)throw new Error(await r.text()||('请求失败 '+r.status));return r}"
"async function load(){try{let st=await(await fetch('/status')).json();if(st.pin_required){adminPin=prompt('请输入安心牌管理密码')||'';}"
"let p=await(await api('/profile')).json();let map={name:'name',help_text:'help',home_area:'area',home_address:'address',"
"contact_name:'contact',relation:'relation',phone:'phone',backup_phone:'backup',medical:'medical',wechat_note:'wechat'};"
"for(let k in map)$(map[k]).value=p[k]||'';$('showAddress').checked=!!p.show_full_address"
"}catch(e){alert(e.message)}}"
"function jpeg(file){return new Promise((resolve,reject)=>{let u=URL.createObjectURL(file),im=new Image;im.onload=()=>{"
"let c=document.createElement('canvas'),s=220,p=8,side=Math.min(im.width,im.height),sx=(im.width-side)/2,sy=(im.height-side)/2;"
"c.width=s;c.height=s;let x=c.getContext('2d');x.fillStyle='#fff';x.fillRect(0,0,s,s);"
"x.imageSmoothingEnabled=false;x.drawImage(im,sx,sy,side,side,p,p,s-p*2,s-p*2);"
"c.toBlob(b=>{URL.revokeObjectURL(u);b?resolve(b):reject(new Error('二维码处理失败'))},'image/jpeg',1)};"
"im.onerror=()=>reject(new Error('无法读取二维码图片'));im.src=u})}"
"$('form').onsubmit=async e=>{e.preventDefault();let b=$('save'),s=$('status');b.disabled=true;b.textContent='正在写入...';"
"try{let f=$('qr').files[0];if(f){let blob=await jpeg(f);await api('/wechat-qr',{method:'POST',headers:{'Content-Type':'image/jpeg'},body:blob})}"
"let data={name:$('name').value,help_text:$('help').value,home_area:$('area').value,home_address:$('address').value,"
"contact_name:$('contact').value,relation:$('relation').value,phone:$('phone').value,backup_phone:$('backup').value,"
"medical:$('medical').value,wechat_note:$('wechat').value,show_full_address:$('showAddress').checked,"
"show_full_phone:true,pin:$('pin').value};await api('/save',{method:'POST',headers:{'Content-Type':'application/json'},"
"body:JSON.stringify(data)});s.style.display='block';s.textContent='保存成功。设备正在关闭热点并返回安心牌。';b.textContent='已保存'"
"}catch(err){s.style.display='block';s.style.background='#fff0ec';s.textContent=err.message;b.disabled=false;b.textContent='重新保存'}};load();"
"</script></body></html>";

static size_t dns_question_end(const uint8_t *packet, size_t length)
{
    size_t index = sizeof(dns_header_t);
    while (index < length && packet[index] != 0) {
        size_t label = packet[index];
        if (label > 63 || index + label + 1 >= length) return 0;
        index += label + 1;
    }
    return index + 5 <= length ? index + 5 : 0;
}

static void dns_server_task(void *argument)
{
    (void)argument;
    int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_port = htons(53),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    struct timeval timeout = {.tv_sec = 1};
    uint8_t packet[320];
    if (fd < 0 || bind(fd, (struct sockaddr *)&address, sizeof(address)) != 0) {
        if (fd >= 0) close(fd);
        s_dns_task = NULL;
        vTaskDelete(NULL);
        return;
    }
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    while (s_dns_running) {
        struct sockaddr_in source;
        socklen_t source_length = sizeof(source);
        int length = recvfrom(fd, packet,
                              sizeof(packet) - sizeof(dns_answer_t), 0,
                              (struct sockaddr *)&source, &source_length);
        if (length <= 0) continue;
        size_t end = dns_question_end(packet, (size_t)length);
        dns_header_t *header = (dns_header_t *)packet;
        if (end == 0 || ntohs(header->questions) != 1) continue;
        header->flags = htons(0x8180);
        header->answers = htons(1);
        dns_answer_t *answer = (dns_answer_t *)(packet + length);
        answer->pointer = htons(0xC00C);
        answer->type = htons(1);
        answer->class_value = htons(1);
        answer->ttl = htonl(30);
        answer->length = htons(4);
        answer->address = inet_addr("192.168.4.1");
        sendto(fd, packet, length + sizeof(*answer), 0,
               (struct sockaddr *)&source, source_length);
    }
    close(fd);
    s_dns_task = NULL;
    vTaskDelete(NULL);
}

static bool copy_json_string(cJSON *root, const char *name,
                             char *output, size_t capacity)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, name);
    if (!cJSON_IsString(item) || !item->valuestring) return false;
    size_t length = strlen(item->valuestring);
    if (length >= capacity) return false;
    memcpy(output, item->valuestring, length + 1);
    return true;
}

static bool json_bool(cJSON *root, const char *name, bool fallback)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, name);
    return cJSON_IsBool(item) ? cJSON_IsTrue(item) : fallback;
}

static void hash_pin(const uint8_t salt[16], const char *pin,
                     uint8_t output[32])
{
    uint8_t input[24];
    size_t length = strlen(pin);
    memcpy(input, salt, 16);
    memcpy(input + 16, pin, length);
    mbedtls_sha256(input, 16 + length, output, 0);
    memset(input, 0, sizeof(input));
}

static bool valid_pin_format(const char *pin)
{
    size_t length = pin ? strlen(pin) : 0;
    if (length < 4 || length > 6) return false;
    for (size_t i = 0; i < length; ++i) {
        if (!isdigit((unsigned char)pin[i])) return false;
    }
    return true;
}

static bool request_authorized(httpd_req_t *request)
{
    if (!s_profile || !s_profile->configured ||
        !safety_profile_has_pin(s_profile)) {
        return true;
    }
    char pin[8] = {0};
    size_t length = httpd_req_get_hdr_value_len(request, "X-Admin-Pin");
    if (length == 0 || length >= sizeof(pin) ||
        httpd_req_get_hdr_value_str(request, "X-Admin-Pin", pin,
                                    sizeof(pin)) != ESP_OK) {
        return false;
    }
    uint8_t digest[32];
    hash_pin(s_profile->pin_salt, pin, digest);
    unsigned difference = 0;
    for (size_t i = 0; i < sizeof(digest); ++i) {
        difference |= digest[i] ^ s_profile->pin_hash[i];
    }
    memset(pin, 0, sizeof(pin));
    memset(digest, 0, sizeof(digest));
    return difference == 0;
}

static esp_err_t forbidden(httpd_req_t *request)
{
    httpd_resp_set_status(request, "403 Forbidden");
    return httpd_resp_sendstr(request, "管理密码错误");
}

static esp_err_t root_get(httpd_req_t *request)
{
    httpd_resp_set_type(request, "text/html; charset=utf-8");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, PAGE_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t status_get(httpd_req_t *request)
{
    char json[96];
    snprintf(json, sizeof(json), "{\"configured\":%s,\"pin_required\":%s}",
             s_profile && s_profile->configured ? "true" : "false",
             s_profile && safety_profile_has_pin(s_profile) ? "true" : "false");
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_sendstr(request, json);
}

static void add_string(cJSON *root, const char *name, const char *value)
{
    cJSON_AddStringToObject(root, name, value ? value : "");
}

static esp_err_t profile_get(httpd_req_t *request)
{
    if (!request_authorized(request)) return forbidden(request);
    cJSON *root = cJSON_CreateObject();
    if (!root) return ESP_ERR_NO_MEM;
    add_string(root, "name", s_profile->name);
    add_string(root, "help_text", s_profile->help_text);
    add_string(root, "home_area", s_profile->home_area);
    add_string(root, "home_address", s_profile->home_address);
    add_string(root, "contact_name", s_profile->contact_name);
    add_string(root, "relation", s_profile->relation);
    add_string(root, "phone", s_profile->phone);
    add_string(root, "backup_phone", s_profile->backup_phone);
    add_string(root, "medical", s_profile->medical);
    add_string(root, "wechat_note", s_profile->wechat_note);
    cJSON_AddBoolToObject(root, "show_full_address", s_profile->show_full_address);
    cJSON_AddBoolToObject(root, "show_full_phone", s_profile->show_full_phone);
    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json) return ESP_ERR_NO_MEM;
    httpd_resp_set_type(request, "application/json; charset=utf-8");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    esp_err_t result = httpd_resp_sendstr(request, json);
    free(json);
    return result;
}

static char *receive_body(httpd_req_t *request, size_t maximum)
{
    if (request->content_len <= 0 ||
        (size_t)request->content_len > maximum) {
        return NULL;
    }
    char *body = calloc(1, (size_t)request->content_len + 1);
    if (!body) return NULL;
    int received = 0;
    while (received < request->content_len) {
        int count = httpd_req_recv(request, body + received,
                                   request->content_len - received);
        if (count <= 0) {
            free(body);
            return NULL;
        }
        received += count;
    }
    return body;
}

static esp_err_t save_post(httpd_req_t *request)
{
    if (!request_authorized(request)) return forbidden(request);
    char *body = receive_body(request, 4096);
    if (!body) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST,
                                   "表单数据无效");
    }
    cJSON *root = cJSON_Parse(body);
    memset(body, 0, strlen(body));
    free(body);
    if (!root) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST,
                                   "JSON 无效");
    }

    safety_profile_t next = *s_profile;
    bool valid = copy_json_string(root, "name", next.name, sizeof(next.name)) &&
                 next.name[0] &&
                 copy_json_string(root, "help_text", next.help_text,
                                  sizeof(next.help_text)) && next.help_text[0] &&
                 copy_json_string(root, "home_area", next.home_area,
                                  sizeof(next.home_area)) &&
                 copy_json_string(root, "home_address", next.home_address,
                                  sizeof(next.home_address)) &&
                 copy_json_string(root, "contact_name", next.contact_name,
                                  sizeof(next.contact_name)) &&
                 copy_json_string(root, "relation", next.relation,
                                  sizeof(next.relation)) &&
                 copy_json_string(root, "phone", next.phone,
                                  sizeof(next.phone)) &&
                 copy_json_string(root, "backup_phone", next.backup_phone,
                                  sizeof(next.backup_phone)) &&
                 copy_json_string(root, "medical", next.medical,
                                  sizeof(next.medical)) &&
                 copy_json_string(root, "wechat_note", next.wechat_note,
                                  sizeof(next.wechat_note));
    next.show_full_address = json_bool(root, "show_full_address", false);
    next.show_full_phone = 1; /* Always show a callable family number. */

    cJSON *pin = cJSON_GetObjectItemCaseSensitive(root, "pin");
    if (valid && cJSON_IsString(pin) && pin->valuestring &&
        pin->valuestring[0]) {
        valid = valid_pin_format(pin->valuestring);
        if (valid) {
            for (size_t i = 0; i < sizeof(next.pin_salt); i += 4) {
                uint32_t random = esp_random();
                memcpy(next.pin_salt + i, &random, 4);
            }
            hash_pin(next.pin_salt, pin->valuestring, next.pin_hash);
        }
    }
    cJSON_Delete(root);

    if (!valid || (next.phone[0] == '\0' && !jpeg_store_has_valid())) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST,
                                   "请填写姓名、求助说明，并至少提供电话或微信二维码；若字段过长请适当精简");
    }
    if (!safety_store_save(&next)) {
        return httpd_resp_send_err(request,
                                   HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "保存失败");
    }
    *s_profile = next;
    s_saved = true;
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_sendstr(request, "{\"ok\":true}");
}

static esp_err_t qr_post(httpd_req_t *request)
{
    if (!request_authorized(request)) return forbidden(request);
    if (jpeg_view_is_busy() || jpeg_view_is_active()) {
        httpd_resp_set_status(request, "409 Conflict");
        return httpd_resp_sendstr(request,
                                  "设备正在显示二维码，请返回后重试");
    }
    if (request->content_len < 100 ||
        (size_t)request->content_len > QR_UPLOAD_MAX) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST,
                                   "二维码图片大小不合适");
    }

    uint8_t buffer[1024];
    int received = 0;
    bool started = false;
    while (received < request->content_len) {
        int remaining = request->content_len - received;
        int count = httpd_req_recv(request, (char *)buffer,
                                   remaining < (int)sizeof(buffer)
                                       ? remaining : (int)sizeof(buffer));
        if (count == HTTPD_SOCK_ERR_TIMEOUT) continue;
        if (count <= 0) {
            if (started) jpeg_store_clear();
            return httpd_resp_send_err(request,
                                       HTTPD_500_INTERNAL_SERVER_ERROR,
                                       "接收图片失败");
        }
        if (!started) {
            if (count < 2 || buffer[0] != 0xFF || buffer[1] != 0xD8 ||
                jpeg_store_begin((uint32_t)request->content_len) != 0) {
                return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST,
                                           "只接受设备可识别的 JPEG 图片");
            }
            started = true;
        }
        if (jpeg_store_write(buffer, count) != 0) {
            jpeg_store_clear();
            return httpd_resp_send_err(request,
                                       HTTPD_500_INTERNAL_SERVER_ERROR,
                                       "写入图片失败");
        }
        received += count;
    }
    if (jpeg_store_end() != 0) {
        jpeg_store_clear();
        return httpd_resp_send_err(request,
                                   HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "完成图片失败");
    }

    const uint8_t *jpeg = NULL;
    int jpeg_length = 0;
    jpeg_probe_t probe = JPEG_PROBE_NOT_JPEG;
    if (jpeg_store_mmap(&jpeg, &jpeg_length) == 0) {
        probe = jpeg_probe(jpeg, jpeg_length);
        jpeg_store_unmap();
    }
    if (probe != JPEG_PROBE_OK) {
        jpeg_store_clear();
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST,
                                   "二维码图片格式不兼容，请换一张清晰截图");
    }
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_sendstr(request, "{\"ok\":true}");
}

static esp_err_t redirect_404(httpd_req_t *request, httpd_err_code_t error)
{
    (void)error;
    return root_get(request);
}

static esp_err_t register_uri(const char *uri, httpd_method_t method,
                              esp_err_t (*handler)(httpd_req_t *))
{
    httpd_uri_t route = {
        .uri = uri,
        .method = method,
        .handler = handler,
        .user_ctx = NULL,
    };
    return httpd_register_uri_handler(s_http, &route);
}

static void make_credentials(void)
{
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    snprintf(s_ssid, sizeof(s_ssid), "AnXin-%02X%02X", mac[4], mac[5]);
    static const char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    for (size_t i = 0; i < 12; ++i) {
        s_password[i] = alphabet[esp_random() % (sizeof(alphabet) - 1)];
    }
    s_password[12] = '\0';
}

bool safety_portal_start(safety_profile_t *profile, bool first_setup)
{
    (void)first_setup;
    if (s_running || !profile) return false;
    s_profile = profile;
    s_saved = false;
    make_credentials();

    if (!s_netif_ready) {
        esp_err_t err = esp_netif_init();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return false;
        s_netif_ready = true;
    }
    if (!s_event_loop_ready) {
        esp_err_t err = esp_event_loop_create_default();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return false;
        s_event_loop_ready = true;
    }
    s_ap_netif = esp_netif_create_default_wifi_ap();
    if (!s_ap_netif) return false;

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&init) != ESP_OK) goto fail_netif;
    wifi_config_t config = {0};
    strncpy((char *)config.ap.ssid, s_ssid, sizeof(config.ap.ssid) - 1);
    strncpy((char *)config.ap.password, s_password,
            sizeof(config.ap.password) - 1);
    config.ap.ssid_len = strlen(s_ssid);
    config.ap.channel = 1;
    config.ap.max_connection = 2;
    config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    if (esp_wifi_set_mode(WIFI_MODE_AP) != ESP_OK ||
        esp_wifi_set_config(WIFI_IF_AP, &config) != ESP_OK ||
        esp_wifi_start() != ESP_OK) {
        goto fail_wifi;
    }

    httpd_config_t http_config = HTTPD_DEFAULT_CONFIG();
    http_config.max_uri_handlers = 8;
    http_config.stack_size = 6144;
    http_config.lru_purge_enable = true;
    if (httpd_start(&s_http, &http_config) != ESP_OK) goto fail_started_wifi;
    if (register_uri("/", HTTP_GET, root_get) != ESP_OK ||
        register_uri("/status", HTTP_GET, status_get) != ESP_OK ||
        register_uri("/profile", HTTP_GET, profile_get) != ESP_OK ||
        register_uri("/save", HTTP_POST, save_post) != ESP_OK ||
        register_uri("/wechat-qr", HTTP_POST, qr_post) != ESP_OK ||
        httpd_register_err_handler(s_http, HTTPD_404_NOT_FOUND,
                                   redirect_404) != ESP_OK) {
        goto fail_http;
    }

    s_dns_running = true;
    if (xTaskCreate(dns_server_task, "safety_dns", 3072, NULL, 3,
                    &s_dns_task) != pdPASS) {
        s_dns_running = false;
        goto fail_http;
    }
    s_running = true;
    ESP_LOGI(TAG, "Local setup AP started: %s", s_ssid);
    return true;

fail_http:
    httpd_stop(s_http);
    s_http = NULL;
fail_started_wifi:
    esp_wifi_stop();
fail_wifi:
    esp_wifi_deinit();
fail_netif:
    if (s_ap_netif) {
        esp_netif_destroy_default_wifi(s_ap_netif);
        s_ap_netif = NULL;
    }
    return false;
}

void safety_portal_stop(void)
{
    if (!s_running) return;
    s_dns_running = false;
    for (int i = 0; i < 15 && s_dns_task; ++i) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    if (s_http) {
        httpd_stop(s_http);
        s_http = NULL;
    }
    esp_wifi_stop();
    esp_wifi_deinit();
    if (s_ap_netif) {
        esp_netif_destroy_default_wifi(s_ap_netif);
        s_ap_netif = NULL;
    }
    s_running = false;
    ESP_LOGI(TAG, "Setup complete; Wi-Fi stack stopped");
}

bool safety_portal_is_running(void) { return s_running; }

bool safety_portal_take_saved(void)
{
    bool value = s_saved;
    s_saved = false;
    return value;
}

const char *safety_portal_ssid(void) { return s_ssid; }
const char *safety_portal_password(void) { return s_password; }
