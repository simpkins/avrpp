#!/usr/bin/python3 -tt
#
# Copyright (c) 2013, Adam Simpkins
#

def prog_emitter(target, source, env):
    '''
    An emitter function for Program().

    This automatically processes static library dependencies.
    If a Program depends on libfoo.a, it automatically adds all other libraries
    that libfoo.a depends on, without having to specify them directly in the
    Program rule.

    This relies on the StaticLibrary() rule for libfoo.a specifying its
    dependencies in a deps argument in its environment.
    '''
    src_objs = []
    src_libs = set()
    for src in source:
        if src.name.startswith('lib') and src.name.endswith('.a'):
            src_libs.add(src)
        else:
            src_objs.append(src)

    # Traverse the dependency tree to find all libraries
    all_libs = {}
    lib_prefix = env['LIBPREFIX']
    lib_suffix = env['LIBSUFFIX']
    while src_libs:
        lib = src_libs.pop()
        if lib in all_libs:
            continue
        deps_param = None
        if lib.env is not None:
            deps_param = lib.env.get('deps')
        if deps_param is None:
            deps = set()
        else:
            deps = set()
            for dep in deps_param:
                parts = dep.rsplit(':', 1)
                if len(parts) == 1:
                    dep_dir = lib.dir
                    dep_base = dep
                else:
                    dep_dir = lib.dir.Dir(parts[0])
                    dep_base = parts[1]
                deps.add(env.File(lib_prefix + dep_base + lib_suffix,
                                  directory=dep_dir))
        all_libs[lib] = deps
        src_libs.update(deps)

    # Topologically sort the static libraries
    sorted_libs = []
    todo = all_libs.items()
    todo.sort(key=lambda pair: pair[0])
    while todo:
        lib = None
        for idx, (candidate, deps) in enumerate(todo):
            if not deps:
                lib = candidate
                del todo[idx]
                break
        if lib is None:
            raise Exception('dependency cycle in static libraries')
        for other, deps in todo:
            deps.discard(lib)
        sorted_libs.append(lib)
    sorted_libs.reverse()

    source = src_objs + sorted_libs
    return (target, source)


def generate(env):
    env['PROGEMITTER'] = prog_emitter


def exists(env):
    return True
