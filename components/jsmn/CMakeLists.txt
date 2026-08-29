# Vendored: jsmn v1.1.0, MIT License.
# Source: https://github.com/zserge/jsmn (jsmn.h, header-only)
# 选用理由：实现文档 5.3 节指定 espressif/cjson，但改 idf_component.yml 必须用
# ESP-IDF 5.5.3 重新生成 dependencies.lock；jsmn 单头文件、零依赖、配额极小，
# 满足"解析 /passport/me 等小 JSON"需求。后续可无缝替换为 cjson。
idf_component_register(INCLUDE_DIRS ".")
