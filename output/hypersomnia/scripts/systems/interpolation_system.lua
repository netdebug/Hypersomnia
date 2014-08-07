interpolation_system = inherits_from (processing_system)

function interpolation_system:get_required_components()
	return { "interpolation" }
end

function interpolation_system:update()
	local msgs = self.owner_entity_system.messages["CURRENT_STEP"]
	
	local new_step = false
	local at_step_sequence;
	
	for i=1, #msgs do
		new_step = true
		at_step_sequence = msgs[i].data.at_step
	end
	
	--if new_step then
	for i=1, #self.targets do
		local target = self.targets[i]
		
		local movement_module = self.targets[i].replication.modules.movement
		if movement_module == nil then
			movement_module = self.targets[i].replication.modules.movement_rotated
		end
		
		if movement_module ~= nil then
			local new_position = movement_module.position
			local new_velocity = movement_module.velocity
			local new_angle = movement_module.angle
			
			if new_angle == nil then
				new_angle = 0
			end
			
			if new_position ~= nil then self.targets[i].cpp_entity.physics.body:SetTransform(new_position, new_angle) end
			if new_velocity ~= nil then self.targets[i].cpp_entity.physics.body:SetLinearVelocity(new_velocity) end
		end
		
	end
	--end
end

