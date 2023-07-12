project(
  'libbej',
  'c',
  'cpp',
  version: '0.1',
  meson_version: '>=1.1.1',
  default_options: [
    'c_std=c18',
    'cpp_std=c++23',
    'werror=true',
    'warning_level=3',
    'tests=' + (meson.is_subproject() ? 'disabled' : 'auto'),
  ])

libbej_incs = include_directories('include', 'include/libbej')
subdir('src')
subdir('include/libbej')
if not get_option('tests').disabled()
  subdir('test')
endif

