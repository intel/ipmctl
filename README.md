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

Some distributions include ipmctl allowing installation via their package manager.
For example (on Fedora):
> dnf install ipmctl

This will update the required dependencies.

For systems that cannot reach the Internet use another system to download the following rpms required to install ipmctl and then copy them to the original system (e.g. via thumb drive).

> ipmctl.rpm, libipmctl.rpm, libsafec.rpm, ndctl, ndctl-libs, json-c.rpm

Run 

> rpm –ivh *.rpm


EPEL 7 packages can be found at: https://copr.fedorainfracloud.org/coprs/jhli/ipmctl

OpenSUSE and SLES packages can be found at: https://build.opensuse.org/package/show/home:jhli/ipmctl

### libsafec


ipmctl requires libsafec as a dependency.


libsafec is available on Fedora.


EPEL 7 packages can be found at: https://copr.fedorainfracloud.org/coprs/jhli/safeclib/


OpenSUSE and SLES packages can be found at: https://build.opensuse.org/package/show/home:jhli/safeclib


Ubuntu packages can be found at: https://launchpad.net/~jhli/+archive/ubuntu/libsafec

Alternately, -DSAFECLIB_SRC_DOWNLOAD_AND_STATIC_LINK=ON to download sources and statically link to safeclib

### libndctl


ipmctl depends on libndctl (ndctl-libs).

It can be found here https://github.com/pmem/ndctl if not available as a package.


## Build

### Specific Instructions Reported as Working on RHEL7.6, CentOS7.6 and Fedora 30.
 
Ipmctl has dependency on libsafec-devel, libndctl-devel and rubygem-asciidoctor
*	copr/jhli repo has libipmctl and its dependency, libsafec-devel.
    * cd /etc/yum.repos.d/
>       wget https://copr.fedorainfracloud.org/coprs/jhli/ipmctl/repo/epel-7/jhli-ipmctl-epel-7.repo
>       wget https://copr.fedorainfracloud.org/coprs/jhli/safeclib/repo/epel-7/jhli-safeclib-epel-7.repo
> 
>       * This should bring down both libipmctl and its dependency, libsafec-devel and 
    
*	epel repos has rubygem-asciidoctor
    *	get the epel repos
>      yum install https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm
*	Enable extra packages in RHEL for ndctl-devel or any other dependencies
    *	EPEL packages may depend on packages from these repositories:
>     # subscription-manager repos --enable "rhel-*-optional-rpms" --enable "rhel-*-extras-rpms"  --enable "rhel-ha-for-rhel-*-server-rpms"
*	Install the prerequisite packages
>	sudo yum install ndctl ndctl-libs ndctl-devel libsafec rubygem-asciidoctor
*	Either Follow ipmctl make, rpmbuild instructions, or install the ipmctl package

### Linux

The latest stable Linux kernel version available is recommended.

libsafec-devel is required to build, see above for package location.

ndctl package is required.

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

Install Visual Studio 2017 (or newer). Be sure to install optional component: 
* Workloads -> Desktop Development with C++
* Individual Components -> Compilers, build tools, and runtimes -> Visual C++ tools for CMake

Open as a CMake project. See: https://docs.microsoft.com/en-us/cpp/build/cmake-projects-in-visual-studio

