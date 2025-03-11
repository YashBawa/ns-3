# -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-

def build(bld):
    module = bld.create_ns3_module('dlarp', ['internet', 'wifi'])
    module.source = [
        'model/dlarp.cc',
        'helper/dlarp-helper.cc',
        ]

    headers = bld(features='ns3header')
    headers.module = 'dlarp'
    headers.source = [
        'model/dlarp.h',
        'helper/dlarp-helper.h',
        ]

    if bld.env.ENABLE_EXAMPLES:
        bld.recurse('examples')

    # bld.ns3_python_bindings()