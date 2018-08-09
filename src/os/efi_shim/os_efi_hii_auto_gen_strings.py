# Copyright (c) 2018, Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause
# Conversion script for UEFI unicode files to OS usage
# Tested with python2 and python3

# Make python3 print function compatible with python2 if needed
from __future__ import print_function
import io
import re
import sys
import os

try:
    from collections import OrderedDict
except ImportError:
    # For python 2.6- support (ESXi build)
    from os_efi_hii_auto_gen_strings_ordered_dict import OrderedDict

def ConvertLine(line):
	'''
	If the line is a special string line, return the useful strings from that line,
	otherwise return None

	#string STR_FOO  #language en  "Foo"
	returns
	['STR_FOO' 'Foo']
	'''
	if (not '#string' in line or not 'STR_' in line):
		return None
	# Plug the below regex into regex101.com for a good explanation
	# Basically two groups (in parens "()") extracting the strings of interest,
	# and ignoring everything else
	match = re.search(r'#string ([\w]+)[^\n"]*"([^\n]*)"', line)
	if not match:
		raise Exception("No matches found for line that contained #string and STR_: ", line)
	return match.group(1), match.group(2)

def ConvertLine_Test():
	assert(None == ConvertLine('#langdef en "English"'))
	assert(len(ConvertLine('#string STR_DCPMM_DECIMAL_MARK  #language en "."')) == 2)

def OrderedDict_Test():
	# Should preserve original insertion order, even with overwriting
	test = OrderedDict()
	test[5] = 'foo'
	test[4] = 'bar'
	test[4] = 'baz'
	l = list(test.keys()) # = [5,4]
	assert(l[0] == 5)
	assert(l[1] == 4)
	assert(test[4] == 'baz')

if __name__ == '__main__':
	# Doesn't hurt to run the unit tests real quick...
	ConvertLine_Test()
	OrderedDict_Test()

	# Get the root uefi directory
	# os.path is an os independent way to get file paths correct. Think '/' vs. '\'
	# __file__ is this file
	uefiRootPath = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.realpath(__file__)))))
	# Later files override earlier files
	inputFiles = [
	os.path.join(uefiRootPath,'DcpmPkg','common','NvmStatus.uni'),
	os.path.join(uefiRootPath,'DcpmPkg','driver','Core','Diagnostics','DiagnosticsMessages.uni')]
	outputFileDir = os.path.dirname(os.path.realpath(__file__))
	# Output to the same directory
	# Different headers because everyone needs indices (defs) but not the actual strings
	outputStringsFileName = os.path.join(outputFileDir,'os_efi_hii_auto_gen_strings.h')
	outputDefsFileName = os.path.join(outputFileDir,'os_efi_hii_auto_gen_defs.h')

	# Parse from command line the files to pull from
	# Ignoring for now
	#if (len(sys.argv) > 1):
	#	inputFiles = sys.argv[1:]

	# "Dictionary" / hash table that remembers the order items were inserted in
	dict = OrderedDict()
	for fileName in inputFiles:
		# Need io to handle utf-16 files in python2
		# https://stackoverflow.com/a/844443
		with io.open(fileName, 'r', encoding='utf-16') as f:
			for line in f:
				#print(line)
				output = ConvertLine(line)
				if (output is None):
					continue
				# Split apart the ConvertLine output
				name, string = output
				#print(name, string)
				# Insert into ordered dictionary and overwrite any previous entries
				dict[name] = string

	# Write output files as utf-8
	with open(outputDefsFileName, 'w') as file:
		# Write out header
		file.write('/**\nDO NOT EDIT\nFILE auto-generated from os_efi_hii_auto_gen_strings.py\n**/\n'
		'#ifndef _AUTO_HII_STRING_DEFS_OS_BUILD\n#define _AUTO_HII_STRING_DEFS_OS_BUILD\n\n')

		# Write indices
		for i, key in enumerate(dict.keys()):
			#print(i, key)
			# Explicit numbering ({0} and {1}) needed for python 2.6-
			# Can use no numbering ({} and {}) for 2.7+
			file.write('#define {0} {1}\n'.format(key, i))
		file.write('\n')
		# Write out footer
		file.write('#endif //// _AUTO_HII_STRING_DEFS_OS_BUILD')

	with open(outputStringsFileName, 'w') as file:
		# Enumerate outputs [i, data[i]] on each iteration, handy when you want to use i
		file.write('/**\nDO NOT EDIT\nFILE auto-generated from os_efi_hii_auto_gen_strings.py\n**/\n'
		'#ifndef _AUTO_HII_STRINGS_OS_BUILD\n#define _AUTO_HII_STRINGS_OS_BUILD\n\nCONST CHAR16* gHiiStrings[] = {\n')
		for i, key in enumerate(dict.keys()):
			file.write('L"{0}", // {1} = {2}\n'.format(dict[key], key, i))
		file.write('};\n\n#endif //// _AUTO_HII_STRINGS_OS_BUILD')
