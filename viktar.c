// viktar.c, Henry Sides, Lab2, CS333

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <zlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <utime.h>

#include "viktar.h"

#define EXTRACT 'x'
#define CREATE 'c'
#define SMALL_TOC 't'
#define BIG_TOC 'T'

#define BUF_SIZE 1024
#define TIME_FORMAT "%Y-%m-%d %H:%M:%S %Z"
#define MODE_SIZE 11
#define BIG_TOC_PAD -13

static int is_verbose = FALSE;

int display_toc(int, const char *, const int);
void set_symbolic_mode(const int, char *);
void validate_tag(const char *, const char *);
void failed_to_open(const char *, const char *);

int main(int argc, char *argv[])
{
	char buf[BUF_SIZE] = {'\0'};
	char * a_name = NULL;		// Archive file name
	int ifd = STDIN_FILENO;
	int ofd = STDOUT_FILENO;
	int opt = 0;
	char last_opt = 0;
	ssize_t bytes_written;
	mode_t old_mode = 0;
	old_mode = umask(0);
	
	setuid(0);
	while((opt = getopt(argc, argv, OPTIONS)) != -1)
	{
		switch(opt)
		{
			case 'x':
				last_opt = EXTRACT;
				break;
			case 'c':
				last_opt = CREATE;
				break;
			case 't':
				last_opt = SMALL_TOC;
				break;
			case 'T':
				last_opt = BIG_TOC;
				break;
			case 'f':
				a_name = optarg;
				break;
			case 'h': 
				printf("help text\n"
        			"\t./viktar\n"
				"\tOptions: xctTf:hv\n"
				"\t\t-x              extract file/files from archive\n"
				"\t\t-c              create an archive file\n"
				"\t\t-t              display a short table of contents of the archive file\n"
				"\t\t-T              display a long table of contents of the archive file\n"
				"\t\tOnly one of xctT can be specified\n"
				"\t\t-f filename     use filename as the archive file\n"
				"\t\t-v              give verbose diagnostic messages\n"
				"\t\t-h              display this AMAZING help message\n");
				exit(EXIT_SUCCESS);
				break;
			case 'v':
				is_verbose = !is_verbose;
				fprintf(stderr, "verbose level: %d\n", is_verbose);
				break;
			default:
				perror("illegal command detected");
				exit(EXIT_FAILURE);
				break;
		}
	}

	switch(last_opt)
	{
		case EXTRACT:
		{
			int bytes_read = 0;
			int read_me = FALSE;
			uint32_t temp_crc32;
			viktar_header_t md_header;
			viktar_footer_t md_footer;
			struct utimbuf times;

			// Open	
			if(a_name)
			{
				ifd = open(a_name, O_RDONLY);
				if(ifd < 0) failed_to_open("failed to open input archive file", a_name);
			}
			else	fprintf(stderr, "reading archive from stdin\n");

			// Read tag & compare
			read(ifd, buf, strlen(VIKTAR_FILE));
			validate_tag(buf, a_name);

			// Read by header
			while(read(ifd, &md_header, sizeof(viktar_header_t)) > 0)
			{
				read_me = FALSE;
				memset(buf, 0, BUF_SIZE);
				strncpy(buf, md_header.viktar_name, VIKTAR_MAX_FILE_NAME_LEN);
				if(optind < argc)
				{
					for(int i = optind; i < argc; ++i)
					{
						if(strcmp(argv[i], md_header.viktar_name) == 0) read_me = TRUE;
					}
				}
				else 	read_me = TRUE;

				// If to be read:
				if(read_me)
				{
					// Then read
					ssize_t total_bytes_read = 0;

					ofd = open(md_header.viktar_name, O_WRONLY | O_CREAT | O_TRUNC, md_header.st_mode);
					
					temp_crc32 = crc32(0x0, Z_NULL, 0);
					while((bytes_read = read(ifd, buf, 
						((md_header.st_size - total_bytes_read) < BUF_SIZE ? 
						(md_header.st_size - total_bytes_read) : BUF_SIZE))) != 0)
					{
						total_bytes_read += bytes_read;
						write(ofd, buf, bytes_read); 
						temp_crc32 = crc32(temp_crc32, (unsigned char *)buf, bytes_read);
					}
					
					read(ifd, &md_footer, sizeof(viktar_footer_t));
					if(md_footer.crc32_data != temp_crc32) 
					{
						fprintf(stderr, "*** CRC32 failure data: %s  in file: 0x%x  extract: 0x%x ***\n",
								md_header.viktar_name, md_footer.crc32_data, temp_crc32);
					}

					// Final modification of permissions for isxid
					fchmod(ofd, md_header.st_mode);
					close(ofd);

					// Final modification of file mod and access times
					times.modtime = md_header.st_mtime;
					times.actime = md_header.st_atime;
					utime(md_header.viktar_name, &times);
				}
				else	lseek(ifd, md_header.st_size + sizeof(viktar_footer_t), SEEK_CUR);
			}
			if(a_name) close(ifd);	
		}
			break;
		case CREATE:
		{
			int bytes_read = 0;	
			uint32_t temp_crc32;
			struct stat sb;
			viktar_header_t md_header;
			viktar_footer_t md_footer;
			
			// Open
			if(a_name)
			{
				ofd = open(a_name, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
				if(ofd < 0) failed_to_open("failed to open output archive file", a_name);
			}

			// Writing tag:
			bytes_written = write(ofd, VIKTAR_FILE, strlen(VIKTAR_FILE));
			if(is_verbose) fprintf(stderr, "Bytes written: %ld\n", bytes_written);
			
			// Loop for reading all other files into archive
			for(int i = optind; i < argc; ++i)
			{
				memset(buf, 0, BUF_SIZE);	
				lstat(argv[i], &sb);
				
				// Header
				memset(&md_header, 0, sizeof(viktar_header_t));
				strncpy(md_header.viktar_name, argv[i], VIKTAR_MAX_FILE_NAME_LEN);
				md_header.st_size = sb.st_size;
				md_header.st_mode = sb.st_mode;
				md_header.st_uid = sb.st_uid;
				md_header.st_gid = sb.st_gid;
				md_header.st_atim = sb.st_atim;
				md_header.st_mtim = sb.st_mtim;
				bytes_written += write(ofd, &md_header, sizeof(viktar_header_t));
				if(is_verbose) fprintf(stderr, "Bytes written: %ld\n", bytes_written);
			
				// Data
				ifd = open(argv[i], O_RDONLY);
				if(ifd < 0)
				{
					fprintf(stderr, "failed to open input file \"%s\"\n", argv[i]);
					fprintf(stderr, "exiting...\n");
					exit(EXIT_SUCCESS);
				}
			
				temp_crc32 = crc32(0x0, Z_NULL, 0);
				while((bytes_read = read(ifd, buf, BUF_SIZE)) != 0)
				{
					bytes_written += write(ofd, buf, bytes_read);
					if(is_verbose) fprintf(stderr, "Bytes written: %ld\n", bytes_written);
					temp_crc32 = crc32(temp_crc32, (unsigned char *)buf, bytes_read);
				}

				// Footer	
				md_footer.crc32_data = temp_crc32;
				bytes_written += write(ofd, &md_footer, sizeof(viktar_footer_t));
				if(is_verbose) fprintf(stderr, "Bytes written: %ld\n", bytes_written);
				close(ifd);
			}
			if(a_name) close(ofd);
		}
			break;
		case SMALL_TOC:
			display_toc(ifd, a_name, 0);
			break;
		case BIG_TOC:
			display_toc(ifd, a_name, 1);
			break;
		default:
			perror("no action supplied");
			perror("exiting without doing ANYTHING...");
			exit(EXIT_SUCCESS);
			break;
	}
	umask(old_mode);	
	return EXIT_SUCCESS;
}

int display_toc(int ifd, const char * a_name, const int is_big)
{
	char buf[BUF_SIZE] = {'\0'};
	char mode_symbol[MODE_SIZE] = {'\0'};
	struct passwd * pwd;
	struct group * grp;
	char time_formatted[BUF_SIZE];
	viktar_header_t md_header;
	viktar_footer_t md_footer;
	uint32_t crc32_data;
	
	// Open
	if(a_name) 
	{
		ifd = open(a_name, O_RDONLY);
		if(ifd < 0) failed_to_open("failed to open input archive file", a_name);
		else fprintf(stderr, "reading archive file: \"%s\"\n", a_name);
	}
	else 
	{
		a_name = "stdin";
		fprintf(stderr, "reading archive from %s\n", a_name);
	}
	
	// Read tag & compare
	read(ifd, buf, strlen(VIKTAR_FILE));
	validate_tag(buf, a_name);
	
	// Printing
	printf("Contents of viktar file: \"%s\"\n", a_name != NULL ? a_name : "stdin");
	while(read(ifd, &md_header, sizeof(viktar_header_t)) > 0)
	{
		memset(buf, 0, BUF_SIZE);
		strncpy(buf, md_header.viktar_name, VIKTAR_MAX_FILE_NAME_LEN);
		printf("\tfile name: %s\n", buf);
		if(is_big)
		{
			//	mode
			memset(mode_symbol, 0, MODE_SIZE);
			strcat(mode_symbol, "-");
			set_symbolic_mode(md_header.st_mode, mode_symbol);
			printf("\t\t%*s %s\n", BIG_TOC_PAD, "mode:", mode_symbol);

			//	user
			pwd = getpwuid(md_header.st_uid);
			printf("\t\t%*s %s\n", BIG_TOC_PAD, "user:", pwd->pw_name);
		
			//	group
			grp = getgrgid(md_header.st_gid);
			printf("\t\t%*s %s\n", BIG_TOC_PAD, "group:", grp->gr_name);

			//	size
			printf("\t\t%*s %jd\n", BIG_TOC_PAD, "size:", md_header.st_size); 

			//	mtime
			strftime(time_formatted, BUF_SIZE, TIME_FORMAT, localtime(&md_header.st_mtime));
			printf("\t\t%*s %s\n", BIG_TOC_PAD, "mtime:", time_formatted);

			//	atime
			strftime(time_formatted, BUF_SIZE, TIME_FORMAT, localtime(&md_header.st_atime));
			printf("\t\t%*s %s\n", BIG_TOC_PAD, "atime:", time_formatted);
			
			//	crc32 data
			lseek(ifd, md_header.st_size, SEEK_CUR);
			read(ifd, &md_footer, sizeof(viktar_footer_t));
			crc32_data = md_footer.crc32_data;
			printf("\t\t%*s 0x%08x\n", BIG_TOC_PAD, "crc32 data: ", crc32_data); 
		}
		else	lseek(ifd, md_header.st_size + sizeof(viktar_footer_t), SEEK_CUR);
	}
	if(a_name) close(ifd);
	return 1;		
}

void set_symbolic_mode(const int mode, char * symbol)
{
        (mode & S_IRUSR) ? strcat(symbol, "r") : strcat(symbol, "-");
        (mode & S_IWUSR) ? strcat(symbol, "w") : strcat(symbol, "-");
	if(mode & S_ISUID)
	{
		(mode & S_IXUSR) ? strcat(symbol, "S") : strcat(symbol, "S");
	}
	else (mode & S_IXUSR) ? strcat(symbol, "x") : strcat(symbol, "-");
        (mode & S_IRGRP) ? strcat(symbol, "r") : strcat(symbol, "-");
        (mode & S_IWGRP) ? strcat(symbol, "w") : strcat(symbol, "-");
       	if(mode & S_ISGID)
	{
		(mode & S_IXGRP) ? strcat(symbol, "S") : strcat(symbol, "S");
	}
	else (mode & S_IXGRP) ? strcat(symbol, "x") : strcat(symbol, "-");
        (mode & S_IROTH) ? strcat(symbol, "r") : strcat(symbol, "-");
        (mode & S_IWOTH) ? strcat(symbol, "w") : strcat(symbol, "-");
        (mode & S_IXOTH) ? strcat(symbol, "x") : strcat(symbol, "-");
}

void validate_tag(const char * buf, const char * a_name)
{
	if(strncmp(buf, VIKTAR_FILE, strlen(VIKTAR_FILE)) != 0)
	{
		fprintf(stderr, "not a viktar file: \"%s\"\n", a_name);
		fprintf(stderr, "\tthis is vik-terrible\n");
		fprintf(stderr, "\texiting...\n");
		exit(EXIT_FAILURE);
	}
	return;
}

void failed_to_open(const char * msg, const char * a_name)
{
	fprintf(stderr, "%s \"%s\"\n", msg, a_name);
	fprintf(stderr, "\texiting...\n");
	exit(EXIT_SUCCESS);
}
