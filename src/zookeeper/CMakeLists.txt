set(MY_PROJ_NAME dsn.replication.zookeeper_provider)


# Source files under CURRENT project directory will be automatically included.
# You can manually set MY_PROJ_SRC to include source files under other directories.
set(MY_PROJ_SRC "")

# Search mode for source files under CURRENT project directory?
# "GLOB_RECURSE" for recursive search
# "GLOB" for non-recursive search
set(MY_SRC_SEARCH_MODE "GLOB")

set(MY_PROJ_LIBS zookeeper hashtable ssl crypto)

# Extra files that will be installed
set(MY_BINPLACES "")

dsn_add_static_library()

add_definitions(-DHAVE_CYRUS_SASL_H)

add_subdirectory(test)
