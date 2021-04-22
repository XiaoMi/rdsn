#pragma once

#include <dsn/http/http_server.h>
#include <dsn/utility/errors.h>
#include <dsn/utility/singleton.h>

namespace dsn {

struct version_info{
    std::string version;
    std::string git_commit;
};

class meta_service_info_registry : public utils::singleton<meta_service_info_registry>
{
public:

    void set_info(version_info &info){
        _info = info;
    }

    version_info get_info(){
        return _info;
    }

private:
    friend class utils::singleton<meta_service_info_registry>;
    meta_service_info_registry() = default;

private:
    version_info _info;
};

} // namespace dsn