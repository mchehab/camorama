glib_dep = dependency('glib-2.0')

libcamera_backend = static_library(
  'camorama-libcamera',
  'libcamera.cpp',
  include_directories: include_directories('..', '../src'),
  dependencies: [libcamera_dep, glib_dep, gtk_dep],
  cpp_args: ['-DHAVE_CONFIG_H'],
  override_options: ['cpp_std=c++20'],
  install: false,
)

libcamera_backend_dep = declare_dependency(
  link_with: libcamera_backend,
  include_directories: include_directories('.'),
  dependencies: [libcamera_dep, glib_dep],
)
meson.override_dependency('camorama-libcamera', libcamera_backend_dep)
