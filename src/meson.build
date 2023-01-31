libbej_lib = library(
  'libbej',
  'bej_decoder_core.c',
  'bej_common.c',
  'bej_dictionary.c',
  'bej_tree.c',
  'bej_decoder_json.cpp',
  include_directories : libbej_incs,
  implicit_include_directories: false,
  version: meson.project_version(),
  install: true,
  install_dir: get_option('libdir')
)

libbej = declare_dependency(
  include_directories: libbej_incs,
  link_with: libbej_lib
)

import('pkgconfig').generate(
  libbej_lib,
  name: 'libbej',
  version: meson.project_version(),
  description: 'libbej')
