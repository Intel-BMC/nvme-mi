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

boost = dependency('boost', required: dep_required, version : '>=1.71',
        modules: [ 'coroutine', 'context'])
if not boost.found()
     boost_proj = subproject('boost_1_71_0', required: true)
     boost = boost_proj.get_variable('boost_dep')
endif

sdbusplus = dependency('sdbusplus', required: dep_required)
if not sdbusplus.found()
    sdbusplus_proj = subproject('sdbusplus', required: true)
    sdbusplus = sdbusplus_proj.get_variable('sdbusplus_dep')
endif
sdbusplus_dep = sdbusplus.partial_dependency(link_args: true,
     includes : true, links: true)

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

if cpp.has_header('nlohmann/json.hpp')
    nlohmann_json = declare_dependency()
    message('nlohmann json already found')
else
    nlohmann_json = dependency('', required: yocto_build.enabled())
    subproject('nlohmann', required: true)
    nlohmann_json = declare_dependency(
      include_directories: [
          'subprojects/nlohmann/single_include',
          'subprojects/nlohmann/single_include/nlohmann'])
endif

configure_file(
  copy: true,
  input: 'service_files/xyz.openbmc_project.nvme-mi.service',
  install: true,
  install_dir: systemd_system_unit_dir,
  output: 'xyz.openbmc_project.nvme-mi.service',
)

threads = dependency('threads')

cmake = import('cmake')

mctpwrapper_dep = dependency('mctpwplus', required: dep_required,
    allow_fallback: false)
if not mctpwrapper_dep.found()
     mctpwrapper_proj = subproject('mctpwplus', required: true)
     mctpwrapper_dep = mctpwrapper_proj.get_variable('wrapperlib_dep')
endif

deps = [
    boost,
    sdbusplus_dep,
    phosphorlog_dep,
    threads,
    mctpwrapper_dep,
    nlohmann_json
]

src_files = ['main.cpp', 'drive.cpp', 'numeric_sensor.cpp',
             'threshold_helper.cpp', 'protocol/linux/crc32c.cpp']

exe_options = ['warning_level=3']
if yocto_build.enabled()
    exe_options = ['warning_level=3', 'werror=true']
endif

executable(
      'nvme-mi',
      src_files,
      implicit_include_directories: false,
      dependencies: deps,
      install: true,
      install_dir: get_option('bindir'),
      override_options : exe_options)

if build_tests.enabled()
    gtest_dep = dependency('gtest', required:dep_required)
    if not gtest_dep.found()
        gtest_subproject = cmake.subproject('gtest')
        cm_gtest = gtest_subproject.dependency('gtest')
        cm_gtest_main = gtest_subproject.dependency('gtest_main')
        gtest_dep = declare_dependency(dependencies: [cm_gtest,
            cm_gtest_main, threads])
    endif

    test_protocol_src = ['tests/test_protocol.cpp', 'protocol/linux/crc32c.cpp']
    test_protocol_dep = [gtest_dep]
    test_protocol = executable('test_protocol', test_protocol_src,
         dependencies:test_protocol_dep)
    test('Protocol-formatting', test_protocol, is_parallel : false)

    test_createdrive_src = ['tests/test_create_drive.cpp',
        'tests/mctp_wrapper.cpp', 'drive.cpp', 'protocol/linux/crc32c.cpp',
        'numeric_sensor.cpp', 'threshold_helper.cpp']
    mctpwrapper_mock_dep = dependency('mctpwplus', required: dep_required,
        allow_fallback: false)
    if not mctpwrapper_mock_dep.found()
        mctpwrapper_mock_proj = subproject('mctpwplus', required: true)
        mctpwrapper_mock_dep = declare_dependency(
            include_directories:'subprojects/mctpwplus/mctpwplus')
    endif
    test_createdrive_dep = [gtest_dep, boost, systemd, sdbusplus,
        phosphorlog_dep, threads, mctpwrapper_mock_dep, nlohmann_json]
    test_createdrive = executable('test_createdrive', test_createdrive_src,
        dependencies:test_createdrive_dep)
    test('Create drive test', test_createdrive, is_parallel : false)

    test_threshold_src = ['tests/test_threshold.cpp',
        'tests/mctp_wrapper.cpp', 'drive.cpp', 'protocol/linux/crc32c.cpp',
        'numeric_sensor.cpp', 'threshold_helper.cpp']
    test_threshold_dep = [gtest_dep, boost, systemd, sdbusplus,
        phosphorlog_dep, threads, mctpwrapper_mock_dep, nlohmann_json]
    test_threshold = executable('test_threshold', test_threshold_src,
        dependencies:test_threshold_dep)
    test('Threshold test', test_threshold, is_parallel : false)
    
    test_collectlog_src = ['tests/test_collectlog.cpp',
        'tests/mctp_wrapper.cpp', 'drive.cpp', 'protocol/linux/crc32c.cpp',
        'numeric_sensor.cpp', 'threshold_helper.cpp']
    test_collectlog_dep = [gtest_dep, boost, systemd, sdbusplus,
        phosphorlog_dep, threads, mctpwrapper_mock_dep, nlohmann_json]
    test_collectlog = executable('test_collectlog', test_collectlog_src,
        dependencies:test_collectlog_dep)
    test('Collect log test', test_collectlog, is_parallel : false)

endif
