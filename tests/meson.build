unittest_sources = ['main.c']
tests_c_flags = []
test_deps = []
enable_unittest = false

python3 = import('python').find_installation('python3', modules: ['polib'])
run_target(
    'translation-report',
    command: [python3, files('check_translations.py')[0]],
)

unittest_sources += ['test_v4l.c', 'mock_v4l.c']
unittest_sources += ['test_streaming.c']
unittest_sources += ['test_filter_chain.c', 'test_image_bounds.c']
enable_unittest = true

if enable_ffmpeg and ffmpeg.found()
    configure_file(input: png_file, output: png_file, copy: true)

    unittest_sources += ['test_utils.c', 'test_img_convert.c',
                         'test_img_streaming.c']
    test_deps += dependency('libpng', required: true)
    enable_unittest = true
endif

# Only enable unit tests if one or more tests is supported
if enable_unittest
    cmocka_dep = dependency('cmocka', version: '>= 2.0', required: false)
    if cmocka_dep.found()
        tests_c_flags += '-DCMOCKA_VERSION_2'
    else
        cmocka_dep = dependency('cmocka', required: false)
    endif

    if cmocka_dep.found()
        test_deps += cmocka_dep

        unittest_exe = executable(
            'unittest', unittest_sources,
            link_with: [libcamorama],
            include_directories: inc,
            dependencies: dependencies + test_deps,
            c_args: ['-g'] + c_flags + tests_c_flags,
        )
    endif
endif
