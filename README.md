# ipmctl

ipmctl is a utility for configuring and managing Intel&#174; Optane&#8482; Persistent Memory modules (PMem).

<a href="https://repology.org/project/ipmctl/versions">
    <img src="https://repology.org/badge/vertical-allrepos/ipmctl.svg" alt="Packaging status" align="right">
</a>

It supports functionality to:
* Discover PMems on the platform.
* Provision the platform memory configuration.
* View and update the firmware on PMems.
* Configure data-at-rest security on PMems.
* Track health and performance of PMems.
* Debug and troubleshoot PMems.

ipmctl refers to the following interface components:

* libipmctl: An Application Programming Interface (API) library for managing PMems.
* ipmctl: A Command Line Interface (CLI) application for configuring and managing PMems from the command line.

Also, metrics exporter for [Prometheus](https://prometheus.io/docs/introduction/overview/) based on libipmctl was provided. For more details take a look [here](https://github.com/intel/ipmctl-exporter)

## Workarounds

### Slow Firmware Updates
When using 02.00.00.x versions of ipmctl software to update or downgrade firmware on Intel® Optane™ PMem 100 Series modules, please use the “-lpmb” CLI option (use DDRT Large Payload transfer). Otherwise the operation may take significantly longer than it normally would.

### Commands Fail on Older Platforms
Some platforms that targeted the Gen 100 modules do not generate a ACPI PMTT table which causes ipmctl (version v02.00.00.xxxx) commands to fail. Particularly
* create -goal 
* show -topology
* show -memoryresources
* show -dimm

If these commands are ran with -v option they present a message about failing to get the PMTT table.

A corrected version is being developed and will hopefully be available soon. Until that is available the best option is to use ipmctl v01.00.00.xxxx or go through the BIOS menus.

## Releases

01.00.00.xxxx  (master_1_0 branch) is for Intel Optane Persistent Memory 100 Series

02.00.00.xxxx  (master_2_0 branch) is for Intel Optane Persistent Memory 200 Series (and is backwards compatible with 100 series) 

03.00.00.xxxx  (master_3_0 branch) is for Intel Optane Persistent Memory 300 Series (and is backwards compatible with both the 100 and 200 series

**Note**: Branches may differ fundamentally. Please pay close attention to README.md of the respective branch.

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

**Note**: Each branch may require different building procedures. Please follow README.md of the respective branch.

### building latest (03.00.00.xxxx) on Linux
1. clone the ipmctl and edk2 repositories:

`git clone -b development https://github.com/intel/ipmctl.git`

`git clone https://github.com/tianocore/edk2.git`

`cd ipmctl`

2. Give execution permissions to the .sh files:

`chmod +x *.sh`

4. Run updateedk.sh, this will copy relevant folders from edk2 into ipmctl

`./updateedk.sh`

5. Build the ipmctl rpms specifying the version number to use

`./rpmbuild.sh 03.00.00.1234`

### building latest (03.00.00.xxxx) on Windows

Install Visual Studio 2017 (or newer). Be sure to install optional component:
* Workloads -> Desktop Development with C++
* Individual Components -> Compilers, build tools, and runtimes -> Visual C++ tools for CMake

clone the ipmctl project

Clone the edk2 repository and copy the directories BaseTools, MdeModulePkg, MdePkg and ShellPkg into the clone of the ipmctl project

Open CMakeLists.txt as a CMake project in Visual Studio. See: https://docs.microsoft.com/en-us/cpp/build/cmake-projects-in-visual-studio

### Specific Instructions Reported as Working for 02.00.00.xxxx versions on SUSE for build in home directory

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

### Specific Instructions Reported as Working for 02.00.00.xxxx versions on RHEL7.6, CentOS7.6 and Fedora 30.

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



