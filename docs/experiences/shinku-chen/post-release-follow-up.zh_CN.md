<p align="right">
  <strong>简体中文</strong> · <a href="post-release-follow-up.md">English</a>
</p>

# 发布后收尾：AI Passport 发布流程的衔接

在把 **Voice Keychain** 固件发布到 AI Passport 社区市场后沉淀。本条记录用发布后的
收尾轨道把一个 fork 的发布闭环时，可复用、对任何 AI Passport 发布都成立的通用经验。

## 先确认发布目的地再分流

发布文档里写了"发到社区"或"发到 Git"，但开发者常常只说"发布项目"。先问清目的地，
再分流到对应工作流，而不是先假设某一边。这个二选一（社区市场 vs 版本控制仓库）能避免跑错工作流。

## 社区上传需要三个输入，拆分要拎清

publisher 工作流需要：单个合并固件 `.bin`（从 `0x0` 烧）、一张代表性封面（≤10 MiB）、
一个公开的 HTTPS Git 源码页。其中两个可从现有仓库准备，源码 URL 从 `git remote -v` 解析
（有歧义时把 SSH 转 HTTPS）。`idf.py merge-bin` 合并出的镜像**只含 bootloader/分区表/app**，
**不含 SPIFFS 音频数据分区**——该分区镜像是单独生成的，容易在发布时被漏掉，导致固件能出
UI 却没有声音。发布方应把该数据分区并入 `full.bin`，或单独随 release 发布并写清烧录地址。

## 通过官方网站授权，绝不碰凭证

publisher skill 用设备码流程：跑 `authorize` 会打开官方创作者页并生成一个十分钟的码；
创作者在网站上注册或登录并批准显示的码。助手**从不**请求、接收或存储创作者的密码，
也**不**自动重试被拒绝的上传。

## 发布后收尾有它自己的同意门槛

发布后可跟三条独立轨道：把应用归档到 `plays/`、把改进建议提交成上游 issue、把可复用开发经验
提交成上游文档 PR。每条都涉及项目私有内容，都需要开发者明确同意、可用的 GitHub 通道、
以及提交前先审查。三者都不得改到开发者当前分支。

## 对下次发布的通用化

- 发布前务必确认目的地（社区 vs Git）。
- 确认合并镜像包含了（或随附发布了）出声所需的数据分区；不要假设"一个文件 = 整台设备"。
- 每条发布后轨道只有经验是通用时才回上游；纯 fork 定制按 `fork-guide.md` 留在本地。

## 相关文档

- `docs/development/publish-to-community.md` — 发布工作流。
- `docs/development/project-completion.md` — 项目完成流程与门槛。
- `docs/development/experience-notes.md` — 本条的索引。
