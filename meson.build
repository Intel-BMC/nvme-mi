project(
    'nvme-mi',
    'cpp',
    license: 'Apache-2.0',
    version: '0.1',
	default_options: [
        'warning_level=0',
        'werror=false',
        'cpp_std=c++17'
    ],
)

build_tests = get_option('tests')
yocto_build = get_option('yocto_dep')

dep_required = false

if yocto_build.enabled()
    dep_required = true
endif

boost = dependency('boost', required: dep_required, version : '>=1.71', modules: [ 'coroutine', 'context'])
if not boost.found()
     subproject('boost_1_71_0', required: true)
     boost = declare_dependency(
         include_directories: 'subprojects/boost_1_71_0/install/include',
		 link_args:['-Lsubprojects/boost_1_71_0/install/lib', '-lboost_context', '-lboost_coroutine']
     )
endif

sdbusplus = dependency('sdbusplus', required: dep_required)
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
        include_directories: 'subprojects/phosphor-logging'
    )
endif

systemd = dependency('systemd', required: dep_required)
systemd_system_unit_dir = systemd.get_pkgconfig_variable(
    'systemdsystemunitdir',
    define_variable: ['prefix', get_option('prefix')])

configure_file(
  copy: true,
  input: 'service_files/xyz.openbmc_project.nvme-mi.service',
  install: true,
  install_dir: systemd_system_unit_dir,
  output: 'xyz.openbmc_project.nvme-mi.service',
)

threads = dependency('threads')

cmake = import('cmake')

mctpwrapper_dep = dependency('mctpwplus', required: dep_required, allow_fallback: false)
if not mctpwrapper_dep.found()
     mctpwrapper_proj = subproject('mctpwplus', required: true)
     mctpwrapper_dep = mctpwrapper_proj.get_variable('wrapperlib_dep')
endif

deps = [
	boost,
	systemd,
	sdbusplus,
	phosphorlog_dep,
	threads,
	mctpwrapper_dep
]

src_files = ['main.cpp', 'drive.cpp']

if yocto_build.enabled()
	executable(
	  'nvme-mi',
	  src_files,
	  implicit_include_directories: false,
	  dependencies: deps,
	  install: true,
	  install_dir: get_option('bindir'),
	  override_options : ['warning_level=0', 'werror=true'])
else
	executable(
	  'nvme-mi',
	  src_files,
	  implicit_include_directories: false,
	  dependencies: deps,
	  install: true,
	  install_dir: get_option('bindir'),
	  override_options : ['warning_level=0', 'werror=false'])
endif
