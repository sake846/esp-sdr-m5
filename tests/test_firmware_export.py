import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location('exporter', Path(__file__).resolve().parents[1] / 'tools/export_web_firmware.py')
exporter = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(exporter)


class FirmwareExport(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.build = self.root / 'build'
        (self.build / 'config').mkdir(parents=True)
        (self.build / 'CMakeCache.txt').write_text('SAMPLE_RATE_PROBE:BOOL=OFF')
        (self.build / 'config/sdkconfig.json').write_text(json.dumps({'IDF_TARGET': 'esp32s3'}))
        (self.build / 'app.bin').write_bytes(b'firmware test fixture')
        self.args = dict(extra_esptool_args={'chip': 'esp32s3'},
                         flash_settings={'flash_size': '2MB', 'flash_mode': 'dio', 'flash_freq': '80m'},
                         flash_files={'0x10000': 'app.bin'})
        self.save_args()

    def save_args(self):
        (self.build / 'flasher_args.json').write_text(json.dumps(self.args))

    def export(self):
        return exporter.export(self.build, self.root / 'output', 'esp32s3', 'ESP32-S3', 'commit', True)

    def test_manifest_and_cli_offsets(self):
        manifest = json.loads(self.export().read_text())
        variant = manifest['variants']['esp32s3']
        self.assertEqual(variant['flash_size_policy'], 'minimum')
        self.assertEqual(variant['parts'][0]['offset'], 65536)
        self.assertIn('0x10000 esp32s3/0-app.bin', (self.root / 'output/flash_args').read_text())

    def test_streaming_profile_identifies_its_application(self):
        (self.build / 'CMakeCache.txt').write_text('ESP_SDR_STREAMING:BOOL=ON\n')
        (self.build / 'config/sdkconfig.json').write_text(json.dumps({'IDF_TARGET': 'esp32s31'}))
        self.args['extra_esptool_args']['chip'] = 'esp32s31'
        self.save_args()
        path = exporter.export(self.build, self.root / 'output', 'esp32s31-stream',
                               'S31 SoapyESPSDR', 'test', True)
        variant = json.loads(path.read_text())['variants']['esp32s31-stream']
        self.assertEqual(variant['application'], 'soapysdr')
        self.assertEqual(variant['target'], 'esp32s31')

    def test_build_date_from_image_descriptor(self):
        data = bytearray(144)
        data[32:36] = bytes.fromhex('3254cdab')
        data[128:139] = b'Sep 28 2026'
        (self.build / 'app.bin').write_bytes(data)
        manifest = json.loads(self.export().read_text())
        self.assertEqual(manifest['variants']['esp32s3']['build_date'], '2026-09-28')
        self.assertIsNone(exporter.firmware_build_date(b'not an app'))

    def test_embedded_version_overrides_descriptor_date(self):
        record = dict(revision='a'*40, build_date='2026-10-06',
                      build_timestamp='2026-10-06T22:12:34Z', profile='esp32s3')
        (self.build / 'app.bin').write_bytes(b'ESP-SDR-VERSION:'+json.dumps(record).encode()+b'\0')
        variant = json.loads(self.export().read_text())['variants']['esp32s3']
        self.assertEqual(variant['git_revision'], record['revision'])
        self.assertEqual(variant['build_timestamp'], record['build_timestamp'])
        self.assertEqual(variant['build_date'], record['build_date'])

    def test_embedded_profile_mismatch_rejected(self):
        record = dict(revision='a'*40, build_date='2026-10-06',
                      build_timestamp='2026-10-06T22:12:34Z', profile='esp32c61')
        (self.build / 'app.bin').write_bytes(b'ESP-SDR-VERSION:'+json.dumps(record).encode()+b'\0')
        with self.assertRaisesRegex(ValueError, 'profile'):
            self.export()
        self.assertFalse((self.root / 'output').exists())

    def test_m5stack_profiles_export_keeps_board_identity(self):
        profiles = [('m5stack-basic', 'esp32', '4MB'),
                    ('m5stack-core2', 'esp32', '16MB'),
                    ('m5stack-cardputer', 'esp32s3', '8MB'),
                    ('m5stack-cardputer-adv', 'esp32s3', '8MB'),
                    ('m5stickc-plus2', 'esp32', '8MB'), ('ats-mini', 'esp32s3', '8MB')]
        profiles += [('lilygo-' + board, 'esp32s3', '16MB') for board in [
            't-pager', 't-display-s3-pro', 't-embed-si4732', 't-embed-cc1101', 't-embed-cc1101-plus',
            't-deck', 't-deck-pro', 't-deck-pro-v11']]
        profiles += [('m5stack-cores3', 'esp32s3', '16MB'),
                     ('m5stack-din-meter', 'esp32s3', '8MB'), ('m5sticks3', 'esp32s3', '8MB'),
                     ('m5stickc-plus', 'esp32', '4MB'), ('m5stickc', 'esp32', '4MB'),
                     ('m5stack-paper-color', 'esp32s3', '16MB'), ('m5stack-papers3', 'esp32s3', '16MB')]
        for profile, target, flash in profiles:
            with self.subTest(profile=profile):
                (self.build / 'config/sdkconfig.json').write_text(json.dumps({'IDF_TARGET': target}))
                self.args['extra_esptool_args']['chip'] = target
                self.args['flash_settings']['flash_size'] = flash
                self.save_args()
                record = dict(revision='a'*40, build_date='2026-10-24',
                              build_timestamp='2026-10-24T00:00:00Z', profile=profile)
                data = b'ESP-SDR-VERSION:'+json.dumps(record).encode()+b'\0'
                (self.build / 'app.bin').write_bytes(data)
                output = self.root / profile
                path = exporter.export(self.build, output, profile, profile, 'test', True)
                variant = json.loads(path.read_text())['variants'][profile]
                self.assertEqual(variant['target'], target)
                self.assertEqual(variant['flash_size'], flash)
                self.assertEqual(variant['flash_size_policy'], 'minimum')
                self.assertEqual(variant['git_revision'], record['revision'])

    def test_probe_firmware_rejected(self):
        for flag in ['RING_PROBE','SAMPLE_RATE_PROBE','FILTER_REGISTER_PROBE','S2_RF_PROBE','S3_RF_PROBE','C5_TUNE_PROBE']:
            with self.subTest(flag=flag):
                (self.build / 'CMakeCache.txt').write_text(flag+':BOOL=ON')
                with self.assertRaisesRegex(ValueError, 'diagnostic'):
                    self.export()
                self.assertFalse((self.root / 'output').exists())

    def test_target_mismatch_rejected(self):
        self.args['extra_esptool_args']['chip'] = 'esp32c5'
        self.save_args()
        with self.assertRaisesRegex(ValueError, 'target differs'):
            self.export()

    def test_existing_release_not_overwritten(self):
        self.export()
        with self.assertRaises(FileExistsError):
            self.export()

    def test_out_of_flash_rejected(self):
        self.args['flash_files'] = {'0x200000': 'app.bin'}
        self.save_args()
        with self.assertRaisesRegex(ValueError, 'out-of-flash'):
            self.export()

    def test_detect_resolves_configured_size_and_preserves_no_stub(self):
        (self.build / 'config/sdkconfig.json').write_text(json.dumps(
            {'IDF_TARGET': 'esp32s3', 'ESPTOOLPY_FLASHSIZE': '16MB'}))
        self.args['flash_settings']['flash_size'] = 'detect'
        self.args['extra_esptool_args']['stub'] = False
        self.save_args()
        variant = json.loads(self.export().read_text())['variants']['esp32s3']
        self.assertEqual(variant['flash_size'], '16MB')
        self.assertIn('--no-stub', (self.root / 'output/flash_command.txt').read_text())

    def test_c2_records_required_crystal(self):
        (self.build / 'config/sdkconfig.json').write_text(json.dumps(
            {'IDF_TARGET': 'esp32c2', 'XTAL_FREQ': 26}))
        self.args['extra_esptool_args']['chip'] = 'esp32c2'
        self.save_args()
        path = exporter.export(self.build, self.root / 'output', 'esp32c2', 'C2', 'test', True)
        variant = json.loads(path.read_text())['variants']['esp32c2']
        self.assertEqual(variant['xtal_mhz'], 26)
        self.assertEqual(variant['chip'], 'ESP32-C2')
