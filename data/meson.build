schema = configure_file(input: 'org.gnome.camorama.gschema.xml.in',
                        output: 'org.gnome.camorama.gschema.xml',
                        configuration: conf,
                        install: true,
                        install_dir: join_paths(get_option('datadir'),
                                                'glib-2.0', 'schemas'))
glib_compile_schemas = find_program('glib-compile-schemas')
custom_target('gsettings-compile',
              input: schema,
              output: 'gschemas.compiled',
              command: [glib_compile_schemas, '--targetdir', '@OUTDIR@',
                        '@OUTDIR@'],
              build_by_default: true)

# Install UI enements in a way that meson devenv works
configure_file(input: camorama_ui, output: camorama_ui, copy: true)
foreach image : [
  'snap-icon.xpm',
  'snap-icon-small.xpm',
  'contrast.png',
  'zoom.png',
  'colourness.png',
  'brightness.png',
  'whiteness.png',
  'stock-tool-hue-saturation-16.png',
]
  configure_file(input: 'pixmaps/' + image, output: image, copy: true)
endforeach

install_data(camorama_ui,
             install_dir: join_paths(get_option('datadir'), 'camorama'))
install_data(
  'pixmaps/snap-icon.xpm',
  'pixmaps/snap-icon-small.xpm',
  'pixmaps/contrast.png',
  'pixmaps/zoom.png',
  'pixmaps/colourness.png',
  'pixmaps/brightness.png',
  'pixmaps/whiteness.png',
  'pixmaps/stock-tool-hue-saturation-16.png',
  install_dir: join_paths(get_option('datadir'), 'camorama'))
