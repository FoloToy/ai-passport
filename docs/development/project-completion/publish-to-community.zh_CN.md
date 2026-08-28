<p align="right">
  <strong>简体中文</strong> · <a href="publish-to-community.md">English</a>
</p>

# 动作 A：发布到社区市场

本动作把固件发布到 AI Passport 社区市场。它是[项目开发完成流程](../project-completion.md)列出的六项可选动作之一。

工作流由官方发布 skill 驱动。运行一次提示词，让助手从官方包安装 skill；仓库无需提交任何东西。

## 输入

- 单个合并的 ESP `.bin`，从 `0x0` 烧录，用 `./tools/validate.sh --firmware` 构建并验证（它产出并校验合并完整镜像；不要用 `idf.py build` 代替，后者只用于日常增量编译）。
- 一张代表产品的封面图（JPEG / PNG / WebP，≤10 MiB）。
- 固件仓库的公开 HTTPS Git 页，从 `git remote -v` 解析。

## 输出

这些值构成[共享发布属性](../project-completion.md#共享发布属性)，供其它收尾动作复用：

- 应用名。
- 双语发布标题与简介。
- 封面图像。
- 源码地址。

## 步骤

1. 从官方包安装发布 skill。
2. 分析项目并整理中英文标题与简介。
3. 解析 HTTPS Git 源码。
4. 准备并校验封面。
5. 经官方站点授权。
6. 正式上传前，把全部内容展示给开发者并取得明确批准。
7. 上传并汇报响应。

## 安全与边界

- 只上传到 `https://ai-passport.folotoy.cn`。发布与更新是外部变更。
- 未经开发者确认的验证、起草与预览不授权上传。
- 助手绝不索取、接收或存储授权凭证。
- 不自动重试被拒的上传；先把响应展示给开发者，查清原因再处理。

## 相关文档

- 社区发布参考：[publish-to-community.md](../publish-to-community.md)
