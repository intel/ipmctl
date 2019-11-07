#!/bin/bash

MAINTAINER="$USER <$USER@$HOSTNAME>"
DATESTAMP=`date "+%a, %d %b %Y %H:%M:%S %z"`

if [ "$#" -le 0 ]; then
	echo "./debbuild.sh xx.xx.xx.xxxx purley | whitley"
	exit 3
fi

BUILDNUM=$1
SOURCEDIR=$PWD
ORIGDIR=$PWD
PLATFORM="purley"

if [ "$2" = "whitley" ]; then
	PLATFORM="whitley"
fi

# create tarball   Exclude file Makefile so cmake will be used as part of debuild
tar --exclude-vcs --exclude="Makefile" --exclude="*output" --exclude="*.swp*" --transform="s,^.,ipmctl-$BUILDNUM," -zcf $SOURCEDIR/ipmctl_$BUILDNUM.orig.tar.gz `ls -d ./*`

# untar to create working area
tar -xzf $SOURCEDIR/ipmctl_$BUILDNUM.orig.tar.gz

cd ipmctl-$BUILDNUM

SOURCEDIR=$PWD

# copy meta-data template files to expected location
cp -r install/linux/debian $SOURCEDIR/.

#update meta-data template files so they are actual meta-data files
sed -i "s/-DBUILDNUM=.*/-DBUILDNUM=$BUILDNUM/g" $SOURCEDIR/debian/rules
sed -i "s/Maintainer: .*/Maintainer: $MAINTAINER/g" $SOURCEDIR/debian/control
sed -i "s/Maintainer/$MAINTAINER/g ; s/^ipmctl (99.99.99.9999/ipmctl ($BUILDNUM/g ; s/Release v.*/Release v$BUILDNUM/g ; s/MainStamp/$DATESTAMP/g" $SOURCEDIR/debian/changelog

# Make output directories
mkdir -p $ORIGDIR/output/debbuild
chmod -x debian/*.install
# call utility that does the build -- unfortunately output is to .. (parent directory)
debuild --no-lintian -us -uc -sd

# copy .deb file to output directory
cp ../ipmctl_$BUILDNUM-1_amd64.deb $ORIGDIR/output/debbuild/.
cp ../ipmctl-dbgsym_$BUILDNUM-1_amd64.ddeb $ORIGDIR/output/debbuild/.
cp ../libipmctl-dev_$BUILDNUM-1_amd64.deb $ORIGDIR/output/debbuild/.
cp ../libipmctl2_$BUILDNUM-1_amd64.deb $ORIGDIR/output/debbuild/.
cp ../libipmctl2-dbgsym_$BUILDNUM-1_amd64.ddeb $ORIGDIR/output/debbuild/.

