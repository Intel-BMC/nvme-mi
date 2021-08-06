project('mctpwplus', 'cpp',
  meson_version : '>=0.56.0',
)

output=run_command('pwd')
message(output.stdout())
boost = dependency('boost', required: false, version : '>=1.71', modules : ['coroutine', 'context'])
if not boost.found()
     boost_proj = subproject('boost_1_71_0', required: true)
     boost = boost_proj.get_variable('boost_dep')
endif

sdbusplus = dependency('sdbusplus', required: false)
if not sdbusplus.found()
    sdbusplus_proj = subproject('sdbusplus', required: true)
    sdbusplus = sdbusplus_proj.get_variable('sdbusplus_dep')
endif

cpp = meson.get_compiler('cpp')
if cpp.has_header('phosphor-logging/log.hpp')
    phosphorlog_dep = declare_dependency()
else
    subproject('phosphor-logging', required: true)
    phosphorlog_dep = declare_dependency(
        include_directories: '../phosphor-logging'
    )
endif

systemd = dependency('systemd')
systemd_system_unit_dir = systemd.get_pkgconfig_variable(
    'systemdsystemunitdir',
    define_variable: ['prefix', get_option('prefix')])

threads = dependency('threads')

subdir = 'mctpwplus'
lib_src_files = ['dbus_cb.cpp', 'mctp_wrapper.cpp', 'mctp_impl.cpp', 'service_monitor.cpp']
src_files = []
foreach file : lib_src_files
  src_files += subdir + '/' + file
endforeach

deps = [
	boost,
	systemd,
	sdbusplus,
	phosphorlog_dep,
	threads
]
message(src_files)

wrapperlib = library('mctpwplus', src_files, install:true, dependencies : deps)
wrapperlib_dep = declare_dependency(include_directories:subdir,
    link_with: wrapperlib
)
