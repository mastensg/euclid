#pragma pack(push)
#pragma pack(1)

struct tga_header {
    uint8_t id_length;
    uint8_t color_map_type;
    uint8_t image_type;
    uint16_t first_entry_index;
    uint16_t color_map_length;
    uint8_t color_map_entry_size;
    uint16_t x_origin;
    uint16_t y_origin;
    uint16_t image_width;
    uint16_t image_height;
    uint8_t pixel_depth;
    uint8_t image_descriptor;
};

#pragma pack(pop)
