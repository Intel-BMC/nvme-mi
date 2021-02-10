project('Phosphor logging', 'cpp',
  meson_version : '>=0.56.0',
)

r=run_command('bootstrap.sh')
output = r.stdout().strip()
message(output)
r2=run_command('configure')
output = r2.stdout().strip()
message(output)
