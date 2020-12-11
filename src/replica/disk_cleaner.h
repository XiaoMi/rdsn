//
// Created by mi on 20-12-11.
//
#pragma once

#include <vector>
#include <dsn/utility/errors.h>
#include <dsn/perf_counter/perf_counter_wrapper.h>

#ifndef DSN_DISK_CLEANER_H
#define DSN_DISK_CLEANER_H

#endif // DSN_DISK_CLEANER_H
namespace dsn {
namespace replication {
DSN_DECLARE_uint64(gc_disk_error_replica_interval_seconds);
DSN_DECLARE_uint64(gc_disk_garbage_replica_interval_seconds);
DSN_DECLARE_uint64(gc_disk_migration_tmp_replica_interval_seconds);
DSN_DECLARE_uint64(gc_disk_migration_origin_replica_interval_seconds);

// the garbage folder suffix, server will check disk folder and deal with them
extern const std::string kFolderSuffixErr; // replica error dir
extern const std::string kFolderSuffixGar; // replica closed and assign garbage dir
extern const std::string kFolderSuffixBak; // replica backup dir
extern const std::string kFolderSuffixOri; // replica disk migration origin dir
extern const std::string kFolderSuffixTmp; // replica disk migration temp dir

struct disk_cleaning_report
{
    int remove_dir_count;

    int garbage_replica_count;
    int error_replica_count;
    int disk_migrate_tmp_count;
    int disk_migrate_origin_count;
};

/// Removes the useless data from data directories.
error_s disk_remove_useless_dirs(const std::vector<std::string> &data_dirs,
                                 /*output*/ disk_cleaning_report &report);

inline bool is_data_dir_removable(const std::string &dir)
{
    if (dir.length() < 4) {
        return false;
    }
    const std::string folder_suffix = dir.substr(dir.length() - 4);
    return (folder_suffix == kFolderSuffixErr || folder_suffix == kFolderSuffixGar ||
            folder_suffix == kFolderSuffixTmp || folder_suffix == kFolderSuffixOri);
}

// Note: ".bak" is invalid but not allow delete, because it can be backed by administrator.
inline bool is_data_dir_invalid(const std::string &dir)
{
    if (dir.length() < 4) {
        return false;
    }
    const std::string folder_suffix = dir.substr(dir.length() - 4);
    return is_data_dir_removable(dir) || folder_suffix == kFolderSuffixBak;
}
}
}
