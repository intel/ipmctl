# Copyright (c) 2018, Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause

# Create the ixp_default.conf file used by installer based on the ixp_default.h

import argparse

delete_list = ["\"", "\\n"]

parser = argparse.ArgumentParser(description='The default ini conf file generator.')
parser.add_argument('src_file', help='input file name')
parser.add_argument('dest_file', help='output file name')
args = parser.parse_args()

infile = open(args.src_file, 'r')
outfile = open(args.dest_file, 'w')
for line in infile:
    if line.rstrip():
        for word in delete_list:
            line = line.replace(word, "")
        outfile.write(line)
infile.close()
outfile.close()
