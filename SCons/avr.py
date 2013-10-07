#!/usr/bin/python3 -tt
#
# Copyright (c) 2013, Adam Simpkins
#
import math

from SCons.Builder import Builder


def generate(env):
    env.Tool('gcc')
    env.Tool('g++')
    env.Tool('ar')

    env['CC'] = 'avr-gcc'
    env['CXX'] = 'avr-g++'
    env['AR'] = 'avr-ar'
    env['RANLIB'] = 'avr-ranlib'
    env['OBJCOPY'] = 'avr-objcopy'

    env['PROGSUFFIX'] = '.elf'

    gen_hex_action = ('$OBJCOPY -O ihex -j .data -j .text -j .bss '
                      '$SOURCE $TARGET')
    avr_hex_builder = Builder(action=gen_hex_action,
                              suffix='.hex',
                              src_suffix='.elf',
                              src_builder=env['BUILDERS']['Program'])
    env.Append(BUILDERS={'AvrHexBuilder': avr_hex_builder})
    env.AddMethod(_build_avr_hex, 'AvrHex')
    env.AddMethod(_setup_avr_flags, 'SetupAvrFlags')


def exists(env):
    '''
    This function is called to detect if this tool is available.
    '''
    return env.Detect('avr-gcc')


def _build_avr_hex(env, target, source, **kwargs):
    if not isinstance(source, (list, tuple)):
        source = [source]
    if not isinstance(target, (list, tuple)):
        target = [target]

    if len(source) == 1 and source[0].endswith('.elf'):
        if kwargs:
            raise TypeError('unexpected keyword arguments')
        env.AvrHexBuilder(target, source)
        return

    if target[0].endswith('.hex'):
        elf_target = target[0][:-4] + '.elf'
    else:
        elf_target = target[0] + '.elf'
    env.Program(elf_target, source=source, **kwargs)
    env.AvrHexBuilder(target, source=elf_target)

def _setup_avr_flags(env, mcu, f_cpu):
    cpu_prescale = int(math.log(16000000 / f_cpu, 2))
    if (f_cpu * (2 ** cpu_prescale)) != 16000000:
        raise ValueError('invalid F_CPU value')
    env.Append(CPPDEFINES=[('F_CPU', f_cpu), ('CPU_PRESCALE', cpu_prescale)])

    mcu_flag = '-mmcu=' + mcu
    env.Append(CCFLAGS=mcu_flag)
    env.Append(LINKFLAGS=mcu_flag)
