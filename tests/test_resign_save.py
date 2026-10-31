import contextlib
import hashlib
import hmac
import io
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'saves'))
import resign_save

CONSOLE_KEY = bytes(range(16))
PORT_KEY = bytes(16)


def signed(key, body):
    return body + hmac.new(key, body, hashlib.sha1).digest()


class ResignSaveTests(unittest.TestCase):
    def make_udata(self, root):
        title = root / 'UDATA' / '4d53002a'
        save = title / '194916D15BE4'
        save.mkdir(parents=True)
        (save / 'SaveMeta.xbx').write_bytes('Name=Player1\r\n'.encode('utf-16'))
        (save / 'profile').write_bytes(signed(CONSOLE_KEY, b'profile data' * 50))
        (save / 'score').write_bytes(signed(PORT_KEY, b'score data' * 20))
        renamed = title / '000000000000'
        renamed.mkdir()
        (renamed / 'SaveMeta.xbx').write_bytes('Name=Player1\r\n'.encode('utf-16'))
        return root / 'UDATA'

    def run_tool(self, *argv):
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            resign_save.main([str(a) for a in argv])
        return output.getvalue()

    def test_folder_names_follow_the_xapi_hash(self):
        self.assertEqual(resign_save.folder_name('Player1'), '194916D15BE4')
        self.assertEqual(resign_save.folder_name('Jay Philbrook'), '560704B49078')

    def test_console_save_is_signed_for_the_port_and_input_kept(self):
        with tempfile.TemporaryDirectory() as temp:
            udata = self.make_udata(Path(temp))
            original = (udata / '4d53002a' / '194916D15BE4' / 'profile').read_bytes()
            report = self.run_tool(udata, Path(temp) / 'fixed', '--key', '')
            fixed = Path(temp) / 'fixed' / '4d53002a' / '194916D15BE4'
            profile = (fixed / 'profile').read_bytes()
            self.assertEqual(profile, signed(PORT_KEY, original[:-20]))
            self.assertEqual((udata / '4d53002a' / '194916D15BE4' / 'profile').read_bytes(), original)
            self.assertEqual((fixed / 'SaveMeta.xbx').read_bytes(),
                             (udata / '4d53002a' / '194916D15BE4' / 'SaveMeta.xbx').read_bytes())
            self.assertIn('profile re-signed', report)
            self.assertIn('score already signed for the port', report)
            self.assertIn('folder should be 194916D15BE4', report)

    def test_key_matches_recomp_xbox_sigkey(self):
        key = 'A0' * 16
        with tempfile.TemporaryDirectory() as temp:
            udata = self.make_udata(Path(temp))
            self.run_tool(udata / '4d53002a' / '194916D15BE4', Path(temp) / 'one', '--key', key)
            body = (udata / '4d53002a' / '194916D15BE4' / 'score').read_bytes()[:-20]
            self.assertEqual((Path(temp) / 'one' / 'score').read_bytes(), signed(bytes.fromhex(key), body))

    def test_bad_key_and_existing_output_are_refused(self):
        with tempfile.TemporaryDirectory() as temp:
            udata = self.make_udata(Path(temp))
            with contextlib.redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit):
                    self.run_tool(udata, Path(temp) / 'out', '--key', '1234')
                with self.assertRaises(SystemExit):
                    self.run_tool(udata, udata, '--key', '')


if __name__ == '__main__':
    unittest.main()
