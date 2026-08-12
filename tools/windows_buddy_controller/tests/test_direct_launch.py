import pathlib
import subprocess
import sys
import unittest


class DirectLaunchTests(unittest.TestCase):
    def test_app_can_be_loaded_as_a_standalone_script(self):
        controller_dir = pathlib.Path(__file__).resolve().parents[1]
        bootstrap = """
import runpy
import sys
import types

tk = types.ModuleType("tkinter")
tk.Tk = type("Tk", (), {})
tk.StringVar = type("StringVar", (), {})
tk.IntVar = type("IntVar", (), {})
tk.Text = type("Text", (), {})
tk.messagebox = types.ModuleType("tkinter.messagebox")
tk.ttk = types.ModuleType("tkinter.ttk")
tk.ttk.Widget = type("Widget", (), {})
sys.modules["tkinter"] = tk
sys.modules["tkinter.messagebox"] = tk.messagebox
sys.modules["tkinter.ttk"] = tk.ttk
runpy.run_path("app.py", run_name="direct_import_test")
"""
        result = subprocess.run(
            [sys.executable, "-c", bootstrap],
            cwd=controller_dir,
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()
