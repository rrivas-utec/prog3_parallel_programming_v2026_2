#!/usr/bin/env bash

project_name='prog3_parallel_programming_v2026_2'
source_code='
  include/parallel_transform_reduce.h
  include/parallel_copy_if.h
  include/parallel_inclusive_scan.h
  include/parallel_find_if.h
  '
cd .. || exit 1
rm -f ${project_name}.zip
zip -r -S ${project_name} ${source_code}
