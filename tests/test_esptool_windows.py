"""Check the Windows reset sequence and preservation of other driver settings."""
import importlib.util
from pathlib import Path
from types import ModuleType, SimpleNamespace
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location(
    'esptool_windows', Path(__file__).resolve().parents[1] / 'tools/esptool_windows.py')
wrapper = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(wrapper)


class WindowsReset(unittest.TestCase):
    def test_handflow_updates_both_lines_and_preserves_other_settings(self):
        port = SimpleNamespace(_dtr_state=None, _rts_state=None)
        written = []

        def ioctl(port, code, buffer, write=False):
            if write:
                self.assertEqual(code, 0x1b0064)
                written.append(list(buffer))
            else:
                self.assertEqual(code, 0x1b0060)
                buffer[:] = [0x19, 0x185, 123, 456]

        with patch.object(wrapper, '_ioctl', side_effect=ioctl):
            wrapper._set_signals(port, False, True)
            wrapper._set_signals(port, True, False)
            wrapper._set_signals(port, False, False)
        self.assertEqual(written, [[0x18, 0x145, 123, 456],
                                   [0x19, 0x105, 123, 456],
                                   [0x18, 0x105, 123, 456]])
        self.assertFalse(port._dtr_state)
        self.assertFalse(port._rts_state)

    def test_reset_holds_boot_low_until_after_reset_release(self):
        strategy = SimpleNamespace(port=object(), reset_delay=0.55)
        events = []
        with patch.object(wrapper, '_set_signals', side_effect=lambda port, dtr, rts: events.append((dtr, rts))), \
                patch.object(wrapper.time, 'sleep', side_effect=lambda seconds: events.append(seconds)):
            wrapper._reset(strategy)
        self.assertEqual(events, [(False, True), 0.1, (True, False), 0.55, (False, False)])

    def test_idf_arguments_and_hardware_flow_control_are_preserved(self):
        original = []

        class ClassicReset:
            def reset(self):
                original.append(self)

        esptool, reset_module = ModuleType('esptool'), ModuleType('esptool.reset')
        reset_module.ClassicReset = ClassicReset
        flow, normal = SimpleNamespace(flow_control=True), SimpleNamespace(flow_control=False)

        def run():
            self.assertEqual(wrapper.sys.argv[1:], ['--chip', 'esp32', 'read-mac'])
            ClassicReset.reset(flow)
            ClassicReset.reset(normal)

        esptool._main = run
        with patch.dict('sys.modules', {'esptool': esptool, 'esptool.reset': reset_module}), \
                patch.object(wrapper.sys, 'platform', 'win32'), \
                patch.object(wrapper.sys, 'argv', ['wrapper', 'esptool', '--chip', 'esp32', 'read-mac']), \
                patch.object(wrapper, '_reset') as reset:
            wrapper.main()
        self.assertEqual(original, [flow])
        reset.assert_called_once_with(normal)
