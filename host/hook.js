#!/usr/bin/env node
/**
 * hook.js —— WorkBuddy hook 转发器（由 ~/.workbuddy/settings.json 调用）
 *
 *     node hook.js <事件名>
 *
 * 为什么用 Node 而不是 Python：hook 是**每个事件起一个进程**，本机 Python
 * 解释器冷启动实测 1.0–1.5s（波动大），Node 约 0.28s。这些延迟会直接叠在
 * WorkBuddy 的响应上，所以只让最轻的运行时干这件最轻的活。
 * 真正的逻辑在 Python 侧的 devlink.server 里。
 *
 * 铁律：绝不能拖慢 WorkBuddy
 *   · 服务没起 → 立刻退出，WorkBuddy 无感
 *   · 普通事件：一次连接、多行管道写完就走，不等回包
 *   · PreToolUse 且审批开关打开时，才阻塞等用户在设备上按键
 */
'use strict';

const net = require('net');
const fs = require('fs');
const path = require('path');

const HERE = __dirname;
const HOST = '127.0.0.1';
const PORT = parseInt(process.env.WB_DAEMON_PORT || '37821', 10);
const FLAG = path.join(HERE, '.approval_on');

// 只读工具：审批模式下也不打扰用户
const READ_ONLY = new Set([
  'Read', 'Glob', 'Grep', 'TodoWrite', 'Task', 'WebSearch', 'WebFetch',
  'NotebookRead', 'LS',
]);

function readStdin() {
  return new Promise((resolve) => {
    let buf = '';
    let done = false;
    const finish = (v) => { if (!done) { done = true; resolve(v); } };
    // WorkBuddy 有时不关 stdin；300ms 兜底，绝不卡住
    const timer = setTimeout(() => finish(buf), 300);
    process.stdin.setEncoding('utf8');
    process.stdin.on('data', (c) => { buf += c; });
    process.stdin.on('end', () => { clearTimeout(timer); finish(buf); });
    process.stdin.on('error', () => { clearTimeout(timer); finish(buf); });
  });
}

function parseJson(s) {
  try { return JSON.parse(s); } catch (_) { return {}; }
}

/**
 * 一次连接发送多行请求。
 * waitReply=true 时只等最后一条的回复并返回；否则写完就关。
 *
 * ⚠ 不能在 connect 回调里就 resolve：那时 write() 只是进了内核缓冲，
 * 一旦 main() 返回、process.exit(0) 执行，socket 被强杀，数据直接丢。
 * （实测：旧写法发出的每一条 hook 事件服务端都收不到。）
 * 所以无论如何都等 'close'——把 FIN 发出去、对端读完，才算发完。
 */
function sendBatch(reqs, waitReply, timeoutMs) {
  return new Promise((resolve) => {
    if (!reqs.length) return resolve(null);
    let settled = false;
    const finish = (v) => { if (!settled) { settled = true; resolve(v); } };

    const sock = net.createConnection({ host: HOST, port: PORT });
    sock.setTimeout(timeoutMs, () => { sock.destroy(); finish(null); });
    sock.on('error', () => finish(null));
    sock.on('connect', () => {
      sock.write(reqs.map((r) => JSON.stringify(r)).join('\n') + '\n');
      if (!waitReply) sock.end();          // end 而不是 destroy：让数据先出去
    });

    let buf = '';
    let seen = 0;
    sock.on('data', (chunk) => {
      buf += chunk.toString('utf8');
      let i;
      while ((i = buf.indexOf('\n')) >= 0) {
        seen += 1;
        const last = seen === reqs.length;
        const line = buf.slice(0, i);
        buf = buf.slice(i + 1);
        if (last && waitReply) { sock.destroy(); finish(parseJson(line)); }
      }
    });
    sock.on('close', () => finish(waitReply ? null : { ok: true }));
  });
}

/** 把工具参数压成一行摘要。 */
function brief(ti) {
  if (!ti || typeof ti !== 'object') return '';
  for (const k of ['command', 'file_path', 'path', 'pattern', 'query', 'url', 'prompt', 'description']) {
    const v = ti[k];
    if (typeof v === 'string' && v.trim()) return v.trim().replace(/\s+/g, ' ').slice(0, 100);
  }
  return '';
}

async function main() {
  const event = process.argv[2] || 'Notification';
  const data = parseJson(await readStdin());

  // 会话身份：WorkBuddy 是「单窗口多标签」，所有会话的事件都挤到同一块屏。
  // 服务侧靠这两个字段给每个会话分配 ①②③ 标签，标在每条消息前面，
  // 否则多开时屏幕上是两条会话的话交替覆盖，看不出属于谁。
  const sid = String(data.session_id || '');
  const tpath = String(data.transcript_path || '');
  const who = () => (sid ? { session_id: sid, transcript_path: tpath } : {});

  if (event === 'PreToolUse') {
    if (!fs.existsSync(FLAG)) return;              // 审批默认关闭
    const tool = data.tool_name || '';
    if (READ_ONLY.has(tool)) return;
    const r = await sendBatch([{
      op: 'approve', tool,
      detail: brief(data.tool_input),
      timeout: 100,
      ...who(),
    }], true, 110000);
    if (!r || !r.ok) return;                       // 没设备 → 交回 WorkBuddy 自己弹窗
    const decision = r.decision || 'ask';
    const out = {
      hookSpecificOutput: { hookEventName: 'PreToolUse', permissionDecision: decision },
    };
    if (decision === 'deny') {
      out.hookSpecificOutput.permissionDecisionReason =
        r.reason || '用户在 AI PASSPORT 上拒绝';
    }
    process.stdout.write(JSON.stringify(out));
    return;
  }

  if (event === 'UserPromptSubmit') {
    const raw = String(data.prompt || data.message || '');
    // 系统内部注入的伪 prompt（后台任务通知、系统提醒、XML 包装的上下文）
    // 不是用户说的话，推到屏幕只会是一大串尖括号。直接丢掉。
    if (/^\s*<(task-notification|system-reminder|additional_data|user_info|memory)\b/i.test(raw)) {
      return;
    }
    const p = raw.replace(/\s+/g, ' ').slice(0, 80);
    await sendBatch([
      { op: 'reply', text: '你：' + p, ...who() },
      { op: 'status', state: 'thinking', text: '正在处理', ...who() },
    ], false, 2000);
    return;
  }

  if (event === 'Notification') {
    const type = data.notification_type || '';
    const msg = String(data.message || '').slice(0, 120);
    if (type === 'permission_prompt') {
      await sendBatch([{ op: 'notify', title: '需要授权', body: msg, priority: 'high', ...who() }], false, 2000);
    } else if (type === 'idle_prompt') {
      await sendBatch([{
        op: 'notify', title: '在等你', body: msg || '已经空闲一会儿了', priority: 'normal', ...who(),
      }], false, 2000);
    }
    return;
  }

  if (event === 'Stop') {
    const reqs = [
      { op: 'status', state: 'idle', text: '这一轮做完了', ...who() },
      { op: 'notify', title: 'WorkBuddy', body: '这一轮做完了，回来看一眼', priority: 'normal', ...who() },
    ];
    // 把结论念出来。transcript_path 是 Stop hook 的公共字段（jsonl 对话记录），
    // 服务侧从里面取"最后一条 assistant 回复"再推流到扬声器。
    // 没有这个字段（老版本）就跳过——设备仍然只更新屏幕。
    if (data.transcript_path) {
      reqs.push({
        op: 'speak_last',
        transcript_path: data.transcript_path,
        max_chars: 120,
        ...who(),
      });
    }
    await sendBatch(reqs, false, 2000);
    return;
  }

  if (event === 'SessionStart') {
    const how = data.source || data.matcher || 'startup';
    // 注意：不能在这里 clear。多会话时新开一个标签会把别的会话的记录一起抹掉。
    // 清屏改由服务侧在"从单会话变多会话"那一刻做，因为它知道全局有几个会话。
    await sendBatch([
      { op: 'status', state: 'idle', text: '会话已开始（' + how + '）', ...who() },
    ], false, 2000);
    return;
  }

  if (event === 'SubagentStop') {
    await sendBatch([{ op: 'reply', text: '子任务完成', ...who() }], false, 2000);
  }
}

main().then(() => process.exit(0)).catch(() => process.exit(0));
