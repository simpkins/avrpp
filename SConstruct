import os

avr_env = Environment(tools=['default', 'avr', 'staticlib'],
                      toolpath=['SCons'])

# -O2 seems like the best trade-off so far for my code.
# The generated size isn't that much larger than -Os, and it does a much
# better job of inlining various small C++ functions (such as in pins.h and
# avr_registers.h).
#
# Using -O3 makes the code size substantially larger.
opt_flags = ['-O2']

warnings = ['-Wall', '-Wextra', '-Werror']
avr_env.Append(CCFLAGS=['-g'] + opt_flags + warnings)
avr_env.Append(CFLAGS=['-std=gnu11'])
avr_env.Append(CXXFLAGS=['-std=gnu++11'])
avr_env.Append(LDFLAGS=['-Wl,--relax,--gc-sections'])



def _header_install_path(env):
    cur_dir = Dir('.').srcnode().path
    parts = cur_dir.split(os.sep)
    if parts[0] == 'src':
        parts[0] = 'avrpp'
    parts.insert(0, env['HEADER_DIR'])
    return os.sep.join(parts)


def avr_library(env, name, source, headers, deps=None):
    node = env.StaticLibrary(name, source, deps=deps)
    env.Install(env['LIB_DIR'], [node])
    for hdr in headers:
        hdr_dir = os.path.dirname(hdr)
        env.Install(os.path.join(_header_install_path(env), hdr_dir), hdr)


def emit_descriptors(env, file_name, source):
    cpp_name = file_name + '.cpp'
    header_name = file_name + '.h'
    env.Command([cpp_name, header_name],
                [source, '#/src/usb_config.py'],
                '$SOURCE ${TARGETS[0]} -h ${TARGETS[1]}')
    env.Install(_header_install_path(env), [header_name])


avr_env.AddMethod(avr_library, 'AvrLibrary')
avr_env.AddMethod(emit_descriptors, 'EmitDescriptors')


def variant(speed, name):
    env = avr_env.Clone()
    env.SetupAvrFlags(mcu='at90usb1286', f_cpu=speed)

    variant_dir = os.path.join('build', name)
    build_dir = '#' + variant_dir
    env['BUILD_DIR'] = build_dir
    env['INSTALL_DIR'] = os.path.join(build_dir, 'install')
    env['HEADER_DIR'] = os.path.join(build_dir, 'install', 'include')
    env['LIB_DIR'] = os.path.join(build_dir, 'install', 'lib')

    env.Append(CPPPATH=env['HEADER_DIR'])
    env.Append(LIBPATH=env['LIB_DIR'])

    Export({'AVR_ENV': env})
    SConscript('src/SConscript', variant_dir=variant_dir, duplicate=False)

variant(1000000, '1MHz')
#variant(16000000, '16MHz')
