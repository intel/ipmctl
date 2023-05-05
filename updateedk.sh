#!/usr/bin/env bash
set -euo pipefail
IFS=$'\n\t'

EDK2_DIR="../edk2"
EDK2_COMPONENT_DIR=("BaseTools" "MdeModulePkg" "MdePkg" "ShellPkg")
EDK2_BATCH="edksetup.bat"

if [ ! -d ${EDK2_DIR} ]; then
	echo "Nothing to do."
	exit 0
fi

for i in ${EDK2_COMPONENT_DIR[@]}; do
	if [ ! -d ${EDK2_DIR}/${i} ]; then
		echo "${i} folder does not exist"
		exit -1
	else
		echo "${i} folder exists"
	fi
done

if [ -f ${EDK2_DIR}/${EDK2_BATCH} ]; then
	echo "${EDK2_BATCH} exists"
else
	echo "${EDK2_BATCH} does not exist"
	exit -1
fi

# Replacing folder
for i in ${EDK2_COMPONENT_DIR[@]}; do
	if [ -d ${i} ]; then
		echo "Deleting ${i} folder"
		rm -rf ${i}
	fi
	echo "Copying ${i} folder"
	cp -pr ${EDK2_DIR}/${i} .
done

if [ -f ${EDK2_BATCH} ]; then
	rm -f ${EDK2_BATCH}
	echo "Copying ${EDK2_BATCH}"
	cp -pr ${EDK2_DIR}/${EDK2_BATCH}
fi

# Apply patches
if [ -d patches ]; then
	set +e
	for i in $(ls patches); do
		echo "Applying patch ${i}"
		git apply patches/$i
	done
	set -e
fi
