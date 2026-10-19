#include "referee_ui.h"

#include <string.h>

#define REFEREE_UI_DATA_DELETE 0x0100U
#define REFEREE_UI_DATA_DRAW_ONE 0x0101U
#define REFEREE_UI_DATA_DRAW_TWO 0x0102U
#define REFEREE_UI_DATA_DRAW_FIVE 0x0103U
#define REFEREE_UI_DATA_DRAW_SEVEN 0x0104U
#define REFEREE_UI_DATA_DRAW_TEXT 0x0110U

static bool GraphicFieldsValid(uint8_t operation, uint8_t type,
                               uint8_t layer, uint8_t color,
                               uint16_t start_angle,
                               uint16_t end_angle, uint16_t width,
                               uint16_t start_x, uint16_t start_y,
                               uint16_t radius, uint16_t end_x,
                               uint16_t end_y)
{
    return operation <= 7U && type <= 7U && layer <= 15U &&
           color <= 15U && start_angle <= 511U &&
           end_angle <= 511U && width <= 1023U &&
           start_x <= 2047U && start_y <= 2047U &&
           radius <= 1023U && end_x <= 2047U && end_y <= 2047U;
}

static bool GraphicValid(const RefereeGraphic *graphic)
{
    return graphic != NULL &&
           GraphicFieldsValid(graphic->operation, graphic->type,
                              graphic->layer, graphic->color,
                              graphic->start_angle, graphic->end_angle,
                              graphic->width, graphic->start_x,
                              graphic->start_y, graphic->radius,
                              graphic->end_x, graphic->end_y);
}

static bool MakeGraphic(RefereeGraphic *graphic, const char name[3],
                        uint8_t operation, uint8_t type,
                        uint8_t layer, uint8_t color,
                        uint16_t start_angle, uint16_t end_angle,
                        uint16_t width, uint16_t start_x,
                        uint16_t start_y, uint16_t radius,
                        uint16_t end_x, uint16_t end_y)
{
    RefereeGraphic result = {0};

    if (graphic == NULL || name == NULL ||
        !GraphicFieldsValid(operation, type, layer, color,
                            start_angle, end_angle, width,
                            start_x, start_y, radius, end_x, end_y))
    {
        return false;
    }

    result.name[0] = (uint8_t)name[2];
    result.name[1] = (uint8_t)name[1];
    result.name[2] = (uint8_t)name[0];
    result.operation = operation;
    result.type = type;
    result.layer = layer;
    result.color = color;
    result.start_angle = start_angle;
    result.end_angle = end_angle;
    result.width = width;
    result.start_x = start_x;
    result.start_y = start_y;
    result.radius = radius;
    result.end_x = end_x;
    result.end_y = end_y;
    *graphic = result;
    return true;
}

bool RefereeUiMakeLine(RefereeGraphic *graphic, const char name[3],
                       uint8_t operation, uint8_t layer, uint8_t color,
                       uint16_t width, uint16_t start_x, uint16_t start_y,
                       uint16_t end_x, uint16_t end_y)
{
    return MakeGraphic(graphic, name, operation, REFEREE_UI_LINE,
                       layer, color, 0U, 0U, width, start_x, start_y,
                       0U, end_x, end_y);
}

bool RefereeUiMakeRectangle(RefereeGraphic *graphic, const char name[3],
                            uint8_t operation, uint8_t layer, uint8_t color,
                            uint16_t width, uint16_t start_x,
                            uint16_t start_y, uint16_t end_x,
                            uint16_t end_y)
{
    return MakeGraphic(graphic, name, operation, REFEREE_UI_RECTANGLE,
                       layer, color, 0U, 0U, width, start_x, start_y,
                       0U, end_x, end_y);
}

bool RefereeUiMakeCircle(RefereeGraphic *graphic, const char name[3],
                         uint8_t operation, uint8_t layer, uint8_t color,
                         uint16_t width, uint16_t center_x,
                         uint16_t center_y, uint16_t radius)
{
    return MakeGraphic(graphic, name, operation, REFEREE_UI_CIRCLE,
                       layer, color, 0U, 0U, width, center_x, center_y,
                       radius, 0U, 0U);
}

bool RefereeUiMakeEllipse(RefereeGraphic *graphic, const char name[3],
                          uint8_t operation, uint8_t layer, uint8_t color,
                          uint16_t width, uint16_t center_x,
                          uint16_t center_y, uint16_t x_radius,
                          uint16_t y_radius)
{
    return MakeGraphic(graphic, name, operation, REFEREE_UI_ELLIPSE,
                       layer, color, 0U, 0U, width, center_x, center_y,
                       0U, x_radius, y_radius);
}

bool RefereeUiMakeArc(RefereeGraphic *graphic, const char name[3],
                      uint8_t operation, uint8_t layer, uint8_t color,
                      uint16_t start_angle, uint16_t end_angle,
                      uint16_t width, uint16_t center_x,
                      uint16_t center_y, uint16_t x_radius,
                      uint16_t y_radius)
{
    return MakeGraphic(graphic, name, operation, REFEREE_UI_ARC,
                       layer, color, start_angle, end_angle, width,
                       center_x, center_y, 0U, x_radius, y_radius);
}

static void SplitIntegerValue(int32_t value, uint16_t *low,
                              uint16_t *middle, uint16_t *high)
{
    const uint32_t bits = (uint32_t)value;
    *low = (uint16_t)(bits & 0x3FFU);
    *middle = (uint16_t)((bits >> 10U) & 0x7FFU);
    *high = (uint16_t)((bits >> 21U) & 0x7FFU);
}

bool RefereeUiMakeFloat(RefereeGraphic *graphic, const char name[3],
                        uint8_t operation, uint8_t layer, uint8_t color,
                        uint16_t font_size, uint16_t decimal_digits,
                        uint16_t width, uint16_t start_x, uint16_t start_y,
                        int32_t scaled_value)
{
    uint16_t low;
    uint16_t middle;
    uint16_t high;

    SplitIntegerValue(scaled_value, &low, &middle, &high);
    return MakeGraphic(graphic, name, operation, REFEREE_UI_FLOAT,
                       layer, color, font_size, decimal_digits, width,
                       start_x, start_y, low, middle, high);
}

bool RefereeUiMakeInteger(RefereeGraphic *graphic, const char name[3],
                          uint8_t operation, uint8_t layer, uint8_t color,
                          uint16_t font_size, uint16_t width,
                          uint16_t start_x, uint16_t start_y, int32_t value)
{
    uint16_t low;
    uint16_t middle;
    uint16_t high;

    SplitIntegerValue(value, &low, &middle, &high);
    return MakeGraphic(graphic, name, operation, REFEREE_UI_INTEGER,
                       layer, color, font_size, 0U, width,
                       start_x, start_y, low, middle, high);
}

bool RefereeUiMakeText(RefereeText *text, const char name[3],
                       uint8_t operation, uint8_t layer, uint8_t color,
                       uint16_t font_size, uint16_t width,
                       uint16_t start_x, uint16_t start_y,
                       const char *content)
{
    RefereeText result = {0};
    size_t length = 0U;

    if (text == NULL || content == NULL)
        return false;
    while (length < REFEREE_UI_TEXT_SIZE && content[length] != '\0')
        ++length;
    if (length == REFEREE_UI_TEXT_SIZE && content[length] != '\0')
        return false;
    if (!MakeGraphic(&result.graphic, name, operation, REFEREE_UI_TEXT,
                     layer, color, font_size, (uint16_t)length, width,
                     start_x, start_y, 0U, 0U, 0U))
    {
        return false;
    }
    if (length != 0U)
        memcpy(result.text, content, length);
    result.length = length;
    *text = result;
    return true;
}

static void WriteBitsLE(uint8_t *bytes, size_t bit_offset,
                        uint8_t width, uint32_t value)
{
    uint8_t bit;

    for (bit = 0U; bit < width; ++bit)
    {
        const size_t output_bit = bit_offset + bit;
        const uint8_t value_bit = (uint8_t)((value >> bit) & 1U);
        bytes[output_bit / 8U] |=
            (uint8_t)(value_bit << (output_bit % 8U));
    }
}

static void PackGraphic(const RefereeGraphic *graphic,
                        uint8_t output[REFEREE_UI_GRAPHIC_SIZE])
{
    uint8_t *fields = &output[3];

    memset(output, 0, REFEREE_UI_GRAPHIC_SIZE);
    memcpy(output, graphic->name, sizeof(graphic->name));
    WriteBitsLE(fields, 0U, 3U, graphic->operation);
    WriteBitsLE(fields, 3U, 3U, graphic->type);
    WriteBitsLE(fields, 6U, 4U, graphic->layer);
    WriteBitsLE(fields, 10U, 4U, graphic->color);
    WriteBitsLE(fields, 14U, 9U, graphic->start_angle);
    WriteBitsLE(fields, 23U, 9U, graphic->end_angle);
    WriteBitsLE(fields, 32U, 10U, graphic->width);
    WriteBitsLE(fields, 42U, 11U, graphic->start_x);
    WriteBitsLE(fields, 53U, 11U, graphic->start_y);
    WriteBitsLE(fields, 64U, 10U, graphic->radius);
    WriteBitsLE(fields, 74U, 11U, graphic->end_x);
    WriteBitsLE(fields, 85U, 11U, graphic->end_y);
}

bool RefereeUiDelete(Referee *instance, uint16_t sender_id,
                     uint16_t client_id, uint8_t operation, uint8_t layer)
{
    const uint8_t data[2] = {operation, layer};

    if (operation > REFEREE_UI_DELETE_ALL || layer > 15U)
        return false;
    return RefereeSendInteraction(instance, sender_id, client_id,
                                  REFEREE_UI_DATA_DELETE,
                                  data, sizeof(data));
}

static bool GraphicCommandId(size_t count, uint16_t *command_id)
{
    switch (count)
    {
    case 1U:
        *command_id = REFEREE_UI_DATA_DRAW_ONE;
        return true;
    case 2U:
        *command_id = REFEREE_UI_DATA_DRAW_TWO;
        return true;
    case 5U:
        *command_id = REFEREE_UI_DATA_DRAW_FIVE;
        return true;
    case 7U:
        *command_id = REFEREE_UI_DATA_DRAW_SEVEN;
        return true;
    default:
        return false;
    }
}

bool RefereeUiSendGraphics(Referee *instance, uint16_t sender_id,
                           uint16_t client_id,
                           const RefereeGraphic *graphics, size_t count)
{
    uint8_t data[7U * REFEREE_UI_GRAPHIC_SIZE];
    uint16_t command_id;
    size_t i;

    if (graphics == NULL || !GraphicCommandId(count, &command_id))
        return false;
    for (i = 0U; i < count; ++i)
    {
        if (!GraphicValid(&graphics[i]))
            return false;
    }
    for (i = 0U; i < count; ++i)
    {
        PackGraphic(&graphics[i],
                    &data[i * REFEREE_UI_GRAPHIC_SIZE]);
    }
    return RefereeSendInteraction(instance, sender_id, client_id,
                                  command_id, data,
                                  count * REFEREE_UI_GRAPHIC_SIZE);
}

bool RefereeUiSendText(Referee *instance, uint16_t sender_id,
                       uint16_t client_id, const RefereeText *text)
{
    uint8_t data[REFEREE_UI_GRAPHIC_SIZE + REFEREE_UI_TEXT_SIZE];

    if (text == NULL || text->length > REFEREE_UI_TEXT_SIZE ||
        !GraphicValid(&text->graphic) ||
        text->graphic.type != REFEREE_UI_TEXT ||
        text->graphic.end_angle != text->length)
    {
        return false;
    }
    PackGraphic(&text->graphic, data);
    memcpy(&data[REFEREE_UI_GRAPHIC_SIZE], text->text,
           REFEREE_UI_TEXT_SIZE);
    return RefereeSendInteraction(instance, sender_id, client_id,
                                  REFEREE_UI_DATA_DRAW_TEXT,
                                  data, sizeof(data));
}
