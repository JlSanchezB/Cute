namespace BoxCityTileSystem
{
	//Parameters that define a zone
	struct ZoneDescriptor
	{
		float length_range_min;
		float length_range_max;
		float angle_inc_range_min;
		float angle_inc_range_max;
		float size_range_min;
		float size_range_max;
		float animation_distance_range_min;
		float animation_distance_range_max;
		float animation_frecuency_range_min;
		float animation_frecuency_range_max;
		float animation_offset_range_min;
		float animation_offset_range_max;
		float static_range;
		uint32_t num_buildings_generated;
		float panel_depth_panel;
		float panel_size_range_min;
		float panel_size_range_max;
		uint32_t num_panel_generated;
		float corridor_size;
	};

	constexpr uint32_t kNumZoneDescriptors = 6;

	const ZoneDescriptor kZoneDescriptors[kNumZoneDescriptors] =
	{
		{50.f, 150.f, -glm::half_pi<float>() * 0.2f, glm::half_pi<float>() * 0.2f, 15.0f, 25.0f, 0.f, 50.f, 0.3f, 1.f, 0.f, 30.f, 10.f, 350, 0.5f, 2.f, 8.f, 48, 50.f},
		{50.f, 120.f, -glm::half_pi<float>() * 0.1f, glm::half_pi<float>() * 0.1f, 25.0f, 35.0f, 0.f, 150.f, 0.5f, 1.f, 0.f, 30.f, 10.f, 350, 0.5f, 2.f, 8.f, 48, 40.f},
		{40.f, 100.f, -glm::half_pi<float>() * 0.25f, glm::half_pi<float>() * 0.25f, 10.0f, 20.0f, 0.f, 70.f, 0.35f, 1.f, 0.f, 30.f, 10.f, 350, 0.5f, 2.f, 8.f, 48, 45.f},
		{30.f, 170.f, -glm::half_pi<float>() * 0.15f, glm::half_pi<float>() * 0.15f, 15.0f, 35.0f, 0.f, 50.f, 0.25f, 1.f, 0.f, 30.f, 10.f, 350, 0.5f, 2.f, 8.f, 48, 40.f},
		{55.f, 200.f, -glm::half_pi<float>() * 0.2f, glm::half_pi<float>() * 0.2f, 10.0f, 25.0f, 0.f, 50.f, 0.4f, 1.f, 0.f, 30.f, 10.f, 350, 0.5f, 2.f, 8.f, 48, 30.f},
		{60.f, 150.f, -glm::half_pi<float>() * 0.15f, glm::half_pi<float>() * 0.15f, 15.0f, 35.0f, 0.f, 50.f, 0.1f, 1.f, 0.f, 30.f, 10.f, 350, 0.5f, 2.f, 8.f, 48, 60.f}
	};
}