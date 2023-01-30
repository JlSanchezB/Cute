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
	uint8_t m_priority;
	inline static BoxCityResources* m_display_resources;

	friend class BoxCityGame;
public:
	DECLARE_RENDER_CLASS("CullCityBoxes");

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

//Render pass definition for our custon box instance pass render
class DrawCityBoxItemsPass : public render::Pass
{
	uint8_t m_priority;
	inline static BoxCityResources* m_display_resources;

	friend class BoxCityGame;

public:
	DECLARE_RENDER_CLASS("DrawCityBoxItems");

	void Load(render::LoadContext& load_context) override;
	void Destroy(display::Device* device) override;
	void Render(render::RenderContext& render_context) const override;
};

#endif //BOX_CITY_RENDER_H