"""Exercise plist argument escaping, private configs and open-log rotation."""
import os
from pathlib import Path
import platform
import plistlib
import subprocess
import tempfile
import unittest


@unittest.skipUnless(platform.system() == 'Darwin', 'macOS plutil and stat')
class ServerServiceTest(unittest.TestCase):
    def test_install_preserves_arguments_configs_and_rotate_open_log(self):
        helper = Path(__file__).resolve().parents[2] / 'scripts/server/cod2-silicon-server'
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            data = root / 'owned data & test'
            (data / 'main').mkdir(parents=True)
            (data / 'main/iw_00.iwd').touch()
            home = root / 'server home'
            environment = dict(os.environ, COD2_SERVER_HOME=str(home),
                               COD2_SERVER_AGENT_DIR=str(root / 'agents'),
                               COD2_SERVER_LABEL='io.github.stixzoor.cod2silicon.fixture')
            command = [str(helper), 'install', '--binary', '/usr/bin/true',
                       '--data', str(data), '--port', '29230', '--bind', '127.0.0.1']
            subprocess.run(command, env=environment, check=True, capture_output=True)
            plist = root / 'agents/io.github.stixzoor.cod2silicon.fixture.plist'
            values = plistlib.loads(plist.read_bytes())
            self.assertEqual(values['ProgramArguments'], [
                '/usr/bin/true', '+set', 'fs_basepath', f'"{data}"',
                '+set', 'fs_homepath', f'"{home}"', '+set', 'net_ip', '127.0.0.1',
                '+set', 'net_port', '29230', '+set', 'dedicated', '1',
                '+exec', 'server.cfg', '+map_rotate'])
            self.assertFalse(values['KeepAlive']['SuccessfulExit'])
            self.assertEqual(values['HardResourceLimits']['Core'], 0)
            config = home / 'main/server.cfg'
            self.assertEqual(config.stat().st_mode & 0o777, 0o600)
            config.write_text('set sv_hostname "private fixture"\n')
            self.assertNotEqual(subprocess.run(command, env=environment, capture_output=True).returncode, 0)
            self.assertIn('private fixture', config.read_text())
            log = home / 'logs/console.log'
            with log.open('ab') as writer:
                writer.write(b'x' * 10485760)
                writer.flush()
                inode = log.stat().st_ino
                subprocess.run([str(helper), 'rotate'], env=environment, check=True)
                writer.write(b'after rotation\n')
                writer.flush()
                self.assertEqual(log.stat().st_ino, inode)
                self.assertEqual(log.read_bytes(), b'after rotation\n')
                self.assertEqual(Path(str(log) + '.1').stat().st_size, 10485760)
            subprocess.run([str(helper), 'uninstall'], env=environment, check=True, capture_output=True)
            self.assertFalse(plist.exists())
            self.assertIn('private fixture', config.read_text())


if __name__ == '__main__':
    unittest.main()
