# v_diskicon.c

set(DOOM_SRCS
        d_event.c
        d_iwad.c
	d_loop.c
        d_mode.c
        deh_text.c
        deh_io.c
        deh_main.c
        deh_mapping.c
        deh_str.c
	i_endoom.c
        i_glob.c
        i_input.c
	i_joystick.c
	i_main.c
        i_musicpack.c
	i_sdlmusic.c
	i_sdlsound.c
	i_sound.c
        i_system.c
        i_timer.c
	i_video.c
        m_argv.c
        m_bbox.c
        m_cheat.c
	m_config.c
        m_fixed.c
        m_controls.c
	m_misc.c
        memio.c
        mus2mid.c
	v_diskicon.c
        v_video.c
        w_checksum.c
	w_file.c
	w_file_posix.c
	w_file_fatfs.c
        w_main.c
	w_merge.c
        w_wad.c
        z_native.c
        sha1.c
	doom/d_main.c
        doom/am_map.c
        doom/doomstat.c
        doom/deh_ammo.c
        doom/deh_bexstr.c
        doom/deh_cheat.c
        doom/deh_frame.c
        doom/deh_doom.c
        doom/deh_misc.c
        doom/deh_ptr.c
        doom/deh_thing.c
        doom/deh_sound.c
        doom/deh_weapon.c 
        doom/d_net.c
        doom/d_items.c
        doom/dstrings.c
        doom/f_wipe.c
        doom/f_finale.c
        doom/g_game.c
        doom/hu_lib.c
        doom/hu_stuff.c
        doom/info.c
        doom/m_menu.c
        doom/m_random.c
        doom/p_doors.c
        doom/p_ceilng.c
        doom/p_enemy.c
        doom/p_floor.c
        doom/p_lights.c
        doom/p_plats.c
        doom/p_telept.c
        doom/p_tick.c
        doom/p_setup.c
        doom/p_sight.c
        doom/p_saveg.c
        doom/p_map.c
        doom/p_maputl.c
        doom/p_mobj.c
        doom/p_inter.c
        doom/p_spec.c
        doom/p_switch.c
        doom/p_pspr.c
        doom/p_user.c
        doom/r_bsp.c
        doom/r_main.c
        doom/r_data.c
        doom/r_draw.c
        doom/r_plane.c
        doom/r_segs.c
        doom/r_sky.c
        doom/r_things.c
        doom/s_sound.c
        doom/st_lib.c
        doom/st_stuff.c
        doom/statdump.c
        doom/sounds.c
        doom/wi_stuff.c
        tables.c
	)

include_directories(
	../
	doom
	)

add_library(doom STATIC ${DOOM_SRCS})
