project('Boost subproject', 'cpp',
  meson_version : '>=0.56.0',
)

r=run_command('bootstrap.sh', '--prefix=install')
output = r.stdout().strip()
message(output)
r2=run_command('b2', 'install')
output = r2.stdout().strip()
message(output)