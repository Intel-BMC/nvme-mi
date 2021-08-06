project('Boost subproject', 'cpp',
  meson_version : '>=0.56.0',
)

fs = import('fs')
builddir = meson.current_build_dir()
installdir = builddir + '/install'
libexists = fs.exists(installdir + '/lib/libboost_context.so') and fs.exists(installdir + '/lib/libboost_coroutine.so')

if not libexists
  message('Boost libs not found. Downloading boost using wget')
  tar_download = run_command('wget', '-nc', '--no-check-certificate', 'https://boostorg.jfrog.io/artifactory/main/release/1.76.0/source/boost_1_76_0.tar.gz')
  message(tar_download.stdout().strip())
  assert(fs.exists('boost_1_76_0.tar.gz'), 'Boost download failed')
  extract_cmd = run_command('tar', '-xvf', 'boost_1_76_0.tar.gz', '--strip-components', '1')
  r=run_command('bootstrap.sh', '--prefix=' + installdir)
  output = r.stdout().strip()
  message(output)
  r2=run_command('b2', 'install')
  output = r2.stdout().strip()
  message(output)
endif

boost_dep = declare_dependency(
         compile_args: '-I' + installdir + '/include',
          link_args:[
         installdir + '/lib/libboost_context.a',
         installdir + '/lib/libboost_coroutine.a',
         installdir + '/lib/libboost_thread.a']
)