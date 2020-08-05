include "../../dsn.thrift"

namespace cpp dsn.security

// negotiation process:
//
//                       client                              server
//                          | ---   SASL_LIST_MECHANISMS    --> |
//                          | <-- SASL_LIST_MECHANISMS_RESP --- |
//                          | --    SASL_SELECT_MECHANISMS  --> |
//                          | <-- SASL_SELECT_MECHANISMS_OK --- |
//                          |                                   |
//                          | ---      SASL_INITIATE        --> |
//                          |                                   |
//                          | <--      SASL_CHALLENGE       --- |
//                          | ---      SASL_RESPONSE        --> |
//                          |                                   |
//                          |              .....                |
//                          |                                   |
//                          | <--      SASL_CHALLENGE       --- |
//                          | ---      SASL_RESPONSE        --> |
//                          |                                   | (authentication will succeed
//                          |                                   |  if all chanllenges passed)
//                          | <--        SASL_SUCC          --- |
// (client won't response   |                                   |
// if servers says ok)      |                                   |
//                          | ---        RPC_CALL          ---> |
//                          | <--        RPC_RESP          ---- |

enum negotiation_status {
    INVALID,
    SASL_LIST_MECHANISMS,
    SASL_LIST_MECHANISMS_RESP,
    SASL_SELECT_MECHANISMS,
    SASL_SELECT_MECHANISMS_OK,
    SASL_INITIATE,
    SASL_CHALLENGE,
    SASL_RESPONSE,
    SASL_SUCC,
    SASL_AUTH_FAIL
}

struct negotiation_message {
    1: negotiation_status status;
    2: dsn.blob msg;
}
