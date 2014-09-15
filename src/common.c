#include <johanson.h>

const char *
jhn_status_to_string(jhn_status stat)
{
    switch (stat) {
    case jhn_status_ok:
        return "ok, no error";
    case jhn_status_client_canceled:
        return "client canceled parse";
    case jhn_status_error:
        return "parse error";
    default:
        return "unknown";
    }
}
