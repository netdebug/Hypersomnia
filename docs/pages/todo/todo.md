---
title: ToDo
tags: [planning]
hide_sidebar: true
permalink: todo
summary: Just a hidden scratchpad.
---

- maybe don't save the last launch unless in debug

- problem: footstep smokes might still be visible when we artificially increase zoom
	- well the client itself knows everything anyways
	- so we can force the zoom to be exact
		- if someone has more than 1920x1080 they technically have the advantage but the footsteps would still be hearable
			- one could still see some distant items thrown but come on

- ImGui popup for progress on loading atlases
	- the imgui atlas itself will load quickly enough

- Initial popup about the zoom change
	- or scale automatically to the server's fog of war size?

- Consider implementing a radius range for item pickups

- Shouldn't we always calc reloading movement to show context-lessly?


- Force proper drawing order of attachments as this really looks bad on the rocket launcher
	- At least on guns?

- Lag estimates for other players
	- send a byte for count, and a byte for each player
		- they can be consecutive and we don't have to include ids
		- the order of existent ids implicitly known
	- can be sent over the solvable stream
		- later we could introduce advantages for players with higher ping, although may not necessarily be good idea

- Detect when the initial state is being sent to optionally display progress
	- Possibly by returning some info from ReceiveBlockData struct

- Properly implement bots
	- Now they can break the client logic

- large block sending augs api
	- asynchronous: deliver_large_data
	- large_data_delivered() const
	- always one large data is sent at a time

- enum-less variants of enum_boolset and enum_map
	- useful for quick mapping users in an array

- So that we can change tickrate dynamically, timing logic can, instead of steps, simply hold the floating point (double) values of the seconds passed
	- alternatively hold unsigned long longs?
	- also store last_fixed_delta for easy cosmos::get_fixed_delta access

- Usability fixes to editor 
	- Setting specific names to entities
	- Complex instantiations
		- I struggle to see any that are necessary before DM
			- even manual transfers arent that necessary
		- Guns with magazines already
		- For now expose transfer GUI?

- enum_boolset begin and end should probably return enums
	- simpler lua i/o
	- no need for for-eaches
	- nice exercise for iterators

- When joining a game, allow choosing the flavour of the character
	- Each might have completely different properties or animations
	- Effectively implements a class

- Arbitrary pasting of entities
	- Vastly useful for importing stuff from testbed maps into existing ones
		- Let alone between community maps
	- This is pretty complex.
		- If we allow pasting across workspaces, we might even have to copy actual asset files.
	- Won't matter until after deathmatch stage
		- Surely?
	- The editor will have to construct the command tree, like
		- paste_flavours + paste_entities, if no requisite flavours are found inside the project at the time of pasting
			- the clipboard will have both the entity and flavour
		- the editor's clipboard can actually become...
			- paste entity flavour + paste entity command, stored, waiting to be executed!
				- the move itself won't need to be stored
	- Cut is just copy + delete

- Allow to change the tickrate in the non-playtesting?
	- This will be a session setting, really
	- We'll just restart on re-tick

- Overwrite notice in editor

- Refactor: conditional log macro

- Always initialize the hotbar with some values if it is not yet initialized

- Commands refactor: separation of undo and redo state
	- redoer and undoer objects
		- each has only the required information
		- redoer returns an undoer
		- redo state is always the same

- Balance colliding bullets so that damage to be dealt is subtracted from the stronger bullet
	- Possibly reduce trace size then

- Local setup should record session live
	- This is really important in the face of bugs.
	- Or just get rid of test scene setup for now and let it by default launch a scene in editor that records inputs

- Fix rendering order of dropped gun attachments
	- Also make it possible to always render them under?

- consider having entity guids in components instead of entity ids for simplicity of network transfers
	- question is, won't we anyway be sending the whole pool state?
		- it's just several bytes overhead per entity
	- there ain't really that many and it will be greatly useful

- Fill new workspace with test scene essentials
	- This would prevent image ids in common state from being invalid
		- Probably less checks to make, in GUI as well
	- Can make those the first in test scene image enums so that we can stop importing images after some point

- copy gfx and sfx folders on save as...

- let particle definitions be split into the invariant and variant parts, like components
	- pro: better cache coherency

- templatize some dangling memory stream arguments

- in go to dialog, make selection groups appear as the first
	- later we might just make a variant of several types instead of entity_guid in match vector

- fix saving of the editor view state when the work is saved to some non-untitled location
	- notice that, we might later introduce some caches for selections to improve performance
		- e.g. only always calculate the selection's aabb and rotation centers once
	- we might also want to encapsulate setting panning cameras and selections to have stacks
		- where we'll able to set modification flags easily
	- **therefore let's do it when we get to these stacks.**
		- because camera & selection state isn't all that critical
			- and untitled work for testing is always saved anyway
	- we can just always write the camera to disk?
		- it's bad though as it incurs disk activity every time
		- and selection state isn't that small
	- we could either set an "is_modified" flag or track it in history
		- not much modification points for selections, so quite viable

- pass display size and implement augs::get_display

- some hint for windows unicode in imgui: https://github.com/ocornut/imgui/issues/1060

- finish work with atlas and sound regeneration
	- regenerate only seen assets
	- always load diffuse map with the corresponding neon map to avoid unilluminated objects near the camera...
		- ...even though many diffuse maps nearby have been loaded
		- perhaps we won't really need the separation between diffuse and neon, because maximum atlases will be huge anyway

- Shake on shooting with a sniper rifle

- Dashing
	- Assigned to space, since we don't have jumping anyway
	- pure color highlight system could be used to add highlight the dashing entity
	- Gradually increase walking force over time
	- The more the speed during dash, the stronger the dash

- Note: drone sound (sentience sounds overall) will be calculated exactly as the firing engine sound
	- Sentience humming caches
	- These don't need their playing pos synchronized

- Since mouse motions will probably be the bottleneck of network communication, both coords will usually be able to be sent in one byte
	- Range would be 0 - 16
	- There would be a bit flag for when it exceeds the range
		- Which will actually be often and then we might use two bytes
			- mouse motion should never really exceed 255
				- will also make it harder to use aimbots that instantaneously change mouse location

