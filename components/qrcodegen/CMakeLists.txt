# Vendored: Nayuki QR Code generator (C), v1.8.0, MIT License.
# Source: https://github.com/nayuki/QR-Code-generator (c/qrcodegen.c, c/qrcodegen.h)
# 行为确定、单文件、host 可测；创智 Passport 登录二维码基线方案（实现文档 5.3 节）。
idf_component_register(
    SRCS "qrcodegen.c"
    INCLUDE_DIRS "."
)
