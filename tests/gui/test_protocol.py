#!/usr/bin/env python3
"""Protocol test for qnvim's GUI mode (src/gui/qnvim_gui.lua).

Runs a real Neovim the way NvimGuiSession does (--embed, the Lua glue via
--cmd) and plays Qt Creator's part over msgpack-rpc: it checks the
notifications Neovim sends ('bufenter', 'modified', 'written', 'bufdelete',
'cursor', 'action') and the functions Qt Creator calls (QNVimGui.open/add/
write/close/set_actions).

No Qt Creator needed. Requirements: Neovim >= 0.10, `pip install pynvim`.

    python3 tests/gui/test_protocol.py [--nvim /path/to/nvim]
"""

import argparse
import os
import sys
import tempfile
import time
import unittest
import warnings

warnings.filterwarnings("ignore", category=DeprecationWarning)

import pynvim

HERE = os.path.dirname(os.path.abspath(__file__))
LUA = os.path.normpath(os.path.join(HERE, "..", "..", "src", "gui", "qnvim_gui.lua"))
NVIM = "nvim"


class GuiProtocolTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.a = self._file("a.cpp", "int a;\nint b;\nint c;\n")
        self.b = self._file("b.cpp", "one\ntwo\nthree\nfour\n")
        self.u = self._file("u.txt", "äöü x\n")
        # The stdio channel of an --embed client is channel 1; Qt Creator sets
        # the variable from NeovimConnector::channel() instead.
        argv = [NVIM, "--embed", "--headless", "--clean",
                "--cmd", "let g:qnvim_gui_channel=1",
                "--cmd", f"lua dofile([==[{LUA}]==])",
                self.a]
        self.nvim = pynvim.attach("child", argv=argv)
        self.nvim.command("set hidden")

    def tearDown(self):
        try:
            self.nvim.command("qall!")
        except Exception:
            pass
        self.nvim.close()
        self.tmp.cleanup()

    # helpers ---------------------------------------------------------------

    def _file(self, name, text):
        path = os.path.join(self.tmp.name, name)
        with open(path, "w", encoding="utf-8") as f:
            f.write(text)
        return os.path.realpath(path)

    def events(self, wait=0.0):
        """Notifications sent to Qt Creator since the last call."""
        if wait:
            time.sleep(wait)
        self.nvim.command("call rpcnotify(1, 'QNVimGui', '__sentinel__')")
        out = []
        while True:
            msg = self.nvim.next_message()
            if msg is None:
                break
            kind, name, args = msg
            if kind != "notification" or name != "QNVimGui":
                continue
            if args[0] == "__sentinel__":
                break
            out.append(tuple(args))
        return out

    def lua(self, code, *args):
        arg = args[0] if len(args) == 1 else list(args)
        return self.nvim.call("luaeval", code, arg)

    def norm(self, path):
        return os.path.realpath(path)

    # tests -----------------------------------------------------------------

    def test_startup_reports_initial_buffer(self):
        ev = self.events()
        self.assertIn(("bufenter", self.a), [(e[0], self.norm(e[1])) for e in ev])

    def test_add_does_not_switch(self):
        self.events()
        self.lua("QNVimGui.add(_A)", self.b)
        ev = self.events()
        self.assertFalse([e for e in ev if e[0] == "bufenter"])
        listed = self.nvim.call("getbufinfo", {"buflisted": 1})
        self.assertIn(self.b, [self.norm(i["name"]) for i in listed])

    def test_open_at_line_and_column(self):
        self.events()
        self.lua("QNVimGui.open(_A[1], _A[2], _A[3], _A[4])", self.b, 3, 2, False)
        ev = self.events()
        self.assertIn(("bufenter", self.b), [(e[0], self.norm(e[1])) for e in ev])
        self.assertEqual(tuple(self.nvim.current.window.cursor), (3, 2))

    def test_utf16_column_maps_to_bytes(self):
        self.lua("QNVimGui.open(_A[1], _A[2], _A[3], _A[4])", self.u, 1, 3, False)
        # "äöü" is 3 UTF-16 units but 6 bytes
        self.assertEqual(tuple(self.nvim.current.window.cursor), (1, 6))

    def test_modified_written_and_saved_to_disk(self):
        self.lua("QNVimGui.open(_A[1], _A[2], _A[3], _A[4])", self.b, 1, 0, False)
        self.events()
        self.nvim.current.buffer[0] = "ONE"
        ev = self.events()
        self.assertIn(("modified", self.b, True), [(e[0], self.norm(e[1]), e[2]) for e in ev
                                                   if e[0] == "modified"])
        # Qt Creator saves a buffer that is not the current one, too.
        self.lua("QNVimGui.open(_A[1], _A[2], _A[3], _A[4])", self.a, 1, 0, False)
        self.events()
        res = self.lua("QNVimGui.write(_A)", self.b)
        self.assertEqual(res, "")
        ev = self.events()
        names = [(e[0], self.norm(e[1])) for e in ev]
        self.assertIn(("written", self.b), names)
        self.assertIn(("modified", self.b, False),
                      [(e[0], self.norm(e[1]), e[2]) for e in ev if e[0] == "modified"])
        with open(self.b, encoding="utf-8") as f:
            self.assertEqual(f.readline().strip(), "ONE")

    def test_write_unmodified_is_noop(self):
        self.assertEqual(self.lua("QNVimGui.write(_A)", self.a), "")
        self.assertEqual(self.lua("QNVimGui.write(_A)", "/nonexistent/file"), "")

    def test_close_from_qtc_is_silent(self):
        self.lua("QNVimGui.add(_A)", self.b)
        self.events()
        self.lua("QNVimGui.close(_A)", self.b)
        ev = self.events()
        self.assertFalse([e for e in ev if e[0] == "bufdelete"])
        self.assertEqual(self.nvim.call("bufexists", self.b), 0)

    def test_bdelete_in_nvim_is_reported(self):
        self.lua("QNVimGui.add(_A)", self.b)
        self.events()
        self.nvim.command("bdelete " + self.b)
        ev = self.events()
        self.assertIn(("bufdelete", self.b), [(e[0], self.norm(e[1])) for e in ev])

    def test_open_avoids_special_windows(self):
        self.nvim.command("botright new")
        self.nvim.command("setlocal buftype=nofile")
        special = self.nvim.current.window.handle
        self.lua("QNVimGui.open(_A[1], _A[2], _A[3], _A[4])", self.b, 2, 0, False)
        self.assertNotEqual(self.nvim.current.window.handle, special)
        self.assertEqual(self.norm(self.nvim.current.buffer.name), self.b)
        # the special window keeps its buffer
        win = self.nvim.request("nvim_win_get_buf", special)
        self.assertEqual(self.nvim.request("nvim_get_option_value", "buftype", {"buf": win.handle}),
                         "nofile")

    def test_open_jumps_to_existing_window(self):
        self.nvim.command("vsplit " + self.b)
        self.nvim.command("wincmd p")  # back to a.cpp
        win_b = [w.handle for w in self.nvim.windows if self.norm(w.buffer.name) == self.b][0]
        self.lua("QNVimGui.open(_A[1], _A[2], _A[3], _A[4])", self.b, 1, 0, False)
        self.assertEqual(self.nvim.current.window.handle, win_b)
        self.assertEqual(len(self.nvim.windows), 2)

    def test_cursor_is_reported_debounced(self):
        self.events()
        self.nvim.command("normal! G")
        # CursorMoved does not fire for API calls without a redraw; trigger it.
        self.nvim.command("doautocmd CursorMoved")
        ev = self.events(wait=0.4)
        cursor = [e for e in ev if e[0] == "cursor"]
        self.assertTrue(cursor, ev)
        self.assertEqual(self.norm(cursor[-1][1]), self.a)
        self.assertEqual(cursor[-1][2], 3)

    def test_float_does_not_report_bufenter(self):
        self.events()
        buf = self.nvim.request("nvim_create_buf", False, True)
        self.nvim.request("nvim_open_win", buf, True,
                          {"relative": "editor", "row": 1, "col": 1, "width": 10, "height": 2})
        ev = self.events()
        self.assertFalse([e for e in ev if e[0] == "bufenter"])

    def test_qtc_commands(self):
        self.events()
        self.lua("QNVimGui.set_actions(_A)",
                 [["ProjectExplorer.Build", "Build Project"], ["QtCreator.Locate", "Locate"]])
        self.nvim.command("Qtc ProjectExplorer.Build")
        self.nvim.command("Build")
        self.nvim.command("Locator")
        ev = self.events()
        self.assertEqual([e for e in ev if e[0] == "action"],
                         [("action", "ProjectExplorer.Build"),
                          ("action", "ProjectExplorer.Build"),
                          ("action", "QtCreator.Locate")])
        comp = self.nvim.call("getcompletion", "Qtc Proj", "cmdline")
        self.assertEqual(comp, ["ProjectExplorer.Build"])

    def test_globals(self):
        self.assertEqual(self.nvim.vars["QNVIM_GUI"], 1)
        self.assertEqual(self.nvim.vars["QNVIM"], 1)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--nvim", default=os.environ.get("NVIM_BIN", "nvim"))
    args, rest = parser.parse_known_args()
    NVIM = args.nvim
    unittest.main(argv=[sys.argv[0]] + rest, verbosity=2)
