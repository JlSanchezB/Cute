//////////////////////////////////////////////////////////////////////////
// Cute engine - Renderer
//////////////////////////////////////////////////////////////////////////
#ifndef RENDERER_H_
#define RENDERER_H_

namespace render
{
	//Base resource class
	class Resource
	{
	public:
		//Load from XML node
		virtual void load() = 0;
	};

	//Base Pass class
	class Pass
	{
	public:
		//Load from XML node
		virtual void load() = 0;
		//Render
		virtual void render() = 0;
	};

	//Class for rendering all the passes
	//Keep a list of global resources and a list of passes
	class Renderer
	{

	};
}

#endif //RENDERER_H_
