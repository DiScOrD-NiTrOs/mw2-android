#!/usr/bin/env python3
"""Regression tests for tools/xdvdfs.py and tools/fetch_asset.py.

Everything is built from small synthetic fixtures (sparse files for the large offsets);
the real disc image is never needed. Run:  python3 tests/test_assets.py
"""
import email.message
import io
import os
import struct
import subprocess
import sys
import tempfile
import unittest
import zipfile
from unittest import mock

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOOLS = os.path.join(ROOT, "tools")
sys.path.insert(0, TOOLS)

import fetch_asset  # noqa: E402
import xdvdfs  # noqa: E402

SECTOR = xdvdfs.SECTOR
XEX = b"XEX2" + bytes(range(60)) * 4


def write(path, data):
    with open(path, "wb") as handle:
        handle.write(data)


def read(path):
    with open(path, "rb") as handle:
        return handle.read()


def build_image(path, files, base=0, pad_to=None, cut=None):
    """A minimal XDVDFS volume with `files` ({name: bytes}) at partition start `base`.
    Layout inside the partition: sector 32 the descriptor, 33 the root directory, data after.
    `cut` truncates the file to that many bytes (a download that stopped early)."""
    entries = bytearray()
    data_sector = 34
    placed = []
    names = list(files)
    for index, name in enumerate(names):
        content = files[name]
        raw = name.encode("latin-1")
        record_len = (14 + len(raw) + 3) & ~3
        right = 0 if index == len(names) - 1 else (len(entries) + record_len) // 4
        entries += struct.pack("<HHIIBB", 0, right, data_sector, len(content), 0x20, len(raw)) + raw
        entries += b"\xff" * (record_len - 14 - len(raw))
        placed.append((data_sector, content))
        data_sector += (len(content) + SECTOR - 1) // SECTOR or 1
    root = bytes(entries).ljust(SECTOR, b"\xff")
    descriptor = bytearray(SECTOR)
    descriptor[:20] = xdvdfs.MAGIC
    struct.pack_into("<II", descriptor, 0x14, 33, SECTOR)
    descriptor[0x7EC:0x800] = xdvdfs.MAGIC
    with open(path, "wb") as image:
        image.seek(base + 32 * SECTOR)
        image.write(descriptor)
        image.write(root)
        for sector, content in placed:
            image.seek(base + sector * SECTOR)
            image.write(content)
        end = base + data_sector * SECTOR
        image.truncate(pad_to or end)
    if cut is not None:
        os.truncate(path, cut)
    return path


class Scratch(unittest.TestCase):
    def setUp(self):
        self._dir = tempfile.TemporaryDirectory()
        self.dir = self._dir.name

    def tearDown(self):
        self._dir.cleanup()

    def path(self, name):
        return os.path.join(self.dir, name)


class VolumeDetection(Scratch):
    def test_every_known_partition_start(self):
        for base in xdvdfs.KNOWN_BASES:
            with self.subTest(base=hex(base)):
                iso = build_image(self.path("g.iso"), {"default.xex": XEX}, base=base)
                with open(iso, "rb") as handle:
                    volume = xdvdfs.find_volume(handle, os.path.getsize(iso), scan=False)
                self.assertEqual(volume.base, base)
                self.assertTrue(fetch_asset.is_disc_image(iso))

    def test_unlisted_offset_is_found_by_scanning_and_then_extracts(self):
        base = 0x123800   # a sector boundary that is in no list
        iso = build_image(self.path("odd.iso"), {"default.xex": XEX}, base=base)
        with open(iso, "rb") as handle:
            with self.assertRaises(xdvdfs.ImageError):
                xdvdfs.find_volume(handle, os.path.getsize(iso), scan=False)
        self.assertTrue(fetch_asset.is_disc_image(iso))
        with xdvdfs.XDvdFs(iso) as fs:
            self.assertEqual(fs.base, base)
            entries = {name: (sec, size) for name, sec, size, attr, isdir in fs.walk()}
            self.assertEqual(entries["default.xex"][1], len(XEX))
            out = self.path("out.xex")
            fs.extract(*entries["default.xex"], out)
        self.assertEqual(read(out), XEX)

    def test_four_leading_bytes_do_not_decide_anything(self):
        # The failure that started this: a file beginning 00 00 00 02 is not "invalid" by
        # that alone. It is judged by its volume, wherever the volume is.
        base = 0x40000
        iso = build_image(self.path("lead.iso"), {"default.xex": XEX}, base=base)
        with open(iso, "r+b") as image:
            image.write(b"\x00\x00\x00\x02")
        self.assertTrue(fetch_asset.is_disc_image(iso))

    def test_truncated_image_is_reported_as_truncated(self):
        files = {"default.xex": XEX}
        iso = build_image(self.path("t.iso"), files, base=0xFD90000)
        # cut inside the root directory's sector range
        os.truncate(iso, 0xFD90000 + 33 * SECTOR + 100)
        with open(iso, "rb") as handle:
            with self.assertRaises(xdvdfs.TruncatedImage):
                xdvdfs.find_volume(handle, os.path.getsize(iso))
        with self.assertRaises(SystemExit) as caught:
            fetch_asset.identify_and_place(iso, "t.iso", self.dir, {})
        self.assertIn("stopped early", str(caught.exception))

    def test_descriptor_magic_alone_is_not_enough(self):
        # A plausible descriptor (root inside the file) whose trailing magic is missing:
        # only the second-magic check can reject it.
        iso = self.path("fake.iso")
        descriptor = bytearray(SECTOR)
        descriptor[:20] = xdvdfs.MAGIC
        struct.pack_into("<II", descriptor, 0x14, 33, SECTOR)
        with open(iso, "wb") as image:
            image.seek(32 * SECTOR)
            image.write(descriptor)
            image.write(bytes(4 * SECTOR))
        self.assertFalse(fetch_asset.is_disc_image(iso))

    def test_scan_finds_magic_across_chunk_boundaries(self):
        data = bytearray(100)
        data[37:37 + 20] = xdvdfs.MAGIC
        path = self.path("c.bin")
        write(path, bytes(data))
        with open(path, "rb") as handle:
            self.assertEqual(xdvdfs.scan_for_magic(handle, 100, chunk=40), [37])


class Rejection(Scratch):
    def describe(self, content):
        path = self.path("x.bin")
        write(path, content)
        return fetch_asset.identify_and_place(path, "x.bin", self.dir, {}), \
            "\n".join(fetch_asset.describe_unrecognised(path))

    def test_html_is_called_a_web_page(self):
        ok, text = self.describe(b"<!DOCTYPE html><html>Sorry, no</html>")
        self.assertFalse(ok)
        self.assertIn("web page", text)

    def test_zeros_with_the_reported_header(self):
        ok, text = self.describe(b"\x00\x00\x00\x02" + bytes(1 << 20))
        self.assertFalse(ok)
        self.assertIn("00 00 00 02", text)
        self.assertIn("size", text)

    def test_other_containers_are_named(self):
        for head, word in ((b"CISO" + bytes(100), "CISO"), (b"LIVE" + bytes(100), "STFS"),
                           (b"Rar!\x1a\x07" + bytes(100), "RAR")):
            with self.subTest(word=word):
                ok, text = self.describe(head)
                self.assertFalse(ok)
                self.assertIn(word, text)

    def test_magic_off_a_sector_boundary_is_reported(self):
        blob = bytearray(70 * SECTOR)
        blob[40000 + 1:40000 + 21] = xdvdfs.MAGIC
        ok, text = self.describe(bytes(blob))
        self.assertFalse(ok)
        self.assertIn("does occur at", text)

    def test_main_reports_before_deleting_and_leaves_nothing_behind(self):
        bad = self.path("bad.iso")
        write(bad, b"\x00\x00\x00\x02" + bytes(70 * SECTOR))
        out = self.path("into")
        os.makedirs(out)
        run = subprocess.run([sys.executable, os.path.join(TOOLS, "fetch_asset.py"),
                              "--into", out, bad], capture_output=True, text=True)
        self.assertNotEqual(run.returncode, 0)
        self.assertIn("first bytes 00 00 00 02", run.stdout + run.stderr)
        self.assertEqual([n for n in os.listdir(out)], [])


class InspectMode(Scratch):
    def run_inspect(self, path):
        return subprocess.run([sys.executable, os.path.join(TOOLS, "fetch_asset.py"), "--inspect", path],
                              capture_output=True, text=True)

    def test_valid_image_is_proven_and_untouched(self):
        iso = build_image(self.path("v.iso"), {"default.xex": XEX, "default_mp.xex": XEX}, base=0xFD90000)
        before = os.path.getsize(iso)
        run = self.run_inspect(iso)
        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
        self.assertIn("game partition at 0xFD90000", run.stdout)
        self.assertIn("default.xex, default_mp.xex", run.stdout)
        self.assertEqual(os.path.getsize(iso), before)

    def test_unknown_file_is_described_and_kept(self):
        junk = self.path("j.bin")
        write(junk, b"\x00\x00\x00\x02" + bytes(5000))
        run = self.run_inspect(junk)
        self.assertEqual(run.returncode, 1)
        self.assertIn("not recognised", run.stdout)
        self.assertTrue(os.path.exists(junk))


class ExecutableNames(Scratch):
    def test_unreadable_or_foreign_files_have_no_built_name(self):
        # Used to raise UnboundLocalError: `struct` was imported inside the try it guards.
        self.assertIsNone(fetch_asset.xex_original_name(self.path("missing.xex")))
        junk = self.path("junk.xex")
        write(junk, b"XEX2")   # a XEX2 signature but no header
        self.assertIsNone(fetch_asset.xex_original_name(junk))


class Placement(Scratch):
    def test_iso_is_placed_and_named(self):
        iso = build_image(self.path("src.iso"), {"default.xex": XEX}, base=0xFD90000)
        placed = {}
        self.assertTrue(fetch_asset.identify_and_place(iso, "Some Game (Canada).iso", self.dir, placed))
        self.assertTrue(os.path.exists(self.path("game.iso")))

    def test_an_archive_is_not_searched_for_a_disc_layout(self):
        # The whole-file search is for odd disc layouts; a (possibly huge) zip must not pay for it.
        archive = self.path("big.zip")
        with zipfile.ZipFile(archive, "w") as z:
            z.writestr("default.xex", XEX)
            z.writestr("default_mp.xex", XEX + b"mp")
        with mock.patch.object(xdvdfs, "scan_for_magic", side_effect=AssertionError("scanned")):
            self.assertTrue(fetch_asset.identify_and_place(archive, "big.zip", self.dir, {}))

    def test_xex_in_a_zip(self):
        archive = self.path("a.zip")
        with zipfile.ZipFile(archive, "w") as z:
            z.writestr("default.xex", XEX)
            z.writestr("default_mp.xex", XEX + b"mp")
        placed = {}
        self.assertTrue(fetch_asset.identify_and_place(archive, "a.zip", self.dir, placed))
        self.assertTrue(os.path.exists(self.path("default.xex")))
        self.assertTrue(os.path.exists(self.path("default_mp.xex")))


class Extraction(Scratch):
    def test_names_cannot_climb_out(self):
        self.assertNotIn("/", xdvdfs.safe_name("../x"))
        self.assertNotIn("\\", xdvdfs.safe_name("..\\x"))
        iso = build_image(self.path("e.iso"), {"../evil.xex": XEX, "ok.xex": XEX})
        out = self.path("out")
        run = subprocess.run([sys.executable, os.path.join(TOOLS, "xdvdfs.py"), iso, out],
                             capture_output=True, text=True)
        self.assertEqual(run.returncode, 0, run.stderr)
        self.assertFalse(os.path.exists(self.path("evil.xex")))
        for _, _, files in os.walk(self.dir):
            self.assertNotIn("evil.xex", files)

    def test_cli_works_under_python_O(self):
        iso = build_image(self.path("o.iso"), {"default.xex": XEX}, base=0x2080000)
        out = self.path("out")
        run = subprocess.run([sys.executable, "-O", os.path.join(TOOLS, "xdvdfs.py"), iso, out, "default.xex"],
                             capture_output=True, text=True)
        self.assertEqual(run.returncode, 0, run.stderr)
        self.assertEqual(open(os.path.join(out, "default.xex"), "rb").read(), XEX)

    def test_entry_outside_the_image_is_an_error_not_garbage(self):
        iso = build_image(self.path("b.iso"), {"default.xex": XEX})
        with xdvdfs.XDvdFs(iso) as fs:
            with self.assertRaises(xdvdfs.ImageError):
                fs.extract(10_000_000, 10, self.path("x"))


# ---- the network side, with a scripted opener ---------------------------------------

class Reply(io.BytesIO):
    def __init__(self, body=b"", content_type="application/octet-stream", length=None, name=None):
        super().__init__(body)
        self.headers = email.message.Message()
        self.headers["Content-Type"] = content_type
        if length is not None:
            self.headers["Content-Length"] = str(length)
        if name:
            self.headers["Content-Disposition"] = f'attachment; filename="{name}"'
        self.url = "https://example.test/file"


class Opener:
    def __init__(self, *replies):
        self.replies = list(replies)
        self.requests = []

    def open(self, request, timeout=None):
        self.requests.append(request.full_url)
        reply = self.replies.pop(0)
        if isinstance(reply, Exception):
            raise reply
        return reply


CONFIRM = (b'<html><form action="https://drive.usercontent.google.com/download" method="get">'
           b'<input type="hidden" name="id" value="FILEID"><input type="hidden" name="export" value="download">'
           b'<input type="hidden" name="confirm" value="t"><input type="hidden" name="uuid" value="U1">'
           b'</form></html>')
DRIVE = "https://drive.google.com/file/d/1PKlLMI4G9Y4mj2M_qwEXzUs8XVtF5vxv/view?usp=share_link"


class DriveAndDownload(Scratch):
    def test_link_shapes_give_the_id(self):
        self.assertEqual(fetch_asset.drive_file_id(DRIVE), "1PKlLMI4G9Y4mj2M_qwEXzUs8XVtF5vxv")
        self.assertEqual(fetch_asset.drive_file_id("https://drive.google.com/open?id=ABCDEFGHIJK"), "ABCDEFGHIJK")
        with self.assertRaises(SystemExit):
            fetch_asset.drive_file_id("https://drive.google.com/drive/folders/ABCDEFGHIJKL")

    def test_confirmation_page_is_answered(self):
        opener = Opener(Reply(CONFIRM, "text/html; charset=utf-8"), Reply(b"DATA", length=4, name="g.iso"))
        response, _ = fetch_asset.open_download(DRIVE, opener)
        self.assertEqual(response.read(), b"DATA")
        self.assertIn("uuid=U1", opener.requests[1])

    def test_direct_file_needs_no_confirmation(self):
        opener = Opener(Reply(b"DATA", length=4))
        response, _ = fetch_asset.open_download(DRIVE, opener)
        self.assertEqual(response.read(), b"DATA")
        self.assertEqual(len(opener.requests), 1)

    def test_quota_page(self):
        opener = Opener(Reply(b"<html>Too many users have downloaded this file recently</html>", "text/html"))
        with self.assertRaises(SystemExit) as caught:
            fetch_asset.open_download(DRIVE, opener)
        self.assertIn("refusing", str(caught.exception))

    def test_unshared_file_page(self):
        opener = Opener(Reply(b"<html>You need access</html>", "text/html"))
        with self.assertRaises(SystemExit) as caught:
            fetch_asset.open_download(DRIVE, opener)
        self.assertIn("anyone with", str(caught.exception))

    def test_complete_download(self):
        body = os.urandom(5000)
        path, name = fetch_asset.download("https://example.test/g.iso", self.dir,
                                          Opener(Reply(body, length=len(body), name="g.iso")))
        self.assertEqual(read(path), body)
        self.assertEqual(name, "g.iso")

    def test_cut_short_download_is_retried_then_fails_and_leaves_no_file(self):
        short = lambda: Reply(b"x" * 50, length=100, name="g.iso")   # noqa: E731
        with mock.patch.object(fetch_asset.time, "sleep"):
            with self.assertRaises(SystemExit) as caught:
                fetch_asset.download("https://example.test/g.iso", self.dir, Opener(short(), short(), short()))
        self.assertIn("50 bytes of 100", str(caught.exception))
        self.assertEqual(os.listdir(self.dir), [])

    def test_transient_error_then_success(self):
        with mock.patch.object(fetch_asset.time, "sleep"):
            path, _ = fetch_asset.download(
                "https://example.test/g.iso", self.dir,
                Opener(OSError("connection reset"), Reply(b"abc", length=3, name="g.iso")))
        self.assertEqual(read(path), b"abc")

    def test_empty_download_is_rejected(self):
        with mock.patch.object(fetch_asset.time, "sleep"):
            with self.assertRaises(SystemExit):
                fetch_asset.download("https://example.test/g.iso", self.dir,
                                     Opener(Reply(b""), Reply(b""), Reply(b"")))


if __name__ == "__main__":
    unittest.main(verbosity=2)
