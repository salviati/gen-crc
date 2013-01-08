/*
    gen-crc - a utility for manipulating checksum in binary Sega Genesis / Mega Drive ROMs
    Copyright (c) 2005, Utkan Güngördü <utkan@freeconsole.org>


    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <sys/stat.h>


#define PACKAGE "gen-crc"
#define VERSION "0.0.1"


static int flag_silent  = 0;
static int flag_inplace = 1;
static int flag_fragile = 0;
static int flag_calconly  = 0;
static int flag_checksum_given = 0;
static int flag_stdout = 0;

static char *arg_outfile = 0;
static unsigned short arg_checksum = 0;


static void display_usage()
{
	fprintf(stdout, "%s\n", PACKAGE);
	fprintf(stdout, "A utility for manipulating checksum in binary SEGA Genesis/MD roms\n\n");

	fprintf(stdout, "usage: %s [options] <romlist...>\n\n", PACKAGE);

	fprintf(stdout, "Options:\n");
	fprintf(stdout, "-c --stdout     output the modified rom to stdout\n");
	fprintf(stdout, "-f --fragile    fragile mode: stop treating roms on first one results in failure\n");
	fprintf(stdout, "-h --help       display this message and quit\n");
	fprintf(stdout, "-i --in-place   edit files in place (default, has no effect when no input file is given)\n");
	fprintf(stdout, "-L --license    display license information\n");
	fprintf(stdout, "-C --calculate-only  just calculate the checksum and print to stdout\n");
	fprintf(stdout, "-s --silent     silent mode: display only error messages\n");
	fprintf(stdout, "-S --set-checksum n  set checksum to n\n");
	fprintf(stdout, "-V --version   display version information and quit\n");
}


static void display_version()
{
	fprintf(stdout, "%s %s (%s)\n", PACKAGE, VERSION, __DATE__);
}


static void display_license()
{
	fprintf(stdout, "You may redistribute copies of this program\n");
	fprintf(stdout, "under the terms of the GNU General Public License.\n");
	fprintf(stdout, "For more information about these matters, see the file named COPYING.\n");
	fprintf(stdout, "Report bugs to <bug@freeconsole.org>.\n");
}


static int treat_file(const char *filename)
{
	FILE *fp = 0;

	unsigned char *rom = 0, *p = 0;
	unsigned short checksum = 0;

	unsigned long i;
	struct stat st;

	if(stat(filename, &st))
	{
		fprintf(stderr, "error in stat: %s", strerror(errno));
		goto fail;
	}

	fp = fopen(filename, "rb");

	if(!fp)
	{
		fprintf(stderr, "couldn't open %s for reading: %s\n", filename, strerror(errno));
		goto fail;
	}
	else if(!flag_silent)
	{
		fprintf(stderr, "%s\n", filename);
	}

	if(!flag_silent)
	{
		fprintf(stderr, "rom size: %li\n", st.st_size);
		fprintf(stderr, "allocating %ld bytes rom buffer...\t", st.st_size);
	}

	if( NULL == (rom = (unsigned char*)malloc(st.st_size)))
	{
		fprintf(stderr, "memory allocation failed: %s\n", strerror(errno));
		exit(1);
	}

	if(!flag_silent) fprintf(stderr, "success\n");

	if(fread(rom, 1, st.st_size, fp) != st.st_size)
	{
		fprintf(stderr, "unexpected length fread from %s: %s\n", filename, strerror(errno));
		goto fail;
	}

	fclose(fp);

	if(flag_checksum_given)
	{
		checksum = arg_checksum;
	}
	else
	{
		p = rom + 0x200;
		i = (st.st_size - 0x200) >> 1;

		if(!flag_silent) fprintf(stderr, "calculating checksum... ");

		for(;;)
		{
			if(i--) checksum += ((unsigned short)*p++) << 8; else break;
			if(i--) checksum += ((unsigned short)*p++);      else break;
		}
	}

	if(flag_calconly)
	{
		fprintf(stderr, "%u\n", checksum);
		free(rom);
		return 0;
	}

	if(!flag_silent) fprintf(stderr, "%u\n", checksum);

	rom[0x18e] = (unsigned char)(checksum & 0xff);
	rom[0x18f] = (unsigned char)(checksum >> 8);

	if(flag_inplace)
	{
		fp = fopen(filename, "wb");
		if(!fp)
		{
			fprintf(stderr, "couldn't open %s for writing: %s\n", filename, strerror(errno));
			return 1;
		}

		fwrite(rom, 1, st.st_size, fp);
		fclose(fp);
	}

	if(flag_stdout)
	{
		fwrite(rom, 1, st.st_size, stdout);
	}

	if(arg_outfile)
	{
		fp = fopen(arg_outfile, "wb");
		if(!fp)
		{
			fprintf(stderr, "couldn't open %s for writing: %s\n", arg_outfile, strerror(errno));
			return 1;
		}

		fwrite(rom, 1, st.st_size, fp);
		fclose(fp);
	}

	free(rom);

	return 0;

fail:
	if(fp) fclose(fp);
	if(rom) free(rom);

	return 1;
}


static int treat_stdin()
{
	FILE *fp_tmp = 0;
	unsigned int c;
	unsigned int i=0;
	unsigned short checksum = 0;

	fp_tmp = tmpfile();

	if(!fp_tmp) fprintf(stderr, "couldn't open a tmp file for writing: %s\n", strerror(errno));

	if(!flag_silent) fprintf(stderr, "calculating checksum... ");

	if(flag_checksum_given)
	{
		checksum = arg_checksum;
		while((c = fgetc(stdin)) != EOF) fputc(c, fp_tmp);
	}
	else
	{
		while((c = fgetc(stdin)) != EOF)
		{
			if(i >= 0x200)
			{
				if((i&1) == 0) checksum += ((unsigned short)c) << 8;
				else           checksum += ((unsigned short)c);
			}
			fputc(c, fp_tmp);
			++i;
		}
	}

	if(flag_calconly)
	{
		fprintf(stderr, "%u\n", checksum);
		return 0;
	}

	if(!flag_silent) fprintf(stderr, "%u\n", checksum);

	fseek(fp_tmp, 0x18e, SEEK_SET);
	fputc(checksum & 0xff, fp_tmp);
	fputc(checksum >> 8, fp_tmp);
	fseek(fp_tmp, 0, SEEK_SET);

	while(i--) { c = fgetc(fp_tmp); fputc(c, stdout); }

	fclose(fp_tmp);

	return 0;
}


int main(int argc, char *argv[])
{
	int c;

	struct option long_options[] =
	{
		{"stdout",    2, 0, 'c'},
		{"calculate-only", 2, 0, 'C'},
		{"fragile",   2, 0, 'f'},
		{"help",      0, 0, 'h'},
		{"in-place",  2, 0, 'i'},
		{"license",   0, 0, 'L'},
		{"output",    1, 0, 'o'},
		{"silent",    2, 0, 's'},
		{"set-checksum",  1, 0, 'S'},
		{"version",   0, 0, 'V'},
		{0, 0, 0, 0}
	};


	while((c = getopt_long (argc, argv, "cCfhiLo:sS:V", long_options, 0)) != EOF)
	{
		switch(c)
		{
			case 'c':
				flag_stdout = optarg ? atoi(optarg) : 1;
			break;

			case 'C':
				flag_calconly = optarg ? atoi(optarg) : 1;
			break;

			case 'f':
				flag_fragile = optarg ? atoi(optarg) : 1;
			break;

			case 'h':
				display_usage();
				exit(0);

			case 'i':
				flag_inplace = optarg ? atoi(optarg) : 1;
			break;

			case 'L':
				display_license();
				exit(0);
			break;

			case 'o':
				arg_outfile = optarg;
			break;

			case 's':
				flag_silent = optarg ? atoi(optarg) : 1;
			break;

			case 'S':
				flag_checksum_given = 1;
				arg_checksum = (unsigned short)atoi(optarg);
			break;

			case 'V':
				display_version();
				exit(0);

			case '?':
				exit(1);
		}
	}

	if(argc-optind == 0)
	{
		if(!flag_silent) fprintf(stderr, "%s: no input files, trying stdin\n", PACKAGE);
		treat_stdin();
		return 0;
	}

	while (optind < argc)
		if(treat_file(argv[optind++]) && flag_fragile) break;

	return 0;
}
