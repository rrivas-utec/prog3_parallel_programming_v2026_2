#!/usr/bin/env bash
set -euo pipefail

project_name='prog3_parallel_programming_v2026_2'
source_code=(
  include/parallel_transform_reduce.h
  include/parallel_copy_if.h
  include/parallel_inclusive_scan.h
  include/parallel_find_if.h
)
project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
output_path="${project_root}/${project_name}.zip"

cd "${project_root}"
rm -f "${output_path}"
zip -r -S "${output_path}" "${source_code[@]}"
echo "Archivo generado: ${output_path}"
