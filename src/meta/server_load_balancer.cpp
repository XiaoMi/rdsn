#include "server_load_balancer.h"
#include <dsn/utility/extensible_object.h>
#include <dsn/utility/string_conv.h>
#include <dsn/tool-api/command_manager.h>
#include <boost/lexical_cast.hpp>
#include <dsn/utils/time_utils.h>

namespace dsn {
namespace replication {
server_load_balancer::server_load_balancer(meta_service *svc) : _svc(svc)
{
    _recent_choose_primary_fail_count.init_app_counter(
        "eon.server_load_balancer",
        "recent_choose_primary_fail_count",
        COUNTER_TYPE_VOLATILE_NUMBER,
        "choose primary fail count in the recent period");
}

void server_load_balancer::register_proposals(meta_view view,
                                              const configuration_balancer_request &req,
                                              configuration_balancer_response &resp)
{
    config_context &cc = *get_config_context(*view.apps, req.gpid);
    partition_configuration &pc = *get_config(*view.apps, req.gpid);
    if (!cc.lb_actions.empty()) {
        resp.err = ERR_INVALID_PARAMETERS;
        return;
    }

    std::vector<configuration_proposal_action> acts = req.action_list;
    for (configuration_proposal_action &act : acts) {
        // for some client generated proposals, the sender may not know the primary address.
        // e.g: "copy_secondary from a to b".
        // the client only knows the secondary a and secondary b, it doesn't know which target
        // to send the proposal to.
        // for these proposals, they should keep the target empty and
        // the meta-server will fill primary as target.
        if (act.target.is_invalid()) {
            if (!pc.primary.is_invalid())
                act.target = pc.primary;
            else {
                resp.err = ERR_INVALID_PARAMETERS;
                return;
            }
        }
    }

    resp.err = ERR_OK;
    cc.lb_actions.assign_balancer_proposals(acts);
    return;
}

void server_load_balancer::apply_balancer(meta_view view, const migration_list &ml)
{
    if (!ml.empty()) {
        configuration_balancer_response resp;
        for (auto &pairs : ml) {
            register_proposals(view, *pairs.second, resp);
            if (resp.err != dsn::ERR_OK) {
                const dsn::gpid &pid = pairs.first;
                dassert(false,
                        "apply balancer for gpid(%d.%d) failed",
                        pid.get_app_id(),
                        pid.get_partition_index());
            }
        }
    }
}
}
}
