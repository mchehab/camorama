unittest_sources = ['main.c']

tests_c_flags = []

test_deps = []

cmocka_dep = dependency('cmocka', version: '>= 2.0', required: false)
if cmocka_dep.found()
    tests_c_flags += '-DCMOCKA_VERSION_2'
else
    cmocka_dep = dependency('cmocka', required: false)
endif

test_deps += cmocka_dep

base_name = 'color_bars'
color_bars_png    = 'smpte_color_bars.png'

formats = {
    'RGB24':   ['rgb24',     ''],
    'BGR24':   ['bgr24',     ''],
}

if enable_ffmpeg
    ffmpeg = find_program('ffmpeg')

    unittest_sources += 'test_img_convert.c'

    ffmpeg_fixtures = []
    foreach name, spec : formats
        pix_fmt = spec[0]

        cmd = [ffmpeg, '-loglevel', '0', '-y', '-i', '@INPUT@', '-pix_fmt', pix_fmt]

        if spec[1] != ''
            extra = spec[1].split(' ')
            cmd += extra
        endif

        cmd += ['-f', 'rawvideo', '@OUTPUT@']

        ffmpeg_fixtures += custom_target(
            'raw_' + name,
            input  : color_bars_png,
            output : base_name + '_' + name + '.raw',
            command: cmd,
            build_by_default: true,
        )
    endforeach
endif

if cmocka_dep.found()
    have_unittest = true

    unittest_exe = executable(
        'unittest', unittest_sources,
        link_with: [libcamorama],
        include_directories: inc,
        dependencies: dependencies + test_deps,
         c_args: ['-g'] + c_flags + tests_c_flags,
    )

#    foreach test_group : hermetic_test_groups
#        test(test_group, unittest_exe, args: ['--group', test_group])
#    endforeach

endif
