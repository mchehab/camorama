# Note: Right now, we're adding camorama icon to both at hicolor and at
# pixmaps directories, to make it work on both devenv and install.

icon_pixmaps = []
foreach size : ['16', '24', '32', '48', '64', '128', '256', '512']
  pixmap_file = 'camorama-' + size + '.png'
  install_data(pixmap_file, rename: 'camorama.png',
               install_dir: join_paths(get_option('datadir'), 'icons',
                                       'hicolor', size + 'x' + size,
                                       'devices'))
  icon_pixmaps += pixmap_file
endforeach

install_data(icon_pixmaps,
             install_dir: join_paths(get_option('datadir'), 'pixmaps'))
