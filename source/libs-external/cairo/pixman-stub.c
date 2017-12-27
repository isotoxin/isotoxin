#include "config.h"
#include "pixman/pixman-private.h"
#include "src/cairo.h"
#include "src/cairo-error-private.h"
#undef COMPILE_TIME_ASSERT
#include "src/cairoint.h"

pixman_implementation_t * _pixman_mips_get_implementations (pixman_implementation_t *imp)
{
    return imp;
}

pixman_implementation_t * _pixman_arm_get_implementations (pixman_implementation_t *imp)
{
    return imp;
}

pixman_implementation_t * _pixman_ppc_get_implementations (pixman_implementation_t *imp)
{
    return imp;
}



// cairo font stub (isotoxin does not use cairo font)
cairo_scaled_font_t * _cairo_scaled_font_create_in_error (cairo_status_t status) {return 0;}
cairo_status_t _cairo_utf8_to_ucs4(const char *str, int	 len, uint32_t  **result, int	*items_written) { return 0; }
cairo_status_t cairo_scaled_font_text_to_glyphs(cairo_scaled_font_t *scaled_font, double x, double y, const char *utf8, int utf8_len, cairo_glyph_t **glyphs, int *num_glyphs, cairo_text_cluster_t **clusters, int *num_clusters, cairo_text_cluster_flags_t *cluster_flags) { return 0; }
void _cairo_scaled_font_freeze_cache(cairo_scaled_font_t *scaled_font) {}
cairo_status_t _cairo_scaled_font_set_error(cairo_scaled_font_t *scaled_font, cairo_status_t status) { return 0; }
cairo_bool_t _cairo_scaled_font_has_color_glyphs(cairo_scaled_font_t *scaled_font) { return 0; }
cairo_int_status_t _cairo_scaled_glyph_lookup(cairo_scaled_font_t *scaled_font, unsigned long index, cairo_scaled_glyph_info_t info, cairo_scaled_glyph_t **scaled_glyph_ret) { return 0; }
void _cairo_scaled_font_thaw_cache(cairo_scaled_font_t *scaled_font) {}
cairo_status_t _cairo_utf8_to_utf16(const char *str, int len, uint16_t  **result, int *items_written) { return 0; }
cairo_font_face_t * cairo_toy_font_face_create(const char *family, cairo_font_slant_t slant, cairo_font_weight_t weight) { return 0; }
cairo_scaled_font_t *cairo_scaled_font_reference(cairo_scaled_font_t *scaled_font) { return 0; }
cairo_status_t _cairo_scaled_font_glyph_path(cairo_scaled_font_t *scaled_font, const cairo_glyph_t *glyphs, int num_glyphs, cairo_path_fixed_t  *path) { return 0; }
void cairo_scaled_font_destroy(cairo_scaled_font_t *scaled_font) {}
double _cairo_scaled_font_get_max_scale(cairo_scaled_font_t *scaled_font) { return 0; }
void cairo_scaled_font_extents(cairo_scaled_font_t  *scaled_font, cairo_font_extents_t *extents) {}
cairo_status_t cairo_scaled_font_status(cairo_scaled_font_t *scaled_font) { return 0; }
void cairo_scaled_font_glyph_extents(cairo_scaled_font_t   *scaled_font, const cairo_glyph_t   *glyphs, int num_glyphs, cairo_text_extents_t  *extents) {}
cairo_scaled_font_t * cairo_scaled_font_create(cairo_font_face_t *font_face, const cairo_matrix_t *font_matrix, const cairo_matrix_t *ctm, const cairo_font_options_t *options) { return 0; }
void _cairo_scaled_font_map_destroy(void) {}
void _cairo_scaled_font_reset_static_data(void) {}
void _cairo_toy_font_face_reset_static_data(void) {}
cairo_bool_t _cairo_scaled_font_glyph_approximate_extents(cairo_scaled_font_t *scaled_font, const cairo_glyph_t	*glyphs, int num_glyphs, cairo_rectangle_int_t *extents) { return 0; }
cairo_status_t _cairo_scaled_font_glyph_device_extents(cairo_scaled_font_t *scaled_font, const cairo_glyph_t *glyphs, int num_glyphs, cairo_rectangle_int_t *extents, cairo_bool_t *overlap) { return 0; }
void _cairo_font_options_init_default(cairo_font_options_t *options) {}
void cairo_font_face_destroy(cairo_font_face_t *font_face) {}
cairo_status_t cairo_font_options_status(cairo_font_options_t *options) { return 0; }
void _cairo_font_options_init_copy(cairo_font_options_t *options, const cairo_font_options_t *other) {}
void _cairo_font_options_set_round_glyph_positions(cairo_font_options_t *options, cairo_round_glyph_positions_t round) {}
void cairo_font_options_set_hint_metrics(cairo_font_options_t *options, cairo_hint_metrics_t  hint_metrics) {}
void cairo_font_options_merge(cairo_font_options_t *options, const cairo_font_options_t *other) {}
cairo_font_face_t * cairo_font_face_reference(cairo_font_face_t *font_face) { return 0; }
cairo_bool_t cairo_font_options_equal(const cairo_font_options_t *options, const cairo_font_options_t *other) { return 0; }


const cairo_font_face_t _cairo_font_face_nil;
//cairo_font_face_t _cairo_font_face_nil_file_not_found;
