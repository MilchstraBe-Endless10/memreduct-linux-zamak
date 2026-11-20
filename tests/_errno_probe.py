"""One errno, one process: report how the advise runner classifies it.

Kept as a separate process so a previous case cannot leave state behind in the
shared library. Standalone rather than inline in the test module to keep the
ABI plumbing in one obvious place.
"""
import ctypes
import os
import pathlib
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent


class IOVec(ctypes.Structure):
    _fields_ = [('iov_base', ctypes.c_void_p), ('iov_len', ctypes.c_size_t)]


def main(argv):
    proc_root = argv[1]
    want_errno = int(argv[2])
    work = pathlib.Path(tempfile.mkdtemp())
    lib = work / 'probe.so'
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic',
                    '-shared', '-fPIC', '-DMEMREDUCT_TESTING',
                    '-DMEMREDUCT_ENABLE_PRODUCTION_ADVISE',
                    str(ROOT / 'native/crossproc_plan.c'),
                    str(ROOT / 'native/crossproc_advise.c'),
                    '-o', str(lib)], check=True)
    d = ctypes.CDLL(str(lib))

    hook = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_int32)(lambda pid: os.dup(0))
    d.mr_crossproc_set_pidfd_hook.argtypes = [ctypes.c_void_p]
    d.mr_crossproc_set_pidfd_hook(ctypes.cast(hook, ctypes.c_void_p))

    def _backend(pidfd, iov, vlen, advice, flags):
        return -1
    backend = ctypes.CFUNCTYPE(ctypes.c_long, ctypes.c_int, ctypes.POINTER(IOVec),
                               ctypes.c_size_t, ctypes.c_int, ctypes.c_uint)(_backend)
    d.mr_crossproc_set_backend.argtypes = [ctypes.c_void_p]
    d.mr_crossproc_set_backend(ctypes.cast(backend, ctypes.c_void_p))
    d.mr_crossproc_test_set_backend_errno.argtypes = [ctypes.c_int]
    d.mr_crossproc_test_set_backend_errno(want_errno)

    os.environ['MEMREDUCT_TEST_PROC_ROOT'] = proc_root
    plan = (ctypes.c_char * 8192)()
    d.mr_crossproc_plan_build.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    d.mr_crossproc_plan_build.restype = ctypes.c_int
    d.mr_crossproc_plan_build(None, ctypes.cast(plan, ctypes.c_void_p))
    d.mr_crossproc_arm_approval.argtypes = [ctypes.c_void_p]
    d.mr_crossproc_arm_approval.restype = ctypes.c_int64
    d.mr_crossproc_arm_approval(ctypes.cast(plan, ctypes.c_void_p))

    out = (ctypes.c_char * 256)()
    d.mr_crossproc_test_advise_into.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    d.mr_crossproc_test_advise_into.restype = ctypes.c_int
    d.mr_crossproc_test_advise_into(ctypes.cast(plan, ctypes.c_void_p),
                                    ctypes.cast(out, ctypes.c_void_p))
    d.mr_crossproc_test_result_status.argtypes = [ctypes.c_void_p]
    d.mr_crossproc_test_result_status.restype = ctypes.c_int64
    print(d.mr_crossproc_test_result_status(ctypes.cast(out, ctypes.c_void_p)))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
