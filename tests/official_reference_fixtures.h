#pragma once

/* Verbatim JSON examples from anthropics/claude-desktop-buddy REFERENCE.md. */
#define OFFICIAL_HEARTBEAT_JSON                                                   \
    "{\n"                                                                        \
    "  \"total\": 3,\n"                                                        \
    "  \"running\": 1,\n"                                                      \
    "  \"waiting\": 1,\n"                                                      \
    "  \"msg\": \"approve: Bash\",\n"                                          \
    "  \"entries\": [\"10:42 git push\", \"10:41 yarn test\", "             \
    "\"10:39 reading file...\"],\n"                                             \
    "  \"tokens\": 184502,\n"                                                  \
    "  \"tokens_today\": 31200,\n"                                            \
    "  \"prompt\": {\n"                                                         \
    "    \"id\": \"req_abc123\",\n"                                             \
    "    \"tool\": \"Bash\",\n"                                                  \
    "    \"hint\": \"rm -rf /tmp/foo\"\n"                                      \
    "  }\n"                                                                        \
    "}"

#define OFFICIAL_HEARTBEAT_NO_PROMPT_JSON                                        \
    "{\"total\":3,\"running\":1,\"waiting\":0,\"msg\":\"Working\","        \
    "\"entries\":[\"10:42 git push\"],\"tokens\":184502,"                     \
    "\"tokens_today\":31200}"

#define OFFICIAL_TIME_JSON "{ \"time\": [1775731234, -25200] }"
#define OFFICIAL_OWNER_JSON "{ \"cmd\": \"owner\", \"name\": \"Felix\" }"
#define OFFICIAL_STATUS_REQUEST_JSON "{\"cmd\":\"status\"}"
#define OFFICIAL_NAME_JSON "{\"cmd\":\"name\",\"name\":\"Clawd\"}"
#define OFFICIAL_UNPAIR_JSON "{\"cmd\":\"unpair\"}"
