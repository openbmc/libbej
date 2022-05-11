project(
  'libbej',
  'c',
  'cpp',
  version: '0.1',
  meson_version: '>=0.57.0',
  default_options: [
    'c_std=c11',
    'cpp_std=c++20',
    'werror=true',
    'warning_level=3',
  ])

libbej_incs = include_directories('include')
subdir('src')
if not get_option('tests').disabled()
  subdir('test')
endif