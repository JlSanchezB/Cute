#ifndef BOX_CITY_RENDER_H
#define BOX_CITY_RENDER_H

#include <render/render.h>
#include <render/render_resource.h>
#include <render/render_helper.h>
#include "box_city_resources.h"
#include <render_module/render_module_gpu_memory.h>
#include "box_city_components.h"

//Cull boxes
class CullCityBoxesPass : public render::Pass
{
	inline static BoxCityResources* m_display_resources;

	friend class BoxCityGame;
public:
	DECLARE_RENDER_CLASS("CullCityBoxes");

	void Render(render::RenderContext& render_context) const override;
};

//Cull second pass boxes
class CullSecondPassCityBoxesPass : public render::Pass
{
	inline static BoxCityResources* m_display_resources;

	friend class BoxCityGame;
public:
	DECLARE_RENDER_CLASS("CullSecondPassCityBoxes");

	void Render(render::RenderContext& render_context) const override;
};

//Render boxes
class DrawCityBoxesPass : public render::Pass
{
	inline static BoxCityResources* m_display_resources;

	friend class BoxCityGame;
public:
	DECLARE_RENDER_CLASS("DrawCityBoxes");

	void Render(render::RenderContext& render_context) const override;
};

#endif //BOX_CITY_RENDER_H