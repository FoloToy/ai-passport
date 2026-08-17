#include "tamagezi_pet_view.h"

static lv_obj_t *shape(lv_obj_t *parent, int x, int y, int w, int h,
                       int radius, uint32_t color)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    return obj;
}

lv_obj_t *tmz_pet_view_create(lv_obj_t *parent, const tmz_model_t *model,
                              int x, int y, int size)
{
    const tmz_pet_def_t *pet = tmz_pet_def((tmz_pet_id_t)model->species);
    const tmz_pet_def_t *accent = tmz_pet_def((tmz_pet_id_t)model->accent_species);
    uint32_t body = pet ? pet->body_color : 0x63D9A8;
    uint32_t detail = accent ? accent->accent_color : 0x87C93C;
    lv_obj_t *root = shape(parent, x, y, size, size, 0, 0xFFFFFF);
    lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, 0);
    int u = size / 10;
    if (u < 4) u = 4;

    shape(root, u * 2, u * 2, u * 6, u * 6, u * 2, body);
    shape(root, u * 3, u * 4, u, u, u / 2, 0x17202A);
    shape(root, u * 6, u * 4, u, u, u / 2, 0x17202A);
    shape(root, u * 4, u * 6, u * 2, u / 2, u / 4, 0x17202A);
    shape(root, u * 3, u * 8, u, u, 0, 0x17202A);
    shape(root, u * 6, u * 8, u, u, 0, 0x17202A);

    switch (model->species) {
    case TMZ_PET_SPRIG:
        shape(root, u * 4, 0, u * 2, u * 3, u, detail);
        shape(root, u * 5, 0, u * 3, u * 2, u, detail);
        break;
    case TMZ_PET_EMBER:
        shape(root, u * 3, 0, u * 4, u * 3, u * 2, detail);
        shape(root, u * 4, 0, u * 2, u * 2, u, 0xFFD34F);
        break;
    case TMZ_PET_BLOOP:
        shape(root, u, u * 3, u * 2, u * 3, u, detail);
        shape(root, u * 7, u * 3, u * 2, u * 3, u, detail);
        break;
    case TMZ_PET_ROOK:
        shape(root, u * 2, u, u * 2, u * 2, 0, detail);
        shape(root, u * 6, u, u * 2, u * 2, 0, detail);
        break;
    case TMZ_PET_NIMBUS:
        shape(root, u, u * 2, u * 3, u * 3, u * 2, detail);
        shape(root, u * 6, u * 2, u * 3, u * 3, u * 2, detail);
        break;
    case TMZ_PET_COGGO:
        for (int i = 1; i < 9; i += 2) shape(root, u * i, u, u, u * 2, 0, detail);
        break;
    case TMZ_PET_NOVA:
        shape(root, u * 4, 0, u * 2, u * 3, 0, detail);
        shape(root, u, u * 3, u * 3, u * 2, 0, detail);
        shape(root, u * 6, u * 3, u * 3, u * 2, 0, detail);
        break;
    case TMZ_PET_INKY:
        shape(root, u * 2, u, u * 2, u * 2, u, detail);
        shape(root, u * 6, u, u * 2, u * 2, u, detail);
        shape(root, u * 8, u * 6, u, u * 3, u, detail);
        break;
    case TMZ_PET_HONEY:
        shape(root, u, u * 3, u * 2, u * 4, u, 0xE9F3FF);
        shape(root, u * 7, u * 3, u * 2, u * 4, u, 0xE9F3FF);
        shape(root, u * 2, u * 5, u * 6, u, 0, detail);
        break;
    case TMZ_PET_TEMPO:
        shape(root, 0, u * 3, u * 3, u * 4, u, detail);
        shape(root, u * 7, u * 3, u * 3, u * 4, u, detail);
        break;
    case TMZ_PET_LUNA:
        shape(root, u * 3, 0, u * 4, u * 3, u * 2, detail);
        shape(root, u * 5, 0, u * 2, u * 2, u, 0xFFFFFF);
        break;
    case TMZ_PET_ZIP:
        shape(root, u, u, u * 3, u * 3, 0, detail);
        shape(root, u * 6, u, u * 3, u * 3, 0, detail);
        shape(root, u * 8, u * 6, u * 2, u, u / 2, detail);
        break;
    }
    if (model->illness != TMZ_ILLNESS_NONE) {
        lv_obj_t *mark = lv_label_create(root);
        lv_label_set_text(mark, "!");
        lv_obj_set_style_text_font(mark, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(mark, lv_color_hex(0xE43B2F), 0);
        lv_obj_align(mark, LV_ALIGN_TOP_RIGHT, -2, 0);
    }
    return root;
}
