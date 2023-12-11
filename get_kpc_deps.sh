#!/bin/bash
git clone https://github.com/NCSU-CSC512-Course-Project/part1-dev.git

cp part1-dev/kpc/Key* ./src
cp part1-dev/kpc/Common.h ./src
cp part1-dev/file_format_style .

rm -rf part1-dev
