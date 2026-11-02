#!/usr/bin/env python3
"""Reject corrupted and non-Mini images before producing release assets."""
import unittest

from package_release import ROOT, validate_mini_hex


class MiniImageValidation(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.image = (ROOT / "firmware/mini/TrackerMini_KIT_0.1.0.ptf").read_bytes()

    def test_user_confirmed_mini_image(self):
        report = validate_mini_hex(self.image)
        self.assertEqual(report["origin"], "0x60020000")
        self.assertGreater(report["payload_bytes"], 1_000_000)

    def test_corrupt_checksum(self):
        lines = self.image.decode("ascii").splitlines()
        lines[1] = lines[1][:-2] + ("00" if lines[1][-2:] != "00" else "01")
        with self.assertRaisesRegex(ValueError, "checksum"):
            validate_mini_hex("\n".join(lines).encode())

    def test_missing_end_record(self):
        lines = self.image.decode("ascii").splitlines()
        self.assertEqual(lines[-1], ":00000001FF")
        with self.assertRaisesRegex(ValueError, "Incomplete"):
            validate_mini_hex("\n".join(lines[:-1]).encode())

    def test_other_target_origin_with_valid_checksum(self):
        lines = self.image.decode("ascii").splitlines()
        first = bytearray.fromhex(lines[0][1:])
        self.assertEqual(first[3], 4)
        # Tracker+ uses 0x60030000. A valid record at that origin must still
        # fail Mini validation instead of being packaged with a Mini filename.
        first[4:6] = (0x6003).to_bytes(2, "big")
        first[-1] = (-sum(first[:-1])) & 255
        lines[0] = ":" + first.hex().upper()
        with self.assertRaisesRegex(ValueError, "origin"):
            validate_mini_hex("\n".join(lines).encode())


if __name__ == "__main__":
    unittest.main()
