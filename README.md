# ipmctl

ipmctl is a utility for configuring and managing Intel Optane DC persistent memory modules (PMM).

It supports functionality to:
* Discover PMMs on the platform.
* Provision the platform memory configuration.
* View and update the firmware on PMMs.
* Configure data-at-rest security on PMMs.
* Monitor PMM health.
* Track performance of PMMs.
* Debug and troubleshoot PMMs.

ipmctl refers to the following interface components:

* libipmctl: An Application Programming Interface (API) library for managing PMMs.
* ipmctl: A Command Line Interface (CLI) application for configuring and managing PMMs from the command line.
* ipmctl-monitor: A monitor daemon/system service for monitoring the health and status of PMMs.

Packages are available on Fedora.

Fedora and EPEL 7 packages can be found at: https://copr.fedorainfracloud.org/coprs/jhli/ipmctl

OpenSUSE and SLES packages can be found at: https://build.opensuse.org/package/show/home:jhli/ipmctl

Ubuntu packages can be found at: https://launchpad.net/~jhli/+archive/ubuntu/ipmctl


## Build

### Linux

The lastest Linux kernel version available is suggested.

libndctl is required to build, packages can be found at: https://copr.fedoraproject.org/coprs/djbw/ndctl/

The source can be found at: https://github.com/pmem/ndctl

All other dependencies are widely available.

```
mkdir output && cd output
cmake -DRELEASE=ON -DCMAKE_INSTALL_PREFIX=/ ..
make -j all
sudo make install
```
build artifacts can be found in output/release

To build RPMs:

```
./rpmbuild.sh xx.xx.xx.xxxx
```

The RPMs will be in output/rpmbuild/RPMS/

## Build Windows

Install VS2017
Open as a CMake project
