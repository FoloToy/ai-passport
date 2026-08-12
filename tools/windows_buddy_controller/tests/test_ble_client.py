import unittest

from windows_buddy_controller.ble_client import split_for_write


class BleClientTests(unittest.TestCase):
    def test_split_for_write_preserves_bytes(self):
        payload = bytes(range(100))
        chunks = split_for_write(payload, 20)
        self.assertEqual(b"".join(chunks), payload)
        self.assertTrue(all(0 < len(chunk) <= 20 for chunk in chunks))

    def test_split_for_write_rejects_invalid_size(self):
        with self.assertRaises(ValueError):
            split_for_write(b"data", 0)


if __name__ == "__main__":
    unittest.main()
