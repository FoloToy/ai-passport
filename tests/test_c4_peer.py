#!/usr/bin/env python3
"""Host tests for the PC-side Connect Four BLE peer (tools/c4_peer.py).

Only the pure logic is covered: the board mirror, the stop-and-wait frame codec
and the match-number/role rule. The transport needs a real board (or a stub) and
is exercised on hardware, not here.
"""

from __future__ import annotations

import importlib.util
import sys
import types
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]

# The transport dependency is imported at module load; stub it so the pure logic
# can be tested without bleak installed.
sys.modules.setdefault("bleak", types.SimpleNamespace(BleakClient=object, BleakScanner=object))

spec = importlib.util.spec_from_file_location("c4_peer", ROOT / "tools" / "c4_peer.py")
PEER = importlib.util.module_from_spec(spec)
spec.loader.exec_module(PEER)


def make_peer(strategy: str = "smart"):
    args = types.SimpleNamespace(strategy=strategy)
    return PEER.Peer(args)


class FrameCodecTest(unittest.TestCase):
    def test_data_frame_layout(self) -> None:
        peer = make_peer()
        frame = peer.encode(PEER.TYPE_MOVE, bytes([3, 5]))
        self.assertEqual(frame[0], PEER.MAGIC)
        self.assertEqual(frame[1], PEER.PROTO_VERSION)
        self.assertEqual(frame[2], PEER.TYPE_MOVE)
        self.assertEqual(frame[3], 0x80)               # sequence 0, data flag set
        self.assertEqual(frame[4], 0x00)               # no acknowledgement yet
        self.assertEqual(frame[5:], bytes([3, 5]))
        self.assertEqual(peer.next_seq, 0)             # encode() does not advance; send() does

    def test_ack_only_frame_has_no_sequence(self) -> None:
        peer = make_peer()
        peer.ack_seq = 4
        peer.have_ack = True
        frame = peer.encode(0, ack_only=True)
        self.assertEqual(frame[2], 0)
        self.assertEqual(frame[3], 0)
        self.assertEqual(frame[4], 0x80 | 4)

    def test_sequence_masks_to_seven_bits(self) -> None:
        peer = make_peer()
        peer.next_seq = 0x7F
        frame = peer.encode(PEER.TYPE_LEAVE)
        self.assertEqual(frame[3], 0x80 | 0x7F)


class RoleRuleTest(unittest.TestCase):
    def test_central_and_peripheral_get_opposite_sides(self) -> None:
        # The peer is always the central; the firmware gives the central P1 on odd
        # match numbers and P2 on even ones.
        peer = make_peer()
        self.assertEqual(peer.side_for(1), 1)
        self.assertEqual(peer.side_for(2), 2)
        self.assertEqual(peer.side_for(7), 1)


class BoardModelTest(unittest.TestCase):
    def test_drop_and_turn_order(self) -> None:
        game = PEER.Board()
        self.assertEqual(game.legal(), list(range(PEER.COLS)))
        game.play(0, 1)
        game.play(0, 2)
        self.assertEqual(game.heights[0], 2)
        self.assertEqual(game.moves, 2)
        self.assertEqual(game.turn, 1)
        self.assertEqual(game.cells[0][0], 1)
        self.assertEqual(game.cells[0][1], 2)

    def test_full_column_is_not_legal(self) -> None:
        game = PEER.Board()
        for _ in range(PEER.ROWS):
            game.play(0, game.turn)
        self.assertNotIn(0, game.legal())

    def test_winning_and_blocking_columns(self) -> None:
        game = PEER.Board()
        # P1 has 0,1,2 at the bottom; completing column 3 wins.
        game.play(0, 1); game.play(0, 2)
        game.play(1, 1); game.play(1, 2)
        game.play(2, 1); game.play(2, 2)
        self.assertEqual(game.turn, 1)
        self.assertEqual(game.winning_cols(), [3])

        # P2's three discs sit at row 1, but column 3 row 0 is still empty, so P2
        # cannot complete that line in one move: there is nothing to block there.
        self.assertEqual(game.blocking_cols(), [])

    def test_horizontal_win_detection(self) -> None:
        game = PEER.Board()
        for col in range(4):
            game.play(col, 1)
            game.play(col, 2)
        # P1 already holds four at row 0; extending that line is still a winning move.
        self.assertEqual(game.winning_cols(), [4])

    def test_vertical_win_detection(self) -> None:
        game = PEER.Board()
        for _ in range(3):
            game.play(0, 1)
            game.play(1, 2)
        self.assertEqual(game.turn, 1)
        self.assertEqual(game.winning_cols(), [0])

    def test_render_shape(self) -> None:
        game = PEER.Board()
        lines = game.render().splitlines()
        self.assertEqual(len(lines), PEER.ROWS + 1)
        self.assertTrue(all(len(line) == PEER.COLS for line in lines))


class MatchResetTest(unittest.TestCase):
    def test_start_match_resets_protocol_state(self) -> None:
        peer = make_peer()
        peer.next_seq = 5
        peer.ack_seq = 3
        peer.have_ack = True
        peer.have_peer_seq = True
        peer.pending = b"stale"
        peer.game.play(0, 1)
        peer.game_id = 4

        peer.start_match()

        self.assertEqual(peer.local_side, 2)      # even match number: central is P2
        self.assertEqual(peer.next_seq, 0)
        self.assertEqual(peer.ack_seq, 0)
        self.assertFalse(peer.have_ack)
        self.assertFalse(peer.have_peer_seq)
        self.assertIsNone(peer.pending)
        self.assertEqual(peer.game.moves, 0)
        self.assertEqual(peer.games_played, 1)


if __name__ == "__main__":
    unittest.main()
