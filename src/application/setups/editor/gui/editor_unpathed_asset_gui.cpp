#define INCLUDE_TYPES_IN 1

#include "augs/string/string_templates.h"

#include "augs/templates/introspection_utils/field_name_tracker.h"
#include "augs/templates/introspection_utils/introspect_with_containers.h"

#include "augs/misc/imgui/imgui_scope_wrappers.h"
#include "augs/misc/imgui/imgui_control_wrappers.h"
#include "augs/misc/imgui/path_tree_structs.h"

#include "game/organization/for_each_entity_type.h"

#include "application/setups/editor/gui/editor_unpathed_asset_gui.h"
#include "application/setups/editor/editor_folder.h"
#include "application/intercosm.h"

#include "application/setups/editor/detail/other_styles.h"
#include "application/setups/editor/detail/format_struct_name.h"
#include "application/setups/editor/property_editor/general_edit_properties.h"
#include "application/setups/editor/detail/find_locations_that_use.h"
#include "application/setups/editor/detail/checkbox_selection.h"
#include "application/setups/editor/detail/duplicate_delete_buttons.h"
#include "application/setups/editor/property_editor/widgets/asset_sane_default_provider.h"
#include "application/setups/editor/property_editor/compare_all_fields_to.h"
#include "application/setups/editor/property_editor/special_widgets.h"
#include "application/setups/editor/property_editor/widgets/pathed_asset_widget.h"
#include "augs/readwrite/byte_readwrite.h"

#include "augs/templates/list_utils.h"

template <class id_type>
struct unpathed_asset_entry {
	std::string name;
	id_type id;

	std::vector<std::string> using_locations;

	bool used() const {
		return using_locations.size() > 0;
	}

	auto tie() const {
		return std::tie(name, id.indirection_index, id.version);
	}

	bool operator<(const unpathed_asset_entry& b) const {
		return tie() < b.tie();
	}
};

template <class asset_id_type>
void editor_unpathed_asset_gui<asset_id_type>::perform(
	const property_editor_settings& settings,
   	const editor_command_input cmd_in
) {
	using namespace augs::imgui;

	auto window = base::make_scoped_window();

	if (!window) {
		return;
	}

	auto& folder = cmd_in.folder;

	using asset_entry_type = unpathed_asset_entry<asset_id_type>;

	thread_local std::vector<asset_entry_type> orphaned_assets;
	thread_local std::vector<asset_entry_type> used_assets;

	orphaned_assets.clear();
	used_assets.clear();

	auto is_ticked = [this](const auto& p) {
		return found_in(ticked_assets, p.id);
	};

	auto get_all_ticked_and_existing = [&](auto in_range) {
		erase_if(in_range, [&] (const auto& candidate) { return !is_ticked(candidate); } );
		return in_range;
	};

	auto& definitions = get_asset_pool<asset_id_type>(cmd_in);
	using def_type = typename remove_cref<decltype(definitions)>::mapped_type;

	const auto label = uncapitalize_first(format_field_name(get_type_name_strip_namespace<def_type>()));

	for_each_id_and_object(definitions,
		[&](const auto id, const def_type& object) mutable {
			asset_entry_type new_entry;
			new_entry.id = id;
			new_entry.name = get_displayed_name(object, get_asset_pool<assets::image_id>(cmd_in));

			find_locations_that_use(id, *folder.work, [&](const auto& location) {
				new_entry.using_locations.push_back(location);
			});

			(new_entry.used() ? used_assets : orphaned_assets).emplace_back(std::move(new_entry));
		}
	);

	checkbox("Show orphaned", show_orphaned);

	ImGui::Separator();

	thread_local ImGuiTextFilter filter;
	filter.Draw();

	base::acquire_keyboard_once();

	auto for_each_range = [](auto callback) {
		callback(orphaned_assets);
		callback(used_assets);
	};

	auto prepare = [&](auto& range) {
		erase_if(range, [&](const auto& entry) {
			const auto& displayed_name = entry.name;

			if (!filter.PassFilter(displayed_name.c_str())) {
				return true;
			}

			return false;
		});

		sort_range(range);
	};

	for_each_range(prepare);

	auto files_view = scoped_child("Assets view");

	const auto num_cols = 3;

	ImGui::Columns(num_cols);

	auto do_asset = [&](const auto& asset_entry, const auto& ticked_in_range, const auto& ticked_ids) {
		(void)ticked_in_range;

		const auto id = asset_entry.id;
		auto scope = scoped_id(id);
		const auto& displayed_name = asset_entry.name;

		const auto current_ticked = is_ticked(asset_entry);

		const auto flags = do_selection_checkbox(ticked_assets, id, current_ticked, id);

		duplicate_delete_buttons<
			duplicate_asset_command<asset_id_type>,
			forget_asset_id_command<asset_id_type>
		> (cmd_in, id, settings, "", !asset_entry.used());

		if (nullptr == mapped_or_nullptr(definitions, id)) {
			/* It has just been deleted. */
			return;
		}

		const auto node = scoped_tree_node_ex(displayed_name + "###Node", flags);

		next_columns(2);

		if (asset_entry.used()) {
			const auto& using_locations = asset_entry.using_locations;

			if (auto node = scoped_tree_node(typesafe_sprintf("%x locations###locations", using_locations.size()).c_str())) {
				for (const auto& l : using_locations) {
					text(l);
				}
			}
		}
		else {
			text_disabled("(Nowhere)");
		}

		ImGui::NextColumn();

		if (node) {
			auto sc = scoped_indent();

			const auto property_location = typesafe_sprintf(" (in %x)", displayed_name);

			using command_type = change_asset_property_command<asset_id_type>;

			auto post_new_change = [&](
				const auto& description,
				const auto field_id,
				const auto& new_content
			) {
				command_type cmd;

				if (current_ticked) {
					cmd.affected_assets = ticked_ids;
				}
				else {
					cmd.affected_assets = { id };
				}

				cmd.property_id.field = field_id;
				cmd.value_after_change = augs::to_bytes(new_content);
				cmd.built_description = description + property_location;

				post_editor_command(cmd_in, std::move(cmd));
			};

			auto rewrite_last_change = [&](
				const auto& description,
				const auto& new_content
			) {
				auto& last = cmd_in.folder.history.last_command();

				if (auto* const cmd = std::get_if<command_type>(std::addressof(last))) {
					cmd->built_description = description + property_location;
					cmd->rewrite_change(augs::to_bytes(new_content), cmd_in);
				}
				else {
					LOG("WARNING! There was some problem with tracking activity of editor controls.");
				}
			};

			auto prop_in = property_editor_input { settings, property_editor_data };
			const auto& project_path = cmd_in.folder.current_path;
			auto& viewables = folder.work->viewables;

			general_edit_properties(
				prop_in,
				definitions[id],
				post_new_change,
				rewrite_last_change,
				[&](const auto& first, const field_address field_id) {
					if (!current_ticked) {
						return true;
					}

					return compare_all_fields_to(
						first,
						asset_property_id<asset_id_type> { field_id }, 
						cmd_in, 
						ticked_ids
					);
				},
				special_widgets(
					pathed_asset_widget { viewables, project_path, cmd_in }
				),
				asset_sane_default_provider { viewables },
				num_cols - 2
			);
		}
	};

	int s = 0;
	
	auto do_section = [&](
		const auto& entries,
		const std::array<std::string, 2> labels,
		const std::optional<rgba> color = std::nullopt
	) {
		if (entries.empty()) {
			return;
		}

		{
			do_tick_all_checkbox(
				settings,
				ticked_assets,
				[&entries](auto callback) {
					for (const auto& p : entries) {
						callback(p.id);
					}
				},
				s++
			);

			if (color) {
				text_color(labels[0], *color);
			}
			else {
				text_disabled(labels[0]);
			}
		}

		ImGui::NextColumn();
		text_disabled("Properties");

		ImGui::NextColumn();
		text_disabled(labels[1]);
		ImGui::NextColumn();
		ImGui::Separator();

		const auto ticked_and_existing = get_all_ticked_and_existing(entries);

		thread_local std::vector<asset_id_type> ticked_ids;
		ticked_ids.clear();

		for (const auto& p : ticked_and_existing) {
			ticked_ids.push_back(p.id);
		}

		for (const auto& p : entries) {
			do_asset(p, ticked_and_existing, ticked_ids);
		}

		ImGui::Separator();
	};

	if (show_orphaned) {
		do_section(
			orphaned_assets,
			{ typesafe_sprintf("Orphaned %xs", label), "Used at" }
		);
	}

	do_section(
		used_assets,
		{ capitalize_first(std::string(label)) + "s", "Used at" }
	);
}

template struct editor_unpathed_asset_gui<assets::plain_animation_id>;
template struct editor_unpathed_asset_gui<assets::torso_animation_id>;
template struct editor_unpathed_asset_gui<assets::legs_animation_id>;