/* Automatically generated nanopb header */
/* google/protobuf/duration.proto - manually created for RX72N */

#ifndef PB_GOOGLE_PROTOBUF_DURATION_PB_H_INCLUDED
#define PB_GOOGLE_PROTOBUF_DURATION_PB_H_INCLUDED
#include <pb.h>

#if PB_PROTO_HEADER_VERSION != 40
#error Regenerate this file with the current version of nanopb generator.
#endif

/* Struct definitions */
/* A Duration represents a signed, fixed-length span of time represented
   as a count of seconds and fractions of seconds at nanosecond
   resolution. */
typedef struct _google_protobuf_Duration {
  /* Signed seconds of the span of time. Must be from -315,576,000,000
       to +315,576,000,000 inclusive. */
  int64_t seconds;
  /* Signed fractions of a second at nanosecond resolution of the span
       of time. Durations less than one second are represented with a 0
       seconds field and a positive or negative nanos field. */
  int32_t nanos;
} google_protobuf_Duration;

#ifdef __cplusplus
extern "C" {
#endif

/* Initializer values for message structs */
#define google_protobuf_Duration_init_default {0, 0}
#define google_protobuf_Duration_init_zero    {0, 0}

/* Field tags (for use in manual encoding/decoding) */
#define google_protobuf_Duration_seconds_tag 1
#define google_protobuf_Duration_nanos_tag   2

/* Struct field encoding specification for nanopb */
#define google_protobuf_Duration_FIELDLIST(X, a)                                                   \
  X(a, STATIC, SINGULAR, INT64, seconds, 1)                                                        \
  X(a, STATIC, SINGULAR, INT32, nanos, 2)
#define google_protobuf_Duration_CALLBACK NULL
#define google_protobuf_Duration_DEFAULT  NULL

extern const pb_msgdesc_t google_protobuf_Duration_msg;

/* Defines for backwards compatibility with code written before nanopb-0.4.0 */
#define google_protobuf_Duration_fields &google_protobuf_Duration_msg

/* Maximum encoded size of messages (where known) */
#define google_protobuf_Duration_size 22

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
