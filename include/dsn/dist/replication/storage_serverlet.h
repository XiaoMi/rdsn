#pragma once

#include <vector>
#include <unordered_map>
#include <functional>

#include <dsn/service_api_cpp.h>

namespace dsn {
namespace replication {
template <typename T>
class storage_serverlet : public dsn::clientlet
{
protected:
    typedef std::function<void(T *, dsn_message_t req)> rpc_handler;
    static std::unordered_map<std::string, rpc_handler> s_handlers;
    static std::vector<rpc_handler> s_vhandlers;

    template <typename TReq, typename TResp>
    static bool
    register_async_rpc_handler(dsn::task_code rpc_code,
                               const char *name,
                               void (*handler)(T *svc, const TReq &req, rpc_replier<TResp> &resp))
    {
        rpc_handler h = [handler](T *p, dsn_message_t r) {
            TReq req;
            ::dsn::unmarshall(r, req);
            rpc_replier<TResp> replier(dsn_msg_create_response(r));
            handler(p, req, replier);
        };

        return register_async_rpc_handler(rpc_code, name, h);
    }

    template <typename TReq>
    static bool register_async_rpc_handler(dsn::task_code rpc_code,
                                           const char *name,
                                           void (*handler)(T *svc, const TReq &req))
    {
        rpc_handler h = [handler](T *p, dsn_message_t r) {
            TReq req;
            ::dsn::unmarshall(r, req);
            handler(p, req);
        };

        return register_async_rpc_handler(rpc_code, name, h);
    }

    static bool register_async_rpc_handler(dsn::task_code rpc_code, const char *name, rpc_handler h)
    {
        dassert(s_handlers.emplace(rpc_code.to_string(), h).second,
                "handler %s has already been registered",
                rpc_code.to_string());
        dassert(s_handlers.emplace(name, h).second, "handler %s has already been registered", name);

        s_vhandlers.resize(rpc_code + 1);
        dassert(s_vhandlers[rpc_code] == nullptr,
                "handler %s(%d) has already been registered",
                rpc_code.to_string(),
                rpc_code.code());
        s_vhandlers[rpc_code] = h;
        return true;
    }

    static const rpc_handler *find_handler(dsn::task_code rpc_code)
    {
        if (rpc_code < s_vhandlers.size() && s_vhandlers[rpc_code] != nullptr)
            return &s_vhandlers[rpc_code];
        auto iter = s_handlers.find(rpc_code.to_string());
        if (iter != s_handlers.end())
            return &(iter->second);
        return nullptr;
    }

    int handle_request(dsn_message_t request)
    {
        dsn::task_code t = dsn_msg_task_code(request);
        const rpc_handler *ptr = find_handler(t);
        if (ptr != nullptr) {
            (*ptr)(static_cast<T *>(this), request);
        } else {
            dassert(false,
                    "recv message with unhandled rpc name %s from %s, trace_id = %016" PRIx64,
                    t.to_string(),
                    dsn_msg_from_address(request).to_string(),
                    dsn_msg_trace_id(request));
            dsn_rpc_reply(dsn_msg_create_response(request), ::dsn::ERR_HANDLER_NOT_FOUND);
        }
        return 0;
    }
};

template <typename T>
std::unordered_map<std::string, typename storage_serverlet<T>::rpc_handler>
    storage_serverlet<T>::s_handlers;

template <typename T>
std::vector<typename storage_serverlet<T>::rpc_handler> storage_serverlet<T>::s_vhandlers;
}
}
