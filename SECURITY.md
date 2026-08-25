<p align="right">
  <a href="SECURITY.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Security Policy

Do not disclose unpatched security vulnerabilities, credentials, tokens, or
directly exploitable details in public issues, pull requests, discussions, or
chat.

## Reporting a vulnerability

The preferred channel is GitHub's private vulnerability reporting form:

<https://github.com/FoloToy/ai-passport/security/advisories/new>

If that form is unavailable, open a public issue containing only a request for a
private security contact. Do not include vulnerability details or reproduction
materials in that issue; wait for a maintainer to provide a private channel.

## What to include

Please provide as much of the following as possible:

- affected version, commit, or distribution channel;
- affected board, component, and configuration;
- reproduction steps, a minimal PoC, or logs, after removing credentials and
  personal data;
- potential impact, exploitation requirements, and any suggested mitigation.

This repository contains firmware for a wearable AI device. Security issues may
involve the ESP32-C3 firmware, the audio input, wireless capabilities, or
protocol handling. If the issue also involves a separately maintained server or
service, identify the affected endpoint and client version so that we can route
it to the appropriate maintainers.

## Response process

We will acknowledge the report, reproduce it, assess its impact, and update the
report when a fix or mitigation can be disclosed. Our target cadence:
**acknowledgement with an initial assessment within 5 business days**, and a
**90-day coordinated disclosure window** for confirmed vulnerabilities
(adjustable in coordination with the reporter). If you hear nothing for more
than 7 days, please ping us through the other channel (email or GitHub).

## Contributor notes

- Do not put real user data, access tokens, private keys, or internal endpoints
  in issues, test fixtures, or commits.
- If you accidentally commit sensitive information, report it privately
  immediately. Deleting the file from the working tree does not invalidate
  secrets that may exist in Git history.
- Use public issues for ordinary bugs, documentation problems, and feature
  requests. Do not use the security channel for those topics.
