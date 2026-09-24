#include <ft2build.h>

/* SFNT module first: shared by tt/cff, required for TTC/TTF/OTF parsing */
FT_USE_MODULE( FT_Module_Class, sfnt_module_class )

/* Font drivers */
FT_USE_MODULE( FT_Driver_ClassRec, tt_driver_class )
FT_USE_MODULE( FT_Driver_ClassRec, cff_driver_class )

/* Bitmap font drivers */
FT_USE_MODULE( FT_Driver_ClassRec, bdf_driver_class )
FT_USE_MODULE( FT_Driver_ClassRec, pcf_driver_class )

/* Shared modules */
FT_USE_MODULE( FT_Module_Class, psnames_module_class )
FT_USE_MODULE( FT_Module_Class, pshinter_module_class )
FT_USE_MODULE( FT_Module_Class, psaux_module_class )

/* Renderers: smooth for 8BPP AA, raster1 for 1BPP mono */
FT_USE_MODULE( FT_Renderer_Class, ft_smooth_renderer_class )
FT_USE_MODULE( FT_Renderer_Class, ft_raster1_renderer_class )

/* Auto-hinting */
FT_USE_MODULE( FT_Module_Class, autofit_module_class )