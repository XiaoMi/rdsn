#include <dsn/utility/flags.h>
#include <dsn/utility/config_api.h>
#include <dsn/utility/singleton.h>
#include <dsn/utility/errors.h>
#include <dsn/utility/string_conv.h>
#include <dsn/c/api_utilities.h>
#include <boost/optional/optional.hpp>
#include <dsn/dist/fmt_logging.h>

#include <map>
#include <dsn/utility/output_utils.h>

class meta_service_info_registry : public utils::singleton<meta_service_info_registry>
{
public:

    
    

private:
    friend class utils::singleton<meta_service_info_registry>;
    meta_service_info_registry() = default;

private:
    std::string version;
    std::string git_commit;
};