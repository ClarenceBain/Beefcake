#include "luahook.h"
#include "libmem/libmem.hpp"
#include "consolecolors.h"

// simple function for logging hex values
template<class... Args>
void logh(const char* text, Args... args) {
	std::cout << std::hex << dye::aqua("[BEEF] ") << text;
	(std::cout << ... << args) << std::dec << std::endl;
}

// simple function for logging number values
template<class... Args>
void logd(const char* text, Args... args) {
	std::cout << dye::aqua("[BEEF] ") << text;
	(std::cout << ... << args) << std::endl;
}

// function to write an array of bytes to memory using libmem
// maybe i'm stupid but I couldnt get mem::bytearray_t or mem::data_t to work, had to make a work-around with the to_array function
void writeAOB(uintptr_t address, mem::byte_t* aob) {															

	// store the addresses current page protection
	mem::prot_t original_protection = mem::in::get_page((mem::voidptr_t)address).protection;						
	// change the addresses current page protection to 0x80 (read, write, & execute)
	mem::in::protect((mem::voidptr_t)address, (mem::size_t)(mem::voidptr_t)address, PAGE_EXECUTE_READWRITE);

	// some addresses for some reason require VirtualProtectEx even though we are injected inside, simple solution is to
	// confirm if the protection has changed or not
	if (mem::in::get_page((mem::voidptr_t)address).protection == original_protection)									
	{
		// if the page protection is still equal to the original protection, we need to use VirtualProtectEx (aka mem::ex)
		// VirtualProtectEx requires a process
		mem::process_t proc = mem::ex::get_process(GetCurrentProcessId());											
		// change the addresses current page protection to 0x80 (read, write, & execute)
		// it would probably be smart to have another check to confirm page has changed, but everything works as intended
		mem::ex::protect(proc, (mem::voidptr_t)address, (mem::size_t)(mem::voidptr_t)address, PAGE_EXECUTE_READWRITE);
		for (int i = 0; i < sizeof(aob); i++)
		{
			// loop though given aob and write to address(s)
			// this actually doesn't support above 9 bytes, though nothing we are writing is this large so it isn't necessary
			mem::ex::write<mem::byte_t>(proc, (mem::voidptr_t)(address + i), (mem::byte_t)aob[i]);							
		}
		// restore page protection to its original value
		mem::ex::protect(proc, (mem::voidptr_t)address, (mem::size_t)(mem::voidptr_t)address, original_protection);		
	}
	else {
		// page protection has successfully changed, use VirtualProtect (aka mem::in)
		for (int i = 0; i < sizeof(aob); i++)
		{
			// loop though given aob and write to address(s)
			// this actually doesn't support above 9 bytes, though nothing we are writing is this large so it isn't necessary
			mem::in::write<mem::byte_t>((mem::voidptr_t)(address + i), (mem::byte_t)aob[i]);
		}
		// restore page protection to its original value
		mem::in::protect((mem::voidptr_t)address, (mem::size_t)(mem::voidptr_t)address, original_protection);		
	}
}

// function to read a pointer from an address
mem::uintptr_t readPTR(uintptr_t address)
{
	// value to store our pointer
	mem::uintptr_t value = 0;
	mem::prot_t original_protection = mem::in::get_page((mem::voidptr_t)address).protection;							
	mem::in::protect((mem::voidptr_t)address, (mem::size_t)(mem::voidptr_t)address, PAGE_EXECUTE_READWRITE);			
	if (mem::in::get_page((mem::voidptr_t)address).protection == original_protection)								
	{
		// again, we need to use VirtualProtectEx
		mem::process_t proc = mem::ex::get_process(GetCurrentProcessId());								
		mem::ex::protect(proc, (mem::voidptr_t)address, (mem::size_t)(mem::voidptr_t)address, PAGE_EXECUTE_READWRITE);
		// lets get the pointer value from the address
		value = mem::ex::read<mem::uintptr_t>(proc, (mem::voidptr_t)address);
		mem::ex::protect(proc, (mem::voidptr_t)address, (mem::size_t)(mem::voidptr_t)address, original_protection);
	}
	else {
		// lets get the pointer value from the address
		value = mem::in::read<mem::uintptr_t>((mem::voidptr_t)address);
		mem::in::protect((mem::voidptr_t)address, (mem::size_t)(mem::voidptr_t)address, original_protection);
	}
	// return the pointer address
	return value;
}

// function to write an integer to address value
// uncommented, explained twice already in readPTR, and writeAOB
void writeINT(uintptr_t address, int value)
{
	mem::prot_t original_protection = mem::in::get_page((mem::voidptr_t)address).protection;
	mem::in::protect((mem::voidptr_t)address, (mem::size_t)(mem::voidptr_t)address, PAGE_EXECUTE_READWRITE);
	if (mem::in::get_page((mem::voidptr_t)address).protection == original_protection)
	{
		mem::process_t proc = mem::ex::get_process(GetCurrentProcessId());
		mem::ex::protect(proc, (mem::voidptr_t)address, (mem::size_t)(mem::voidptr_t)address, PAGE_EXECUTE_READWRITE);
		mem::ex::write<mem::int32_t>(proc, (mem::voidptr_t)address, (mem::int32_t)value);
		mem::ex::protect(proc, (mem::voidptr_t)address, (mem::size_t)(mem::voidptr_t)address, original_protection);
	}
	else {
		mem::in::write<mem::int32_t>((mem::voidptr_t)address, (mem::int32_t)value);
		mem::in::protect((mem::voidptr_t)address, (mem::size_t)(mem::voidptr_t)address, original_protection); 
	}
}

// function to convert std::vector<mem::byte_t> to mem::byte_t*
mem::byte_t* to_array(std::vector<mem::byte_t> aob)
{
	// create an array of bytes with the size of the byte_t vector argument
	mem::byte_t naob[sizeof(aob)];
	for (int i = 0; i < sizeof(aob); i++)
	{
		// add the contents of aob vector to the naob array
		naob[i] = aob[i];
	}
	// return the byte array
	return naob;
}

void luahook::luahook_init(){
	luahook::noita::first = NULL;
	// normally wouldn't hardcode a base, but it never changes so
	luahook::noita::noitabase = 0x00400000;
	luahook::noita::security_cookie = luahook::noita::noitabase + 0xB3A000;	
	logh("Security Cookie found at: ", luahook::noita::security_cookie);
	luahook::noita::seed = luahook::noita::noitabase + 0xBEE850;
	logh("World Seed found at: ", luahook::noita::seed);
	luahook::noita::noitalibrary_init = luahook::noita::noitabase + 0x318F00;
	logh("Noita Functions Library found at: ", luahook::noita::noitalibrary_init);
	luahook::noita::mapi_check = luahook::noita::noitabase + 0x23D89B;
	//luahook::noita::restriction_checkifmod = luahook::noita::noitabase + 0xA3A276;
	luahook::noita::lib = (luahook::noita::nlib)luahook::noita::noitalibrary_init;	

	luahook::lua::luabase = (uintptr_t)GetModuleHandleA("lua51.dll");
	logh("lua51.dll base found at: ", luahook::lua::luabase);
	logh("Locating required lua functions..");
	luahook::lua::call_a =	luahook::lua::luabase + 0x6870;
	luahook::lua::error_a = luahook::lua::luabase + 0x23030;
	luahook::lua::getfield_a = luahook::lua::luabase + 0x6C90;
	luahook::lua::gettop_a = luahook::lua::luabase + 0x6E20;
	luahook::lua::iscfunction_a = luahook::lua::luabase + 0x6F00;
	luahook::lua::isnumber_a = luahook::lua::luabase + 0x6F30;
	luahook::lua::isstring_a = luahook::lua::luabase + 0x6F80;
	luahook::lua::loadstring_a = luahook::lua::luabase + 0x29C20;
	luahook::lua::newstate_a = luahook::lua::luabase + 0x3E150;
	luahook::lua::newthread_a = luahook::lua::luabase + 0x7070;
	luahook::lua::pcall_a = luahook::lua::luabase + 0x71F0;
	luahook::lua::pushcclosure_a = luahook::lua::luabase + 0x72A0;
	luahook::lua::pushboolean_a = luahook::lua::luabase + 0x7270;
	luahook::lua::pushinteger_a = luahook::lua::luabase + 0x7360;
	luahook::lua::pushnumber_a = luahook::lua::luabase + 0x7430;
	luahook::lua::pushstring_a = luahook::lua::luabase + 0x7470;
	luahook::lua::pushvalue_a = luahook::lua::luabase + 0x7520;
	luahook::lua::rawgeti_a = luahook::lua::luabase + 0x7610;
	luahook::lua::ref_a = luahook::lua::luabase + 0x43020;
	luahook::lua::register_a = luahook::lua::luabase + 0x430D0;
	luahook::lua::resume_a = luahook::lua::luabase + 0x7890;
	luahook::lua::setfield_a = luahook::lua::luabase + 0x79A0;
	luahook::lua::settop_a = luahook::lua::luabase + 0x7BF0;
	luahook::lua::toboolean_a = luahook::lua::luabase + 0x7D00;
	luahook::lua::tocfunction_a = luahook::lua::luabase + 0x7D20;
	luahook::lua::tointeger_a = luahook::lua::luabase + 0x7D50;
	luahook::lua::tolstring_a = luahook::lua::luabase + 0x7DB0;
	luahook::lua::tonumber_a = luahook::lua::luabase + 0x7E30;
	luahook::lua::unref_a = luahook::lua::luabase + 0x430F0;
	luahook::lua::xmove_a = luahook::lua::luabase + 0x7FF0;
	// these counts are hardcoded, only useful for debugging
	logd("Functions found: ", 30);											
	logh("Locating restricted lua libraries..");
	luahook::lua::bit_a = luahook::lua::luabase + 0x44410;
	luahook::lua::debug_a = luahook::lua::luabase + 0x45050;
	luahook::lua::ffi_a = luahook::lua::luabase + 0x46A80;
	luahook::lua::io_a = luahook::lua::luabase + 0x47A80;
	luahook::lua::jit_a = luahook::lua::luabase + 0x48C10;
	luahook::lua::os_a = luahook::lua::luabase + 0x49980;
	// these counts are hardcoded, only useful for debugging
	logd("Libraries found: ", 6);
	logh("Setting up functions..");
	luahook::lua::lua_call = (luahook::lua::lua51_call)luahook::lua::call_a;
	luahook::lua::luaL_error = (luahook::lua::luaL51_error)luahook::lua::error_a;
	luahook::lua::lua_getfield = (luahook::lua::lua51_getfield)luahook::lua::getfield_a;
	luahook::lua::lua_gettop = (luahook::lua::lua51_gettop)luahook::lua::gettop_a;
	luahook::lua::luaL_loadstring = (luahook::lua::luaL51_loadstring)luahook::lua::loadstring_a;
	luahook::lua::lua_pcall = (luahook::lua::lua51_pcall)luahook::lua::pcall_a;
	luahook::lua::lua_pushcclosure = (luahook::lua::lua51_pushcclosure)luahook::lua::pushcclosure_a;
	luahook::lua::lua_pushboolean = (luahook::lua::lua51_pushboolean)luahook::lua::pushboolean_a;
	luahook::lua::lua_pushinteger = (luahook::lua::lua51_pushinteger)luahook::lua::pushinteger_a;
	luahook::lua::lua_pushnumber = (luahook::lua::lua51_pushnumber)luahook::lua::pushnumber_a;
	luahook::lua::lua_pushstring = (luahook::lua::lua51_pushstring)luahook::lua::pushstring_a;
	luahook::lua::lua_pushvalue = (luahook::lua::lua51_pushvalue)luahook::lua::pushvalue_a;
	luahook::lua::lua_rawgeti = (luahook::lua::lua51_rawgeti)luahook::lua::rawgeti_a;
	luahook::lua::luaL_register = (luahook::lua::luaL51_register)luahook::lua::register_a;
	luahook::lua::lua_resume = (luahook::lua::lua51_resume)luahook::lua::resume_a;
	luahook::lua::lua_setfield = (luahook::lua::lua51_setfield)luahook::lua::setfield_a;
	luahook::lua::lua_settop = (luahook::lua::lua51_settop)luahook::lua::settop_a;
	luahook::lua::lua_toboolean = (luahook::lua::lua51_toboolean)luahook::lua::toboolean_a;
	luahook::lua::lua_tocfunction = (luahook::lua::lua51_tocfunction)luahook::lua::tocfunction_a;
	luahook::lua::lua_tointeger = (luahook::lua::lua51_tointeger)luahook::lua::tointeger_a;
	luahook::lua::lua_tolstring = (luahook::lua::lua51_tolstring)luahook::lua::tolstring_a;
	luahook::lua::lua_tonumber = (luahook::lua::lua51_tonumber)luahook::lua::tonumber_a;
	luahook::lua::lua_xmove = (luahook::lua::lua51_xmove)luahook::lua::xmove_a;
	logh("Setting up libraries..");
	luahook::lua::luaopen_bit = (luahook::lua::lib51_bit)luahook::lua::bit_a;
	luahook::lua::luaopen_debug = (luahook::lua::lib51_debug)luahook::lua::debug_a;
	luahook::lua::luaopen_ffi = (luahook::lua::lib51_ffi)luahook::lua::ffi_a;
	luahook::lua::luaopen_io = (luahook::lua::lib51_io)luahook::lua::io_a;
	luahook::lua::luaopen_jit = (luahook::lua::lib51_jit)luahook::lua::jit_a;
	luahook::lua::luaopen_os = (luahook::lua::lib51_os)luahook::lua::os_a;
	Sleep(1000);
	logh("Hooking Noita Functions Library..");
	// hook the original library function with nlib_hook, takes 9 bytes
	// in a lot of cases you would normally restore the detour, in this case the function is called during every new lua state from mods, noita, luacomponents, etc..
	luahook::noita::lib = (luahook::noita::nlib)mem::in::detour_trampoline((mem::voidptr_t)luahook::noita::lib, (mem::voidptr_t)luahook::noita::nlib_hook, 9, mem::MEM_DT_M1);
	logh("Noita Functions Library hooked at: ", luahook::noita::noitalibrary_init, " with ", luahook::noita::nlib_hook);
	int p = 0;
LOOP:
	// lets get the pointer address of MAPITable
	// mostly initiated on game load, sometimes before
	uintptr_t address = readPTR(luahook::noita::noitabase + 0xBEE860);
	if (address == NULL)
	{
		if (p == 0)
		{
			logh("Waiting for MAPITable to initiate..");
			p = 1;
		}
		goto LOOP;
	}
	else {
		logh("MAPITable found: ", address);
		logh("IsModded variable found: ", address + 0x00000120);
		logh("Changing mod status..");
		// once mods are enabled, mod status is set every frame
		// we need to disable the every frame check, then change our status to 0 (unmodded)
		writeAOB(luahook::noita::mapi_check, to_array({ 0x71, 0x11 }));
		writeINT(address + 0x00000120, 0);
		// another hardcoded count, only really useful for debugging
		logd("Mod status changed, bytes patched: ", 6);
	}
}

// excluding nlib_hook, these are lua c functions and i'll likely not comment these as they read pretty straight forward
int luahook::beefcake::GetPlayer(lua_State* L) {
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "EntityGetWithTag");
	luahook::lua::lua_pushstring(L, "player_unit");
	if (luahook::lua::lua_pcall(L, 1, 1, 0) != 0)
		return 0;
	luahook::lua::lua_rawgeti(L, -1, 1);
	return 1;
}

int luahook::beefcake::SetWorldTime(lua_State* L) {
	if (luahook::lua::lua_gettop(L) != 1)
		return 0;
	double time = luahook::lua::lua_tonumber(L, 1);
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "GameGetWorldStateEntity");
	luahook::lua::lua_call(L, 0, 1);
	int world = (int)luahook::lua::lua_tonumber(L, -1);
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "EntityGetFirstComponent");
	luahook::lua::lua_pushinteger(L, world);
	luahook::lua::lua_pushstring(L, "WorldStateComponent");
	if (luahook::lua::lua_pcall(L, 2, 1, 0) != 0)
		return 0;
	int comp = (int)luahook::lua::lua_tonumber(L, -1);
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "ComponentSetValue2");
	luahook::lua::lua_pushinteger(L, comp);
	luahook::lua::lua_pushstring(L, "time");
	luahook::lua::lua_pushnumber(L, time);
	if (luahook::lua::lua_pcall(L, 3, 0, 0) != 0)
		return 0;
	luahook::lua::lua_settop(L, 0);
	return 0;
}

int luahook::beefcake::ExecuteThroughLoader(lua_State* L)
{
	if (luahook::lua::lua_gettop(L) != 1)
		return 0;
	const char* script = luahook::lua::lua_tolstring(L, 1, NULL);
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "EntityGetWithTag");
	luahook::lua::lua_pushstring(L, "player_unit");
	if (luahook::lua::lua_pcall(L, 1, 1, 0) != 0)
		return 0;
	luahook::lua::lua_rawgeti(L, -1, 1);
	int player = (int)luahook::lua::lua_tonumber(L, -1);
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "EntityGetFirstComponent");
	luahook::lua::lua_pushinteger(L, player);
	luahook::lua::lua_pushstring(L, "LuaComponent");
	if (luahook::lua::lua_pcall(L, 2, 1, 0) != 0)
		return 0;
	int lua = (int)luahook::lua::lua_tonumber(L, -1);
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "ModTextFileSetContent");
	luahook::lua::lua_pushstring(L, "data/scripts/empty.lua");
	std::string skript = script;
	std::string encode = "\nComponentSetValue2(og_lua, 'enable_coroutines', '0'); ComponentSetValue2(og_lua, 'vm_type', 'ONE_PER_COMPONENT_INSTANCE'); ComponentSetValue2(og_lua, 'execute_every_n_frame', 80); ComponentSetValue2(og_lua, 'script_source_file', 'data/scripts/magic/player_biome_check.lua'); ModTextFileSetContent('data/scripts/empty.lua','');";
	std::string code = "local og_lua = EntityGetFirstComponent(LocalPlayer.GetID(),'LuaComponent');\n" + skript + encode;
	luahook::lua::lua_pushstring(L, code.c_str());
	if (luahook::lua::lua_pcall(L, 2, 0, 0) != 0)
		return 0;
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "ComponentSetValue2");
	luahook::lua::lua_pushinteger(L, lua);
	luahook::lua::lua_pushstring(L, "vm_type");
	luahook::lua::lua_pushstring(L, "CREATE_NEW_EVERY_EXECUTION");
	if (luahook::lua::lua_pcall(L, 3, 0, 0) != 0)
		return 0;
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "ComponentSetValue2");
	luahook::lua::lua_pushinteger(L, lua);
	luahook::lua::lua_pushstring(L, "enable_coroutines");
	luahook::lua::lua_pushinteger(L, 1);
	if (luahook::lua::lua_pcall(L, 3, 0, 0) != 0)
		return 0;
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "ComponentSetValue2");
	luahook::lua::lua_pushinteger(L, lua);
	luahook::lua::lua_pushstring(L, "script_source_file");
	luahook::lua::lua_pushstring(L, "data/scripts/empty.lua");
	if (luahook::lua::lua_pcall(L, 3, 0, 0) != 0)
		return 0;
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "ComponentSetValue2");
	luahook::lua::lua_pushinteger(L, lua);
	luahook::lua::lua_pushstring(L, "execute_every_n_frame");
	luahook::lua::lua_pushinteger(L, 1);
	if (luahook::lua::lua_pcall(L, 3, 0, 0) != 0)
		return 0;
	luahook::lua::lua_settop(L, 0);
	return 0;
}

int luahook::beefcake::GetPlayerPos(lua_State* L) {
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "EntityGetWithTag");
	luahook::lua::lua_pushstring(L, "player_unit");
	if (luahook::lua::lua_pcall(L, 1, 1, 0) != 0)
		return 0;
	luahook::lua::lua_rawgeti(L, -1, 1);
	int player = (int)luahook::lua::lua_tonumber(L, -1);
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "EntityGetTransform");
	luahook::lua::lua_pushinteger(L, player);
	if (luahook::lua::lua_pcall(L, 1, 2, 0) != 0)
		return 0;
	return 2;
}

int luahook::beefcake::GetPlayerHealth(lua_State* L) {
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "EntityGetWithTag");
	luahook::lua::lua_pushstring(L, "player_unit");
	if (luahook::lua::lua_pcall(L, 1, 1, 0) != 0)
		return 0;
	luahook::lua::lua_rawgeti(L, -1, 1);
	int player = (int)luahook::lua::lua_tonumber(L, -1);
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "EntityGetFirstComponent");
	luahook::lua::lua_pushinteger(L, player);
	luahook::lua::lua_pushstring(L, "DamageModelComponent");
	if (luahook::lua::lua_pcall(L, 2, 1, 0) != 0)
		return 0;
	int damage = (int)luahook::lua::lua_tonumber(L, -1);
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "ComponentGetValue2");
	luahook::lua::lua_pushinteger(L, damage);
	luahook::lua::lua_pushstring(L, "hp");
	if (luahook::lua::lua_pcall(L, 2, 1, 0) != 0)
		return 0;
	int health = (int)luahook::lua::lua_tonumber(L, -1);
	health = health * 25;
	luahook::lua::lua_pushinteger(L, health);
	return 1;
}

int luahook::beefcake::GetPlayerHealthM(lua_State* L) {
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "EntityGetWithTag");
	luahook::lua::lua_pushstring(L, "player_unit");
	if (luahook::lua::lua_pcall(L, 1, 1, 0) != 0)
		return 0;
	luahook::lua::lua_rawgeti(L, -1, 1);
	int player = (int)luahook::lua::lua_tonumber(L, -1);
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "EntityGetFirstComponent");
	luahook::lua::lua_pushinteger(L, player);
	luahook::lua::lua_pushstring(L, "DamageModelComponent");
	if (luahook::lua::lua_pcall(L, 2, 1, 0) != 0)
		return 0;
	int damage = (int)luahook::lua::lua_tonumber(L, -1);
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "ComponentGetValue2");
	luahook::lua::lua_pushinteger(L, damage);
	luahook::lua::lua_pushstring(L, "max_hp");
	if (luahook::lua::lua_pcall(L, 2, 1, 0) != 0)
		return 0;
	int health = (int)luahook::lua::lua_tonumber(L, -1);
	health = health * 25;
	luahook::lua::lua_pushinteger(L, health);
	return 1;
}

int luahook::beefcake::GetPlayerGold(lua_State* L) {
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "EntityGetWithTag");
	luahook::lua::lua_pushstring(L, "player_unit");
	if (luahook::lua::lua_pcall(L, 1, 1, 0) != 0)
		return 0;
	luahook::lua::lua_rawgeti(L, -1, 1);
	int player = (int)luahook::lua::lua_tonumber(L, -1);
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "EntityGetFirstComponent");
	luahook::lua::lua_pushinteger(L, player);
	luahook::lua::lua_pushstring(L, "WalletComponent");
	if (luahook::lua::lua_pcall(L, 2, 1, 0) != 0)
		return 0;
	int wallet = (int)luahook::lua::lua_tonumber(L, -1);
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "ComponentGetValue2");
	luahook::lua::lua_pushinteger(L, wallet);
	luahook::lua::lua_pushstring(L, "money");
	if (luahook::lua::lua_pcall(L, 2, 1, 0) != 0)
		return 0;
	return 1;
}

int luahook::beefcake::GenomeGetHerdId(lua_State* L) {
	if (luahook::lua::lua_gettop(L) != 1)
		return 0;
	int entity = luahook::lua::lua_tointeger(L, 1);
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "EntityGetFirstComponent");
	luahook::lua::lua_pushinteger(L, entity);
	luahook::lua::lua_pushstring(L, "GenomeDataComponent");
	if (luahook::lua::lua_pcall(L, 2, 1, 0) != 0)
		return 0;
	int gdc = luahook::lua::lua_tointeger(L, -1);
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "ComponentGetValue2");
	luahook::lua::lua_pushinteger(L, gdc);
	luahook::lua::lua_pushstring(L, "herd_id");
	if (luahook::lua::lua_pcall(L, 2, 1, 0) != 0)
		return 0;
	int id = luahook::lua::lua_tointeger(L, -1);
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "HerdIdToString");
	luahook::lua::lua_pushinteger(L, id);
	if (luahook::lua::lua_pcall(L, 1, 1, 0) != 0)
		return 0;
	const char* herd_id = luahook::lua::lua_tolstring(L, -1, NULL);
	luahook::lua::lua_pushstring(L, herd_id);
	return 1;
}

int luahook::beefcake::GetIgnored(lua_State* L) {
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "EntityGetWithTag");
	luahook::lua::lua_pushstring(L, "player_unit");
	if (luahook::lua::lua_pcall(L, 1, 1, 0) != 0)
		return 0;
	luahook::lua::lua_rawgeti(L, -1, 1);
	int player = (int)luahook::lua::lua_tonumber(L, -1);
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "EntityGetFirstComponent");
	luahook::lua::lua_pushinteger(L, player);
	luahook::lua::lua_pushstring(L, "GenomeDataComponent");
	if (luahook::lua::lua_pcall(L, 2, 1, 0) != 0)
		return 0;
	int gdc = luahook::lua::lua_tointeger(L, -1);
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "ComponentGetValue2");
	luahook::lua::lua_pushinteger(L, gdc);
	luahook::lua::lua_pushstring(L, "herd_id");
	if (luahook::lua::lua_pcall(L, 2, 1, 0) != 0)
		return 0;
	const char* herd_id = luahook::lua::lua_tolstring(L, -1, NULL);
	if (strcmp(herd_id, "healer"))
	{
		luahook::lua::lua_pushboolean(L, 1);
		return 1;
	}
	else if(strcmp(herd_id, "player")) {
		luahook::lua::lua_pushboolean(L, 0);
		return 1;
	}
	else {
		return 0;
	}
}

int luahook::beefcake::SetPlayerPos(lua_State* L) {
	if (luahook::lua::lua_gettop(L) != 2)
		return 0;
	int x = luahook::lua::lua_tonumber(L, 1);
	int y = luahook::lua::lua_tonumber(L, 2);
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "EntityGetWithTag");
	luahook::lua::lua_pushstring(L, "player_unit");
	if (luahook::lua::lua_pcall(L, 1, 1, 0) != 0)
		return 0;
	luahook::lua::lua_rawgeti(L, -1, 1);
	int player = (int)luahook::lua::lua_tonumber(L, -1);
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "EntitySetTransform");
	luahook::lua::lua_pushinteger(L, player);
	luahook::lua::lua_pushnumber(L, x);
	luahook::lua::lua_pushnumber(L, y);
	luahook::lua::lua_pushnumber(L, 0);
	luahook::lua::lua_pushnumber(L, 1);
	luahook::lua::lua_pushnumber(L, 1);
	if (luahook::lua::lua_pcall(L, 6, 0, 0) != 0)
		return 0;
	luahook::lua::lua_settop(L, 0);
	return 0;
}

int luahook::beefcake::SetPlayerHealth(lua_State* L) {
	if (luahook::lua::lua_gettop(L) != 1)
		return 0;
	double health = luahook::lua::lua_tonumber(L, 1);
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "EntityGetWithTag");
	luahook::lua::lua_pushstring(L, "player_unit");
	if (luahook::lua::lua_pcall(L, 1, 1, 0) != 0)
		return 0;
	luahook::lua::lua_rawgeti(L, -1, 1);
	int player = (int)luahook::lua::lua_tonumber(L, -1);
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "EntityGetFirstComponent");
	luahook::lua::lua_pushinteger(L, player);
	luahook::lua::lua_pushstring(L, "DamageModelComponent");
	if (luahook::lua::lua_pcall(L, 2, 1, 0) != 0)
		return 0;
	int damage = (int)luahook::lua::lua_tonumber(L, -1);
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "ComponentSetValue2");
	luahook::lua::lua_pushinteger(L, damage);
	luahook::lua::lua_pushstring(L, "hp");
	luahook::lua::lua_pushnumber(L, health / 25);
	if (luahook::lua::lua_pcall(L, 3, 0, 0) != 0)
		return 0;
	luahook::lua::lua_settop(L, 0);
	return 0;
}

int luahook::beefcake::SetPlayerHealthM(lua_State* L) {
	if (luahook::lua::lua_gettop(L) != 1)
		return 0;
	double health = luahook::lua::lua_tonumber(L, 1);
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "EntityGetWithTag");
	luahook::lua::lua_pushstring(L, "player_unit");
	if (luahook::lua::lua_pcall(L, 1, 1, 0) != 0)
		return 0;
	luahook::lua::lua_rawgeti(L, -1, 1);
	int player = (int)luahook::lua::lua_tonumber(L, -1);
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "EntityGetFirstComponent");
	luahook::lua::lua_pushinteger(L, player);
	luahook::lua::lua_pushstring(L, "DamageModelComponent");
	if (luahook::lua::lua_pcall(L, 2, 1, 0) != 0)
		return 0;
	int damage = (int)luahook::lua::lua_tonumber(L, -1);
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "ComponentSetValue2");
	luahook::lua::lua_pushinteger(L, damage);
	luahook::lua::lua_pushstring(L, "max_hp");
	luahook::lua::lua_pushnumber(L, health / 25);
	if (luahook::lua::lua_pcall(L, 3, 0, 0) != 0)
		return 0;
	luahook::lua::lua_settop(L, 0);
	return 0;
}

int luahook::beefcake::SetPlayerGold(lua_State* L) {
	if (luahook::lua::lua_gettop(L) != 1)
		return 0;
	int mon = (int)luahook::lua::lua_tonumber(L, 1);
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "EntityGetWithTag");
	luahook::lua::lua_pushstring(L, "player_unit");
	if (luahook::lua::lua_pcall(L, 1, 1, 0) != 0)
		return 0;
	luahook::lua::lua_rawgeti(L, -1, 1);
	int player = (int)luahook::lua::lua_tonumber(L, -1);
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "EntityGetFirstComponent");
	luahook::lua::lua_pushinteger(L, player);
	luahook::lua::lua_pushstring(L, "WalletComponent");
	if (luahook::lua::lua_pcall(L, 2, 1, 0) != 0)
		return 0;
	int wallet = (int)luahook::lua::lua_tonumber(L, -1);
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "ComponentSetValue2");
	luahook::lua::lua_pushinteger(L, wallet);
	luahook::lua::lua_pushstring(L, "money");
	luahook::lua::lua_pushinteger(L, mon);
	if (luahook::lua::lua_pcall(L, 3, 0, 0) != 0)
		return 0;
	luahook::lua::lua_settop(L, 0);
	return 0;
}

int luahook::beefcake::SetIgnored(lua_State* L) {
	if (luahook::lua::lua_gettop(L) != 1)
		return 0;
	int b00l = luahook::lua::lua_toboolean(L, 1);
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "EntityGetWithTag");
	luahook::lua::lua_pushstring(L, "player_unit");
	if (luahook::lua::lua_pcall(L, 1, 1, 0) != 0)
		return 0;
	luahook::lua::lua_rawgeti(L, -1, 1);
	int player = (int)luahook::lua::lua_tonumber(L, -1);
	// GenomeSetHerdId is "deprecated", but not really since the game has been untouched for a year or so now
	// if for some reason they do finally remove it, i'll just reinstate it
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "GenomeSetHerdId");
	luahook::lua::lua_pushinteger(L, player);
	if (b00l >= 1)
		luahook::lua::lua_pushstring(L, "healer");
	else
		luahook::lua::lua_pushstring(L, "player");
	if (luahook::lua::lua_pcall(L, 2, 0, 0) != 0)
		return 0;
	luahook::lua::lua_settop(L, 0);
	return 0;
}

int luahook::beefcake::AddPerk(lua_State* L)
{
	if (luahook::lua::lua_gettop(L) != 1)
		return 0;
	const char* perk = luahook::lua::lua_tolstring(L, 1, NULL);
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "EntityGetWithTag");
	luahook::lua::lua_pushstring(L, "player_unit");
	if (luahook::lua::lua_pcall(L, 1, 1, 0) != 0)
		return 0;
	luahook::lua::lua_rawgeti(L, -1, 1);
	int player = (int)luahook::lua::lua_tonumber(L, -1);
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "dofile_once");
	luahook::lua::lua_pushstring(L, "data/scripts/perks/perk.lua");
	if (luahook::lua::lua_pcall(L, 1, 1, 0) != 0)
		return 0;
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "perk_spawn");
	luahook::lua::lua_pushnumber(L, 0);
	luahook::lua::lua_pushnumber(L, 0);
	luahook::lua::lua_pushstring(L, perk);
	if (luahook::lua::lua_pcall(L, 3, 1, 0) != 0)
		return 0;
	int perka = luahook::lua::lua_tointeger(L, -1);
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "perk_pickup");
	luahook::lua::lua_pushinteger(L, perka);
	luahook::lua::lua_pushinteger(L, player);
	luahook::lua::lua_pushinteger(L, 0);
	luahook::lua::lua_pushboolean(L, 1);
	luahook::lua::lua_pushboolean(L, 0);
	if (luahook::lua::lua_pcall(L, 5, 0, 0) != 0)
		return 0;
	luahook::lua::lua_settop(L, 0);
	return 0;
}

int luahook::beefcake::AddSpellToWand(lua_State* L)
{
	if (luahook::lua::lua_gettop(L) != 3)
		return 0;
	int wand = luahook::lua::lua_tointeger(L, 1);
	const char* spell = luahook::lua::lua_tolstring(L, 2, NULL);
	int perm = luahook::lua::lua_toboolean(L, 3);
	if (spell == "")
		return 0;
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "CreateItemActionEntity");
	luahook::lua::lua_pushstring(L, spell);
	luahook::lua::lua_pushnumber(L, 0);
	luahook::lua::lua_pushnumber(L, 0);
	if (luahook::lua::lua_pcall(L, 3, 1, 0) != 0)
		return 0;
	int action = (int)luahook::lua::lua_tonumber(L, -1);
	if (action != 0)
	{
		if (perm == 0)
		{
			luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "EntityAddChild");
			luahook::lua::lua_pushinteger(L, wand);
			luahook::lua::lua_pushinteger(L, action);
			if (luahook::lua::lua_pcall(L, 2, 0, 0) != 0)
				return 0;
			luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "EntitySetComponentsWithTagEnabled");
			luahook::lua::lua_pushinteger(L, action);
			luahook::lua::lua_pushstring(L, "enabled_in_world");
			luahook::lua::lua_pushboolean(L, 0);
			if (luahook::lua::lua_pcall(L, 3, 0, 0) != 0)
				return 0;
			return 0;
		}
		else {
			luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "EntityGetFirstComponentIncludingDisabled");
			luahook::lua::lua_pushinteger(L, wand);
			luahook::lua::lua_pushstring(L, "AbilityComponent");
			if (luahook::lua::lua_pcall(L, 2, 1, 0) != 0)
				return 0;
			int ac = (int)luahook::lua::lua_tonumber(L, -1);
			luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "ComponentObjectGetValue2");
			luahook::lua::lua_pushinteger(L, ac);
			luahook::lua::lua_pushstring(L, "gun_config");
			luahook::lua::lua_pushstring(L, "deck_capacity");
			if (luahook::lua::lua_pcall(L, 3, 1, 0) != 0)
				return 0;
			int cap = (int)luahook::lua::lua_tonumber(L, -1);
			luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "ComponentObjectSetValue2");
			luahook::lua::lua_pushinteger(L, ac);
			luahook::lua::lua_pushstring(L, "gun_config");
			luahook::lua::lua_pushstring(L, "deck_capacity");
			luahook::lua::lua_pushinteger(L, cap + 1);
			if (luahook::lua::lua_pcall(L, 4, 0, 0) != 0)
				return 0;
			luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "EntityGetFirstComponent");
			luahook::lua::lua_pushinteger(L, action);
			luahook::lua::lua_pushstring(L, "ItemComponent");
			if (luahook::lua::lua_pcall(L, 2, 1, 0) != 0)
				return 0;
			int ic = (int)luahook::lua::lua_tonumber(L, -1);
			luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "ComponentSetValue2");
			luahook::lua::lua_pushinteger(L, ic);
			luahook::lua::lua_pushstring(L, "permanently_attached");
			luahook::lua::lua_pushboolean(L, 1);
			if (luahook::lua::lua_pcall(L, 3, 0, 0) != 0)
				return 0;
			luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "EntityAddChild");
			luahook::lua::lua_pushinteger(L, wand);
			luahook::lua::lua_pushinteger(L, action);
			if (luahook::lua::lua_pcall(L, 2, 0, 0) != 0)
				return 0;
			luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "EntitySetComponentsWithTagEnabled");
			luahook::lua::lua_pushinteger(L, action);
			luahook::lua::lua_pushstring(L, "enabled_in_world");
			luahook::lua::lua_pushboolean(L, 0);
			if (luahook::lua::lua_pcall(L, 3, 0, 0) != 0)
				return 0;
		}
	}
	return 0;
}

int luahook::beefcake::CreateWand(lua_State* L)
{
	if (luahook::lua::lua_gettop(L) != 8)
		return 0;
	const char* name = luahook::lua::lua_tolstring(L, 1, NULL);
	const char* entity = luahook::lua::lua_tolstring(L, 2, NULL);
	double mana_charge = luahook::lua::lua_tonumber(L, 3);
	double mana_max = luahook::lua::lua_tonumber(L, 4);
	double reloadspeed = luahook::lua::lua_tonumber(L, 5);
	double recoil = luahook::lua::lua_tonumber(L, 6);
	int actions_per_round = (int)luahook::lua::lua_tonumber(L, 7);
	int capacity = (int)luahook::lua::lua_tonumber(L, 8);
	if (mana_charge <= 0)
		mana_charge = 100;
	if (mana_max <= 0)
		mana_max = 75;
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "EntityGetWithTag");
	luahook::lua::lua_pushstring(L, "player_unit");
	if (luahook::lua::lua_pcall(L, 1, 1, 0) != 0)
		return 0;
	luahook::lua::lua_rawgeti(L, -1, 1);
	int player = (int)luahook::lua::lua_tonumber(L, -1);
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "EntityGetTransform");
	luahook::lua::lua_pushinteger(L, player);
	if (luahook::lua::lua_pcall(L, 1, 2, 0) != 0)
		return 0;
	double x = luahook::lua::lua_tonumber(L, -1);
	double y = luahook::lua::lua_tonumber(L, -2);
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "EntityLoad");
	luahook::lua::lua_pushstring(L, "data/entities/items/starting_wand.xml");
	luahook::lua::lua_pushnumber(L, y);
	luahook::lua::lua_pushnumber(L, x);
	if (luahook::lua::lua_pcall(L, 3, 1, 0) != 0)
		return 0;
	int wand = (int)luahook::lua::lua_tonumber(L, -1);
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "EntityGetFirstComponent");
	luahook::lua::lua_pushinteger(L, wand);
	luahook::lua::lua_pushstring(L, "AbilityComponent");
	if (luahook::lua::lua_pcall(L, 2, 1, 0) != 0)
		return 0;
	int ac = (int)luahook::lua::lua_tonumber(L, -1);
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "EntityGetFirstComponent");
	luahook::lua::lua_pushinteger(L, wand);
	luahook::lua::lua_pushstring(L, "HotspotComponent");
	if (luahook::lua::lua_pcall(L, 2, 1, 0) != 0)
		return 0;
	int hc = (int)luahook::lua::lua_tonumber(L, -1);
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "ComponentSetValue2");
	luahook::lua::lua_pushinteger(L, ac);
	luahook::lua::lua_pushstring(L, "entity_file");
	luahook::lua::lua_pushstring(L, entity);
	if (luahook::lua::lua_pcall(L, 3, 0, 0) != 0)
		return 0;
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "ComponentSetValue2");
	luahook::lua::lua_pushinteger(L, ac);
	luahook::lua::lua_pushstring(L, "mana_charge_speed");
	luahook::lua::lua_pushnumber(L, mana_charge);
	if (luahook::lua::lua_pcall(L, 3, 0, 0) != 0)
		return 0;
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "ComponentSetValue2");
	luahook::lua::lua_pushinteger(L, ac);
	luahook::lua::lua_pushstring(L, "mana_max");
	luahook::lua::lua_pushnumber(L, mana_max);
	if (luahook::lua::lua_pcall(L, 3, 0, 0) != 0)
		return 0;
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "ComponentSetValue2");
	luahook::lua::lua_pushinteger(L, ac);
	luahook::lua::lua_pushstring(L, "reload_time_frames");
	luahook::lua::lua_pushnumber(L, reloadspeed);
	if (luahook::lua::lua_pcall(L, 3, 0, 0) != 0)
		return 0;
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "ComponentSetValue2");
	luahook::lua::lua_pushinteger(L, ac);
	luahook::lua::lua_pushstring(L, "item_recoil_max");
	luahook::lua::lua_pushnumber(L, recoil);
	if (luahook::lua::lua_pcall(L, 3, 0, 0) != 0)
		return 0;
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "ComponentSetValue2");
	luahook::lua::lua_pushinteger(L, ac);
	luahook::lua::lua_pushstring(L, "ui_name");
	luahook::lua::lua_pushstring(L, name);
	if (luahook::lua::lua_pcall(L, 3, 0, 0) != 0)
		return 0;
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "ComponentObjectSetValue2");
	luahook::lua::lua_pushinteger(L, ac);
	luahook::lua::lua_pushstring(L, "gun_config");
	luahook::lua::lua_pushstring(L, "actions_per_round");
	luahook::lua::lua_pushinteger(L, actions_per_round);
	if (luahook::lua::lua_pcall(L, 4, 0, 0) != 0)
		return 0;
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "ComponentObjectSetValue2");
	luahook::lua::lua_pushinteger(L, ac);
	luahook::lua::lua_pushstring(L, "gun_config");
	luahook::lua::lua_pushstring(L, "reload_time");
	luahook::lua::lua_pushnumber(L, reloadspeed);
	if (luahook::lua::lua_pcall(L, 4, 0, 0) != 0)
		return 0;
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "ComponentObjectSetValue2");
	luahook::lua::lua_pushinteger(L, ac);
	luahook::lua::lua_pushstring(L, "gun_config");
	luahook::lua::lua_pushstring(L, "deck_capacity");
	luahook::lua::lua_pushinteger(L, capacity);
	if (luahook::lua::lua_pcall(L, 4, 0, 0) != 0)
		return 0;
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "ComponentObjectSetValue2");
	luahook::lua::lua_pushinteger(L, ac);
	luahook::lua::lua_pushstring(L, "gun_config");
	luahook::lua::lua_pushstring(L, "fire_rate_wait");
	luahook::lua::lua_pushinteger(L, reloadspeed);
	if (luahook::lua::lua_pcall(L, 4, 0, 0) != 0)
		return 0;
	//luahook::lua::lua_settop(L, 0);
	luahook::lua::lua_pushinteger(L, wand);
	return 1;
}

int luahook::beefcake::SpawnPerk(lua_State* L)
{
	if (luahook::lua::lua_gettop(L) != 3)
		return 0;
	const char* perk = luahook::lua::lua_tolstring(L, 1, NULL);
	double x = luahook::lua::lua_tonumber(L, 2);
	double y = luahook::lua::lua_tonumber(L, 3);
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "dofile_once");
	luahook::lua::lua_pushstring(L, "data/scripts/perks/perk.lua");
	if (luahook::lua::lua_pcall(L, 1, 1, 0) != 0)
		return 0;
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "perk_spawn");
	luahook::lua::lua_pushnumber(L, x);
	luahook::lua::lua_pushnumber(L, y);
	luahook::lua::lua_pushstring(L, perk);
	if (luahook::lua::lua_pcall(L, 3, 1, 0) != 0)
		return 0;
	return 1;
}

int luahook::beefcake::SpawnSpell(lua_State* L)
{
	if (luahook::lua::lua_gettop(L) != 3)
		return 0;
	const char* spell = luahook::lua::lua_tolstring(L, 1, NULL);
	double x = luahook::lua::lua_tonumber(L, 2);
	double y = luahook::lua::lua_tonumber(L, 3);
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "CreateItemActionEntity");
	luahook::lua::lua_pushstring(L, spell);
	luahook::lua::lua_pushnumber(L, x);
	luahook::lua::lua_pushnumber(L, y);
	if (luahook::lua::lua_pcall(L, 3, 1, 0) != 0)
		return 0;
	return 1;
}

int luahook::beefcake::SpawnFlask(lua_State* L)
{
	if (luahook::lua::lua_gettop(L) != 4)
		return 0;
	const char* flask = luahook::lua::lua_tolstring(L, 1, NULL);
	double x = luahook::lua::lua_tonumber(L, 2);
	double y = luahook::lua::lua_tonumber(L, 3);
	double amount = luahook::lua::lua_tonumber(L, 4);
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "EntityLoad");
	luahook::lua::lua_pushstring(L, "data/entities/items/pickup/potion_empty.xml");
	luahook::lua::lua_pushnumber(L, x);
	luahook::lua::lua_pushnumber(L, y);
	if (luahook::lua::lua_pcall(L, 3, 1, 0) != 0)
		return 0;
	int pot = (int)luahook::lua::lua_tonumber(L, -1);
	luahook::lua::lua_getfield(L, LUA_GLOBALSINDEX, "AddMaterialInventoryMaterial");
	luahook::lua::lua_pushinteger(L, pot);
	luahook::lua::lua_pushstring(L, flask);
	luahook::lua::lua_pushnumber(L, amount);
	if (luahook::lua::lua_pcall(L, 3, 0, 0) != 0)
		return 0;
	luahook::lua::lua_pushinteger(L, pot);
	return 1;
}

int luahook::beefcake::ReadMemory(lua_State* L) {
	if (luahook::lua::lua_gettop(L) != 2)
		return 0;
	int address = luahook::lua::lua_tointeger(L, 1);
	int type = luahook::lua::lua_tointeger(L, 2);
	mem::prot_t original_protection = mem::in::get_page((mem::voidptr_t)address).protection;
	mem::in::protect((mem::voidptr_t)address, (mem::size_t)(mem::voidptr_t)address, PAGE_EXECUTE_READWRITE);
	if (mem::in::get_page((mem::voidptr_t)address).protection == original_protection)
	{
		mem::process_t proc = mem::ex::get_process(GetCurrentProcessId());
		mem::ex::protect(proc, (mem::voidptr_t)address, (mem::size_t)(mem::voidptr_t)address, PAGE_EXECUTE_READWRITE);
		if (type == 0)
		{
			mem::int32_t value = mem::ex::read<mem::int32_t>(proc, (mem::voidptr_t)address);
			mem::ex::protect(proc, (mem::voidptr_t)address, (mem::size_t)(mem::voidptr_t)address, original_protection);
			luahook::lua::lua_pushinteger(L, value);
			return 1;
		}
		else if (type == 1) {
			mem::cstring_t value = mem::ex::read<mem::cstring_t>(proc, (mem::voidptr_t)address);
			mem::ex::protect(proc, (mem::voidptr_t)address, (mem::size_t)(mem::voidptr_t)address, original_protection);
			luahook::lua::lua_pushstring(L, (const char*)value);
			return 1;
		}
		else {
			return 0;
		}
	}
	else {
		if (type == 0)
		{
			mem::int32_t value = mem::in::read<mem::int32_t>((mem::voidptr_t)address);
			mem::in::protect((mem::voidptr_t)address, (mem::size_t)(mem::voidptr_t)address, original_protection);
			luahook::lua::lua_pushinteger(L, value);
			return 1;
		}
		else if (type == 1) {
			mem::cstring_t value = mem::in::read<mem::cstring_t>((mem::voidptr_t)address);
			mem::in::protect((mem::voidptr_t)address, (mem::size_t)(mem::voidptr_t)address, original_protection);
			luahook::lua::lua_pushstring(L, (const char*)value);
			return 1;
		}
		else {
			return 0;
		}
	}
	return 0;
}

// i lied, i'll comment on this
// this is pretty much directly ripped from the lua 5.1 source, modified to work with the beefcake console
int luahook::beefcake::Print(lua_State* L)
{
	int n = luahook::lua::lua_gettop(L);
	int i;
	lua_getglobal(L, "tostring");
	std::cout << dye::light_green("[SCRIPT] ");
	for (i = 1; i <= n; i++)
	{
		const char* s;
		luahook::lua::lua_pushvalue(L, -1);
		luahook::lua::lua_pushvalue(L, i);
		luahook::lua::lua_call(L, 1, 1);
		s = luahook::lua::lua_tolstring(L, -1, NULL);
		if (s == NULL)
		{
			std::cout << dye::light_red("[ERROR] ") << LUA_QL("tostring") " must return a string to " LUA_QL("print") << std::endl;
			return 0;
		}
		if (i > 1) std::cout << "\t";
		std::cout << s;
		lua_pop(L, 1);
	}
	std::cout << std::endl;
	return 0;
}

int luahook::beefcake::ForceSeed(lua_State* L)
{
	if (luahook::lua::lua_gettop(L) != 1)
		return 0;
	int seed = luahook::lua::lua_tointeger(L, 1);
	writeINT(luahook::noita::seed, seed);
	return 0;
}

int luahook::beefcake::GetState(lua_State* L)
{
	luahook::lua::lua_pushinteger(L, (uintptr_t)L);
	return 1;
}

int luahook::beefcake::GetCFunctionPointer(lua_State* L)
{
	if (luahook::lua::lua_gettop(L) != 1)
		return 0;
	luahook::lua::lua_CFunction func = luahook::lua::lua_tocfunction(L, 1);
	luahook::lua::lua_pushinteger(L, (uintptr_t)func);
	return 1;
}

int puff = 0;
void __fastcall luahook::noita::nlib_hook(lua_State* L)
{
	// is this our first time executing?
	if (puff == 0)
	{
		// no longer our first time
		puff = 1;
		// store the lua_state
		luahook::noita::first = L;
		logh("First lua state found: ", luahook::noita::first);
	}

	// initiate LocalPlayer library
	const struct luahook::lua::luaL_Reg LocalPlayerFuncs[] = {
			{"AddPerk", luahook::beefcake::AddPerk},
			{"GetID", luahook::beefcake::GetPlayer},
			{"GetIsIgnored", luahook::beefcake::GetIgnored},
			{"GetPosition", luahook::beefcake::GetPlayerPos},
			{"GetHealth", luahook::beefcake::GetPlayerHealth},
			{"GetMaxHealth", luahook::beefcake::GetPlayerHealthM},
			{"GetGold", luahook::beefcake::GetPlayerGold},
			{"SetPosition", luahook::beefcake::SetPlayerPos},
			{"SetHealth", luahook::beefcake::SetPlayerHealth},
			{"SetMaxHealth", luahook::beefcake::SetPlayerHealthM},
			{"SetGold", luahook::beefcake::SetPlayerGold},
			{"SetIsIgnored", luahook::beefcake::SetIgnored},
			{NULL, NULL}
	};

	// initiate task library
	const struct luahook::lua::luaL_Reg taskfuncs[] = {
			{"GetCFunctionPointer", luahook::beefcake::GetCFunctionPointer},
			{"GetState", luahook::beefcake::GetState},
			{"ReadMemory", luahook::beefcake::ReadMemory},
			{NULL, NULL}
	};

	// register our libraries
	luahook::lua::luaL_register(L, "LocalPlayer", LocalPlayerFuncs);		
	luahook::lua::luaL_register(L, "task", taskfuncs);
	// register our globals
	lua_register(L, "AddSpellToWand", luahook::beefcake::AddSpellToWand);			
	lua_register(L, "SetWorldTime", luahook::beefcake::SetWorldTime);			
	lua_register(L, "ExecuteThroughLoader", luahook::beefcake::ExecuteThroughLoader);	
	lua_register(L, "CreateWand", luahook::beefcake::CreateWand);				
	lua_register(L, "SpawnPerk", luahook::beefcake::SpawnPerk);			
	lua_register(L, "SpawnSpell", luahook::beefcake::SpawnSpell);						
	lua_register(L, "SpawnFlask", luahook::beefcake::SpawnFlask);				
	lua_register(L, "print", luahook::beefcake::Print);	
	lua_register(L, "ForceSeed", luahook::beefcake::ForceSeed);
	lua_register(L, "GenomeGetHerdId", luahook::beefcake::GenomeGetHerdId);
	lua_registerV(L, luahook::lua::lua_pushnumber, "inf", 2147483647);
	// noita adds restrictions, bypass those by loading standard libraries
	luahook::lua::luaopen_bit(L);
	luahook::lua::luaopen_debug(L);
	luahook::lua::luaopen_ffi(L);
	luahook::lua::luaopen_io(L);
	luahook::lua::luaopen_jit(L);
	luahook::lua::luaopen_os(L);

	// return original functions (EntityLoad, etc..)
	return luahook::noita::lib(L);
}
