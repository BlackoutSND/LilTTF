#if __has_include("LilTTF/FileReader.h")
  #include "LilTTF/FileReader.h"
  #include "LilTTF/Config.h"
#else
  #include "FileReader.h"
  #include "Config.h"
#endif

#define LOG_ERROR(msg) RASTERIZER_LOG("%s\r\n", msg)

FontContext InitFontContext(const uint8_t *data, size_t size) {
  FontContext ctx;
  ctx.data = data;
  ctx.size = size;
  ctx.offset = 0;
  ctx.unitsPerEm = 0;
  ctx.ascender = 0;
  ctx.descender = 0;
  return ctx;
}

uint8_t ReadUInt8(FontContext *context) {
  if (context->offset + 1 > context->size) {
    LOG_ERROR("Error reading from buffer (EOF)");
    return 0;
  }
  uint8_t value = context->data[context->offset];
  context->offset += 1;
  return value;
}

uint16_t ReadUInt16(FontContext *context) {
  if (context->offset + 2 > context->size) {
    LOG_ERROR("Error reading from buffer (EOF)");
    return 0;
  }
  uint16_t value = (uint16_t)((context->data[context->offset] << 8) |
                              context->data[context->offset + 1]);
  context->offset += 2;
  return value;
}

int16_t ReadInt16(FontContext *context) {
  if (context->offset + 2 > context->size) {
    LOG_ERROR("Error reading from buffer (EOF)");
    return 0;
  }
  int16_t value = (int16_t)((context->data[context->offset] << 8) |
                            context->data[context->offset + 1]);
  context->offset += 2;
  return value;
}

int8_t ReadInt8(FontContext *context) {
  if (context->offset + 1 > context->size) {
    LOG_ERROR("Error reading from buffer (EOF)");
    return 0;
  }
  int8_t value = (int8_t)context->data[context->offset];
  context->offset += 1;
  return value;
}

uint32_t ReadUInt32(FontContext *context) {
  if (context->offset + 4 > context->size) {
    LOG_ERROR("Error reading from buffer (EOF)");
    return 0;
  }
  uint32_t value = ((uint32_t)context->data[context->offset] << 24) |
                   ((uint32_t)context->data[context->offset + 1] << 16) |
                   ((uint32_t)context->data[context->offset + 2] << 8) |
                   (uint32_t)context->data[context->offset + 3];
  context->offset += 4;
  return value;
}

void ShiftReadPointer(FontContext *context, int offset) {
  if (offset < 0 && (size_t)(-offset) > context->offset) {
    LOG_ERROR("Seeking before start of buffer");
    context->offset = 0;
    return;
  }
  if (offset > 0 && context->offset + (size_t)offset > context->size) {
    LOG_ERROR("Seeking past end of buffer");
    context->offset = context->size;
    return;
  }
  context->offset += offset;
}

void GoToOffset(FontContext *context, int offset) {
  if (offset < 0 || (size_t)offset > context->size) {
    LOG_ERROR("Seeking out of bounds");
    return;
  }
  context->offset = (size_t)offset;
}

uint32_t GetCurrentOffset(FontContext *context) {
  return (uint32_t)context->offset;
}
