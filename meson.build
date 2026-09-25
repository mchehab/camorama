project('camorama', 'c', version: '0.21.2',
        default_options: ['c_std=gnu11', 'warning_level=2'])

cc = meson.get_compiler('c')

sources = [
  'src/camera-backend.c',
  'src/gtk_common_callbacks.c',
  'src/img_convert.c',
  'src/camorama-filter-chain.c',
  'src/camorama-filter-laplace.c',
  'src/camorama-filter-mirror.c',
  'src/camorama-filter-reichardt.c',
  'src/camorama-window.c',
  'src/img_bayer.c',
  'src/fileio.c',
  'src/filter.c',
  'src/media-devices.c',
  'src/support.c',
  'src/streaming.c',
  'src/v4l.c',
]

gtk4 = get_option('gtk4')
if gtk4
  gtk_dep = dependency('gtk4', version: '>=4.0')
  camorama_ui = 'camorama-gtk4.ui'
else
  gtk_dep = dependency('gtk+-3.0', version: '>=3.22')
  camorama_ui = 'camorama-gtk3.ui'
endif

libv4l2_dep = dependency('libv4l2', required: get_option('libv4l2'))

dependencies = [
  dependency('gdk-pixbuf-2.0'),
  gtk_dep,
  libv4l2_dep,
  cc.find_library('m'),
]

libpulse_dep = dependency('libpulse', required: get_option('audio'))
libpulse_simple_dep = dependency('libpulse-simple', required: get_option('audio'))

if libpulse_dep.found() and libpulse_simple_dep.found()
    dependencies += libpulse_dep
    dependencies += libpulse_simple_dep

    sources += 'src/audio.c'

    enable_audio = true
else
    enable_audio = false
endif

ffmpeg_deps = [
  dependency('libavcodec', required: get_option('ffmpeg')),
  dependency('libavutil', required: get_option('ffmpeg')),
  dependency('libswscale', required: get_option('ffmpeg')),
]

enable_ffmpeg = true
foreach dep : ffmpeg_deps
    enable_ffmpeg = enable_ffmpeg and dep.found()
endforeach

if enable_ffmpeg
    dependencies += ffmpeg_deps
    sources += 'src/img_ffmpeg.c'

    # ffmeg binary is needed for img_convert unittests
    ffmpeg = find_program('ffmpeg')
    png_file = 'smpte_color_bars.png'
endif

conf = configuration_data()
conf.set('HAVE_CONFIG_H', 1)
conf.set('HAVE_AUDIO', enable_audio)
conf.set('HAVE_LIBV4L2', libv4l2_dep.found())
conf.set('HAVE_FFMPEG', enable_ffmpeg)
conf.set_quoted('FFMPEG_BIN', ffmpeg.full_path())
conf.set_quoted('PNG_FILE', png_file)
conf.set_quoted('PACKAGE', meson.project_name())
conf.set_quoted('PACKAGE_NAME', meson.project_name())
conf.set_quoted('PACKAGE_VERSION', meson.project_version())
conf.set_quoted('PACKAGE_DATA_DIR', join_paths(get_option('prefix'),
                                                get_option('datadir')))
conf.set_quoted('PACKAGE_LOCALE_DIR', join_paths(get_option('prefix'),
                                                  get_option('localedir')))
conf.set_quoted('GETTEXT_PACKAGE', meson.project_name())
configure_file(output: 'config.h', configuration: conf)

# Directories to be used by meson devenv. Overrides PACKAGE_DATA_DIR if set
devenv = environment()
devenv.set('GSETTINGS_SCHEMA_DIR', join_paths(meson.current_build_dir(),
                                              'data'))
devenv.set('CAMORAMA_DATA_DIR', join_paths(meson.current_build_dir(),
                                           'data'))
devenv.set('CAMORAMA_ICON_FILE', join_paths(meson.current_source_dir(),
                                            'pixmaps',
                                            'camorama-128.png'))
meson.add_devenv(devenv)

inc = include_directories('.', 'src')

if gtk4
  gnome = import('gnome')
  sources += gnome.compile_resources(
    'camorama-icons',
    'data/camorama-icons.gresource.xml',
    source_dir: meson.current_source_dir(),
    c_name: 'camorama_icons',
  )
  sources += 'src/gtk4_callbacks.c'
else
  sources += 'src/gtk3_callbacks.c'
endif

#
# Static library
#

c_flags = [
    '-DHAVE_CONFIG_H',
    '-DCAMORAMA_UI="@0@"'.format(camorama_ui),
]

libcamorama = static_library(
    'camorama',
    sources,
    dependencies: dependencies,
    c_args: c_flags,
)

executable('camorama', 'src/main.c',
           dependencies: dependencies,
           link_whole: [libcamorama],
           include_directories: inc,
           c_args: c_flags,
           install: true)

i18n = import('i18n')
subdir('po')
subdir('data')
subdir('pixmaps')
subdir('tests')

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

summary(
    {
        'GTK interface': gtk4 ? 'GTK 4' : 'GTK 3',
        'Audio routing': enable_audio,
        'libv4l2': libv4l2_dep.found(),
        'FFmpeg': enable_ffmpeg,
    },
    bool_yn: true,
    section: 'Configuration'
)
