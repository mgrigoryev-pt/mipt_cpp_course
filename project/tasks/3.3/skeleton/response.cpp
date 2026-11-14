#include <string>

#include "detection.h"
#include "entity_model.h"
#include "event.h"
#include "os_handle.h"
#include "response.h"

namespace nano_edr {

// --- ResponsePlanner -------------------------------------------------------

ResponsePlanner::ResponsePlanner(const EntityModel* model,
                                 std::string quarantine_dir)
    : model_(model), quarantine_dir_(quarantine_dir) {}

ResponsePlan ResponsePlanner::Plan(const Detection&) const {
    return ResponsePlan();
}

std::string ResponsePlanner::QuarantinePath(const std::string&,
                                            const std::string&) const {
    return "";
}

// --- Responder -------------------------------------------------------------

Responder::Responder(OsHandle* os, bool dry_run) : os_(os), dry_run_(dry_run) {}

void Responder::Execute(const ResponsePlan&) {}

}  // namespace nano_edr
