"""GMK67 (05AC:024F) static colour via stock firmware. Windows, no installs (ctypes + hid.dll).
Usage: python gmk67_static.py 00 ff 00 [brightness 0-15]
       python gmk67_static.py perkey   (Esc red, WASD blue, rest off)
Sequence (from capture #9966-9982 + aloran-dev/gmk67-rgb): 04 18, 04 13 ..[8]=01, effect page, 04 f0.
"""
import ctypes, ctypes.wintypes as wt, sys

VID, PID = 0x05AC, 0x024F
hid = ctypes.WinDLL('hid.dll'); sa = ctypes.WinDLL('setupapi.dll'); k32 = ctypes.WinDLL('kernel32.dll', use_last_error=True)

class GUID(ctypes.Structure):
    _fields_ = [('a', wt.DWORD), ('b', wt.WORD), ('c', wt.WORD), ('d', ctypes.c_ubyte * 8)]
class IFDATA(ctypes.Structure):
    _fields_ = [('cbSize', wt.DWORD), ('guid', GUID), ('flags', wt.DWORD), ('res', ctypes.c_void_p)]
class ATTR(ctypes.Structure):
    _fields_ = [('Size', wt.ULONG), ('VID', wt.USHORT), ('PID', wt.USHORT), ('Ver', wt.USHORT)]
class CAPS(ctypes.Structure):
    _fields_ = [('Usage', wt.USHORT), ('UsagePage', wt.USHORT), ('InLen', wt.USHORT), ('OutLen', wt.USHORT),
                ('FeatLen', wt.USHORT), ('res', wt.USHORT * 17), ('rest', wt.USHORT * 10)]

k32.CreateFileW.restype = wt.HANDLE
sa.SetupDiGetClassDevsW.restype = ctypes.c_void_p

def find_device():
    g = GUID(); hid.HidD_GetHidGuid(ctypes.byref(g))
    hdev = ctypes.c_void_p(sa.SetupDiGetClassDevsW(ctypes.byref(g), None, None, 0x12))
    i = 0
    while True:
        ifd = IFDATA(); ifd.cbSize = ctypes.sizeof(IFDATA)
        if not sa.SetupDiEnumDeviceInterfaces(hdev, None, ctypes.byref(g), i, ctypes.byref(ifd)):
            return None
        i += 1
        need = wt.DWORD()
        sa.SetupDiGetDeviceInterfaceDetailW(hdev, ctypes.byref(ifd), None, 0, ctypes.byref(need), None)
        buf = ctypes.create_string_buffer(need.value)
        ctypes.cast(buf, ctypes.POINTER(wt.DWORD))[0] = 8 if ctypes.sizeof(ctypes.c_void_p) == 8 else 6
        if not sa.SetupDiGetDeviceInterfaceDetailW(hdev, ctypes.byref(ifd), buf, need, None, None):
            continue
        path = ctypes.wstring_at(ctypes.addressof(buf) + 4)
        if 'vid_05ac&pid_024f' not in path.lower():
            continue
        for access in (0xC0000000, 0):
            h = k32.CreateFileW(path, access, 3, None, 3, 0, None)
            if h not in (None, ctypes.c_void_p(-1).value):
                break
        else:
            print('open failed', ctypes.get_last_error(), path)
            continue
        a = ATTR(); a.Size = ctypes.sizeof(ATTR); hid.HidD_GetAttributes(h, ctypes.byref(a))
        pp = ctypes.c_void_p(); caps = CAPS()
        if hid.HidD_GetPreparsedData(h, ctypes.byref(pp)):
            hid.HidP_GetCaps(pp, ctypes.byref(caps)); hid.HidD_FreePreparsedData(pp)
        print(f'access={access:08x} page={caps.UsagePage:04x} usage={caps.Usage:02x} '
              f'in={caps.InLen} out={caps.OutLen} feat={caps.FeatLen} {path}')
        if 'mi_00' in path.lower() and caps.FeatLen == 65:
            return h
        k32.CloseHandle(h)

def send(h, payload):
    buf = (ctypes.c_ubyte * 65)(0, *payload, *([0] * (64 - len(payload))))
    if not hid.HidD_SetFeature(h, buf, 65):
        raise OSError(f'SetFeature failed {payload[:2]}: {ctypes.get_last_error()}')
    rsp = (ctypes.c_ubyte * 65)()
    hid.HidD_GetFeature(h, rsp, 65)
    return bytes(rsp[1:5]).hex(' ')

def perkey():
    h = find_device()
    if not h:
        sys.exit('GMK67 MI_00 feature collection (65 bytes) not found')
    area = bytearray(576)
    for idx, rgb in [(1, (255, 0, 0)), (39, (0, 0, 255)), (56, (0, 0, 255)), (57, (0, 0, 255)), (58, (0, 0, 255))]:
        area[idx * 4:idx * 4 + 4] = bytes([idx, *rgb])
    area[574:576] = b'\xaa\x55'
    page = [0x80, 0, 0, 0, 0, 0, 0, 0, 0x01, 16, 0x0F, 0, 0, 0, 0xAA, 0x55]
    print('04 18 ->', send(h, [0x04, 0x18]))
    print('04 23 ->', send(h, [0x04, 0x23, 0, 0, 0, 0, 0, 0, 0x09]))
    for i in range(9):
        print(f'page {i} ->', send(h, list(area[i * 64:(i + 1) * 64])))
    print('04 13 ->', send(h, [0x04, 0x13, 0, 0, 0, 0, 0, 0, 0x01]))
    print('mode 80 ->', send(h, page))
    print('04 f0 ->', send(h, [0x04, 0xF0]))
    k32.CloseHandle(h)

def main():
    if sys.argv[1:2] == ['perkey']:
        return perkey()
    r, g, b = (int(x, 16) for x in sys.argv[1:4])
    bright = int(sys.argv[4]) if len(sys.argv) > 4 else 15
    h = find_device()
    if not h:
        sys.exit('GMK67 MI_00 feature collection (65 bytes) not found')
    page = [0x01, r, g, b, 0, 0, 0, 0, 0, bright + 1, 0x0B, 0, 0, 0, 0xAA, 0x55]
    for p in ([0x04, 0x18], [0x04, 0x13, 0, 0, 0, 0, 0, 0, 0x01], page, [0x04, 0xF0]):
        print(bytes(p[:2]).hex(' '), '->', send(h, p))
    k32.CloseHandle(h)

if __name__ == '__main__':
    main()
