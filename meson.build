project('camorama', 'c', version: '0.21.2',
        default_options: ['c_std=gnu11', 'warning_level=2'])

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

conf = configuration_data()
conf.set('HAVE_CONFIG_H', 1)
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
  'src/callbacks.c',
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

executable('camorama', sources,
           dependencies: dependencies,
           include_directories: inc,
           c_args: ['-DHAVE_CONFIG_H',
                    '-DCAMORAMA_UI="@0@"'.format(camorama_ui)],
           install: true)

i18n = import('i18n')
subdir('po')

subdir('data')

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
