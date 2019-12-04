file(REMOVE_RECURSE
  "libdsn_runtime.a"
  "libdsn_runtime.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang CXX)
  include(CMakeFiles/dsn_runtime.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
