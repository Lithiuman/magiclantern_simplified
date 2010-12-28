/** \file
 * Autoboot flag control
 */
#include "dryos.h"
#include "bmp.h"
#include "menu.h"
#include "config.h"

/* CF device structure */
struct cf_device
{
	// If block has the top bit set the physical blocks will be read
	// instead of from the first partition.  Cool.
	int 			(*read_block)(
		struct cf_device *		dev,
		uintptr_t			block,
		size_t				num_blocks,
		void *				buf
	);

	int 			(*write_block)(
		struct cf_device *		dev,
		uintptr_t			block,
		size_t				num_blocks,
		const void *			buf
	);

	void *			io_control;
	void *			soft_reset;
};

extern struct cf_device * const cf_device[];
extern struct cf_device * const sd_device[];


/** Shadow copy of the NVRAM boot flags stored at 0xF8000000 */
#define NVRAM_BOOTFLAGS		((void*) 0xF8000000)
struct boot_flags
{
	uint32_t		firmware;	// 0x00
	uint32_t		bootdisk;	// 0x04
	uint32_t		ram_exe;	// 0x08
	uint32_t		update;		// 0x0c
	uint32_t		flag_0x10;
	uint32_t		flag_0x14;
	uint32_t		flag_0x18;
	uint32_t		flag_0x1c;
};

static struct boot_flags * const	boot_flags = NVRAM_BOOTFLAGS;;



/** Write the auto-boot flags to the CF card and to the flash memory */
static void
bootflag_toggle( void * priv )
{
	if( boot_flags->bootdisk )
		call( "DisableBootDisk" );
	else
		call( "EnableBootDisk" );
}


void
bootflag_display(
	void *			priv,
	int			x,
	int			y,
	int			selected
)
{
	bmp_printf(
		selected ? MENU_FONT_SEL : MENU_FONT,
		x, y,
		//23456789012
		"Autoboot    %s",
		boot_flags->bootdisk != 0 ? "ON " : "OFF"
	);
}


// gcc mempcy has odd alignment issues?
static inline void
my_memcpy(
	uint8_t *		dest,
	const uint8_t *		src,
	size_t			len
)
{
	while( len-- > 0 )
		*dest++ = *src++;
}


void
bootflag_write_bootblock( void )
{
	gui_stop_menu();

	bmp_printf( FONT_SMALL, 0, 30, "cf=%08lx sd=%08lx", (uint32_t)sd_device[0], (uint32_t) sd_device[1]);

	struct cf_device * const dev = sd_device[1];


	uint8_t *block = alloc_dma_memory( 0x200*0x40 );
	uint8_t * user_block = (void*)((uintptr_t) block & ~0x40000000);
	int i;
	DebugMsg(DM_MAGIC, 3, "%s: buf=%08x", __func__, (uint32_t)block);
	for(i=0 ; i<0x200 ; i++) block[i] = 0xAA;
	bmp_printf( FONT_SMALL, 0, 40, "mem=%08lx read=%08lx", (uint32_t)block, (uint32_t)dev->read_block );
	bmp_hexdump( FONT_SMALL, 0, 250, sd_device[1], 0x100 );

	dm_set_store_level(0x23, 0);
	int rc = dev->read_block( dev, block, 0x0, 0x40 );
	clean_d_cache();
        flush_caches();


	dm_set_store_level(0x23, 3);
	DebugMsg(DM_MAGIC, 3, "%s: rc=%d %08x %08x", __func__,
		rc,
		*(uint32_t*) &user_block[0x47],
		*(uint32_t*) &user_block[0x5C]
	);
	msleep( 100 );

	bmp_printf( FONT_MED, 600, 40, "read=%d", rc );
	bmp_hexdump( FONT_SMALL, 0, 60, user_block, 0x100 );

/*
	// Update the first partition header to include the magic
	// strings
	my_memcpy( block + 0x47, (uint8_t*) "EOS_DEVELOP", 0xB );
	my_memcpy( block + 0x5C, (uint8_t*) "BOOTDISK", 0x8 );

	rc = dev->write_block( dev, 0x0, 1, block );
	bmp_printf( FONT_MED, 600, 60, "write=%d", rc );
	free_dma_memory( block );
*/
}


/** Perform an initial install and configuration */
static void
initial_install(void)
{
	bmp_fill(COLOR_BG, 0, 0, 720, 480);
	bmp_printf(FONT_LARGE, 0, 30, "Magic Lantern install");

	FILE * f = FIO_CreateFile("B:/ROM0.BIN");
	if (f != (void*) -1)
	{
		bmp_printf(FONT_LARGE, 0, 60, "Writing RAM");
		FIO_WriteFile(f, (void*) 0xFF010000, 0x900000);
		FIO_CloseFile(f);
	}

	bmp_printf(FONT_LARGE, 0, 90, "Setting boot flag");
	bootdisk_enable();

	//bmp_printf(FONT_LARGE, 0, 120, "Writing boot block");
	//bootflag_write_bootblock();

	bmp_printf(FONT_LARGE, 0, 150, "Writing boot log");
	dumpf();

	bmp_printf(FONT_LARGE, 0, 180, "Done!");
}



#if 0
void
bootflag_display_all(
	void *			priv,
	int			x,
	int			y,
	int			selected
)
{
	bmp_printf( selected ? MENU_FONT_SEL : MENU_FONT,
		x,
		y,
		//23456789012
		"Firmware    %s\n"
		"Bootdisk    %s\n"
		"RAM_EXE     %s\n"
		"Update      %s\n",
		boot_flags->firmware == 0 ? "ON" : "OFF",
		boot_flags->bootdisk == 0  ? "ON" : "OFF",
		boot_flags->ram_exe == 0 ? "ON" : "OFF",
		boot_flags->update == 0 ? "ON" : "OFF"
	);
}
#endif


CONFIG_INT( "disable-powersave", disable_powersave, 0 );

static void
powersave_display(
	void *			priv,
	int			x,
	int			y,
	int			selected
)
{
	bmp_printf(
		selected ? MENU_FONT_SEL : MENU_FONT,
		x,
		y,
		//23456789012
		"Powersave   %s\n",
		!disable_powersave ? "ON " : "OFF"
	);
}


static void
powersave_toggle( void )
{
	disable_powersave = !disable_powersave;

	prop_request_icu_auto_poweroff(
		disable_powersave ? EM_PROHIBIT : EM_ALLOW
	);
}



struct menu_entry boot_menus[] = {
	//~ {
		//~ .display	= menu_print,
		//~ .priv		= "Write MBR",
		//~ .select		= bootflag_write_bootblock,
	//~ },

	//~ {
		//~ .display	= bootflag_display,
		//~ .select		= bootflag_toggle,
	//~ },

	{
		.display	= powersave_display,
		.select		= powersave_toggle,
	},

#if 0
	{
		.display	= bootflag_display_all,
	},
#endif
};


static void
bootflags_init( void )
{
	if( autoboot_loaded == 0 )
		initial_install();

	menu_add( "Debug", boot_menus, COUNT(boot_menus) );

	if( disable_powersave )
	{
		DebugMsg( DM_MAGIC, 3,
			"%s: Disabling powersave",
			__func__
		);

		prop_request_icu_auto_poweroff( EM_PROHIBIT );
	}

}


INIT_FUNC( __FILE__, bootflags_init );
