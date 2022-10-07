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
		{50.f, 150.f, -glm::half_pi<float>() * 0.2f, glm::half_pi<float>() * 0.2f, 20.0f, 30.0f, 0.f, 50.f, 0.3f, 1.f, 0.f, 40.f, 10.f, 350, 5.f, 5.f, 15.f, 16, 2.f},
		{100.f, 250.f, -glm::half_pi<float>() * 0.4f, glm::half_pi<float>() * 0.4f, 15.0f, 25.0f, 0.f, 60.f, 0.3f, 2.f, 0.f, 30.f, 5.f, 250, 5.f, 5.f, 10.f, 24, 0.f},
		{30.f, 50.f, -glm::half_pi<float>() * 0.7f, glm::half_pi<float>() * 0.7f, 20.0f, 30.0f, 0.f, 20.f, 0.3f, 0.5f, 0.f, 20.f, 5.f, 350, 5.f, 5.f, 20.f, 12, 0.f},
		{40.f, 60.f, -glm::half_pi<float>() * 0.2f, glm::half_pi<float>() * 0.2f, 20.0f, 60.0f, 0.f, 70.f, 0.6f, 2.f, 0.f, 10.f, 1.f, 350, 5.f, 5.f, 15.f, 8, 0.f},
		{200.f, 350.f, -glm::half_pi<float>() * 0.1f, glm::half_pi<float>() * 0.1f, 20.0f, 40.0f, 0.f, 50.f, 0.1f, 0.5f, 0.f, 10.f, 2.f, 220, 5.f, 3.f, 8.f, 24, 0.f},
		{50.f, 250.f, -glm::half_pi<float>() * 0.5f, glm::half_pi<float>() * 0.5f, 20.0f, 70.0f, 0.f, 250.f, 0.05f, 1.f, 0.f, 10.f, 10.f, 350, 5.f, 5.f, 15.f, 16, 0.f}
	};
}