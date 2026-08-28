<p align="right">
  <strong>简体中文</strong> · <a href="PULL_REQUEST_TEMPLATE.md">English</a>
</p>

> GitHub 默认使用英文 PR 模板。本文件仅作为中文填写参考，提交的 PR 标题与正文仍须使用英文。

## 摘要

<!-- 这个 PR 解决了什么问题？用户可见行为有哪些变化？ -->

## 范围与兼容性

- 受影响板卡/版本：
- 受影响子系统：
- 接线或引脚映射影响：无
- Flash、分区或持久化数据影响：无
- 向后兼容性影响：无

## 验证

| 检查 | 结果 | 证据或备注 |
| --- | --- | --- |
| Build | PASS / FAIL / NOT RUN | |
| Host tests | PASS / FAIL / NOT RUN | |
| Device tests | PASS / FAIL / NOT RUN | |
| Unverified | — | 列出仍需板卡、仪器或用户完成的检查 |

执行的命令：

```text
./tools/validate.sh
```

## 真机证据

<!-- 固件修改必须逐项写出硬件指南验收行，并附观察结果、测量值、日志片段或显示照片。纯文档修改写“Device tests: NOT RUN — no firmware change”。禁止包含凭证或设备二维码秘密。 -->

## 检查清单

- [ ] 已审查完整 diff，并排除无关或生成文件。
- [ ] 已运行 `./tools/validate.sh`，或上方每个 `NOT RUN` 字段都写明不可用的环境或设备。
- [ ] 已分别报告 build、host tests 与 device tests。
- [ ] 已更新发生变化的硬件事实或长期行为对应的权威文档。
- [ ] 除非本 PR 由发布维护者记录已经发布的基线行为，否则没有向 `docs/CHANGELOG.zh_CN.md` 添加条目。
- [ ] 已移除凭证、私密设备链接、个人数据和未脱敏日志。
