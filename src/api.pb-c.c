/* Generated by the protocol buffer compiler.  DO NOT EDIT! */
/* Generated from: api.proto */

/* Do not generate deprecated warnings for self */
#ifndef PROTOBUF_C__NO_DEPRECATED
#define PROTOBUF_C__NO_DEPRECATED
#endif

#include "api.pb-c.h"
void   doppler_settings__init
                     (DopplerSettings         *message)
{
  static const DopplerSettings init_value = DOPPLER_SETTINGS__INIT;
  *message = init_value;
}
size_t doppler_settings__get_packed_size
                     (const DopplerSettings *message)
{
  assert(message->base.descriptor == &doppler_settings__descriptor);
  return protobuf_c_message_get_packed_size ((const ProtobufCMessage*)(message));
}
size_t doppler_settings__pack
                     (const DopplerSettings *message,
                      uint8_t       *out)
{
  assert(message->base.descriptor == &doppler_settings__descriptor);
  return protobuf_c_message_pack ((const ProtobufCMessage*)message, out);
}
size_t doppler_settings__pack_to_buffer
                     (const DopplerSettings *message,
                      ProtobufCBuffer *buffer)
{
  assert(message->base.descriptor == &doppler_settings__descriptor);
  return protobuf_c_message_pack_to_buffer ((const ProtobufCMessage*)message, buffer);
}
DopplerSettings *
       doppler_settings__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data)
{
  return (DopplerSettings *)
     protobuf_c_message_unpack (&doppler_settings__descriptor,
                                allocator, len, data);
}
void   doppler_settings__free_unpacked
                     (DopplerSettings *message,
                      ProtobufCAllocator *allocator)
{
  if(!message)
    return;
  assert(message->base.descriptor == &doppler_settings__descriptor);
  protobuf_c_message_free_unpacked ((ProtobufCMessage*)message, allocator);
}
void   fsk_demodulation_settings__init
                     (FskDemodulationSettings         *message)
{
  static const FskDemodulationSettings init_value = FSK_DEMODULATION_SETTINGS__INIT;
  *message = init_value;
}
size_t fsk_demodulation_settings__get_packed_size
                     (const FskDemodulationSettings *message)
{
  assert(message->base.descriptor == &fsk_demodulation_settings__descriptor);
  return protobuf_c_message_get_packed_size ((const ProtobufCMessage*)(message));
}
size_t fsk_demodulation_settings__pack
                     (const FskDemodulationSettings *message,
                      uint8_t       *out)
{
  assert(message->base.descriptor == &fsk_demodulation_settings__descriptor);
  return protobuf_c_message_pack ((const ProtobufCMessage*)message, out);
}
size_t fsk_demodulation_settings__pack_to_buffer
                     (const FskDemodulationSettings *message,
                      ProtobufCBuffer *buffer)
{
  assert(message->base.descriptor == &fsk_demodulation_settings__descriptor);
  return protobuf_c_message_pack_to_buffer ((const ProtobufCMessage*)message, buffer);
}
FskDemodulationSettings *
       fsk_demodulation_settings__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data)
{
  return (FskDemodulationSettings *)
     protobuf_c_message_unpack (&fsk_demodulation_settings__descriptor,
                                allocator, len, data);
}
void   fsk_demodulation_settings__free_unpacked
                     (FskDemodulationSettings *message,
                      ProtobufCAllocator *allocator)
{
  if(!message)
    return;
  assert(message->base.descriptor == &fsk_demodulation_settings__descriptor);
  protobuf_c_message_free_unpacked ((ProtobufCMessage*)message, allocator);
}
void   fsk_modulation_settings__init
                     (FskModulationSettings         *message)
{
  static const FskModulationSettings init_value = FSK_MODULATION_SETTINGS__INIT;
  *message = init_value;
}
size_t fsk_modulation_settings__get_packed_size
                     (const FskModulationSettings *message)
{
  assert(message->base.descriptor == &fsk_modulation_settings__descriptor);
  return protobuf_c_message_get_packed_size ((const ProtobufCMessage*)(message));
}
size_t fsk_modulation_settings__pack
                     (const FskModulationSettings *message,
                      uint8_t       *out)
{
  assert(message->base.descriptor == &fsk_modulation_settings__descriptor);
  return protobuf_c_message_pack ((const ProtobufCMessage*)message, out);
}
size_t fsk_modulation_settings__pack_to_buffer
                     (const FskModulationSettings *message,
                      ProtobufCBuffer *buffer)
{
  assert(message->base.descriptor == &fsk_modulation_settings__descriptor);
  return protobuf_c_message_pack_to_buffer ((const ProtobufCMessage*)message, buffer);
}
FskModulationSettings *
       fsk_modulation_settings__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data)
{
  return (FskModulationSettings *)
     protobuf_c_message_unpack (&fsk_modulation_settings__descriptor,
                                allocator, len, data);
}
void   fsk_modulation_settings__free_unpacked
                     (FskModulationSettings *message,
                      ProtobufCAllocator *allocator)
{
  if(!message)
    return;
  assert(message->base.descriptor == &fsk_modulation_settings__descriptor);
  protobuf_c_message_free_unpacked ((ProtobufCMessage*)message, allocator);
}
void   rx_request__init
                     (RxRequest         *message)
{
  static const RxRequest init_value = RX_REQUEST__INIT;
  *message = init_value;
}
size_t rx_request__get_packed_size
                     (const RxRequest *message)
{
  assert(message->base.descriptor == &rx_request__descriptor);
  return protobuf_c_message_get_packed_size ((const ProtobufCMessage*)(message));
}
size_t rx_request__pack
                     (const RxRequest *message,
                      uint8_t       *out)
{
  assert(message->base.descriptor == &rx_request__descriptor);
  return protobuf_c_message_pack ((const ProtobufCMessage*)message, out);
}
size_t rx_request__pack_to_buffer
                     (const RxRequest *message,
                      ProtobufCBuffer *buffer)
{
  assert(message->base.descriptor == &rx_request__descriptor);
  return protobuf_c_message_pack_to_buffer ((const ProtobufCMessage*)message, buffer);
}
RxRequest *
       rx_request__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data)
{
  return (RxRequest *)
     protobuf_c_message_unpack (&rx_request__descriptor,
                                allocator, len, data);
}
void   rx_request__free_unpacked
                     (RxRequest *message,
                      ProtobufCAllocator *allocator)
{
  if(!message)
    return;
  assert(message->base.descriptor == &rx_request__descriptor);
  protobuf_c_message_free_unpacked ((ProtobufCMessage*)message, allocator);
}
void   tx_request__init
                     (TxRequest         *message)
{
  static const TxRequest init_value = TX_REQUEST__INIT;
  *message = init_value;
}
size_t tx_request__get_packed_size
                     (const TxRequest *message)
{
  assert(message->base.descriptor == &tx_request__descriptor);
  return protobuf_c_message_get_packed_size ((const ProtobufCMessage*)(message));
}
size_t tx_request__pack
                     (const TxRequest *message,
                      uint8_t       *out)
{
  assert(message->base.descriptor == &tx_request__descriptor);
  return protobuf_c_message_pack ((const ProtobufCMessage*)message, out);
}
size_t tx_request__pack_to_buffer
                     (const TxRequest *message,
                      ProtobufCBuffer *buffer)
{
  assert(message->base.descriptor == &tx_request__descriptor);
  return protobuf_c_message_pack_to_buffer ((const ProtobufCMessage*)message, buffer);
}
TxRequest *
       tx_request__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data)
{
  return (TxRequest *)
     protobuf_c_message_unpack (&tx_request__descriptor,
                                allocator, len, data);
}
void   tx_request__free_unpacked
                     (TxRequest *message,
                      ProtobufCAllocator *allocator)
{
  if(!message)
    return;
  assert(message->base.descriptor == &tx_request__descriptor);
  protobuf_c_message_free_unpacked ((ProtobufCMessage*)message, allocator);
}
void   response__init
                     (Response         *message)
{
  static const Response init_value = RESPONSE__INIT;
  *message = init_value;
}
size_t response__get_packed_size
                     (const Response *message)
{
  assert(message->base.descriptor == &response__descriptor);
  return protobuf_c_message_get_packed_size ((const ProtobufCMessage*)(message));
}
size_t response__pack
                     (const Response *message,
                      uint8_t       *out)
{
  assert(message->base.descriptor == &response__descriptor);
  return protobuf_c_message_pack ((const ProtobufCMessage*)message, out);
}
size_t response__pack_to_buffer
                     (const Response *message,
                      ProtobufCBuffer *buffer)
{
  assert(message->base.descriptor == &response__descriptor);
  return protobuf_c_message_pack_to_buffer ((const ProtobufCMessage*)message, buffer);
}
Response *
       response__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data)
{
  return (Response *)
     protobuf_c_message_unpack (&response__descriptor,
                                allocator, len, data);
}
void   response__free_unpacked
                     (Response *message,
                      ProtobufCAllocator *allocator)
{
  if(!message)
    return;
  assert(message->base.descriptor == &response__descriptor);
  protobuf_c_message_free_unpacked ((ProtobufCMessage*)message, allocator);
}
void   tx_data__init
                     (TxData         *message)
{
  static const TxData init_value = TX_DATA__INIT;
  *message = init_value;
}
size_t tx_data__get_packed_size
                     (const TxData *message)
{
  assert(message->base.descriptor == &tx_data__descriptor);
  return protobuf_c_message_get_packed_size ((const ProtobufCMessage*)(message));
}
size_t tx_data__pack
                     (const TxData *message,
                      uint8_t       *out)
{
  assert(message->base.descriptor == &tx_data__descriptor);
  return protobuf_c_message_pack ((const ProtobufCMessage*)message, out);
}
size_t tx_data__pack_to_buffer
                     (const TxData *message,
                      ProtobufCBuffer *buffer)
{
  assert(message->base.descriptor == &tx_data__descriptor);
  return protobuf_c_message_pack_to_buffer ((const ProtobufCMessage*)message, buffer);
}
TxData *
       tx_data__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data)
{
  return (TxData *)
     protobuf_c_message_unpack (&tx_data__descriptor,
                                allocator, len, data);
}
void   tx_data__free_unpacked
                     (TxData *message,
                      ProtobufCAllocator *allocator)
{
  if(!message)
    return;
  assert(message->base.descriptor == &tx_data__descriptor);
  protobuf_c_message_free_unpacked ((ProtobufCMessage*)message, allocator);
}
static const ProtobufCFieldDescriptor doppler_settings__field_descriptors[4] =
{
  {
    "tle",
    1,
    PROTOBUF_C_LABEL_REPEATED,
    PROTOBUF_C_TYPE_STRING,
    offsetof(DopplerSettings, n_tle),
    offsetof(DopplerSettings, tle),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "latitude",
    2,
    PROTOBUF_C_LABEL_REQUIRED,
    PROTOBUF_C_TYPE_UINT32,
    0,   /* quantifier_offset */
    offsetof(DopplerSettings, latitude),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "longitude",
    3,
    PROTOBUF_C_LABEL_REQUIRED,
    PROTOBUF_C_TYPE_UINT32,
    0,   /* quantifier_offset */
    offsetof(DopplerSettings, longitude),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "altitude",
    4,
    PROTOBUF_C_LABEL_REQUIRED,
    PROTOBUF_C_TYPE_UINT32,
    0,   /* quantifier_offset */
    offsetof(DopplerSettings, altitude),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
};
static const unsigned doppler_settings__field_indices_by_name[] = {
  3,   /* field[3] = altitude */
  1,   /* field[1] = latitude */
  2,   /* field[2] = longitude */
  0,   /* field[0] = tle */
};
static const ProtobufCIntRange doppler_settings__number_ranges[1 + 1] =
{
  { 1, 0 },
  { 0, 4 }
};
const ProtobufCMessageDescriptor doppler_settings__descriptor =
{
  PROTOBUF_C__MESSAGE_DESCRIPTOR_MAGIC,
  "doppler_settings",
  "DopplerSettings",
  "DopplerSettings",
  "",
  sizeof(DopplerSettings),
  4,
  doppler_settings__field_descriptors,
  doppler_settings__field_indices_by_name,
  1,  doppler_settings__number_ranges,
  (ProtobufCMessageInit) doppler_settings__init,
  NULL,NULL,NULL    /* reserved[123] */
};
static const ProtobufCFieldDescriptor fsk_demodulation_settings__field_descriptors[3] =
{
  {
    "demod_fsk_deviation",
    1,
    PROTOBUF_C_LABEL_REQUIRED,
    PROTOBUF_C_TYPE_INT32,
    0,   /* quantifier_offset */
    offsetof(FskDemodulationSettings, demod_fsk_deviation),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "demod_fsk_transition_width",
    2,
    PROTOBUF_C_LABEL_REQUIRED,
    PROTOBUF_C_TYPE_UINT32,
    0,   /* quantifier_offset */
    offsetof(FskDemodulationSettings, demod_fsk_transition_width),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "demod_fsk_use_dc_block",
    3,
    PROTOBUF_C_LABEL_REQUIRED,
    PROTOBUF_C_TYPE_BOOL,
    0,   /* quantifier_offset */
    offsetof(FskDemodulationSettings, demod_fsk_use_dc_block),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
};
static const unsigned fsk_demodulation_settings__field_indices_by_name[] = {
  0,   /* field[0] = demod_fsk_deviation */
  1,   /* field[1] = demod_fsk_transition_width */
  2,   /* field[2] = demod_fsk_use_dc_block */
};
static const ProtobufCIntRange fsk_demodulation_settings__number_ranges[1 + 1] =
{
  { 1, 0 },
  { 0, 3 }
};
const ProtobufCMessageDescriptor fsk_demodulation_settings__descriptor =
{
  PROTOBUF_C__MESSAGE_DESCRIPTOR_MAGIC,
  "fsk_demodulation_settings",
  "FskDemodulationSettings",
  "FskDemodulationSettings",
  "",
  sizeof(FskDemodulationSettings),
  3,
  fsk_demodulation_settings__field_descriptors,
  fsk_demodulation_settings__field_indices_by_name,
  1,  fsk_demodulation_settings__number_ranges,
  (ProtobufCMessageInit) fsk_demodulation_settings__init,
  NULL,NULL,NULL    /* reserved[123] */
};
static const ProtobufCFieldDescriptor fsk_modulation_settings__field_descriptors[1] =
{
  {
    "mod_fsk_deviation",
    1,
    PROTOBUF_C_LABEL_REQUIRED,
    PROTOBUF_C_TYPE_INT32,
    0,   /* quantifier_offset */
    offsetof(FskModulationSettings, mod_fsk_deviation),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
};
static const unsigned fsk_modulation_settings__field_indices_by_name[] = {
  0,   /* field[0] = mod_fsk_deviation */
};
static const ProtobufCIntRange fsk_modulation_settings__number_ranges[1 + 1] =
{
  { 1, 0 },
  { 0, 1 }
};
const ProtobufCMessageDescriptor fsk_modulation_settings__descriptor =
{
  PROTOBUF_C__MESSAGE_DESCRIPTOR_MAGIC,
  "fsk_modulation_settings",
  "FskModulationSettings",
  "FskModulationSettings",
  "",
  sizeof(FskModulationSettings),
  1,
  fsk_modulation_settings__field_descriptors,
  fsk_modulation_settings__field_indices_by_name,
  1,  fsk_modulation_settings__number_ranges,
  (ProtobufCMessageInit) fsk_modulation_settings__init,
  NULL,NULL,NULL    /* reserved[123] */
};
static const ProtobufCFieldDescriptor rx_request__field_descriptors[10] =
{
  {
    "rx_center_freq",
    1,
    PROTOBUF_C_LABEL_REQUIRED,
    PROTOBUF_C_TYPE_UINT32,
    0,   /* quantifier_offset */
    offsetof(RxRequest, rx_center_freq),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "rx_sampling_freq",
    2,
    PROTOBUF_C_LABEL_REQUIRED,
    PROTOBUF_C_TYPE_UINT32,
    0,   /* quantifier_offset */
    offsetof(RxRequest, rx_sampling_freq),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "rx_dump_file",
    3,
    PROTOBUF_C_LABEL_REQUIRED,
    PROTOBUF_C_TYPE_BOOL,
    0,   /* quantifier_offset */
    offsetof(RxRequest, rx_dump_file),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "rx_sdr_server_band_freq",
    4,
    PROTOBUF_C_LABEL_REQUIRED,
    PROTOBUF_C_TYPE_UINT32,
    0,   /* quantifier_offset */
    offsetof(RxRequest, rx_sdr_server_band_freq),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "demod_type",
    5,
    PROTOBUF_C_LABEL_REQUIRED,
    PROTOBUF_C_TYPE_ENUM,
    0,   /* quantifier_offset */
    offsetof(RxRequest, demod_type),
    &modem_type__descriptor,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "demod_baud_rate",
    6,
    PROTOBUF_C_LABEL_REQUIRED,
    PROTOBUF_C_TYPE_UINT32,
    0,   /* quantifier_offset */
    offsetof(RxRequest, demod_baud_rate),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "demod_decimation",
    7,
    PROTOBUF_C_LABEL_REQUIRED,
    PROTOBUF_C_TYPE_UINT32,
    0,   /* quantifier_offset */
    offsetof(RxRequest, demod_decimation),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "demod_destination",
    8,
    PROTOBUF_C_LABEL_REQUIRED,
    PROTOBUF_C_TYPE_ENUM,
    0,   /* quantifier_offset */
    offsetof(RxRequest, demod_destination),
    &demod_destination__descriptor,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "doppler",
    9,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_MESSAGE,
    0,   /* quantifier_offset */
    offsetof(RxRequest, doppler),
    &doppler_settings__descriptor,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "fsk_settings",
    10,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_MESSAGE,
    0,   /* quantifier_offset */
    offsetof(RxRequest, fsk_settings),
    &fsk_demodulation_settings__descriptor,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
};
static const unsigned rx_request__field_indices_by_name[] = {
  5,   /* field[5] = demod_baud_rate */
  6,   /* field[6] = demod_decimation */
  7,   /* field[7] = demod_destination */
  4,   /* field[4] = demod_type */
  8,   /* field[8] = doppler */
  9,   /* field[9] = fsk_settings */
  0,   /* field[0] = rx_center_freq */
  2,   /* field[2] = rx_dump_file */
  1,   /* field[1] = rx_sampling_freq */
  3,   /* field[3] = rx_sdr_server_band_freq */
};
static const ProtobufCIntRange rx_request__number_ranges[1 + 1] =
{
  { 1, 0 },
  { 0, 10 }
};
const ProtobufCMessageDescriptor rx_request__descriptor =
{
  PROTOBUF_C__MESSAGE_DESCRIPTOR_MAGIC,
  "RxRequest",
  "RxRequest",
  "RxRequest",
  "",
  sizeof(RxRequest),
  10,
  rx_request__field_descriptors,
  rx_request__field_indices_by_name,
  1,  rx_request__number_ranges,
  (ProtobufCMessageInit) rx_request__init,
  NULL,NULL,NULL    /* reserved[123] */
};
static const ProtobufCFieldDescriptor tx_request__field_descriptors[7] =
{
  {
    "tx_center_freq",
    1,
    PROTOBUF_C_LABEL_REQUIRED,
    PROTOBUF_C_TYPE_UINT32,
    0,   /* quantifier_offset */
    offsetof(TxRequest, tx_center_freq),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "tx_sampling_freq",
    2,
    PROTOBUF_C_LABEL_REQUIRED,
    PROTOBUF_C_TYPE_UINT32,
    0,   /* quantifier_offset */
    offsetof(TxRequest, tx_sampling_freq),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "tx_dump_file",
    3,
    PROTOBUF_C_LABEL_REQUIRED,
    PROTOBUF_C_TYPE_BOOL,
    0,   /* quantifier_offset */
    offsetof(TxRequest, tx_dump_file),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "mod_type",
    4,
    PROTOBUF_C_LABEL_REQUIRED,
    PROTOBUF_C_TYPE_ENUM,
    0,   /* quantifier_offset */
    offsetof(TxRequest, mod_type),
    &modem_type__descriptor,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "mod_baud_rate",
    5,
    PROTOBUF_C_LABEL_REQUIRED,
    PROTOBUF_C_TYPE_UINT32,
    0,   /* quantifier_offset */
    offsetof(TxRequest, mod_baud_rate),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "doppler",
    6,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_MESSAGE,
    0,   /* quantifier_offset */
    offsetof(TxRequest, doppler),
    &doppler_settings__descriptor,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "fsk_settings",
    7,
    PROTOBUF_C_LABEL_OPTIONAL,
    PROTOBUF_C_TYPE_MESSAGE,
    0,   /* quantifier_offset */
    offsetof(TxRequest, fsk_settings),
    &fsk_modulation_settings__descriptor,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
};
static const unsigned tx_request__field_indices_by_name[] = {
  5,   /* field[5] = doppler */
  6,   /* field[6] = fsk_settings */
  4,   /* field[4] = mod_baud_rate */
  3,   /* field[3] = mod_type */
  0,   /* field[0] = tx_center_freq */
  2,   /* field[2] = tx_dump_file */
  1,   /* field[1] = tx_sampling_freq */
};
static const ProtobufCIntRange tx_request__number_ranges[1 + 1] =
{
  { 1, 0 },
  { 0, 7 }
};
const ProtobufCMessageDescriptor tx_request__descriptor =
{
  PROTOBUF_C__MESSAGE_DESCRIPTOR_MAGIC,
  "TxRequest",
  "TxRequest",
  "TxRequest",
  "",
  sizeof(TxRequest),
  7,
  tx_request__field_descriptors,
  tx_request__field_indices_by_name,
  1,  tx_request__number_ranges,
  (ProtobufCMessageInit) tx_request__init,
  NULL,NULL,NULL    /* reserved[123] */
};
static const ProtobufCFieldDescriptor response__field_descriptors[2] =
{
  {
    "status",
    1,
    PROTOBUF_C_LABEL_REQUIRED,
    PROTOBUF_C_TYPE_ENUM,
    0,   /* quantifier_offset */
    offsetof(Response, status),
    &response_status__descriptor,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "details",
    2,
    PROTOBUF_C_LABEL_REQUIRED,
    PROTOBUF_C_TYPE_UINT32,
    0,   /* quantifier_offset */
    offsetof(Response, details),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
};
static const unsigned response__field_indices_by_name[] = {
  1,   /* field[1] = details */
  0,   /* field[0] = status */
};
static const ProtobufCIntRange response__number_ranges[1 + 1] =
{
  { 1, 0 },
  { 0, 2 }
};
const ProtobufCMessageDescriptor response__descriptor =
{
  PROTOBUF_C__MESSAGE_DESCRIPTOR_MAGIC,
  "Response",
  "Response",
  "Response",
  "",
  sizeof(Response),
  2,
  response__field_descriptors,
  response__field_indices_by_name,
  1,  response__number_ranges,
  (ProtobufCMessageInit) response__init,
  NULL,NULL,NULL    /* reserved[123] */
};
static const ProtobufCFieldDescriptor tx_data__field_descriptors[1] =
{
  {
    "data",
    1,
    PROTOBUF_C_LABEL_REQUIRED,
    PROTOBUF_C_TYPE_BYTES,
    0,   /* quantifier_offset */
    offsetof(TxData, data),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
};
static const unsigned tx_data__field_indices_by_name[] = {
  0,   /* field[0] = data */
};
static const ProtobufCIntRange tx_data__number_ranges[1 + 1] =
{
  { 1, 0 },
  { 0, 1 }
};
const ProtobufCMessageDescriptor tx_data__descriptor =
{
  PROTOBUF_C__MESSAGE_DESCRIPTOR_MAGIC,
  "TxData",
  "TxData",
  "TxData",
  "",
  sizeof(TxData),
  1,
  tx_data__field_descriptors,
  tx_data__field_indices_by_name,
  1,  tx_data__number_ranges,
  (ProtobufCMessageInit) tx_data__init,
  NULL,NULL,NULL    /* reserved[123] */
};
static const ProtobufCEnumValue modem_type__enum_values_by_number[1] =
{
  { "GMSK", "MODEM_TYPE__GMSK", 1 },
};
static const ProtobufCIntRange modem_type__value_ranges[] = {
{1, 0},{0, 1}
};
static const ProtobufCEnumValueIndex modem_type__enum_values_by_name[1] =
{
  { "GMSK", 0 },
};
const ProtobufCEnumDescriptor modem_type__descriptor =
{
  PROTOBUF_C__ENUM_DESCRIPTOR_MAGIC,
  "modem_type",
  "modem_type",
  "ModemType",
  "",
  1,
  modem_type__enum_values_by_number,
  1,
  modem_type__enum_values_by_name,
  1,
  modem_type__value_ranges,
  NULL,NULL,NULL,NULL   /* reserved[1234] */
};
static const ProtobufCEnumValue demod_destination__enum_values_by_number[3] =
{
  { "FILE", "DEMOD_DESTINATION__FILE", 0 },
  { "SOCKET", "DEMOD_DESTINATION__SOCKET", 1 },
  { "BOTH", "DEMOD_DESTINATION__BOTH", 2 },
};
static const ProtobufCIntRange demod_destination__value_ranges[] = {
{0, 0},{0, 3}
};
static const ProtobufCEnumValueIndex demod_destination__enum_values_by_name[3] =
{
  { "BOTH", 2 },
  { "FILE", 0 },
  { "SOCKET", 1 },
};
const ProtobufCEnumDescriptor demod_destination__descriptor =
{
  PROTOBUF_C__ENUM_DESCRIPTOR_MAGIC,
  "demod_destination",
  "demod_destination",
  "DemodDestination",
  "",
  3,
  demod_destination__enum_values_by_number,
  3,
  demod_destination__enum_values_by_name,
  1,
  demod_destination__value_ranges,
  NULL,NULL,NULL,NULL   /* reserved[1234] */
};
static const ProtobufCEnumValue response_status__enum_values_by_number[2] =
{
  { "SUCCESS", "RESPONSE_STATUS__SUCCESS", 0 },
  { "FAILURE", "RESPONSE_STATUS__FAILURE", 1 },
};
static const ProtobufCIntRange response_status__value_ranges[] = {
{0, 0},{0, 2}
};
static const ProtobufCEnumValueIndex response_status__enum_values_by_name[2] =
{
  { "FAILURE", 1 },
  { "SUCCESS", 0 },
};
const ProtobufCEnumDescriptor response_status__descriptor =
{
  PROTOBUF_C__ENUM_DESCRIPTOR_MAGIC,
  "response_status",
  "response_status",
  "ResponseStatus",
  "",
  2,
  response_status__enum_values_by_number,
  2,
  response_status__enum_values_by_name,
  1,
  response_status__value_ranges,
  NULL,NULL,NULL,NULL   /* reserved[1234] */
};
