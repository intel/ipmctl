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

## Packages

ipmctl is availible on Fedora.

EPEL 7 packages can be found at: https://copr.fedorainfracloud.org/coprs/jhli/ipmctl

OpenSUSE and SLES packages can be found at: https://build.opensuse.org/package/show/home:jhli/ipmctl

Ubuntu packages can be found at: https://launchpad.net/~jhli/+archive/ubuntu/ipmctl

### libsafec


ipmctl requires libsafec as a dependency.


libsafec is availible on Fedora.


EPEL 7 packages can be found at: https://copr.fedorainfracloud.org/coprs/jhli/safeclib/


OpenSUSE and SLES packages can be found at: https://build.opensuse.org/package/show/home:jhli/safeclib


Ubuntu packages can be found at: https://launchpad.net/~jhli/+archive/ubuntu/libsafec

Alternately, -DSAFECLIB_SRC_DOWNLOAD_AND_STATIC_LINK=ON to download sources and statically link to safeclib

## Build

### Linux

The latest stable Linux kernel version available is recommended.

libsafec-devel is required to build, see above for package location.

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

### Windows

Install the Windows RS5 (Build >17650) Driver Development Kit. See https://www.microsoft.com/en-us/software-download/windowsinsiderpreviewSDK for access if it's not externally available at https://docs.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk. Make sure that winioctl.h is available after installation.

Install VS2017

Open as a CMake project
