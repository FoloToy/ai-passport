#!/usr/bin/env python3
"""Play Connect Four against one AI Passport board from a PC, over BLE.

The board's LINK PLAY mode advertises a custom GATT service (Nordic UART layout)
and expects the peer to speak the same tiny stop-and-wait protocol the boards use
between themselves. This script is that peer, so protocol work can be exercised
with a single board instead of two.

Requirements: Python 3.9+ and bleak.

    python -m pip install bleak
    python tools/c4_peer.py --list
    python tools/c4_peer.py --name C4-6F50
    python tools/c4_peer.py --name-prefix C4- --strategy smart --keep-playing

What it does:
  1. Scans for an advertising board whose name starts with the prefix.
  2. Connects as a central, subscribes to the board's notify characteristic and
     sends HELLO with the same protocol version and match number.
  3. Answers every MOVE the board sends with a legal column of its own, and
     acknowledges REMATCH / LEAVE. The board plays first when it is the central,
     so this peer may have to wait for its first MOVE.
  4. Prints every frame so the exchange can be compared with the board's log.

Frame format (the same layout is described in the reference note
`docs/reference/shinku-chen/two-device-ble-link.md`):

    [0] 0xC4 magic   [1] protocol version      [2] type
    [3] bit7 = carries a data packet, low 7 bits = sequence
    [4] bit7 = ack valid, low 7 bits = last accepted peer sequence
    [5..] payload

Types: HELLO [version, first_side, game_id], MOVE [col, ply], REMATCH [game_id],
LEAVE [].

Nothing here is hardware-specific beyond the UUIDs and the board's device-name
prefix; the physical device does not need to be touched.
"""

from __future__ import annotations

import argparse
import asyncio
import random
import sys
import time

try:
    from bleak import BleakClient, BleakScanner
except ImportError:  # pragma: no cover - dependency hint only
    sys.exit("bleak is required: python -m pip install bleak")

MAGIC = 0xC4
PROTO_VERSION = 1

TYPE_HELLO = 1
TYPE_MOVE = 2
TYPE_REMATCH = 3
TYPE_LEAVE = 4

PAYLOAD_LEN = {TYPE_HELLO: 3, TYPE_MOVE: 2, TYPE_REMATCH: 1, TYPE_LEAVE: 0}

COLS = 10
ROWS = 7
WIN = 4

SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
WRITE_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"   # peer -> board
NOTIFY_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"  # board -> peer

RETRY_SECONDS = 0.4
MAX_RETRIES = 5


class Board:
    """Minimal mirror of the board's game model, enough to play legally."""

    def __init__(self) -> None:
        self.cells = [[0] * ROWS for _ in range(COLS)]   # 0 empty, 1 P1, 2 P2
        self.heights = [0] * COLS
        self.turn = 1
        self.moves = 0

    def reset(self, first_player: int) -> None:
        self.cells = [[0] * ROWS for _ in range(COLS)]
        self.heights = [0] * COLS
        self.turn = first_player
        self.moves = 0

    def legal(self) -> list[int]:
        return [c for c in range(COLS) if self.heights[c] < ROWS]

    def play(self, col: int, player: int) -> None:
        row = self.heights[col]
        self.cells[col][row] = player
        self.heights[col] += 1
        self.moves += 1
        self.turn = 2 if player == 1 else 1

    def winning_cols(self) -> list[int]:
        """Columns that complete a line for the side to move, then anything legal."""
        me = self.turn
        won = []
        for col in self.legal():
            row = self.heights[col]
            self.cells[col][row] = me
            if self._is_win(col, row, me):
                won.append(col)
            self.cells[col][row] = 0
        return won

    def blocking_cols(self) -> list[int]:
        other = 2 if self.turn == 1 else 1
        blocked = []
        for col in self.legal():
            row = self.heights[col]
            self.cells[col][row] = other
            if self._is_win(col, row, other):
                blocked.append(col)
            self.cells[col][row] = 0
        return blocked

    def _is_win(self, col: int, row: int, player: int) -> bool:
        for dx, dy in ((1, 0), (0, 1), (1, 1), (1, -1)):
            total = 1
            for sign in (1, -1):
                cx, cy = col + dx * sign, row + dy * sign
                while 0 <= cx < COLS and 0 <= cy < ROWS and self.cells[cx][cy] == player:
                    total += 1
                    cx += dx * sign
                    cy += dy * sign
            if total >= WIN:
                return True
        return False

    def render(self) -> str:
        lines = []
        for row in range(ROWS - 1, -1, -1):
            cells = []
            for col in range(COLS):
                cells.append(".YX"[self.cells[col][row]])
            lines.append("".join(cells))
        lines.append("".join(str(c % 10) for c in range(COLS)))
        return "\n".join(lines)


class Peer:
    def __init__(self, args: argparse.Namespace) -> None:
        self.args = args
        self.game = Board()
        self.game_id = 1
        self.client: BleakClient | None = None
        self.local_side = 2          # this peer is the central: P2 unless game_id says otherwise
        self.next_seq = 0
        self.ack_seq = 0
        self.have_ack = False
        self.have_peer_seq = False
        self.pending: bytes | None = None
        self.pending_seq = 0
        self.retries = 0
        self.sent_at = 0.0
        self.peer_ready = False
        self.last_col = COLS // 2
        self.games_played = 0
        self.last_activity = time.monotonic()
        self.stop = asyncio.Event()

    # ---- protocol -----------------------------------------------------

    def encode(self, ptype: int, payload: bytes = b"", seq: int | None = None,
               ack_only: bool = False) -> bytes:
        frame = bytearray(5)
        frame[0] = MAGIC
        frame[1] = PROTO_VERSION
        if ack_only:
            frame[2] = 0
            frame[3] = 0
        else:
            frame[2] = ptype
            frame[3] = 0x80 | (self.next_seq & 0x7F)
        frame[4] = (0x80 | (self.ack_seq & 0x7F)) if self.have_ack else 0
        return bytes(frame) + payload

    async def send(self, ptype: int, payload: bytes = b"") -> None:
        expected = PAYLOAD_LEN[ptype]
        if len(payload) != expected:
            raise ValueError(f"type {ptype} needs {expected} payload bytes")
        frame = self.encode(ptype, payload)
        self.pending = frame
        self.pending_seq = self.next_seq & 0x7F
        self.next_seq = (self.next_seq + 1) & 0x7F
        self.retries = 0
        self.sent_at = time.monotonic()
        await self.raw_send(frame)
        print(f"[tx] type={ptype} seq={self.pending_seq} ack={self.ack_seq} payload={payload.hex() or '-'}")

    async def raw_send(self, frame: bytes) -> None:
        assert self.client is not None
        await self.client.write_gatt_char(WRITE_UUID, frame, response=False)

    async def retransmit_loop(self) -> None:
        while not self.stop.is_set():
            await asyncio.sleep(0.05)
            if self.pending is None:
                continue
            if time.monotonic() - self.sent_at < RETRY_SECONDS:
                continue
            if self.retries >= MAX_RETRIES:
                print("[!!] no acknowledgement after retries - peer considered gone")
                self.pending = None
                self.stop.set()
                return
            self.retries += 1
            self.sent_at = time.monotonic()
            await self.raw_send(self.pending)
            print(f"[tx] retransmit seq={self.pending_seq} attempt={self.retries}")

    def on_notify(self, _handle: int, data: bytearray) -> None:
        asyncio.create_task(self.handle_frame(bytes(data)))

    async def handle_frame(self, frame: bytes) -> None:
        if len(frame) < 5 or len(frame) > 8 or frame[0] != MAGIC:
            print(f"[rx] ignoring malformed frame {frame.hex()}")
            return
        if frame[1] != PROTO_VERSION:
            print(f"[rx] version mismatch {frame[1]} (expected {PROTO_VERSION}); send LEAVE")
            await self.send(TYPE_LEAVE)
            return

        if frame[4] & 0x80:
            ack = frame[4] & 0x7F
            if self.pending is not None and ack == self.pending_seq:
                self.pending = None
                self.retries = 0

        if not frame[3] & 0x80:
            print("[rx] ack-only")
            return

        ptype = frame[2]
        seq = frame[3] & 0x7F
        expected = 0 if not self.have_peer_seq else (self.ack_seq + 1) & 0x7F
        if self.have_peer_seq and seq == self.ack_seq:
            await self.raw_send(self.encode(0, ack_only=True))
            print("[rx] duplicate, re-acked")
            return
        if seq != expected:
            print(f"[rx] out-of-order seq={seq} expected={expected}; not acked")
            return
        self.ack_seq = seq
        self.have_ack = True
        self.have_peer_seq = True
        await self.raw_send(self.encode(0, ack_only=True))

        self.last_activity = time.monotonic()
        payload = frame[5:]
        if ptype == TYPE_HELLO:
            self.peer_ready = True
            self.game_id = payload[2]
            self.local_side = self.side_for(self.game_id)
            self.game.reset(1)
            print(f"[rx] HELLO peer_side={payload[1]} game_id={payload[2]}; "
                  f"this peer plays P{self.local_side}")
            print(self.game.render())
            if self.game.turn != self.local_side:
                print("[..] waiting for the board's first move")
        elif ptype == TYPE_MOVE:
            col, ply = payload[0], payload[1]
            if ply != self.game.moves + 1 or col >= COLS or self.game.heights[col] >= ROWS:
                print(f"[!!] board move {col} ply={ply} does not fit local state "
                      f"(ply should be {self.game.moves + 1}); stopping")
                self.stop.set()
                return
            self.game.play(col, 2 if self.local_side == 1 else 1)
            print(f"[rx] MOVE col={col} ply={ply}")
            print(self.game.render())
            await self.reply_move()
        elif ptype == TYPE_REMATCH:
            self.game_id = payload[0]
            print(f"[rx] REMATCH game_id={self.game_id}; agreeing")
            await self.send(TYPE_REMATCH, bytes([self.game_id]))
            self.start_match()
        elif ptype == TYPE_LEAVE:
            print("[rx] LEAVE - the board left the link")
            self.stop.set()
        else:
            print(f"[rx] unknown type {ptype}")

    def side_for(self, game_id: int) -> int:
        """Central plays P1 on odd match numbers, matching the firmware rule."""
        return 1 if game_id % 2 == 1 else 2

    async def reply_move(self) -> None:
        legal = self.game.legal()
        if not legal:
            print("[..] no legal column left; waiting for the board")
            return
        if self.args.strategy == "random":
            col = random.choice(legal)
        else:
            win = self.game.winning_cols()
            block = self.game.blocking_cols()
            col = (win or block or legal)[0]
        await self.send(TYPE_MOVE, bytes([col, self.game.moves + 1]))
        self.game.play(col, self.game.turn)
        print(f"[tx] our move col={col}")

    def start_match(self) -> None:
        self.games_played += 1
        self.local_side = self.side_for(self.game_id)
        self.game.reset(1)
        self.have_peer_seq = False
        self.have_ack = False
        self.ack_seq = 0
        self.next_seq = 0
        self.pending = None
        print(f"[==] match {self.game_id}: this peer plays P{self.local_side}")

    # ---- transport ----------------------------------------------------

    async def find_board(self) -> str:
        print(f"[..] scanning for a board named {self.args.name_prefix}* ...")
        deadline = time.monotonic() + self.args.scan_seconds
        while time.monotonic() < deadline:
            for device in await BleakScanner.discover(timeout=2.0):
                name = device.name or ""
                if self.args.name and name == self.args.name:
                    print(f"[..] found {name} [{device.address}]")
                    return device.address
                if not self.args.name and name.startswith(self.args.name_prefix) and name != "":
                    print(f"[..] found {name} [{device.address}]")
                    return device.address
        raise SystemExit(f"no board advertising {self.args.name or self.args.name_prefix + '*'} "
                         f"within {self.args.scan_seconds}s - is LINK PLAY open on it?")

    async def run(self) -> None:
        address = self.args.address or await self.find_board()
        async with BleakClient(address, timeout=self.args.connect_timeout) as client:
            self.client = client
            print(f"[..] connected to {address}; subscribing to {NOTIFY_UUID}")
            await client.start_notify(NOTIFY_UUID, self.on_notify)
            await asyncio.sleep(0.2)

            self.start_match()
            await self.send(TYPE_HELLO, bytes([PROTO_VERSION, 1 if self.local_side == 1 else 2,
                                               self.game_id]))
            pump = asyncio.create_task(self.retransmit_loop())
            while not self.stop.is_set():
                await asyncio.sleep(0.1)
                if not self.peer_ready:
                    continue
                if time.monotonic() - self.last_activity <= self.args.move_timeout:
                    continue
                if self.args.keep_playing:
                    self.last_activity = time.monotonic()
                    continue
                print("[..] no board move for a while; leaving")
                await self.send(TYPE_LEAVE)
                self.stop.set()
            pump.cancel()
            await client.stop_notify(NOTIFY_UUID)
            print("[..] done")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--list", action="store_true", help="scan and list boards, then exit")
    parser.add_argument("--name", help="exact board name, e.g. C4-6F50")
    parser.add_argument("--name-prefix", default="C4-", help="device-name prefix (default C4-)")
    parser.add_argument("--address", help="skip scanning and use this BLE address")
    parser.add_argument("--strategy", choices=("random", "smart"), default="smart",
                        help="smart wins/blocks when possible, random plays any legal column")
    parser.add_argument("--scan-seconds", type=float, default=20.0)
    parser.add_argument("--connect-timeout", type=float, default=20.0)
    parser.add_argument("--move-timeout", type=float, default=45.0,
                        help="give up after this long without a board move")
    parser.add_argument("--keep-playing", action="store_true",
                        help="keep waiting instead of leaving when the board goes quiet")
    args = parser.parse_args()

    if args.list:
        async def list_boards() -> None:
            for device in await BleakScanner.discover(timeout=6.0):
                if (device.name or "").startswith(args.name_prefix):
                    print(f"{device.name}\t{device.address}")
        asyncio.run(list_boards())
        return

    asyncio.run(Peer(args).run())


if __name__ == "__main__":
    main()
