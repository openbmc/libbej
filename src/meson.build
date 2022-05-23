libbej_lib = static_library(
  'libbej',
  'bej_decoder_core.c',
  'rde_common.c',
  'bej_dictionary.c',
  'bej_decoder_json.cpp',
  include_directories : libbej_incs,
  implicit_include_directories: false
)

libbej = declare_dependency(
  include_directories: libbej_incs,
  link_with: libbej_lib
)
