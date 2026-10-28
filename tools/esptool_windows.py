"""ESP-IDF ESPTOOL_WRAPPER for Windows bridges needing a paired DTR/RTS reset."""
import ctypes
import sys
import time


def _ioctl(port, code, buffer, write=False):
    from ctypes import wintypes
    from serial import win32

    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    control = kernel.DeviceIoControl
    control.argtypes = [wintypes.HANDLE, wintypes.DWORD, wintypes.LPVOID, wintypes.DWORD,
                        wintypes.LPVOID, wintypes.DWORD, ctypes.POINTER(wintypes.DWORD),
                        ctypes.POINTER(win32.OVERLAPPED)]
    control.restype = wintypes.BOOL
    completed = wintypes.DWORD()
    overlapped = win32.OVERLAPPED()
    overlapped.hEvent = win32.CreateEvent(None, True, False, None)
    if not overlapped.hEvent:
        raise ctypes.WinError()
    pointer, size = ctypes.byref(buffer), ctypes.sizeof(buffer)
    try:
        success = control(port._port_handle, code,
                          pointer if write else None, size if write else 0,
                          None if write else pointer, 0 if write else size,
                          ctypes.byref(completed), ctypes.byref(overlapped))
        if not success:
            error = ctypes.get_last_error()
            if error != win32.ERROR_IO_PENDING:
                raise ctypes.WinError(error)
            if not win32.GetOverlappedResult(port._port_handle, ctypes.byref(overlapped),
                                            ctypes.byref(completed), True):
                raise ctypes.WinError()
    finally:
        win32.CloseHandle(overlapped.hEvent)


def _set_signals(port, dtr, rts):
    handflow = (ctypes.c_uint32 * 4)()
    _ioctl(port, 0x1b0060, handflow)  # IOCTL_SERIAL_GET_HANDFLOW
    handflow[0] = (handflow[0] & ~3) | int(dtr)
    handflow[1] = (handflow[1] & ~0xc0) | (0x40 if rts else 0)
    # One driver request avoids the transient reset caused by separate DTR/RTS writes.
    _ioctl(port, 0x1b0064, handflow, write=True)  # IOCTL_SERIAL_SET_HANDFLOW
    port._dtr_state, port._rts_state = dtr, rts


def _reset(strategy):
    _set_signals(strategy.port, False, True)
    time.sleep(0.1)
    _set_signals(strategy.port, True, False)
    time.sleep(strategy.reset_delay)
    _set_signals(strategy.port, False, False)


def main():
    import esptool
    from esptool.reset import ClassicReset

    # ESP-IDF passes the module name before the normal esptool arguments.
    if sys.argv[1:2] == ['esptool']:
        del sys.argv[1]
    if sys.platform == 'win32':
        original_reset = ClassicReset.reset

        def reset(strategy):
            if strategy.flow_control:
                return original_reset(strategy)
            return _reset(strategy)

        ClassicReset.reset = reset
    esptool._main()


if __name__ == '__main__':
    main()
