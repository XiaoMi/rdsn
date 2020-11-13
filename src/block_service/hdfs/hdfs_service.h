#pragma once

#include <dsn/dist/block_service.h>
#include <hdfs/hdfs.h>

namespace dsn {
namespace dist {
namespace block_service {

class hdfs_service : public block_filesystem
{
public:
    hdfs_service();
    error_code create_fs();
    hdfsFS get_fs() { return _fs; }

    virtual ~hdfs_service() override;
    virtual error_code initialize(const std::vector<std::string> &args) override;
    virtual dsn::task_ptr list_dir(const ls_request &req,
                                   dsn::task_code code,
                                   const ls_callback &cb,
                                   dsn::task_tracker *tracker) override;
    virtual dsn::task_ptr create_file(const create_file_request &req,
                                      dsn::task_code code,
                                      const create_file_callback &cb,
                                      dsn::task_tracker *tracker) override;
    virtual dsn::task_ptr remove_path(const remove_path_request &req,
                                      dsn::task_code code,
                                      const remove_path_callback &cb,
                                      dsn::task_tracker *tracker) override;

private:
    std::string get_entry_name(const std::string &hdfs_path);

    hdfsFS _fs;
    std::string _hdfs_name_node;
    std::string _hdfs_path;
};

class hdfs_file_object : public block_file
{
public:
    hdfs_file_object(hdfs_service *s, const std::string &name);
    virtual ~hdfs_file_object();
    virtual uint64_t get_size() override { return _size; }
    virtual const std::string &get_md5sum() override { return _md5sum; }
    virtual dsn::task_ptr write(const write_request &req,
                                dsn::task_code code,
                                const write_callback &cb,
                                dsn::task_tracker *tracker) override;
    virtual dsn::task_ptr read(const read_request &req,
                               dsn::task_code code,
                               const read_callback &cb,
                               dsn::task_tracker *tracker) override;
    virtual dsn::task_ptr upload(const upload_request &req,
                                 dsn::task_code code,
                                 const upload_callback &cb,
                                 dsn::task_tracker *tracker) override;
    virtual dsn::task_ptr download(const download_request &req,
                                   dsn::task_code code,
                                   const download_callback &cb,
                                   dsn::task_tracker *tracker) override;
    error_code get_file_meta();

private:
    error_code
    write_data_in_batches(const char *data, const uint64_t data_size, uint64_t &written_size);
    error_code read_data_in_batches(uint64_t start_pos,
                                    int64_t length,
                                    std::string &read_buffer,
                                    size_t &read_length);

    hdfs_service *_service;
    std::string _md5sum;
    uint64_t _size;
    bool _has_meta_synced;
};
} // namespace block_service
} // namespace dist
} // namespace dsn
