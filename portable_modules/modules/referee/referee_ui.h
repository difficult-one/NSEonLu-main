#ifndef PORTABLE_REFEREE_UI_H
#define PORTABLE_REFEREE_UI_H

#include "referee.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define REFEREE_UI_GRAPHIC_SIZE 15U
#define REFEREE_UI_TEXT_SIZE 30U

typedef enum
{
    REFEREE_UI_DELETE_NONE = 0,
    REFEREE_UI_DELETE_LAYER = 1,
    REFEREE_UI_DELETE_ALL = 2
} RefereeUiDeleteOperation;

typedef enum
{
    REFEREE_UI_ADD = 1,
    REFEREE_UI_CHANGE = 2,
    REFEREE_UI_DELETE = 3
} RefereeUiGraphicOperation;

typedef enum
{
    REFEREE_UI_LINE = 0,
    REFEREE_UI_RECTANGLE = 1,
    REFEREE_UI_CIRCLE = 2,
    REFEREE_UI_ELLIPSE = 3,
    REFEREE_UI_ARC = 4,
    REFEREE_UI_FLOAT = 5,
    REFEREE_UI_INTEGER = 6,
    REFEREE_UI_TEXT = 7
} RefereeUiGraphicType;

typedef enum
{
    REFEREE_UI_MAIN = 0,
    REFEREE_UI_YELLOW = 1,
    REFEREE_UI_GREEN = 2,
    REFEREE_UI_ORANGE = 3,
    REFEREE_UI_PURPLISH_RED = 4,
    REFEREE_UI_PINK = 5,
    REFEREE_UI_CYAN = 6,
    REFEREE_UI_BLACK = 7,
    REFEREE_UI_WHITE = 8
} RefereeUiColor;

typedef struct
{
    uint8_t name[3];
    uint8_t operation;
    uint8_t type;
    uint8_t layer;
    uint8_t color;
    uint16_t start_angle;
    uint16_t end_angle;
    uint16_t width;
    uint16_t start_x;
    uint16_t start_y;
    uint16_t radius;
    uint16_t end_x;
    uint16_t end_y;
} RefereeGraphic;

typedef struct
{
    RefereeGraphic graphic;
    uint8_t text[REFEREE_UI_TEXT_SIZE];
    size_t length;
} RefereeText;

bool RefereeUiMakeLine(RefereeGraphic *graphic, const char name[3],
                       uint8_t operation, uint8_t layer, uint8_t color,
                       uint16_t width, uint16_t start_x, uint16_t start_y,
                       uint16_t end_x, uint16_t end_y);
bool RefereeUiMakeRectangle(RefereeGraphic *graphic, const char name[3],
                            uint8_t operation, uint8_t layer, uint8_t color,
                            uint16_t width, uint16_t start_x,
                            uint16_t start_y, uint16_t end_x,
                            uint16_t end_y);
bool RefereeUiMakeCircle(RefereeGraphic *graphic, const char name[3],
                         uint8_t operation, uint8_t layer, uint8_t color,
                         uint16_t width, uint16_t center_x,
                         uint16_t center_y, uint16_t radius);
bool RefereeUiMakeEllipse(RefereeGraphic *graphic, const char name[3],
                          uint8_t operation, uint8_t layer, uint8_t color,
                          uint16_t width, uint16_t center_x,
                          uint16_t center_y, uint16_t x_radius,
                          uint16_t y_radius);
bool RefereeUiMakeArc(RefereeGraphic *graphic, const char name[3],
                      uint8_t operation, uint8_t layer, uint8_t color,
                      uint16_t start_angle, uint16_t end_angle,
                      uint16_t width, uint16_t center_x,
                      uint16_t center_y, uint16_t x_radius,
                      uint16_t y_radius);
bool RefereeUiMakeFloat(RefereeGraphic *graphic, const char name[3],
                        uint8_t operation, uint8_t layer, uint8_t color,
                        uint16_t font_size, uint16_t decimal_digits,
                        uint16_t width, uint16_t start_x, uint16_t start_y,
                        int32_t scaled_value);
bool RefereeUiMakeInteger(RefereeGraphic *graphic, const char name[3],
                          uint8_t operation, uint8_t layer, uint8_t color,
                          uint16_t font_size, uint16_t width,
                          uint16_t start_x, uint16_t start_y, int32_t value);
bool RefereeUiMakeText(RefereeText *text, const char name[3],
                       uint8_t operation, uint8_t layer, uint8_t color,
                       uint16_t font_size, uint16_t width,
                       uint16_t start_x, uint16_t start_y,
                       const char *content);
bool RefereeUiDelete(Referee *instance, uint16_t sender_id,
                     uint16_t client_id, uint8_t operation, uint8_t layer);
bool RefereeUiSendGraphics(Referee *instance, uint16_t sender_id,
                           uint16_t client_id,
                           const RefereeGraphic *graphics, size_t count);
bool RefereeUiSendText(Referee *instance, uint16_t sender_id,
                       uint16_t client_id, const RefereeText *text);

#endif
