project('sup2srt', 'c')

deps = [
  dependency('libpng'),
  dependency('libavutil', version: '>= 56.12.100'),
  dependency('libavcodec', version: '>= 58.16.100'),
  dependency('libavformat', version: '>= 58.9.100'),
  dependency('libswscale', version: '>= 5.0.101'),
  dependency('libavfilter', version: '>= 7.14.100'),
  dependency('libswresample', version: '>= 3.0.100'),
]

sources = [
 'src/ta/ta_utils.c',
 'src/ta/ta.c',
 'src/image.c',
 'src/demuxer.c',
]

executable('sup2srt', sources + ['src/main.c'], dependencies: deps)

test('demuxer', executable(
  'demuxer_test',
  sources + ['test/demuxer_test.c'],
  dependencies: deps))
