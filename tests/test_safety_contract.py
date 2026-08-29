from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "main/main.c").read_text(encoding="utf-8")
PORTAL = (ROOT / "main/safety_portal.c").read_text(encoding="utf-8")
PROFILE = (ROOT / "main/safety_profile.h").read_text(encoding="utf-8")
STORE = (ROOT / "main/safety_store.c").read_text(encoding="utf-8")
UI = (ROOT / "main/ui_safety.c").read_text(encoding="utf-8")
PARTITIONS = (ROOT / "partitions.csv").read_text(encoding="utf-8")


class SafetyCardContractTest(unittest.TestCase):
    def test_idle_and_wakeup_contract(self):
        self.assertIn("NORMAL_IDLE_MS (60u * 1000u)", MAIN)
        self.assertIn("PORTAL_IDLE_MS (5u * 60u * 1000u)", MAIN)
        self.assertIn("esp_deep_sleep_enable_gpio_wakeup", MAIN)
        self.assertIn("bsp_button_deinit", MAIN)

    def test_local_only_setup_routes(self):
        for route in ('"/status"', '"/profile"', '"/save"', '"/wechat-qr"'):
            self.assertIn(route, PORTAL)
        self.assertNotIn("https://", PORTAL)
        self.assertNotIn("http://", PORTAL)
        self.assertIn("esp_wifi_deinit", PORTAL)

    def test_profile_compatibility_and_pin_storage(self):
        self.assertIn('SAFETY_NAMESPACE "trae_cfg"', STORE)
        self.assertIn('SAFETY_PROFILE_KEY "safe_card"', STORE)
        self.assertIn("pin_salt[16]", PROFILE)
        self.assertIn("pin_hash[32]", PROFILE)
        self.assertNotIn("char pin[", PROFILE)
        self.assertIn("mbedtls_sha256", PORTAL)

    def test_qr_and_protected_partition_layout(self):
        for line in (
            "factory,   app,  factory, 0x10000,  0x300000",
            "imgstore,  data, 0x40,    0x310000, 0x20000",
            "imgframe,  data, 0x41,    0x330000, 0x26000",
            "cardid,    data, nvs,     0x356000, 0x4000",
            "recovery,  app,  test,    0x700000, 0x100000",
        ):
            self.assertIn(line, PARTITIONS)
        self.assertIn("jpeg_probe", PORTAL)

    def test_unsupported_glyphs_fall_back_visibly(self):
        self.assertIn("display_safe_text", UI)
        self.assertIn("lv_font_get_glyph_dsc", UI)


if __name__ == "__main__":
    unittest.main()
