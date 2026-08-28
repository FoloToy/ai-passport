<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Development Guidelines

This directory contains the complete engineering path from an application requirement to a verified merged firmware image. Hardware facts remain in `docs/hardware-design/`.

## Documents

- [agent-guide.md](agent-guide.md): code-change workflow, source priorities, BSP boundary, runtime rules, and delivery fields.
- [environment-setup.md](environment-setup.md): clean-machine ESP-IDF 5.5.3 setup for international and mainland-China networks.
- [build-and-test.md](build-and-test.md): exact local checks, firmware output, CI triggers, release behavior, and device-test boundary.
- [coding-conventions.md](coding-conventions.md): C, LVGL, naming, resources, and host-test rules.
- [repository-optimization-plan.md](repository-optimization-plan.md): decisions and implementation record for the minimal AI-to-firmware baseline.

Every new engineering rule states its trigger, required action, prohibited action, and validation command. Rules that can be automated must also be implemented in `tools/` or CI.
