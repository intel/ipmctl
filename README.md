# ipmctl

ipmctl is a utility for configuring and managing Intel Optane DC persistent memory modules (DCPMM).

It supports functionality to:
* Discover DCPMMs on the platform.
* Provision the platform memory configuration.
* View and update the firmware on DCPMMs.
* Configure data-at-rest security on DCPMMs.
* Track health and performance of DCPMMs.
* Debug and troubleshoot DCPMMs.

ipmctl refers to the following interface components:

* libipmctl: An Application Programming Interface (API) library for managing DCPMMs.
* ipmctl: A Command Line Interface (CLI) application for configuring and managing DCPMMs from the command line.

## Workarounds
When using 02.00.00.x versions of ipmctl software to update or downgrade firmware on Intel® Optane™ PMem 100 Series modules, please use the “-lpmb” CLI option (use DDRT Large Payload transfer). Otherwise the operation may take significantly longer than it normally would.

## Releases

01.00.00.xxxx (master_1_0 branch) targets first generation of hardware

02.00.00.xxxx  (master_2_0 branch) adds support for up coming hardware generations while maintaining support for previous generations as well as other enhancements

## Packages

Some distributions include ipmctl allowing installation via their package manager.
For example (on Fedora):
> dnf install ipmctl

This will update the required dependencies.

For systems that cannot reach the Internet use another system to download the following rpms required to install ipmctl and then copy them to the original system (e.g. via thumb drive).

> ipmctl.rpm, libipmctl.rpm, libsafec.rpm (only needed for 1.x releases), libndctl, json-c.rpm

Run

> rpm –ivh *.rpm


CentOS and RHEL systems maybe able to use an EPEL package found at: https://src.fedoraproject.org/rpms/ipmctl

OpenSUSE and SLES packages can be found at: https://build.opensuse.org/package/show/hardware:nvdimm/ipmctl

Ubuntu releases can be found at: https://launchpad.net/ubuntu/+source/ipmctl

### libndctl


ipmctl depends on libndctl (ndctl-libs).

It can be found here https://github.com/pmem/ndctl if not available as a package.


## Build

### Specific Instructions Reported as Working on SUSE for build in home directory

Replace homedir with the actual account

> git clone https://github.com/intel/ipmctl.git
>
> zypper in libndctl-devel ruby2.5-rubygem-asciidoctor
>
> cd ipmctl
>
> mkdir output
>
> cd output
>
> cmake -DRELEASE=ON -DSAFECLIB_SRC_DOWNLOAD_AND_STATIC_LINK=ON -DCMAKE_INSTALL_PREFIX=/home/homedir/ipmctl/ ..
>
> make all

### Specific Instructions Reported as Working on RHEL7.6, CentOS7.6 and Fedora 30.

Ipmctl has dependency on libsafec-devel (for 1.x builds only), libndctl-devel and rubygem-asciidoctor
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

libndctl-devel package is required.

All other dependencies are widely available.

```
mkdir output && cd output
cmake -DRELEASE=ON -DCMAKE_INSTALL_PREFIX=/usr ..
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

