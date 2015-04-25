-- gets map filename and scene object to save global entities like players/cameras

return function(map_filename, scene_object)
	-- setup shortcut
	local world = scene_object.world_object
	
	scene_object.simulation_world = simulation_world_class:create()
	
	-- load map data
	scene_object.resource_storage = {}
	local objects_by_type, type_table_by_object = tiled_map_loader.get_all_objects_by_type(map_filename)

	-- helper function for getting all objects of given type
	local function get_all_objects(entries)
		local sum_of_all = {}
		for i = 1, #entries do
			sum_of_all = table.concatenate( { sum_of_all, objects_by_type[entries[i]] } )
		end
		
		return sum_of_all
	end
	
	create_particle_effects(scene_object)
	
	local function basic_table(object)
		return tiled_map_loader.basic_entity_table(object, type_table_by_object[object], scene_object.resource_storage, scene_object.world_camera, scene_object.texture_by_filename)
	end
	
	local background_objects = get_all_objects { "ground_snow" }
	for i = 1, #background_objects do
		local object = background_objects[i]
		world:create_entity (basic_table(object))
	end
	
	-- initialize environmental physical objects
	local environmental_objects = get_all_objects { "static_snow" }
	
	for i = 1, #environmental_objects do
		local object = environmental_objects[i]
		
		local new_entity = basic_table(object)
		
		new_entity.particle_emitter = {
			available_particle_effects = scene_object.particles.metal_effects
		}
		
		world:create_entity (new_entity)
		scene_object.simulation_world:create_entity {
			transform = new_entity.transform,
			physics = new_entity.physics
		}
	end
	
	scene_object:load_tile_functionality(map_filename)

	local tile_layers = tiled_map_loader.for_every_object(map_filename, nil, function(tile_layer_table)
		local new_model = scene_object:generate_tile_layer(tile_layer_table)
		
		world:create_entity {
			render = {
				model = new_model,
				layer = render_layers["GROUND"]
			},

			transform = {}
		}

	end)
	
	world.physics_system.enable_interpolation = 1
	world.physics_system:configure_stepping(config_table.tickrate, 5)
	scene_object.simulation_world.physics_system:configure_stepping(config_table.tickrate, 5)
	
	-- initialize input
	world.input_system:clear_contexts()
	world.input_system:add_context(main_input_context)
	world.input_system:add_context(gui_input_context)
	
	-- initialize camera
	scene_object.world_camera = create_world_camera_entity(world, scene_object.sprite_library["blank"])
	scene_object.world_camera.script.owner_scene = scene_object
	
	--if config_table.multiple_clients_view == 1 then
	--	scene_object.world_camera.script.min_zoom = -400
	--	scene_object.world_camera.script:set_zoom_level(-400)
	--end
	
	-- initialize player
	scene_object.teleport_position = vec2(0, 0)--objects_by_type["teleport_position"][1].pos
	
	scene_object.crosshair_sprite = create_sprite {
		image = scene_object.sprite_library["crosshair"],
		color = rgba(0, 255, 255, 255)
	}
	
	scene_object.bullet_sprite = create_sprite {
		image = scene_object.sprite_library["bullet"],
		size_multiplier = vec2(1, 1)
	}
	
	
	scene_object.legs_sets = create_all_legs_sets(scene_object.sprite_library)
	scene_object.torso_sets = create_all_torso_sets(scene_object.sprite_library)
	
	scene_object.simulation_player = create_simulation_player(scene_object.simulation_world)
	
	scene_object.main_input = world:create_entity {
		input = {
			custom_intents.SWITCH_CLIENT_1,
			custom_intents.SWITCH_CLIENT_2,
			custom_intents.SWITCH_CLIENT_3,
			custom_intents.SWITCH_CLIENT_4,
			
			custom_intents.QUIT
		},
		
		script = {
			intent_message = function(self, message)
				if message.intent == custom_intents.QUIT then
					SHOULD_QUIT_FLAG = true
				elseif message.intent == custom_intents.SWITCH_CLIENT_1 then
					set_active_client(1)
				elseif message.intent == custom_intents.SWITCH_CLIENT_2 then
					set_active_client(2)
				elseif message.intent == custom_intents.SWITCH_CLIENT_3 then
					set_active_client(3)
				elseif message.intent == custom_intents.SWITCH_CLIENT_4 then
					set_active_client(4)
				end		
			end
		}
	}
	
	local all_sound_files = get_all_files_in_directory("hypersomnia\\data\\sfx")
	local sound_by_filename = {}
	local sound_library = {}
	
	for k, v in pairs(all_sound_files) do
		local sound_object = create_sound("hypersomnia\\data\\sfx\\" .. v)
		
		sound_by_filename[k] = sound_object
		
		-- tokenize filename to only get the filename and the extension
		local tokenized = tokenize_string(v, "\\/")
		
		-- the last token is just filename + extension
		save_resource_in_item_library(tokenized[#tokenized], sound_object, sound_library)
	end
	
	scene_object.sound_library = sound_library
	scene_object.sound_by_filename = sound_by_filename
	
	-- bind the atlas once
	-- GL.glActiveTexture(GL.GL_TEXTURE0)
	-- scene_object.all_atlas:bind()
	-- now have to bind every time because rendering several clients
end