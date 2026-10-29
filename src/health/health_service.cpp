#include "health/health_service.h"

namespace hms::health {

Response liveness_response()
{
    return {200, "{\"status\":\"alive\",\"checks\":{\"process\":\"ok\"}}"};
}

Response readiness_response(bool postgres_available)
{
    if (postgres_available) {
        return {200,
                "{\"status\":\"ready\",\"checks\":{"
                "\"process\":\"ok\",\"postgres\":\"ok\"}}"};
    }
    return {503,
            "{\"status\":\"unavailable\",\"checks\":{"
            "\"process\":\"ok\",\"postgres\":\"unavailable\"}}"};
}

} // namespace hms::health
