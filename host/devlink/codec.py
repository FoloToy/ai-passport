"""音频编解码：IMA ADPCM（4:1）、单声道重采样、识别前处理。

全部是纯函数/无状态类，可以在没有硬件的情况下单测（见 tests/test_codec.py）。
ADPCM 的编/解必须严格对称：任何一侧的步长重建写错，声音就会越走越歪。
"""
from __future__ import annotations

import struct

IMA_STEPS = (
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230,
    253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963,
    1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327,
    3660, 4026, 4428, 4871, 5358, 5894, 6482, 7132, 7845, 8630, 9493, 10442,
    11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
    32767,
)
INDEX_ADJ = (-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8)

# 每个 ADPCM 数据块的头部长度：vp(2, LE) + step_idx(1) + 保留(1)
BLOCK_HEADER = 4


def clamp16(v: int) -> int:
    return -32768 if v < -32768 else (32767 if v > 32767 else v)


class AdpcmDecoder:
    """解设备上传的音频块。每块自带起始预测器，丢块不会污染后续。"""

    def __init__(self) -> None:
        self.last = 0
        self.step = 0
        self.blocks = 0

    def decode(self, block: bytes) -> bytes:
        """block = [vp_lo, vp_hi, step, 0, nibbles...] → 16bit LE PCM"""
        if len(block) <= BLOCK_HEADER:
            return b""
        self.last = struct.unpack_from("<h", block, 0)[0]
        self.step = min(88, max(0, block[2]))
        v, si = self.last, self.step
        out = bytearray((len(block) - BLOCK_HEADER) * 4)

        o = 0
        for byte in block[BLOCK_HEADER:]:
            for code in ((byte >> 4) & 0x0F, byte & 0x0F):
                step = IMA_STEPS[si]
                delta = step >> 3
                if code & 4:
                    delta += step
                if code & 2:
                    delta += step >> 1
                if code & 1:
                    delta += step >> 2
                if code & 8:
                    delta = -delta
                v = clamp16(v + delta)
                struct.pack_into("<h", out, o, v)
                o += 2
                si = min(88, max(0, si + INDEX_ADJ[code & 7]))
        self.last, self.step = v, si
        self.blocks += 1
        return bytes(out)


class AdpcmEncoder:
    """编码下发给设备的播报音频。状态跨块连续，每块头部写该块的起始状态。"""

    def __init__(self) -> None:
        self.last = 0
        self.step = 0

    def _encode(self, sample: int) -> int:
        step = IMA_STEPS[self.step]
        delta = sample - self.last
        code = 0
        if delta < 0:
            code = 8
            delta = -delta
        diff = 0
        if delta >= step:
            code |= 4
            delta -= step
            diff += step
        if delta >= (step >> 1):
            code |= 2
            delta -= step >> 1
            diff += step >> 1
        if delta >= (step >> 2):
            code |= 1
            diff += step >> 2
        diff += step >> 3
        if code & 8:
            diff = -diff
        self.last = clamp16(self.last + diff)
        self.step = min(88, max(0, self.step + INDEX_ADJ[code & 7]))
        return code & 0x0F

    def encode_block(self, samples) -> bytes:
        head = struct.pack("<hBB", self.last, self.step, 0)
        body = bytearray()
        n = len(samples)
        i = 0
        while i + 1 < n:
            hi = self._encode(int(samples[i]))
            lo = self._encode(int(samples[i + 1]))
            body.append((hi << 4) | lo)
            i += 2
        if i < n:                                  # 奇数个样点，补半个字节
            body.append(self._encode(int(samples[i])) << 4)
        return head + bytes(body)


def resample_mono16(pcm: bytes, src_hz: int, dst_hz: int) -> bytes:
    """单声道 16bit 重采样。降采样前先滑动平均抗混叠，再线性插值。"""
    if src_hz == dst_hz or not pcm:
        return pcm
    import numpy as np

    x = np.frombuffer(pcm, dtype=np.int16).astype(np.float32)
    if x.size < 2:
        return b""
    if dst_hz < src_hz:
        k = max(2, int(round(src_hz / dst_hz)))
        x = np.convolve(x, np.ones(k, dtype=np.float32) / k, mode="same")
    n_out = int(round(x.size * dst_hz / src_hz))
    if n_out < 2:
        return b""
    xi = np.linspace(0, x.size - 1, n_out, dtype=np.float32)
    y = np.interp(xi, np.arange(x.size, dtype=np.float32), x)
    return np.clip(y, -32768, 32767).astype(np.int16).tobytes()


def trim_transient(pcm: bytes, hz: int = 16000, window_ms: int = 300):
    """丢掉录音开头的 codec 上电爆音。

    实测这块板子每次开录的最初 100ms 内有 464 个满幅样本（rms 23077），
    之后立刻掉到 120~650。这段不是人声，但会：
      · 让 peak 直接顶到 32768 → 削顶检测误判成"声音太大"
      · 被当成语音开头送进识别器 → 结果开头多出几个莫名其妙的字
    所以每次识别前先切掉它。只切开头，中间一个样本都不动。
    """
    import numpy as np

    x = np.frombuffer(pcm, dtype=np.int16)
    n = int(hz * window_ms / 1000)
    if x.size < hz // 5 or n <= 0:
        return pcm
    head = np.abs(x[:n])
    loud = np.flatnonzero(head >= 32000)
    if loud.size == 0:
        return pcm
    # 最后一个满幅样本之后，再多留 20ms 让滤波器收敛
    cut = int(loud[-1]) + int(hz * 0.02)
    if cut >= x.size:
        return b""
    return x[cut:].tobytes()


def prepare_for_asr(pcm: bytes, hz: int = 16000, floor_rms: float = 18.0):
    """去直流 + 90Hz 高通 + 归一化，返回 float32 数组（-1..1）。

    这块板子的 ES8311 采集带 ≈ -0.84 FS 的直流偏置（实测 mean≈-27650），
    不除掉的话 VAD 会一直判"有人说话"，识别结果全是幻觉。
    """
    import numpy as np

    x = np.frombuffer(pcm, dtype=np.int16).astype(np.float32)
    if x.size == 0:
        return x
    x -= float(x.mean())

    rc = 1.0 / (2 * np.pi * 90.0)
    alpha = rc / (rc + 1.0 / hz)
    # 一阶 IIR：y[n] = alpha*(y[n-1] + x[n] - x[n-1])，用 lfilter 避免 Python 逐点循环
    b = np.array([alpha, -alpha], dtype=np.float32)
    a = np.array([1.0, -alpha], dtype=np.float32)
    try:
        from scipy.signal import lfilter
        y = lfilter(b, a, x).astype(np.float32)
    except Exception:
        y = np.empty_like(x)
        prev_x = x[0]
        prev_y = 0.0
        for i in range(x.size):
            cur = x[i]
            prev_y = alpha * (prev_y + cur - prev_x)
            prev_x = cur
            y[i] = prev_y

    rms = float(np.sqrt(np.mean(y ** 2))) + 1e-6
    if rms < floor_rms:                    # 只有底噪，别喂给识别器
        return np.zeros(0, dtype=np.float32)

    # 300~3400Hz 带通：人声只占这一段，其余频段（风机、电流声、爆音的余振）
    # 对识别只有干扰。同一段弱信号实测：
    #   只高通 → '见过他然后就是帮我一体推进进坚持一体推'
    #   加带通 → '问过他然后就是帮我把这个坚持一个'   （更接近人话）
    # 用 FFT 软边滤波，不依赖 scipy；过渡带做斜坡避免硬切引入振铃。
    n = y.size
    spec = np.fft.rfft(y)
    f = np.fft.rfftfreq(n, 1.0 / hz)
    lo, hi = 300.0, 3400.0
    lo_edge = max(1.0, lo * 0.6)
    hi_edge = min(hz / 2.0, hi * 1.35)
    mask = np.ones_like(f)
    mask[f < lo_edge] = 0.0
    mask[f > hi_edge] = 0.0
    m = (f >= lo_edge) & (f < lo)
    if m.any():
        mask[m] = (f[m] - lo_edge) / max(1e-6, lo - lo_edge)
    m = (f > hi) & (f <= hi_edge)
    if m.any():
        mask[m] = (hi_edge - f[m]) / max(1e-6, hi_edge - hi)
    y2 = np.fft.irfft(spec * mask, n=n).astype(np.float32)
    # 带通会掉能量，按原 rms 归一回去，保证"是否静音"的判据仍然可比
    r2 = float(np.sqrt(np.mean(y2 ** 2))) + 1e-6
    y = y2 * (rms / r2)
    rms = float(np.sqrt(np.mean(y ** 2))) + 1e-6

    gain = min(8.0, 0.25 * 32768.0 / (rms * 4.0))
    y = np.clip(y * gain, -32768, 32767)
    return y / 32768.0
