project('camorama', 'c', version: '0.21.2',
        default_options: ['c_std=gnu11', 'cpp_std=c++17', 'warning_level=2'])

cc = meson.get_compiler('c')
gtk4 = get_option('gtk4')

if gtk4
  gtk_dep = dependency('gtk4', version: '>=4.0')
  camorama_ui = 'camorama-gtk4.ui'
else
  gtk_dep = dependency('gtk+-3.0', version: '>=3.22')
  camorama_ui = 'camorama-gtk3.ui'
endif

dependencies = [
  dependency('gdk-pixbuf-2.0'),
  dependency('libv4l2'),
  gtk_dep,
  cc.find_library('m'),
]

libcamera_dep = dependency('libcamera', required: get_option('libcamera'))
if libcamera_dep.found()
  add_languages('cpp', required: true, native: false)
endif

ffmpeg_deps = [
  dependency('libavcodec', required: get_option('ffmpeg')),
  dependency('libavutil', required: get_option('ffmpeg')),
  dependency('libswscale', required: get_option('ffmpeg')),
]
ffmpeg_found = true
foreach dep : ffmpeg_deps
  ffmpeg_found = ffmpeg_found and dep.found()
endforeach
if ffmpeg_found
  dependencies += ffmpeg_deps
endif

conf = configuration_data()
conf.set('HAVE_CONFIG_H', 1)
conf.set('HAVE_LIBCAMERA', libcamera_dep.found())
conf.set('HAVE_FFMPEG', ffmpeg_found)
conf.set_quoted('PACKAGE', meson.project_name())
conf.set_quoted('PACKAGE_NAME', meson.project_name())
conf.set_quoted('PACKAGE_VERSION', meson.project_version())
conf.set_quoted('PACKAGE_DATA_DIR', join_paths(get_option('prefix'),
                                                get_option('datadir')))
conf.set_quoted('PACKAGE_LOCALE_DIR', join_paths(get_option('prefix'),
                                                  get_option('localedir')))
conf.set_quoted('GETTEXT_PACKAGE', meson.project_name())
configure_file(output: 'config.h', configuration: conf)

inc = include_directories('.', 'src')

sources = files(
  'src/camera-backend.c',
  'src/gtk_common_callbacks.c',
  'src/img_convert.c',
  'src/camorama-filter-chain.c',
  'src/camorama-filter-laplace.c',
  'src/camorama-filter-mirror.c',
  'src/camorama-filter-reichardt.c',
  'src/camorama-window.c',
  'src/fileio.c',
  'src/filter.c',
  'src/main.c',
  'src/support.c',
  'src/v4l.c',
)

if libcamera_dep.found()
  sources += files('src/libcamera.c', 'src/libcamera-bridge.cpp')
  dependencies += libcamera_dep
endif

if gtk4
  sources += files('src/gtk4_callbacks.c', 'src/gtk4-effects.c')
else
  sources += files('src/gtk3_callbacks.c', 'src/gtk3-effects.c')
endif

executable('camorama', sources,
           dependencies: dependencies,
           include_directories: inc,
           c_args: ['-DHAVE_CONFIG_H',
                    '-DCAMORAMA_UI="@0@"'.format(camorama_ui)],
           install: true)

i18n = import('i18n')
subdir('po')

subdir('data')

# Allow the program to be run directly from the build tree with
# `meson devenv -C <builddir>`.  GSettings data is generated in the build
# directory, while the UI and image files can be used from the source tree.
devenv = environment()
devenv.set('GSETTINGS_SCHEMA_DIR', join_paths(meson.current_build_dir(),
                                              'data'))
devenv.set('CAMORAMA_DATA_DIR', join_paths(meson.current_build_dir(),
                                           'data'))
devenv.set('CAMORAMA_ICON_FILE', join_paths(meson.current_source_dir(),
                                            'pixmaps',
                                            'camorama-128.png'))
meson.add_devenv(devenv)

desktop = i18n.merge_file(input: 'camorama.desktop.in',
                          output: 'camorama.desktop',
                          type: 'desktop',
                          po_dir: 'po',
                          install: true,
                          install_dir: join_paths(get_option('datadir'),
                                                  'applications'))
appdata = i18n.merge_file(input: 'camorama.appdata.xml.in',
                          output: 'camorama.appdata.xml',
                          type: 'xml',
                          po_dir: 'po',
                          install: true,
                          install_dir: join_paths(get_option('datadir'),
                                                  'metainfo'))

install_man('camorama.1')

install_data(files('pixmaps/camorama-16.png', 'pixmaps/camorama-24.png',
                   'pixmaps/camorama-32.png', 'pixmaps/camorama-48.png',
                   'pixmaps/camorama-64.png', 'pixmaps/camorama-128.png',
                   'pixmaps/camorama-256.png', 'pixmaps/camorama-512.png'),
             install_dir: join_paths(get_option('datadir'), 'pixmaps'))

foreach size : ['16', '24', '32', '48', '64', '128', '256', '512']
  install_data('pixmaps/camorama-' + size + '.png',
               rename: 'camorama.png',
               install_dir: join_paths(get_option('datadir'), 'icons',
                                       'hicolor', size + 'x' + size,
                                       'devices'))
endforeach

summary('GTK interface', gtk4 ? 'GTK 4' : 'GTK 3',
        section: 'Configuration')

summary({
  'libcamera': libcamera_dep.found(),
  'FFmpeg': ffmpeg_found,
}, section: 'Optional features', bool_yn: true)
