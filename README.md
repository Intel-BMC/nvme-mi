# DISCONTINUATION OF PROJECT #  
This project will no longer be maintained by Intel.  
Intel has ceased development and contributions including, but not limited to, maintenance, bug fixes, new releases, or updates, to this project.  
Intel no longer accepts patches to this project.  
 If you have an ongoing need to use this project, are interested in independently developing it, or would like to maintain patches for the open source software community, please create your own fork of this project.  
# NVMe Management Interface over MCTP

The NVMe® Management Interface (NVMe-MI™) specification was created to
define a command set and architecture for managing NVMe storage, making
it possible to discover, monitor, configure, and update NVMe devices in
multiple operating environments. NVMe-MI technology provides an industry
standard for management of NVMe devices

[MCTP Base Specification](https://www.dmtf.org/sites/default/files/standards/documents/DSP0236_1.3.0.pdf)

[NVMe-MI 1.0a Specification](http://nvmexpress.org/wp-content/uploads/NVM_Express_Management_Interface_1_0a_2017.04.08_-_gold.pdf)

## Design

  A simple daemon that communicates with available NVMe drives through MCTP wrapper
library. On initialization the daemon will detect all available MCTP devices which
support NVMeMgmt message type and create dbus sensor objects for each. If any
drive comes up later MCTP wrapper library will provide callback and new drive
object will be created.

  Each drive will have a set of threshold interfaces associated with them. Threshold
will contain Critical and Warning alarms for both high and low value for
temperature reading.

  The application will periodically send NVM subsystem health status poll request to
all available NVMe drives and will parse temperature value from the response. The sensor
value will be updated on DBus and the value will be checked against thresholds.
NVMe MI daemon will provide a DBus method to dump output from NVMe MI commands

1. Read NVMe MI data structure
2. Controller health status poll
3. Subsystem health status poll
4. Configuration get
5. Get features
6. Get log page
7. Identify
Output will be in JSON format.

### Example
<pre>
{
    "GetConfiguration": {
        "PortId1": {
            "Frequency": "100 KHZ",
            "MCTP_Transmission_Unit_Size": 64
        },
        "PortId2": {
            "Frequency": "100 KHZ",
            "MCTP_Transmission_Unit_Size": 64
        }
    },
    "ControllerHealthStatus": [
        {
            "Dump": "Hexstring",
            "ID": 1234
        },
        {
            "Dump": "Hexstring",
            "ID": 1235
        }
    ],
    "ControllerIds": [
        0,
        1,
        2
    ],
    "Controllers": {
        "ControllerId1": "Hexstring",
        "ControllerId2": "Hexstring",
        "ControllerId3": "Hexstring"
    },
    "GetFeatures": {
        "Arbitration": "HexString",
        "Asynchronous Event Configuration": "HexString",
        "Autonomous Power State Transition": "HexString",
        "Endurance Group Event Configuration": "HexString",
        "Error Recovery": "HexString",
        "Host Behavior Support": "HexString",
        "Host Controlled Thermal Management": "HexString",
        "Host Memory Buffer": "HexString",
        "Interrupt Coalescing": "HexString",
        "Interrupt Vector Configurationd": "HexString",
        "Keep Alive Timer": "HexString",
        "LBA Range Type": "HexString",
        "LBA Status Information Report Interval": "HexString",
        "Non-Operational Power State Config": "HexString",
        "Number of Queues": "HexString",
        "Power Management": "HexString",
        "Predictable Latency Mode Config": "HexString",
        "Predictable Latency Mode Window": "HexString",
        "Read Recovery Level Config": "HexString",
        "Sanitize Config": "HexString",
        "Temperature Threshold": "HexString",
        "Timestamp": "HexString",
        "Volatile Write Cache": "HexString",
        "Write Atomicity": "HexString"
    },
    "GetLogPage": {
        "Changed Namespace List": "HexString",
        "Commands Supported and Effects": "HexString",
        "Controller Asymmetric Namespace Access": "HexString",
        "Controller LBA Status Information": "HexString",
        "Controller Telemetry Controller-Initiated": "HexString",
        "Controller Telemetry Host-Initiated": "HexString",
        "Device Self-test": "HexString",
        "Firmware Slot Information": "HexString",
        "NVM subsystem Endurance Group Event Aggregate": "HexString",
        "NVM subsystem Endurance Group Information": "HexString",
        "NVM subsystem Persistent Event Log": "HexString",
        "NVM subsystem Predictable Latency Event Aggregate": "HexString",
        "NVM subsystem Predictable Latency Per NVM Set": "HexString",
        "SMART / Health Information": "HexString"
    },
    "Identify": {
        "Active Namespace": "HexString",
        "Controllers": {
            "Controller1": "HexString",
            "Controller2": "HexString"
        },
        "Identify Common Namespace": "HexString",
        "Namespace Identification Descriptors": {
            "NSID1": "HexString",
            "NSID2": "HexString"
        }
    },
    "NVM_Subsystem_Info": {
        "Major": 1,
        "Minor": 0,
        "Ports": 3
    },
    "OptionalCommands": [
        {
            "OpCode": 9,
            "Type": 16
        },
        {
            "OpCode": 16,
            "Type": 16
        },
        {
            "OpCode": 17,
            "Type": 16
        }
    ],
    "Ports": {
        "Port1":"HexString",
        "Port2":"HexString"
    },
    "SubsystemHealthStatusPoll": "Hexstring"
}
</pre>
## Architecture
<pre>
┌────────────────────┐                 ┌────────┐
│                    │                 │        │     │ │
│ NVMe MI Daemon     │                 │        │
│                    │                 │   M    │     │ │          ┌────────┐
├────────────────────┤                 │   C    │      T           │        │
│                    │                 │   T    │     │R│◄────────►│ Drive1 │
│                    │                 │   P    │      A           │        │
│                    │                 │        │     │N│          └────────┘
│                    │  Detect drives  │   W    │      S
│                    │◄───────────────►│   R    │◄───►│P│
│                    │                 │   A    │      O
│                    │                 │   P    │     │R│
│                    │                 │   P    │      T
│                    │                 │   E    │     │ │          ┌────────┐
│                    │  Subsystem      │   R    │                  │        │
│                    │  HS Poll        │        │     │ │◄────────►│Drive2  │
│                    │◄───────────────►│        │                  │        │
│                    │                 │        │     │ │          └────────┘
│                    │                 │        │
│                    │                 │        │     │ │
└────────────────────┘                 └────────┘
</pre>

## Building
NVMe MI daemon uses meson as build system.
### Prerequisites
* Tested only on Ubuntu 18.04
* Compiler toolset needs to be installed
* Depends on libraries. Needs to be installed manually
    * libsystemd
* Libraries included in meson script
    * boost
    * gtest
    * mctpwrapper
    * nlohmann json
    * phosphor logging
    * sdbusplus

### Steps
```
meson setup build -Dtests=enabled
```
-Dtests option is to enable unit tests. It is not
mandatory. This step will create a folder named build and
meson will populate the build files inside it.
```
meson compile -C build
```
This step will compile the source using g++. nvme-mi binary
will be produced in the build folder. The environment
variable LD_LIBRARY_PATH should be pointed to boost
subproject $(pwd)/libs/boost/lib/ if boost is downloaded in
the meson configure step.
```
 meson test -C build
```
Run this step if unit testing is to be performed.

## Integrating the code

This particular repo depends on the following:

1. **mctpwplus** : NVMe MI protocol specification is on top of MCTP. This
application uses MCTP functions like
   * Detect all available MCTP endpoints in the system
   * Send and receive NVMe packets over MCTP
   * Get notified when a new MCTP device appears etc.
 mctpwplus provides all these abstraction inside a C++ library.

   This can be picked up from : [Intel-BMC nvme mi](https://github.com/Intel-BMC/nvme-mi)

2. **mctp layer** : NVMe application expects an independent mctpd layer to
exist in the system with which the mctpwplus library will interact for mctp
communication. At the time of this writing Current mctpwplus implmentation
talks to mctp layer through DBus. It may be different in different
implementations. NVMe application expects mctp layer to be abstraced by
mctpwplus library and send recive functionalities will be same even if
underlying mctp implementation changes. Example of mctpd implementation can
be found in : [Intel-BMC mctpd](https://github.com/Intel-BMC/pmci)

3. **OpenBMC dependencies** : This repo aligns with concepts used in
OpenBMC project (D-Bus based IPC mechanisms, usage of boost library for 
async operations, etc.). These dependencies are required for the code to
build.

To integrate the features into your BMC, the following recipes can be
used as a reference in your Yocto build. It includes recipes for mctpd
mctpwplus as well as nvme mi.
[PMCI recipes](https://github.com/Intel-BMC/openbmc/tree/intel/meta-openbmc-mods/meta-common/recipes-phosphor/pmci)