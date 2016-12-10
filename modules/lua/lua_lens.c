/***
 Lens functions
 
 @author Magic Lantern Team
 @copyright 2014
 @license GPL
 @module lens
 */

#include <dryos.h>
#include <string.h>
#include <lens.h>
#include <focus.h>

#include "lua_common.h"

static int luaCB_lens_index(lua_State * L)
{
    LUA_PARAM_STRING_OPTIONAL(key, 2, "");
    /// Get/Set the name of the lens (reported by the lens)
    // @tfield string name
    if(!strcmp(key, "name")) lua_pushstring(L, lens_info.name);
    /// Get/Set the focal length of the lens (in mm)
    // @tfield int focal_length
    else if(!strcmp(key, "focal_length")) lua_pushinteger(L, lens_info.focal_len);
    /// Get the current focus distance (in mm)
    // @tfield int focus_distance readonly
    else if(!strcmp(key, "focus_distance")) lua_pushinteger(L, lens_info.focus_dist * 10);
    /// Get the hyperfocal distance of the lens (in mm)
    // @tfield int hyperfocal readonly
    else if(!strcmp(key, "hyperfocal")) lua_pushinteger(L, lens_info.hyperfocal);
    /// Get the distance to the DOF near (in mm)
    // @tfield int dof_near readonly
    else if(!strcmp(key, "dof_near")) lua_pushinteger(L, lens_info.dof_near);
    /// Get the distance to the DOF far (in mm)
    // @tfield int dof_far readonly
    else if(!strcmp(key, "dof_far")) lua_pushinteger(L, lens_info.dof_far);
    /// Get whether or not auto focus is enabled
    // @tfield bool af readonly
    else if(!strcmp(key, "af")) lua_pushboolean(L, !is_manual_focus());
    /// Get the current auto focus mode (may be model-specific)
    // @tfield int af_mode readonly
    else if(!strcmp(key, "af_mode")) lua_pushinteger(L, af_mode);
    /// Use to manually set the len's aperture value for non-chipped lenses (for metadata purposes)
    // @tfield bool manual_aperture
    else if(!strcmp(key, "manual_aperture")) lua_pushnumber(L, lens_info.aperture / 10.0);
    /// Get if the lens chipped
    // @tfield bool is_chipped readonly
    else if(!strcmp(key, "is_chipped")) lua_pushboolean(L, lens_info.lens_exists);
    else lua_rawget(L, 1);
    return 1;
}

static int luaCB_lens_newindex(lua_State * L)
{
    LUA_PARAM_STRING_OPTIONAL(key, 2, "");
    if(!strcmp(key, "name"))
    {
        LUA_PARAM_STRING(value, 3);
        strncpy(lens_info.name, value, 31);
    }
    else if(!strcmp(key, "focal_length"))
    {
        LUA_PARAM_INT(value, 3);
        lens_info.focal_len = value;
    }
    else if(!strcmp(key, "manual_aperture"))
    {
        if(lens_info.lens_exists) return luaL_error(L, "Can't set manual aperture for chipped lens");
        LUA_PARAM_NUMBER(value, 3);
        lens_info.aperture = (int)(value * 10);
    }
    else if(!strcmp(key, "focus_distance") || !strcmp(key, "hyperfocal") || !strcmp(key, "dof_near") || !strcmp(key, "dof_far") || !strcmp(key, "af") || !strcmp(key, "is_chipped"))
    {
        return luaL_error(L, "'%s' is readonly!", key);
    }
    else
    {
        lua_rawset(L, 1);
    }
    return 0;
}

/***
 Moves the focus motor a specified number of steps. Only works in LV.
 @tparam int num_steps
 @tparam[opt=2] int step_size
 @tparam[opt=true] bool wait
 @tparam[opt=0] int extra_delay
 @function focus
 */
static int luaCB_lens_focus(lua_State * L)
{
    LUA_PARAM_INT(num_steps, 1);
    LUA_PARAM_INT_OPTIONAL(step_size, 2, 2);
    LUA_PARAM_BOOL_OPTIONAL(wait, 3, true);
    LUA_PARAM_INT_OPTIONAL(extra_delay, 4, 0);
    lua_pushboolean(L, lens_focus(num_steps, step_size, wait, extra_delay));
    return 1;
}

static const char * lua_lens_fields[] =
{
    "name",
    "focal_length",
    "focus_distance",
    "hyperfocal",
    "dof_near",
    "dof_far",
    "af",
    "af_mode",
    NULL
};

static const luaL_Reg lenslib[] =
{
    { "focus", luaCB_lens_focus },
    { NULL, NULL }
};

LUA_LIB(lens)
