#ifndef BOX_CITY_RENDER_H
#define BOX_CITY_RENDER_H

#include <render/render.h>
#include <render/render_resource.h>
#include <render/render_helper.h>
#include "resources.h"
#include <render_module/render_module_gpu_memory.h>
#include "box_city_components.h"

//render pass definition for our custom GPU renderer
class DrawCityBoxesPass : public render::Pass
{
	uint8_t m_priority;
	inline static DisplayResource* m_display_resources;

	friend class BoxCityGame;
public:
	DECLARE_RENDER_CLASS("DrawCityBoxes");

	void Load(render::LoadContext& load_context) override;
	void Render(render::RenderContext& render_context) const override;
};

//Render pass definition for our custon box instance pass render
class DrawCityBoxItemsPass : public render::Pass
{
	uint8_t m_priority;
	inline static DisplayResource* m_display_resources;

	friend class BoxCityGame;
public:
	DECLARE_RENDER_CLASS("DrawCityBoxItems");

	void Load(render::LoadContext& load_context) override;
	void Render(render::RenderContext& render_context) const override;
};

#endif //BOX_CITY_RENDER_H