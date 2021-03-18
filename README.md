# ipmctl

This branch is for development work which is intended to be merged into the
main branch (likely as 3.0 version)

## Dependencies

* ndctl
* asciidoc or asciidoctor  (if building documentation)

## Building

1. clone the ipmctl and edk2 repositories:

`git clone https://github.com/intel/ipmctl.git`

`git clone https://github.com/tianocore/edk2.git`

`cd ipmctl`

2. Give execution permissions to the .sh files:

`chmod +x *.sh`

4. Run updateedk.sh, this will copy relevant folders from edk2 into ipmctl

`./updateedk.sh`

5. Build the ipmctl rpms specifying the version number to use

`./rpmbuild.sh 03.00.00.1234`
